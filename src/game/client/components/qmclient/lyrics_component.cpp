#include "lyrics_component.h"

#include "lyrics/lyric_parser.h"

#include <base/log.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>

#include <game/client/components/system_media_controls.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <iterator>
#include <string>
#include <utility>

namespace
{

	static void TrimString(std::string &Str)
	{
		while(!Str.empty() && isspace((unsigned char)Str.back()))
			Str.pop_back();
		size_t Start = 0;
		while(Start < Str.size() && isspace((unsigned char)Str[Start]))
			Start++;
		if(Start > 0)
			Str.erase(0, Start);
	}

	static bool JsonIsString(const json_value *pValue)
	{
		return pValue != nullptr && pValue != &json_value_none && pValue->type == json_string;
	}

	static bool JsonIsObject(const json_value *pValue)
	{
		return pValue != nullptr && pValue != &json_value_none && pValue->type == json_object;
	}

	static bool JsonIsArray(const json_value *pValue)
	{
		return pValue != nullptr && pValue != &json_value_none && pValue->type == json_array;
	}

	static bool JsonIsInteger(const json_value *pValue)
	{
		return pValue != nullptr && pValue != &json_value_none && pValue->type == json_integer;
	}

	static const char *JsonStringOrEmpty(const json_value *pObj, const char *pName)
	{
		const json_value *pValue = json_object_get(pObj, pName);
		return JsonIsString(pValue) ? json_string_get(pValue) : "";
	}

	static int JsonIntOrDefault(const json_value *pObj, const char *pName, int Default)
	{
		const json_value *pValue = json_object_get(pObj, pName);
		return JsonIsInteger(pValue) ? json_int_get(pValue) : Default;
	}

	static const json_value *JsonGetPath(const json_value *pObj, const char *pA, const char *pB = nullptr, const char *pC = nullptr, const char *pD = nullptr)
	{
		const json_value *pValue = pObj;
		const char *apPath[] = {pA, pB, pC, pD};
		for(const char *pKey : apPath)
		{
			if(pKey == nullptr)
				break;
			if(!JsonIsObject(pValue))
				return &json_value_none;
			pValue = json_object_get(pValue, pKey);
		}
		return pValue;
	}

	static std::string ResponseText(CHttpRequest *pRequest)
	{
		unsigned char *pResult = nullptr;
		size_t ResultLength = 0;
		pRequest->Result(&pResult, &ResultLength);
		if(pResult == nullptr || ResultLength == 0)
			return {};
		return std::string(reinterpret_cast<const char *>(pResult), ResultLength);
	}

	static std::string ExtractJsonObjectText(const std::string &Text)
	{
		const size_t Start = Text.find('{');
		const size_t End = Text.rfind('}');
		if(Start == std::string::npos || End == std::string::npos || End < Start)
			return {};
		return Text.substr(Start, End - Start + 1);
	}

	static std::string ExtractXmlElementText(const std::string &Text, const char *pName)
	{
		std::string StartTag = "<";
		StartTag += pName;
		std::string EndTag = "</";
		EndTag += pName;
		EndTag += ">";
		size_t Start = Text.find(StartTag);
		while(Start != std::string::npos)
		{
			const size_t NameEnd = Start + StartTag.size();
			if(NameEnd < Text.size() && (Text[NameEnd] == '>' || isspace((unsigned char)Text[NameEnd])))
				break;
			Start = Text.find(StartTag, NameEnd);
		}
		if(Start == std::string::npos)
			return {};
		const size_t TagEnd = Text.find('>', Start);
		if(TagEnd == std::string::npos)
			return {};
		Start = TagEnd + 1;
		const size_t End = Text.find(EndTag, Start);
		if(End == std::string::npos)
			return {};
		std::string Value = Text.substr(Start, End - Start);
		static constexpr const char *pCdataStart = "<![CDATA[";
		static constexpr const char *pCdataEnd = "]]>";
		const size_t CdataStartLength = str_length(pCdataStart);
		const size_t CdataEndLength = str_length(pCdataEnd);
		if(Value.rfind(pCdataStart, 0) == 0 && Value.size() >= CdataStartLength + CdataEndLength &&
			Value.compare(Value.size() - CdataEndLength, CdataEndLength, pCdataEnd) == 0)
		{
			Value = Value.substr(CdataStartLength, Value.size() - CdataStartLength - CdataEndLength);
		}
		return Value;
	}

	static std::string ExtractXmlAttributeValue(const std::string &Text, const char *pElementName, const char *pAttributeName)
	{
		std::string ElementNeedle = "<";
		ElementNeedle += pElementName;
		size_t Element = Text.find(ElementNeedle);
		while(Element != std::string::npos)
		{
			const size_t TagEnd = Text.find('>', Element);
			if(TagEnd == std::string::npos)
				return {};
			std::string AttrNeedle = pAttributeName;
			AttrNeedle += "=\"";
			size_t Attr = Text.find(AttrNeedle, Element);
			if(Attr != std::string::npos && Attr < TagEnd)
			{
				Attr += AttrNeedle.size();
				const size_t End = Text.find('"', Attr);
				if(End != std::string::npos && End <= TagEnd)
					return Text.substr(Attr, End - Attr);
			}
			Element = Text.find(ElementNeedle, TagEnd);
		}
		return {};
	}

	static void AppendJsonField(char *pBuf, size_t BufSize, const char *pKey, const char *pValue)
	{
		char aValue[512];
		EscapeJson(aValue, sizeof(aValue), pValue ? pValue : "");
		if(pBuf[0] != '\0')
			str_append(pBuf, ",", BufSize);
		str_append(pBuf, "\"", BufSize);
		str_append(pBuf, pKey, BufSize);
		str_append(pBuf, "\":\"", BufSize);
		str_append(pBuf, aValue, BufSize);
		str_append(pBuf, "\"", BufSize);
	}

	static void BuildNeteaseHeaderJson(char *pBuf, size_t BufSize)
	{
		char aFields[1024];
		aFields[0] = '\0';
		char aBuildVer[32];
		char aRequestId[64];
		const int64_t NowSeconds = time_timestamp();
		const int64_t NowMs = NowSeconds * 1000 + (time_get() % time_freq()) * 1000 / time_freq();
		str_format(aBuildVer, sizeof(aBuildVer), "%" PRId64, NowSeconds);
		str_format(aRequestId, sizeof(aRequestId), "%" PRId64 "_%04d", NowMs, (int)(NowMs % 1000));
		AppendJsonField(aFields, sizeof(aFields), "__csrf", "");
		AppendJsonField(aFields, sizeof(aFields), "appver", "8.0.0");
		AppendJsonField(aFields, sizeof(aFields), "buildver", aBuildVer);
		AppendJsonField(aFields, sizeof(aFields), "channel", "");
		AppendJsonField(aFields, sizeof(aFields), "deviceId", "");
		AppendJsonField(aFields, sizeof(aFields), "mobilename", "");
		AppendJsonField(aFields, sizeof(aFields), "resolution", "1920x1080");
		AppendJsonField(aFields, sizeof(aFields), "os", "android");
		AppendJsonField(aFields, sizeof(aFields), "osver", "");
		AppendJsonField(aFields, sizeof(aFields), "requestId", aRequestId);
		AppendJsonField(aFields, sizeof(aFields), "versioncode", "140");
		AppendJsonField(aFields, sizeof(aFields), "MUSIC_U", "");
		str_format(pBuf, BufSize, "{%s}", aFields);
	}

