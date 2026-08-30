// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_H

#include "qmclient_utils.h"

#include <base/hash.h>

#include <engine/http.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <memory>
#include <mutex>

class IJob;

struct SQmClientLocalModeStats
{
	std::string m_GameMode;
	std::string m_CommunityId;
	bool m_IsAxiom = false;
	int m_Maps = 0;
	int64_t m_Score = 0;
	int64_t m_PlaytimeSeconds = 0;
};

class CQmClient : public CComponent
{
	std::shared_ptr<IHttpRequest> m_pQmClientAuthTokenTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientUsersTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientUsersSendTask = nullptr;
	std::shared_ptr<IJob> m_pQmClientUsersParseJob = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmDeveloperPresenceTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmDeveloperPresencesTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientLifecycleStartTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientLifecycleCrashTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientLifecycleStopTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientServerTimeTask = nullptr;
	std::shared_ptr<IHttpRequest> m_pQmClientPlaytimeQueryTask = nullptr;
	std::shared_ptr<IJob> m_pQmClientLifecycleMarkerWriteJob = nullptr;
	std::shared_ptr<std::mutex> m_pQmClientLifecycleMarkerMutex = std::make_shared<std::mutex>();
	std::shared_ptr<IHttpRequest> m_pQmDdnetPlayerTask = nullptr;
	std::shared_ptr<IJob> m_pQmDdnetPlayerParseJob = nullptr;

	char m_aQmClientAuthToken[256] = "";
	char m_aQmClientMachineHash[SHA256_MAXSTRSIZE] = "";
	char m_aQmClientLifecycleSessionId[64] = "";
	char m_aQmClientPlaytimeClientId[65] = "";
	char m_aQmDdnetPlayerName[MAX_NAME_LENGTH] = "";
	char m_aQmDdnetFavoritePartner[MAX_NAME_LENGTH] = "";
	char m_aQmClientPendingVoicePresenceServerAddress[NETADDR_MAXSTRSIZE] = "";
	char m_aQmDeveloperToken[65] = "";
	char m_aQmDeveloperSessionId[33] = "";
	char m_aQmDeveloperPendingServerAddress[NETADDR_MAXSTRSIZE] = "";

	int64_t m_QmClientLastSync = 0;
	int64_t m_QmDeveloperLastSync = 0;
	int64_t m_QmClientServerNow = 0;
	int64_t m_QmClientServerSessionStart = 0;
	int64_t m_QmClientServerTimeLastSync = 0;
	int64_t m_QmClientServerPlaytimeSeconds = -1;
	int64_t m_QmClientPlaytimeLastSync = 0;
	int64_t m_QmClientRecoveryStopAt = 0;
	int64_t m_QmClientRecoveryNextRetry = 0;
	int64_t m_QmClientStartupNextRetry = 0;
	int64_t m_QmClientMarkerStartedAt = 0;
	int64_t m_QmClientMarkerLastSeenAt = 0;
	int64_t m_QmClientMarkerLastFlushTick = 0;
	int64_t m_QmDdnetPlayerLastSync = 0;
	int64_t m_QmDdnetPlayerNextRetry = 0;
	int m_QmClientOnlineUserCount = 0;
	int m_QmClientOnlineDummyCount = 0;
	int m_QmDdnetTotalFinishes = -1;
	int64_t m_QmDdnetPoints = -1;
	int64_t m_QmDdnetPointsTotal = -1;
	mutable bool m_QmStatisticsFileExists = false;
	int m_QmClientPendingVoicePresencePlayers = 0;
	bool m_QmClientDistributionSuccessLatched = false;
	bool m_QmClientShutdownReported = false;
	bool m_QmClientAwaitingRecoveryStop = false;
	bool m_QmClientStartupSent = false;
	std::vector<SQmClientServerDistribution> m_vQmClientServerDistribution;
	std::vector<SQmClientLocalModeStats> m_vQmClientLocalModeStats;
	std::string m_QmClientActiveLocalMode;
	std::string m_QmClientActiveLocalCommunityId;
	bool m_QmClientActiveLocalIsAxiom = false;
	int64_t m_QmClientLocalModeLastTimestamp = 0;

