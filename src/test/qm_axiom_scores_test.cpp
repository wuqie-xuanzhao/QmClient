#include "test.h"

#include <base/system.h>

#include <engine/shared/json.h>

#include <game/client/components/qmclient/axiom_scores.h>
#include <game/client/components/qmclient/axiom_scores_data.h>

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

IHttp *CComponentInterfaces::Http() const
{
	return nullptr;
}

namespace
{
	class CFakeAxiomHttpRequest final : public IQmAxiomHttpRequest
	{
		bool m_Done = false;
		bool m_TransportSucceeded = true;
		bool m_Aborted = false;
		int m_StatusCode = 200;
		std::vector<unsigned char> m_vBody;

	public:
		bool Done() const override { return m_Done; }
		bool TransportSucceeded() const override { return m_TransportSucceeded; }
		int StatusCode() const override { return m_StatusCode; }
		void Result(const unsigned char **ppData, size_t *pDataSize) const override
		{
			*ppData = m_vBody.empty() ? nullptr : m_vBody.data();
			*pDataSize = m_vBody.size();
		}
		void Abort() override { m_Aborted = true; }

		void Complete(std::string Body, int StatusCode = 200, bool TransportSucceeded = true)
		{
			m_vBody.assign(Body.begin(), Body.end());
			m_StatusCode = StatusCode;
			m_TransportSucceeded = TransportSucceeded;
			m_Done = true;
		}

		bool Aborted() const { return m_Aborted; }
	};

	struct SRecordedAxiomRequest
	{
		std::string m_Url;
		int m_ConnectTimeoutMs;
		int m_TimeoutMs;
		int64_t m_MaxResponseBytes;
		std::shared_ptr<CFakeAxiomHttpRequest> m_pRequest;
	};

	class CFakeAxiomHttp final : public IQmAxiomHttp
	{
	public:
		std::vector<SRecordedAxiomRequest> m_vRequests;

		std::shared_ptr<IQmAxiomHttpRequest> Get(const char *pUrl, int ConnectTimeoutMs, int TimeoutMs, int64_t MaxResponseBytes) override
		{
			auto pRequest = std::make_shared<CFakeAxiomHttpRequest>();
			m_vRequests.push_back({pUrl, ConnectTimeoutMs, TimeoutMs, MaxResponseBytes, pRequest});
			return pRequest;
		}

		const SRecordedAxiomRequest &Request(size_t Index) const
		{
			return m_vRequests.at(Index);
		}
	};

	class CTestAxiomScores final : public CQmAxiomScores
	{
		int64_t m_Now = time_freq();

	protected:
		int64_t CurrentTick() const override { return m_Now; }

	public:
		explicit CTestAxiomScores(IQmAxiomHttp *pHttp) :
			CQmAxiomScores(pHttp)
		{
		}

		void AdvanceMs(int64_t Milliseconds)
		{
			m_Now += Milliseconds * time_freq() / 1000;
		}
	};

	EQmAxiomParseResult ParseSearch(const char *pJson, const char *pPlayerName, SQmAxiomSearchMatch &Match)
	{
		return QmParseAxiomSearchResponse(pJson, std::strlen(pJson), pPlayerName, Match);
	}

	EQmAxiomParseResult ParseInfo(const char *pJson, SQmAxiomModeScore &Score)
	{
		return QmParseAxiomInfoResponse(pJson, std::strlen(pJson), Score);
	}

	std::string SearchResponse(const char *pPlayerName, int UserId = 5528)
	{
		return std::string("{\"code\":200,\"data\":{\"results\":[{\"user_id\":") +
		       std::to_string(UserId) + ",\"player_name\":\"" + pPlayerName + "\",\"dummy_name\":\"\"}]}}";
	}

	std::string InfoResponse(const char *pPlayerName, int Points)
	{
		return std::string("{\"code\":200,\"data\":{\"player\":{\"player_name\":\"") + pPlayerName +
		       "\",\"points\":" + std::to_string(Points) +
		       ",\"global_rank\":1,\"team_rank\":null,\"total_play_time\":2,\"total_maps_completed\":3,\"performance_points\":4,\"mileage\":5},\"difficultyData\":{}}}";
	}