	static void BuildNeteaseCookie(char *pBuf, size_t BufSize)
	{
		char aBuildVer[32];
		char aRequestId[64];
		const int64_t NowSeconds = time_timestamp();
		const int64_t NowMs = NowSeconds * 1000 + (time_get() % time_freq()) * 1000 / time_freq();
		str_format(aBuildVer, sizeof(aBuildVer), "%" PRId64, NowSeconds);
		str_format(aRequestId, sizeof(aRequestId), "%" PRId64 "_%04d", NowMs, (int)(NowMs % 1000));
		str_format(pBuf, BufSize,
			"__csrf=; appver=8.0.0; buildver=%s; channel=; deviceId=; mobilename=; resolution=1920x1080; os=android; osver=; requestId=%s; versioncode=140; MUSIC_U=",
			aBuildVer, aRequestId);
	}

	static std::string BuildKeyword(const char *pTitle, const char *pArtist)
	{
		std::string Keyword = pTitle ? pTitle : "";
		if(pArtist != nullptr && pArtist[0] != '\0')
		{
			Keyword.push_back(' ');
			Keyword.append(pArtist);
		}
		return Keyword;
	}

	static int CandidateScore(const char *pTitle, const char *pArtist, const char *pAlbum, int DurationMs, const char *pWantedTitle, const char *pWantedArtist, const char *pWantedAlbum, int WantedDurationSec)
	{
		int Score = 0;
		if(pTitle != nullptr && pTitle[0] != '\0' && pWantedTitle != nullptr && str_comp_nocase(pTitle, pWantedTitle) == 0)
			Score += 1000;
		if(pArtist != nullptr && pArtist[0] != '\0' && pWantedArtist != nullptr && str_utf8_find_nocase(pArtist, pWantedArtist) != nullptr)
			Score += 700;
		if(pWantedAlbum != nullptr && pWantedAlbum[0] != '\0' && pAlbum != nullptr && pAlbum[0] != '\0' && str_comp_nocase(pAlbum, pWantedAlbum) == 0)
			Score += 200;
		if(DurationMs > 0 && WantedDurationSec > 0)
		{
			const int DiffMs = std::abs(DurationMs - WantedDurationSec * 1000);
			if(DiffMs <= 3000)
				Score += 300;
			else if(DiffMs <= 8000)
				Score += 100;
		}
		return Score;
	}

	static void AppendUrlEncoded(char *pBuf, size_t BufSize, const char *pKey, const char *pValue)
	{
		char aEscaped[512];
		EscapeUrl(aEscaped, sizeof(aEscaped), pValue);
		if(pBuf[0] != '\0')
			str_append(pBuf, "&", BufSize);
		str_append(pBuf, pKey, BufSize);
		str_append(pBuf, "=", BufSize);
		str_append(pBuf, aEscaped, BufSize);
	}

	static bool SourceAppContains(const char *pSourceAppId, const char *pNeedle)
	{
		return pSourceAppId != nullptr && pSourceAppId[0] != '\0' && str_find_nocase(pSourceAppId, pNeedle) != nullptr;
	}

	static void FillLineState(CLyrics::SLineState &State, const CLyricLine &Line, int LineIndex, int64_t PositionMs, bool Current)
	{
		State = CLyrics::SLineState{};
		if(Line.m_Text.empty())
			return;

		str_copy(State.m_aText, Line.m_Text.c_str(), sizeof(State.m_aText));
		State.m_LineIndex = LineIndex;
		State.m_HasTimedReveal = Current && !Line.m_vSyllables.empty();
		if(State.m_HasTimedReveal)
		{
			QmLyrics::BuildVisibleLineText(Line, PositionMs, State.m_aVisibleText, sizeof(State.m_aVisibleText));
		}
		else
		{
			str_copy(State.m_aVisibleText, State.m_aText, sizeof(State.m_aVisibleText));
		}
	}

} // namespace

const char *CLyrics::EndpointName(EEndpoint Endpoint)
{
	switch(Endpoint)
	{
	case EEndpoint::QQ_SEARCH:
		return "qq-search";
	case EEndpoint::QQ_LYRIC:
		return "qq-lyric";
	case EEndpoint::QQ_LYRIC_OLD:
		return "qq-lyric-old";
	case EEndpoint::NETEASE_SEARCH:
		return "netease-search";
	case EEndpoint::NETEASE_SEARCH_LEGACY:
		return "netease-search-legacy";
	case EEndpoint::NETEASE_LYRIC:
		return "netease-lyric";
	case EEndpoint::LRCLIB_CACHED:
		return "lrclib-get-cached";
	case EEndpoint::LRCLIB_FULL:
		return "lrclib-get";
	case EEndpoint::LRCLIB_SEARCH:
		return "lrclib-search";
	}
	return "unknown";
}

const char *CLyrics::SourceName(ESource Source)
{
	switch(Source)
	{
	case ESource::QQ:
		return "qq";
	case ESource::NETEASE:
		return "netease";
	case ESource::LRCLIB:
		return "lrclib";
	}
	return "unknown";
}

void CLyrics::OnInit()
{
	// Keep legacy config variables loadable, but the HUD no longer renders these styles.
	g_Config.m_QmLyricsEnlargeFirstChar = 0;
	g_Config.m_QmLyricsShowIndicator = 0;
	(void)g_Config.m_QmLyricsFirstCharScale;
	(void)g_Config.m_QmLyricsFirstCharColor;
	(void)g_Config.m_QmLyricsIndicatorColor;
	ResetState();
}

void CLyrics::OnShutdown()
{
	CancelRequest();
	ClearLyrics();
	m_LastSignature.clear();
}

void CLyrics::ResetState()
{
	CancelRequest();
	ClearLyrics();
	m_LastSignature.clear();
	m_RequestSignature.clear();
	m_QqSongId.clear();
	m_QqSongMid.clear();
	m_NeteaseSongId.clear();
}

void CLyrics::ClearLyrics()
{
	m_State = EState::IDLE;
	m_IsSynced = false;
	m_Lines.clear();
	m_PlainLine.clear();
	m_aLastError[0] = '\0';
	m_TimelineMode = ETimelineMode::UNDECIDED;
	m_AnchorPositionMs = 0;
	m_AnchorTick = 0;
	m_AnchorRunning = false;
	m_LastMediaPlaying = false;
	m_LastSmtcPositionMs = 0;
	m_SmtcZeroCount = 0;
}

void CLyrics::CancelRequest()
{
	if(m_pRequest)
	{
		m_pRequest->Abort();
		m_pRequest.reset();
	}
}

bool CLyrics::IsValidTrack(const STrackInfo &Info) const
{
	return Info.m_aTitle[0] != '\0' && Info.m_aArtist[0] != '\0';
}

std::string CLyrics::BuildSignature(const STrackInfo &Info) const
{
	std::string Key;
	Key.reserve(512);
	Key.append(Info.m_aSourceAppId);
	Key.push_back('\n');
	Key.append(Info.m_aTitle);
	Key.push_back('\n');
	Key.append(Info.m_aArtist);
	Key.push_back('\n');
	Key.append(Info.m_aAlbum);
	return Key;
}

