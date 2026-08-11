// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_LIVE_LIVE_REPLAY_SIDECAR_H
#define GAME_CLIENT_LIVE_LIVE_REPLAY_SIDECAR_H

#include <base/hash.h>
#include <base/system.h>

#include <engine/storage.h>

#include <string>
#include <vector>

struct SLiveReplayFinishEvent
{
	int m_Tick = -1;
	int m_Team = -1;
	int m_Time = 0;
	int m_ClientId = -1;
};

struct SLiveReplayTeamEvent
{
	int m_Tick = -1;
	int m_ClientId = -1;
	int m_OldTeam = -1;
	int m_NewTeam = -1;
};

struct SLiveReplaySidecarData
{
	static constexpr int FORMAT_VERSION = 1;

	int m_FormatVersion = FORMAT_VERSION;
	char m_aDemoFilename[IO_MAX_PATH_LENGTH] = "";
	char m_aMapName[64] = "";
	SHA256_DIGEST m_MapSha256 = SHA256_DIGEST{};
	unsigned m_MapCrc = 0;
	int m_StartTick = -1;
	int m_EndTick = -1;
	std::vector<SLiveReplayFinishEvent> m_vFinishEvents;
	std::vector<SLiveReplayTeamEvent> m_vTeamEvents;
};

class CLiveReplaySidecar
{
public:
	void Reset();
	void Start(const char *pDemoFilename, const char *pMapName, SHA256_DIGEST MapSha256, unsigned MapCrc, int StartTick);
	void SetEndTick(int EndTick);

	bool Active() const { return m_Active; }
	const SLiveReplaySidecarData &Data() const { return m_Data; }

	bool AddFinishEvent(int Tick, int Team, int Time, int ClientId);
	bool AddTeamEvent(int Tick, int ClientId, int OldTeam, int NewTeam);

	std::string BuildJson() const;
	bool WriteAtomic(IStorage *pStorage, const char *pFilename) const;

	static bool SidecarPathForDemo(const char *pDemoFilename, char *pBuffer, int BufferSize);
	static bool LoadFromString(const char *pJson, SLiveReplaySidecarData &Out, char *pError, int ErrorSize);
	static bool MatchesDemo(const SLiveReplaySidecarData &Data, const char *pDemoFilename, const char *pMapName, SHA256_DIGEST MapSha256, unsigned MapCrc);

private:
	SLiveReplaySidecarData m_Data;
	bool m_Active = false;
};

#endif // GAME_CLIENT_LIVE_LIVE_REPLAY_SIDECAR_H
