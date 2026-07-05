#include <base/system.h>

#include <game/client/live/live_director.h>
#include <game/client/live/live_finish_ranking.h>
#include <game/client/live/live_replay_buffer.h>
#include <game/client/live/live_replay_sidecar.h>
#include <game/client/live/live_team_render_filter.h>
#include <game/teamscore.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
	std::array<int, MAX_CLIENTS> DefaultTeams()
	{
		std::array<int, MAX_CLIENTS> aTeams{};
		aTeams.fill(TEAM_FLOCK);
		return aTeams;
	}

	std::array<bool, MAX_CLIENTS> DefaultActivePlayers()
	{
		std::array<bool, MAX_CLIENTS> aActivePlayers{};
		aActivePlayers.fill(false);
		return aActivePlayers;
	}
} // namespace

TEST(QmLiveDirector, BuildsRowsForActiveDDRaceTeams)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[1] = TEAM_FLOCK;
	aTeams[2] = 4;
	aTeams[3] = 4;
	aTeams[4] = 7;
	aTeams[5] = TEAM_SUPER;
	for(int ClientId = 1; ClientId <= 5; ++ClientId)
		aActivePlayers[ClientId] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 2u);
	EXPECT_TRUE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[0].m_Team, 4);
	EXPECT_EQ(Director.Entries()[0].m_NumPlayers, 2);
	EXPECT_EQ(Director.Entries()[1].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[1].m_Team, 7);
	EXPECT_EQ(Director.Entries()[1].m_NumPlayers, 1);
	EXPECT_EQ(Director.FallbackPlayer(), 1);
}

TEST(QmLiveDirector, IgnoresInactivePlayers)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[2] = 3;
	aTeams[6] = 6;
	aActivePlayers[6] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 1u);
	EXPECT_TRUE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[0].m_Team, 6);
	EXPECT_EQ(Director.Entries()[0].m_NumPlayers, 1);
	EXPECT_EQ(Director.FallbackPlayer(), 6);
}

TEST(QmLiveDirector, FallsBackToPlayerRowsWithoutDDRaceTeams)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[2] = TEAM_FLOCK;
	aTeams[7] = TEAM_SUPER;
	aActivePlayers[2] = true;
	aActivePlayers[7] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 2u);
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::PLAYER);
	EXPECT_EQ(Director.Entries()[0].m_ClientId, 2);
	EXPECT_EQ(Director.Entries()[1].m_Type, CLiveDirector::EEntryType::PLAYER);
	EXPECT_EQ(Director.Entries()[1].m_ClientId, 7);
	EXPECT_EQ(Director.FallbackPlayer(), 2);
}

TEST(QmLiveDirector, SelectRandomTeamUsesStableModulo)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[0] = 2;
	aTeams[1] = 5;
	aActivePlayers[0] = true;
	aActivePlayers[1] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_EQ(Director.SelectRandomTeam(0), 2);
	EXPECT_EQ(Director.SelectRandomTeam(1), 5);
	EXPECT_EQ(Director.SelectRandomTeam(2), 2);
}

TEST(QmLivePresentationMode, LiveObserverKeepsCompatDirectorPresentation)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t ModeStart = Source.find("CGameClient::EQmLivePresentationMode CGameClient::LivePresentationMode() const");
	ASSERT_NE(ModeStart, std::string::npos);
	const size_t ModeEnd = Source.find("void CGameClient::OnConsoleInit()", ModeStart);
	ASSERT_NE(ModeEnd, std::string::npos);
	const std::string Body = Source.substr(ModeStart, ModeEnd - ModeStart);

	EXPECT_NE(Body.find("Client()->QmLiveDirectorActive()"), std::string::npos);
}

