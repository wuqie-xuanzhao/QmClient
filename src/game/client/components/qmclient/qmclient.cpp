// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qmclient.h"

#include <base/hash.h>
#include <base/lock.h>
#include <base/log.h>
#include <base/str.h>
#include <base/system.h>
#include <base/windows.h>

#include <engine/client.h>
#include <engine/client/enums.h>
#include <engine/client/updater.h>
#include <engine/engine.h>
#include <engine/external/regex.h>
#include <engine/external/tinyexpr.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/storage.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/ui.h>
#include <game/layers.h>
#include <game/localization.h>
#include <game/mapitems.h>
#include <game/version.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <limits>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>
#endif

[[maybe_unused]] static constexpr const char *TCLIENT_INFO_URL = "http://42.194.185.210:8080/client/version";
[[maybe_unused]] static constexpr const char *TCLIENT_UPDATE_EXE_URL = "https://github.com/wxj881027/QmClient/releases/latest/download/DDNet.exe";
[[maybe_unused]] static constexpr const char *MAP_CATEGORY_CACHE_FILE = "qmclient/map_categories.json";
[[maybe_unused]] static constexpr int64_t MAP_CATEGORY_CACHE_SAVE_DELAY_SEC = 5;
static constexpr int QMCLIENT_SYNC_INTERVAL_SECONDS = 30;
static constexpr int QMCLIENT_VOICE_SYNC_INTERVAL_SECONDS = 10;
static constexpr const char *QMCLIENT_DEFAULT_VOICE_SERVER = "42.194.185.210:9987";
// Recognition, online-user distribution and voice presence are served by the
// voice service itself (default :9987) and use the /qm/* namespace.
static constexpr const char *QMCLIENT_TOKEN_PATH = "/qm/token";
static constexpr const char *QMCLIENT_REPORT_PATH = "/qm/report";
static constexpr const char *QMCLIENT_USERS_PATH = "/qm/users.json";
static void LogQmClientRecognitionEvent(const char *pStage, const char *pDetail)
{
	log_info("qmclient", "recognition %s: %s", pStage, pDetail ? pDetail : "");
}

static void LogQmClientDistributionRequestEvent(const char *pStage, const char *pDetail)
{
	log_info("qmclient", "distribution %s: %s", pStage, pDetail ? pDetail : "");
}

static void LogQmClientDistributionEvent(const char *pStage, int Users, int Dummies, int LocalMarks)
{
	log_info("qmclient", "distribution %s: users=%d dummies=%d local_marks=%d", pStage, Users, Dummies, LocalMarks);
}

static void LogQmClientDistributionFailureEvent(const char *pStage, const char *pDetail)
{
	log_warn("qmclient", "distribution %s: %s", pStage, pDetail ? pDetail : "");
}

static void LogQmClientVoicePresenceResultEvent(const char *pStage, const char *pServerAddress, int Players, int StatusCode, EHttpState State)
{
	log_warn("qmclient", "voice_presence %s: server='%s' players=%d status=%d state=%d",
		pStage,
		pServerAddress ? pServerAddress : "",
		Players,
		StatusCode,
		(int)State);
}
// Version/health/playtime endpoints are provided by the separate center HTTP
// service (:8080) and intentionally do not use the /qm/* prefix.
static constexpr const char *QMCLIENT_HEALTH_URL = "http://42.194.185.210:8080/healthz";
static constexpr const char *QMCLIENT_PLAYTIME_START_URL = "http://42.194.185.210:8080/playtime/start";
static constexpr const char *QMCLIENT_PLAYTIME_STOP_URL = "http://42.194.185.210:8080/playtime/stop";
static constexpr const char *QMCLIENT_PLAYTIME_QUERY_URL = "http://42.194.185.210:8080/playtime/query";
static constexpr const char *QMCLIENT_DEVELOPER_PRESENCE_URL = "https://qmclient.icu/api/v1/developers/presence";
static constexpr const char *QMCLIENT_DEVELOPER_PRESENCES_URL = "https://qmclient.icu/api/v1/developers/presences";
static constexpr const char *QMCLIENT_DEVELOPER_TOKEN_FILE = "qmclient/developer_token.txt";
static constexpr int QMCLIENT_DEVELOPER_SYNC_INTERVAL_SECONDS = 5;
static constexpr const char *QMCLIENT_LIFECYCLE_MARKER_FILE = "qmclient/lifecycle_pending.marker";
static constexpr const char *QMCLIENT_PLAYTIME_CLIENT_ID_FILE = "qmclient/playtime_client_id.txt";
static constexpr const char *QMCLIENT_MACHINE_ID_FALLBACK_FILE = "qmclient/voice_machine_id.txt";
static constexpr int QMCLIENT_SERVER_TIME_SYNC_INTERVAL_SECONDS = 15;
static constexpr int QMCLIENT_PLAYTIME_QUERY_INTERVAL_SECONDS = 10;
static constexpr int QMCLIENT_RECOVERY_RETRY_SECONDS = 3;
static constexpr int QMCLIENT_MARKER_FLUSH_INTERVAL_SECONDS = 5;
static constexpr const char *DDNET_PLAYER_STATS_URL = "https://ddnet.org/players/?json2=";
static constexpr int QMCLIENT_DDNET_PLAYER_SYNC_INTERVAL_SECONDS = 120;
static constexpr int QMCLIENT_DDNET_PLAYER_RETRY_DELAY_SECONDS = 10;
static constexpr const char *QMCLIENT_FREEZE_WAKEUP_TEXT = "快醒醒!";
static constexpr const char *QMCLIENT_LOCAL_MODE_STATS_FILE = "qmclient/statistics.json";

static bool ParseStrictInt64(const char *pText, int64_t &Out)
{
	if(!pText || pText[0] == '\0')
		return false;
	Out = str_toint64_base(pText);
	char aCanonical[64];
	str_format(aCanonical, sizeof(aCanonical), "%" PRId64, Out);
	return str_comp(aCanonical, pText) == 0;
}

[[maybe_unused]] static bool TextContainsAny(const char *pText, const std::initializer_list<const char *> &Tokens)
{
	if(!pText || pText[0] == '\0')
		return false;

	for(const char *pToken : Tokens)
	{
		if(pToken && pToken[0] != '\0' && str_find_nocase(pText, pToken))
			return true;
	}
	return false;
}
[[maybe_unused]] static constexpr float QMCLIENT_TEXT_POPUP_FONT_SIZE = 30.0f;
[[maybe_unused]] static constexpr vec2 QMCLIENT_FREEZE_WAKEUP_POPUP_OFFSET = vec2(34.0f, -78.0f);
[[maybe_unused]] static constexpr vec2 QMCLIENT_FREEZE_WAKEUP_POPUP_DRIFT = vec2(18.0f, -16.0f);
[[maybe_unused]] static constexpr int QMCLIENT_COMBO_POPUP_WINDOW_SECONDS = 2;
[[maybe_unused]] static constexpr ColorRGBA QMCLIENT_POPUP_ROLL_COLOR_FROM = ColorRGBA(0.0f, 1.0f, 1.0f, 1.0f);
[[maybe_unused]] static constexpr ColorRGBA QMCLIENT_POPUP_ROLL_COLOR_TO = ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f);
[[maybe_unused]] static constexpr const char *s_apKeywordNegationWords[] = {
	"不",
	"没",
	"無",
	"无",
	"別",
	"别",
	"勿",
	"莫",
	"非",
	"未",
	"沒",
};
[[maybe_unused]] static constexpr const char *s_apKeywordClauseContrastWords[] = {
	"但是",
	"但",
	"不过",
	"然而",
	"可是",
};
// 与 tclient 一致：默认英文 source key，空模板回退用
static constexpr const char *s_pFriendEnterBroadcastDefaultText = "%s joined this server";

[[maybe_unused]] static int AutoReplySeparatorLength(const char *pStr);
[[maybe_unused]] static bool AppendAutoReplyRuleBlock(char *pOutRules, size_t OutRulesSize, const char *pRules);
static const json_value *JsonObjectField(const json_value *pObject, const char *pName);
static bool JsonReadNonNegativeInt64(const json_value *pValue, int64_t &OutValue);

namespace
{
	enum class ETextPopupType
	{
		FREEZE_WAKEUP = 0,
		NUM_TYPES,
	};

	struct STextPopupDefinition
	{
		const char *m_pText;
	};

	[[maybe_unused]] static constexpr std::array<STextPopupDefinition, (int)ETextPopupType::NUM_TYPES> s_aTextPopupDefinitions = {{
		{QMCLIENT_FREEZE_WAKEUP_TEXT},
	}};

	class CQmClientUsersParseJob : public IJob
	{
	public:
		using SResult = SQmClientUsersParseResult;

	private:
		std::shared_ptr<IHttpRequest> m_pTask;
		char m_aServerAddress[NETADDR_MAXSTRSIZE] = "";
		int64_t m_ExpireTick = 0;
		CLock m_Lock;
		SResult m_Result;

	protected:
		void Run() override REQUIRES(!m_Lock)
		{
			SResult Result;
			if(m_pTask && m_pTask->State() == EHttpState::DONE)
			{
				json_value *pRoot = m_pTask->ResultJson();
				if(pRoot != nullptr)
				{
					ParseQmClientUsersJson(pRoot, m_aServerAddress, Result);
					json_value_free(pRoot);
				}
			}

			{
				const CLockScope Lock(m_Lock);
				m_Result = std::move(Result);
			}
			m_pTask = nullptr;
		}

	public:
		CQmClientUsersParseJob(std::shared_ptr<IHttpRequest> pTask, const char *pServerAddress, int64_t ExpireTick) :
			m_pTask(std::move(pTask)),
			m_ExpireTick(ExpireTick)
		{
			str_copy(m_aServerAddress, pServerAddress, sizeof(m_aServerAddress));
		}

		SResult TakeResult() REQUIRES(!m_Lock)
		{
			const CLockScope Lock(m_Lock);
			SResult Result = std::move(m_Result);
			m_Result = SResult();
			return Result;
		}

		int64_t ExpireTick() const { return m_ExpireTick; }
	};

	class CQmDdnetPlayerStatsParseJob : public IJob
	{
	public:
		struct SResult
		{
			bool m_Parsed = false;
			std::string m_FavoritePartner;
			int m_TotalFinishes = -1;
			int64_t m_Points = -1;
			int64_t m_PointsTotal = -1;
		};

	private:
		std::shared_ptr<IHttpRequest> m_pTask;
		CLock m_Lock;
		SResult m_Result;

