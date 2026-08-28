#include "netease_integration.h"

#include <base/color.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <game/client/components/system_media_controls.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <chrono>

namespace
{
	bool IsNeteaseSourceAppId(std::string_view SourceAppId)
	{
		std::string Lower(SourceAppId);
		std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char Character) {
			return Character >= 'A' && Character <= 'Z' ? (char)(Character - 'A' + 'a') : (char)Character;
		});
		return Lower.find("cloudmusic") != std::string::npos || Lower.find("netease") != std::string::npos;
	}

	uint64_t MonotonicTickMs()
	{
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}

CNeteaseIntegration::CNeteaseIntegration() = default;

void CNeteaseIntegration::OnInit()
{
	m_LyricState.Reset();
	m_LastBridgeSequence = 0;
	m_LastBridgeGeneration = 0;
	m_LastBridgeSnapshotTick = 0;
	m_LastBridgePositionMs = 0;
	m_LastCloudMusicPid = 0;
	m_BlockedBridgeSongId = 0;
	m_BlockedBridgeGeneration = 0;
	m_LastMediaTitle.clear();
	m_LastMediaArtist.clear();
	m_WaitingForBridgeIdentity = false;
	m_LastBridgePositionValid = false;
	m_Initialized = true;
}

void CNeteaseIntegration::OnShutdown()
{
	m_LyricState.Reset();
	m_LastBridgeSequence = 0;
	m_LastBridgeGeneration = 0;
	m_LastBridgeSnapshotTick = 0;
	m_LastBridgePositionMs = 0;
	m_LastCloudMusicPid = 0;
	m_BlockedBridgeSongId = 0;
	m_BlockedBridgeGeneration = 0;
	m_LastMediaTitle.clear();
	m_LastMediaArtist.clear();
	m_WaitingForBridgeIdentity = false;
	m_LastBridgePositionValid = false;
	m_Initialized = false;
}

void CNeteaseIntegration::OnReset()
{
	m_LyricState.Reset();
	m_LastBridgeSequence = 0;
	m_LastBridgeGeneration = 0;
	m_LastBridgeSnapshotTick = 0;
	m_LastBridgePositionMs = 0;
	m_LastCloudMusicPid = 0;
	m_BlockedBridgeSongId = 0;
	m_BlockedBridgeGeneration = 0;
	m_LastMediaTitle.clear();
	m_LastMediaArtist.clear();
	m_WaitingForBridgeIdentity = false;
	m_LastBridgePositionValid = false;
}

void CNeteaseIntegration::ClearForStaleMedia()
{
	m_LyricState.MarkStopped();
	m_LastBridgeSequence = 0;
	m_LastBridgeGeneration = 0;
	m_LastBridgeSnapshotTick = 0;
	m_LastBridgePositionMs = 0;
	m_LastCloudMusicPid = 0;
	m_BlockedBridgeSongId = 0;
	m_BlockedBridgeGeneration = 0;
	m_WaitingForBridgeIdentity = false;
	m_LastBridgePositionValid = false;
}

