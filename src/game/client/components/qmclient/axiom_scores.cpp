#include "axiom_scores.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/http.h>

#include <algorithm>
#include <utility>

namespace
{
	constexpr int64_t AXIOM_SCORE_CACHE_TTL_MS = 30 * 60 * 1000;
	constexpr int64_t AXIOM_SEARCH_CACHE_TTL_MS = 2 * 60 * 60 * 1000;
	constexpr int64_t AXIOM_FAILURE_RETRY_MS = 30 * 1000;
	constexpr int64_t AXIOM_MAX_RESPONSE_BYTES = 8 * 1024 * 1024;
	constexpr size_t AXIOM_MAX_CACHE_ENTRIES = 64;
	constexpr size_t AXIOM_MAX_QUERY_NAME_BYTES = 256;
	constexpr int AXIOM_CONNECT_TIMEOUT_MS = 5000;
	constexpr int AXIOM_SEARCH_TIMEOUT_MS = 10000;
	constexpr int AXIOM_INFO_TIMEOUT_MS = 45000;

	EQmAxiomScoreStatus ParseStatus(EQmAxiomParseResult Result)
	{
		switch(Result)
		{
		case EQmAxiomParseResult::SUCCESS: return EQmAxiomScoreStatus::READY;
		case EQmAxiomParseResult::NOT_FOUND: return EQmAxiomScoreStatus::NOT_FOUND;
		case EQmAxiomParseResult::AMBIGUOUS: return EQmAxiomScoreStatus::AMBIGUOUS;
		case EQmAxiomParseResult::API_ERROR: return EQmAxiomScoreStatus::API_ERROR;
		case EQmAxiomParseResult::INVALID_RESPONSE: return EQmAxiomScoreStatus::INVALID_RESPONSE;
		}
		return EQmAxiomScoreStatus::INVALID_RESPONSE;
	}

	void PrepareRequest(CHttpRequest *pRequest, int TimeoutMs)
	{
		pRequest->Timeout(CTimeout{AXIOM_CONNECT_TIMEOUT_MS, TimeoutMs, 0, 0});
		pRequest->MaxResponseSize(AXIOM_MAX_RESPONSE_BYTES);
		pRequest->LogProgress(HTTPLOG::FAILURE);
		pRequest->FailOnErrorStatus(false);
		pRequest->HeaderString("Accept", "application/json");
		pRequest->HeaderString("User-Agent", "QmClient (https://github.com/wxj881027/QmClient)");
	}

	class CNativeAxiomHttpRequest final : public IQmAxiomHttpRequest
	{
		std::shared_ptr<CHttpRequest> m_pRequest;

	public:
		explicit CNativeAxiomHttpRequest(std::shared_ptr<CHttpRequest> pRequest) :
			m_pRequest(std::move(pRequest))
		{
		}

		bool Done() const override { return m_pRequest->Done(); }
		bool TransportSucceeded() const override { return m_pRequest->State() == EHttpState::DONE; }
		int StatusCode() const override { return m_pRequest->StatusCode(); }
		void Result(const unsigned char **ppData, size_t *pDataSize) const override
		{
			unsigned char *pData = nullptr;
			m_pRequest->Result(&pData, pDataSize);
			*ppData = pData;
		}
		void Abort() override { m_pRequest->Abort(); }
	};
}

const SQmAxiomModeResult &SQmAxiomPlayerResult::Mode(EQmAxiomMode Mode) const
{
	return m_aModes[Mode == EQmAxiomMode::AXRACE ? 1 : 0];
}

int CQmAxiomScores::ModeIndex(EQmAxiomMode Mode)
{
	return Mode == EQmAxiomMode::AXRACE ? 1 : 0;
}

EQmAxiomMode CQmAxiomScores::ModeFromIndex(int Index)
{
	return Index == 1 ? EQmAxiomMode::AXRACE : EQmAxiomMode::GORES;
}

bool CQmAxiomScores::IsFailureStatus(EQmAxiomScoreStatus Status)
{
	return Status == EQmAxiomScoreStatus::NOT_FOUND ||
	       Status == EQmAxiomScoreStatus::AMBIGUOUS ||
	       Status == EQmAxiomScoreStatus::HTTP_ERROR ||
	       Status == EQmAxiomScoreStatus::API_ERROR ||
	       Status == EQmAxiomScoreStatus::INVALID_RESPONSE;
}

bool CQmAxiomScores::IsWithinWindow(int64_t Timestamp, int64_t Now, int64_t WindowMs)
{
	if(Timestamp <= 0 || Now < Timestamp)
		return false;
	return Now - Timestamp < WindowMs * time_freq() / 1000;
}