	protected:
		void Run() override REQUIRES(!m_Lock)
		{
			SResult Result;
			if(m_pTask && m_pTask->State() == EHttpState::DONE && m_pTask->StatusCode() == 200)
			{
				json_value *pRoot = m_pTask->ResultJson();
				if(pRoot && pRoot->type == json_object)
				{
					const json_value *pPoints = JsonObjectField(pRoot, "points");
					if(pPoints != &json_value_none && pPoints->type == json_object)
					{
						const json_value *pCurrent = JsonObjectField(pPoints, "points");
						const json_value *pTotal = JsonObjectField(pPoints, "total");
						if(pCurrent != &json_value_none && pCurrent->type == json_integer && pCurrent->u.integer >= 0)
							Result.m_Points = pCurrent->u.integer;
						if(pTotal != &json_value_none && pTotal->type == json_integer && pTotal->u.integer >= 0)
							Result.m_PointsTotal = pTotal->u.integer;
					}

					const json_value *pFavoritePartners = JsonObjectField(pRoot, "favorite_partners");
					if(pFavoritePartners->type == json_array)
					{
						const char *pBestPartner = nullptr;
						int BestPartnerFinishes = -1;
						for(unsigned i = 0; i < pFavoritePartners->u.array.length; ++i)
						{
							const json_value &Partner = (*pFavoritePartners)[i];
							if(Partner.type != json_object)
								continue;

							const json_value *pName = JsonObjectField(&Partner, "name");
							if(pName->type != json_string)
								continue;

							const char *pPartnerName = json_string_get(pName);
							if(!pPartnerName || pPartnerName[0] == '\0')
								continue;

							int PartnerFinishes = 0;
							const json_value *pFinishes = JsonObjectField(&Partner, "finishes");
							if(pFinishes->type == json_integer && pFinishes->u.integer > 0)
							{
								if(pFinishes->u.integer > std::numeric_limits<int>::max())
									PartnerFinishes = std::numeric_limits<int>::max();
								else
									PartnerFinishes = (int)pFinishes->u.integer;
							}

							if(!pBestPartner ||
								PartnerFinishes > BestPartnerFinishes ||
								(PartnerFinishes == BestPartnerFinishes && str_comp_nocase(pPartnerName, pBestPartner) < 0))
							{
								pBestPartner = pPartnerName;
								BestPartnerFinishes = PartnerFinishes;
							}
						}

						if(pBestPartner)
							Result.m_FavoritePartner = pBestPartner;
					}

					const json_value *pTypes = JsonObjectField(pRoot, "types");
					if(pTypes->type == json_object)
					{
						Result.m_Parsed = true;
						int64_t TotalFinishes = 0;
						for(unsigned i = 0; i < pTypes->u.object.length; ++i)
						{
							const json_value *pTypeObj = pTypes->u.object.values[i].value;
							if(!pTypeObj || pTypeObj->type != json_object)
								continue;

							const json_value *pMaps = JsonObjectField(pTypeObj, "maps");
							if(pMaps->type != json_object)
								continue;

							for(unsigned j = 0; j < pMaps->u.object.length; ++j)
							{
								const json_value *pMapObj = pMaps->u.object.values[j].value;
								if(!pMapObj || pMapObj->type != json_object)
									continue;

								const json_value *pFinishes = JsonObjectField(pMapObj, "finishes");
								if(pFinishes->type != json_integer || pFinishes->u.integer <= 0 || TotalFinishes >= std::numeric_limits<int>::max())
									continue;

								int64_t SafeAdd = pFinishes->u.integer;
								if(SafeAdd > std::numeric_limits<int>::max())
									SafeAdd = std::numeric_limits<int>::max();

								const int64_t MaxTotal = std::numeric_limits<int>::max();
								if(SafeAdd > MaxTotal - TotalFinishes)
									TotalFinishes = MaxTotal;
								else
									TotalFinishes += SafeAdd;
							}
						}
						Result.m_TotalFinishes = (int)TotalFinishes;
					}
					json_value_free(pRoot);
				}
			}

			{
				const CLockScope Lock(m_Lock);
				m_Result = std::move(Result);
			}
			m_pTask = nullptr;
		}

	public:
		explicit CQmDdnetPlayerStatsParseJob(std::shared_ptr<IHttpRequest> pTask) :
			m_pTask(std::move(pTask))
		{
		}

		SResult TakeResult() REQUIRES(!m_Lock)
		{
			const CLockScope Lock(m_Lock);
			SResult Result = std::move(m_Result);
			m_Result = SResult();
			return Result;
		}
	};

	class CQmClientLifecycleMarkerWriteJob : public IJob
	{
		IStorage *m_pStorage = nullptr;
		std::string m_Content;
		std::shared_ptr<std::mutex> m_pMutex;

	protected:
		void Run() override
		{
			if(m_pMutex == nullptr)
				return;
			std::lock_guard<std::mutex> Lock(*m_pMutex);
			if(m_pStorage == nullptr || State() == IJob::STATE_ABORTED)
				return;

			m_pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			IOHANDLE File = m_pStorage->OpenFile(QMCLIENT_LIFECYCLE_MARKER_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!File)
				return;
			if(State() == IJob::STATE_ABORTED)
			{
				io_close(File);
				return;
			}

			io_write(File, m_Content.data(), m_Content.size());
			io_close(File);
		}

	public:
		CQmClientLifecycleMarkerWriteJob(IStorage *pStorage, std::string Content, std::shared_ptr<std::mutex> pMutex) :
			m_pStorage(pStorage),
			m_Content(std::move(Content)),
			m_pMutex(std::move(pMutex))
		{
			Abortable(true);
		}
	};

}

// NOLINTNEXTLINE(misc-use-internal-linkage)
struct SKeywordReplyRule
{
	std::string m_Keywords;
	std::string m_Reply;
	bool m_AutoRename = false;
	bool m_Regex = false;
	bool m_HasExplicitRenameFlag = false;
	bool m_HasExplicitRegexFlag = false;
};

static bool ParseQmClientServiceHostPort(const char *pAddrStr, char *pHost, size_t HostSize, int &Port)
{
	const char *pColon = str_rchr(pAddrStr, ':');
	if(!pColon || pColon == pAddrStr || *(pColon + 1) == '\0')
		return false;

	str_truncate(pHost, HostSize, pAddrStr, pColon - pAddrStr);
	if(pHost[0] == '[')
	{
		const int Len = str_length(pHost);
		if(Len >= 2 && pHost[Len - 1] == ']')
		{
			mem_move(pHost, pHost + 1, Len - 2);
			pHost[Len - 2] = '\0';
		}
	}

	Port = str_toint(pColon + 1);
	return Port > 0 && Port <= 65535;
}

// NOLINTNEXTLINE(misc-use-internal-linkage)
const char *GetEffectiveQmVoiceServer()
{
	return g_Config.m_QmVoiceServer[0] != '\0' ? g_Config.m_QmVoiceServer : QMCLIENT_DEFAULT_VOICE_SERVER;
}

static void TrimQmClientTextInPlace(char *pText)
{
	if(!pText || pText[0] == '\0')
		return;
	char *pTrimmed = (char *)str_utf8_skip_whitespaces(pText);
	str_utf8_trim_right(pTrimmed);
	if(pTrimmed != pText)
		mem_move(pText, pTrimmed, str_length(pTrimmed) + 1);
}

static char *ParseAutoReplyRulePrefixes(char *pLine, bool &OutAutoRename, bool &OutRegex, bool &OutHasExplicitRenameFlag, bool &OutHasExplicitRegexFlag)
{
	OutAutoRename = false;
	OutRegex = false;
	OutHasExplicitRenameFlag = false;
	OutHasExplicitRegexFlag = false;

	char *pTrimmedLine = (char *)str_utf8_skip_whitespaces(pLine);
	while(true)
	{
		const char *pAfterPrefix = str_startswith_nocase(pTrimmedLine, "[rename]");
		if(!pAfterPrefix)
			pAfterPrefix = str_startswith_nocase(pTrimmedLine, "[r]");
		if(pAfterPrefix)
		{
			OutAutoRename = true;
			OutHasExplicitRenameFlag = true;
			pTrimmedLine = (char *)str_utf8_skip_whitespaces(pAfterPrefix);
			continue;
		}

		pAfterPrefix = str_startswith_nocase(pTrimmedLine, "[regex]");
		if(!pAfterPrefix)
			pAfterPrefix = str_startswith_nocase(pTrimmedLine, "[re]");
		if(!pAfterPrefix)
			pAfterPrefix = str_startswith_nocase(pTrimmedLine, "[rx]");
		if(pAfterPrefix)
		{
			OutRegex = true;
			OutHasExplicitRegexFlag = true;
			pTrimmedLine = (char *)str_utf8_skip_whitespaces(pAfterPrefix);
			continue;
		}

		break;
	}

	return pTrimmedLine;
}

[[maybe_unused]] static void ParseKeywordReplyRules(const char *pRules, std::vector<SKeywordReplyRule> &vOutRules)
{
	vOutRules.clear();
	if(!pRules || pRules[0] == '\0')
		return;

	const char *pCursor = pRules;
	while(*pCursor)
	{
		char aLine[sizeof(g_Config.m_QmKeywordReplyRules)];
		int LineLen = 0;
		while(*pCursor && *pCursor != '\n' && *pCursor != '\r')
		{
			if(LineLen < (int)sizeof(aLine) - 1)
				aLine[LineLen++] = *pCursor;
			++pCursor;
		}
		aLine[LineLen] = '\0';

		while(*pCursor == '\n' || *pCursor == '\r')
			++pCursor;

		char *pLine = (char *)str_utf8_skip_whitespaces(aLine);
		str_utf8_trim_right(pLine);
		if(pLine[0] == '\0' || pLine[0] == '#')
			continue;

		bool AutoRename = false;
		bool RegexRule = false;
		bool HasExplicitRenameFlag = false;
		bool HasExplicitRegexFlag = false;
		char *pRuleText = ParseAutoReplyRulePrefixes(pLine, AutoRename, RegexRule, HasExplicitRenameFlag, HasExplicitRegexFlag);
		const char *pArrowConst = str_find(pRuleText, "=>");
		if(!pArrowConst)
			continue;

		char *pArrow = pRuleText + (pArrowConst - pRuleText);
		*pArrow = '\0';
		pArrow += 2;

		char *pKeywords = (char *)str_utf8_skip_whitespaces(pRuleText);
		str_utf8_trim_right(pKeywords);
		char *pReply = (char *)str_utf8_skip_whitespaces(pArrow);
		str_utf8_trim_right(pReply);
		if(pKeywords[0] == '\0' || pReply[0] == '\0')
			continue;

		vOutRules.push_back({pKeywords, pReply, AutoRename, RegexRule, HasExplicitRenameFlag, HasExplicitRegexFlag});
	}
}

[[maybe_unused]] static void BuildKeywordReplyRules(const std::vector<SKeywordReplyRule> &vRules, char *pOutRules, size_t OutRulesSize)
{
	if(!pOutRules || OutRulesSize == 0)
		return;

	pOutRules[0] = '\0';
	for(const auto &Rule : vRules)
	{
		if(Rule.m_Keywords.empty() || Rule.m_Reply.empty())
			continue;

		if(pOutRules[0] != '\0')
			str_append(pOutRules, "\n", OutRulesSize);
		if(Rule.m_AutoRename)
			str_append(pOutRules, "[rename] ", OutRulesSize);
		if(Rule.m_Regex)
			str_append(pOutRules, "[regex] ", OutRulesSize);
		str_append(pOutRules, Rule.m_Keywords.c_str(), OutRulesSize);
		str_append(pOutRules, "=>", OutRulesSize);
		str_append(pOutRules, Rule.m_Reply.c_str(), OutRulesSize);
	}
}

[[maybe_unused]] static bool ReadQmClientAbsoluteTextFile(const char *pFilename, char *pBuf, size_t BufSize)
{
	if(!pFilename || !pBuf || BufSize == 0)
		return false;

	IOHANDLE File = io_open(pFilename, IOFLAG_READ);
	if(!File)
		return false;

	const int Read = io_read(File, pBuf, (unsigned)(BufSize - 1));
	io_close(File);
	if(Read <= 0)
		return false;

	pBuf[Read] = '\0';
	TrimQmClientTextInPlace(pBuf);
	return pBuf[0] != '\0';
}

static bool ReadPlatformMachineIdentity(std::string &OutIdentity)
{
#if defined(CONF_FAMILY_WINDOWS)
	HKEY Key = nullptr;
	LONG OpenResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &Key);
	if(OpenResult != ERROR_SUCCESS)
		OpenResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &Key);
	if(OpenResult == ERROR_SUCCESS && Key != nullptr)
	{
		wchar_t aValue[256] = {};
		DWORD Type = 0;
		DWORD Size = sizeof(aValue);
		const LONG QueryResult = RegQueryValueExW(Key, L"MachineGuid", nullptr, &Type, reinterpret_cast<LPBYTE>(aValue), &Size);
		RegCloseKey(Key);
		if(QueryResult == ERROR_SUCCESS && Type == REG_SZ)
		{
			const auto Utf8 = windows_wide_to_utf8(aValue);
			if(Utf8.has_value() && !Utf8->empty())
			{
				OutIdentity = *Utf8;
				return true;
			}
		}
	}
#elif defined(CONF_PLATFORM_LINUX)
	char aBuf[256];
	if(ReadQmClientAbsoluteTextFile("/etc/machine-id", aBuf, sizeof(aBuf)) ||
		ReadQmClientAbsoluteTextFile("/var/lib/dbus/machine-id", aBuf, sizeof(aBuf)))
	{
		OutIdentity = aBuf;
		return true;
	}
#endif

	return false;
}

