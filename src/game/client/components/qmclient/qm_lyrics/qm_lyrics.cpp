// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_lyrics.h"

#include "qm_lyrics_cache.h"
#include "qm_lyrics_match.h"
#include "qm_lyrics_media_identity.h"
#include "qm_lyrics_parser_lrc.h"
#include "qm_lyrics_parser_ttml.h"
#include "qm_lyrics_render.h"
#include "qm_lyrics_song_search_map.h"
#include "qm_lyrics_source_amll_ttml_db.h"
#include "qm_lyrics_source_apple_music.h"
#include "qm_lyrics_source_kugou.h"
#include "qm_lyrics_source_local_files.h"
#include "qm_lyrics_source_lrclib.h"
#include "qm_lyrics_source_lyricify_cn.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/hud_editor.h>
#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/components/system_media_controls.h>
#include <game/client/gameclient.h>
#include <game/client/ui_rect.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace
{

	constexpr float HUD_WIDTH = 480.0f;
	constexpr float HUD_PADDING_X = 8.0f;
	constexpr float HUD_PADDING_Y = 6.0f;
	constexpr int CACHE_MAX_ENTRIES = 1000;
	constexpr int CONCURRENT_SEARCH_GRACE_MS = 200;

	enum class ELyricsSourceIndex
	{
		QQ = 0,
		KUGOU,
		NETEASE,
		LRCLIB,
		AMLL_TTML_DB,
		APPLE_MUSIC,
		LOCAL_MUSIC_FILE,
		LOCAL_LRC,
		LOCAL_ESLRC,
		LOCAL_TTML,
	};

	QmLyrics::SMatchQuery BuildQueryFromMedia(const CSystemMediaControls::SState &State)
	{
		QmLyrics::SMatchQuery Q;
		Q.m_Title = State.m_aTitle;
		Q.m_Artist = State.m_aArtist;
		Q.m_Album = State.m_aAlbum;
		Q.m_PlayerId = State.m_aSourceAppId;
		Q.m_NeteaseSongId = State.m_aNeteaseSongId;
		Q.m_QqMusicSongId = State.m_aQqMusicSongId;
		Q.m_LinkedFileName = State.m_aLinkedFileName;
		Q.m_DurationSec = State.m_DurationMs > 0 ? (int)(State.m_DurationMs / 1000) : 0;
		return Q;
	}

	bool HasUsefulQuery(const QmLyrics::SMatchQuery &Query)
	{
		return !Query.m_Title.empty() || !Query.m_Artist.empty() || !Query.m_LinkedFileName.empty();
	}

	bool IsLocalLyricsSourceId(std::string_view SourceId)
	{
		return SourceId.size() >= 6 && SourceId.substr(0, 6) == "local-";
	}

	int64_t FormatDurationMsForPlain(const QmLyrics::SMatchCandidate &Metadata)
	{
		return Metadata.m_DurationSec > 0 ? (int64_t)Metadata.m_DurationSec * 1000 : 0;
	}

	bool ParsePlainLyrics(std::string_view Text, const QmLyrics::SMatchCandidate &Metadata, QmLyrics::SLyricsTrack *pOut)
	{
		if(pOut == nullptr || Text.empty())
			return false;

		std::vector<std::string_view> vLines;
		const char *p = Text.data();
		const char *pEnd = p + Text.size();
		while(p < pEnd)
		{
			const char *pLineStart = p;
			while(p < pEnd && *p != '\n')
				++p;
			std::string_view Line(pLineStart, (size_t)(p - pLineStart));
			if(p < pEnd)
				++p;
			while(!Line.empty() && (Line.back() == '\r' || Line.back() == ' ' || Line.back() == '\t'))
				Line.remove_suffix(1);
			while(!Line.empty() && (Line.front() == ' ' || Line.front() == '\t'))
				Line.remove_prefix(1);
			if(!Line.empty())
				vLines.push_back(Line);
		}
		if(vLines.empty())
			return false;

		pOut->m_Format = QmLyrics::EFormat::PLAIN;
		pOut->m_Title = Metadata.m_Title.empty() ? std::optional<std::string>() : std::optional<std::string>(Metadata.m_Title);
		pOut->m_Artist = Metadata.m_Artist.empty() ? std::optional<std::string>() : std::optional<std::string>(Metadata.m_Artist);
		pOut->m_Album = Metadata.m_Album.empty() ? std::optional<std::string>() : std::optional<std::string>(Metadata.m_Album);
		pOut->m_OffsetMs = 0;
		pOut->m_vLines.clear();
		pOut->m_vLines.reserve(vLines.size());

		const int64_t DurationMs = FormatDurationMsForPlain(Metadata);
		const int64_t StepMs = DurationMs > 0 ? std::max<int64_t>(1000, DurationMs / (int64_t)vLines.size()) : 5000;
		for(size_t i = 0; i < vLines.size(); ++i)
		{
			QmLyrics::SLyricsLine Line;
			Line.m_StartMs = (int64_t)i * StepMs;
			Line.m_EndMs = (int64_t)(i + 1) * StepMs;
			Line.m_RawText.assign(vLines[i]);
			pOut->m_vLines.push_back(std::move(Line));
		}
		return true;
	}

	bool ParseCandidateTrack(const QmLyrics::SSourceCandidate &Candidate, QmLyrics::SLyricsTrack *pOut, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(Candidate.m_RawText.empty())
			return false;

		bool Parsed = false;
		switch(Candidate.m_FormatHint)
		{
		case QmLyrics::EFormat::PLAIN:
			Parsed = ParsePlainLyrics(Candidate.m_RawText, Candidate.m_Metadata, pOut);
			break;
		case QmLyrics::EFormat::TTML:
			Parsed = QmLyrics::ParseTtml(Candidate.m_RawText.c_str(), Candidate.m_RawText.size(), pOut, pErr, ErrSize);
			break;
		case QmLyrics::EFormat::KRC:
			Parsed = QmLyrics::ParseKrcText(Candidate.m_RawText.c_str(), Candidate.m_RawText.size(), pOut, pErr, ErrSize);
			break;
		case QmLyrics::EFormat::QRC:
			Parsed = QmLyrics::ParseQqMusicQrcText(Candidate.m_RawText.c_str(), Candidate.m_RawText.size(), pOut, pErr, ErrSize);
			break;
		case QmLyrics::EFormat::LRC_STANDARD:
		case QmLyrics::EFormat::LRC_ENHANCED:
		case QmLyrics::EFormat::ESLRC:
		default:
			Parsed = QmLyrics::ParseLrc(Candidate.m_RawText.c_str(), Candidate.m_RawText.size(), pOut, pErr, ErrSize);
			break;
		}

		if(!Parsed)
			return false;
		if(!pOut->m_Title.has_value() && !Candidate.m_Metadata.m_Title.empty())
			pOut->m_Title = Candidate.m_Metadata.m_Title;
		if(!pOut->m_Artist.has_value() && !Candidate.m_Metadata.m_Artist.empty())
			pOut->m_Artist = Candidate.m_Metadata.m_Artist;
		if(!pOut->m_Album.has_value() && !Candidate.m_Metadata.m_Album.empty())
			pOut->m_Album = Candidate.m_Metadata.m_Album;
		auto MergeTimedSubLines = [&Candidate](QmLyrics::SLyricsTrack *pTrack, const std::string &Text, bool Translation) {
			if(pTrack == nullptr || Text.empty())
				return;
			QmLyrics::SLyricsTrack SubTrack;
			char aSubErr[64];
			bool ParsedSub = false;
			if(Candidate.m_FormatHint == QmLyrics::EFormat::QRC)
				ParsedSub = QmLyrics::ParseQqMusicQrcText(Text.c_str(), Text.size(), &SubTrack, aSubErr, sizeof(aSubErr));
			if(!ParsedSub)
				ParsedSub = QmLyrics::ParseLrc(Text.c_str(), Text.size(), &SubTrack, aSubErr, sizeof(aSubErr));
			if(!ParsedSub)
				return;
			for(QmLyrics::SLyricsLine &Line : pTrack->m_vLines)
			{
				const QmLyrics::SLyricsLine *pBest = nullptr;
				int64_t BestDelta = 250;
				for(const QmLyrics::SLyricsLine &SubLine : SubTrack.m_vLines)
				{
					const int64_t Delta = std::llabs(SubLine.m_StartMs - Line.m_StartMs);
					if(Delta < BestDelta)
					{
						BestDelta = Delta;
						pBest = &SubLine;
					}
				}
				if(pBest == nullptr || pBest->m_RawText.empty())
					continue;
				if(Translation)
					Line.m_Translation = pBest->m_RawText;
				else
					Line.m_Transliteration = pBest->m_RawText;
			}
		};
		MergeTimedSubLines(pOut, Candidate.m_TranslationText, true);
		MergeTimedSubLines(pOut, Candidate.m_TransliterationText, false);
		return !pOut->m_vLines.empty();
	}

	QmLyrics::SSourceCandidate CandidateFromPayload(const QmLyrics::SCachePayload &Payload)
	{
		QmLyrics::SSourceCandidate Candidate;
		Candidate.m_RawText = Payload.m_RawText;
		Candidate.m_TranslationText = Payload.m_TranslationText;
		Candidate.m_TransliterationText = Payload.m_TransliterationText;
		Candidate.m_FormatHint = Payload.m_Format;
		Candidate.m_Metadata = Payload.m_Metadata;
		Candidate.m_SourceId = Payload.m_Source;
		Candidate.m_SourceScore = 1.0f;
		return Candidate;
	}

	QmLyrics::SCachePayload PayloadFromCandidate(const QmLyrics::SSourceCandidate &Candidate, const char *pSourceId)
	{
		QmLyrics::SCachePayload Payload;
		Payload.m_RawText = Candidate.m_RawText;
		Payload.m_TranslationText = Candidate.m_TranslationText;
		Payload.m_TransliterationText = Candidate.m_TransliterationText;
		Payload.m_Format = Candidate.m_FormatHint;
		Payload.m_Metadata = Candidate.m_Metadata;
		Payload.m_Source = Candidate.m_SourceId.empty() && pSourceId != nullptr ? pSourceId : Candidate.m_SourceId;
		return Payload;
	}

	std::optional<int> SourceIndexForProviderId(std::string_view ProviderId)
	{
		const std::string Canonical = QmLyrics::CanonicalLyricsProviderId(ProviderId);
		if(Canonical == "qq")
			return (int)ELyricsSourceIndex::QQ;
		if(Canonical == "kugou")
			return (int)ELyricsSourceIndex::KUGOU;
		if(Canonical == "netease")
			return (int)ELyricsSourceIndex::NETEASE;
		if(Canonical == "lrclib")
			return (int)ELyricsSourceIndex::LRCLIB;
		if(Canonical == "amll-ttml-db")
			return (int)ELyricsSourceIndex::AMLL_TTML_DB;
		if(Canonical == "apple-music")
			return (int)ELyricsSourceIndex::APPLE_MUSIC;
		if(Canonical == "local-music-file")
			return (int)ELyricsSourceIndex::LOCAL_MUSIC_FILE;
		if(Canonical == "local-lrc")
			return (int)ELyricsSourceIndex::LOCAL_LRC;
		if(Canonical == "local-eslrc")
			return (int)ELyricsSourceIndex::LOCAL_ESLRC;
		if(Canonical == "local-ttml")
			return (int)ELyricsSourceIndex::LOCAL_TTML;
		return std::nullopt;
	}

	bool IsProviderListSeparator(char C)
	{
		return C == '|' || C == ',' || C == ';' || C == '\n' || C == '\r';
	}

	std::vector<std::string> SplitProviderList(std::string_view Text)
	{
		std::vector<std::string> vItems;
		size_t Start = 0;
		for(size_t i = 0; i <= Text.size(); ++i)
		{
			if(i != Text.size() && !IsProviderListSeparator(Text[i]))
				continue;
			std::string Item(Text.substr(Start, i - Start));
			const char *pTrimmed = str_utf8_skip_whitespaces(Item.c_str());
			if(pTrimmed != Item.c_str())
				Item.erase(0, pTrimmed - Item.c_str());
			str_utf8_trim_right(Item.data());
			Item.resize(str_length(Item.c_str()));
			if(!Item.empty())
				vItems.push_back(std::move(Item));
			Start = i + 1;
		}
		return vItems;
	}

	void AppendUniqueSourceIndex(std::vector<int> *pvOrder, int SourceIndex)
	{
		if(std::find(pvOrder->begin(), pvOrder->end(), SourceIndex) == pvOrder->end())
			pvOrder->push_back(SourceIndex);
	}

	void AppendDefaultSourceOrder(std::vector<int> *pvOrder)
	{
		pvOrder->push_back((int)ELyricsSourceIndex::LRCLIB);
		pvOrder->push_back((int)ELyricsSourceIndex::QQ);
		pvOrder->push_back((int)ELyricsSourceIndex::KUGOU);
		pvOrder->push_back((int)ELyricsSourceIndex::NETEASE);
		pvOrder->push_back((int)ELyricsSourceIndex::AMLL_TTML_DB);
		pvOrder->push_back((int)ELyricsSourceIndex::LOCAL_MUSIC_FILE);
		pvOrder->push_back((int)ELyricsSourceIndex::LOCAL_LRC);
		pvOrder->push_back((int)ELyricsSourceIndex::LOCAL_ESLRC);
		pvOrder->push_back((int)ELyricsSourceIndex::LOCAL_TTML);
		pvOrder->push_back((int)ELyricsSourceIndex::APPLE_MUSIC);
	}

	std::vector<int> BuildSourceOrder()
	{
		std::vector<int> vOrder;
		switch(g_Config.m_QmLyricsSource)
		{
		case 1:
			vOrder.push_back((int)ELyricsSourceIndex::LRCLIB);
			break;
		case 2:
			vOrder.push_back((int)ELyricsSourceIndex::KUGOU);
			break;
		case 3:
			vOrder.push_back((int)ELyricsSourceIndex::QQ);
			break;
		case 4:
			vOrder.push_back((int)ELyricsSourceIndex::NETEASE);
			break;
		case 5:
			vOrder.push_back((int)ELyricsSourceIndex::AMLL_TTML_DB);
			break;
		case 6:
			vOrder.push_back((int)ELyricsSourceIndex::APPLE_MUSIC);
			break;
		case 7:
			vOrder.push_back((int)ELyricsSourceIndex::LOCAL_MUSIC_FILE);
			break;
		case 8:
			vOrder.push_back((int)ELyricsSourceIndex::LOCAL_LRC);
			break;
		case 9:
			vOrder.push_back((int)ELyricsSourceIndex::LOCAL_ESLRC);
			break;
		case 10:
			vOrder.push_back((int)ELyricsSourceIndex::LOCAL_TTML);
			break;
		default:
			for(const std::string &Provider : SplitProviderList(g_Config.m_QmLyricsSourceOrder))
			{
				if(const std::optional<int> SourceIndex = SourceIndexForProviderId(Provider))
					AppendUniqueSourceIndex(&vOrder, *SourceIndex);
			}
			if(vOrder.empty())
				AppendDefaultSourceOrder(&vOrder);
			break;
		}
		return vOrder;
	}

	std::string CacheKeyForSource(std::string_view TrackKey, std::string_view SourceId)
	{
		if(SourceId.empty())
			return std::string(TrackKey);
		std::string Out(TrackKey);
		Out.append("|provider:");
		Out.append(SourceId);
		return Out;
	}

	bool ProviderListContains(std::string_view List, std::string_view SourceId)
	{
		const std::string CanonicalSource = QmLyrics::CanonicalLyricsProviderId(SourceId);
		if(CanonicalSource.empty())
			return false;
		for(const std::string &Provider : SplitProviderList(List))
		{
			if(QmLyrics::CanonicalLyricsProviderId(Provider) == CanonicalSource)
				return true;
		}
		return false;
	}

	int MatchThresholdForSource(std::string_view SourceId)
	{
		const std::string CanonicalSource = QmLyrics::CanonicalLyricsProviderId(SourceId);
		if(CanonicalSource.empty())
			return std::clamp(g_Config.m_QmLyricsMatchThreshold, 0, 100);
		for(const std::string &Rule : SplitProviderList(g_Config.m_QmLyricsProviderThresholds))
		{
			const size_t Split = Rule.find_first_of("=:");
			if(Split == std::string::npos)
				continue;
			const std::string Provider = Rule.substr(0, Split);
			if(QmLyrics::CanonicalLyricsProviderId(Provider) != CanonicalSource)
				continue;
			const std::string ValueText = Rule.substr(Split + 1);
			char *pEnd = nullptr;
			const long Value = std::strtol(ValueText.c_str(), &pEnd, 10);
			if(pEnd != ValueText.c_str())
				return std::clamp((int)Value, 0, 100);
		}
		return std::clamp(g_Config.m_QmLyricsMatchThreshold, 0, 100);
	}

	bool IgnoreCacheForSource(std::string_view SourceId)
	{
		return ProviderListContains(g_Config.m_QmLyricsIgnoreCacheProviders, SourceId);
	}

} // namespace