	void CompleteSuccessfulQuery(CTestAxiomScores &Scores, CFakeAxiomHttp &Http, const char *pPlayerName, int UserId = 5528)
	{
		const size_t SearchIndex = Http.m_vRequests.size();
		Scores.EnsureQueried(pPlayerName);
		ASSERT_EQ(Http.m_vRequests.size(), SearchIndex + 1);
		Http.Request(SearchIndex).m_pRequest->Complete(SearchResponse(pPlayerName, UserId));
		Scores.OnUpdate();
		ASSERT_EQ(Http.m_vRequests.size(), SearchIndex + 3);
		Http.Request(SearchIndex + 1).m_pRequest->Complete(InfoResponse(pPlayerName, 10));
		Http.Request(SearchIndex + 2).m_pRequest->Complete(InfoResponse(pPlayerName, 20));
		Scores.OnUpdate();
	}
}

TEST(QmAxiomScoresUrl, EncodesPlayerNameAndKeepsModesSeparate)
{
	const std::string SearchUrl = QmBuildAxiomSearchUrl("wolf test&dummy");
	EXPECT_EQ(SearchUrl.find("https://api.axiom.teeworlds.cn/v1/query/search?"), 0u);
	EXPECT_NE(SearchUrl.find("q=wolf%20test%26dummy"), std::string::npos);
	EXPECT_NE(SearchUrl.find("search_type=player&page=1&limit=18"), std::string::npos);

	EXPECT_EQ(QmBuildAxiomInfoUrl(5528, EQmAxiomMode::GORES), "https://api.axiom.teeworlds.cn/v1/query/user/info?user_id=5528&mode=Gores");
	EXPECT_EQ(QmBuildAxiomInfoUrl(5528, EQmAxiomMode::AXRACE), "https://api.axiom.teeworlds.cn/v1/query/user/info?user_id=5528&mode=AXRace");
}

TEST(QmAxiomScoresSearch, SelectsExactPlayerNameInsteadOfFirstFuzzyResult)
{
	const char *pJson = R"({
		"code": 200,
		"data": {
			"results": [
				{"user_id": 1, "player_name": "wolf_test_2", "dummy_name": ""},
				{"user_id": 5528, "player_name": "wolf_test", "dummy_name": "xulangzhi"}
			]
		}
	})";
	SQmAxiomSearchMatch Match;
	EXPECT_EQ(ParseSearch(pJson, "wolf_test", Match), EQmAxiomParseResult::SUCCESS);
	EXPECT_EQ(Match.m_UserId, 5528);
	EXPECT_EQ(Match.m_PlayerName, "wolf_test");
	EXPECT_EQ(Match.m_DummyName, "xulangzhi");
}

TEST(QmAxiomScoresSearch, AcceptsExactDummyName)
{
	const char *pJson = R"({"code":200,"data":{"results":[{"user_id":5528,"player_name":"wolf_test","dummy_name":"xulangzhi"}]}})";
	SQmAxiomSearchMatch Match;
	EXPECT_EQ(ParseSearch(pJson, "xulangzhi", Match), EQmAxiomParseResult::SUCCESS);
	EXPECT_EQ(Match.m_UserId, 5528);
}

TEST(QmAxiomScoresSearch, RejectsAmbiguousExactMatches)
{
	const char *pJson = R"({"code":200,"data":{"results":[
		{"user_id":1,"player_name":"one","dummy_name":"same"},
		{"user_id":2,"player_name":"two","dummy_name":"same"}
	]}})";
	SQmAxiomSearchMatch Match;
	EXPECT_EQ(ParseSearch(pJson, "same", Match), EQmAxiomParseResult::AMBIGUOUS);
}

TEST(QmAxiomScoresSearch, AcceptsDuplicateExactMatchesForTheSameUser)
{
	const char *pJson = R"({"code":200,"data":{"results":[
		{"user_id":5528,"player_name":"wolf_test","dummy_name":"xulangzhi"},
		{"user_id":5528,"player_name":"wolf_test","dummy_name":"xulangzhi"}
	]}})";
	SQmAxiomSearchMatch Match;
	EXPECT_EQ(ParseSearch(pJson, "wolf_test", Match), EQmAxiomParseResult::SUCCESS);
	EXPECT_EQ(Match.m_UserId, 5528);
}