static bool IsValidQmClientMachineHash(const char *pHash)
{
	if(!pHash || str_length(pHash) != SHA256_DIGEST_LENGTH * 2)
		return false;

	for(const char *p = pHash; *p; ++p)
	{
		if(!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
			return false;
	}
	return true;
}

[[maybe_unused]] static std::string BuildFriendEnterBroadcastText(const char *pTemplate, std::string_view FriendNames)
{
	const char *pFormat = pTemplate != nullptr && pTemplate[0] != '\0' ? pTemplate : s_pFriendEnterBroadcastDefaultText;
	std::string Result;
	Result.reserve(str_length(pFormat) + FriendNames.size() + 8);

	const std::string_view Placeholder = "%s";
	const std::string_view FormatView = pFormat;
	size_t Pos = 0;
	bool Replaced = false;
	while(true)
	{
		const size_t Match = FormatView.find(Placeholder, Pos);
		if(Match == std::string_view::npos)
		{
			Result.append(FormatView.substr(Pos));
			break;
		}

		Result.append(FormatView.substr(Pos, Match - Pos));
		Result.append(FriendNames);
		Pos = Match + Placeholder.size();
		Replaced = true;
	}

	if(!Replaced)
	{
		// Backward compatibility: if users remove '%s', keep friend names visible.
		Result.clear();
		Result.reserve(FriendNames.size() + FormatView.size());
		Result.append(FriendNames);
		Result.append(FormatView);
	}

	return Result;
}

static const json_value *JsonObjectField(const json_value *pObject, const char *pName)
{
	if(!pObject || pObject->type != json_object)
		return &json_value_none;
	return json_object_get(pObject, pName);
}

static bool JsonReadNonNegativeInt64(const json_value *pValue, int64_t &OutValue)
{
	if(!pValue)
		return false;

	if(pValue->type == json_integer)
	{
		if(pValue->u.integer < 0)
			return false;
		OutValue = pValue->u.integer;
		return true;
	}
	if(pValue->type == json_double)
	{
		if(pValue->u.dbl < 0.0)
			return false;
		OutValue = (int64_t)pValue->u.dbl;
		return true;
	}
	return false;
}

static bool IsValidQmClientPlaytimeId(const char *pClientId)
{
	if(!pClientId)
		return false;

	const int Len = str_length(pClientId);
	if(Len < 8 || Len > 64)
		return false;

	for(int i = 0; i < Len; ++i)
	{
		const unsigned char C = (unsigned char)pClientId[i];
		if(std::isalnum(C) || C == '_' || C == '-')
			continue;
		return false;
	}
	return true;
}

void CQmClient::OnInit()
{
	InitQmClientLifecycle();
	LoadQmClientLocalModeStats();
	if(!m_QmStatisticsFileExists)
		SaveQmClientStatistics();
	InitQmDeveloperAuthentication();
}

void CQmClient::LoadQmClientLocalModeStats()
{
	m_vQmClientLocalModeStats.clear();
	m_QmStatisticsFileExists = false;
	char *pJson = Storage()->ReadFileStr(QMCLIENT_LOCAL_MODE_STATS_FILE, IStorage::TYPE_SAVE);
	if(!pJson)
		return;
	json_value *pRoot = JsonParse(pJson, str_length(pJson));
	free(pJson);
	if(!pRoot || pRoot->type != json_object)
	{
		if(pRoot)
			json_value_free(pRoot);
		return;
	}
	const json_value *pLocal = JsonObjectField(pRoot, "local");
	const json_value *pRemote = JsonObjectField(pRoot, "remote");
	if(pLocal->type != json_object || pRemote->type != json_object)
	{
		json_value_free(pRoot);
		return;
	}
	m_QmStatisticsFileExists = true;
	const json_value *pModes = JsonObjectField(pLocal, "modes");
	if(pModes && pModes->type == json_array)
	{
		for(int Index = 0; Index < json_array_length(pModes); ++Index)
		{
			const json_value *pMode = json_array_get(pModes, Index);
			if(!pMode || pMode->type != json_object)
				continue;
			const json_value *pName = JsonObjectField(pMode, "mode");
			const json_value *pCommunity = JsonObjectField(pMode, "community_id");
			const json_value *pAxiom = JsonObjectField(pMode, "axiom");
			const json_value *pMaps = JsonObjectField(pMode, "maps");
			const json_value *pScore = JsonObjectField(pMode, "score");
			const json_value *pPlaytime = JsonObjectField(pMode, "playtime_seconds");
			if(!pName || pName->type != json_string || !pMaps || pMaps->type != json_integer || !pScore)
				continue;
			const char *pModeName = json_string_get(pName);
			int64_t Maps = pMaps->u.integer;
			int64_t Score = 0;
			if(!pModeName || pModeName[0] == '\0' || static_cast<size_t>(str_length(pModeName)) > 128 || !str_utf8_check(pModeName) || Maps < 0 || Maps > std::numeric_limits<int>::max())
				continue;
			if(pScore->type == json_integer)
				Score = pScore->u.integer;
			else if(pScore->type == json_string)
			{
				const char *pScoreText = json_string_get(pScore);
				if(!ParseStrictInt64(pScoreText, Score))
					continue;
			}
			else
				continue;
			SQmClientLocalModeStats Stats;
			Stats.m_GameMode = pModeName;
			if(pCommunity != &json_value_none && pCommunity->type == json_string && json_string_get(pCommunity) && static_cast<size_t>(str_length(json_string_get(pCommunity))) <= 128 && str_utf8_check(json_string_get(pCommunity)))
				Stats.m_CommunityId = json_string_get(pCommunity);
			Stats.m_IsAxiom = pAxiom->type == json_boolean && json_boolean_get(pAxiom);
			Stats.m_Maps = (int)Maps;
			Stats.m_Score = Score;
			if(pPlaytime)
			{
				if(pPlaytime->type == json_integer && pPlaytime->u.integer >= 0)
					Stats.m_PlaytimeSeconds = pPlaytime->u.integer;
				else if(pPlaytime->type == json_string)
				{
					const char *pPlaytimeText = json_string_get(pPlaytime);
					int64_t ParsedPlaytime = 0;
					if(ParseStrictInt64(pPlaytimeText, ParsedPlaytime) && ParsedPlaytime >= 0)
						Stats.m_PlaytimeSeconds = ParsedPlaytime;
				}
			}
			m_vQmClientLocalModeStats.push_back(std::move(Stats));
		}
	}
	const json_value *pQmClient = JsonObjectField(pRemote, "qmclient");
	const json_value *pOpenSeconds = JsonObjectField(pQmClient, "open_seconds");
	if(pOpenSeconds->type == json_integer && pOpenSeconds->u.integer >= 0)
		m_QmClientServerPlaytimeSeconds = pOpenSeconds->u.integer;
	else if(pOpenSeconds->type == json_string)
	{
		int64_t OpenSeconds = 0;
		if(ParseStrictInt64(json_string_get(pOpenSeconds), OpenSeconds) && OpenSeconds >= 0)
			m_QmClientServerPlaytimeSeconds = OpenSeconds;
	}
	const json_value *pDdnet = JsonObjectField(pRemote, "ddnet");
	const json_value *pPlayerName = JsonObjectField(pDdnet, "player_name");
	if(pPlayerName->type == json_string && json_string_get(pPlayerName) && str_utf8_check(json_string_get(pPlayerName)))
		str_copy(m_aQmDdnetPlayerName, json_string_get(pPlayerName), sizeof(m_aQmDdnetPlayerName));
	const json_value *pFavoritePartner = JsonObjectField(pDdnet, "favorite_partner");
	if(pFavoritePartner->type == json_string && json_string_get(pFavoritePartner) && str_utf8_check(json_string_get(pFavoritePartner)))
		str_copy(m_aQmDdnetFavoritePartner, json_string_get(pFavoritePartner), sizeof(m_aQmDdnetFavoritePartner));
	const json_value *pPoints = JsonObjectField(pDdnet, "points");
	const json_value *pPointsTotal = JsonObjectField(pDdnet, "points_total");
	const json_value *pFinishes = JsonObjectField(pDdnet, "finishes");
	if(pPoints->type == json_integer)
		m_QmDdnetPoints = pPoints->u.integer;
	else if(pPoints->type == json_string)
	{
		int64_t Points = 0;
		if(ParseStrictInt64(json_string_get(pPoints), Points))
			m_QmDdnetPoints = Points;
	}
	if(pPointsTotal->type == json_integer)
		m_QmDdnetPointsTotal = pPointsTotal->u.integer;
	else if(pPointsTotal->type == json_string)
	{
		int64_t PointsTotal = 0;
		if(ParseStrictInt64(json_string_get(pPointsTotal), PointsTotal))
			m_QmDdnetPointsTotal = PointsTotal;
	}
	if(pFinishes->type == json_integer && pFinishes->u.integer >= 0 && pFinishes->u.integer <= std::numeric_limits<int>::max())
		m_QmDdnetTotalFinishes = (int)pFinishes->u.integer;
	else if(pFinishes->type == json_string)
	{
		int64_t Finishes = 0;
		if(ParseStrictInt64(json_string_get(pFinishes), Finishes) && Finishes >= 0 && Finishes <= std::numeric_limits<int>::max())
			m_QmDdnetTotalFinishes = (int)Finishes;
	}
	if(GameClient() != nullptr)
		GameClient()->m_QmAxiomScores.LoadPersistentCache(pRoot);
	json_value_free(pRoot);
}

void CQmClient::SaveQmClientStatistics() const
{
	Storage()->CreateFolder("qmclient", IStorage::TYPE_SAVE);
	char aTempFilename[IO_MAX_PATH_LENGTH];
	IStorage::FormatTmpPath(aTempFilename, sizeof(aTempFilename), QMCLIENT_LOCAL_MODE_STATS_FILE);
	IOHANDLE File = Storage()->OpenFile(aTempFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	{
		CJsonFileWriter Writer(File);
		Writer.BeginObject();
		Writer.WriteAttribute("local");
		Writer.BeginObject();
		Writer.WriteAttribute("modes");
		Writer.BeginArray();
		for(const SQmClientLocalModeStats &Stats : m_vQmClientLocalModeStats)
		{
			Writer.BeginObject();
			Writer.WriteAttribute("mode");
			Writer.WriteStrValue(Stats.m_GameMode.c_str());
			if(!Stats.m_CommunityId.empty())
			{
				Writer.WriteAttribute("community_id");
				Writer.WriteStrValue(Stats.m_CommunityId.c_str());
			}
			if(Stats.m_IsAxiom)
			{
				Writer.WriteAttribute("axiom");
				Writer.WriteBoolValue(true);
			}
			Writer.WriteAttribute("maps");
			Writer.WriteIntValue(Stats.m_Maps);
			Writer.WriteAttribute("score");
			char aScore[64];
			str_format(aScore, sizeof(aScore), "%" PRId64, Stats.m_Score);
			Writer.WriteStrValue(aScore);
			Writer.WriteAttribute("playtime_seconds");
			char aPlaytime[64];
			str_format(aPlaytime, sizeof(aPlaytime), "%" PRId64, std::max<int64_t>(0, Stats.m_PlaytimeSeconds));
			Writer.WriteStrValue(aPlaytime);
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();
		Writer.WriteAttribute("remote");
		Writer.BeginObject();
		Writer.WriteAttribute("qmclient");
		Writer.BeginObject();
		if(m_QmClientServerPlaytimeSeconds >= 0)
		{
			Writer.WriteAttribute("open_seconds");
			char aOpenSeconds[64];
			str_format(aOpenSeconds, sizeof(aOpenSeconds), "%" PRId64, m_QmClientServerPlaytimeSeconds);
			Writer.WriteStrValue(aOpenSeconds);
		}
		Writer.EndObject();
		Writer.WriteAttribute("ddnet");
		Writer.BeginObject();
		if(m_aQmDdnetPlayerName[0] != '\0')
		{
			Writer.WriteAttribute("player_name");
			Writer.WriteStrValue(m_aQmDdnetPlayerName);
		}
		if(m_aQmDdnetFavoritePartner[0] != '\0')
		{
			Writer.WriteAttribute("favorite_partner");
			Writer.WriteStrValue(m_aQmDdnetFavoritePartner);
		}
		if(m_QmDdnetPoints >= 0)
		{
			Writer.WriteAttribute("points");
			char aPoints[64];
			str_format(aPoints, sizeof(aPoints), "%" PRId64, m_QmDdnetPoints);
			Writer.WriteStrValue(aPoints);
		}
		if(m_QmDdnetPointsTotal >= 0)
		{
			Writer.WriteAttribute("points_total");
			char aPointsTotal[64];
			str_format(aPointsTotal, sizeof(aPointsTotal), "%" PRId64, m_QmDdnetPointsTotal);
			Writer.WriteStrValue(aPointsTotal);
		}
		if(m_QmDdnetTotalFinishes >= 0)
		{
			Writer.WriteAttribute("finishes");
			Writer.WriteIntValue(m_QmDdnetTotalFinishes);
		}
		Writer.EndObject();
		if(GameClient() != nullptr)
			GameClient()->m_QmAxiomScores.WritePersistentCache(Writer);
		else
		{
			Writer.WriteAttribute("axiom");
			Writer.BeginObject();
			Writer.WriteAttribute("players");
			Writer.BeginArray();
			Writer.EndArray();
			Writer.EndObject();
		}
		Writer.EndObject();
		Writer.EndObject();
	}
	char aBackupFilename[2 * IO_MAX_PATH_LENGTH];
	const bool Saved = IStorage::ReplaceFileSafely(Storage(), aTempFilename, QMCLIENT_LOCAL_MODE_STATS_FILE, aBackupFilename, sizeof(aBackupFilename));
	if(!Saved)
		Storage()->RemoveFile(aTempFilename, IStorage::TYPE_SAVE);
	else
		m_QmStatisticsFileExists = true;
}

void CQmClient::RecordQmClientLocalMapFinish(const char *pGameMode, int Score)
{
	if(!pGameMode || pGameMode[0] == '\0' || static_cast<size_t>(str_length(pGameMode)) > 128 || !str_utf8_check(pGameMode))
		return;
	const bool IsAxiom = Client()->State() == IClient::STATE_ONLINE && GameClient() != nullptr && GameClient()->m_QmAxiomAutoLogin.IsAxiomCommunity();
	const std::string CommunityId = Client()->State() == IClient::STATE_ONLINE ? Client()->ServerInfo().m_aCommunityId : "";
	if(CommunityId.size() > 128 || !str_utf8_check(CommunityId.c_str()))
		return;
	AccumulateQmClientLocalModePlaytime(time_timestamp());
	for(SQmClientLocalModeStats &Stats : m_vQmClientLocalModeStats)
	{
		if(Stats.m_GameMode == pGameMode && Stats.m_CommunityId == CommunityId && Stats.m_IsAxiom == IsAxiom)
		{
			++Stats.m_Maps;
			Stats.m_Score += Score;
			return;
		}
	}
	SQmClientLocalModeStats Stats;
	Stats.m_GameMode = pGameMode;
	Stats.m_CommunityId = CommunityId;
	Stats.m_IsAxiom = IsAxiom;
	Stats.m_Maps = 1;
	Stats.m_Score = Score;
	m_vQmClientLocalModeStats.push_back(std::move(Stats));
}

void CQmClient::AccumulateQmClientLocalModePlaytime(int64_t Now)
{
	if(m_QmClientActiveLocalMode.empty())
		return;
	if(m_QmClientLocalModeLastTimestamp <= 0)
	{
		m_QmClientLocalModeLastTimestamp = Now;
		return;
	}
	const int64_t Delta = Now - m_QmClientLocalModeLastTimestamp;
	m_QmClientLocalModeLastTimestamp = Now;
	if(Delta <= 0 || Delta > 24 * 60 * 60)
		return;
	for(SQmClientLocalModeStats &Stats : m_vQmClientLocalModeStats)
	{
		if(Stats.m_GameMode == m_QmClientActiveLocalMode && Stats.m_CommunityId == m_QmClientActiveLocalCommunityId && Stats.m_IsAxiom == m_QmClientActiveLocalIsAxiom)
		{
			Stats.m_PlaytimeSeconds += Delta;
			return;
		}
	}
	SQmClientLocalModeStats Stats;
	Stats.m_GameMode = m_QmClientActiveLocalMode;
	Stats.m_CommunityId = m_QmClientActiveLocalCommunityId;
	Stats.m_IsAxiom = m_QmClientActiveLocalIsAxiom;
	Stats.m_PlaytimeSeconds = Delta;
	m_vQmClientLocalModeStats.push_back(std::move(Stats));
}

void CQmClient::UpdateQmClientLocalModePlaytime()
{
	const int64_t Now = time_timestamp();
	const bool Online = Client()->State() == IClient::STATE_ONLINE;
	const char *pMode = Online ? Client()->ServerInfo().m_aGameType : "";
	const std::string CommunityId = Online ? Client()->ServerInfo().m_aCommunityId : "";
	const bool IsAxiom = Online && GameClient() != nullptr && GameClient()->m_QmAxiomAutoLogin.IsAxiomCommunity();
	if(m_QmClientActiveLocalMode != pMode || m_QmClientActiveLocalCommunityId != CommunityId || m_QmClientActiveLocalIsAxiom != IsAxiom)
	{
		AccumulateQmClientLocalModePlaytime(Now);
		m_QmClientActiveLocalMode = pMode;
		m_QmClientActiveLocalCommunityId = CommunityId;
		m_QmClientActiveLocalIsAxiom = IsAxiom;
		m_QmClientLocalModeLastTimestamp = pMode[0] != '\0' ? Now : 0;
		return;
	}
	AccumulateQmClientLocalModePlaytime(Now);
}

void CQmClient::EndQmClientLocalModePlaytime()
{
	if(!m_QmClientActiveLocalMode.empty())
	{
		AccumulateQmClientLocalModePlaytime(time_timestamp());
		m_QmClientActiveLocalMode.clear();
		m_QmClientActiveLocalCommunityId.clear();
		m_QmClientActiveLocalIsAxiom = false;
		m_QmClientLocalModeLastTimestamp = 0;
	}
}

void CQmClient::OnShutdown()
{
	EndQmClientLocalModePlaytime();
	SaveQmClientStatistics();
	if(!m_QmClientShutdownReported)
	{
		m_QmClientShutdownReported = true;
		TouchQmClientLifecycleMarker(true);
		SendQmClientLifecyclePing("shutdown", m_pQmClientLifecycleStopTask);
	}

	auto AbortTask = [](std::shared_ptr<IHttpRequest> &pTask) {
		if(pTask)
		{
			pTask->Abort();
			pTask = nullptr;
		}
	};

	AbortTask(m_pQmClientLifecycleStartTask);
	AbortTask(m_pQmClientLifecycleCrashTask);
	AbortTask(m_pQmClientServerTimeTask);
	AbortTask(m_pQmClientPlaytimeQueryTask);
	AbortTask(m_pQmDdnetPlayerTask);
	AbortTask(m_pQmClientAuthTokenTask);
	AbortTask(m_pQmClientUsersTask);
	AbortTask(m_pQmClientUsersSendTask);
	AbortTask(m_pQmDeveloperPresenceTask);
	AbortTask(m_pQmDeveloperPresencesTask);
	m_pQmClientUsersParseJob = nullptr;
	m_pQmDdnetPlayerParseJob = nullptr;
}

void CQmClient::OnUpdate()
{
	UpdateQmClientLocalModePlaytime();
	UpdateQmClientRecognition();
	UpdateQmDeveloperPresence();
	UpdateQmClientLifecycleAndServerTime();
	UpdateQmDdnetPlayerStats();

	// Axiom 分数在后台按缓存 TTL 查询，统计页面只负责展示，不应成为唯一触发点。
	bool HasAxiomGoresStats = false;
	for(const SQmClientLocalModeStats &Stats : m_vQmClientLocalModeStats)
	{
		if(Stats.m_IsAxiom && str_find_nocase(Stats.m_GameMode.c_str(), "gores") != nullptr)
		{
			HasAxiomGoresStats = true;
			break;
		}
	}
	if((HasAxiomGoresStats || (GameClient() != nullptr && GameClient()->m_QmAxiomAutoLogin.IsAxiomCommunity())) && m_aQmDdnetPlayerName[0] != '\0')
		GameClient()->m_QmAxiomScores.EnsureQueried(m_aQmDdnetPlayerName);
}

void CQmClient::OnStateChange(int NewState, int OldState)
{
	if(NewState != IClient::STATE_ONLINE && OldState == IClient::STATE_ONLINE)
		EndQmClientLocalModePlaytime();

	if((NewState == IClient::STATE_QUITTING || NewState == IClient::STATE_RESTARTING) && !m_QmClientShutdownReported)
	{
		m_QmClientShutdownReported = true;
		TouchQmClientLifecycleMarker(true);
		SendQmClientLifecyclePing(NewState == IClient::STATE_RESTARTING ? "restart" : "shutdown", m_pQmClientLifecycleStopTask);
	}

	if(NewState == IClient::STATE_ONLINE && OldState != IClient::STATE_ONLINE)
	{
		secure_random_password(m_aQmDeveloperSessionId, sizeof(m_aQmDeveloperSessionId), sizeof(m_aQmDeveloperSessionId) - 1);
		m_QmDeveloperLastSync = 0;
	}
	else if(NewState != IClient::STATE_ONLINE && OldState == IClient::STATE_ONLINE)
	{
		ResetQmDeveloperPresenceTasks();
		m_aQmDeveloperSessionId[0] = '\0';
	}
}

bool CQmClient::ReadQmClientLifecycleMarker(int64_t &OutStartedAt, int64_t &OutLastSeenAt)
{
	OutStartedAt = 0;
	OutLastSeenAt = 0;

	void *pFileData = nullptr;
	unsigned FileSize = 0;
	if(!Storage()->ReadFile(QMCLIENT_LIFECYCLE_MARKER_FILE, IStorage::TYPE_SAVE, &pFileData, &FileSize))
		return false;

	std::string Marker;
	if(pFileData && FileSize > 0)
		Marker.assign(static_cast<const char *>(pFileData), FileSize);
	free(pFileData);
	if(Marker.empty())
		return true;

	char aLine[256];
	const char *pStr = Marker.c_str();
	while((pStr = str_next_token(pStr, "\n", aLine, sizeof(aLine))))
	{
		if(const char *pValue = str_startswith(aLine, "started_at="))
			OutStartedAt = maximum<int64_t>(0, str_toint(pValue));
		else if(const char *pLastSeenValue = str_startswith(aLine, "last_seen_at="))
			OutLastSeenAt = maximum<int64_t>(0, str_toint(pLastSeenValue));
	}
	return true;
}

void CQmClient::WriteQmClientLifecycleMarker()
{
	std::lock_guard<std::mutex> Lock(*m_pQmClientLifecycleMarkerMutex);
	if(m_QmClientMarkerStartedAt <= 0)
		m_QmClientMarkerStartedAt = time_timestamp();
	if(m_QmClientMarkerLastSeenAt <= 0)
		m_QmClientMarkerLastSeenAt = m_QmClientMarkerStartedAt;

	Storage()->CreateFolder("qmclient", IStorage::TYPE_SAVE);

	IOHANDLE File = Storage()->OpenFile(QMCLIENT_LIFECYCLE_MARKER_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aLine[384];
	str_format(aLine, sizeof(aLine),
		"session=%s\nstarted_at=%d\nlast_seen_at=%d\nclient_id=%s\n",
		m_aQmClientLifecycleSessionId, (int)m_QmClientMarkerStartedAt, (int)m_QmClientMarkerLastSeenAt, m_aQmClientPlaytimeClientId);
	io_write(File, aLine, str_length(aLine));
	io_close(File);
}

void CQmClient::TouchQmClientLifecycleMarker(bool ForceWrite)
{
	const int64_t NowTick = time_get();
	const int64_t Interval = (int64_t)QMCLIENT_MARKER_FLUSH_INTERVAL_SECONDS * time_freq();
	if(!ForceWrite && m_QmClientMarkerLastFlushTick != 0 && NowTick - m_QmClientMarkerLastFlushTick < Interval)
		return;

	if(m_QmClientMarkerStartedAt <= 0)
		m_QmClientMarkerStartedAt = time_timestamp();
	m_QmClientMarkerLastSeenAt = time_timestamp();
	m_QmClientMarkerLastFlushTick = NowTick;

	if(ForceWrite)
	{
		if(m_pQmClientLifecycleMarkerWriteJob && !m_pQmClientLifecycleMarkerWriteJob->Done())
		{
			m_pQmClientLifecycleMarkerWriteJob->Abort();
			m_pQmClientLifecycleMarkerWriteJob = nullptr;
		}
		WriteQmClientLifecycleMarker();
		return;
	}

	if(m_pQmClientLifecycleMarkerWriteJob && !m_pQmClientLifecycleMarkerWriteJob->Done())
		return;

	char aLine[384];
	str_format(aLine, sizeof(aLine),
		"session=%s\nstarted_at=%d\nlast_seen_at=%d\nclient_id=%s\n",
		m_aQmClientLifecycleSessionId, (int)m_QmClientMarkerStartedAt, (int)m_QmClientMarkerLastSeenAt, m_aQmClientPlaytimeClientId);
	m_pQmClientLifecycleMarkerWriteJob = std::make_shared<CQmClientLifecycleMarkerWriteJob>(Storage(), aLine, m_pQmClientLifecycleMarkerMutex);
	Engine()->AddJob(m_pQmClientLifecycleMarkerWriteJob);
}

void CQmClient::ClearQmClientLifecycleMarker()
{
	Storage()->RemoveFile(QMCLIENT_LIFECYCLE_MARKER_FILE, IStorage::TYPE_SAVE);
}

void CQmClient::SendQmClientLifecyclePing(const char *pEvent, std::shared_ptr<IHttpRequest> &pTaskSlot)
{
	if(!pEvent || pEvent[0] == '\0')
		return;

	if(pTaskSlot)
	{
		if(!pTaskSlot->Done())
			return;
		pTaskSlot = nullptr;
	}

	if(str_comp(pEvent, "startup") == 0)
	{
		SendQmClientPlaytimeRequest(QMCLIENT_PLAYTIME_START_URL, pTaskSlot);
		return;
	}

	if(str_comp(pEvent, "recover_crash") == 0)
	{
		const int64_t StopAt = m_QmClientRecoveryStopAt > 0 ? m_QmClientRecoveryStopAt : time_timestamp();
		SendQmClientPlaytimeRequest(QMCLIENT_PLAYTIME_STOP_URL, pTaskSlot, StopAt);
		return;
	}

	if(str_comp(pEvent, "shutdown") == 0 || str_comp(pEvent, "restart") == 0)
	{
		SendQmClientPlaytimeRequest(QMCLIENT_PLAYTIME_STOP_URL, pTaskSlot, time_timestamp());
		return;
	}
}

void CQmClient::EnsureQmClientPlaytimeClientId()
{
	if(m_aQmClientPlaytimeClientId[0] != '\0')
		return;

	char aLoaded[128] = "";
	IOHANDLE File = Storage()->OpenFile(QMCLIENT_PLAYTIME_CLIENT_ID_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(File)
	{
		const int Read = io_read(File, aLoaded, sizeof(aLoaded) - 1);
		io_close(File);
		if(Read > 0)
		{
			aLoaded[Read] = '\0';
			char *pTrimmed = (char *)str_utf8_skip_whitespaces(aLoaded);
			str_utf8_trim_right(pTrimmed);
			if(IsValidQmClientPlaytimeId(pTrimmed))
				str_copy(m_aQmClientPlaytimeClientId, pTrimmed, sizeof(m_aQmClientPlaytimeClientId));
		}
	}

	if(m_aQmClientPlaytimeClientId[0] == '\0')
	{
		unsigned char aRandom[16];
		secure_random_fill(aRandom, sizeof(aRandom));

		static constexpr const char HEX[] = "0123456789abcdef";
		char aHex[sizeof(aRandom) * 2 + 1];
		for(size_t i = 0; i < sizeof(aRandom); ++i)
		{
			aHex[i * 2] = HEX[aRandom[i] >> 4];
			aHex[i * 2 + 1] = HEX[aRandom[i] & 0x0f];
		}
		aHex[sizeof(aHex) - 1] = '\0';
		str_format(m_aQmClientPlaytimeClientId, sizeof(m_aQmClientPlaytimeClientId), "qm%s", aHex);
	}

	if(!IsValidQmClientPlaytimeId(m_aQmClientPlaytimeClientId))
	{
		m_aQmClientPlaytimeClientId[0] = '\0';
		return;
	}

	Storage()->CreateFolder("qmclient", IStorage::TYPE_SAVE);
	IOHANDLE OutFile = Storage()->OpenFile(QMCLIENT_PLAYTIME_CLIENT_ID_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(OutFile)
	{
		io_write(OutFile, m_aQmClientPlaytimeClientId, str_length(m_aQmClientPlaytimeClientId));
		io_write(OutFile, "\n", 1);
		io_close(OutFile);
	}
}

void CQmClient::SendQmClientPlaytimeRequest(const char *pUrl, std::shared_ptr<IHttpRequest> &pTaskSlot, int64_t StopAt)
{
	if(!pUrl || pUrl[0] == '\0')
		return;

	EnsureQmClientPlaytimeClientId();
	if(m_aQmClientPlaytimeClientId[0] == '\0')
		return;

	CJsonStringWriter JsonWriter;
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("client_id");
	JsonWriter.WriteStrValue(m_aQmClientPlaytimeClientId);
	JsonWriter.WriteAttribute("player_name");
	JsonWriter.WriteStrValue(g_Config.m_PlayerName);
	if(StopAt > 0)
	{
		JsonWriter.WriteAttribute("stop_at");
		const int StopAtInt = StopAt > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : (int)StopAt;
		JsonWriter.WriteIntValue(StopAtInt);
	}
	JsonWriter.EndObject();

	std::string Body = JsonWriter.GetOutputString();
	pTaskSlot = HttpPostJson(pUrl, Body.c_str());
	pTaskSlot->AllowInsecureProtocol();
	pTaskSlot->Timeout(CTimeout{3000, 0, 250, 6});
	pTaskSlot->IpResolve(IPRESOLVE::V4);
	pTaskSlot->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(pTaskSlot);
}

bool CQmClient::FinishQmClientPlaytimeTask(std::shared_ptr<IHttpRequest> &pTaskSlot, bool UpdateSessionStart)
{
	if(!pTaskSlot)
		return false;

	const bool Ok = pTaskSlot->State() == EHttpState::DONE && pTaskSlot->StatusCode() == 200;

	if(Ok)
	{
		json_value *pRoot = pTaskSlot->ResultJson();
		if(pRoot && pRoot->type == json_object)
		{
			int64_t ServerNow = 0;
			if(JsonReadNonNegativeInt64(JsonObjectField(pRoot, "ts"), ServerNow))
				m_QmClientServerNow = ServerNow;

			int64_t TotalSeconds = 0;
			if(JsonReadNonNegativeInt64(JsonObjectField(pRoot, "total_seconds"), TotalSeconds))
				m_QmClientServerPlaytimeSeconds = TotalSeconds;

			const json_value *pRunning = JsonObjectField(pRoot, "running");
			const bool Running = pRunning->type == json_boolean && json_boolean_get(pRunning);
			int64_t LastStartAt = 0;
			if(UpdateSessionStart || Running)
			{
				if(JsonReadNonNegativeInt64(JsonObjectField(pRoot, "last_start_at"), LastStartAt) && LastStartAt > 0)
					m_QmClientServerSessionStart = LastStartAt;
			}
		}
		if(pRoot)
			json_value_free(pRoot);
	}

	if(Ok && (&pTaskSlot == &m_pQmClientLifecycleStopTask || &pTaskSlot == &m_pQmClientLifecycleCrashTask))
		ClearQmClientLifecycleMarker();

	pTaskSlot = nullptr;
	return Ok;
}

void CQmClient::FinishQmClientServerTimeTask()
{
	if(!m_pQmClientServerTimeTask)
		return;

	m_QmClientServerTimeLastSync = time_get();

	if(m_pQmClientServerTimeTask->State() == EHttpState::DONE)
	{
		json_value *pRoot = m_pQmClientServerTimeTask->ResultJson();
		if(pRoot)
		{
			const json_value *pTs = JsonObjectField(pRoot, "ts");
			if(pTs != &json_value_none && pTs->type == json_integer && pTs->u.integer > 0)
			{
				m_QmClientServerNow = pTs->u.integer;
				if(m_QmClientServerSessionStart == 0)
					m_QmClientServerSessionStart = m_QmClientServerNow;
			}
			json_value_free(pRoot);
		}
	}

	m_pQmClientServerTimeTask = nullptr;
}

void CQmClient::UpdateQmClientLifecycleAndServerTime()
{
	if(m_pQmClientLifecycleStartTask && m_pQmClientLifecycleStartTask->Done())
	{
		const bool Ok = FinishQmClientPlaytimeTask(m_pQmClientLifecycleStartTask, true);
		if(!Ok)
		{
			m_QmClientStartupSent = false;
			m_QmClientStartupNextRetry = time_get() + (int64_t)QMCLIENT_RECOVERY_RETRY_SECONDS * time_freq();
		}
		else
		{
			m_QmClientStartupNextRetry = 0;
		}
	}
	if(m_pQmClientLifecycleCrashTask && m_pQmClientLifecycleCrashTask->Done())
	{
		const bool Ok = FinishQmClientPlaytimeTask(m_pQmClientLifecycleCrashTask, false);
		if(Ok)
		{
			m_QmClientAwaitingRecoveryStop = false;
			m_QmClientRecoveryNextRetry = 0;
		}
		else
		{
			m_QmClientRecoveryNextRetry = time_get() + (int64_t)QMCLIENT_RECOVERY_RETRY_SECONDS * time_freq();
		}
	}
	if(m_pQmClientLifecycleStopTask && m_pQmClientLifecycleStopTask->Done())
		FinishQmClientPlaytimeTask(m_pQmClientLifecycleStopTask, false);
	if(m_pQmClientPlaytimeQueryTask && m_pQmClientPlaytimeQueryTask->Done())
	{
		FinishQmClientPlaytimeTask(m_pQmClientPlaytimeQueryTask, false);
		m_QmClientPlaytimeLastSync = time_get();
	}

	if(m_pQmClientServerTimeTask && m_pQmClientServerTimeTask->Done())
		FinishQmClientServerTimeTask();

	if(Client()->State() == IClient::STATE_QUITTING || Client()->State() == IClient::STATE_RESTARTING)
		return;

	const int64_t Now = time_get();

	if(m_QmClientAwaitingRecoveryStop && !m_pQmClientLifecycleCrashTask &&
		(m_QmClientRecoveryNextRetry == 0 || Now >= m_QmClientRecoveryNextRetry))
	{
		SendQmClientLifecyclePing("recover_crash", m_pQmClientLifecycleCrashTask);
		if(m_pQmClientLifecycleCrashTask)
			m_QmClientRecoveryNextRetry = Now + (int64_t)QMCLIENT_RECOVERY_RETRY_SECONDS * time_freq();
	}

	if(!m_QmClientAwaitingRecoveryStop && !m_QmClientStartupSent && !m_pQmClientLifecycleStartTask &&
		(m_QmClientStartupNextRetry == 0 || Now >= m_QmClientStartupNextRetry))
	{
		m_QmClientMarkerStartedAt = time_timestamp();
		m_QmClientMarkerLastSeenAt = m_QmClientMarkerStartedAt;
		m_QmClientMarkerLastFlushTick = Now;
		WriteQmClientLifecycleMarker();

		SendQmClientLifecyclePing("startup", m_pQmClientLifecycleStartTask);
		m_QmClientStartupSent = m_pQmClientLifecycleStartTask != nullptr;
		if(m_QmClientStartupSent)
			m_QmClientStartupNextRetry = 0;
	}

	if(!m_QmClientAwaitingRecoveryStop && m_QmClientStartupSent)
		TouchQmClientLifecycleMarker(false);

	const int64_t PlaytimeIntervalTicks = (int64_t)QMCLIENT_PLAYTIME_QUERY_INTERVAL_SECONDS * time_freq();
	if(!m_pQmClientPlaytimeQueryTask && (m_QmClientPlaytimeLastSync == 0 || Now - m_QmClientPlaytimeLastSync >= PlaytimeIntervalTicks))
		SendQmClientPlaytimeRequest(QMCLIENT_PLAYTIME_QUERY_URL, m_pQmClientPlaytimeQueryTask);

	const int64_t IntervalTicks = (int64_t)QMCLIENT_SERVER_TIME_SYNC_INTERVAL_SECONDS * time_freq();
	if(m_pQmClientServerTimeTask || (m_QmClientServerTimeLastSync != 0 && Now - m_QmClientServerTimeLastSync < IntervalTicks))
		return;

	char aUrl[512];
	const int Nonce = secure_rand_below(1000000);
	str_format(aUrl, sizeof(aUrl), "%s?event=time_sync&session=%s&nonce=%d", QMCLIENT_HEALTH_URL, m_aQmClientLifecycleSessionId, Nonce);

	m_pQmClientServerTimeTask = HttpGet(aUrl);
	m_pQmClientServerTimeTask->AllowInsecureProtocol();
	m_pQmClientServerTimeTask->Timeout(CTimeout{3000, 0, 250, 5});
	m_pQmClientServerTimeTask->IpResolve(IPRESOLVE::V4);
	m_pQmClientServerTimeTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmClientServerTimeTask);
}

void CQmClient::UpdateQmDdnetPlayerStats()
{
	if(m_pQmDdnetPlayerParseJob && m_pQmDdnetPlayerParseJob->Done())
		FinishQmDdnetPlayerStats();
	if(m_pQmDdnetPlayerTask && m_pQmDdnetPlayerTask->Done())
		FinishQmDdnetPlayerStats();

	const char *pConfiguredName = g_Config.m_PlayerName;
	if(!pConfiguredName || pConfiguredName[0] == '\0')
	{
		if(m_pQmDdnetPlayerTask)
		{
			m_pQmDdnetPlayerTask->Abort();
			m_pQmDdnetPlayerTask = nullptr;
		}
		m_pQmDdnetPlayerParseJob = nullptr;
		if(m_aQmDdnetPlayerName[0] != '\0')
		{
			m_aQmDdnetPlayerName[0] = '\0';
			m_aQmDdnetFavoritePartner[0] = '\0';
			m_QmDdnetTotalFinishes = -1;
			m_QmDdnetPlayerLastSync = 0;
			m_QmDdnetPlayerNextRetry = 0;
			m_QmDdnetPoints = -1;
			m_QmDdnetPointsTotal = -1;
		}
		return;
	}

	if(str_comp(m_aQmDdnetPlayerName, pConfiguredName) != 0)
	{
		if(m_pQmDdnetPlayerTask)
		{
			m_pQmDdnetPlayerTask->Abort();
			m_pQmDdnetPlayerTask = nullptr;
		}
		m_pQmDdnetPlayerParseJob = nullptr;

		str_copy(m_aQmDdnetPlayerName, pConfiguredName, sizeof(m_aQmDdnetPlayerName));
		m_aQmDdnetFavoritePartner[0] = '\0';
		m_QmDdnetTotalFinishes = -1;
		m_QmDdnetPoints = -1;
		m_QmDdnetPointsTotal = -1;
		m_QmDdnetPlayerLastSync = 0;
		m_QmDdnetPlayerNextRetry = 0;
	}

	if(m_pQmDdnetPlayerParseJob)
		return;
	if(m_pQmDdnetPlayerTask)
		return;

	const int64_t Now = time_get();
	if(m_QmDdnetPlayerNextRetry != 0 && Now < m_QmDdnetPlayerNextRetry)
		return;

	if(m_QmDdnetPlayerNextRetry == 0)
	{
		const int64_t SyncIntervalTicks = (int64_t)QMCLIENT_DDNET_PLAYER_SYNC_INTERVAL_SECONDS * time_freq();
		if(m_QmDdnetPlayerLastSync != 0 && Now - m_QmDdnetPlayerLastSync < SyncIntervalTicks)
			return;
	}

	FetchQmDdnetPlayerStats(m_aQmDdnetPlayerName);
}

void CQmClient::FetchQmDdnetPlayerStats(const char *pPlayerName)
{
	if(!pPlayerName || pPlayerName[0] == '\0')
		return;
	if(m_pQmDdnetPlayerParseJob && !m_pQmDdnetPlayerParseJob->Done())
		return;
	if(m_pQmDdnetPlayerTask && !m_pQmDdnetPlayerTask->Done())
		return;

	char aEncodedName[256];
	EscapeUrl(aEncodedName, sizeof(aEncodedName), pPlayerName);

	char aUrl[512];
	str_format(aUrl, sizeof(aUrl), "%s%s", DDNET_PLAYER_STATS_URL, aEncodedName);

	m_pQmDdnetPlayerTask = HttpGet(aUrl);
	m_pQmDdnetPlayerTask->Timeout(CTimeout{10000, 30000, 100, 10});
	m_pQmDdnetPlayerTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmDdnetPlayerTask);
}

void CQmClient::FinishQmDdnetPlayerStats()
{
	if(m_pQmDdnetPlayerParseJob)
	{
		if(!m_pQmDdnetPlayerParseJob->Done())
			return;

		auto pParseJob = std::static_pointer_cast<CQmDdnetPlayerStatsParseJob>(m_pQmDdnetPlayerParseJob);
		CQmDdnetPlayerStatsParseJob::SResult Result = pParseJob->TakeResult();
		m_pQmDdnetPlayerParseJob = nullptr;

		const int64_t Now = time_get();
		if(Result.m_Parsed)
		{
			m_QmDdnetPlayerLastSync = Now;
			m_QmDdnetPlayerNextRetry = 0;
			str_copy(m_aQmDdnetFavoritePartner, Result.m_FavoritePartner.c_str(), sizeof(m_aQmDdnetFavoritePartner));
			m_QmDdnetTotalFinishes = Result.m_TotalFinishes;
			m_QmDdnetPoints = Result.m_Points;
			m_QmDdnetPointsTotal = Result.m_PointsTotal;
		}
		else
		{
			m_QmDdnetPlayerLastSync = 0;
			m_QmDdnetPlayerNextRetry = Now + (int64_t)QMCLIENT_DDNET_PLAYER_RETRY_DELAY_SECONDS * time_freq();
		}
		return;
	}

	if(!m_pQmDdnetPlayerTask)
		return;

	if(m_pQmDdnetPlayerTask->State() != EHttpState::DONE || m_pQmDdnetPlayerTask->StatusCode() != 200)
	{
		m_QmDdnetPlayerLastSync = 0;
		m_QmDdnetPlayerNextRetry = time_get() + (int64_t)QMCLIENT_DDNET_PLAYER_RETRY_DELAY_SECONDS * time_freq();
		m_pQmDdnetPlayerTask = nullptr;
		return;
	}

	m_pQmDdnetPlayerParseJob = std::make_shared<CQmDdnetPlayerStatsParseJob>(m_pQmDdnetPlayerTask);
	Engine()->AddJob(m_pQmDdnetPlayerParseJob);
	m_pQmDdnetPlayerTask = nullptr;
}

void CQmClient::RefreshQmDdnetPlayerStats()
{
	if(m_pQmDdnetPlayerTask)
	{
		m_pQmDdnetPlayerTask->Abort();
		m_pQmDdnetPlayerTask = nullptr;
	}
	m_pQmDdnetPlayerParseJob = nullptr;
	m_QmDdnetPlayerLastSync = 0;
	m_QmDdnetPlayerNextRetry = 0;
	if(m_aQmDdnetPlayerName[0] != '\0')
		FetchQmDdnetPlayerStats(m_aQmDdnetPlayerName);
}

void CQmClient::RefreshQmClientStatistics()
{
	RefreshQmDdnetPlayerStats();
	if(GameClient() != nullptr && m_aQmDdnetPlayerName[0] != '\0')
		GameClient()->m_QmAxiomScores.Refresh(m_aQmDdnetPlayerName);
	SaveQmClientStatistics();
}

void CQmClient::InitQmClientLifecycle()
{
	unsigned SessionRandom = 0;
	secure_random_fill(&SessionRandom, sizeof(SessionRandom));
	str_format(m_aQmClientLifecycleSessionId, sizeof(m_aQmClientLifecycleSessionId), "%08x%08x", (unsigned)time_timestamp(), SessionRandom);

	EnsureQmClientPlaytimeClientId();
	int64_t PreviousStartedAt = 0;
	int64_t PreviousLastSeenAt = 0;
	const bool HadPendingMarker = ReadQmClientLifecycleMarker(PreviousStartedAt, PreviousLastSeenAt);
	m_QmClientRecoveryStopAt = PreviousLastSeenAt > 0 ? PreviousLastSeenAt : PreviousStartedAt;
	if(m_QmClientRecoveryStopAt <= 0)
		m_QmClientRecoveryStopAt = time_timestamp();
	m_QmClientMarkerStartedAt = PreviousStartedAt;
	m_QmClientMarkerLastSeenAt = PreviousLastSeenAt;
	m_QmClientMarkerLastFlushTick = 0;

	m_QmClientShutdownReported = false;
	m_QmClientAwaitingRecoveryStop = HadPendingMarker;
	m_QmClientStartupSent = false;
	m_QmClientRecoveryNextRetry = 0;
	m_QmClientStartupNextRetry = 0;
	m_QmClientServerNow = 0;
	m_QmClientServerSessionStart = 0;
	m_QmClientServerTimeLastSync = 0;
	m_QmClientServerPlaytimeSeconds = -1;
	m_QmClientPlaytimeLastSync = 0;

	if(HadPendingMarker)
	{
		SendQmClientLifecyclePing("recover_crash", m_pQmClientLifecycleCrashTask);
		if(!m_pQmClientLifecycleCrashTask)
			m_QmClientRecoveryNextRetry = time_get() + (int64_t)QMCLIENT_RECOVERY_RETRY_SECONDS * time_freq();
	}
}

void CQmClient::InitQmDeveloperAuthentication()
{
	m_aQmDeveloperToken[0] = '\0';
	char *pTokenText = Storage()->ReadFileStr(QMCLIENT_DEVELOPER_TOKEN_FILE, IStorage::TYPE_SAVE);
	if(!pTokenText)
		return;

	char *pToken = str_skip_whitespaces(pTokenText);
	char *pEnd = pToken + str_length(pToken);
	while(pEnd > pToken && std::isspace((unsigned char)pEnd[-1]))
		--pEnd;
	*pEnd = '\0';

	const int TokenLength = str_length(pToken);
	bool Valid = TokenLength == 64;
	for(int i = 0; Valid && i < TokenLength; ++i)
		Valid = std::isxdigit((unsigned char)pToken[i]) != 0;
	if(Valid)
		str_copy(m_aQmDeveloperToken, pToken, sizeof(m_aQmDeveloperToken));
	else
		log_warn("qmclient", "ignored invalid developer credential file");
	free(pTokenText);
}

void CQmClient::ResetQmDeveloperPresenceTasks()
{
	if(m_pQmDeveloperPresenceTask)
	{
		m_pQmDeveloperPresenceTask->Abort();
		m_pQmDeveloperPresenceTask = nullptr;
	}
	if(m_pQmDeveloperPresencesTask)
	{
		m_pQmDeveloperPresencesTask->Abort();
		m_pQmDeveloperPresencesTask = nullptr;
	}
	m_aQmDeveloperPendingServerAddress[0] = '\0';
	m_QmDeveloperLastSync = 0;
	GameClient()->ClearQmDeveloperMarks();
}

void CQmClient::SendQmDeveloperPresence(const char *pServerAddress)
{
	if(m_aQmDeveloperToken[0] == '\0' || m_aQmDeveloperSessionId[0] == '\0' || !pServerAddress || pServerAddress[0] == '\0')
		return;
	if(m_pQmDeveloperPresenceTask && !m_pQmDeveloperPresenceTask->Done())
		return;

	CJsonStringWriter JsonWriter;
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("server_address");
	JsonWriter.WriteStrValue(pServerAddress);
	JsonWriter.WriteAttribute("session_id");
	JsonWriter.WriteStrValue(m_aQmDeveloperSessionId);
	JsonWriter.WriteAttribute("players");
	JsonWriter.BeginArray();
	int PlayerCount = 0;
	for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)
	{
		if(Dummy == 1 && !Client()->DummyConnected())
			continue;
		const int ClientId = GameClient()->m_aLocalIds[Dummy];
		if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_aClients[ClientId].m_Active)
			continue;

		JsonWriter.BeginObject();
		JsonWriter.WriteAttribute("player_id");
		JsonWriter.WriteIntValue(ClientId);
		JsonWriter.WriteAttribute("player_name");
		JsonWriter.WriteStrValue(GameClient()->m_aClients[ClientId].m_aName);
		JsonWriter.WriteAttribute("dummy");
		JsonWriter.WriteBoolValue(Dummy == 1);
		JsonWriter.EndObject();
		++PlayerCount;
	}
	JsonWriter.EndArray();
	JsonWriter.EndObject();
	if(PlayerCount == 0)
		return;

	const std::string JsonBody = JsonWriter.GetOutputString();
	m_pQmDeveloperPresenceTask = HttpPostJson(QMCLIENT_DEVELOPER_PRESENCE_URL, JsonBody.c_str());
	m_pQmDeveloperPresenceTask->MaxResponseSize(16 * 1024);
	char aAuthorization[80];
	str_format(aAuthorization, sizeof(aAuthorization), "Bearer %s", m_aQmDeveloperToken);
	m_pQmDeveloperPresenceTask->HeaderString("Authorization", aAuthorization);
	m_pQmDeveloperPresenceTask->Timeout(CTimeout{3000, 5000, 500, 5});
	m_pQmDeveloperPresenceTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmDeveloperPresenceTask);
}

void CQmClient::FetchQmDeveloperPresences(const char *pServerAddress)
{
	if(!pServerAddress || pServerAddress[0] == '\0')
		return;
	if(m_pQmDeveloperPresencesTask && !m_pQmDeveloperPresencesTask->Done())
		return;

	char aEscapedServerAddress[NETADDR_MAXSTRSIZE * 3];
	EscapeUrl(aEscapedServerAddress, sizeof(aEscapedServerAddress), pServerAddress);
	char aUrl[512];
	str_format(aUrl, sizeof(aUrl), "%s?server_address=%s", QMCLIENT_DEVELOPER_PRESENCES_URL, aEscapedServerAddress);
	m_pQmDeveloperPresencesTask = HttpGet(aUrl);
	m_pQmDeveloperPresencesTask->MaxResponseSize(128 * 1024);
	m_pQmDeveloperPresencesTask->Timeout(CTimeout{3000, 5000, 500, 5});
	m_pQmDeveloperPresencesTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmDeveloperPresencesTask);
	str_copy(m_aQmDeveloperPendingServerAddress, pServerAddress, sizeof(m_aQmDeveloperPendingServerAddress));
}

