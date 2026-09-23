//! QmClient 的 WS/WSS 传输；句柄只允许由 C++ 网络线程访问。

use std::ffi::{c_char, c_void, CStr};
use std::io;
use std::net::{TcpStream, ToSocketAddrs};
use std::ptr;
use std::time::{Duration, Instant};
use tungstenite::client::IntoClientRequest;
use tungstenite::http::header::{HeaderName, HeaderValue, SEC_WEBSOCKET_PROTOCOL};
use tungstenite::protocol::WebSocketConfig;
use tungstenite::stream::MaybeTlsStream;
use tungstenite::{client_tls_with_config, Error, Message, WebSocket};

struct Connection {
    socket: WebSocket<MaybeTlsStream<TcpStream>>,
    received: Vec<u8>,
}

unsafe fn text<'a>(value: *const c_char) -> Result<&'a str, String> {
    if value.is_null() {
        return Err("空字符串指针".into());
    }
    CStr::from_ptr(value).to_str().map_err(|e| e.to_string())
}

unsafe fn write_error(output: *mut c_char, size: usize, error: impl std::fmt::Display) {
    if !output.is_null() && size > 0 {
        let message = error.to_string();
        let length = message.len().min(size - 1);
        ptr::copy_nonoverlapping(message.as_ptr(), output.cast(), length);
        *output.add(length) = 0;
    }
}

fn connect(
    url: &str,
    protocol: &str,
    headers: &str,
    timeout_ms: u32,
    limit: usize,
) -> Result<Connection, String> {
    let mut request = url.into_client_request().map_err(|e| e.to_string())?;
    if !protocol.is_empty() {
        request.headers_mut().insert(
            SEC_WEBSOCKET_PROTOCOL,
            HeaderValue::from_str(protocol).map_err(|e| e.to_string())?,
        );
    }
    for header in headers.lines() {
        let (name, value) = header.split_once(':').ok_or("请求头格式无效")?;
        request.headers_mut().append(
            HeaderName::from_bytes(name.trim().as_bytes()).map_err(|e| e.to_string())?,
            HeaderValue::from_str(value.trim()).map_err(|e| e.to_string())?,
        );
    }
    if !matches!(request.uri().scheme_str(), Some("ws" | "wss")) {
        return Err("只支持 ws/wss".into());
    }
    let host = request
        .uri()
        .host()
        .ok_or("缺少主机名")?
        .trim_start_matches('[')
        .trim_end_matches(']');
    let port = request
        .uri()
        .port_u16()
        .unwrap_or(if request.uri().scheme_str() == Some("wss") {
            443
        } else {
            80
        });
    let timeout = Duration::from_millis(u64::from(timeout_ms.max(1)));
    let deadline = Instant::now() + timeout;
    let addresses = (host, port).to_socket_addrs().map_err(|e| e.to_string())?;
    let mut connected = Err(io::Error::new(io::ErrorKind::NotFound, "主机没有可用地址"));
    for address in addresses {
        let remaining = deadline.saturating_duration_since(Instant::now());
        if remaining.is_zero() {
            break;
        }
        connected = TcpStream::connect_timeout(&address, remaining);
        if connected.is_ok() {
            break;
        }
    }
    let stream = connected.map_err(|e| e.to_string())?;
    stream
        .set_read_timeout(Some(timeout))
        .map_err(|e| e.to_string())?;
    stream
        .set_write_timeout(Some(timeout))
        .map_err(|e| e.to_string())?;
    let config = WebSocketConfig {
        max_message_size: Some(limit),
        max_frame_size: Some(limit),
        max_write_buffer_size: limit * 2 + 1024,
        ..Default::default()
    };
    // rustls 校验证书链、有效期和域名，所有平台使用同一套 Mozilla 信任根。
    let (socket, _) =
        client_tls_with_config(request, stream, Some(config), None).map_err(|e| e.to_string())?;
    let tcp = match socket.get_ref() {
        MaybeTlsStream::Plain(tcp) => tcp,
        MaybeTlsStream::Rustls(tls) => &tls.sock,
        _ => return Err("未知 TLS 后端".into()),
    };
    tcp.set_read_timeout(Some(Duration::from_millis(50)))
        .map_err(|e| e.to_string())?;
    tcp.set_write_timeout(Some(Duration::from_secs(5)))
        .map_err(|e| e.to_string())?;
    // 握手后使用非阻塞收发，分片地图也不能让退出等待被对端无限延长。
    tcp.set_nonblocking(true).map_err(|e| e.to_string())?;
    Ok(Connection {
        socket,
        received: Vec::new(),
    })
}