TEST(QmAxiomScoresSearch, RejectsFuzzyOnlyAndMalformedResponses)
{
	SQmAxiomSearchMatch Match;
	EXPECT_EQ(ParseSearch(R"({"code":200,"data":{"results":[{"user_id":1,"player_name":"wolf_test_2"}]}})", "wolf_test", Match), EQmAxiomParseResult::NOT_FOUND);
	EXPECT_EQ(ParseSearch(R"({"code":500,"data":null})", "wolf_test", Match), EQmAxiomParseResult::API_ERROR);
	EXPECT_EQ(ParseSearch(R"({"code":200,"data":{"results":{}}})", "wolf_test", Match), EQmAxiomParseResult::INVALID_RESPONSE);
}

TEST(QmAxiomScoresInfo, ParsesNullableRanksAndDifficultyStats)
{
	const char *pJson = R"({
		"code": 200,
		"data": {
			"player": {
				"player_name": "wolf_test",
				"points": 38,
				"global_rank": 817,
				"team_rank": null,
				"total_play_time": 3,
				"total_maps_completed": 11,
				"performance_points": 11,
				"mileage": 38
			},
			"difficultyData": {
				"Easy 简单": {
					"stats": {
						"total_points": 100,
						"total_maps": 34,
						"points": 38,
						"global_rank": 400,
						"completed_maps": 11,
						"remaining_maps": 23
					}
				}
			}
		}
	})";

	SQmAxiomModeScore Score;
	ASSERT_EQ(ParseInfo(pJson, Score), EQmAxiomParseResult::SUCCESS);
	EXPECT_EQ(Score.m_PlayerName, "wolf_test");
	EXPECT_EQ(Score.m_Points, 38);
	ASSERT_TRUE(Score.m_GlobalRank.has_value());
	EXPECT_EQ(*Score.m_GlobalRank, 817);
	EXPECT_FALSE(Score.m_TeamRank.has_value());
	EXPECT_EQ(Score.m_TotalPlayTime, 3);
	EXPECT_EQ(Score.m_TotalMapsCompleted, 11);
	EXPECT_EQ(Score.m_PerformancePoints, 11);
	EXPECT_EQ(Score.m_Mileage, 38);
	ASSERT_EQ(Score.m_vDifficulties.size(), 1u);
	const SQmAxiomDifficultyStats &Difficulty = Score.m_vDifficulties[0];
	EXPECT_EQ(Difficulty.m_Name, "Easy 简单");
	EXPECT_EQ(Difficulty.m_Points, 38);
	ASSERT_TRUE(Difficulty.m_GlobalRank.has_value());
	EXPECT_EQ(*Difficulty.m_GlobalRank, 400);
	EXPECT_FALSE(Difficulty.m_TeamRank.has_value());
	EXPECT_EQ(Difficulty.m_CompletedMaps, 11);
	EXPECT_EQ(Difficulty.m_RemainingMaps, 23);
	ASSERT_TRUE(Difficulty.m_TotalPoints.has_value());
	EXPECT_EQ(*Difficulty.m_TotalPoints, 100);
	ASSERT_TRUE(Difficulty.m_TotalMaps.has_value());
	EXPECT_EQ(*Difficulty.m_TotalMaps, 34);
}

TEST(QmAxiomScoresInfo, RejectsMissingOrInvalidRequiredFields)
{
	SQmAxiomModeScore Score;
	EXPECT_EQ(ParseInfo(R"({"code":200,"data":{"player":{"player_name":"wolf","points":1,"global_rank":null,"team_rank":null,"total_play_time":1,"total_maps_completed":1,"performance_points":1},"difficultyData":{}}})", Score), EQmAxiomParseResult::INVALID_RESPONSE);
	EXPECT_EQ(ParseInfo(R"({"code":200,"data":{"player":{"player_name":"wolf","points":-1,"global_rank":null,"team_rank":null,"total_play_time":1,"total_maps_completed":1,"performance_points":1,"mileage":1},"difficultyData":{}}})", Score), EQmAxiomParseResult::INVALID_RESPONSE);
	EXPECT_EQ(ParseInfo(R"({"code":503,"data":null})", Score), EQmAxiomParseResult::API_ERROR);
}