void CQmClient::FinishQmDeveloperPresences(const char *pServerAddress)
{
	GameClient()->ClearQmDeveloperMarks();
	if(!m_pQmDeveloperPresencesTask || m_pQmDeveloperPresencesTask->State() != EHttpState::DONE || m_pQmDeveloperPresencesTask->StatusCode() != 200)
		return;

	char aCurrentServerAddress[NETADDR_MAXSTRSIZE] = "";
	if(Client()->State() == IClient::STATE_ONLINE)
	{
		const NETADDR *pServerAddr = Client()->ServerAddress();
		if(pServerAddr)
			net_addr_str(pServerAddr, aCurrentServerAddress, sizeof(aCurrentServerAddress), true);
	}
	if(!pServerAddress || str_comp(aCurrentServerAddress, pServerAddress) != 0)
		return;

	json_value *pRoot = m_pQmDeveloperPresencesTask->ResultJson();
	if(!pRoot)
		return;
	SQmDeveloperPresenceParseResult Result;
	const bool Parsed = ParseQmDeveloperPresencesJson(pRoot, pServerAddress, Result);
	json_value_free(pRoot);
	if(!Parsed)
		return;

	const int64_t NowTick = time_get();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!GameClient()->m_aClients[ClientId].m_Active)
			continue;
		const SQmDeveloperPresence *pPresence = FindQmDeveloperPresence(
			Result.m_vPresences,
			pServerAddress,
			ClientId,
			GameClient()->m_aClients[ClientId].m_aName,
			Result.m_ServerTime);
		if(!pPresence)
			continue;
		const int64_t RemainingSeconds = std::min<int64_t>(pPresence->m_ExpiresAt - Result.m_ServerTime, QMCLIENT_DEVELOPER_SYNC_INTERVAL_SECONDS * 2);
		if(RemainingSeconds <= 0)
			continue;
		GameClient()->MarkQmDeveloperClient(
			ClientId,
			pPresence->m_PlayerName.c_str(),
			NowTick + RemainingSeconds * time_freq(),
			QmDeveloperBadgeStyleFromBucket(pPresence->m_StyleBucket) == EQmDeveloperBadgeStyle::RAINBOW);
	}
}