struct CQmLyrics::SImpl
{
	enum class EState
	{
		IDLE,
		FETCHING,
		READY,
		NO_RESULT,
	};

	EState m_State = EState::IDLE;
	QmLyrics::SMediaIdentity m_LastMediaIdentity;
	QmLyrics::SMediaIdentity m_ClockMediaIdentity;
	std::string m_LastTrackKey;
	std::string m_LastLyricsHttpProxy;
	QmLyrics::SMatchQuery m_PendingQuery;
	QmLyrics::CLyricsSourceLrclib *m_pLrclibSource = nullptr;
	std::vector<std::unique_ptr<QmLyrics::IQmLyricsSource>> m_vSources;
	std::vector<int> m_vSourceOrder;
	std::vector<QmLyrics::SSourceCandidate> m_vPendingCandidates;
	QmLyrics::SLyricsTrack m_Track;
	QmLyrics::CClockInterpolator m_Clock;
	QmLyrics::CCacheIndex m_Cache;
	int m_ActiveLineIndex = -1;
	int m_LastAnimatedLineIndex = -1;
	int m_PreviousAnimatedLineIndex = -1;
	int64_t m_ActiveLineChangedTick = 0;
	int64_t m_FirstCandidateTick = 0;
	int m_SearchGeneration = 0;
	int m_PendingSources = 0;
	int m_NextSourceOrderIndex = 0;
	int m_LastLyricsHttpTimeoutMs = 0;
	bool m_CacheLoaded = false;
	bool m_CacheDirty = false;
	bool m_LastMediaPlaying = false;
	bool m_SequentialSearch = true;
	float m_LastMatchScore = 0.0f;
	char m_aStatus[128] = {};
};

CQmLyrics::CQmLyrics() :
	m_pImpl(std::make_unique<SImpl>())
{
}

CQmLyrics::~CQmLyrics() = default;