TEST(QmAxiomScoresLifecycle, OnlyCurrentGenerationCanPublish)
{
	EXPECT_TRUE(QmAxiomResponseIsCurrent(7, 7, "new_player", "new_player"));
	EXPECT_FALSE(QmAxiomResponseIsCurrent(7, 6, "new_player", "new_player"));
	EXPECT_FALSE(QmAxiomResponseIsCurrent(7, 7, "new_player", "old_player"));
}

TEST(QmAxiomScoresComponent, StartsBothModesAndKeepsFailuresIndependent)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);

	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	EXPECT_NE(Http.Request(0).m_Url.find("/v1/query/search?"), std::string::npos);
	EXPECT_EQ(Http.Request(0).m_ConnectTimeoutMs, 5000);
	EXPECT_EQ(Http.Request(0).m_TimeoutMs, 10000);
	EXPECT_EQ(Http.Request(0).m_MaxResponseBytes, 8 * 1024 * 1024);

	Http.Request(0).m_pRequest->Complete(SearchResponse("wolf_test"));
	Scores.OnUpdate();
	ASSERT_EQ(Http.m_vRequests.size(), 3u);
	EXPECT_NE(Http.Request(1).m_Url.find("mode=Gores"), std::string::npos);
	EXPECT_NE(Http.Request(2).m_Url.find("mode=AXRace"), std::string::npos);
	EXPECT_EQ(Http.Request(1).m_TimeoutMs, 45000);
	EXPECT_EQ(Http.Request(2).m_TimeoutMs, 45000);

	Http.Request(1).m_pRequest->Complete(InfoResponse("wolf_test", 38));
	Http.Request(2).m_pRequest->Complete("", 503, false);
	Scores.OnUpdate();

	const SQmAxiomPlayerResult *pResult = Scores.GetResult("wolf_test");
	ASSERT_NE(pResult, nullptr);
	EXPECT_EQ(pResult->m_SearchStatus, EQmAxiomScoreStatus::READY);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::GORES).m_Status, EQmAxiomScoreStatus::READY);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::GORES).m_Score.m_Points, 38);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::AXRACE).m_Status, EQmAxiomScoreStatus::HTTP_ERROR);

	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 3u);
	Scores.AdvanceMs(29999);
	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 3u);
	Scores.AdvanceMs(1);
	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 4u);
	EXPECT_NE(Http.Request(3).m_Url.find("mode=AXRace"), std::string::npos);
}

TEST(QmAxiomScoresComponent, KeepsPersistedScoreVisibleWhileRefreshing)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);
	const char *pJson = R"({
		"remote": {"axiom": {"players": [{
			"name": "wolf_test",
			"user_id": 5528,
			"player_name": "wolf_test",
			"dummy_name": "",
			"modes": [{
				"mode": "Gores",
				"points": 38,
				"total_play_time": 2,
				"total_maps_completed": 3,
				"performance_points": 4,
				"mileage": 5,
				"difficulties": []
			}]
		}]}}
	})";
	json_value *pRoot = JsonParse(pJson, std::strlen(pJson));
	ASSERT_NE(pRoot, nullptr);
	Scores.LoadPersistentCache(pRoot);
	json_value_free(pRoot);

	const SQmAxiomPlayerResult *pResult = Scores.GetResult("wolf_test");
	ASSERT_NE(pResult, nullptr);
	EXPECT_TRUE(pResult->Mode(EQmAxiomMode::GORES).m_HasData);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::GORES).m_Score.m_Points, 38);

	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	pResult = Scores.GetResult("wolf_test");
	ASSERT_NE(pResult, nullptr);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::GORES).m_Status, EQmAxiomScoreStatus::READY);
	EXPECT_EQ(pResult->Mode(EQmAxiomMode::GORES).m_Score.m_Points, 38);
}

