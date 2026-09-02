#include "music_lyrics_integration.h"

#include "qm_soda_lyric_file.h"

#include <base/color.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
	bool ContainsAsciiInsensitive(std::string_view Text, std::string_view Needle)
	{
		if(Needle.empty() || Text.size() < Needle.size())
			return false;
		for(size_t Offset = 0; Offset <= Text.size() - Needle.size(); ++Offset)
		{
			bool Match = true;
			for(size_t Index = 0; Index < Needle.size(); ++Index)
			{
				const char TextChar = Text[Offset + Index] >= 'A' && Text[Offset + Index] <= 'Z' ? (char)(Text[Offset + Index] - 'A' + 'a') : Text[Offset + Index];
				const char NeedleChar = Needle[Index] >= 'A' && Needle[Index] <= 'Z' ? (char)(Needle[Index] - 'A' + 'a') : Needle[Index];
				if(TextChar != NeedleChar)
				{
					Match = false;
					break;
				}
			}
			if(Match)
				return true;
		}
		return false;
	}

	bool IsSodaSourceAppId(std::string_view SourceAppId)
	{
		return ContainsAsciiInsensitive(SourceAppId, "sodamusic") || ContainsAsciiInsensitive(SourceAppId, "soda") ||
		       ContainsAsciiInsensitive(SourceAppId, "qishui") || ContainsAsciiInsensitive(SourceAppId, "汽水");
	}

	uint64_t MonotonicTickMs()
	{
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	// 从 mediaId 文本提取稳定数字身份(供 generation/切歌判断)。
	uint64_t ParseMediaId(const char *pText)
	{
		if(pText == nullptr)
			return 0;
		uint64_t Result = 0;
		bool Any = false;
		for(const char *p = pText; *p != '\0'; ++p)
		{
			if(*p >= '0' && *p <= '9')
			{
				Result = Result * 10 + (uint64_t)(*p - '0');
				Any = true;
			}
		}
		return Any ? Result : 0;
	}
}

struct CMusicLyricsIntegration::SImpl
{
	CQmSodaHookProvider m_Provider;
	bool m_HookConfigInitialized = false;
	bool m_LastHookEnabled = false;
	std::string m_LastHelperPath;
	QmSodaHook::SSnapshot m_Snapshot{};
	bool m_HasSnapshot = false;
	uint64_t m_LastReadTick = 0;
	bool m_ReadInitialized = false;

	// 歌词数据。
	QmMusicLyrics::SLyricsData m_Lyrics;
	bool m_HasLyrics = false;
	std::string m_LoadedFilePath;
	uint64_t m_LoadedGeneration = 0;
	uint64_t m_SongId = 0;
	bool m_HasSong = false;
	int64_t m_PositionMs = 0;
	bool m_PositionValid = false;
	bool m_ActiveLyrics = false;

	// 当前句选择。
	std::string m_CurrentLyric;
	int64_t m_LineStartMs = -1;
	int64_t m_LineEndMs = -1;
};

CMusicLyricsIntegration::CMusicLyricsIntegration() :
	m_pImpl(std::make_unique<SImpl>()) {}

CMusicLyricsIntegration::~CMusicLyricsIntegration() = default;

void CMusicLyricsIntegration::OnInit()
{
	m_pImpl->m_HookConfigInitialized = false;
	m_pImpl->m_LastHookEnabled = false;
	m_pImpl->m_LastHelperPath.clear();
	m_pImpl->m_HasSnapshot = false;
	m_pImpl->m_Snapshot = {};
	m_pImpl->m_HasLyrics = false;
	m_pImpl->m_LoadedFilePath.clear();
	m_pImpl->m_LoadedGeneration = 0;
	m_pImpl->m_SongId = 0;
	m_pImpl->m_HasSong = false;
	m_pImpl->m_PositionValid = false;
	m_pImpl->m_ActiveLyrics = false;
	m_pImpl->m_CurrentLyric.clear();
	m_pImpl->m_LineStartMs = -1;
	m_pImpl->m_LineEndMs = -1;
	SyncHookConfiguration();
}

void CMusicLyricsIntegration::OnShutdown()
{
	m_pImpl->m_Provider.Stop();
	m_pImpl->m_HasLyrics = false;
	m_pImpl->m_HasSnapshot = false;
	m_pImpl->m_ActiveLyrics = false;
	m_pImpl->m_CurrentLyric.clear();
}