int64_t CLyrics::GetDisplayPositionMs(int64_t PositionMs) const
{
	if(PositionMs > 100 && m_TimelineMode != ETimelineMode::INTERNAL_TIMER)
		return PositionMs;
	if(m_AnchorTick <= 0)
		return std::max<int64_t>(0, PositionMs);
	if(!m_AnchorRunning)
		return std::max<int64_t>(0, m_AnchorPositionMs);
	const int64_t ElapsedMs = (time_get() - m_AnchorTick) * 1000 / time_freq();
	return std::max<int64_t>(0, m_AnchorPositionMs + ElapsedMs);
}

CLyrics::ESource CLyrics::PreferredSource() const
{
	const char *pSource = m_Track.m_aSourceAppId;
	if(SourceAppContains(pSource, "netease") ||
		SourceAppContains(pSource, "cloudmusic") ||
		SourceAppContains(pSource, "orpheus") ||
		SourceAppContains(pSource, "163music") ||
		SourceAppContains(pSource, "music.163"))
	{
		return ESource::NETEASE;
	}
	if(SourceAppContains(pSource, "qqmusic") ||
		SourceAppContains(pSource, "tencent.qqmusic") ||
		SourceAppContains(pSource, "qq.music"))
	{
		return ESource::QQ;
	}
	return ESource::QQ;
}

void CLyrics::StartSource(ESource Source)
{
	switch(Source)
	{
	case ESource::QQ:
		StartRequest(EEndpoint::QQ_SEARCH);
		return;
	case ESource::NETEASE:
		StartRequest(EEndpoint::NETEASE_SEARCH);
		return;
	case ESource::LRCLIB:
		StartRequest(EEndpoint::LRCLIB_SEARCH);
		return;
	}
	m_State = EState::ERROR;
}

void CLyrics::StartNextSource(ESource FailedSource)
{
	ESource aOrder[3] = {ESource::QQ, ESource::NETEASE, ESource::LRCLIB};
	if(PreferredSource() == ESource::NETEASE)
	{
		aOrder[0] = ESource::NETEASE;
		aOrder[1] = ESource::QQ;
	}

	bool FoundFailedSource = false;
	for(ESource Source : aOrder)
	{
		if(!FoundFailedSource)
		{
			FoundFailedSource = Source == FailedSource;
			continue;
		}
		StartSource(Source);
		return;
	}
	m_State = EState::ERROR;
}

