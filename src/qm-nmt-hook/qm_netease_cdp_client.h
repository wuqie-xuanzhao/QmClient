#ifndef QM_NMT_HOOK_QM_NETEASE_CDP_CLIENT_H
#define QM_NMT_HOOK_QM_NETEASE_CDP_CLIENT_H

#include "qm_netease_cdp.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace NeteaseLyrics
{
	struct SRawLyrics;
}

namespace QmNeteaseCdp
{
	class CCdpSession
	{
	public:
		using TNotificationCallback = std::function<void(std::string_view)>;

		CCdpSession();
		~CCdpSession();
		CCdpSession(const CCdpSession &) = delete;
		CCdpSession &operator=(const CCdpSession &) = delete;

		bool Connect(std::string_view WebSocketUrl);
		void Close();
		bool IsConnected() const;
		void SetNotificationCallback(TNotificationCallback Callback);
		bool Command(std::string_view Method, std::string_view ParametersJson, std::string *pResultJson, int TimeoutMs = 1500);
		bool Evaluate(std::string_view Expression, std::string *pValueJson, int TimeoutMs = 1500);
		bool Pump(int TimeoutMs = 250);

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};

	// 独立 worker：网络、WebSocket、重连和 JS hook 生命周期都不占用游戏
	// render thread。回调只携带不可变的 JSON 文本。
	class CFrontendBridgeWorker
	{
	public:
		using TReportCallback = std::function<void(std::string_view)>;

		CFrontendBridgeWorker();
		~CFrontendBridgeWorker();
		CFrontendBridgeWorker(const CFrontendBridgeWorker &) = delete;
		CFrontendBridgeWorker &operator=(const CFrontendBridgeWorker &) = delete;

		bool Start(uint32_t CloudMusicPid, std::wstring CommandLine, TReportCallback Callback);
		void Stop();
		bool IsConnected() const;

	private:
		void Run();
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};

	// 可由测试和日志使用的幂等 JS payload。payload 不包含 cookie、header
	// 或任何用户 session 数据。
	std::string BuildInstallHookScript();
	// 仅按当前 songId 请求网易云自身接口；不做搜索、模糊匹配或第三方源。
	std::wstring BuildNeteaseLyricRequestPath(uint64_t SongId);
	bool FetchNeteaseLyricEndpoint(uint64_t SongId, NeteaseLyrics::SRawLyrics *pOut, int TimeoutMs = 1500);

} // namespace QmNeteaseCdp

#endif