void CMusicLyricsIntegration::OnReset()
{
	m_pImpl->m_HasLyrics = false;
	m_pImpl->m_HasSnapshot = false;
	m_pImpl->m_ActiveLyrics = false;
	m_pImpl->m_CurrentLyric.clear();
	m_pImpl->m_LoadedFilePath.clear();
	m_pImpl->m_LoadedGeneration = 0;
}

void CMusicLyricsIntegration::SyncHookConfiguration()
{
	const bool HookEnabled = g_Config.m_QmSodaHookEnable != 0;
	const bool ConfigurationChanged = !m_pImpl->m_HookConfigInitialized ||
					  HookEnabled != m_pImpl->m_LastHookEnabled ||
					  (HookEnabled && m_pImpl->m_LastHelperPath != g_Config.m_QmSodaHookHelperPath);
	if(!ConfigurationChanged)
		return;
	if(HookEnabled)
		m_pImpl->m_Provider.Start(g_Config.m_QmSodaHookHelperPath);
	else
		m_pImpl->m_Provider.Stop();
	m_pImpl->m_HookConfigInitialized = true;
	m_pImpl->m_LastHookEnabled = HookEnabled;
	m_pImpl->m_LastHelperPath = HookEnabled ? g_Config.m_QmSodaHookHelperPath : "";
	m_pImpl->m_HasSnapshot = false;
	m_pImpl->m_Snapshot = {};
}

void CMusicLyricsIntegration::ClearForStaleMedia()
{
	m_pImpl->m_HasLyrics = false;
	m_pImpl->m_HasSnapshot = false;
	m_pImpl->m_Snapshot = {};
	m_pImpl->m_HasSong = false;
	m_pImpl->m_SongId = 0;
	m_pImpl->m_ActiveLyrics = false;
	m_pImpl->m_CurrentLyric.clear();
	m_pImpl->m_LineStartMs = -1;
	m_pImpl->m_LineEndMs = -1;
	m_pImpl->m_LoadedFilePath.clear();
	m_pImpl->m_LoadedGeneration = 0;
}

void CMusicLyricsIntegration::LoadLyricFile(const char *pPath)
{
	if(pPath == nullptr || pPath[0] == '\0')
		return;
	std::ifstream Stream(pPath, std::ios::binary);
	if(!Stream)
		return;
	std::ostringstream Buffer;
	Buffer << Stream.rdbuf();
	const std::string Json = Buffer.str();
	if(Json.empty())
		return;
	QmMusicLyrics::SLyricsData Lyrics;
	std::string Error;
	if(!QmSodaLyricFile::ParseLyricFileJson(Json, &Lyrics, &Error))
		return;
	m_pImpl->m_Lyrics = std::move(Lyrics);
	m_pImpl->m_HasLyrics = m_pImpl->m_Lyrics.HasLyrics();
	m_pImpl->m_LoadedFilePath = pPath;
	m_pImpl->m_ActiveLyrics = true;
}