void CQmClient::UpdateQmDeveloperPresence()
{
	if(m_pQmDeveloperPresenceTask && m_pQmDeveloperPresenceTask->Done())
		m_pQmDeveloperPresenceTask = nullptr;
	if(m_pQmDeveloperPresencesTask && m_pQmDeveloperPresencesTask->Done())
	{
		FinishQmDeveloperPresences(m_aQmDeveloperPendingServerAddress);
		m_pQmDeveloperPresencesTask = nullptr;
		m_aQmDeveloperPendingServerAddress[0] = '\0';
	}

	if(Client()->State() != IClient::STATE_ONLINE)
	{
		GameClient()->ClearQmDeveloperMarks();
		return;
	}
	if(m_aQmDeveloperSessionId[0] == '\0')
		secure_random_password(m_aQmDeveloperSessionId, sizeof(m_aQmDeveloperSessionId), sizeof(m_aQmDeveloperSessionId) - 1);

	const int64_t Now = time_get();
	if(m_QmDeveloperLastSync != 0 && Now - m_QmDeveloperLastSync < (int64_t)QMCLIENT_DEVELOPER_SYNC_INTERVAL_SECONDS * time_freq())
		return;

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	const NETADDR *pServerAddr = Client()->ServerAddress();
	if(pServerAddr)
		net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	if(aServerAddress[0] == '\0')
		return;

	SendQmDeveloperPresence(aServerAddress);
	FetchQmDeveloperPresences(aServerAddress);
	m_QmDeveloperLastSync = Now;
}