int64_t CQmAxiomScores::CurrentTick() const
{
	return time_get();
}

std::shared_ptr<IQmAxiomHttpRequest> CQmAxiomScores::StartRequest(const char *pUrl, int TimeoutMs)
{
	if(m_pHttpOverride != nullptr)
		return m_pHttpOverride->Get(pUrl, AXIOM_CONNECT_TIMEOUT_MS, TimeoutMs, AXIOM_MAX_RESPONSE_BYTES);
	if(Http() == nullptr)
		return nullptr;

	auto pRequest = std::make_shared<CHttpRequest>(pUrl);
	PrepareRequest(pRequest.get(), TimeoutMs);
	Http()->Run(pRequest);
	return std::make_shared<CNativeAxiomHttpRequest>(std::move(pRequest));
}

void CQmAxiomScores::AbortActiveRequests(bool ResetFetchingStates)
{
	SCacheEntry *pEntry = nullptr;
	const auto CacheIt = m_Cache.find(m_ActivePlayerName);
	if(CacheIt != m_Cache.end())
		pEntry = &CacheIt->second;

	if(m_SearchRequest.m_pRequest)
	{
		m_SearchRequest.m_pRequest->Abort();
		m_SearchRequest.m_pRequest.reset();
		if(ResetFetchingStates && pEntry && pEntry->m_Result.m_SearchStatus == EQmAxiomScoreStatus::FETCHING)
			pEntry->m_Result.m_SearchStatus = EQmAxiomScoreStatus::NOT_REQUESTED;
	}
	for(int Index = 0; Index < (int)m_aModeRequests.size(); ++Index)
	{
		SRequestSlot &Slot = m_aModeRequests[Index];
		if(!Slot.m_pRequest)
			continue;
		Slot.m_pRequest->Abort();
		Slot.m_pRequest.reset();
		if(ResetFetchingStates && pEntry && pEntry->m_Result.m_aModes[Index].m_Status == EQmAxiomScoreStatus::FETCHING)
			pEntry->m_Result.m_aModes[Index].m_Status = EQmAxiomScoreStatus::NOT_REQUESTED;
	}
	m_SearchRequest.m_PlayerName.clear();
	for(SRequestSlot &Slot : m_aModeRequests)
		Slot.m_PlayerName.clear();
	m_ActivePlayerName.clear();
	++m_Generation;
}

void CQmAxiomScores::BeginActiveQuery(const char *pPlayerName)
{
	if(m_ActivePlayerName == pPlayerName)
		return;
	AbortActiveRequests(true);
	m_ActivePlayerName = pPlayerName;
}

void CQmAxiomScores::EvictCacheEntryIfNeeded()
{
	if(m_Cache.size() < AXIOM_MAX_CACHE_ENTRIES)
		return;
	auto Oldest = m_Cache.end();
	for(auto It = m_Cache.begin(); It != m_Cache.end(); ++It)
	{
		if(It->first == m_ActivePlayerName)
			continue;
		if(Oldest == m_Cache.end() || It->second.m_LastAccessTick < Oldest->second.m_LastAccessTick)
			Oldest = It;
	}
	if(Oldest != m_Cache.end())
		m_Cache.erase(Oldest);
}

void CQmAxiomScores::StartSearchRequest(const char *pPlayerName, SCacheEntry &Entry)
{
	Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::FETCHING;
	Entry.m_Result.m_Match = {};
	for(SQmAxiomModeResult &ModeResult : Entry.m_Result.m_aModes)
		ModeResult = {};

	const std::string Url = QmBuildAxiomSearchUrl(pPlayerName);
	if(Url.empty())
	{
		Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::INVALID_RESPONSE;
		Entry.m_LastSearchFailureTick = CurrentTick();
		return;
	}

	m_SearchRequest.m_pRequest = StartRequest(Url.c_str(), AXIOM_SEARCH_TIMEOUT_MS);
	if(!m_SearchRequest.m_pRequest)
	{
		Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::HTTP_ERROR;
		Entry.m_LastSearchFailureTick = CurrentTick();
		return;
	}
	m_SearchRequest.m_Generation = m_Generation;
	m_SearchRequest.m_PlayerName = pPlayerName;
}