void CQmLyrics::OnInit()
{
	IHttp *pHttp = Kernel()->RequestInterface<IHttp>();
	m_pImpl->m_vSources.clear();
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceQqMusic>(pHttp, g_Config.m_QmLyricsHttpTimeoutMs));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceKugou>(pHttp, g_Config.m_QmLyricsHttpTimeoutMs));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceNetease>(pHttp, g_Config.m_QmLyricsHttpTimeoutMs));
	auto pLrclibSource = std::make_unique<QmLyrics::CLyricsSourceLrclib>(pHttp, g_Config.m_QmLyricsHttpTimeoutMs, g_Config.m_QmLyricsLrclibBase, g_Config.m_QmLyricsHttpProxy);
	m_pImpl->m_pLrclibSource = pLrclibSource.get();
	m_pImpl->m_vSources.emplace_back(std::move(pLrclibSource));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceAmllTtmlDb>(pHttp, Storage(), Kernel()->RequestInterface<IEngine>(), g_Config.m_QmLyricsHttpTimeoutMs, g_Config.m_QmLyricsAmllTtmlDbBase));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceAppleMusic>(pHttp, g_Config.m_QmLyricsHttpTimeoutMs, g_Config.m_QmLyricsAppleMusicMediaUserToken));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceLocalMusicFile>(Storage(), Kernel()->RequestInterface<IEngine>(), g_Config.m_QmLyricsLocalMediaFolders));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceLocalLyricsFile>(Storage(), QmLyrics::ELocalLyricsFileKind::LRC));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceLocalLyricsFile>(Storage(), QmLyrics::ELocalLyricsFileKind::ESLRC));
	m_pImpl->m_vSources.emplace_back(std::make_unique<QmLyrics::CLyricsSourceLocalLyricsFile>(Storage(), QmLyrics::ELocalLyricsFileKind::TTML));
	m_pImpl->m_LastLyricsHttpTimeoutMs = g_Config.m_QmLyricsHttpTimeoutMs;
	m_pImpl->m_LastLyricsHttpProxy = g_Config.m_QmLyricsHttpProxy;
	if(g_Config.m_QmLyricsCacheEnable)
	{
		QmLyrics::LoadCacheIndex(Storage(), &m_pImpl->m_Cache);
		m_pImpl->m_CacheLoaded = true;
	}
	(void)g_Config.m_QmLyricsShowTransliteration;
	(void)g_Config.m_QmLyricsHideNoLyrics;
}

void CQmLyrics::OnShutdown()
{
	for(std::unique_ptr<QmLyrics::IQmLyricsSource> &pSource : m_pImpl->m_vSources)
	{
		if(pSource)
			pSource->Cancel();
	}
	m_pImpl->m_vSources.clear();
	m_pImpl->m_pLrclibSource = nullptr;
	if(m_pImpl->m_CacheLoaded && m_pImpl->m_CacheDirty && QmLyrics::SaveCacheIndex(Storage(), m_pImpl->m_Cache))
		m_pImpl->m_CacheDirty = false;
}

void CQmLyrics::OnReset()
{
	// 歌词状态和服务器/地图无关；跨 reset 保留请求与缓存，只重置当前行动画。
	m_pImpl->m_ActiveLineIndex = -1;
	m_pImpl->m_LastAnimatedLineIndex = -1;
	m_pImpl->m_PreviousAnimatedLineIndex = -1;
}

namespace
{

	void CancelAllSources(CQmLyrics::SImpl *pImpl)
	{
		++pImpl->m_SearchGeneration;
		for(std::unique_ptr<QmLyrics::IQmLyricsSource> &pSource : pImpl->m_vSources)
		{
			if(pSource)
				pSource->Cancel();
		}
		pImpl->m_PendingSources = 0;
		pImpl->m_vSourceOrder.clear();
		pImpl->m_NextSourceOrderIndex = 0;
		pImpl->m_vPendingCandidates.clear();
		pImpl->m_FirstCandidateTick = 0;
	}

	void ResetPlaybackClock(CQmLyrics::SImpl *pImpl)
	{
		pImpl->m_ClockMediaIdentity.m_Valid = false;
		pImpl->m_LastMediaPlaying = false;
		pImpl->m_Clock.Reset();
	}

	void ResetPlaybackState(CQmLyrics::SImpl *pImpl)
	{
		pImpl->m_LastMediaIdentity.m_Valid = false;
		pImpl->m_LastTrackKey.clear();
		pImpl->m_Track.m_vLines.clear();
		pImpl->m_ActiveLineIndex = -1;
		pImpl->m_LastAnimatedLineIndex = -1;
		pImpl->m_PreviousAnimatedLineIndex = -1;
		pImpl->m_FirstCandidateTick = 0;
		ResetPlaybackClock(pImpl);
	}

	void UpdatePlaybackClock(CQmLyrics::SImpl *pImpl, const CSystemMediaControls::SState &MediaState)
	{
		QmLyrics::SPlaybackSnapshot PlaybackSnapshot;
		PlaybackSnapshot.m_PositionMs = MediaState.m_PositionMs;
		PlaybackSnapshot.m_PositionUpdatedTick = MediaState.m_PositionUpdatedTick;
		PlaybackSnapshot.m_TimelineGeneration = MediaState.m_TimelineGeneration;
		PlaybackSnapshot.m_PlaybackRate = std::max(0.0, MediaState.m_PlaybackRate);
		PlaybackSnapshot.m_Playing = MediaState.m_Playing;
		PlaybackSnapshot.m_IdentityChanged = !QmLyrics::MediaIdentityEquals(pImpl->m_ClockMediaIdentity, MediaState);
		pImpl->m_Clock.SetDriftCorrectMs(g_Config.m_QmLyricsDriftCorrectMs);
		pImpl->m_Clock.SetOffsetMs(QmLyrics::EffectivePlaybackOffsetMs(g_Config.m_QmLyricsOffsetMs, pImpl->m_Track.m_OffsetMs));
		pImpl->m_Clock.Update(PlaybackSnapshot, time_get(), time_freq());
		QmLyrics::SetMediaIdentity(&pImpl->m_ClockMediaIdentity, MediaState);
		pImpl->m_LastMediaPlaying = MediaState.m_Playing;
	}

	void SetStateStatus(CQmLyrics::SImpl *pImpl, const char *pStatus)
	{
		str_copy(pImpl->m_aStatus, pStatus != nullptr ? pStatus : "", sizeof(pImpl->m_aStatus));
	}

	bool ApplyCandidate(CQmLyrics::SImpl *pImpl, const QmLyrics::SSourceCandidate &Candidate, float Score)
	{
		char aErr[128];
		QmLyrics::SLyricsTrack Track;
		if(!ParseCandidateTrack(Candidate, &Track, aErr, sizeof(aErr)))
			return false;
		pImpl->m_Track = std::move(Track);
		pImpl->m_State = CQmLyrics::SImpl::EState::READY;
		pImpl->m_ActiveLineIndex = -1;
		pImpl->m_LastAnimatedLineIndex = -1;
		pImpl->m_PreviousAnimatedLineIndex = -1;
		pImpl->m_ActiveLineChangedTick = time_get();
		pImpl->m_LastMatchScore = Score;
		SetStateStatus(pImpl, "");
		return true;
	}

	void StoreCandidateInCache(CQmLyrics::SImpl *pImpl, IStorage *pStorage, const std::string &TrackKey, const QmLyrics::SSourceCandidate &Candidate, float Score)
	{
		if(!g_Config.m_QmLyricsCacheEnable || TrackKey.empty() || IsLocalLyricsSourceId(Candidate.m_SourceId))
			return;
		const std::string SourceId = Candidate.m_SourceId.empty() ? "unknown" : Candidate.m_SourceId;
		const std::string EntryKey = TrackKey;
		const int64_t NowSec = time_timestamp();
		QmLyrics::SCacheEntry Entry;
		Entry.m_Key = EntryKey;
		const std::string &Title = pImpl->m_PendingQuery.m_Title.empty() ? Candidate.m_Metadata.m_Title : pImpl->m_PendingQuery.m_Title;
		const std::string &Artist = pImpl->m_PendingQuery.m_Artist.empty() ? Candidate.m_Metadata.m_Artist : pImpl->m_PendingQuery.m_Artist;
		Entry.m_FileName = QmLyrics::ChooseCachePayloadFileName(pImpl->m_Cache, Title, Artist, EntryKey, EntryKey);
		Entry.m_Source = SourceId;
		Entry.m_Score = Score;
		Entry.m_StoredAt = NowSec;
		Entry.m_LastUsedAt = NowSec;
		if(!QmLyrics::CommitCacheEntry(
			pStorage,
			&pImpl->m_Cache,
			Entry,
			PayloadFromCandidate(Candidate, Entry.m_Source.c_str()),
			CACHE_MAX_ENTRIES))
			return;
		pImpl->m_CacheLoaded = true;
		pImpl->m_CacheDirty = false;
	}

	bool TryApplyBestCandidate(CQmLyrics::SImpl *pImpl, IStorage *pStorage, const std::string &TrackKey)
	{
		std::vector<QmLyrics::SCandidateApplyRank> vRanked;
		vRanked.reserve(pImpl->m_vPendingCandidates.size());
		for(size_t i = 0; i < pImpl->m_vPendingCandidates.size(); ++i)
		{
			const QmLyrics::SSourceCandidate &Candidate = pImpl->m_vPendingCandidates[i];
			if(Candidate.m_RawText.empty())
				continue;
			const float Score = QmLyrics::Score(pImpl->m_PendingQuery, Candidate.m_Metadata);
			const float Threshold = (float)MatchThresholdForSource(Candidate.m_SourceId);
			if(Score < Threshold)
				continue;
			int SourceOrder = std::numeric_limits<int>::max();
			for(size_t Order = 0; Order < pImpl->m_vSourceOrder.size(); ++Order)
			{
				const int SourceIndex = pImpl->m_vSourceOrder[Order];
				if(SourceIndex >= 0 && SourceIndex < (int)pImpl->m_vSources.size() && pImpl->m_vSources[SourceIndex] &&
					std::string_view(pImpl->m_vSources[SourceIndex]->Id()) == Candidate.m_SourceId)
				{
					SourceOrder = (int)Order;
					break;
				}
			}
			vRanked.push_back({i, Score, Candidate.m_SourceScore, SourceOrder});
		}

		QmLyrics::SortCandidateApplyRanks(&vRanked);

		for(const QmLyrics::SCandidateApplyRank &Ranked : vRanked)
		{
			const QmLyrics::SSourceCandidate &Candidate = pImpl->m_vPendingCandidates[Ranked.m_Index];
			if(ApplyCandidate(pImpl, Candidate, Ranked.m_Score))
			{
				StoreCandidateInCache(pImpl, pStorage, TrackKey, Candidate, Ranked.m_Score);
				pImpl->m_vPendingCandidates.clear();
				return true;
			}
		}
		return false;
	}