TEST(QmAxiomScoresComponent, SwitchingPlayersCancelsEveryOldRequest)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);

	Scores.EnsureQueried("player_a");
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	Scores.EnsureQueried("player_b");
	ASSERT_EQ(Http.m_vRequests.size(), 2u);
	EXPECT_TRUE(Http.Request(0).m_pRequest->Aborted());
	ASSERT_NE(Scores.GetResult("player_a"), nullptr);
	EXPECT_EQ(Scores.GetResult("player_a")->m_SearchStatus, EQmAxiomScoreStatus::NOT_REQUESTED);

	Http.Request(1).m_pRequest->Complete(SearchResponse("player_b"));
	Scores.OnUpdate();
	ASSERT_EQ(Http.m_vRequests.size(), 4u);
	Scores.EnsureQueried("player_c");
	ASSERT_EQ(Http.m_vRequests.size(), 5u);
	EXPECT_TRUE(Http.Request(2).m_pRequest->Aborted());
	EXPECT_TRUE(Http.Request(3).m_pRequest->Aborted());

	Http.Request(2).m_pRequest->Complete(InfoResponse("player_b", 10));
	Http.Request(3).m_pRequest->Complete(InfoResponse("player_b", 20));
	Scores.OnUpdate();
	const SQmAxiomPlayerResult *pResultB = Scores.GetResult("player_b");
	ASSERT_NE(pResultB, nullptr);
	EXPECT_EQ(pResultB->Mode(EQmAxiomMode::GORES).m_Status, EQmAxiomScoreStatus::NOT_REQUESTED);
	EXPECT_EQ(pResultB->Mode(EQmAxiomMode::AXRACE).m_Status, EQmAxiomScoreStatus::NOT_REQUESTED);
}

TEST(QmAxiomScoresComponent, ResetCancelsAndShutdownClearsCache)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);

	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	Scores.OnReset();
	EXPECT_TRUE(Http.Request(0).m_pRequest->Aborted());
	ASSERT_NE(Scores.GetResult("wolf_test"), nullptr);
	EXPECT_EQ(Scores.GetResult("wolf_test")->m_SearchStatus, EQmAxiomScoreStatus::NOT_REQUESTED);

	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 2u);
	Scores.OnShutdown();
	EXPECT_TRUE(Http.Request(1).m_pRequest->Aborted());
	EXPECT_EQ(Scores.GetResult("wolf_test"), nullptr);
}

TEST(QmAxiomScoresComponent, SearchFailureUsesThirtySecondBackoff)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);

	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 1u);
	Http.Request(0).m_pRequest->Complete("", 503, false);
	Scores.OnUpdate();
	ASSERT_NE(Scores.GetResult("wolf_test"), nullptr);
	EXPECT_EQ(Scores.GetResult("wolf_test")->m_SearchStatus, EQmAxiomScoreStatus::HTTP_ERROR);

	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 1u);
	Scores.AdvanceMs(29999);
	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 1u);
	Scores.AdvanceMs(1);
	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 2u);
}

TEST(QmAxiomScoresComponent, ModeAndSearchCacheUseIndependentTtl)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);
	CompleteSuccessfulQuery(Scores, Http, "wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 3u);

	Scores.AdvanceMs(30 * 60 * 1000 - 1);
	Scores.EnsureQueried("wolf_test");
	EXPECT_EQ(Http.m_vRequests.size(), 3u);
	Scores.AdvanceMs(1);
	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 5u);
	EXPECT_NE(Http.Request(3).m_Url.find("mode=Gores"), std::string::npos);
	EXPECT_NE(Http.Request(4).m_Url.find("mode=AXRace"), std::string::npos);
	Http.Request(3).m_pRequest->Complete(InfoResponse("wolf_test", 11));
	Http.Request(4).m_pRequest->Complete(InfoResponse("wolf_test", 21));
	Scores.OnUpdate();

	Scores.AdvanceMs(90 * 60 * 1000);
	Scores.EnsureQueried("wolf_test");
	ASSERT_EQ(Http.m_vRequests.size(), 6u);
	EXPECT_NE(Http.Request(5).m_Url.find("/v1/query/search?"), std::string::npos);
}

TEST(QmAxiomScoresComponent, EvictsLeastRecentlyUsedEntryAtCapacity)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);

	for(int Index = 0; Index < 64; ++Index)
	{
		const std::string PlayerName = "player_" + std::to_string(Index);
		Scores.EnsureQueried(PlayerName.c_str());
		ASSERT_EQ(Http.m_vRequests.size(), static_cast<size_t>(Index + 1));
		Http.Request(Index).m_pRequest->Complete("", 503, false);
		Scores.OnUpdate();
		Scores.AdvanceMs(1);
	}

	ASSERT_NE(Scores.GetResult("player_0"), nullptr);
	Scores.EnsureQueried("player_64");
	EXPECT_EQ(Scores.GetResult("player_0"), nullptr);
	EXPECT_NE(Scores.GetResult("player_1"), nullptr);
	EXPECT_NE(Scores.GetResult("player_64"), nullptr);
}

