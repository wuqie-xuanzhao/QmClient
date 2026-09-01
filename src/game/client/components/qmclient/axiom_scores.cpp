#include "axiom_scores.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/http.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
	constexpr int64_t AXIOM_SCORE_CACHE_TTL_MS = 30 * 60 * 1000;
	constexpr int64_t AXIOM_SEARCH_CACHE_TTL_MS = 2 * 60 * 60 * 1000;
	constexpr int64_t AXIOM_FAILURE_RETRY_MS = 30 * 1000;
	constexpr int64_t AXIOM_MAX_RESPONSE_BYTES = 8 * 1024 * 1024;
	constexpr size_t AXIOM_MAX_QUERY_NAME_BYTES = 256;
	constexpr size_t AXIOM_MAX_DIFFICULTY_NAME_BYTES = 192;
	constexpr int AXIOM_CONNECT_TIMEOUT_MS = 5000;
	constexpr int AXIOM_SEARCH_TIMEOUT_MS = 10000;
	constexpr int AXIOM_INFO_TIMEOUT_MS = 45000;

	const json_value *JsonField(const json_value *pObject, const char *pName)
	{
		if(!pObject || pObject->type != json_object)
			return &json_value_none;
		return json_object_get(pObject, pName);
	}

	bool ReadInt64(const json_value *pObject, const char *pName, int64_t &Out)
	{
		const json_value *pValue = JsonField(pObject, pName);
		if(pValue->type == json_integer)
		{
			Out = pValue->u.integer;
			return true;
		}
		if(pValue->type != json_string)
			return false;
		const char *pText = json_string_get(pValue);
		if(!pText || pText[0] == '\0')
			return false;
		Out = str_toint64_base(pText);
		char aCanonical[64];
		str_format(aCanonical, sizeof(aCanonical), "%lld", (long long)Out);
		return str_comp(aCanonical, pText) == 0;
	}

	bool ReadNonNegativeInt64(const json_value *pObject, const char *pName, int64_t &Out)
	{
		return ReadInt64(pObject, pName, Out) && Out >= 0;
	}

	void WriteInt64(CJsonFileWriter &Writer, const char *pName, int64_t Value)
	{
		char aValue[64];
		str_format(aValue, sizeof(aValue), "%lld", (long long)Value);
		Writer.WriteAttribute(pName);
		Writer.WriteStrValue(aValue);
	}

	void WriteOptionalInt64(CJsonFileWriter &Writer, const char *pName, const std::optional<int64_t> &Value)
	{
		if(Value)
			WriteInt64(Writer, pName, *Value);
	}

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

	void PrepareRequest(IHttpRequest *pRequest, int TimeoutMs)
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
		std::shared_ptr<IHttpRequest> m_pRequest;

	public:
		explicit CNativeAxiomHttpRequest(std::shared_ptr<IHttpRequest> pRequest) :
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

