#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_SCORES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AXIOM_SCORES_H

#include "axiom_scores_data.h"

#include <engine/http.h>

#include <game/client/component.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

class IQmAxiomHttpRequest
{
public:
	virtual ~IQmAxiomHttpRequest() = default;
	virtual bool Done() const = 0;
	virtual bool TransportSucceeded() const = 0;
	virtual int StatusCode() const = 0;
	virtual void Result(const unsigned char **ppData, size_t *pDataSize) const = 0;
	virtual void Abort() = 0;
};

class IQmAxiomHttp
{
public:
	virtual ~IQmAxiomHttp() = default;
	virtual std::shared_ptr<IQmAxiomHttpRequest> Get(const char *pUrl, int ConnectTimeoutMs, int TimeoutMs, int64_t MaxResponseBytes) = 0;
};

enum class EQmAxiomScoreStatus
{
	NOT_REQUESTED,
	FETCHING,
	READY,
	NOT_FOUND,
	AMBIGUOUS,
	HTTP_ERROR,
	API_ERROR,
	INVALID_RESPONSE,
};

struct SQmAxiomModeResult
{
	EQmAxiomScoreStatus m_Status = EQmAxiomScoreStatus::NOT_REQUESTED;
	SQmAxiomModeScore m_Score;
};

struct SQmAxiomPlayerResult
{
	EQmAxiomScoreStatus m_SearchStatus = EQmAxiomScoreStatus::NOT_REQUESTED;
	SQmAxiomSearchMatch m_Match;
	std::array<SQmAxiomModeResult, 2> m_aModes;

	const SQmAxiomModeResult &Mode(EQmAxiomMode Mode) const;
};

class CQmAxiomScores : public CComponent
{
	struct SCacheEntry
	{
		SQmAxiomPlayerResult m_Result;
		int64_t m_LastSearchSuccessTick = 0;
		int64_t m_LastSearchFailureTick = 0;
		std::array<int64_t, 2> m_aLastModeSuccessTick{};
		std::array<int64_t, 2> m_aLastModeFailureTick{};
		int64_t m_LastAccessTick = 0;
	};

	struct SRequestSlot
	{
		std::shared_ptr<IQmAxiomHttpRequest> m_pRequest;
		uint64_t m_Generation = 0;
		std::string m_PlayerName;
	};

	std::map<std::string, SCacheEntry> m_Cache;
	SRequestSlot m_SearchRequest;
	std::array<SRequestSlot, 2> m_aModeRequests;
	std::string m_ActivePlayerName;
	uint64_t m_Generation = 0;
	IQmAxiomHttp *m_pHttpOverride = nullptr;

	static int ModeIndex(EQmAxiomMode Mode);
	static EQmAxiomMode ModeFromIndex(int Index);
	static bool IsFailureStatus(EQmAxiomScoreStatus Status);
	static bool IsWithinWindow(int64_t Timestamp, int64_t Now, int64_t WindowMs);

	void AbortActiveRequests(bool ResetFetchingStates);
	void BeginActiveQuery(const char *pPlayerName);
	void EvictCacheEntryIfNeeded();
	void StartSearchRequest(const char *pPlayerName, SCacheEntry &Entry);
	void StartModeRequest(const char *pPlayerName, SCacheEntry &Entry, EQmAxiomMode Mode);
	void ProcessSearchRequest();
	void ProcessModeRequests();
	void FinishActiveQueryIfIdle();
	std::shared_ptr<IQmAxiomHttpRequest> StartRequest(const char *pUrl, int TimeoutMs);

protected:
	virtual int64_t CurrentTick() const;

public:
	CQmAxiomScores() = default;
	explicit CQmAxiomScores(IQmAxiomHttp *pHttpOverride) :
		m_pHttpOverride(pHttpOverride)
	{
	}
	int Sizeof() const override { return sizeof(*this); }
	void OnUpdate() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnStateChange(int NewState, int OldState) override;

	void EnsureQueried(const char *pPlayerName);
	const SQmAxiomPlayerResult *GetResult(const char *pPlayerName) const;
};

#endif
