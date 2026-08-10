// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <engine/shared/qm_ime_policy.h>

#include <game/client/qm_ime_manager.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <string>

TEST(QmImePlatform, SystemCandidateUiPolicyMatchesPlatform)
{
#if defined(CONF_FAMILY_WINDOWS)
	EXPECT_FALSE(QmImeShouldUseSystemCandidateUi());
	EXPECT_TRUE(QmImeShouldRenderCustomCandidateUi());
#else
	EXPECT_TRUE(QmImeShouldUseSystemCandidateUi());
	EXPECT_FALSE(QmImeShouldRenderCustomCandidateUi());
#endif
}

TEST(QmImePlatform, CandidateRenderActionKeepsLifecycleValidationOnAllPlatforms)
{
	EXPECT_EQ(QmImeComputeCandidateRenderAction(false, 0), EQmImeCandidateRenderAction::VALIDATE_ONLY);
	EXPECT_EQ(QmImeComputeCandidateRenderAction(false, 1), EQmImeCandidateRenderAction::VALIDATE_ONLY);
	EXPECT_EQ(QmImeComputeCandidateRenderAction(true, 0), EQmImeCandidateRenderAction::LEGACY);
	EXPECT_EQ(QmImeComputeCandidateRenderAction(true, 1), EQmImeCandidateRenderAction::POPUP);
}

TEST(QmImePlatform, CandidateListNotifyFlagsSelectChangedList)
{
	EXPECT_TRUE(QmImeNotifyFlagsIncludeCandidateList(0, 0));
	EXPECT_FALSE(QmImeNotifyFlagsIncludeCandidateList(0, 1));
	EXPECT_TRUE(QmImeNotifyFlagsIncludeCandidateList(1u << 2, 2));
	EXPECT_FALSE(QmImeNotifyFlagsIncludeCandidateList(1u << 2, 0));
	EXPECT_TRUE(QmImeNotifyFlagsIncludeCandidateList((1u << 1) | (1u << 3), 3));
	EXPECT_FALSE(QmImeNotifyFlagsIncludeCandidateList(1u << 3, 32));
}

TEST(QmImePlatform, CandidatePageSizeFallsBackToCount)
{
	EXPECT_EQ(QmImeCandidatePageSizeOrCount(0, 9), 9u);
	EXPECT_EQ(QmImeCandidatePageSizeOrCount(5, 9), 5u);
}

TEST(QmImePlatform, PerfDiagnosticsDistinguishImeBlockingBoundaries)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/input.cpp");

	for(const char *pStage : {
		    "sdl_poll_event_call",
		    "process_system_message",
		    "ime_get_context",
		    "ime_candidate_size_query",
		    "ime_candidate_data_query",
		    "ime_release_context",
	    })
	{
		EXPECT_NE(Source.find(pStage), std::string::npos) << pStage;
	}
	EXPECT_NE(Source.find("wparam=%"), std::string::npos);
	EXPECT_NE(Source.find("lparam=%"), std::string::npos);
	EXPECT_NE(Source.find("candidate_index=%"), std::string::npos);
	EXPECT_NE(Source.find("returned_size=%"), std::string::npos);
}