TEST(QmLiveMatchReplay, StartKeepsOrdinaryDDNetRecordingCompatible)
{
	const std::string Source = ReadTestSourceFile("src/game/client/live/live_match_replay.cpp");
	const size_t StartPos = Source.find("bool CLiveMatchReplay::Start(CGameClient *pGameClient)");
	ASSERT_NE(StartPos, std::string::npos);
	const size_t StopPos = Source.find("bool CLiveMatchReplay::Stop(CGameClient *pGameClient, bool WriteSidecarFile)", StartPos);
	ASSERT_NE(StopPos, std::string::npos);
	const std::string Body = Source.substr(StartPos, StopPos - StartPos);

	EXPECT_EQ(Body.find("QmLiveObserverActive"), std::string::npos);
	EXPECT_NE(Body.find("DemoRecorder_Start"), std::string::npos);
	EXPECT_NE(Source.find("demos/qm_live/matches"), std::string::npos);
}

TEST(QmLiveDirector, SelectRandomPlayerUsesStableModulo)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[3] = TEAM_FLOCK;
	aTeams[8] = TEAM_SUPER;
	aActivePlayers[3] = true;
	aActivePlayers[8] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_EQ(Director.SelectRandomPlayer(0), 3);
	EXPECT_EQ(Director.SelectRandomPlayer(1), 8);
	EXPECT_EQ(Director.SelectRandomPlayer(2), 3);
	EXPECT_EQ(Director.SelectRandomTeam(0), -1);
}

TEST(QmLiveDirector, EmptyDataHasNoEntries)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_TRUE(Director.Entries().empty());
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.SelectRandomTeam(0), -1);
	EXPECT_EQ(Director.SelectRandomPlayer(0), -1);
	EXPECT_EQ(Director.FallbackPlayer(), -1);
}

TEST(QmLiveDirector, ResetClearsState)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[0] = 2;
	aActivePlayers[0] = true;
	Director.UpdateEntries(aTeams, aActivePlayers);
	Director.SetMode(CLiveObserverSession::EDirectorMode::FOLLOW_TEAM);

	Director.Reset();

	EXPECT_TRUE(Director.Entries().empty());
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.FallbackPlayer(), -1);
	EXPECT_EQ(Director.Mode(), CLiveObserverSession::EDirectorMode::FREEVIEW);
}

TEST(QmLiveReplayBuffer, KeepsLatestFramesAndCopiesData)
{
	CLiveReplayBuffer Buffer;
	Buffer.SetMaxFrames(2);

	uint8_t aFrame[] = {1, 2, 3};
	Buffer.PushSnapshot(10, aFrame, sizeof(aFrame));
	aFrame[0] = 9;
	Buffer.PushSnapshot(11, aFrame, sizeof(aFrame));
	Buffer.PushSnapshot(12, aFrame, sizeof(aFrame));

	ASSERT_EQ(Buffer.Frames().size(), 2u);
	EXPECT_EQ(Buffer.Frames()[0].m_Tick, 11);
	EXPECT_EQ(Buffer.Frames()[1].m_Tick, 12);
	ASSERT_EQ(Buffer.Frames()[0].m_vData.size(), 3u);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[0], 9);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[1], 2);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[2], 3);
}

TEST(QmLiveReplayBuffer, RejectsEmptyFramesAndHonorsDisabledBuffer)
{
	CLiveReplayBuffer Buffer;
	const uint8_t aFrame[] = {1};

	Buffer.PushSnapshot(1, aFrame, sizeof(aFrame));
	EXPECT_TRUE(Buffer.Frames().empty());

	Buffer.SetMaxFrames(2);
	Buffer.PushSnapshot(2, nullptr, sizeof(aFrame));
	Buffer.PushSnapshot(3, aFrame, 0);
	EXPECT_TRUE(Buffer.Frames().empty());
}

TEST(QmLiveTeamRenderFilter, AcceptsOnlyRegularDDRaceTeams)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();

	EXPECT_FALSE(Filter.SetTeam(TEAM_FLOCK));
	EXPECT_FALSE(Filter.SetTeam(TEAM_SUPER));
	EXPECT_FALSE(Filter.SetTeam(-1));
	ASSERT_TRUE(Filter.SetTeam(4));

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	aTeams[2] = 4;
	aTeams[3] = 5;
	Filter.UpdateTeams(aTeams);

	EXPECT_TRUE(Filter.AllowsClient(2));
	EXPECT_FALSE(Filter.AllowsClient(3));
	EXPECT_FALSE(Filter.AllowsTeam(TEAM_FLOCK));
	EXPECT_FALSE(Filter.AllowsTeam(TEAM_SUPER));
}

