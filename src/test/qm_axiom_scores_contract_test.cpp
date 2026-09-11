#include <gtest/gtest.h>
#include <test/test.h>

#include <string>

TEST(QmAxiomScoresContract, QueriesOnlyFromTheAxiomScoreboardPopup)
{
	const std::string Scoreboard = ReadTestSourceFile("src/game/client/components/scoreboard.cpp");
	const std::string Component = ReadTestSourceFile("src/game/client/components/qmclient/axiom_scores.cpp");
	for(const char *pToken : {"m_QmAxiomAutoLogin.IsAxiomCommunity() && !GameClient()->ShouldHideStreamerIdentity(ClientId)", "m_QmAxiomScores.EnsureQueried(ClientData.m_aName);", "QmAxiomPopupSize(", "m_AxiomScrollRegion.Begin", "pResult->Mode(EQmAxiomMode::GORES)", "pResult->Mode(EQmAxiomMode::AXRACE)"})
		EXPECT_NE(Scoreboard.find(pToken), std::string::npos) << pToken;
	for(const char *pToken : {"AbortActiveRequests(true);", "QmAxiomResponseIsCurrent", "MaxResponseSize(AXIOM_MAX_RESPONSE_BYTES)", "AXIOM_INFO_TIMEOUT_MS = 90000"})
		EXPECT_NE(Component.find(pToken), std::string::npos) << pToken;
}

TEST(QmAxiomScoresContract, KeepsTheExistingDdnetPointsColumn)
{
	const std::string Scoreboard = ReadTestSourceFile("src/game/client/components/scoreboard.cpp");
	EXPECT_NE(Scoreboard.find("m_PlayerPoints.GetPoints(ClientData.m_aName)"), std::string::npos);
	EXPECT_NE(Scoreboard.find("m_PlayerPoints.EnsureQueried(GameClient()->m_aClients[i].m_aName)"), std::string::npos);
}