void CMusicLyricsIntegration::OnUpdate()
{
	SyncHookConfiguration();
	if(!g_Config.m_QmSodaHookEnable)
	{
		ClearForStaleMedia();
		return;
	}

	QmSodaHook::SSnapshot Snapshot{};
	const bool HasSnapshot = m_pImpl->m_Provider.Read(&Snapshot, g_Config.m_QmSodaHookTimeoutMs);
	if(!HasSnapshot)
	{
		// 短暂读取失败保留有限窗口,避免闪烁;超时后清理。
		if(m_pImpl->m_HasSnapshot)
		{
			const uint64_t Now = MonotonicTickMs();
			if(m_pImpl->m_LastReadTick == 0 || Now - m_pImpl->m_LastReadTick <= (uint64_t)std::max(1500, g_Config.m_QmSodaHookTimeoutMs * 2))
				return;
		}
		ClearForStaleMedia();
		return;
	}
	m_pImpl->m_LastReadTick = MonotonicTickMs();
	const bool HadSnapshot = m_pImpl->m_HasSnapshot;
	m_pImpl->m_Snapshot = Snapshot;
	m_pImpl->m_HasSnapshot = true;

	const bool HasSong = (Snapshot.m_Flags & QmSodaHook::FLAG_HAS_SONG) != 0;
	const uint64_t SongId = ParseMediaId(Snapshot.m_aMediaId);
	const uint64_t Generation = Snapshot.m_Generation;
	const bool SongChanged = HasSong && (!m_pImpl->m_HasSong || m_pImpl->m_SongId != SongId || m_pImpl->m_LoadedGeneration != Generation);
	if(SongChanged)
	{
		// 歌曲或 generation 变化:清空旧歌词并加载新歌词文件。
		m_pImpl->m_HasLyrics = false;
		m_pImpl->m_CurrentLyric.clear();
		m_pImpl->m_LineStartMs = -1;
		m_pImpl->m_LineEndMs = -1;
		m_pImpl->m_HasSong = true;
		m_pImpl->m_SongId = SongId;
		m_pImpl->m_LoadedGeneration = Generation;
		m_pImpl->m_ActiveLyrics = false;
	}
	else if(!HasSong)
	{
		ClearForStaleMedia();
		return;
	}
	// 同一首歌持续播放时,helper 每次发布都带歌词路径(歌曲变化时重写文件)。
	// 若客户端启动晚于 helper、首次快照没有路径,这里在后续快照带路径时补加载。
	if(HasSong && !m_pImpl->m_HasLyrics && (Snapshot.m_Flags & QmSodaHook::FLAG_HAS_LYRIC_FILE) != 0)
		LoadLyricFile(Snapshot.m_aLyricFilePath);

	// 进度更新。
	if((Snapshot.m_Flags & QmSodaHook::FLAG_POSITION_VALID) != 0)
	{
		m_pImpl->m_PositionMs = Snapshot.m_PositionMs;
		m_pImpl->m_PositionValid = true;
	}

	// 选择当前句(使用统一时间轴选择逻辑)。
	if(m_pImpl->m_HasLyrics)
	{
		const NeteaseLyrics::STimeline &Timeline = m_pImpl->m_Lyrics.m_Timeline;
		const NeteaseLyrics::SSelectedLine Selected = NeteaseLyrics::SelectCurrentLine(Timeline, m_pImpl->m_PositionValid ? m_pImpl->m_PositionMs : 0);
		if(Selected.m_pLine == nullptr)
		{
			m_pImpl->m_CurrentLyric.clear();
			m_pImpl->m_LineStartMs = -1;
			m_pImpl->m_LineEndMs = -1;
		}
		else
		{
			m_pImpl->m_CurrentLyric = Selected.m_pLine->m_Text;
			m_pImpl->m_LineStartMs = Selected.m_pLine->m_StartMs;
			m_pImpl->m_LineEndMs = Selected.m_pLine->m_EndMs;
		}
		m_pImpl->m_ActiveLyrics = true;
	}
	(void)HadSnapshot;
}

bool CMusicLyricsIntegration::GetCurrentLyric(char *pBuffer, size_t BufferSize) const
{
	if(pBuffer == nullptr || BufferSize == 0)
		return false;
	pBuffer[0] = '\0';
	if(!g_Config.m_QmLyrics || !g_Config.m_QmLyricsInMediaIsland)
		return false;
	if(!m_pImpl->m_HasSong || !m_pImpl->m_HasLyrics || m_pImpl->m_CurrentLyric.empty())
		return false;
	QmSodaHook::CopyUtf8Truncated(pBuffer, BufferSize, m_pImpl->m_CurrentLyric.data(), m_pImpl->m_CurrentLyric.size());
	return pBuffer[0] != '\0';
}

bool CMusicLyricsIntegration::HasCurrentLyric() const
{
	return g_Config.m_QmLyrics != 0 && g_Config.m_QmLyricsInMediaIsland != 0 &&
	       m_pImpl->m_HasSong && m_pImpl->m_HasLyrics && !m_pImpl->m_CurrentLyric.empty();
}

bool CMusicLyricsIntegration::HasActiveLyrics() const
{
	return g_Config.m_QmLyrics != 0 && g_Config.m_QmLyricsInMediaIsland != 0 &&
	       m_pImpl->m_HasSong && m_pImpl->m_ActiveLyrics;
}

uint64_t CMusicLyricsIntegration::CurrentSongId() const
{
	return m_pImpl->m_SongId;
}
