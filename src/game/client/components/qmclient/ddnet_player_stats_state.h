#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_DDNET_PLAYER_STATS_STATE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_DDNET_PLAYER_STATS_STATE_H

#include <cstdint>
#include <string>

enum class EQmDdnetPlayerStatsPhase
{
	IDLE,
	HTTP,
	PARSING,
};

enum class EQmDdnetPlayerStatsRefreshAction
{
	START_REQUEST,
	WAIT_FOR_PARSE,
};

class CQmDdnetPlayerStatsState
{
	std::string m_PlayerName;
	std::string m_RequestPlayerName;
	EQmDdnetPlayerStatsPhase m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
	int64_t m_LastSync = 0;
	int64_t m_NextRetry = 0;
	bool m_RefreshPending = false;

public:
	void SetPlayer(const char *pPlayerName)
	{
		const std::string PlayerName = pPlayerName ? pPlayerName : "";
		if(m_PlayerName == PlayerName)
			return;
		m_PlayerName = PlayerName;
		m_LastSync = 0;
		m_NextRetry = 0;
		m_RefreshPending = false;
		if(m_Phase == EQmDdnetPlayerStatsPhase::HTTP)
		{
			m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
			m_RequestPlayerName.clear();
		}
	}

	void Reset()
	{
		m_PlayerName.clear();
		m_RequestPlayerName.clear();
		m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
		m_LastSync = 0;
		m_NextRetry = 0;
		m_RefreshPending = false;
	}

	const std::string &PlayerName() const { return m_PlayerName; }
	const std::string &RequestPlayerName() const { return m_RequestPlayerName; }
	EQmDdnetPlayerStatsPhase Phase() const { return m_Phase; }
	int64_t LastSync() const { return m_LastSync; }
	int64_t NextRetry() const { return m_NextRetry; }
	bool RefreshPending() const { return m_RefreshPending; }

	bool ShouldFetch(int64_t Now, int64_t SyncIntervalTicks) const
	{
		if(m_PlayerName.empty() || m_Phase != EQmDdnetPlayerStatsPhase::IDLE)
			return false;
		if(m_NextRetry != 0)
			return Now >= m_NextRetry;
		return m_LastSync == 0 || Now - m_LastSync >= SyncIntervalTicks;
	}

	void BeginHttp(const char *pPlayerName)
	{
		m_PlayerName = pPlayerName ? pPlayerName : "";
		m_RequestPlayerName = m_PlayerName;
		m_Phase = EQmDdnetPlayerStatsPhase::HTTP;
	}

	void AbortHttp()
	{
		if(m_Phase != EQmDdnetPlayerStatsPhase::HTTP)
			return;
		m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
		m_RequestPlayerName.clear();
	}

	void CompleteHttp(bool Success, int64_t Now, int64_t RetryDelayTicks)
	{
		if(m_Phase != EQmDdnetPlayerStatsPhase::HTTP)
			return;
		if(Success)
		{
			m_Phase = EQmDdnetPlayerStatsPhase::PARSING;
			return;
		}
		m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
		m_RequestPlayerName.clear();
		m_LastSync = 0;
		m_NextRetry = Now + RetryDelayTicks;
	}

	EQmDdnetPlayerStatsRefreshAction RequestRefresh()
	{
		if(m_Phase == EQmDdnetPlayerStatsPhase::PARSING)
		{
			m_RefreshPending = true;
			return EQmDdnetPlayerStatsRefreshAction::WAIT_FOR_PARSE;
		}
		m_LastSync = 0;
		m_NextRetry = 0;
		return EQmDdnetPlayerStatsRefreshAction::START_REQUEST;
	}

	bool CompleteParse(const std::string &RequestPlayerName, bool Parsed, int64_t Now, int64_t RetryDelayTicks, bool &StartRefresh)
	{
		StartRefresh = false;
		if(m_Phase != EQmDdnetPlayerStatsPhase::PARSING || m_RequestPlayerName != RequestPlayerName)
			return false;

		m_Phase = EQmDdnetPlayerStatsPhase::IDLE;
		m_RequestPlayerName.clear();
		const bool CurrentPlayer = m_PlayerName == RequestPlayerName;
		const bool RefreshPending = m_RefreshPending;
		m_RefreshPending = false;
		if(!CurrentPlayer)
			return true;

		if(Parsed)
		{
			m_LastSync = Now;
			m_NextRetry = 0;
		}
		else
		{
			m_LastSync = 0;
			m_NextRetry = Now + RetryDelayTicks;
		}
		if(RefreshPending)
		{
			m_LastSync = 0;
			m_NextRetry = 0;
			StartRefresh = true;
		}
		return true;
	}
};

#endif