TEST(QmLiveTeamRenderFilter, StrictUnknownEventsDefaultRejects)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();
	ASSERT_TRUE(Filter.SetTeam(2));

	EXPECT_FALSE(Filter.AllowsUnknownPlayerEvent());
	Filter.SetStrictUnknownEvents(false);
	EXPECT_TRUE(Filter.AllowsUnknownPlayerEvent());
}

TEST(QmLiveTeamRenderFilter, ObservesPlaybackSeekAndRestart)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();
	ASSERT_TRUE(Filter.SetTeam(3));
	const int InitialSerial = Filter.ResetSerial();

	EXPECT_FALSE(Filter.ObservePlaybackTick(100));
	EXPECT_FALSE(Filter.ObservePlaybackTick(101));
	EXPECT_TRUE(Filter.ObservePlaybackTick(50));
	EXPECT_GT(Filter.ResetSerial(), InitialSerial);
}

TEST(QmLiveReplaySidecar, BuildsParsesAndDeduplicatesEvents)
{
	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	Sidecar.SetEndTick(90);

	EXPECT_TRUE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	EXPECT_FALSE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	EXPECT_TRUE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));
	EXPECT_FALSE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));

	const std::string Json = Sidecar.BuildJson();
	EXPECT_NE(Json.find("\"time_ms\""), std::string::npos);
	SLiveReplaySidecarData Parsed;
	char aError[128];
	ASSERT_TRUE(CLiveReplaySidecar::LoadFromString(Json.c_str(), Parsed, aError, sizeof(aError))) << aError;

	EXPECT_EQ(Parsed.m_FormatVersion, SLiveReplaySidecarData::FORMAT_VERSION);
	EXPECT_STREQ(Parsed.m_aDemoFilename, "demos/live/Map_match.demo");
	EXPECT_STREQ(Parsed.m_aMapName, "Map");
	EXPECT_EQ(Parsed.m_MapCrc, 1234u);
	EXPECT_EQ(Parsed.m_StartTick, 10);
	EXPECT_EQ(Parsed.m_EndTick, 90);
	ASSERT_EQ(Parsed.m_vFinishEvents.size(), 1u);
	EXPECT_EQ(Parsed.m_vFinishEvents[0].m_Team, 4);
	ASSERT_EQ(Parsed.m_vTeamEvents.size(), 1u);
	EXPECT_EQ(Parsed.m_vTeamEvents[0].m_NewTeam, 4);
}

TEST(QmLiveReplaySidecar, RejectsDamagedAndMismatchedSidecars)
{
	SLiveReplaySidecarData Parsed;
	char aError[128];
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString("{\"format_version\":1,\"finish_events\":[", Parsed, aError, sizeof(aError)));

	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	const std::string Json = Sidecar.BuildJson();
	ASSERT_TRUE(CLiveReplaySidecar::LoadFromString(Json.c_str(), Parsed, aError, sizeof(aError)));

	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "archive/Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Other.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Other", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 4321));
}