	bool HasPotentialCandidate(const CQmLyrics::SImpl *pImpl)
	{
		for(const QmLyrics::SSourceCandidate &Candidate : pImpl->m_vPendingCandidates)
		{
			if(!Candidate.m_RawText.empty() &&
				QmLyrics::Score(pImpl->m_PendingQuery, Candidate.m_Metadata) >= (float)MatchThresholdForSource(Candidate.m_SourceId))
				return true;
		}
		return false;
	}

	void SelectBestCandidate(CQmLyrics::SImpl *pImpl, IStorage *pStorage, const std::string &TrackKey)
	{
		if(TryApplyBestCandidate(pImpl, pStorage, TrackKey))
			return;
		pImpl->m_State = CQmLyrics::SImpl::EState::NO_RESULT;
		SetStateStatus(pImpl, Localize("Lyrics: no lyrics found"));
		pImpl->m_vPendingCandidates.clear();
	}

	const char *SourceIdForOrderIndex(CQmLyrics::SImpl *pImpl, int SourceIndex)
	{
		if(SourceIndex < 0 || SourceIndex >= (int)pImpl->m_vSources.size() || !pImpl->m_vSources[SourceIndex])
			return "";
		return pImpl->m_vSources[SourceIndex]->Id();
	}

	bool TryLoadCacheEntry(CQmLyrics::SImpl *pImpl, IStorage *pStorage, const std::string &EntryKey, const std::string &TrackKey, std::string_view RequiredSourceId)
	{
		const QmLyrics::SCacheEntry *pEntry = pImpl->m_Cache.Lookup(EntryKey, time_timestamp());
		if(pEntry == nullptr)
			return false;
		pImpl->m_CacheDirty = true;
		if(!RequiredSourceId.empty() && std::string_view(pEntry->m_Source) != RequiredSourceId)
			return false;
		if(IgnoreCacheForSource(pEntry->m_Source))
			return false;

		QmLyrics::SCachePayload Payload;
		if(!QmLyrics::LoadCachePayload(pStorage, pEntry->m_FileName.c_str(), &Payload))
		{
			std::string FileName = pEntry->m_FileName;
			pImpl->m_Cache.Remove(EntryKey, &FileName);
			pImpl->m_CacheDirty = true;
			QmLyrics::RemoveCachePayload(pStorage, FileName.c_str());
			if(QmLyrics::SaveCacheIndex(pStorage, pImpl->m_Cache))
				pImpl->m_CacheDirty = false;
			return false;
		}
		const QmLyrics::SSourceCandidate Candidate = CandidateFromPayload(Payload);
		const float Score = QmLyrics::Score(pImpl->m_PendingQuery, Candidate.m_Metadata);
		if(Score < (float)MatchThresholdForSource(Candidate.m_SourceId.empty() ? pEntry->m_Source : Candidate.m_SourceId))
			return false;
		if(!ApplyCandidate(pImpl, Candidate, Score))
			return false;
		if((EntryKey != TrackKey || std::string_view(pEntry->m_FileName).ends_with(".json")) &&
			QmLyrics::MigrateLegacyCacheEntry(
				pStorage,
				&pImpl->m_Cache,
				EntryKey,
				TrackKey,
				pImpl->m_PendingQuery.m_Title,
				pImpl->m_PendingQuery.m_Artist,
				CACHE_MAX_ENTRIES))
		{
			pImpl->m_CacheDirty = false;
		}
		return true;
	}

	bool TryLoadCache(CQmLyrics::SImpl *pImpl, IStorage *pStorage, const std::string &TrackKey, std::string_view RequiredSourceId, const std::vector<int> &vSourceOrder)
	{
		if(!g_Config.m_QmLyricsCacheEnable || TrackKey.empty())
			return false;
		if(!pImpl->m_CacheLoaded)
		{
			QmLyrics::LoadCacheIndex(pStorage, &pImpl->m_Cache);
			pImpl->m_CacheLoaded = true;
		}
		const int64_t NowSec = time_timestamp();
		const std::vector<std::string> vExpired = pImpl->m_Cache.EvictExpired(g_Config.m_QmLyricsCacheTtlDays, NowSec);
		for(const std::string &FileName : vExpired)
			QmLyrics::RemoveCachePayload(pStorage, FileName.c_str());
		if(!vExpired.empty())
		{
			pImpl->m_CacheDirty = true;
			if(QmLyrics::SaveCacheIndex(pStorage, pImpl->m_Cache))
				pImpl->m_CacheDirty = false;
		}

		// New cache layout keeps one selected winner per track, independent of provider.
		if(TryLoadCacheEntry(pImpl, pStorage, TrackKey, TrackKey, RequiredSourceId))
			return true;

		if(!RequiredSourceId.empty())
		{
			return !IgnoreCacheForSource(RequiredSourceId) && TryLoadCacheEntry(pImpl, pStorage, CacheKeyForSource(TrackKey, RequiredSourceId), TrackKey, RequiredSourceId);
		}

		for(int SourceIndex : vSourceOrder)
		{
			const char *pSourceId = SourceIdForOrderIndex(pImpl, SourceIndex);
			if(pSourceId[0] == '\0' || IgnoreCacheForSource(pSourceId))
				continue;
			if(TryLoadCacheEntry(pImpl, pStorage, CacheKeyForSource(TrackKey, pSourceId), TrackKey, pSourceId))
				return true;
		}
		return false;
	}

} // namespace

