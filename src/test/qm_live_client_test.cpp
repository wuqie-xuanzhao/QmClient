#include <game/client/live/live_director.h>
#include <game/client/live/live_replay_buffer.h>
#include <game/teamscore.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

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