TEST(QmLiveReplaySidecar, RejectsInvalidTimelineFields)
{
	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	Sidecar.SetEndTick(90);
	ASSERT_TRUE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	ASSERT_TRUE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));
	const std::string Json = Sidecar.BuildJson();

	SLiveReplaySidecarData Parsed;
	char aError[128];

	std::string InvalidRecording = Json;
	size_t Pos = InvalidRecording.find("\"end_tick\": 90");
	ASSERT_NE(Pos, std::string::npos);
	InvalidRecording.replace(Pos, std::string("\"end_tick\": 90").size(), "\"end_tick\": 9");
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidRecording.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid recording tick range");

	std::string InvalidFinishTeam = Json;
	Pos = InvalidFinishTeam.find("\"team\": 4");
	ASSERT_NE(Pos, std::string::npos);
	InvalidFinishTeam.replace(Pos, std::string("\"team\": 4").size(), "\"team\": 0");
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidFinishTeam.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid finish event");

	std::string InvalidTeamClient = Json;
	const size_t TeamEventsPos = InvalidTeamClient.find("\"team_events\"");
	ASSERT_NE(TeamEventsPos, std::string::npos);
	Pos = InvalidTeamClient.find("\"client_id\": 7", TeamEventsPos);
	ASSERT_NE(Pos, std::string::npos);
	const std::string InvalidClient = "\"client_id\": " + std::to_string(MAX_CLIENTS);
	InvalidTeamClient.replace(Pos, std::string("\"client_id\": 7").size(), InvalidClient);
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidTeamClient.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid team event");
}

TEST(QmLiveFinishRanking, ResolvesPendingFinishTeamAttribution)
{
	CLiveFinishRanking Ranking;
	CLiveFinishRanking::CResult Pending = Ranking.OnFinishMessage(5, 151420, 100, false, -1);
	EXPECT_EQ(Pending.m_Status, CLiveFinishRanking::EFinishStatus::PENDING);
	EXPECT_TRUE(Ranking.Events().empty());

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	aTeams[5] = 4;
	const CLiveFinishRanking::CResolveResult Resolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 101);

	ASSERT_EQ(Resolved.m_vAccepted.size(), 1u);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_Team, 4);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_ClientId, 5);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_TimeMs, 151420);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Rank, 1);
}

TEST(QmLiveFinishRanking, KeepsPendingFinishUntilTeamBecomesReliable)
{
	CLiveFinishRanking Ranking;
	Ranking.OnFinishMessage(5, 151420, 100, false, -1);

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	CLiveFinishRanking::CResolveResult Unresolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 101);
	EXPECT_TRUE(Unresolved.m_vAccepted.empty());
	EXPECT_EQ(Unresolved.m_DroppedPending, 0);

	aTeams[5] = 4;
	CLiveFinishRanking::CResolveResult Resolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 102);
	ASSERT_EQ(Resolved.m_vAccepted.size(), 1u);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_Team, 4);
}

