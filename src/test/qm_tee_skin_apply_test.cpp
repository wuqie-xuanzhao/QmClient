#include <game/client/components/qmclient/tee_skin_apply.h>

#include <gtest/gtest.h>

TEST(QmTeeSkinApply, MapsMouseButtonsToExplicitTargets)
{
	EXPECT_EQ(QmTeeSkinApplyTargetForButton(2), ETeeSkinApplyTarget::DUMMY);
	EXPECT_EQ(QmTeeSkinApplyTargetForButton(1), ETeeSkinApplyTarget::MAIN);
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::DUMMY), 1);
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::MAIN), 0);
}

TEST(QmTeeSkinApply, AppliesColorKeyToDummyWithoutChangingMain)
{
	CConfig Config;
	str_copy(Config.m_ClPlayerSkin, "main");
	Config.m_ClPlayerUseCustomColor = 1;
	Config.m_ClPlayerColorBody = 10;
	Config.m_ClPlayerColorFeet = 20;

	QmApplyTeeSkinToTarget(Config, ETeeSkinApplyTarget::DUMMY, "dummy", true, true, 1, 2);
	EXPECT_STREQ(Config.m_ClDummySkin, "dummy");
	EXPECT_EQ(Config.m_ClDummyUseCustomColor, 1);
	EXPECT_EQ(Config.m_ClDummyColorBody, 1);
	EXPECT_EQ(Config.m_ClDummyColorFeet, 2);
	EXPECT_STREQ(Config.m_ClPlayerSkin, "main");
	EXPECT_EQ(Config.m_ClPlayerUseCustomColor, 1);
	EXPECT_EQ(Config.m_ClPlayerColorBody, 10);
	EXPECT_EQ(Config.m_ClPlayerColorFeet, 20);
}

TEST(QmTeeSkinApply, AppliesMainWithoutColorKeyAndPreservesColors)
{
	CConfig Config;
	str_copy(Config.m_ClDummySkin, "dummy");
	Config.m_ClPlayerUseCustomColor = 1;
	Config.m_ClPlayerColorBody = 10;
	Config.m_ClPlayerColorFeet = 20;

	QmApplyTeeSkinToTarget(Config, ETeeSkinApplyTarget::MAIN, "main", false, false, 1, 2);
	EXPECT_STREQ(Config.m_ClPlayerSkin, "main");
	EXPECT_EQ(Config.m_ClPlayerUseCustomColor, 1);
	EXPECT_EQ(Config.m_ClPlayerColorBody, 10);
	EXPECT_EQ(Config.m_ClPlayerColorFeet, 20);
	EXPECT_STREQ(Config.m_ClDummySkin, "dummy");
}

TEST(QmTeeSkinApply, DisabledColorKeyTurnsOffCustomColorWithoutOverwritingColors)
{
	CConfig Config;
	Config.m_ClDummyUseCustomColor = 1;
	Config.m_ClDummyColorBody = 10;
	Config.m_ClDummyColorFeet = 20;

	QmApplyTeeSkinToTarget(Config, ETeeSkinApplyTarget::DUMMY, "dummy", true, false, 1, 2);
	EXPECT_STREQ(Config.m_ClDummySkin, "dummy");
	EXPECT_EQ(Config.m_ClDummyUseCustomColor, 0);
	EXPECT_EQ(Config.m_ClDummyColorBody, 10);
	EXPECT_EQ(Config.m_ClDummyColorFeet, 20);
}