void CLyrics::StartRequest(EEndpoint Endpoint)
{
	std::shared_ptr<CHttpRequest> pRequest;
	char aUrl[1024];
	aUrl[0] = '\0';
	m_RequestEndpoint = Endpoint;
	m_RequestSignature = m_LastSignature;

	if(Endpoint == EEndpoint::QQ_SEARCH)
	{
		str_copy(aUrl, "https://u.y.qq.com/cgi-bin/musicu.fcg", sizeof(aUrl));
		pRequest = std::make_shared<CHttpRequest>(aUrl);
		char aKeyword[512];
		char aKeywordEscaped[1024];
		str_copy(aKeyword, BuildKeyword(m_Track.m_aTitle, m_Track.m_aArtist).c_str(), sizeof(aKeyword));
		EscapeJson(aKeywordEscaped, sizeof(aKeywordEscaped), aKeyword);
		char aJson[1400];
		str_format(aJson, sizeof(aJson),
			"{\"req_1\":{\"method\":\"DoSearchForQQMusicDesktop\",\"module\":\"music.search.SearchCgiService\",\"param\":{\"num_per_page\":\"20\",\"page_num\":\"1\",\"query\":\"%s\",\"search_type\":0}}}",
			aKeywordEscaped);
		pRequest->PostJson(aJson);
		pRequest->HeaderString("Referer", "https://y.qq.com/");
	}
	else if(Endpoint == EEndpoint::QQ_LYRIC || Endpoint == EEndpoint::QQ_LYRIC_OLD)
	{
		if(Endpoint == EEndpoint::QQ_LYRIC)
		{
			str_copy(aUrl, "https://c.y.qq.com/qqmusic/fcgi-bin/lyric_download.fcg", sizeof(aUrl));
			pRequest = std::make_shared<CHttpRequest>(aUrl);
			char aBody[256] = {};
			AppendUrlEncoded(aBody, sizeof(aBody), "version", "15");
			AppendUrlEncoded(aBody, sizeof(aBody), "miniversion", "82");
			AppendUrlEncoded(aBody, sizeof(aBody), "lrctype", "4");
			AppendUrlEncoded(aBody, sizeof(aBody), "musicid", m_QqSongId.c_str());
			pRequest->Post(reinterpret_cast<const unsigned char *>(aBody), str_length(aBody));
			pRequest->HeaderString("Content-Type", "application/x-www-form-urlencoded");
		}
		else
		{
			str_copy(aUrl, "https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg", sizeof(aUrl));
			pRequest = std::make_shared<CHttpRequest>(aUrl);
			char aPcacheTime[32];
			str_format(aPcacheTime, sizeof(aPcacheTime), "%" PRId64, time_timestamp() * 1000);
			char aBody[512] = {};
			AppendUrlEncoded(aBody, sizeof(aBody), "callback", "MusicJsonCallback_lrc");
			AppendUrlEncoded(aBody, sizeof(aBody), "pcachetime", aPcacheTime);
			AppendUrlEncoded(aBody, sizeof(aBody), "songmid", m_QqSongMid.c_str());
			AppendUrlEncoded(aBody, sizeof(aBody), "g_tk", "5381");
			AppendUrlEncoded(aBody, sizeof(aBody), "jsonpCallback", "MusicJsonCallback_lrc");
			AppendUrlEncoded(aBody, sizeof(aBody), "loginUin", "0");
			AppendUrlEncoded(aBody, sizeof(aBody), "hostUin", "0");
			AppendUrlEncoded(aBody, sizeof(aBody), "format", "jsonp");
			AppendUrlEncoded(aBody, sizeof(aBody), "inCharset", "utf8");
			AppendUrlEncoded(aBody, sizeof(aBody), "outCharset", "utf8");
			AppendUrlEncoded(aBody, sizeof(aBody), "notice", "0");
			AppendUrlEncoded(aBody, sizeof(aBody), "platform", "yqq");
			AppendUrlEncoded(aBody, sizeof(aBody), "needNewCode", "0");
			pRequest->Post(reinterpret_cast<const unsigned char *>(aBody), str_length(aBody));
			pRequest->HeaderString("Content-Type", "application/x-www-form-urlencoded");
		}
		pRequest->HeaderString("Referer", "https://c.y.qq.com/");
	}
	else if(Endpoint == EEndpoint::NETEASE_SEARCH)
	{
		str_copy(aUrl, "https://interface.music.163.com/eapi/cloudsearch/pc", sizeof(aUrl));
		pRequest = std::make_shared<CHttpRequest>(aUrl);
		char aKeyword[512];
		char aKeywordEscaped[1024];
		str_copy(aKeyword, BuildKeyword(m_Track.m_aTitle, m_Track.m_aArtist).c_str(), sizeof(aKeyword));
		EscapeJson(aKeywordEscaped, sizeof(aKeywordEscaped), aKeyword);
		char aHeader[1024];
		char aHeaderEscaped[1600];
		BuildNeteaseHeaderJson(aHeader, sizeof(aHeader));
		EscapeJson(aHeaderEscaped, sizeof(aHeaderEscaped), aHeader);
		char aJson[2400];
		str_format(aJson, sizeof(aJson), "{\"s\":\"%s\",\"type\":\"1\",\"limit\":\"30\",\"offset\":\"0\",\"total\":\"true\",\"header\":\"%s\"}", aKeywordEscaped, aHeaderEscaped);
		char aErr[128];
		const std::string Body = QmLyrics::BuildNeteaseEapiBody(aUrl, aJson, aErr, sizeof(aErr));
		if(Body.empty())
		{
			str_copy(m_aLastError, aErr, sizeof(m_aLastError));
			StartNextFallback(m_aLastError);
			return;
		}
		pRequest->Post(reinterpret_cast<const unsigned char *>(Body.data()), Body.size());
		pRequest->HeaderString("Content-Type", "application/x-www-form-urlencoded");
		pRequest->HeaderString("Referer", "https://music.163.com/");
		char aCookie[1024];
		BuildNeteaseCookie(aCookie, sizeof(aCookie));
		pRequest->HeaderString("Cookie", aCookie);
		pRequest->HeaderString("User-Agent", "Mozilla/5.0 (Linux; Android 9; PCT-AL10) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.64 HuaweiBrowser/10.0.3.311 Mobile Safari/537.36");
	}
	else if(Endpoint == EEndpoint::NETEASE_SEARCH_LEGACY)
	{
		char aKeyword[512];
		EscapeUrl(aKeyword, sizeof(aKeyword), BuildKeyword(m_Track.m_aTitle, m_Track.m_aArtist).c_str());
		str_format(aUrl, sizeof(aUrl),
			"http://music.163.com/api/search/get/web?csrf_token=&hlpretag=&hlposttag=&s=%s&type=1&offset=0&total=true&limit=20",
			aKeyword);
		pRequest = std::make_shared<CHttpRequest>(aUrl);
		pRequest->AllowInsecureProtocol();
		pRequest->HeaderString("Referer", "https://music.163.com/");
	}
	else if(Endpoint == EEndpoint::NETEASE_LYRIC)
	{
		str_copy(aUrl, "https://interface3.music.163.com/eapi/song/lyric/v1", sizeof(aUrl));
		pRequest = std::make_shared<CHttpRequest>(aUrl);
		char aHeader[1024];
		char aHeaderEscaped[1600];
		BuildNeteaseHeaderJson(aHeader, sizeof(aHeader));
		EscapeJson(aHeaderEscaped, sizeof(aHeaderEscaped), aHeader);
		char aJson[2200];
		str_format(aJson, sizeof(aJson), "{\"id\":\"%s\",\"cp\":\"false\",\"lv\":\"0\",\"kv\":\"0\",\"tv\":\"0\",\"rv\":\"0\",\"yv\":\"0\",\"ytv\":\"0\",\"yrv\":\"0\",\"csrf_token\":\"\",\"header\":\"%s\"}", m_NeteaseSongId.c_str(), aHeaderEscaped);
		char aErr[128];
		const std::string Body = QmLyrics::BuildNeteaseEapiBody(aUrl, aJson, aErr, sizeof(aErr));
		if(Body.empty())
		{
			str_copy(m_aLastError, aErr, sizeof(m_aLastError));
			StartNextFallback(m_aLastError);
			return;
		}
		pRequest->Post(reinterpret_cast<const unsigned char *>(Body.data()), Body.size());
		pRequest->HeaderString("Content-Type", "application/x-www-form-urlencoded");
		pRequest->HeaderString("Referer", "https://music.163.com/");
		char aCookie[1024];
		BuildNeteaseCookie(aCookie, sizeof(aCookie));
		pRequest->HeaderString("Cookie", aCookie);
		pRequest->HeaderString("User-Agent", "Mozilla/5.0 (Linux; Android 9; PCT-AL10) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.64 HuaweiBrowser/10.0.3.311 Mobile Safari/537.36");
	}
	else
	{
		char aTitle[256];
		char aArtist[256];
		char aAlbum[256];
		EscapeUrl(aTitle, sizeof(aTitle), m_Track.m_aTitle);
		EscapeUrl(aArtist, sizeof(aArtist), m_Track.m_aArtist);
		EscapeUrl(aAlbum, sizeof(aAlbum), m_Track.m_aAlbum);

		if(Endpoint == EEndpoint::LRCLIB_SEARCH)
		{
			str_format(aUrl, sizeof(aUrl),
				"https://lrclib.net/api/search?track_name=%s&artist_name=%s",
				aTitle, aArtist);
		}
		else
		{
			const char *pEndpoint = Endpoint == EEndpoint::LRCLIB_CACHED ? "get-cached" : "get";
			str_format(aUrl, sizeof(aUrl),
				"https://lrclib.net/api/%s?track_name=%s&artist_name=%s&album_name=%s&duration=%d",
				pEndpoint, aTitle, aArtist, aAlbum, m_Track.m_DurationSec);
		}
		pRequest = std::make_shared<CHttpRequest>(aUrl);
		pRequest->HeaderString("Accept", "application/json");
	}

	pRequest->LogProgress(HTTPLOG::FAILURE);
	pRequest->FailOnErrorStatus(false);
	pRequest->Timeout(CTimeout{10000, 0, 500, 10});
	if(Endpoint != EEndpoint::NETEASE_SEARCH && Endpoint != EEndpoint::NETEASE_LYRIC)
		pRequest->HeaderString("User-Agent", "QimenGClient");

	m_pRequest = pRequest;
	m_State = EState::REQUESTING;

	Http()->Run(pRequest);
	log_info("lyrics", "request start endpoint=%s url='%s' source='%s' title='%s' artist='%s' album='%s' duration=%d",
		EndpointName(Endpoint), aUrl, m_Track.m_aSourceAppId, m_Track.m_aTitle, m_Track.m_aArtist, m_Track.m_aAlbum, m_Track.m_DurationSec);
}

void CLyrics::StartNextFallback(const char *pReason)
{
	log_info("lyrics", "fallback from endpoint=%s reason='%s'", EndpointName(m_RequestEndpoint), pReason ? pReason : "");
	switch(m_RequestEndpoint)
	{
	case EEndpoint::QQ_SEARCH:
		StartNextSource(ESource::QQ);
		return;
	case EEndpoint::QQ_LYRIC:
		if(!m_QqSongMid.empty())
		{
			StartRequest(EEndpoint::QQ_LYRIC_OLD);
			return;
		}
		StartNextSource(ESource::QQ);
		return;
	case EEndpoint::QQ_LYRIC_OLD:
		StartNextSource(ESource::QQ);
		return;
	case EEndpoint::NETEASE_SEARCH:
		StartRequest(EEndpoint::NETEASE_SEARCH_LEGACY);
		return;
	case EEndpoint::NETEASE_SEARCH_LEGACY:
	case EEndpoint::NETEASE_LYRIC:
		StartNextSource(ESource::NETEASE);
		return;
	case EEndpoint::LRCLIB_SEARCH:
	case EEndpoint::LRCLIB_FULL:
	case EEndpoint::LRCLIB_CACHED:
		m_State = EState::ERROR;
		return;
	}
	m_State = EState::ERROR;
}