TEST(QmLiveFinishRanking, IgnoresInvalidTeamsAfterTeamStateIsKnown)
{
	CLiveFinishRanking Ranking;

	EXPECT_EQ(Ranking.OnFinishMessage(1, 1000, 10, true, TEAM_FLOCK).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_EQ(Ranking.OnFinishMessage(2, 1000, 11, true, TEAM_SUPER).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_EQ(Ranking.OnFinishMessage(MAX_CLIENTS, 1000, 12, true, 3).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_TRUE(Ranking.Events().empty());
}

TEST(QmLiveFinishRanking, DeduplicatesSameTeamFinishMessages)
{
	CLiveFinishRanking Ranking;

	EXPECT_EQ(Ranking.OnFinishMessage(1, 90000, 200, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::ACCEPTED);
	EXPECT_EQ(Ranking.OnFinishMessage(2, 90000, 201, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::DUPLICATE);
	EXPECT_EQ(Ranking.OnFinishMessage(3, 91000, 260, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::DUPLICATE);

	ASSERT_EQ(Ranking.Events().size(), 1u);
	EXPECT_EQ(Ranking.Events()[0].m_Team, 7);
	EXPECT_EQ(Ranking.Events()[0].m_ClientId, 1);
}

TEST(QmLiveFinishRanking, SortsFinishedTeamsByOfficialTime)
{
	CLiveFinishRanking Ranking;

	Ranking.OnFinishMessage(1, 200000, 50, true, 2);
	Ranking.OnFinishMessage(2, 100000, 55, true, 3);

	EXPECT_EQ(Ranking.RankForTeam(3, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
}

TEST(QmLiveFinishRanking, UsesStableTieBreakersForEqualFinishTimes)
{
	CLiveFinishRanking Ranking;

	Ranking.OnFinishMessage(2, 100000, 10, true, 2);
	Ranking.OnFinishMessage(1, 100000, 10, true, 1);
	Ranking.OnFinishMessage(3, 100000, 20, true, 3);

	EXPECT_EQ(Ranking.RankForTeam(1, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
	EXPECT_EQ(Ranking.RankForTeam(3, false), 3);
}

TEST(QmLiveFinishRanking, FiltersConfiguredTeamRangeWithoutChangingInternalLog)
{
	CLiveFinishRanking Ranking;
	Ranking.SetTeamRange(1, 2);

	Ranking.OnFinishMessage(5, 80000, 10, true, 5);
	Ranking.OnFinishMessage(2, 120000, 20, true, 2);

	ASSERT_EQ(Ranking.Events().size(), 2u);
	EXPECT_FALSE(Ranking.IsTeamInConfiguredRange(5));
	EXPECT_EQ(Ranking.RankForTeam(5, false), 0);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
	EXPECT_EQ(Ranking.RankForTeam(5, true), 1);
}

TEST(QmLiveFinishRanking, ResetClearsEventsPendingAndCards)
{
	CLiveFinishRanking Ranking;
	Ranking.OnFinishMessage(1, 1000, 10, false, -1);
	const CLiveFinishRanking::CResult Accepted = Ranking.OnFinishMessage(2, 2000, 11, true, 2);
	Ranking.EnqueueCard(Accepted.m_Event, 1, 11);
	ASSERT_NE(Ranking.VisibleCard(11, 50), nullptr);

	Ranking.Reset();

	EXPECT_TRUE(Ranking.Events().empty());
	EXPECT_EQ(Ranking.VisibleCard(12, 50), nullptr);
}

TEST(QmLiveFinishRanking, SidecarMissingAndDamagedInputFailSafely)
{
	std::vector<CLiveFinishEvent> vEvents;
	EXPECT_FALSE(CLiveFinishRanking::ParseSidecarJson(nullptr, 0, vEvents));
	EXPECT_FALSE(CLiveFinishRanking::ParseSidecarJson("{\"finish_events\":[", 18, vEvents));
	EXPECT_TRUE(vEvents.empty());
}

TEST(QmLiveFinishRanking, SidecarJsonUsesLiveReplayTimeFieldAndAcceptsLegacyTimeMs)
{
	std::vector<CLiveFinishEvent> vSource;
	vSource.push_back({2, 1, 12345, 67});
	const std::string Json = CLiveFinishRanking::EventsToSidecarJson(vSource);
	EXPECT_NE(Json.find("\"time\""), std::string::npos);
	EXPECT_EQ(Json.find("time_ms"), std::string::npos);

	std::vector<CLiveFinishEvent> vEvents;
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(Json.c_str(), Json.size(), vEvents));
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_TimeMs, 12345);

	const char *pLegacyJson = "{\"finish_events\":[{\"team\":2,\"client_id\":1,\"time_ms\":12345,\"tick\":67}]}";
	vEvents.clear();
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(pLegacyJson, str_length(pLegacyJson), vEvents));
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_TimeMs, 12345);
}

TEST(QmLiveFinishRanking, SidecarRebuildUsesOnlyEventsAtOrBeforeCurrentTick)
{
	const char *pJson = "{\"version\":1,\"finish_events\":[{\"team\":3,\"client_id\":3,\"time\":80000,\"tick\":90},{\"team\":4,\"client_id\":4,\"time\":70000,\"tick\":150}]}";
	std::vector<CLiveFinishEvent> vEvents;
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(pJson, str_length(pJson), vEvents));

	CLiveFinishRanking Ranking;
	EXPECT_TRUE(Ranking.RebuildFromEvents(vEvents, 100));

	ASSERT_EQ(Ranking.Events().size(), 1u);
	EXPECT_EQ(Ranking.Events()[0].m_Team, 3);
	EXPECT_EQ(Ranking.RankForTeam(3, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(4, false), 0);
}
