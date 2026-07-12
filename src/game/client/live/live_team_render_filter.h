// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_LIVE_LIVE_TEAM_RENDER_FILTER_H
#define GAME_CLIENT_LIVE_LIVE_TEAM_RENDER_FILTER_H

#include <engine/shared/protocol.h>

#include <array>

class CLiveTeamRenderFilter
{
public:
	static bool IsValidDDRaceTeam(int Team);

	void Reset();
	bool SetTeam(int Team);
	void Disable();
	void UpdateTeams(const std::array<int, MAX_CLIENTS> &aTeams);

	bool Active() const { return m_Active; }
	int Team() const { return m_Team; }

	void SetPreviewEnabled(bool Enabled) { m_PreviewEnabled = Enabled; }
	void SetAudioEnabled(bool Enabled) { m_AudioEnabled = Enabled; }
	void SetHideExternalFinish(bool Enabled) { m_HideExternalFinish = Enabled; }
	void SetStrictUnknownEvents(bool Enabled) { m_StrictUnknownEvents = Enabled; }

	bool PreviewEnabled() const { return m_PreviewEnabled; }
	bool AudioEnabled() const { return m_AudioEnabled; }
	bool HideExternalFinish() const { return m_HideExternalFinish; }
	bool StrictUnknownEvents() const { return m_StrictUnknownEvents; }

	bool AllowsTeam(int Team) const;
	bool AllowsClient(int ClientId) const;
	bool AllowsKnownOwner(int ClientId) const;
	bool AllowsUnknownPlayerEvent() const;
	bool AllowsFinishForTeam(int Team) const;

	bool ObservePlaybackTick(int Tick);
	void MarkTransientReset() { ++m_ResetSerial; }
	int ResetSerial() const { return m_ResetSerial; }

private:
	std::array<int, MAX_CLIENTS> m_aTeams{};
	int m_Team = -1;
	int m_LastPlaybackTick = -1;
	int m_ResetSerial = 0;
	bool m_Active = false;
	bool m_PreviewEnabled = true;
	bool m_AudioEnabled = true;
	bool m_HideExternalFinish = true;
	bool m_StrictUnknownEvents = true;
};

#endif // GAME_CLIENT_LIVE_LIVE_TEAM_RENDER_FILTER_H