void CQmLyrics::TickStateMachine()
{
	const int64_t PerfStartNs = QmPerfEnabled() ? time_get_nanoseconds().count() : 0;
	if(!g_Config.m_QmLyrics)
	{
		if(m_pImpl->m_State == SImpl::EState::FETCHING)
		{
			CancelAllSources(m_pImpl.get());
			m_pImpl->m_LastMediaIdentity.m_Valid = false;
			m_pImpl->m_State = SImpl::EState::IDLE;
		}
		CSystemMediaControls::SState MediaState{};
		if(g_Config.m_QmSmtcEnable && GameClient()->m_SystemMediaControls.GetStateSnapshot(MediaState))
			UpdatePlaybackClock(m_pImpl.get(), MediaState);
		else
			ResetPlaybackClock(m_pImpl.get());
		return;
	}

	if(!g_Config.m_QmSmtcEnable)
	{
		if(m_pImpl->m_State == SImpl::EState::FETCHING)
			CancelAllSources(m_pImpl.get());
		ResetPlaybackState(m_pImpl.get());
		m_pImpl->m_State = SImpl::EState::IDLE;
		return;
	}

	const bool HttpOptionsChanged = QmLyrics::LrclibHttpOptionsChanged(
		m_pImpl->m_LastLyricsHttpTimeoutMs,
		m_pImpl->m_LastLyricsHttpProxy.c_str(),
		g_Config.m_QmLyricsHttpTimeoutMs,
		g_Config.m_QmLyricsHttpProxy);
	m_pImpl->m_LastLyricsHttpTimeoutMs = g_Config.m_QmLyricsHttpTimeoutMs;
	m_pImpl->m_LastLyricsHttpProxy = g_Config.m_QmLyricsHttpProxy;
	const bool RestartSearchForHttpOptions = m_pImpl->m_State == SImpl::EState::FETCHING && HttpOptionsChanged;
	if(RestartSearchForHttpOptions)
		CancelAllSources(m_pImpl.get());

	for(std::unique_ptr<QmLyrics::IQmLyricsSource> &pSource : m_pImpl->m_vSources)
	{
		if(pSource)
			pSource->Tick();
	}

	CSystemMediaControls::SState MediaState{};
	const bool HasMedia = GameClient()->m_SystemMediaControls.GetStateSnapshot(MediaState);
	if(!HasMedia)
	{
		if(m_pImpl->m_State == SImpl::EState::FETCHING)
			CancelAllSources(m_pImpl.get());
		ResetPlaybackState(m_pImpl.get());
		m_pImpl->m_State = SImpl::EState::IDLE;
		SetStateStatus(m_pImpl.get(), Localize("Lyrics: no media playing"));
		return;
	}
	const bool MediaIdentityChanged = !QmLyrics::MediaIdentityEquals(m_pImpl->m_LastMediaIdentity, MediaState);
	UpdatePlaybackClock(m_pImpl.get(), MediaState);

	auto DispatchSource = [this](int SourceIndex, int Generation) {
		if(SourceIndex < 0 || SourceIndex >= (int)m_pImpl->m_vSources.size() || !m_pImpl->m_vSources[SourceIndex])
			return false;
		QmLyrics::IQmLyricsSource *pSource = m_pImpl->m_vSources[SourceIndex].get();
		if(pSource == m_pImpl->m_pLrclibSource)
			m_pImpl->m_pLrclibSource->UpdateHttpOptions(g_Config.m_QmLyricsHttpTimeoutMs, g_Config.m_QmLyricsHttpProxy);
		const std::string SourceId = pSource->Id();
		m_pImpl->m_PendingSources++;
		pSource->QueryAsync(
			m_pImpl->m_PendingQuery,
			[this, Generation, SourceId](std::vector<QmLyrics::SSourceCandidate> vCandidates) {
				if(Generation != m_pImpl->m_SearchGeneration)
					return;
				for(QmLyrics::SSourceCandidate &Candidate : vCandidates)
				{
					if(Candidate.m_SourceId.empty())
						Candidate.m_SourceId = SourceId;
					m_pImpl->m_vPendingCandidates.push_back(std::move(Candidate));
				}
				m_pImpl->m_PendingSources = std::max(0, m_pImpl->m_PendingSources - 1);
				if(m_pImpl->m_State != SImpl::EState::FETCHING)
					return;
				if(m_pImpl->m_SequentialSearch)
				{
					if(!TryApplyBestCandidate(m_pImpl.get(), Storage(), m_pImpl->m_LastTrackKey))
						m_pImpl->m_vPendingCandidates.clear();
				}
				else if(m_pImpl->m_FirstCandidateTick == 0 && HasPotentialCandidate(m_pImpl.get()))
				{
					m_pImpl->m_FirstCandidateTick = time_get();
				}
			},
			[this, Generation](const char *) {
				if(Generation != m_pImpl->m_SearchGeneration)
					return;
				m_pImpl->m_PendingSources = std::max(0, m_pImpl->m_PendingSources - 1);
				if(m_pImpl->m_State != SImpl::EState::FETCHING)
					return;
			});
		return true;
	};
	if(MediaIdentityChanged || RestartSearchForHttpOptions)
	{
		if(!RestartSearchForHttpOptions)
			CancelAllSources(m_pImpl.get());
		QmLyrics::SetMediaIdentity(&m_pImpl->m_LastMediaIdentity, MediaState);
		m_pImpl->m_Track.m_vLines.clear();
		m_pImpl->m_ActiveLineIndex = -1;
		m_pImpl->m_PendingQuery = BuildQueryFromMedia(MediaState);
		m_pImpl->m_State = SImpl::EState::IDLE;
		SetStateStatus(m_pImpl.get(), "");

		std::string RequiredSourceId;
		std::vector<QmLyrics::SSongSearchMapEntry> vSongSearchMap;
		if(QmLyrics::LoadSongSearchMap(Storage(), &vSongSearchMap))
		{
			if(const QmLyrics::SSongSearchMapEntry *pMapping = QmLyrics::FindSongSearchMapping(m_pImpl->m_PendingQuery, vSongSearchMap))
			{
				m_pImpl->m_PendingQuery = QmLyrics::ApplySongSearchMapping(m_pImpl->m_PendingQuery, *pMapping);
				RequiredSourceId = QmLyrics::CanonicalLyricsProviderId(pMapping->m_LyricsSearchProvider);
				if(pMapping->m_IsMarkedAsPureMusic)
				{
					const QmLyrics::SSourceCandidate Candidate = QmLyrics::BuildPureMusicCandidate(m_pImpl->m_PendingQuery);
					ApplyCandidate(m_pImpl.get(), Candidate, 100.0f);
				}
			}
		}

		m_pImpl->m_LastTrackKey = QmLyrics::BuildCacheKey(
			m_pImpl->m_PendingQuery.m_Title,
			m_pImpl->m_PendingQuery.m_Artist,
			m_pImpl->m_PendingQuery.m_Album,
			m_pImpl->m_PendingQuery.m_DurationSec);
		m_pImpl->m_SequentialSearch = g_Config.m_QmLyricsSearchType == 0;
		m_pImpl->m_vSourceOrder = BuildSourceOrder();
		if(!RequiredSourceId.empty())
		{
			if(const std::optional<int> SourceIndex = SourceIndexForProviderId(RequiredSourceId))
				m_pImpl->m_vSourceOrder = {*SourceIndex};
			else
				m_pImpl->m_vSourceOrder.clear();
		}

		if(m_pImpl->m_State == SImpl::EState::READY)
		{
			// ready from song-search-map
		}
		else if(!HasUsefulQuery(m_pImpl->m_PendingQuery))
		{
			m_pImpl->m_State = SImpl::EState::NO_RESULT;
			SetStateStatus(m_pImpl.get(), Localize("Lyrics: no media playing"));
		}
		else if(TryLoadCache(m_pImpl.get(), Storage(), m_pImpl->m_LastTrackKey, RequiredSourceId, m_pImpl->m_vSourceOrder))
		{
			// ready from cache
		}
		else if(g_Config.m_QmLyricsAutoFetch)
		{
			m_pImpl->m_State = SImpl::EState::FETCHING;
			SetStateStatus(m_pImpl.get(), Localize("Lyrics: searching..."));
			m_pImpl->m_SearchGeneration++;
			m_pImpl->m_PendingSources = 0;
			m_pImpl->m_NextSourceOrderIndex = 0;
			m_pImpl->m_FirstCandidateTick = 0;
			m_pImpl->m_vPendingCandidates.clear();
			const int Generation = m_pImpl->m_SearchGeneration;
			if(!m_pImpl->m_SequentialSearch)
			{
				for(int SourceIndex : m_pImpl->m_vSourceOrder)
					DispatchSource(SourceIndex, Generation);
				if(m_pImpl->m_PendingSources == 0)
				{
					m_pImpl->m_State = SImpl::EState::NO_RESULT;
					SetStateStatus(m_pImpl.get(), Localize("Lyrics: no lyrics found"));
				}
			}
		}
	}
	if(MediaIdentityChanged && PerfStartNs != 0)
	{
		char aExtra[128];
		str_format(aExtra, sizeof(aExtra), "pending_sources=%d source_count=%d state=%d", m_pImpl->m_PendingSources, (int)m_pImpl->m_vSourceOrder.size(), (int)m_pImpl->m_State);
		const double DurationMs = (time_get_nanoseconds().count() - PerfStartNs) / 1000000.0;
		QmPerfLogStage("perf/lyrics", "media_identity_change", DurationMs, true, Client(), nullptr, nullptr, aExtra);
	}

	if(m_pImpl->m_State == SImpl::EState::FETCHING && m_pImpl->m_SequentialSearch && m_pImpl->m_PendingSources == 0)
	{
		const int Generation = m_pImpl->m_SearchGeneration;
		bool Dispatched = false;
		while(m_pImpl->m_NextSourceOrderIndex < (int)m_pImpl->m_vSourceOrder.size())
		{
			const int SourceIndex = m_pImpl->m_vSourceOrder[m_pImpl->m_NextSourceOrderIndex++];
			if(DispatchSource(SourceIndex, Generation))
			{
				Dispatched = true;
				break;
			}
		}
		if(!Dispatched)
		{
			m_pImpl->m_State = SImpl::EState::NO_RESULT;
			SetStateStatus(m_pImpl.get(), Localize("Lyrics: no lyrics found"));
			m_pImpl->m_vPendingCandidates.clear();
		}
	}
	else if(m_pImpl->m_State == SImpl::EState::FETCHING && !m_pImpl->m_SequentialSearch &&
		QmLyrics::ShouldPublishConcurrentSearch(
			m_pImpl->m_PendingSources,
			m_pImpl->m_FirstCandidateTick,
			time_get(),
			time_freq(),
			CONCURRENT_SEARCH_GRACE_MS))
	{
		if(m_pImpl->m_PendingSources == 0)
		{
			SelectBestCandidate(m_pImpl.get(), Storage(), m_pImpl->m_LastTrackKey);
		}
		else if(TryApplyBestCandidate(m_pImpl.get(), Storage(), m_pImpl->m_LastTrackKey))
		{
			CancelAllSources(m_pImpl.get());
		}
		else
		{
			m_pImpl->m_vPendingCandidates.clear();
			m_pImpl->m_FirstCandidateTick = 0;
		}
	}

	m_pImpl->m_Clock.SetOffsetMs(QmLyrics::EffectivePlaybackOffsetMs(g_Config.m_QmLyricsOffsetMs, m_pImpl->m_Track.m_OffsetMs));
	if(m_pImpl->m_State == SImpl::EState::READY && !m_pImpl->m_Track.m_vLines.empty())
	{
		const int64_t Now = m_pImpl->m_Clock.Now(time_get(), time_freq());
		const int Active = QmLyrics::FindActiveLineIndex(m_pImpl->m_Track, Now);
		if(Active != m_pImpl->m_ActiveLineIndex)
		{
			const int PreviousActive = m_pImpl->m_ActiveLineIndex;
			const bool AnimateLineChange = PreviousActive >= 0 && Active >= 0 && std::abs(Active - PreviousActive) <= 2;
			m_pImpl->m_PreviousAnimatedLineIndex = AnimateLineChange ? PreviousActive : -1;
			m_pImpl->m_ActiveLineChangedTick = time_get();
			m_pImpl->m_LastAnimatedLineIndex = Active;
		}
		m_pImpl->m_ActiveLineIndex = Active;
	}
}

namespace
{

	ColorRGBA WithMultipliedAlpha(ColorRGBA Color, float Alpha)
	{
		Color.a *= Alpha;
		return Color;
	}

	ColorRGBA WithBrightness(ColorRGBA Color, float Brightness)
	{
		Color.r = std::min(1.0f, Color.r * Brightness);
		Color.g = std::min(1.0f, Color.g * Brightness);
		Color.b = std::min(1.0f, Color.b * Brightness);
		return Color;
	}

