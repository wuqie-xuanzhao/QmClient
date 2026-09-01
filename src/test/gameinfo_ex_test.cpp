#include <game/teamscore.h>

#include <gtest/gtest.h>

TEST(GameInfoEx, NormalizesTeamSchema)
{
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(VANILLA_MAX_CLIENTS + 1), VANILLA_MAX_CLIENTS + 1);
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(LEGACY_MAX_CLIENTS + 1), LEGACY_MAX_CLIENTS + 1);
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(NUM_DDRACE_TEAMS), NUM_DDRACE_TEAMS);
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(0), LEGACY_MAX_CLIENTS + 1);
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(VANILLA_MAX_CLIENTS), LEGACY_MAX_CLIENTS + 1);
	EXPECT_EQ(QmNormalizeNumDDRaceTeams(NUM_DDRACE_TEAMS + 1), LEGACY_MAX_CLIENTS + 1);
}

TEST(GameInfoEx, TeamSuperFollowsAdvertisedSchema)
{
	CTeamsCore Teams;
	EXPECT_EQ(Teams.TeamSuper(), NUM_DDRACE_TEAMS - 1);

	Teams.m_NumDDRaceTeams = VANILLA_MAX_CLIENTS + 1;
	EXPECT_EQ(Teams.TeamSuper(), VANILLA_TEAM_SUPER);

	Teams.m_NumDDRaceTeams = LEGACY_MAX_CLIENTS + 1;
	EXPECT_EQ(Teams.TeamSuper(), LEGACY_TEAM_SUPER);

	Teams.m_NumDDRaceTeams = NUM_DDRACE_TEAMS;
	EXPECT_EQ(Teams.TeamSuper(), TEAM_SUPER);
}
