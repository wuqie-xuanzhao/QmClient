// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_PERF_LOGGING_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_PERF_LOGGING_H

#include <base/log.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>

#include <cinttypes>
#include <cstring>

inline bool QmPerfEnabled()
{
	return g_Config.m_QmPerfDebug != 0 || g_Config.m_QmPerfLogfile != 0 || g_Config.m_QmPerfStutterDiagnostics != 0;
}

inline bool QmMacosGraphicsDiagnosticsEnabled()
{
	return g_Config.m_QmMacosGraphicsDiagnostics != 0;
}

inline double QmPerfThresholdMs()
{
	const double Configured = g_Config.m_QmPerfDebugThresholdMs > 0 ? g_Config.m_QmPerfDebugThresholdMs : 1.0;
	const double StutterBudget = 1000.0 / 300.0;
	return g_Config.m_QmPerfStutterDiagnostics != 0 && Configured > StutterBudget ? StutterBudget : Configured;
}

inline bool QmPerfShouldLogDuration(double DurationMs, bool Force = false)
{
	return Force || DurationMs >= QmPerfThresholdMs();
}

inline uint64_t QmPerfSessionId()
{
	static const uint64_t s_SessionId = (uint64_t)time_timestamp();
	return s_SessionId;
}

inline uint64_t QmPerfFrameId(const IClient *pClient)
{
	return pClient != nullptr ? pClient->PerfFrame() : 0;
}

inline bool QmPerfTokenLooksNumeric(const char *pValue)
{
	if(pValue == nullptr || pValue[0] == '\0')
		return false;
	int Pos = (pValue[0] == '-' || pValue[0] == '+') ? 1 : 0;
	bool HasDigit = false;
	for(; pValue[Pos] != '\0'; ++Pos)
	{
		const char c = pValue[Pos];
		if(c >= '0' && c <= '9')
		{
			HasDigit = true;
			continue;
		}
		if(c == '.')
			continue;
		return false;
	}
	return HasDigit;
}

inline void QmPerfAppendCommonKeyValue(char *pBuf, int BufSize, const IClient *pClient, const char *pPage = nullptr, const char *pTab = nullptr)
{
	char aCommon[128];
	str_format(aCommon, sizeof(aCommon), " frame=%" PRIu64 " session=%" PRIu64, QmPerfFrameId(pClient), QmPerfSessionId());
	str_append(pBuf, aCommon, BufSize);
	if(pPage != nullptr && pPage[0] != '\0')
	{
		char aPage[128];
		str_format(aPage, sizeof(aPage), " page=%s", pPage);
		str_append(pBuf, aPage, BufSize);
	}
	if(pTab != nullptr && pTab[0] != '\0')
	{
		char aTab[128];
		str_format(aTab, sizeof(aTab), " tab=%s", pTab);
		str_append(pBuf, aTab, BufSize);
	}
}

inline void QmPerfAppendJsonField(char *pBuf, int BufSize, bool &First, const char *pKey, const char *pValue)
{
	if(pKey == nullptr || pKey[0] == '\0' || pValue == nullptr)
		return;
	if(!First)
		str_append(pBuf, ",", BufSize);
	First = false;

	char aEscaped[512];
	EscapeJson(aEscaped, sizeof(aEscaped), pValue);
	str_append(pBuf, "\"", BufSize);
	str_append(pBuf, pKey, BufSize);
	str_append(pBuf, "\":", BufSize);
	if(QmPerfTokenLooksNumeric(pValue))
		str_append(pBuf, pValue, BufSize);
	else
	{
		str_append(pBuf, "\"", BufSize);
		str_append(pBuf, aEscaped, BufSize);
		str_append(pBuf, "\"", BufSize);
	}
}

