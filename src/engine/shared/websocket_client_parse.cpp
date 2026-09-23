// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "websocket_client.h"

#include <base/system.h>

#include <cstdlib>
#include <cstring>

namespace
{
	constexpr size_t QM_WS_MAX_URL_LENGTH = 2048;

	bool ParsePort(const std::string &Text, int &OutPort)
	{
		if(Text.empty() || Text.size() > 5)
			return false;
		int Port = 0;
		for(const char Ch : Text)
		{
			if(Ch < '0' || Ch > '9')
				return false;
			Port = Port * 10 + (Ch - '0');
		}
		if(Port < 1 || Port > 65535)
			return false;
		OutPort = Port;
		return true;
	}

	// 校验主机名：允许字母/数字/点/连字符/下划线，以及 IPv6 字面量里的冒号。
	bool IsValidHost(const std::string &Host)
	{
		if(Host.empty() || Host.size() > 255)
			return false;
		for(const char Ch : Host)
		{
			const bool Ok = (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') ||
					(Ch >= '0' && Ch <= '9') || Ch == '.' || Ch == '-' || Ch == '_' || Ch == ':';
			if(!Ok)
				return false;
		}
		return true;
	}
}

std::string ParseQmWebSocketUrl(const char *pUrl, SQmWebSocketConnectConfig &Out)
{
	if(pUrl == nullptr || pUrl[0] == '\0')
		return "地址为空";

	const std::string Url(pUrl);
	if(Url.size() > QM_WS_MAX_URL_LENGTH)
		return "地址过长";

	SQmWebSocketConnectConfig Config;
	size_t Pos = 0;
	if(Url.compare(0, 5, "ws://") == 0)
	{
		Config.m_UseTls = false;
		Config.m_Port = 80;
		Pos = 5;
	}
	else if(Url.compare(0, 6, "wss://") == 0)
	{
		Config.m_UseTls = true;
		Config.m_Port = 443;
		Pos = 6;
	}
	else
	{
		return "只支持 ws:// 或 wss:// 地址";
	}

	const size_t PathPos = Url.find('/', Pos);
	const std::string Authority = Url.substr(Pos, PathPos == std::string::npos ? std::string::npos : PathPos - Pos);
	Config.m_Path = PathPos == std::string::npos ? std::string("/") : Url.substr(PathPos);
	if(Authority.empty())
		return "地址缺少主机名";
	if(Config.m_Path.find('#') != std::string::npos)
		return "地址不应包含片段(#)";

	std::string HostText = Authority;
	const size_t PortPos = Authority.rfind(':');
	bool HasPort = false;
	if(PortPos != std::string::npos && Authority.find(']') == std::string::npos)
	{
		HasPort = true;
		HostText = Authority.substr(0, PortPos);
		if(!ParsePort(Authority.substr(PortPos + 1), Config.m_Port))
			return "端口无效";
	}
	else if(PortPos != std::string::npos)
	{
		// IPv6 字面量必须写成 [::1] 或 [::1]:8080。
		if(Authority[0] != '[')
			return "IPv6 主机需要用方括号包裹";
		const size_t Close = Authority.find(']');
		HostText = Authority.substr(1, Close - 1);
		if(Close + 1 < Authority.size())
		{
			if(Authority[Close + 1] != ':')
				return "方括号后只能是端口";
			HasPort = true;
			if(!ParsePort(Authority.substr(Close + 2), Config.m_Port))
				return "端口无效";
		}
	}
	(void)HasPort;

	if(!IsValidHost(HostText))
		return "主机名含非法字符";
	Config.m_Host = HostText;

	Out = std::move(Config);
	return "";
}

int QmWebSocketBackoffDelayMs(int Attempt, int BaseMs, int MaxMs)
{
	if(BaseMs <= 0)
		BaseMs = 1000;
	if(MaxMs < BaseMs)
		MaxMs = BaseMs;
	if(Attempt < 0)
		Attempt = 0;
	// 封顶到 1 小时，避免移位溢出。
	if(Attempt > 20)
		Attempt = 20;

	int64_t Delay = BaseMs;
	for(int i = 0; i < Attempt; i++)
	{
		Delay *= 2;
		if(Delay >= MaxMs)
		{
			Delay = MaxMs;
			break;
		}
	}
	if(Delay > MaxMs)
		Delay = MaxMs;

	// 0% ~ 25% 抖动，避免所有客户端同时重连。
	if(Delay > 0)
		Delay += (int64_t)(((uint64_t)rand() % (uint64_t)(Delay / 4 + 1)));
	return (int)Delay;
}