/// 建立连接；失败返回空句柄，错误写入调用方缓冲区。
#[no_mangle]
pub unsafe extern "C" fn qm_ws_connect(
    url: *const c_char,
    protocol: *const c_char,
    headers: *const c_char,
    timeout_ms: u32,
    limit: usize,
    error: *mut c_char,
    error_size: usize,
) -> *mut c_void {
    let result = (|| {
        connect(
            text(url)?,
            text(protocol)?,
            text(headers)?,
            timeout_ms,
            limit,
        )
    })();
    match result {
        Ok(connection) => Box::into_raw(Box::new(connection)).cast(),
        Err(reason) => {
            write_error(error, error_size, reason);
            ptr::null_mut()
        }
    }
}

/// 发送完整消息；0/1/2 分别表示文本、二进制、心跳。
#[no_mangle]
pub unsafe extern "C" fn qm_ws_send(
    handle: *mut c_void,
    kind: i32,
    data: *const u8,
    size: usize,
    error: *mut c_char,
    error_size: usize,
) -> bool {
    let connection = &mut *handle.cast::<Connection>();
    let bytes = if size == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(data, size)
    };
    let message = match kind {
        0 => match std::str::from_utf8(bytes) {
            Ok(value) => Message::Text(value.to_owned()),
            Err(reason) => {
                write_error(error, error_size, reason);
                return false;
            }
        },
        1 => Message::Binary(bytes.to_vec()),
        2 => Message::Ping(bytes.to_vec()),
        _ => return false,
    };
    match connection.socket.send(message) {
        Ok(()) => true,
        // tungstenite 已保留待发数据，后续 read/flush 继续发送，不能再次入队同一消息。
        Err(Error::Io(ref reason)) if reason.kind() == io::ErrorKind::WouldBlock => true,
        Err(reason) => {
            write_error(error, error_size, reason);
            false
        }
    }
}

/// 接收返回 0=暂无消息、1=文本、2=二进制、3=pong、-1=失败、-2=正常关闭。
/// 数据指针在下一次 read 或 close 前有效，调用方必须立即复制。
#[no_mangle]
pub unsafe extern "C" fn qm_ws_read(
    handle: *mut c_void,
    data: *mut *const u8,
    size: *mut usize,
    error: *mut c_char,
    error_size: usize,
) -> i32 {
    let connection = &mut *handle.cast::<Connection>();
    if let Err(reason) = connection.socket.flush() {
        if !matches!(&reason, Error::Io(io) if io.kind() == io::ErrorKind::WouldBlock) {
            write_error(error, error_size, reason);
            return -1;
        }
    }
    match connection.socket.read() {
        Ok(message) => {
            let kind = match &message {
                Message::Text(_) => 1,
                Message::Binary(_) => 2,
                Message::Pong(_) => 3,
                Message::Close(_) => return -2,
                _ => return 0,
            };
            connection.received = message.into_data();
            *data = connection.received.as_ptr();
            *size = connection.received.len();
            kind
        }
        Err(Error::Io(ref reason))
            if matches!(
                reason.kind(),
                io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut
            ) =>
        {
            0
        }
        Err(Error::ConnectionClosed | Error::AlreadyClosed) => -2,
        Err(reason) => {
            write_error(error, error_size, reason);
            -1
        }
    }
}

/// 释放句柄前发送关闭帧；调用方须先完成待发送业务消息。
#[no_mangle]
pub unsafe extern "C" fn qm_ws_close(handle: *mut c_void) {
    if !handle.is_null() {
        let mut connection = Box::from_raw(handle.cast::<Connection>());
        let _ = connection.socket.close(None);
        let deadline = Instant::now() + Duration::from_secs(1);
        while Instant::now() < deadline {
            match connection.socket.flush() {
                Err(Error::Io(ref reason)) if reason.kind() == io::ErrorKind::WouldBlock => {
                    std::thread::sleep(Duration::from_millis(5));
                }
                _ => break,
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn invalid_header_fails_before_network() {
        assert!(connect("wss://qmclient.icu/ws", "qmclient-json", "invalid", 1, 1024).is_err());
    }

    #[test]
    fn invalid_scheme_is_rejected() {
        assert!(connect("https://qmclient.icu/ws", "", "", 1, 1024).is_err());
    }
}