	ColorRGBA MixColor(const ColorRGBA &A, const ColorRGBA &B, float T)
	{
		T = std::clamp(T, 0.0f, 1.0f);
		return ColorRGBA(mix(A.r, B.r, T), mix(A.g, B.g, T), mix(A.b, B.b, T), mix(A.a, B.a, T));
	}

	const char *VisibleLineText(const QmLyrics::SLyricsLine &Line)
	{
		return Line.m_RawText.empty() ? "♪" : Line.m_RawText.c_str();
	}

	bool IntersectRects(const CUIRect &A, const CUIRect &B, CUIRect *pOut)
	{
		const float X0 = maximum(A.x, B.x);
		const float Y0 = maximum(A.y, B.y);
		const float X1 = minimum(A.x + A.w, B.x + B.w);
		const float Y1 = minimum(A.y + A.h, B.y + B.h);
		if(X1 <= X0 || Y1 <= Y0)
			return false;
		*pOut = {X0, Y0, X1 - X0, Y1 - Y0};
		return true;
	}

	void EnableCurrentScreenClip(IGraphics *pGraphics, const CUIRect &Clip)
	{
		float ScreenX0 = 0.0f;
		float ScreenY0 = 0.0f;
		float ScreenX1 = 0.0f;
		float ScreenY1 = 0.0f;
		pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		const float ScreenW = maximum(0.001f, ScreenX1 - ScreenX0);
		const float ScreenH = maximum(0.001f, ScreenY1 - ScreenY0);
		const float XScale = pGraphics->ScreenWidth() / ScreenW;
		const float YScale = pGraphics->ScreenHeight() / ScreenH;
		pGraphics->ClipEnable(
			round_to_int((Clip.x - ScreenX0) * XScale),
			round_to_int((Clip.y - ScreenY0) * YScale),
			maximum(1, round_to_int(Clip.w * XScale)),
			maximum(1, round_to_int(Clip.h * YScale)));
	}

	void DrawTextClipped(ITextRender *pTextRender, IGraphics *pGraphics, const CUIRect &Clip, const char *pText, float X, float Y, float FontSize, const ColorRGBA &Color)
	{
		if(Clip.w <= 0.0f || Clip.h <= 0.0f || Color.a <= 0.0f)
			return;
		EnableCurrentScreenClip(pGraphics, Clip);
		pTextRender->TextColor(Color);
		pTextRender->Text(X, Y, FontSize, pText, -1.0f);
		pGraphics->ClipDisable();
	}

	float ScrolledTextX(const CUIRect &Rect, float TextW, bool AllowScroll, float Progress)
	{
		const float MaxWidth = maximum(1.0f, Rect.w - HUD_PADDING_X * 2.0f);
		if(TextW <= MaxWidth)
			return Rect.x + Rect.w * 0.5f - TextW * 0.5f;
		if(!AllowScroll)
			return Rect.x + HUD_PADDING_X;
		const float ScrollStart = 0.10f;
		const float ScrollEnd = 0.92f;
		const float T = QmLyrics::EaseOutCubic((Progress - ScrollStart) / maximum(0.001f, ScrollEnd - ScrollStart));
		return Rect.x + HUD_PADDING_X - (TextW - MaxWidth) * T;
	}

	CUIRect LineClipRect(const CUIRect &Rect, float Y, float FontSize)
	{
		CUIRect Clip;
		if(IntersectRects(Rect, {Rect.x + HUD_PADDING_X, Y - 4.0f, maximum(1.0f, Rect.w - HUD_PADDING_X * 2.0f), FontSize + 8.0f}, &Clip))
			return Clip;
		return {Rect.x, Rect.y, 0.0f, 0.0f};
	}

	void DrawCenteredTextLine(ITextRender *pTextRender, IGraphics *pGraphics, const CUIRect &Rect, const char *pText, float TextWidth, float FontSize, float Y, const ColorRGBA &Color, bool AllowScroll, float Progress)
	{
		const float X = ScrolledTextX(Rect, TextWidth, AllowScroll, Progress);
		DrawTextClipped(pTextRender, pGraphics, LineClipRect(Rect, Y, FontSize), pText, X, Y, FontSize, Color);
	}

	float MeasureLyricsTextWidth(void *pUser, float FontSize, const char *pText)
	{
		return static_cast<ITextRender *>(pUser)->TextWidth(FontSize, pText);
	}

	size_t TextWidthContextHash()
	{
		size_t Hash = std::hash<std::string_view>{}(g_Config.m_TcCustomFont);
		Hash ^= std::hash<std::string_view>{}(g_Config.m_ClLanguagefile) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		return Hash;
	}

	const QmLyrics::SLineTextWidthCache &LineTextWidthCache(ITextRender *pTextRender, const QmLyrics::SLyricsLine &Line, float FontSize)
	{
		QmLyrics::UpdateLineTextWidthCache(&Line.m_TextWidthCache, Line, FontSize, TextWidthContextHash(), pTextRender, MeasureLyricsTextWidth);
		return Line.m_TextWidthCache;
	}

	const QmLyrics::STextWidthCache &TextWidthCache(ITextRender *pTextRender, QmLyrics::STextWidthCache *pCache, const char *pText, float FontSize)
	{
		QmLyrics::UpdateTextWidthCache(pCache, pText, FontSize, TextWidthContextHash(), pTextRender, MeasureLyricsTextWidth);
		return *pCache;
	}

	void DrawStatusLine(ITextRender *pTextRender, const CUIRect &Rect, const char *pText, float FontSize, float Y, const ColorRGBA &Color)
	{
		pTextRender->TextColor(Color);
		CTextCursor Cursor;
		Cursor.m_FontSize = FontSize;
		Cursor.m_LineWidth = Rect.w - HUD_PADDING_X * 2.0f;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.SetPosition(vec2(Rect.x + HUD_PADDING_X, Y));
		pTextRender->TextEx(&Cursor, pText);
	}

	float PlayedWidthForLine(ITextRender *pTextRender, const QmLyrics::SLyricsLine &Line, int64_t NowMs, float FontSize)
	{
		const char *pText = VisibleLineText(Line);
		const QmLyrics::SLineTextWidthCache &Cache = LineTextWidthCache(pTextRender, Line, FontSize);
		const float FullLineWidth = Cache.m_RawTextWidth;
		if(Line.m_vWords.empty())
			return FullLineWidth * QmLyrics::LinePlayProgress(Line, NowMs);
		if(QmLyrics::WordPlayProgress(Line.m_vWords.front(), NowMs) <= 0.0f)
			return 0.0f;
		if(QmLyrics::WordPlayProgress(Line.m_vWords.back(), NowMs) >= 1.0f)
			return FullLineWidth;

		float FallbackWidth = 0.0f;
		size_t SearchOffset = 0;
		const bool HasRawText = !Line.m_RawText.empty();
		for(size_t WordIndex = 0; WordIndex < Line.m_vWords.size(); ++WordIndex)
		{
			const QmLyrics::SLyricsWord &Word = Line.m_vWords[WordIndex];
			const float WordWidth = WordIndex < Cache.m_vWordWidths.size() ? Cache.m_vWordWidths[WordIndex] : pTextRender->TextWidth(FontSize, Word.m_Text.c_str());
			if(HasRawText && !Word.m_Text.empty())
			{
				const size_t WordOffset = QmLyrics::FindNextWordTextOffset(Line.m_RawText, Word.m_Text, SearchOffset);
				if(QmLyrics::HasOnlyAsciiSpacingBetweenWords(Line.m_RawText, SearchOffset, WordOffset))
					FallbackWidth += (float)(WordOffset - SearchOffset) * Cache.m_SpaceWidth;
				if(WordOffset != std::string::npos)
					SearchOffset = WordOffset + Word.m_Text.size();
			}
			const float Progress = QmLyrics::WordPlayProgress(Word, NowMs);
			FallbackWidth += WordWidth * Progress;
			if(Progress < 1.0f)
				break;
		}
		return std::clamp(FallbackWidth, 0.0f, FullLineWidth);
	}

	ColorRGBA PlayedColorForCurrentWord(ColorRGBA Played, const QmLyrics::SLyricsLine &Line, int64_t NowMs)
	{
		for(const QmLyrics::SLyricsWord &Word : Line.m_vWords)
		{
			const float Progress = QmLyrics::WordPlayProgress(Word, NowMs);
			if(Progress > 0.0f && Progress < 1.0f)
			{
				Played = WithBrightness(Played, QmLyrics::LongWordPulseBrightness(Word, NowMs));
				break;
			}
		}
		return Played;
	}