	void InitQmClientLifecycle();
	void UpdateQmClientLifecycleAndServerTime();
	void SendQmClientLifecyclePing(const char *pEvent, std::shared_ptr<IHttpRequest> &pTaskSlot);
	bool FinishQmClientPlaytimeTask(std::shared_ptr<IHttpRequest> &pTaskSlot, bool UpdateSessionStart);
	void FinishQmClientServerTimeTask();
	void SendQmClientPlaytimeRequest(const char *pUrl, std::shared_ptr<IHttpRequest> &pTaskSlot, int64_t StopAt = 0);
	void EnsureQmClientPlaytimeClientId();
	bool ReadQmClientLifecycleMarker(int64_t &OutStartedAt, int64_t &OutLastSeenAt);
	void TouchQmClientLifecycleMarker(bool ForceWrite);
	void WriteQmClientLifecycleMarker();
	void ClearQmClientLifecycleMarker();

	void UpdateQmClientRecognition();
	void SyncQmClientUsers();
	void FetchQmClientAuthToken();
	void SendQmClientPlayerData();
	void FetchQmClientUsers();
	void FinishQmClientAuthToken();
	void FinishQmClientUsers();
	void ResetQmClientRecognitionTasks();
	bool NeedsQmClientRecognition() const;
	bool NeedsFastQmClientSync() const;
	bool EnsureQmClientMachineHash();
	bool BuildQmClientRecognitionUrl(const char *pPath, char *pBuf, size_t BufSize, const char *pQuery = nullptr) const;
	void ClearQmClientServerDistribution();
	void InitQmDeveloperAuthentication();
	void UpdateQmDeveloperPresence();
	void SendQmDeveloperPresence(const char *pServerAddress);
	void FetchQmDeveloperPresences(const char *pServerAddress);
	void FinishQmDeveloperPresences(const char *pServerAddress);
	void ResetQmDeveloperPresenceTasks();

	void UpdateQmDdnetPlayerStats();
	void FetchQmDdnetPlayerStats(const char *pPlayerName);
	void FinishQmDdnetPlayerStats();
	void LoadQmClientLocalModeStats();
	void UpdateQmClientLocalModePlaytime();
	void AccumulateQmClientLocalModePlaytime(int64_t Now);
	void EndQmClientLocalModePlaytime();
	void RefreshQmDdnetPlayerStats();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;
	void OnStateChange(int NewState, int OldState) override;

	bool HasQmClientRecognitionService() const;
	bool HasQmServerTime() const { return m_QmClientServerNow > 0; }
	int64_t QmServerTimeNow() const { return m_QmClientServerNow; }
	int64_t QmServerSessionStartTime() const { return m_QmClientServerSessionStart; }
	bool HasQmServerPlaytime() const { return m_QmClientServerPlaytimeSeconds >= 0; }
	int64_t QmServerPlaytimeSeconds() const { return m_QmClientServerPlaytimeSeconds; }
	const std::vector<SQmClientServerDistribution> &QmClientServerDistribution() const { return m_vQmClientServerDistribution; }
	int QmClientOnlineUserCount() const { return m_QmClientOnlineUserCount; }
	int QmClientOnlineDummyCount() const { return m_QmClientOnlineDummyCount; }
	int QmDdnetTotalFinishes() const { return m_QmDdnetTotalFinishes; }
	int64_t QmDdnetPoints() const { return m_QmDdnetPoints; }
	int64_t QmDdnetPointsTotal() const { return m_QmDdnetPointsTotal; }
	const char *QmDdnetPlayerName() const { return m_aQmDdnetPlayerName; }
	const char *QmDdnetFavoritePartner() const { return m_aQmDdnetFavoritePartner; }
	const std::vector<SQmClientLocalModeStats> &QmClientLocalModeStats() const { return m_vQmClientLocalModeStats; }
	void SaveQmClientStatistics() const;
	void RecordQmClientLocalMapFinish(const char *pGameMode, int Score);
	void RefreshQmClientStatistics();
};

#endif