inline bool QmPerfPayloadLooksLikeKeyValueStart(const char *pTokenStart)
{
	if(pTokenStart == nullptr || pTokenStart[0] == '\0')
		return false;
	bool HasKeyChar = false;
	for(int Pos = 0; pTokenStart[Pos] != '\0'; ++Pos)
	{
		const char c = pTokenStart[Pos];
		if(c == '=')
			return HasKeyChar;
		if((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '_')
		{
			HasKeyChar = true;
			continue;
		}
		return false;
	}
	return false;
}

inline void QmPerfAppendPayloadJsonFields(char *pBuf, int BufSize, bool &First, const char *pPayload)
{
	if(pPayload == nullptr || pPayload[0] == '\0')
		return;

	for(const char *pCursor = pPayload; pCursor != nullptr && pCursor[0] != '\0';)
	{
		while(*pCursor == ' ')
			++pCursor;
		if(*pCursor == '\0')
			break;

		const char *pEqual = std::strchr(pCursor, '=');
		if(pEqual == nullptr)
			break;

		const ptrdiff_t KeyLength = pEqual - pCursor;
		if(KeyLength <= 0)
		{
			pCursor = pEqual + 1;
			continue;
		}

		char aKey[128];
		const int KeyCopyLength = minimum((int)KeyLength, (int)sizeof(aKey) - 1);
		mem_copy(aKey, pCursor, KeyCopyLength);
		aKey[KeyCopyLength] = '\0';

		const char *pValueStart = pEqual + 1;
		const char *pValueEnd = pValueStart;
		char aValue[512];
		if(*pValueStart == '"')
		{
			++pValueStart;
			pValueEnd = pValueStart;
			while(*pValueEnd != '\0' && *pValueEnd != '"')
				++pValueEnd;
		}
		else
		{
			while(*pValueEnd != '\0')
			{
				if(*pValueEnd == ' ')
				{
					const char *pNextToken = pValueEnd + 1;
					while(*pNextToken == ' ')
						++pNextToken;
					if(QmPerfPayloadLooksLikeKeyValueStart(pNextToken))
						break;
				}
				++pValueEnd;
			}
		}

		const ptrdiff_t ValueLength = pValueEnd - pValueStart;
		const int ValueCopyLength = maximum(0, minimum((int)ValueLength, (int)sizeof(aValue) - 1));
		mem_copy(aValue, pValueStart, ValueCopyLength);
		aValue[ValueCopyLength] = '\0';
		QmPerfAppendJsonField(pBuf, BufSize, First, aKey, aValue);

		pCursor = pValueEnd;
		if(*pCursor == '"')
			++pCursor;
	}
}

inline void QmPerfLogPayloadUnchecked(const char *pSystem, const char *pPayload, const IClient *pClient = nullptr, const char *pPage = nullptr, const char *pTab = nullptr)
{
	char aJson[2048];
	bool First = true;
	str_copy(aJson, "{", sizeof(aJson));
	QmPerfAppendJsonField(aJson, sizeof(aJson), First, "system", pSystem != nullptr ? pSystem : "");
	char aFrame[64];
	char aSession[64];
	str_format(aFrame, sizeof(aFrame), "%" PRIu64, QmPerfFrameId(pClient));
	str_format(aSession, sizeof(aSession), "%" PRIu64, QmPerfSessionId());
	QmPerfAppendJsonField(aJson, sizeof(aJson), First, "frame", aFrame);
	QmPerfAppendJsonField(aJson, sizeof(aJson), First, "session", aSession);
	if(pPage != nullptr && pPage[0] != '\0')
		QmPerfAppendJsonField(aJson, sizeof(aJson), First, "page", pPage);
	if(pTab != nullptr && pTab[0] != '\0')
		QmPerfAppendJsonField(aJson, sizeof(aJson), First, "tab", pTab);
	QmPerfAppendPayloadJsonFields(aJson, sizeof(aJson), First, pPayload);
	str_append(aJson, "}", sizeof(aJson));
	dbg_msg(pSystem, "%s", aJson);
}

inline void QmPerfLogPayload(const char *pSystem, const char *pPayload, const IClient *pClient = nullptr, const char *pPage = nullptr, const char *pTab = nullptr)
{
	if(!QmPerfEnabled())
		return;
	QmPerfLogPayloadUnchecked(pSystem, pPayload, pClient, pPage, pTab);
}

inline void QmPerfLogPayloadForce(const char *pSystem, const char *pPayload, const IClient *pClient = nullptr, const char *pPage = nullptr, const char *pTab = nullptr)
{
	QmPerfLogPayloadUnchecked(pSystem, pPayload, pClient, pPage, pTab);
}

inline void QmMacosGraphicsDiagnosticsLogPayload(const char *pSystem, const char *pPayload, const IClient *pClient = nullptr)
{
	if(!QmMacosGraphicsDiagnosticsEnabled())
		return;
	QmPerfLogPayloadUnchecked(pSystem, pPayload, pClient);
}

inline void QmPerfLogStage(const char *pSystem, const char *pStage, double DurationMs, bool Force = false, const IClient *pClient = nullptr, const char *pPage = nullptr, const char *pTab = nullptr, const char *pExtra = nullptr)
{
	if(!QmPerfEnabled())
		return;
	if(!QmPerfShouldLogDuration(DurationMs, Force))
		return;

	char aPayload[1024];
	if(pExtra != nullptr && pExtra[0] != '\0')
		str_format(aPayload, sizeof(aPayload), "stage=%s duration_ms=%.3f %s", pStage, DurationMs, pExtra);
	else
		str_format(aPayload, sizeof(aPayload), "stage=%s duration_ms=%.3f", pStage, DurationMs);
	QmPerfLogPayload(pSystem, aPayload, pClient, pPage, pTab);
}

#endif