void CQmClient::ResetQmClientRecognitionTasks()
{
	auto AbortTask = [](std::shared_ptr<IHttpRequest> &pTask) {
		if(pTask)
		{
			pTask->Abort();
			pTask = nullptr;
		}
	};
	AbortTask(m_pQmClientAuthTokenTask);
	AbortTask(m_pQmClientUsersTask);
	AbortTask(m_pQmClientUsersSendTask);
	m_pQmClientUsersParseJob = nullptr;
	m_QmClientDistributionSuccessLatched = false;
	m_aQmClientAuthToken[0] = '\0';
	m_aQmClientPendingVoicePresenceServerAddress[0] = '\0';
	m_QmClientPendingVoicePresencePlayers = 0;
	m_QmClientLastSync = 0;
	ClearQmClientServerDistribution();
}

bool CQmClient::NeedsQmClientRecognition() const
{
	return HasQmClientRecognitionService();
}

bool CQmClient::HasQmClientRecognitionService() const
{
	return GetEffectiveQmVoiceServer()[0] != '\0';
}

bool CQmClient::NeedsFastQmClientSync() const
{
	return g_Config.m_QmVoiceEnable != 0 || g_Config.m_QmClientShowBadge != 0 || g_Config.m_QmClientMarkTrail != 0;
}