void CNeteaseIntegration::OnUpdate()
{
	if(!m_Initialized || GameClient() == nullptr)
		return;

	QmNeteaseHook::SSnapshotV5 BridgeSnapshot{};
	const bool HasBridgeSnapshot = GameClient()->m_SystemMediaControls.GetNeteaseSnapshot(BridgeSnapshot);
	CSystemMediaControls::SState MediaState{};
	const bool HasMedia = GameClient()->m_SystemMediaControls.GetStateSnapshot(MediaState);
	// SMTC 是标准媒体状态的唯一权威来源。没有媒体时立即清除；Bridge
	// 短暂失联时仅对暂停中的已确认当前句使用有限宽限期。
	if(!HasMedia)
	{
		ClearForStaleMedia();
		return;
	}
	if(MediaState.m_aSourceAppId[0] != '\0' && !IsNeteaseSourceAppId(MediaState.m_aSourceAppId))
	{
		// 当前系统媒体会话属于其它播放器时，不能把网易云私有歌词
		// 合并到它的标题、封面或播放状态上。
		ClearForStaleMedia();
		return;
	}
	const std::string MediaTitle = MediaState.m_aTitle;
	const std::string MediaArtist = MediaState.m_aArtist;
	if(NeteaseLyrics::IsMeaningfulMediaIdentityChange(m_LastMediaTitle, m_LastMediaArtist, MediaTitle, MediaArtist))
	{
		// SMTC metadata 可能早于私有 songId 报告变化，必须立即丢弃旧句，
		// 并隔离旧 Bridge 身份，避免下一帧重新接受上一首歌词。
		m_BlockedBridgeSongId = m_LyricState.Snapshot().m_HasSong ? m_LyricState.Snapshot().m_SongId : 0;
		m_BlockedBridgeGeneration = m_LastBridgeGeneration;
		m_WaitingForBridgeIdentity = m_BlockedBridgeSongId != 0;
		m_LyricState.ClearLyrics();
		m_LastBridgeSequence = 0;
		m_LastBridgePositionMs = 0;
		m_LastBridgePositionValid = false;
	}
	if(!MediaTitle.empty())
	{
		const bool TitleChanged = !m_LastMediaTitle.empty() && MediaTitle != m_LastMediaTitle;
		m_LastMediaTitle = MediaTitle;
		if(TitleChanged || !MediaArtist.empty())
			m_LastMediaArtist = MediaArtist;
	}
	if(!HasBridgeSnapshot)
	{
		const uint64_t HookTimeout = (uint64_t)std::max(1, g_Config.m_QmNeteaseHookTimeoutMs);
		const uint64_t PausedGrace = std::max<uint64_t>(1500, HookTimeout);
		const NeteaseLyrics::SCurrentState &State = m_LyricState.Snapshot();
		if(NeteaseLyrics::ShouldPreservePausedLyric(
			   MediaState.m_PlaybackState == CSystemMediaControls::EPlaybackState::Paused,
			   State.m_HasSong,
			   State.m_LyricValid,
			   m_LastBridgeSnapshotTick,
			   MonotonicTickMs(),
			   PausedGrace))
			return;
		// 切歌后的阻断必须跨过短暂的 reader 失败；否则旧快照恢复时
		// 会再次被当成新歌接受。歌词已经清空，因此这里只保留阻断状态。
		if(m_WaitingForBridgeIdentity)
			return;
		if(State.m_HasSong)
			ClearForStaleMedia();
		return;
	}
	m_LastBridgeSnapshotTick = MonotonicTickMs();
	if(m_LastCloudMusicPid != 0 && BridgeSnapshot.m_CloudMusicPid != m_LastCloudMusicPid)
	{
		m_LyricState.ClearLyrics();
		m_LastBridgeSequence = 0;
		m_LastBridgeGeneration = 0;
		m_LastBridgePositionMs = 0;
		m_LastBridgePositionValid = false;
	}
	m_LastCloudMusicPid = BridgeSnapshot.m_CloudMusicPid;
	if(m_WaitingForBridgeIdentity)
	{
		const bool HasCandidateSong = (BridgeSnapshot.m_Flags & QmNeteaseHook::V5_FLAG_HAS_SONG) != 0 && BridgeSnapshot.m_SongId != 0;
		if(!HasCandidateSong)
		{
			// 空身份快照可能只是 Helper 切歌/重连的中间态，不能解除对旧
			// songId 的阻断。记录更高 generation 作为旧快照的下界。
			if(m_BlockedBridgeGeneration != 0 && BridgeSnapshot.m_Generation > m_BlockedBridgeGeneration)
				m_BlockedBridgeGeneration = BridgeSnapshot.m_Generation;
			m_LastBridgeSequence = BridgeSnapshot.m_Sequence;
			return;
		}
		if(NeteaseLyrics::IsBridgeIdentityStillBlocked(m_BlockedBridgeSongId, m_BlockedBridgeGeneration, BridgeSnapshot.m_SongId, BridgeSnapshot.m_Generation))
		{
			m_LastBridgeSequence = BridgeSnapshot.m_Sequence;
			return;
		}
		m_BlockedBridgeSongId = 0;
		m_BlockedBridgeGeneration = 0;
		m_WaitingForBridgeIdentity = false;
	}

	if((BridgeSnapshot.m_Flags & QmNeteaseHook::V5_FLAG_HAS_SONG) == 0 || BridgeSnapshot.m_SongId == 0 ||
		MediaState.m_PlaybackState == CSystemMediaControls::EPlaybackState::Stopped)
	{
		ClearForStaleMedia();
		return;
	}

	const bool SongChanged = m_LyricState.UpdateSong(BridgeSnapshot.m_SongId);
	if(SongChanged)
	{
		// UpdateSong 已经清空旧歌词；这一步必须发生在接受新快照之前。
		m_LastBridgeSequence = 0;
		m_LastBridgeGeneration = 0;
		m_LastBridgePositionMs = 0;
		m_LastBridgePositionValid = false;
	}
	if(m_LastBridgeGeneration != 0 && BridgeSnapshot.m_Generation != m_LastBridgeGeneration)
	{
		// Helper/DLL 重启后 generation 变化，即使 songId 恰好相同也不能
		// 把旧时间轴当成新进程的结果。
		m_LyricState.ClearLyrics();
		m_LastBridgeSequence = 0;
		m_LastBridgePositionMs = 0;
		m_LastBridgePositionValid = false;
	}
	m_LastBridgeGeneration = BridgeSnapshot.m_Generation;
	m_LyricState.AdoptGeneration(BridgeSnapshot.m_Generation);
	const bool PositionValid = (BridgeSnapshot.m_Flags & QmNeteaseHook::V5_FLAG_POSITION_VALID) != 0;
	const bool PositionAnchored = (BridgeSnapshot.m_Flags & QmNeteaseHook::V5_FLAG_POSITION_ANCHORED) != 0;
	const bool PausedPositionChanged = NeteaseLyrics::HasPausedLyricSeek(
		MediaState.m_PlaybackState == CSystemMediaControls::EPlaybackState::Paused,
		PositionValid,
		PositionAnchored,
		BridgeSnapshot.m_PositionMs,
		m_LastBridgePositionValid,
		m_LastBridgePositionMs);
	const bool KeepPausedLyric = MediaState.m_PlaybackState == CSystemMediaControls::EPlaybackState::Paused &&
				     m_LyricState.Snapshot().m_LyricValid && !SongChanged && !PausedPositionChanged;

	if(BridgeSnapshot.m_Sequence == m_LastBridgeSequence)
	{
		if((MediaState.m_Playing || PausedPositionChanged) && PositionValid)
			m_LyricState.UpdatePosition(BridgeSnapshot.m_PositionMs);
		if(PositionAnchored)
		{
			m_LastBridgePositionMs = BridgeSnapshot.m_PositionMs;
			m_LastBridgePositionValid = true;
		}
		return;
	}
	m_LastBridgeSequence = BridgeSnapshot.m_Sequence;
	if(PositionAnchored)
	{
		m_LastBridgePositionMs = BridgeSnapshot.m_PositionMs;
		m_LastBridgePositionValid = true;
	}
	if((BridgeSnapshot.m_Flags & QmNeteaseHook::V5_FLAG_LYRIC_VALID) == 0 || BridgeSnapshot.m_aCurrentLyric[0] == '\0')
	{
		if(!KeepPausedLyric)
			m_LyricState.ClearLyrics();
		return;
	}

	// SMTC 暂停时保留已经选中的句子；新歌曲仍可接受第一句，只有检测到
	// Bridge 进度锚点发生 seek 时才允许暂停快照重新定位当前句。
	if(KeepPausedLyric)
		return;

	NeteaseLyrics::ESource Source = NeteaseLyrics::ESource::Frontend;
	switch((QmNeteaseHook::ENeteaseLyricSource)BridgeSnapshot.m_LyricSource)
	{
	case QmNeteaseHook::ENeteaseLyricSource::DesktopLyricsFallback: Source = NeteaseLyrics::ESource::DesktopLyricsFallback; break;
	case QmNeteaseHook::ENeteaseLyricSource::Frontend: Source = NeteaseLyrics::ESource::Frontend; break;
	case QmNeteaseHook::ENeteaseLyricSource::None: return;
	case QmNeteaseHook::ENeteaseLyricSource::ReservedLegacyApi: return;
	}
	const int64_t PositionMs = PositionValid ? BridgeSnapshot.m_PositionMs : m_LyricState.Snapshot().m_PositionMs;
	m_LyricState.ApplyCurrentLine(Source, BridgeSnapshot.m_aCurrentLyric, BridgeSnapshot.m_LineStartMs, BridgeSnapshot.m_LineEndMs, PositionMs);
}

