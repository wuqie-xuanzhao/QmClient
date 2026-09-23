#include <game/client/components/qmclient/tee_skin_apply.h>

#include <gtest/gtest.h>
TEST(QmTeeSkinApply, AppliesDummy)
{
	CConfig Config;
	EXPECT_EQ(QmTeeSkinApplyTargetForButton(2), ETeeSkinApplyTarget::DUMMY);
	EXPECT_EQ(QmTeeSkinApplyTargetForButton(1), ETeeSkinApplyTarget::MAIN);
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::DUMMY), 1);
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::MAIN), 0);

	QmApplyTeeSkinToTarget(Config, ETeeSkinApplyTarget::DUMMY, "test", true, true, 1, 2);
	EXPECT_STREQ(Config.m_ClDummySkin, "test");
	EXPECT_EQ(Config.m_ClDummyColorBody, 1);
	EXPECT_EQ(Config.m_ClDummyColorFeet, 2);
}