bool CQmClient::BuildQmClientRecognitionUrl(const char *pPath, char *pBuf, size_t BufSize, const char *pQuery) const
{
	if(!pPath || pPath[0] == '\0' || !pBuf || BufSize == 0)
		return false;

	const char *pVoiceServer = GetEffectiveQmVoiceServer();
	char aHost[128];
	int Port = 0;
	if(!ParseQmClientServiceHostPort(pVoiceServer, aHost, sizeof(aHost), Port))
		return false;

	const bool NeedsIpv6Brackets = str_find(aHost, ":") != nullptr;
	if(pQuery && pQuery[0] != '\0')
	{
		if(NeedsIpv6Brackets)
			str_format(pBuf, BufSize, "http://[%s]:%d%s?%s", aHost, Port, pPath, pQuery);
		else
			str_format(pBuf, BufSize, "http://%s:%d%s?%s", aHost, Port, pPath, pQuery);
	}
	else
	{
		if(NeedsIpv6Brackets)
			str_format(pBuf, BufSize, "http://[%s]:%d%s", aHost, Port, pPath);
		else
			str_format(pBuf, BufSize, "http://%s:%d%s", aHost, Port, pPath);
	}

	return pBuf[0] != '\0';
}

void CQmClient::ClearQmClientServerDistribution()
{
	m_vQmClientServerDistribution.clear();
	m_QmClientOnlineUserCount = 0;
	m_QmClientOnlineDummyCount = 0;
}

bool CQmClient::EnsureQmClientMachineHash()
{
	if(IsValidQmClientMachineHash(m_aQmClientMachineHash))
		return true;

	std::string Identity;
	if(!ReadPlatformMachineIdentity(Identity))
	{
		char aLoaded[128] = "";
		IOHANDLE File = Storage()->OpenFile(QMCLIENT_MACHINE_ID_FALLBACK_FILE, IOFLAG_READ, IStorage::TYPE_SAVE);
		if(File)
		{
			const int Read = io_read(File, aLoaded, sizeof(aLoaded) - 1);
			io_close(File);
			if(Read > 0)
			{
				aLoaded[Read] = '\0';
				TrimQmClientTextInPlace(aLoaded);
				if(aLoaded[0] != '\0')
					Identity = aLoaded;
			}
		}

		if(Identity.empty())
		{
			unsigned char aRandom[32];
			secure_random_fill(aRandom, sizeof(aRandom));

			static constexpr const char HEX[] = "0123456789abcdef";
			char aHex[sizeof(aRandom) * 2 + 1];
			for(size_t i = 0; i < sizeof(aRandom); ++i)
			{
				aHex[i * 2] = HEX[aRandom[i] >> 4];
				aHex[i * 2 + 1] = HEX[aRandom[i] & 0x0f];
			}
			aHex[sizeof(aHex) - 1] = '\0';
			Identity = aHex;

			Storage()->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			IOHANDLE OutFile = Storage()->OpenFile(QMCLIENT_MACHINE_ID_FALLBACK_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(OutFile)
			{
				io_write(OutFile, Identity.c_str(), Identity.size());
				io_write(OutFile, "\n", 1);
				io_close(OutFile);
			}
		}
	}

	if(Identity.empty())
		return false;

	const SHA256_DIGEST Digest = sha256(Identity.data(), Identity.size());
	sha256_str(Digest, m_aQmClientMachineHash, sizeof(m_aQmClientMachineHash));
	return IsValidQmClientMachineHash(m_aQmClientMachineHash);
}

void CQmClient::FetchQmClientAuthToken()
{
	if(m_pQmClientAuthTokenTask && !m_pQmClientAuthTokenTask->Done())
		return;

	if(!EnsureQmClientMachineHash())
		return;

	char aQuery[128];
	str_format(aQuery, sizeof(aQuery), "machine_hash=%s", m_aQmClientMachineHash);

	char aUrl[256];
	if(!BuildQmClientRecognitionUrl(QMCLIENT_TOKEN_PATH, aUrl, sizeof(aUrl), aQuery))
		return;

	m_pQmClientAuthTokenTask = HttpGet(aUrl);
	m_pQmClientAuthTokenTask->AllowInsecureProtocol();
	m_pQmClientAuthTokenTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pQmClientAuthTokenTask->IpResolve(IPRESOLVE::V4);
	m_pQmClientAuthTokenTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmClientAuthTokenTask);
	LogQmClientRecognitionEvent("token_request", aUrl);
}