	void DrawKaraokeLine(ITextRender *pTextRender, IGraphics *pGraphics, const QmLyrics::SLyricsLine &Line, int64_t NowMs, const CUIRect &Rect, float FontSize, float Y, ColorRGBA Played, ColorRGBA Unplayed, float Alpha)
	{
		const char *pText = VisibleLineText(Line);
		const float LineProgress = QmLyrics::LinePlayProgress(Line, NowMs);
		const QmLyrics::SLineTextWidthCache &Cache = LineTextWidthCache(pTextRender, Line, FontSize);
		const float X = ScrolledTextX(Rect, Cache.m_RawTextWidth, true, LineProgress);
		const CUIRect BaseClip = LineClipRect(Rect, Y, FontSize);
		if(Line.m_vWords.empty() || !g_Config.m_QmLyricsKaraoke)
		{
			DrawTextClipped(pTextRender, pGraphics, BaseClip, pText, X, Y, FontSize, WithMultipliedAlpha(Played, Alpha));
			return;
		}

		DrawTextClipped(pTextRender, pGraphics, BaseClip, pText, X, Y, FontSize, WithMultipliedAlpha(Unplayed, Alpha));

		const float PlayedWidth = std::clamp(PlayedWidthForLine(pTextRender, Line, NowMs, FontSize), 0.0f, Cache.m_RawTextWidth);
		if(PlayedWidth <= 0.0f)
			return;

		const float SoftEdge = (float)std::clamp(g_Config.m_QmLyricsHighlightEdgeSoft, 0, 32);
		CUIRect PlayedClip;
		if(IntersectRects(BaseClip, {X, BaseClip.y, PlayedWidth, BaseClip.h}, &PlayedClip))
		{
			const ColorRGBA PulsePlayed = PlayedColorForCurrentWord(Played, Line, NowMs);
			DrawTextClipped(pTextRender, pGraphics, PlayedClip, pText, X, Y, FontSize, WithMultipliedAlpha(PulsePlayed, Alpha));
		}
		if(SoftEdge > 0.0f)
		{
			CUIRect EdgeClip;
			if(IntersectRects(BaseClip, {X + PlayedWidth, BaseClip.y, SoftEdge, BaseClip.h}, &EdgeClip))
			{
				const ColorRGBA EdgeColor = MixColor(Unplayed, Played, 0.45f);
				DrawTextClipped(pTextRender, pGraphics, EdgeClip, pText, X, Y, FontSize, WithMultipliedAlpha(EdgeColor, Alpha));
			}
		}
	}

} // namespace

bool CQmLyrics::GetMediaIslandText(char *pBuf, size_t BufSize, ColorRGBA *pColor) const
{
	if(pBuf == nullptr || BufSize == 0)
		return false;
	pBuf[0] = '\0';

	if(!g_Config.m_QmLyrics || !g_Config.m_QmLyricsInMediaIsland || g_Config.m_QmHudIslandUseOriginalStyle)
		return false;
	if(g_Config.m_QmLyricsHideWhenPaused && m_pImpl->m_State == SImpl::EState::READY && !m_pImpl->m_LastMediaPlaying)
		return false;
	if(g_Config.m_QmLyricsHideNoLyrics && m_pImpl->m_State == SImpl::EState::NO_RESULT)
		return false;

	auto SetText = [&](const char *pText, ColorRGBA Color) {
		str_copy(pBuf, pText != nullptr ? pText : "", BufSize);
		if(pColor != nullptr)
			*pColor = Color;
		return pBuf[0] != '\0';
	};

	const float Opacity = std::clamp(g_Config.m_QmLyricsOpacity / 100.0f, 0.0f, 1.0f);
	switch(m_pImpl->m_State)
	{
	case SImpl::EState::IDLE:
		return false;
	case SImpl::EState::FETCHING:
		return SetText(Localize("Lyrics: searching..."), ColorRGBA(0.7f, 0.85f, 1.0f, 0.90f * Opacity));
	case SImpl::EState::NO_RESULT:
		return SetText(Localize("Lyrics: no lyrics found"), ColorRGBA(1.0f, 0.7f, 0.7f, 0.90f * Opacity));
	case SImpl::EState::READY:
		break;
	}

	if(m_pImpl->m_Track.m_vLines.empty())
		return SetText(Localize("Lyrics: empty track"), ColorRGBA(1.0f, 0.7f, 0.7f, 0.90f * Opacity));

	const int64_t NowMs = m_pImpl->m_Clock.Now(time_get(), time_freq());
	const int Active = QmLyrics::ResolveDisplayLineIndex(m_pImpl->m_Track, m_pImpl->m_ActiveLineIndex, NowMs);
	if(Active < 0 || Active >= (int)m_pImpl->m_Track.m_vLines.size())
		return false;

	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorPlayed, true));
	Color.a *= 0.92f * Opacity;
	return SetText(VisibleLineText(m_pImpl->m_Track.m_vLines[Active]), Color);
}

bool CQmLyrics::RenderMediaIslandLine(const CUIRect &Rect, float FontSize, float Alpha)
{
	if(!g_Config.m_QmLyrics || !g_Config.m_QmLyricsInMediaIsland || g_Config.m_QmHudIslandUseOriginalStyle)
		return false;
	if(g_Config.m_QmLyricsHideWhenPaused && m_pImpl->m_State == SImpl::EState::READY && !m_pImpl->m_LastMediaPlaying)
		return false;
	if(g_Config.m_QmLyricsHideNoLyrics && m_pImpl->m_State == SImpl::EState::NO_RESULT)
		return false;
	if(Rect.w <= 0.0f || Rect.h <= 0.0f || Alpha <= 0.0f)
		return false;

	const float Opacity = std::clamp(g_Config.m_QmLyricsOpacity / 100.0f, 0.0f, 1.0f) * Alpha;
	const float TextY = Rect.y + std::max(0.0f, (Rect.h - FontSize) * 0.5f);

	const unsigned int PrevFlags = TextRender()->GetRenderFlags();
	const ColorRGBA PrevTextColor = TextRender()->GetTextColor();
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);

	auto DrawStatus = [&](const char *pText, ColorRGBA Color) {
		DrawStatusLine(TextRender(), Rect, pText, FontSize, TextY, Color);
	};

	bool Rendered = true;
	switch(m_pImpl->m_State)
	{
	case SImpl::EState::IDLE:
		Rendered = false;
		break;
	case SImpl::EState::FETCHING:
		DrawStatus(Localize("Lyrics: searching..."), ColorRGBA(0.7f, 0.85f, 1.0f, 0.90f * Opacity));
		break;
	case SImpl::EState::NO_RESULT:
		DrawStatus(Localize("Lyrics: no lyrics found"), ColorRGBA(1.0f, 0.7f, 0.7f, 0.90f * Opacity));
		break;
	case SImpl::EState::READY:
		if(m_pImpl->m_Track.m_vLines.empty())
		{
			DrawStatus(Localize("Lyrics: empty track"), ColorRGBA(1.0f, 0.7f, 0.7f, 0.90f * Opacity));
			break;
		}

		{
			const int64_t NowMs = m_pImpl->m_Clock.Now(time_get(), time_freq());
			const int Active = QmLyrics::ResolveDisplayLineIndex(m_pImpl->m_Track, m_pImpl->m_ActiveLineIndex, NowMs);
			if(Active < 0 || Active >= (int)m_pImpl->m_Track.m_vLines.size())
			{
				Rendered = false;
				break;
			}

			ColorRGBA Played = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorPlayed, true));
			ColorRGBA Unplayed = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorUnplayed, true));
			DrawKaraokeLine(TextRender(), Graphics(), m_pImpl->m_Track.m_vLines[Active], NowMs, Rect, FontSize, TextY, Played, Unplayed, Opacity);
		}
		break;
	}

	TextRender()->TextColor(PrevTextColor);
	TextRender()->SetRenderFlags(PrevFlags);
	return Rendered;
}

