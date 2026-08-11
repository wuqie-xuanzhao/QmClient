// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_LIVE_LIVE_DIRECTOR_H
#define GAME_CLIENT_LIVE_LIVE_DIRECTOR_H

#include <engine/shared/protocol.h>
#include <engine/shared/qm_live_observer_session.h>

#include <array>
#include <vector>

class CLiveDirector
{
public:
	enum class EEntryType
	{
		DDRACE_TEAM,
		PLAYER,
	};

	class CEntry
	{
	public:
		EEntryType m_Type = EEntryType::PLAYER;
		int m_Team = -1;
		int m_ClientId = -1;
		int m_NumPlayers = 0;
	};

	void Reset();
	void UpdateEntries(const std::array<int, MAX_CLIENTS> &aTeams, const std::array<bool, MAX_CLIENTS> &aActivePlayers);
	bool HasEntries() const { return !m_vEntries.empty(); }
	bool HasDDRaceTeams() const { return m_HasDDRaceTeams; }
	const std::vector<CEntry> &Entries() const { return m_vEntries; }

	int SelectRandomTeam(unsigned Seed) const;
	int SelectRandomPlayer(unsigned Seed) const;
	int FallbackPlayer() const { return m_FallbackPlayer; }

	void SetMode(CLiveObserverSession::EDirectorMode Mode) { m_Mode = Mode; }
	CLiveObserverSession::EDirectorMode Mode() const { return m_Mode; }

private:
	std::vector<CEntry> m_vEntries;
	int m_FallbackPlayer = -1;
	bool m_HasDDRaceTeams = false;
	CLiveObserverSession::EDirectorMode m_Mode = CLiveObserverSession::EDirectorMode::FREEVIEW;
};

#endif // GAME_CLIENT_LIVE_LIVE_DIRECTOR_H