void CQmAxiomScores::LoadPersistentCache(const json_value *pRoot)
{
	if(!pRoot || pRoot->type != json_object)
		return;
	const json_value *pRemote = JsonField(pRoot, "remote");
	const json_value *pAxiom = JsonField(pRemote, "axiom");
	const json_value *pPlayers = JsonField(pAxiom, "players");
	if(pPlayers->type != json_array)
		return;
	for(unsigned PlayerIndex = 0; PlayerIndex < pPlayers->u.array.length; ++PlayerIndex)
	{
		const json_value *pPlayer = pPlayers->u.array.values[PlayerIndex];
		const json_value *pName = JsonField(pPlayer, "name");
		int64_t UserId = 0;
		if(pName->type != json_string || !json_string_get(pName) || json_string_get(pName)[0] == '\0' || static_cast<size_t>(str_length(json_string_get(pName))) > AXIOM_MAX_QUERY_NAME_BYTES || !str_utf8_check(json_string_get(pName)) || !ReadInt64(pPlayer, "user_id", UserId) || UserId <= 0)
			continue;
		SCacheEntry Entry;
		Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::READY;
		Entry.m_Result.m_Match.m_UserId = UserId;
		const json_value *pPlayerName = JsonField(pPlayer, "player_name");
		const json_value *pDummyName = JsonField(pPlayer, "dummy_name");
		if(pPlayerName->type == json_string && json_string_get(pPlayerName) && static_cast<size_t>(str_length(json_string_get(pPlayerName))) <= AXIOM_MAX_QUERY_NAME_BYTES && str_utf8_check(json_string_get(pPlayerName)))
			Entry.m_Result.m_Match.m_PlayerName = json_string_get(pPlayerName);
		if(pDummyName->type == json_string && json_string_get(pDummyName) && static_cast<size_t>(str_length(json_string_get(pDummyName))) <= AXIOM_MAX_QUERY_NAME_BYTES && str_utf8_check(json_string_get(pDummyName)))
			Entry.m_Result.m_Match.m_DummyName = json_string_get(pDummyName);
		const json_value *pModes = JsonField(pPlayer, "modes");
		if(pModes->type != json_array)
			continue;
		bool HasMode = false;
		for(unsigned ModeIndex = 0; ModeIndex < pModes->u.array.length; ++ModeIndex)
		{
			const json_value *pMode = pModes->u.array.values[ModeIndex];
			const json_value *pModeName = JsonField(pMode, "mode");
			if(pModeName->type != json_string || !json_string_get(pModeName))
				continue;
			EQmAxiomMode Mode;
			if(str_comp_nocase(json_string_get(pModeName), "AXRace") == 0)
				Mode = EQmAxiomMode::AXRACE;
			else if(str_comp_nocase(json_string_get(pModeName), "Gores") == 0)
				Mode = EQmAxiomMode::GORES;
			else
				continue;
			const int Index = Mode == EQmAxiomMode::AXRACE ? 1 : 0;
			SQmAxiomModeScore &Score = Entry.m_Result.m_aModes[Index].m_Score;
			int64_t Value = 0;
			if(!ReadNonNegativeInt64(pMode, "points", Value) ||
				!ReadNonNegativeInt64(pMode, "total_play_time", Score.m_TotalPlayTime) ||
				!ReadNonNegativeInt64(pMode, "total_maps_completed", Score.m_TotalMapsCompleted) ||
				!ReadNonNegativeInt64(pMode, "performance_points", Score.m_PerformancePoints) ||
				!ReadNonNegativeInt64(pMode, "mileage", Score.m_Mileage))
				continue;
			Score.m_Points = Value;
			Value = 0;
			ReadNonNegativeInt64(pMode, "global_rank", Value);
			if(Value > 0)
				Score.m_GlobalRank = Value;
			Value = 0;
			ReadNonNegativeInt64(pMode, "team_rank", Value);
			if(Value > 0)
				Score.m_TeamRank = Value;
			Score.m_PlayerName = Entry.m_Result.m_Match.m_PlayerName;
			const json_value *pDifficulties = JsonField(pMode, "difficulties");
			if(pDifficulties->type == json_array && pDifficulties->u.array.length <= 128)
			{
				bool ValidDifficulties = true;
				for(unsigned DifficultyIndex = 0; DifficultyIndex < pDifficulties->u.array.length; ++DifficultyIndex)
				{
					const json_value *pDifficulty = pDifficulties->u.array.values[DifficultyIndex];
					const json_value *pDifficultyName = JsonField(pDifficulty, "name");
					SQmAxiomDifficultyStats Difficulty;
					int64_t DifficultyValue = 0;
					if(pDifficultyName->type != json_string || !json_string_get(pDifficultyName) || json_string_get(pDifficultyName)[0] == '\0' || static_cast<size_t>(str_length(json_string_get(pDifficultyName))) > AXIOM_MAX_DIFFICULTY_NAME_BYTES || !str_utf8_check(json_string_get(pDifficultyName)) || !ReadNonNegativeInt64(pDifficulty, "points", Difficulty.m_Points) || !ReadNonNegativeInt64(pDifficulty, "completed_maps", Difficulty.m_CompletedMaps) || !ReadNonNegativeInt64(pDifficulty, "remaining_maps", Difficulty.m_RemainingMaps))
					{
						ValidDifficulties = false;
						break;
					}
					Difficulty.m_Name = json_string_get(pDifficultyName);
					DifficultyValue = 0;
					if(ReadNonNegativeInt64(pDifficulty, "global_rank", DifficultyValue) && DifficultyValue > 0)
						Difficulty.m_GlobalRank = DifficultyValue;
					DifficultyValue = 0;
					if(ReadNonNegativeInt64(pDifficulty, "team_rank", DifficultyValue) && DifficultyValue > 0)
						Difficulty.m_TeamRank = DifficultyValue;
					DifficultyValue = 0;
					if(ReadNonNegativeInt64(pDifficulty, "total_points", DifficultyValue))
						Difficulty.m_TotalPoints = DifficultyValue;
					DifficultyValue = 0;
					if(ReadNonNegativeInt64(pDifficulty, "total_maps", DifficultyValue))
						Difficulty.m_TotalMaps = DifficultyValue;
					Score.m_vDifficulties.push_back(std::move(Difficulty));
				}
				if(!ValidDifficulties)
					Score.m_vDifficulties.clear();
			}
			Entry.m_Result.m_aModes[Index].m_Status = EQmAxiomScoreStatus::READY;
			Entry.m_Result.m_aModes[Index].m_HasData = true;
			HasMode = true;
		}
		if(!HasMode)
			continue;
		// Persisted data remains visible, but is refreshed in the background on startup.
		Entry.m_LastSearchSuccessTick = 0;
		for(int Index = 0; Index < 2; ++Index)
			Entry.m_aLastModeSuccessTick[Index] = 0;
		m_Cache[json_string_get(pName)] = std::move(Entry);
	}
}