void CQmClient::SendQmClientPlayerData()
{
	if(m_aQmClientAuthToken[0] == '\0')
		return;
	if(m_pQmClientUsersSendTask && !m_pQmClientUsersSendTask->Done())
		return;
	if(!EnsureQmClientMachineHash())
		return;

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	if(Client()->State() == IClient::STATE_ONLINE)
	{
		const NETADDR *pServerAddr = Client()->ServerAddress();
		if(pServerAddr)
			net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	}
	if(aServerAddress[0] == '\0')
		return;

	CJsonStringWriter JsonWriter;
	JsonWriter.BeginObject();
	JsonWriter.WriteAttribute("server_address");
	JsonWriter.WriteStrValue(aServerAddress);
	JsonWriter.WriteAttribute("auth_token");
	JsonWriter.WriteStrValue(m_aQmClientAuthToken);
	JsonWriter.WriteAttribute("client_type");
	JsonWriter.WriteStrValue("qm");
	JsonWriter.WriteAttribute("machine_hash");
	JsonWriter.WriteStrValue(m_aQmClientMachineHash);
	JsonWriter.WriteAttribute("timestamp");
	JsonWriter.WriteIntValue((int)time_timestamp());
	JsonWriter.WriteAttribute("players");
	JsonWriter.BeginArray();
	int PlayerCount = 0;

	for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)
	{
		if(Dummy == 1 && !Client()->DummyConnected())
			continue;

		const int ClientId = GameClient()->m_aLocalIds[Dummy];
		if(ClientId < 0 || ClientId >= MAX_CLIENTS || !GameClient()->m_aClients[ClientId].m_Active)
			continue;

		JsonWriter.BeginObject();
		JsonWriter.WriteAttribute("player_name");
		JsonWriter.WriteStrValue(GameClient()->m_aClients[ClientId].m_aName);
		JsonWriter.WriteAttribute("dummy");
		JsonWriter.WriteBoolValue(Dummy == 1);
		JsonWriter.WriteAttribute("foot_particles_enabled");
		JsonWriter.WriteBoolValue(g_Config.m_QmFootParticles != 0);
		JsonWriter.WriteAttribute("remote_particles_enabled");
		JsonWriter.WriteBoolValue(g_Config.m_QmClientMarkTrail != 0);
		JsonWriter.WriteAttribute("voice_supported");
		JsonWriter.WriteBoolValue(true);
		JsonWriter.EndObject();
		PlayerCount++;
	}

	JsonWriter.EndArray();
	JsonWriter.EndObject();

	std::string JsonBody = JsonWriter.GetOutputString();
	char aUrl[256];
	if(!BuildQmClientRecognitionUrl(QMCLIENT_REPORT_PATH, aUrl, sizeof(aUrl)))
		return;

	m_pQmClientUsersSendTask = HttpPostJson(aUrl, JsonBody.c_str());
	m_pQmClientUsersSendTask->AllowInsecureProtocol();
	m_pQmClientUsersSendTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pQmClientUsersSendTask->IpResolve(IPRESOLVE::V4);
	m_pQmClientUsersSendTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmClientUsersSendTask);
	str_copy(m_aQmClientPendingVoicePresenceServerAddress, aServerAddress, sizeof(m_aQmClientPendingVoicePresenceServerAddress));
	m_QmClientPendingVoicePresencePlayers = PlayerCount;
}

void CQmClient::FetchQmClientUsers()
{
	if(m_pQmClientUsersTask && !m_pQmClientUsersTask->Done())
		return;

	char aUrl[256];
	if(!BuildQmClientRecognitionUrl(QMCLIENT_USERS_PATH, aUrl, sizeof(aUrl)))
		return;

	m_pQmClientUsersTask = HttpGet(aUrl);
	m_pQmClientUsersTask->AllowInsecureProtocol();
	m_pQmClientUsersTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pQmClientUsersTask->IpResolve(IPRESOLVE::V4);
	m_pQmClientUsersTask->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pQmClientUsersTask);
	if(!m_QmClientDistributionSuccessLatched)
		LogQmClientDistributionRequestEvent("request", aUrl);
}

void CQmClient::FinishQmClientAuthToken()
{
	if(!m_pQmClientAuthTokenTask || m_pQmClientAuthTokenTask->State() != EHttpState::DONE)
		return;

	json_value *pRoot = m_pQmClientAuthTokenTask->ResultJson();
	if(!pRoot)
		return;

	const json_value *pToken = JsonObjectField(pRoot, "auth_token");
	if(pToken == &json_value_none)
		pToken = JsonObjectField(pRoot, "token");
	if(pToken == &json_value_none)
		pToken = JsonObjectField(pRoot, "qid");
	if(pToken != &json_value_none && pToken->type == json_string)
	{
		str_copy(m_aQmClientAuthToken, pToken->u.string.ptr, sizeof(m_aQmClientAuthToken));
		m_QmClientLastSync = 0;
		LogQmClientRecognitionEvent("token_ok", "auth token updated");
	}
	else
	{
		LogQmClientRecognitionEvent("token_missing", "response did not contain auth token");
	}
	json_value_free(pRoot);
}

void CQmClient::FinishQmClientUsers()
{
	if(m_pQmClientUsersParseJob)
	{
		if(!m_pQmClientUsersParseJob->Done())
			return;

		auto pParseJob = std::static_pointer_cast<CQmClientUsersParseJob>(m_pQmClientUsersParseJob);
		const int64_t ExpireTick = pParseJob->ExpireTick();
		CQmClientUsersParseJob::SResult Result = pParseJob->TakeResult();
		m_pQmClientUsersParseJob = nullptr;

		GameClient()->ClearQ1menGSyncMarks();
		GameClient()->ClearQmVoiceSyncMarks();
		ClearQmClientServerDistribution();

		if(!Result.m_Parsed)
		{
			m_QmClientDistributionSuccessLatched = false;
			LogQmClientDistributionFailureEvent("parse_failed", "users payload could not be parsed");
			return;
		}

		m_vQmClientServerDistribution = std::move(Result.m_vServerDistribution);
		m_QmClientOnlineUserCount = Result.m_OnlineUserCount;
		m_QmClientOnlineDummyCount = Result.m_OnlineDummyCount;
		if(!m_QmClientDistributionSuccessLatched)
			LogQmClientDistributionEvent("parse_ok", Result.m_OnlineUserCount, Result.m_OnlineDummyCount, (int)Result.m_vLocalServerMarks.size());
		m_QmClientDistributionSuccessLatched = true;
		for(const auto &Mark : Result.m_vLocalServerMarks)
		{
			for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
			{
				if(!GameClient()->m_aClients[ClientId].m_Active || str_comp(GameClient()->m_aClients[ClientId].m_aName, Mark.m_Name.c_str()) != 0)
					continue;

				GameClient()->MarkQ1menGSyncClient(ClientId, ExpireTick, Mark.m_FootParticlesEnabled, Mark.m_RemoteParticlesEnabled, Mark.m_Qid.c_str(), Mark.m_ClientBrand);
				if(Mark.m_VoiceSupported)
					GameClient()->MarkQmVoiceSupportedClient(ClientId, ExpireTick);
				break;
			}
		}
		return;
	}

	GameClient()->ClearQ1menGSyncMarks();
	GameClient()->ClearQmVoiceSyncMarks();

	if(!m_pQmClientUsersTask)
	{
		ClearQmClientServerDistribution();
		return;
	}

	if(m_pQmClientUsersTask->State() != EHttpState::DONE)
	{
		GameClient()->ClearQ1menGSyncMarks();
		GameClient()->ClearQmVoiceSyncMarks();
		ClearQmClientServerDistribution();
		const EHttpState State = m_pQmClientUsersTask->State();
		const int StatusCode = State == EHttpState::DONE ? m_pQmClientUsersTask->StatusCode() : -1;
		char aFailure[128];
		str_format(aFailure, sizeof(aFailure), "state=%d status=%d", (int)State, StatusCode);
		m_pQmClientUsersTask = nullptr;
		m_QmClientDistributionSuccessLatched = false;
		LogQmClientDistributionFailureEvent("request_failed", aFailure);
		return;
	}

	char aServerAddress[NETADDR_MAXSTRSIZE] = "";
	if(Client()->State() == IClient::STATE_ONLINE)
	{
		const NETADDR *pServerAddr = Client()->ServerAddress();
		if(pServerAddr)
			net_addr_str(pServerAddr, aServerAddress, sizeof(aServerAddress), true);
	}
	const bool FastSync = NeedsFastQmClientSync();
	const int SyncInterval = FastSync ? QMCLIENT_VOICE_SYNC_INTERVAL_SECONDS : QMCLIENT_SYNC_INTERVAL_SECONDS;
	const int64_t ExpireTick = time_get() + (int64_t)SyncInterval * time_freq() * 2;
	m_pQmClientUsersParseJob = std::make_shared<CQmClientUsersParseJob>(m_pQmClientUsersTask, aServerAddress, ExpireTick);
	Engine()->AddJob(m_pQmClientUsersParseJob);
	m_pQmClientUsersTask = nullptr;
}

void CQmClient::SyncQmClientUsers()
{
	if(m_pQmClientUsersTask && !m_pQmClientUsersTask->Done())
		return;
	if(m_pQmClientUsersParseJob && !m_pQmClientUsersParseJob->Done())
		return;
	if(Client()->State() == IClient::STATE_ONLINE && m_pQmClientUsersSendTask && !m_pQmClientUsersSendTask->Done())
		return;

	if(m_aQmClientAuthToken[0] == '\0')
	{
		FetchQmClientAuthToken();
		return;
	}

	if(Client()->State() == IClient::STATE_ONLINE)
		SendQmClientPlayerData();
	FetchQmClientUsers();
}

void CQmClient::UpdateQmClientRecognition()
{
	const bool Online = Client()->State() == IClient::STATE_ONLINE;
	if(!Online)
	{
		GameClient()->ClearQ1menGSyncMarks();
		GameClient()->ClearQmVoiceSyncMarks();
	}

	if(m_pQmClientAuthTokenTask && m_pQmClientAuthTokenTask->Done())
	{
		FinishQmClientAuthToken();
		m_pQmClientAuthTokenTask = nullptr;
	}
	if(m_pQmClientUsersParseJob && m_pQmClientUsersParseJob->Done())
	{
		FinishQmClientUsers();
	}
	if(m_pQmClientUsersTask && m_pQmClientUsersTask->Done())
	{
		FinishQmClientUsers();
	}
	if(m_pQmClientUsersSendTask && m_pQmClientUsersSendTask->Done())
	{
		// Report can fail after center service restart (stale auth token).
		// Clear token to force refetch on the next sync cycle.
		const EHttpState State = m_pQmClientUsersSendTask->State();
		const int StatusCode = State == EHttpState::DONE ? m_pQmClientUsersSendTask->StatusCode() : 0;
		const bool HttpOk = StatusCode >= 200 && StatusCode < 300;
		if(State != EHttpState::DONE || !HttpOk)
		{
			LogQmClientVoicePresenceResultEvent("report_failed", m_aQmClientPendingVoicePresenceServerAddress, m_QmClientPendingVoicePresencePlayers, StatusCode, State);
			if(StatusCode == 401)
				m_aQmClientAuthToken[0] = '\0';
		}
		m_aQmClientPendingVoicePresenceServerAddress[0] = '\0';
		m_QmClientPendingVoicePresencePlayers = 0;
		m_pQmClientUsersSendTask = nullptr;
	}

	const bool NeedRecognition = NeedsQmClientRecognition();
	if(!NeedRecognition)
	{
		if(m_pQmClientAuthTokenTask || m_pQmClientUsersTask || m_pQmClientUsersSendTask || m_pQmClientUsersParseJob || m_aQmClientAuthToken[0] != '\0' || m_QmClientLastSync != 0)
			ResetQmClientRecognitionTasks();
		GameClient()->ClearQ1menGSyncMarks();
		GameClient()->ClearQmVoiceSyncMarks();
		return;
	}

	const bool FastSync = g_Config.m_QmVoiceEnable || g_Config.m_QmClientShowBadge;
	const int SyncInterval = FastSync ? QMCLIENT_VOICE_SYNC_INTERVAL_SECONDS : QMCLIENT_SYNC_INTERVAL_SECONDS;
	const int64_t IntervalTicks = (int64_t)SyncInterval * time_freq();
	const int64_t Now = time_get();

	if(m_QmClientLastSync == 0 || Now - m_QmClientLastSync >= IntervalTicks)
	{
		SyncQmClientUsers();
		m_QmClientLastSync = Now;
	}
}