void CQmLyrics::RenderHud()
{
	if(!g_Config.m_QmLyrics)
		return;
	if(g_Config.m_QmLyricsInMediaIsland && !g_Config.m_QmHudIslandUseOriginalStyle)
		return;
	if(g_Config.m_QmLyricsHideWhenPaused && m_pImpl->m_State == SImpl::EState::READY && !m_pImpl->m_LastMediaPlaying)
		return;
	if(g_Config.m_QmLyricsHideNoLyrics && m_pImpl->m_State == SImpl::EState::NO_RESULT)
		return;

	const int Above = std::clamp(g_Config.m_QmLyricsLinesAbove, 0, 6);
	const int Below = std::clamp(g_Config.m_QmLyricsLinesBelow, 0, 6);
	const float FontActive = (float)g_Config.m_QmLyricsFontSize;
	const float FontOther = (float)g_Config.m_QmLyricsFontSizeOther;
	const float LineGap = (float)g_Config.m_QmLyricsLineSpacing;
	const float Opacity = std::clamp(g_Config.m_QmLyricsOpacity / 100.0f, 0.0f, 1.0f);
	const float InactiveMinOpacity = std::clamp(g_Config.m_QmLyricsInactiveOpacity / 100.0f, 0.0f, 1.0f);
	const float ScaleActive = std::clamp(g_Config.m_QmLyricsScaleActive / 100.0f, 1.0f, 2.0f);
	const float ScaleFalloff = std::clamp(g_Config.m_QmLyricsScaleFalloff / 100.0f, 0.0f, 0.2f);
	const float FadePerLine = std::clamp(g_Config.m_QmLyricsFadePerLine / 100.0f, 0.0f, 0.4f);
	const float SubtitleFontSize = std::max(8.0f, FontOther * 0.85f);
	const int MaxSubtitleCount = (g_Config.m_QmLyricsShowTranslation ? 1 : 0) + (g_Config.m_QmLyricsShowTransliteration ? 1 : 0);

	const float ActiveBlockHeight = QmLyrics::LineHeight(FontActive * ScaleActive, LineGap, SubtitleFontSize, MaxSubtitleCount);
	const float OtherBlockHeight = QmLyrics::LineHeight(FontOther, LineGap, SubtitleFontSize, 0);
	const float TotalHeight = HUD_PADDING_Y * 2.0f + ActiveBlockHeight + (float)(Above + Below) * OtherBlockHeight + LineGap * 2.0f;
	float SavedX0 = 0, SavedY0 = 0, SavedX1 = 0, SavedY1 = 0;
	Graphics()->GetScreen(&SavedX0, &SavedY0, &SavedX1, &SavedY1);
	const float CanvasH = 300.0f;
	const float CanvasW = CanvasH * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, CanvasW, CanvasH);
	const float HudWidth = minimum(HUD_WIDTH, maximum(240.0f, CanvasW - 16.0f));
	const float DefaultX = CanvasW * 0.5f - HudWidth * 0.5f;
	const float DefaultY = CanvasH * 0.68f - TotalHeight * 0.5f;

	const CUIRect DefaultRect = {DefaultX, DefaultY, HudWidth, TotalHeight};
	const QmHudEditor::SEdgeMargin Margin = QmHudEditor::SEdgeMargin::Uniform((float)g_Config.m_QmLyricsEdgeMargin);
	const auto Scope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::Lyrics, DefaultRect, DefaultRect, Margin);
	const CUIRect &Rect = DefaultRect;

	ColorRGBA Played = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorPlayed, true));
	ColorRGBA Unplayed = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorUnplayed, true));
	ColorRGBA Translation = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorTranslation, true));

	const unsigned int PrevFlags = TextRender()->GetRenderFlags();
	const ColorRGBA PrevTextColor = TextRender()->GetTextColor();
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);

	auto DrawStatus = [&](const char *pText, ColorRGBA Color) {
		DrawStatusLine(TextRender(), Rect, pText, FontOther, Rect.y + HUD_PADDING_Y, Color);
	};

	switch(m_pImpl->m_State)
	{
	case SImpl::EState::IDLE:
		DrawStatus(Localize("Lyrics: no media playing"), ColorRGBA(0.7f, 0.7f, 0.7f, Opacity));
		goto end_render;
	case SImpl::EState::FETCHING:
		DrawStatus(Localize("Lyrics: searching..."), ColorRGBA(0.7f, 0.85f, 1.0f, Opacity));
		goto end_render;
	case SImpl::EState::NO_RESULT:
		DrawStatus(Localize("Lyrics: no lyrics found"), ColorRGBA(1.0f, 0.7f, 0.7f, Opacity));
		goto end_render;
	case SImpl::EState::READY:
		break;
	}

	if(m_pImpl->m_Track.m_vLines.empty())
	{
		DrawStatus(Localize("Lyrics: empty track"), ColorRGBA(1.0f, 0.7f, 0.7f, Opacity));
		goto end_render;
	}

	{
		int Active = m_pImpl->m_ActiveLineIndex;
		const int64_t NowMs = m_pImpl->m_Clock.Now(time_get(), time_freq());
		Active = QmLyrics::ResolveDisplayLineIndex(m_pImpl->m_Track, Active, NowMs);
		if(Active < 0)
			goto end_render;

		const int Total = (int)m_pImpl->m_Track.m_vLines.size();
		const int FirstIdx = std::max(0, Active - Above);
		const int LastIdx = std::min(Total - 1, Active + Below);
		const float MotionScale = QmLyrics::MotionScaleForLevel(g_Config.m_QmUiMotionLevel);
		const float ScrollMs = (float)g_Config.m_QmLyricsScrollMs * MotionScale;
		float ScrollOffset = 0.0f;

		auto SubtitleCountForLine = [&](const QmLyrics::SLyricsLine &Line, bool IsActive) {
			if(!IsActive)
				return 0;
			int Count = 0;
			if(g_Config.m_QmLyricsShowTranslation && Line.m_Translation.has_value() && !Line.m_Translation->empty())
				++Count;
			if(g_Config.m_QmLyricsShowTransliteration && Line.m_Transliteration.has_value() && !Line.m_Transliteration->empty())
				++Count;
			return Count;
		};
		if(ScrollMs > 0.0f && m_pImpl->m_PreviousAnimatedLineIndex >= 0 && m_pImpl->m_PreviousAnimatedLineIndex != Active)
		{
			const float ElapsedMs = (float)(time_get() - m_pImpl->m_ActiveLineChangedTick) * 1000.0f / (float)time_freq();
			const float T = QmLyrics::EaseOutCubic(ElapsedMs / ScrollMs);
			const float Direction = Active > m_pImpl->m_PreviousAnimatedLineIndex ? 1.0f : -1.0f;
			const int ActiveSubtitleCount = SubtitleCountForLine(m_pImpl->m_Track.m_vLines[Active], true);
			ScrollOffset = Direction * QmLyrics::LineTransitionDistance(FontActive * ScaleActive, FontOther, LineGap, SubtitleFontSize, ActiveSubtitleCount) * (1.0f - T);
		}
		auto BuildVisual = [&](int Index, float DistanceFromActive) {
			const QmLyrics::SLyricsLine &Line = m_pImpl->m_Track.m_vLines[Index];
			return QmLyrics::BuildLineVisual(
				Index,
				Active,
				Rect.y + Rect.h * 0.45f + ScrollOffset,
				DistanceFromActive,
				FontActive,
				FontOther,
				LineGap,
				Opacity,
				InactiveMinOpacity,
				ScaleActive,
				ScaleFalloff,
				FadePerLine,
				SubtitleCountForLine(Line, Index == Active),
				SubtitleFontSize);
		};

		struct SDrawEntry
		{
			int m_Index = -1;
			QmLyrics::SLineVisual m_Visual;
		};
		std::array<SDrawEntry, 13> aEntries{};
		int EntryCount = 0;
		auto AddEntry = [&](int Index, const QmLyrics::SLineVisual &Visual) {
			if(EntryCount < (int)aEntries.size())
			{
				aEntries[EntryCount].m_Index = Index;
				aEntries[EntryCount].m_Visual = Visual;
				++EntryCount;
			}
		};

		const QmLyrics::SLineVisual ActiveVisual = BuildVisual(Active, 0.0f);
		AddEntry(Active, ActiveVisual);

		float PreviousHeight = ActiveVisual.m_Height;
		float CenterOffset = 0.0f;
		for(int i = Active - 1; i >= FirstIdx; --i)
		{
			const QmLyrics::SLineVisual VisualAtCenter = BuildVisual(i, 0.0f);
			CenterOffset -= PreviousHeight * 0.5f + VisualAtCenter.m_Height * 0.5f;
			AddEntry(i, BuildVisual(i, CenterOffset));
			PreviousHeight = VisualAtCenter.m_Height;
		}

		PreviousHeight = ActiveVisual.m_Height;
		CenterOffset = 0.0f;
		for(int i = Active + 1; i <= LastIdx; ++i)
		{
			const QmLyrics::SLineVisual VisualAtCenter = BuildVisual(i, 0.0f);
			CenterOffset += PreviousHeight * 0.5f + VisualAtCenter.m_Height * 0.5f;
			AddEntry(i, BuildVisual(i, CenterOffset));
			PreviousHeight = VisualAtCenter.m_Height;
		}

		auto DrawEntry = [&](const SDrawEntry &Entry) {
			if(Entry.m_Index < 0)
				return;
			const QmLyrics::SLyricsLine &Line = m_pImpl->m_Track.m_vLines[Entry.m_Index];
			const QmLyrics::SLineVisual &Visual = Entry.m_Visual;
			const bool IsActive = Entry.m_Index == Active;
			if(IsActive)
			{
				DrawKaraokeLine(TextRender(), Graphics(), Line, NowMs, Rect, Visual.m_FontSize, Visual.m_PrimaryY, Played, Unplayed, Visual.m_Alpha);
				float SubtitleY = Visual.m_SubtitleY;
				if(g_Config.m_QmLyricsShowTranslation && Line.m_Translation.has_value() && !Line.m_Translation->empty())
				{
					const QmLyrics::STextWidthCache &Cache = TextWidthCache(TextRender(), &Line.m_TranslationWidthCache, Line.m_Translation->c_str(), SubtitleFontSize);
					DrawCenteredTextLine(TextRender(), Graphics(), Rect, Line.m_Translation->c_str(), Cache.m_TextWidth, SubtitleFontSize, SubtitleY, WithMultipliedAlpha(Translation, Visual.m_Alpha * 0.9f), true, QmLyrics::LinePlayProgress(Line, NowMs));
					SubtitleY += SubtitleFontSize + maximum(1.0f, LineGap * 0.35f);
				}
				if(g_Config.m_QmLyricsShowTransliteration && Line.m_Transliteration.has_value() && !Line.m_Transliteration->empty())
				{
					const QmLyrics::STextWidthCache &Cache = TextWidthCache(TextRender(), &Line.m_TransliterationWidthCache, Line.m_Transliteration->c_str(), SubtitleFontSize);
					DrawCenteredTextLine(TextRender(), Graphics(), Rect, Line.m_Transliteration->c_str(), Cache.m_TextWidth, SubtitleFontSize, SubtitleY, WithMultipliedAlpha(Translation, Visual.m_Alpha * 0.72f), true, QmLyrics::LinePlayProgress(Line, NowMs));
				}
			}
			else
			{
				const QmLyrics::SLineTextWidthCache &Cache = LineTextWidthCache(TextRender(), Line, Visual.m_FontSize);
				DrawCenteredTextLine(TextRender(), Graphics(), Rect, VisibleLineText(Line), Cache.m_RawTextWidth, Visual.m_FontSize, Visual.m_PrimaryY, WithMultipliedAlpha(Unplayed, Visual.m_Alpha), false, 0.0f);
			}
		};

		for(int Distance = 6; Distance >= 1; --Distance)
		{
			for(int i = 0; i < EntryCount; ++i)
			{
				if(std::abs(aEntries[i].m_Index - Active) == Distance)
					DrawEntry(aEntries[i]);
			}
		}
		for(int i = 0; i < EntryCount; ++i)
		{
			if(aEntries[i].m_Index == Active)
				DrawEntry(aEntries[i]);
		}
	}

end_render:
	TextRender()->TextColor(PrevTextColor);
	TextRender()->SetRenderFlags(PrevFlags);

	GameClient()->m_HudEditor.EndTransform(Scope);
	Graphics()->MapScreen(SavedX0, SavedY0, SavedX1, SavedY1);
}

void CQmLyrics::OnUpdate()
{
	TickStateMachine();
}

void CQmLyrics::OnRender()
{
	RenderHud();
}