void CQmAxiomScores::WritePersistentCache(CJsonFileWriter &Writer) const
{
	Writer.WriteAttribute("axiom");
	Writer.BeginObject();
	Writer.WriteAttribute("players");
	Writer.BeginArray();
	for(const auto &[Name, Entry] : m_Cache)
	{
		if(Entry.m_Result.m_Match.m_UserId <= 0)
			continue;
		bool HasReadyMode = false;
		for(const SQmAxiomModeResult &ModeResult : Entry.m_Result.m_aModes)
			HasReadyMode |= ModeResult.m_HasData;
		if(!HasReadyMode)
			continue;
		Writer.BeginObject();
		Writer.WriteAttribute("name");
		Writer.WriteStrValue(Name.c_str());
		WriteInt64(Writer, "user_id", Entry.m_Result.m_Match.m_UserId);
		Writer.WriteAttribute("player_name");
		Writer.WriteStrValue(Entry.m_Result.m_Match.m_PlayerName.c_str());
		Writer.WriteAttribute("dummy_name");
		Writer.WriteStrValue(Entry.m_Result.m_Match.m_DummyName.c_str());
		Writer.WriteAttribute("modes");
		Writer.BeginArray();
		for(int Index = 0; Index < 2; ++Index)
		{
			const SQmAxiomModeResult &ModeResult = Entry.m_Result.m_aModes[Index];
			if(!ModeResult.m_HasData)
				continue;
			const SQmAxiomModeScore &Score = ModeResult.m_Score;
			Writer.BeginObject();
			Writer.WriteAttribute("mode");
			Writer.WriteStrValue(QmAxiomModeName(ModeFromIndex(Index)));
			WriteInt64(Writer, "points", Score.m_Points);
			WriteOptionalInt64(Writer, "global_rank", Score.m_GlobalRank);
			WriteOptionalInt64(Writer, "team_rank", Score.m_TeamRank);
			WriteInt64(Writer, "total_play_time", Score.m_TotalPlayTime);
			WriteInt64(Writer, "total_maps_completed", Score.m_TotalMapsCompleted);
			WriteInt64(Writer, "performance_points", Score.m_PerformancePoints);
			WriteInt64(Writer, "mileage", Score.m_Mileage);
			Writer.WriteAttribute("difficulties");
			Writer.BeginArray();
			for(const SQmAxiomDifficultyStats &Difficulty : Score.m_vDifficulties)
			{
				Writer.BeginObject();
				Writer.WriteAttribute("name");
				Writer.WriteStrValue(Difficulty.m_Name.c_str());
				WriteInt64(Writer, "points", Difficulty.m_Points);
				WriteOptionalInt64(Writer, "global_rank", Difficulty.m_GlobalRank);
				WriteOptionalInt64(Writer, "team_rank", Difficulty.m_TeamRank);
				WriteInt64(Writer, "completed_maps", Difficulty.m_CompletedMaps);
				WriteInt64(Writer, "remaining_maps", Difficulty.m_RemainingMaps);
				WriteOptionalInt64(Writer, "total_points", Difficulty.m_TotalPoints);
				WriteOptionalInt64(Writer, "total_maps", Difficulty.m_TotalMaps);
				Writer.EndObject();
			}
			Writer.EndArray();
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();
	}
	Writer.EndArray();
	Writer.EndObject();
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

	std::shared_ptr<IHttpRequest> pRequest = HttpGet(pUrl);
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

void CQmAxiomScores::StartSearchRequest(const char *pPlayerName, SCacheEntry &Entry)
{
	if(m_SearchRequest.m_pRequest)
	{
		m_SearchRequest.m_pRequest->Abort();
		m_SearchRequest.m_pRequest.reset();
		m_SearchRequest.m_PlayerName.clear();
	}
	Entry.m_Result.m_SearchStatus = EQmAxiomScoreStatus::FETCHING;

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

	if(Entry.m_Result.m_Match.m_UserId <= 0)
	{
		ModeResult.m_Status = EQmAxiomScoreStatus::HTTP_ERROR;
		Entry.m_aLastModeFailureTick[Index] = CurrentTick();
		return;
	}

	SRequestSlot &Slot = m_aModeRequests[Index];
	if(Slot.m_pRequest)
	{
		Slot.m_pRequest->Abort();
		Slot.m_pRequest.reset();
		Slot.m_PlayerName.clear();
	}
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

	const bool MatchChanged = Entry.m_Result.m_Match.m_UserId != Match.m_UserId;
	Entry.m_Result.m_Match = std::move(Match);
	if(MatchChanged)
	{
		m_PersistentCacheDirty = true;
		for(SQmAxiomModeResult &ModeResult : Entry.m_Result.m_aModes)
			ModeResult = {};
	}
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
		ModeResult.m_HasData = true;
		Entry.m_aLastModeSuccessTick[Index] = Now;
		Entry.m_aLastModeFailureTick[Index] = 0;
		m_PersistentCacheDirty = true;
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
	if(!pPlayerName || pPlayerName[0] == '\0' || static_cast<size_t>(str_length(pPlayerName)) > AXIOM_MAX_QUERY_NAME_BYTES || !str_utf8_check(pPlayerName))
		return;

	const int64_t Now = CurrentTick();
	auto CacheIt = m_Cache.find(pPlayerName);
	if(CacheIt == m_Cache.end())
		CacheIt = m_Cache.emplace(pPlayerName, SCacheEntry{}).first;
	SCacheEntry &Entry = CacheIt->second;
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

void CQmAxiomScores::Refresh(const char *pPlayerName)
{
	if(!pPlayerName || pPlayerName[0] == '\0')
		return;
	// 强制刷新必须先取消当前玩家的搜索和模式请求，避免新搜索完成后覆盖仍在运行的旧请求槽。
	AbortActiveRequests(true);
	const auto It = m_Cache.find(pPlayerName);
	if(It != m_Cache.end())
	{
		It->second.m_LastSearchSuccessTick = 0;
		It->second.m_LastSearchFailureTick = 0;
		It->second.m_aLastModeSuccessTick.fill(0);
		It->second.m_aLastModeFailureTick.fill(0);
	}
	EnsureQueried(pPlayerName);
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