bool CNeteaseIntegration::GetCurrentLyric(char *pBuffer, size_t BufferSize, ColorRGBA *pColor) const
{
	if(pBuffer == nullptr || BufferSize == 0)
		return false;
	pBuffer[0] = '\0';
	if(!g_Config.m_QmLyrics || !g_Config.m_QmLyricsInMediaIsland)
		return false;
	const NeteaseLyrics::SCurrentState &State = m_LyricState.Snapshot();
	if(!State.m_HasSong || !State.m_LyricValid || State.m_CurrentLyric.empty())
		return false;
	QmNeteaseHook::CopyUtf8Truncated(pBuffer, BufferSize, State.m_CurrentLyric.data(), State.m_CurrentLyric.size());
	if(pColor != nullptr)
		*pColor = ColorRGBA(0.97f, 0.98f, 1.0f, 0.90f);
	return pBuffer[0] != '\0';
}

bool CNeteaseIntegration::HasCurrentLyric() const
{
	return g_Config.m_QmLyrics != 0 && g_Config.m_QmLyricsInMediaIsland != 0 && m_LyricState.Snapshot().m_LyricValid;
}

uint64_t CNeteaseIntegration::CurrentSongId() const
{
	return m_LyricState.Snapshot().m_SongId;
}

uint64_t CNeteaseIntegration::CurrentGeneration() const
{
	return m_LyricState.Snapshot().m_Generation;
}