void CQmAxiomScores::StartModeRequest(const char *pPlayerName, SCacheEntry &Entry, EQmAxiomMode Mode)
{
	const int Index = ModeIndex(Mode);
	SQmAxiomModeResult &ModeResult = Entry.m_Result.m_aModes[Index];
	ModeResult.m_Status = EQmAxiomScoreStatus::FETCHING;
	ModeResult.m_Score = {};

	if(Entry.m_Result.m_Match.m_UserId <= 0)
	{
		ModeResult.m_Status = EQmAxiomScoreStatus::HTTP_ERROR;
		Entry.m_aLastModeFailureTick[Index] = CurrentTick();
		return;
	}

	SRequestSlot &Slot = m_aModeRequests[Index];
	const std::string Url = QmBuildAxiomInfoUrl(Entry.m_Result.m_Match.m_UserId, Mode);
	Slot.m_pRequest = StartRequest(Url.c_str(), AXIOM_INFO_TIMEOUT_MS);
	if(!Slot.m_pRequest)
	{
		ModeResult.m_Status = EQmAxiomScoreStatus::HTTP_ERROR;
		Entry.m_aLastModeFailureTick[Index] = CurrentTick();
		return;
	}
	Slot.m_Generation = m_Generation;
	Slot.m_PlayerName = pPlayerName;
}

void CQmAxiomScores::ProcessSearchRequest()
{
	if(!m_SearchRequest.m_pRequest || !m_SearchRequest.m_pRequest->Done())
		return;

	std::shared_ptr<IQmAxiomHttpRequest> pRequest = std::move(m_SearchRequest.m_pRequest);
	const uint64_t ResponseGeneration = m_SearchRequest.m_Generation;
	const std::string ResponsePlayerName = std::move(m_SearchRequest.m_PlayerName);
	if(!QmAxiomResponseIsCurrent(m_Generation, ResponseGeneration, m_ActivePlayerName, ResponsePlayerName))
		return;

	const auto CacheIt = m_Cache.find(ResponsePlayerName);
	if(CacheIt == m_Cache.end())
		return;
	SCacheEntry &Entry = CacheIt->second;
	const int64_t Now = CurrentTick();

	if(!pRequest->TransportSucceeded() || pRequest->StatusCode() != 200)
	{
		Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::HTTP_ERROR;
		Entry.m_LastSearchFailureTick = Now;
		return;
	}

	const unsigned char *pData = nullptr;
	size_t DataSize = 0;
	pRequest->Result(&pData, &DataSize);
	SQmAxiomSearchMatch Match;
	const EQmAxiomParseResult ParseResult = QmParseAxiomSearchResponse(reinterpret_cast<const char *>(pData), DataSize, ResponsePlayerName.c_str(), Match);
	Entry.m_Result.m_SearchStatus = ParseStatus(ParseResult);
	if(ParseResult != EQmAxiomParseResult::SUCCESS)
	{
		Entry.m_LastSearchFailureTick = Now;
		return;
	}

	Entry.m_Result.m_Match = std::move(Match);
	Entry.m_LastSearchSuccessTick = Now;
	Entry.m_LastSearchFailureTick = 0;
	StartModeRequest(ResponsePlayerName.c_str(), Entry, EQmAxiomMode::GORES);
	StartModeRequest(ResponsePlayerName.c_str(), Entry, EQmAxiomMode::AXRACE);
}

void CQmAxiomScores::ProcessModeRequests()
{
	for(int Index = 0; Index < (int)m_aModeRequests.size(); ++Index)
	{
		SRequestSlot &Slot = m_aModeRequests[Index];
		if(!Slot.m_pRequest || !Slot.m_pRequest->Done())
			continue;

		std::shared_ptr<IQmAxiomHttpRequest> pRequest = std::move(Slot.m_pRequest);
		const uint64_t ResponseGeneration = Slot.m_Generation;
		const std::string ResponsePlayerName = std::move(Slot.m_PlayerName);
		if(!QmAxiomResponseIsCurrent(m_Generation, ResponseGeneration, m_ActivePlayerName, ResponsePlayerName))
			continue;

		const auto CacheIt = m_Cache.find(ResponsePlayerName);
		if(CacheIt == m_Cache.end())
			continue;
		SCacheEntry &Entry = CacheIt->second;
		SQmAxiomModeResult &ModeResult = Entry.m_Result.m_aModes[Index];
		const int64_t Now = CurrentTick();

		if(!pRequest->TransportSucceeded() || pRequest->StatusCode() != 200)
		{
			ModeResult.m_Status = EQmAxiomScoreStatus::HTTP_ERROR;
			Entry.m_aLastModeFailureTick[Index] = Now;
			continue;
		}

		const unsigned char *pData = nullptr;
		size_t DataSize = 0;
		pRequest->Result(&pData, &DataSize);
		SQmAxiomModeScore Score;
		const EQmAxiomParseResult ParseResult = QmParseAxiomInfoResponse(reinterpret_cast<const char *>(pData), DataSize, Score);
		ModeResult.m_Status = ParseStatus(ParseResult);
		if(ParseResult != EQmAxiomParseResult::SUCCESS || Score.m_PlayerName != Entry.m_Result.m_Match.m_PlayerName)
		{
			if(ParseResult == EQmAxiomParseResult::SUCCESS)
				ModeResult.m_Status = EQmAxiomScoreStatus::INVALID_RESPONSE;
			Entry.m_aLastModeFailureTick[Index] = Now;
			continue;
		}

		ModeResult.m_Score = std::move(Score);
		Entry.m_aLastModeSuccessTick[Index] = Now;
		Entry.m_aLastModeFailureTick[Index] = 0;
	}
}