void CLyrics::HandleRequestDone()
{
	std::shared_ptr<CHttpRequest> pRequest = m_pRequest;
	m_pRequest.reset();

	if(m_RequestSignature != m_LastSignature)
		return;

	if(pRequest->State() == EHttpState::ABORTED)
	{
		str_copy(m_aLastError, "Aborted", sizeof(m_aLastError));
		m_State = EState::ERROR;
		return;
	}
	if(pRequest->State() != EHttpState::DONE)
	{
		str_copy(m_aLastError, "Curl error", sizeof(m_aLastError));
		StartNextFallback(m_aLastError);
		return;
	}

	const int StatusCode = pRequest->StatusCode();
	if(StatusCode == 404 && m_RequestEndpoint == EEndpoint::LRCLIB_CACHED)
	{
		StartRequest(EEndpoint::LRCLIB_FULL);
		return;
	}
	if(StatusCode != 200)
	{
		str_format(m_aLastError, sizeof(m_aLastError), "HTTP %d", StatusCode);
		log_error("lyrics", "request fail endpoint=%s http=%d", EndpointName(m_RequestEndpoint), StatusCode);
		StartNextFallback(m_aLastError);
		return;
	}

	bool Ok = false;
	const bool RawResponse = m_RequestEndpoint == EEndpoint::QQ_LYRIC || m_RequestEndpoint == EEndpoint::QQ_LYRIC_OLD;
	json_value *pObj = RawResponse ? nullptr : pRequest->ResultJson();
	switch(m_RequestEndpoint)
	{
	case EEndpoint::QQ_SEARCH:
		Ok = ParseQqSearchResponse(pObj, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::QQ_LYRIC:
		Ok = ParseQqLyricResponse(pRequest.get(), false, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::QQ_LYRIC_OLD:
		Ok = ParseQqLyricResponse(pRequest.get(), true, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::NETEASE_SEARCH:
	case EEndpoint::NETEASE_SEARCH_LEGACY:
		Ok = ParseNeteaseSearchResponse(pObj, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::NETEASE_LYRIC:
		Ok = ParseNeteaseLyricResponse(pObj, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::LRCLIB_SEARCH:
		Ok = ParseSearchResponse(pObj, m_aLastError, sizeof(m_aLastError));
		break;
	case EEndpoint::LRCLIB_CACHED:
	case EEndpoint::LRCLIB_FULL:
		Ok = ParseLyricsResponse(pObj, m_aLastError, sizeof(m_aLastError));
		break;
	}
	if(pObj)
		json_value_free(pObj);
	if(!Ok)
	{
		log_error("lyrics", "request fail endpoint=%s error='%s'", EndpointName(m_RequestEndpoint), m_aLastError);
		StartNextFallback(m_aLastError);
		return;
	}

	if(m_RequestEndpoint == EEndpoint::QQ_SEARCH)
	{
		StartRequest(EEndpoint::QQ_LYRIC);
		return;
	}
	if(m_RequestEndpoint == EEndpoint::NETEASE_SEARCH || m_RequestEndpoint == EEndpoint::NETEASE_SEARCH_LEGACY)
	{
		StartRequest(EEndpoint::NETEASE_LYRIC);
		return;
	}

	m_State = EState::READY;
	log_info("lyrics", "request ok endpoint=%s lines=%zu synced=%d",
		EndpointName(m_RequestEndpoint), m_Lines.size(), m_IsSynced ? 1 : 0);
}

bool CLyrics::ApplyLines(std::vector<CLyricLine> &&vLines, char *pErr, size_t ErrSize)
{
	if(vLines.empty())
	{
		str_copy(pErr, "No parsed lyrics", ErrSize);
		return false;
	}
	m_Lines = std::move(vLines);
	QmLyrics::SortAndFillDurations(m_Lines);
	m_IsSynced = true;
	return true;
}

bool CLyrics::ApplyLyrics(const std::string &Plain, const std::string &Synced, bool Instrumental, char *pErr, size_t ErrSize)
{
	if(Instrumental || (Plain.empty() && Synced.empty()))
	{
		str_copy(pErr, "No lyric in response", ErrSize);
		return false;
	}

	if(!Synced.empty())
	{
		std::vector<CLyricLine> vLines;
		char aParseErr[128];
		if(QmLyrics::ParseLrcLyrics(Synced, vLines, aParseErr, sizeof(aParseErr)))
			return ApplyLines(std::move(vLines), pErr, ErrSize);
	}

	if(!Plain.empty())
	{
		const char *pLineStart = Plain.c_str();
		while(*pLineStart)
		{
			const char *pLineEnd = str_find(pLineStart, "\n");
			if(!pLineEnd)
				pLineEnd = pLineStart + str_length(pLineStart);
			std::string Line(pLineStart, pLineEnd);
			if(!Line.empty() && Line.back() == '\r')
				Line.pop_back();
			TrimString(Line);
			if(!Line.empty())
			{
				m_PlainLine = Line;
				m_IsSynced = false;
				return true;
			}
			pLineStart = *pLineEnd ? pLineEnd + 1 : pLineEnd;
		}
	}

	str_copy(pErr, "No usable lyric", ErrSize);
	return false;
}

bool CLyrics::ParseQqSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize)
{
	if(!JsonIsObject(pObj))
	{
		str_copy(pErr, "QQ search is not object", ErrSize);
		return false;
	}
	const json_value *pList = JsonGetPath(pObj, "req_1", "data", "body", "song");
	if(!JsonIsObject(pList))
		pList = JsonGetPath(pObj, "music.search.SearchCgiService", "data", "body", "song");
	if(JsonIsObject(pList))
		pList = json_object_get(pList, "list");
	if(!JsonIsArray(pList))
	{
		str_copy(pErr, "No QQ songs", ErrSize);
		return false;
	}

	std::string BestId;
	std::string BestMid;
	int BestScore = -1000000;
	const int Count = json_array_length(pList);
	for(int i = 0; i < Count; ++i)
	{
		const json_value *pSong = json_array_get(pList, i);
		if(!JsonIsObject(pSong))
			continue;
		const char *pTitle = JsonStringOrEmpty(pSong, "title");
		if(pTitle[0] == '\0')
			pTitle = JsonStringOrEmpty(pSong, "name");
		const char *pMid = JsonStringOrEmpty(pSong, "mid");
		if(pMid[0] == '\0')
			pMid = JsonStringOrEmpty(pSong, "songmid");
		const int SongId = JsonIntOrDefault(pSong, "id", JsonIntOrDefault(pSong, "songid", 0));
		const int DurationMs = JsonIntOrDefault(pSong, "interval", 0) * 1000;
		std::string Artist;
		const json_value *pSingerArray = json_object_get(pSong, "singer");
		if(JsonIsArray(pSingerArray))
		{
			const int SingerCount = json_array_length(pSingerArray);
			for(int SingerIndex = 0; SingerIndex < SingerCount; ++SingerIndex)
			{
				const json_value *pSinger = json_array_get(pSingerArray, SingerIndex);
				if(!JsonIsObject(pSinger))
					continue;
				if(!Artist.empty())
					Artist.append("/");
				Artist.append(JsonStringOrEmpty(pSinger, "name"));
			}
		}
		const json_value *pAlbum = json_object_get(pSong, "album");
		const char *pAlbumName = JsonIsObject(pAlbum) ? JsonStringOrEmpty(pAlbum, "title") : "";
		if(pAlbumName[0] == '\0' && JsonIsObject(pAlbum))
			pAlbumName = JsonStringOrEmpty(pAlbum, "name");
		const int Score = CandidateScore(pTitle, Artist.c_str(), pAlbumName, DurationMs, m_Track.m_aTitle, m_Track.m_aArtist, m_Track.m_aAlbum, m_Track.m_DurationSec);
		if(Score > BestScore && (SongId > 0 || pMid[0] != '\0'))
		{
			char aSongId[32];
			str_format(aSongId, sizeof(aSongId), "%d", SongId);
			BestId = SongId > 0 ? aSongId : pMid;
			BestMid = pMid;
			BestScore = Score;
		}
	}
	if(BestId.empty())
	{
		str_copy(pErr, "No QQ match", ErrSize);
		return false;
	}
	m_QqSongId = std::move(BestId);
	m_QqSongMid = std::move(BestMid);
	return true;
}

bool CLyrics::ParseQqLyricResponse(CHttpRequest *pRequest, bool OldEndpoint, char *pErr, size_t ErrSize)
{
	std::string Body = ResponseText(pRequest);
	if(Body.empty())
	{
		str_copy(pErr, "QQ lyric empty", ErrSize);
		return false;
	}
	std::string Payload;
	std::string Translation;
	std::string Romanized;
	if(OldEndpoint)
	{
		const std::string JsonText = ExtractJsonObjectText(Body);
		json_value *pObj = JsonParse(reinterpret_cast<const json_char *>(JsonText.data()), JsonText.size());
		if(!JsonIsObject(pObj))
		{
			if(pObj)
				json_value_free(pObj);
			str_copy(pErr, "QQ old lyric invalid", ErrSize);
			return false;
		}
		Payload = JsonStringOrEmpty(pObj, "lyric");
		Translation = JsonStringOrEmpty(pObj, "trans");
		json_value_free(pObj);
	}
	else
	{
		Payload = ExtractXmlElementText(Body, "content");
		Translation = ExtractXmlElementText(Body, "contentts");
		Romanized = ExtractXmlElementText(Body, "contentroma");
		if(Payload.empty())
			Payload = ExtractXmlElementText(Body, "Lyric_1");
	}

	std::string LyricText;
	if(!QmLyrics::DecryptQqQrcPayload(Payload, LyricText, pErr, ErrSize))
		return false;
	if(LyricText.find("<?xml") != std::string::npos)
	{
		const std::string InnerLyric = ExtractXmlAttributeValue(LyricText, "Lyric_1", "LyricContent");
		if(!InnerLyric.empty())
			LyricText = InnerLyric;
	}

	std::vector<CLyricLine> vLines;
	if(!QmLyrics::ParseQrcLyrics(LyricText, vLines, pErr, ErrSize) &&
		!QmLyrics::ParseLrcLyrics(LyricText, vLines, pErr, ErrSize))
	{
		return false;
	}
	if(!Translation.empty())
	{
		std::string TranslationText;
		if(QmLyrics::DecryptQqQrcPayload(Translation, TranslationText, nullptr, 0))
			QmLyrics::MergeLineTextByTimestamp(vLines, TranslationText, true, nullptr, 0);
	}
	if(!Romanized.empty())
	{
		std::string RomanizedText;
		if(QmLyrics::DecryptQqQrcPayload(Romanized, RomanizedText, nullptr, 0))
			QmLyrics::MergeLineTextByTimestamp(vLines, RomanizedText, false, nullptr, 0);
	}
	return ApplyLines(std::move(vLines), pErr, ErrSize);
}

bool CLyrics::ParseNeteaseSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize)
{
	if(!JsonIsObject(pObj))
	{
		str_copy(pErr, "Netease search is not object", ErrSize);
		return false;
	}
	const json_value *pSongs = JsonGetPath(pObj, "result", "songs");
	if(!JsonIsArray(pSongs))
	{
		str_copy(pErr, "No Netease songs", ErrSize);
		return false;
	}
	int BestScore = -1000000;
	std::string BestId;
	const int Count = json_array_length(pSongs);
	for(int i = 0; i < Count; ++i)
	{
		const json_value *pSong = json_array_get(pSongs, i);
		if(!JsonIsObject(pSong))
			continue;
		const int SongId = JsonIntOrDefault(pSong, "id", 0);
		if(SongId <= 0)
			continue;
		const char *pTitle = JsonStringOrEmpty(pSong, "name");
		const int DurationMs = JsonIntOrDefault(pSong, "duration", JsonIntOrDefault(pSong, "dt", 0));
		std::string Artist;
		const json_value *pArtists = json_object_get(pSong, "artists");
		if(!JsonIsArray(pArtists))
			pArtists = json_object_get(pSong, "ar");
		if(JsonIsArray(pArtists))
		{
			const int ArtistCount = json_array_length(pArtists);
			for(int ArtistIndex = 0; ArtistIndex < ArtistCount; ++ArtistIndex)
			{
				const json_value *pArtist = json_array_get(pArtists, ArtistIndex);
				if(!JsonIsObject(pArtist))
					continue;
				if(!Artist.empty())
					Artist.append("/");
				Artist.append(JsonStringOrEmpty(pArtist, "name"));
			}
		}
		const json_value *pAlbum = json_object_get(pSong, "album");
		if(!JsonIsObject(pAlbum))
			pAlbum = json_object_get(pSong, "al");
		const char *pAlbumName = JsonIsObject(pAlbum) ? JsonStringOrEmpty(pAlbum, "name") : "";
		const int Score = CandidateScore(pTitle, Artist.c_str(), pAlbumName, DurationMs, m_Track.m_aTitle, m_Track.m_aArtist, m_Track.m_aAlbum, m_Track.m_DurationSec);
		if(Score > BestScore)
		{
			char aSongId[32];
			str_format(aSongId, sizeof(aSongId), "%d", SongId);
			BestId = aSongId;
			BestScore = Score;
		}
	}
	if(BestId.empty())
	{
		str_copy(pErr, "No Netease match", ErrSize);
		return false;
	}
	m_NeteaseSongId = std::move(BestId);
	return true;
}

bool CLyrics::ParseNeteaseLyricResponse(const json_value *pObj, char *pErr, size_t ErrSize)
{
	if(!JsonIsObject(pObj))
	{
		str_copy(pErr, "Netease lyric is not object", ErrSize);
		return false;
	}
	std::string Yrc = JsonStringOrEmpty(JsonGetPath(pObj, "yrc"), "lyric");
	std::string Lrc = JsonStringOrEmpty(JsonGetPath(pObj, "lrc"), "lyric");
	std::string Translation = JsonStringOrEmpty(JsonGetPath(pObj, "tlyric"), "lyric");
	std::string Romanized = JsonStringOrEmpty(JsonGetPath(pObj, "romalrc"), "lyric");

	std::vector<CLyricLine> vLines;
	if(!Yrc.empty() && QmLyrics::ParseYrcLyrics(Yrc, vLines, pErr, ErrSize))
	{
		if(!Translation.empty())
			QmLyrics::MergeLineTextByTimestamp(vLines, Translation, true, nullptr, 0);
		if(!Romanized.empty())
			QmLyrics::MergeLineTextByTimestamp(vLines, Romanized, false, nullptr, 0);
		return ApplyLines(std::move(vLines), pErr, ErrSize);
	}
	if(!Lrc.empty() && QmLyrics::ParseLrcLyrics(Lrc, vLines, pErr, ErrSize))
	{
		if(!Translation.empty())
			QmLyrics::MergeLineTextByTimestamp(vLines, Translation, true, nullptr, 0);
		return ApplyLines(std::move(vLines), pErr, ErrSize);
	}

	str_copy(pErr, "No Netease lyric", ErrSize);
	return false;
}

bool CLyrics::ParseLyricsResponse(const json_value *pObj, char *pErr, size_t ErrSize)
{
	ClearLyrics();
	if(!JsonIsObject(pObj))
	{
		str_copy(pErr, "Response is not object", ErrSize);
		return false;
	}

	const json_value *pInstr = json_object_get(pObj, "instrumental");
	const bool Instrumental = pInstr != &json_value_none && pInstr->type == json_boolean ? json_boolean_get(pInstr) != 0 : false;
	std::string Plain;
	std::string Synced;
	const json_value *pPlain = json_object_get(pObj, "plainLyrics");
	if(JsonIsString(pPlain))
		Plain = json_string_get(pPlain);
	const json_value *pSynced = json_object_get(pObj, "syncedLyrics");
	if(JsonIsString(pSynced))
		Synced = json_string_get(pSynced);

	return ApplyLyrics(Plain, Synced, Instrumental, pErr, ErrSize);
}

bool CLyrics::ParseSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize)
{
	ClearLyrics();
	if(!JsonIsArray(pObj))
	{
		str_copy(pErr, "Response is not array", ErrSize);
		return false;
	}

	struct SCandidate
	{
		std::string m_Track;
		std::string m_Artist;
		std::string m_Album;
		std::string m_Plain;
		std::string m_Synced;
		bool m_Instrumental = false;
	};

	const int Count = json_array_length(pObj);
	if(Count <= 0)
	{
		str_copy(pErr, "No matching lyrics", ErrSize);
		return false;
	}

	SCandidate Best;
	bool HasBest = false;
	int BestScore = -1000000;

	for(int i = 0; i < Count; ++i)
	{
		const json_value *pItem = json_array_get(pObj, i);
		if(!JsonIsObject(pItem))
			continue;

		SCandidate Candidate;
		const json_value *pTrack = json_object_get(pItem, "trackName");
		if(JsonIsString(pTrack))
			Candidate.m_Track = json_string_get(pTrack);
		const json_value *pArtist = json_object_get(pItem, "artistName");
		if(JsonIsString(pArtist))
			Candidate.m_Artist = json_string_get(pArtist);
		const json_value *pAlbum = json_object_get(pItem, "albumName");
		if(JsonIsString(pAlbum))
			Candidate.m_Album = json_string_get(pAlbum);
		const json_value *pInstr = json_object_get(pItem, "instrumental");
		if(pInstr != &json_value_none && pInstr->type == json_boolean)
			Candidate.m_Instrumental = json_boolean_get(pInstr) != 0;
		const json_value *pPlain = json_object_get(pItem, "plainLyrics");
		if(JsonIsString(pPlain))
			Candidate.m_Plain = json_string_get(pPlain);
		const json_value *pSynced = json_object_get(pItem, "syncedLyrics");
		if(JsonIsString(pSynced))
			Candidate.m_Synced = json_string_get(pSynced);

		if(Candidate.m_Instrumental || (Candidate.m_Plain.empty() && Candidate.m_Synced.empty()))
			continue;

		int Score = CandidateScore(Candidate.m_Track.c_str(), Candidate.m_Artist.c_str(), Candidate.m_Album.c_str(), 0, m_Track.m_aTitle, m_Track.m_aArtist, m_Track.m_aAlbum, m_Track.m_DurationSec);
		if(!Candidate.m_Synced.empty())
			Score += 50;
		else if(!Candidate.m_Plain.empty())
			Score += 10;

		if(!HasBest || Score > BestScore)
		{
			Best = std::move(Candidate);
			BestScore = Score;
			HasBest = true;
		}
	}

	if(!HasBest)
	{
		str_copy(pErr, "No matching lyrics", ErrSize);
		return false;
	}

	return ApplyLyrics(Best.m_Plain, Best.m_Synced, Best.m_Instrumental, pErr, ErrSize);
}

void CLyrics::OnUpdate()
{
	if(!g_Config.m_QmSmtcLyricsEnable)
	{
		return;
	}

	CSystemMediaControls::SState MediaState;
	if(!GameClient()->m_SystemMediaControls.GetStateSnapshot(MediaState))
	{
		if(g_Config.m_QmLyricsAutoHideNoSmtc && !m_LastSignature.empty())
			ResetState();
		return;
	}

	STrackInfo CurrentTrack;
	str_copy(CurrentTrack.m_aSourceAppId, MediaState.m_aSourceAppId, sizeof(CurrentTrack.m_aSourceAppId));
	str_copy(CurrentTrack.m_aTitle, MediaState.m_aTitle, sizeof(CurrentTrack.m_aTitle));
	str_copy(CurrentTrack.m_aArtist, MediaState.m_aArtist, sizeof(CurrentTrack.m_aArtist));
	str_copy(CurrentTrack.m_aAlbum, MediaState.m_aAlbum, sizeof(CurrentTrack.m_aAlbum));
	CurrentTrack.m_DurationSec = (int)((MediaState.m_DurationMs + 500) / 1000);

	if(!IsValidTrack(CurrentTrack))
	{
		if(!m_LastSignature.empty())
			ResetState();
		return;
	}

	const std::string Signature = BuildSignature(CurrentTrack);
	const bool NeedsRequestForCurrentTrack = Signature == m_LastSignature && m_State == EState::IDLE && !m_pRequest && m_Lines.empty() && m_PlainLine.empty();
	if(Signature != m_LastSignature || NeedsRequestForCurrentTrack)
	{
		CancelRequest();
		ClearLyrics();
		m_QqSongId.clear();
		m_QqSongMid.clear();
		m_NeteaseSongId.clear();
		m_Track = CurrentTrack;
		m_LastSignature = Signature;
		m_TimelineMode = IsInternalTimerSource(MediaState.m_aSourceAppId) ? ETimelineMode::INTERNAL_TIMER : ETimelineMode::UNDECIDED;
		m_AnchorPositionMs = MediaState.m_PositionMs;
		m_AnchorTick = time_get();
		m_AnchorRunning = MediaState.m_Playing;
		m_LastMediaPlaying = MediaState.m_Playing;
		m_LastSmtcPositionMs = MediaState.m_PositionMs;
		m_SmtcZeroCount = 0;
		const ESource Source = PreferredSource();
		log_info("lyrics", "track source='%s' preferred=%s mode=%s title='%s' artist='%s'",
			m_Track.m_aSourceAppId, SourceName(Source),
			m_TimelineMode == ETimelineMode::INTERNAL_TIMER ? "internal-timer" : "undecided",
			m_Track.m_aTitle, m_Track.m_aArtist);
		StartSource(Source);
		return;
	}

	UpdateTimeline(MediaState.m_Playing, MediaState.m_PositionMs, MediaState.m_aSourceAppId);

	if(!m_pRequest)
		return;

	if(m_pRequest->State() == EHttpState::RUNNING || m_pRequest->State() == EHttpState::QUEUED)
		return;

	HandleRequestDone();
}

bool CLyrics::IsInternalTimerSource(const char *pSourceAppId) const
{
	// Players that don't expose SMTC timeline data — must rely on local clock estimation.
	return SourceAppContains(pSourceAppId, "cloudmusic") ||
	       SourceAppContains(pSourceAppId, "netease") ||
	       SourceAppContains(pSourceAppId, "163music") ||
	       SourceAppContains(pSourceAppId, "music.163") ||
	       SourceAppContains(pSourceAppId, "foobar2000") ||
	       SourceAppContains(pSourceAppId, "kugou");
}

void CLyrics::UpdateTimeline(bool MediaPlaying, int64_t SmtcPositionMs, const char *pSourceAppId)
{
	const int64_t NowTick = time_get();
	const bool PlaybackStateChanged = MediaPlaying != m_LastMediaPlaying;

	// Mode auto-detection: after ~3s of playback, if SMTC keeps reporting zero,
	// fall back to internal timer for this track.
	if(m_TimelineMode == ETimelineMode::UNDECIDED)
	{
		if(MediaPlaying)
		{
			if(SmtcPositionMs <= 100)
			{
				m_SmtcZeroCount++;
				if(m_SmtcZeroCount >= 30)
				{
					m_TimelineMode = ETimelineMode::INTERNAL_TIMER;
					log_info("lyrics", "switched to internal-timer mode for source='%s'", pSourceAppId ? pSourceAppId : "");
				}
			}
			else
			{
				m_TimelineMode = ETimelineMode::SMTC_TRUSTED;
				m_AnchorPositionMs = SmtcPositionMs;
				m_AnchorTick = NowTick;
				m_AnchorRunning = true;
				log_info("lyrics", "switched to smtc-trusted mode for source='%s' position=%" PRId64 "ms", pSourceAppId ? pSourceAppId : "", SmtcPositionMs);
			}
		}
	}

	if(m_TimelineMode == ETimelineMode::INTERNAL_TIMER)
	{
		// Local-clock mode: SMTC position is unreliable. Anchor only moves on
		// play/pause edges and when SMTC reports a believable non-zero jump.
		if(PlaybackStateChanged)
		{
			if(MediaPlaying)
			{
				// Resume: keep anchor position, restart the wallclock.
				m_AnchorTick = NowTick;
				m_AnchorRunning = true;
			}
			else
			{
				// Pause: freeze the displayed position into the anchor.
				if(m_AnchorRunning)
				{
					const int64_t ElapsedMs = (NowTick - m_AnchorTick) * 1000 / time_freq();
					m_AnchorPositionMs = std::max<int64_t>(0, m_AnchorPositionMs + ElapsedMs);
				}
				m_AnchorTick = NowTick;
				m_AnchorRunning = false;
			}
		}
		else if(MediaPlaying && SmtcPositionMs > 1000)
		{
			// Rare: a normally-silent player suddenly reported a real position
			// (e.g. user manually seeked). Resync.
			const int64_t Estimated = m_AnchorRunning ? m_AnchorPositionMs + (NowTick - m_AnchorTick) * 1000 / time_freq() : m_AnchorPositionMs;
			const int64_t Diff = std::abs(SmtcPositionMs - Estimated);
			if(Diff > 3000)
			{
				log_info("lyrics", "internal-timer resync via smtc position=%" PRId64 "ms estimated=%" PRId64 "ms", SmtcPositionMs, Estimated);
				m_AnchorPositionMs = SmtcPositionMs;
				m_AnchorTick = NowTick;
				m_AnchorRunning = true;
			}
		}
	}
	else if(m_TimelineMode == ETimelineMode::SMTC_TRUSTED)
	{
		// SMTC position is trustworthy. Re-anchor on every report.
		if(PlaybackStateChanged)
		{
			if(MediaPlaying)
			{
				m_AnchorPositionMs = SmtcPositionMs;
				m_AnchorTick = NowTick;
				m_AnchorRunning = true;
			}
			else
			{
				m_AnchorPositionMs = SmtcPositionMs;
				m_AnchorTick = NowTick;
				m_AnchorRunning = false;
			}
		}
		else if(MediaPlaying)
		{
			// Detect seeks (large jumps vs. our prediction).
			const int64_t Estimated = m_AnchorRunning ? m_AnchorPositionMs + (NowTick - m_AnchorTick) * 1000 / time_freq() : m_AnchorPositionMs;
			const int64_t Diff = std::abs(SmtcPositionMs - Estimated);
			if(Diff > 1500)
			{
				m_AnchorPositionMs = SmtcPositionMs;
				m_AnchorTick = NowTick;
				m_AnchorRunning = true;
			}
		}
		else
		{
			m_AnchorPositionMs = SmtcPositionMs;
			m_AnchorTick = NowTick;
			m_AnchorRunning = false;
		}
	}

	m_LastMediaPlaying = MediaPlaying;
	m_LastSmtcPositionMs = SmtcPositionMs;
}

bool CLyrics::GetCurrentLine(char *pBuf, size_t BufSize, int64_t PositionMs) const
{
	SLineState State;
	if(!GetCurrentLineState(State, PositionMs) || BufSize == 0)
		return false;
	str_copy(pBuf, State.m_aText, BufSize);
	return pBuf[0] != '\0';
}

bool CLyrics::GetNextLine(char *pBuf, size_t BufSize, int64_t PositionMs) const
{
	SLineState State;
	if(!GetNextLineState(State, PositionMs) || BufSize == 0)
		return false;
	str_copy(pBuf, State.m_aText, BufSize);
	return pBuf[0] != '\0';
}

bool CLyrics::GetCurrentLineState(SLineState &State, int64_t PositionMs) const
{
	State = SLineState{};
	if(m_State != EState::READY)
		return false;

	PositionMs = GetDisplayPositionMs(PositionMs) + g_Config.m_QmSmtcLyricsOffsetMs;

	if(m_IsSynced && !m_Lines.empty())
	{
		auto It = std::upper_bound(m_Lines.begin(), m_Lines.end(), PositionMs, [](int64_t Time, const CLyricLine &Line) {
			return Time < Line.m_TimeMs;
		});
		if(It == m_Lines.begin())
			return false;
		--It;
		if(It->m_Text.empty())
			return false;
		FillLineState(State, *It, (int)std::distance(m_Lines.begin(), It), PositionMs, true);
		return State.m_aText[0] != '\0';
	}

	if(!m_PlainLine.empty())
	{
		str_copy(State.m_aText, m_PlainLine.c_str(), sizeof(State.m_aText));
		str_copy(State.m_aVisibleText, State.m_aText, sizeof(State.m_aVisibleText));
		State.m_LineIndex = 0;
		return true;
	}

	return false;
}

bool CLyrics::GetNextLineState(SLineState &State, int64_t PositionMs) const
{
	State = SLineState{};
	if(m_State != EState::READY || !m_IsSynced || m_Lines.empty())
		return false;

	PositionMs = GetDisplayPositionMs(PositionMs) + g_Config.m_QmSmtcLyricsOffsetMs;
	auto It = std::upper_bound(m_Lines.begin(), m_Lines.end(), PositionMs, [](int64_t Time, const CLyricLine &Line) {
		return Time < Line.m_TimeMs;
	});
	if(It == m_Lines.begin())
	{
		if(It->m_Text.empty())
			return false;
		FillLineState(State, *It, 0, PositionMs, false);
		return State.m_aText[0] != '\0';
	}
	if(It == m_Lines.end())
		return false;
	if(It->m_Text.empty())
		return false;
	FillLineState(State, *It, (int)std::distance(m_Lines.begin(), It), PositionMs, false);
	return State.m_aText[0] != '\0';
}