TEST(QmAxiomScoresComponent, RejectsInvalidPlayerNamesBeforeHttp)
{
	CFakeAxiomHttp Http;
	CTestAxiomScores Scores(&Http);
	std::string TooLong(257, 'a');
	const char aInvalidUtf8[] = {static_cast<char>(0xff), '\0'};

	Scores.EnsureQueried(nullptr);
	Scores.EnsureQueried("");
	Scores.EnsureQueried(TooLong.c_str());
	Scores.EnsureQueried(aInvalidUtf8);
	EXPECT_TRUE(Http.m_vRequests.empty());
}

TEST(QmAxiomScoresLayout, ConstrainsPopupAtCommonUiScales)
{
	const SQmAxiomPopupSize Scale100 = QmAxiomPopupSize(1066.0f, 600.0f);
	EXPECT_FLOAT_EQ(Scale100.m_Width, 360.0f);
	EXPECT_FLOAT_EQ(Scale100.m_Height, 390.0f);

	const SQmAxiomPopupSize Scale150 = QmAxiomPopupSize(711.0f, 400.0f);
	EXPECT_FLOAT_EQ(Scale150.m_Width, 360.0f);
	EXPECT_FLOAT_EQ(Scale150.m_Height, 390.0f);

	const SQmAxiomPopupSize Scale200 = QmAxiomPopupSize(533.0f, 300.0f);
	EXPECT_FLOAT_EQ(Scale200.m_Width, 360.0f);
	EXPECT_FLOAT_EQ(Scale200.m_Height, 290.0f);
	EXPECT_LE(Scale200.m_Width, 533.0f - 10.0f);
	EXPECT_LE(Scale200.m_Height, 300.0f - 10.0f);
}

TEST(QmAxiomScoresIntegration, QueriesOnlyFromTheAxiomScoreboardPopup)
{
	const std::string Scoreboard = ReadTestSourceFile("src/game/client/components/scoreboard.cpp");
	const std::string Component = ReadTestSourceFile("src/game/client/components/qmclient/axiom_scores.cpp");

	EXPECT_NE(Scoreboard.find("m_QmAxiomAutoLogin.IsAxiomCommunity() && !GameClient()->ShouldHideStreamerIdentity(ClientId)"), std::string::npos);
	EXPECT_NE(Scoreboard.find("m_QmAxiomScores.EnsureQueried(ClientData.m_aName);"), std::string::npos);
	EXPECT_NE(Scoreboard.find("QmAxiomPopupSize("), std::string::npos);
	EXPECT_NE(Scoreboard.find("m_AxiomScrollRegion.Begin"), std::string::npos);
	EXPECT_NE(Scoreboard.find("pResult->Mode(EQmAxiomMode::GORES)"), std::string::npos);
	EXPECT_NE(Scoreboard.find("pResult->Mode(EQmAxiomMode::AXRACE)"), std::string::npos);
	EXPECT_NE(Component.find("AbortActiveRequests(true);"), std::string::npos);
	EXPECT_NE(Component.find("QmAxiomResponseIsCurrent"), std::string::npos);
	EXPECT_NE(Component.find("MaxResponseSize(AXIOM_MAX_RESPONSE_BYTES)"), std::string::npos);
	EXPECT_NE(Component.find("AXIOM_INFO_TIMEOUT_MS = 45000"), std::string::npos);
}

TEST(QmAxiomScoresIntegration, KeepsTheExistingDdnetPointsColumn)
{
	const std::string Scoreboard = ReadTestSourceFile("src/game/client/components/scoreboard.cpp");
	EXPECT_NE(Scoreboard.find("m_PlayerPoints.GetPoints(ClientData.m_aName)"), std::string::npos);
	EXPECT_NE(Scoreboard.find("m_PlayerPoints.EnsureQueried(GameClient()->m_aClients[i].m_aName)"), std::string::npos);
}