void CQmAxiomScores::FinishActiveQueryIfIdle()
{
	if(m_SearchRequest.m_pRequest)
		return;
	for(const SRequestSlot &Slot : m_aModeRequests)
	{
		if(Slot.m_pRequest)
			return;
	}
	m_ActivePlayerName.clear();
}

void CQmAxiomScores::EnsureQueried(const char *pPlayerName)
{
	if(!pPlayerName || pPlayerName[0] == '\0' || str_length(pPlayerName) > AXIOM_MAX_QUERY_NAME_BYTES || !str_utf8_check(pPlayerName))
		return;

	const int64_t Now = CurrentTick();
	auto CacheIt = m_Cache.find(pPlayerName);
	if(CacheIt == m_Cache.end())
	{
		EvictCacheEntryIfNeeded();
		CacheIt = m_Cache.emplace(pPlayerName, SCacheEntry{}).first;
	}
	SCacheEntry &Entry = CacheIt->second;
	Entry.m_LastAccessTick = Now;

	const bool SearchFresh = Entry.m_Result.m_SearchStatus == EQmAxiomScoreStatus::READY && IsWithinWindow(Entry.m_LastSearchSuccessTick, Now, AXIOM_SEARCH_CACHE_TTL_MS);
	if(!SearchFresh)
	{
		if(Entry.m_Result.m_SearchStatus == EQmAxiomScoreStatus::FETCHING && m_ActivePlayerName == pPlayerName && m_SearchRequest.m_pRequest)
			return;
		if(IsFailureStatus(Entry.m_Result.m_SearchStatus) && IsWithinWindow(Entry.m_LastSearchFailureTick, Now, AXIOM_FAILURE_RETRY_MS))
			return;

		BeginActiveQuery(pPlayerName);
		StartSearchRequest(pPlayerName, Entry);
		FinishActiveQueryIfIdle();
		return;
	}

	std::array<bool, 2> aNeedsRequest{};
	bool AnyRequestNeeded = false;
	for(int Index = 0; Index < (int)Entry.m_Result.m_aModes.size(); ++Index)
	{
		const EQmAxiomScoreStatus Status = Entry.m_Result.m_aModes[Index].m_Status;
		if(Status == EQmAxiomScoreStatus::READY && IsWithinWindow(Entry.m_aLastModeSuccessTick[Index], Now, AXIOM_SCORE_CACHE_TTL_MS))
			continue;
		if(Status == EQmAxiomScoreStatus::FETCHING && m_ActivePlayerName == pPlayerName && m_aModeRequests[Index].m_pRequest)
			continue;
		if(IsFailureStatus(Status) && IsWithinWindow(Entry.m_aLastModeFailureTick[Index], Now, AXIOM_FAILURE_RETRY_MS))
			continue;
		aNeedsRequest[Index] = true;
		AnyRequestNeeded = true;
	}
	if(!AnyRequestNeeded)
		return;

	BeginActiveQuery(pPlayerName);
	for(int Index = 0; Index < (int)aNeedsRequest.size(); ++Index)
	{
		if(aNeedsRequest[Index])
			StartModeRequest(pPlayerName, Entry, ModeFromIndex(Index));
	}
	FinishActiveQueryIfIdle();
}

const SQmAxiomPlayerResult *CQmAxiomScores::GetResult(const char *pPlayerName) const
{
	if(!pPlayerName || pPlayerName[0] == '\0')
		return nullptr;
	const auto It = m_Cache.find(pPlayerName);
	return It == m_Cache.end() ? nullptr : &It->second.m_Result;
}

void CQmAxiomScores::OnUpdate()
{
	ProcessSearchRequest();
	ProcessModeRequests();
	FinishActiveQueryIfIdle();
}

void CQmAxiomScores::OnReset()
{
	AbortActiveRequests(true);
}

void CQmAxiomScores::OnShutdown()
{
	AbortActiveRequests(false);
	m_Cache.clear();
}

void CQmAxiomScores::OnStateChange(int NewState, int OldState)
{
	if(NewState < IClient::STATE_ONLINE)
		AbortActiveRequests(true);
}
