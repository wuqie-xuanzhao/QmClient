#include <game/client/components/binds.h>

#include <gtest/gtest.h>

TEST(Binds, AllowsUnmodifiedFallbackForSingleModifierKey)
{
	EXPECT_TRUE(CBinds::AllowsUnmodifiedFallback(KEY_LSHIFT, KeyModifier::NONE));
	EXPECT_TRUE(CBinds::AllowsUnmodifiedFallback(KEY_RSHIFT, KeyModifier::NONE));
}

TEST(Binds, BlocksShiftOnlyBindFallbackForScreenshotCombinations)
{
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_LSHIFT, 1 << KeyModifier::CTRL));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_RSHIFT, 1 << KeyModifier::CTRL));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_LSHIFT, 1 << KeyModifier::ALT));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_RSHIFT, 1 << KeyModifier::ALT));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_LSHIFT, 1 << KeyModifier::GUI));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_RSHIFT, 1 << KeyModifier::GUI));
}

TEST(Binds, KeepsPlainKeyFallbackForNonScreenshotCombinations)
{
	EXPECT_TRUE(CBinds::AllowsUnmodifiedFallback(KEY_C, 1 << KeyModifier::CTRL));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_C, (1 << KeyModifier::CTRL) | (1 << KeyModifier::SHIFT)));
	EXPECT_FALSE(CBinds::AllowsUnmodifiedFallback(KEY_C, (1 << KeyModifier::GUI) | (1 << KeyModifier::SHIFT)));
}

TEST(Binds, ReleasesShiftOnlyBindWhenScreenshotModifierIsPressedLater)
{
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_LSHIFT, KeyModifier::NONE), 1 << KeyModifier::CTRL));
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_RSHIFT, KeyModifier::NONE), 1 << KeyModifier::CTRL));
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_LSHIFT, KeyModifier::NONE), 1 << KeyModifier::ALT));
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_RSHIFT, KeyModifier::NONE), 1 << KeyModifier::ALT));
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_LSHIFT, KeyModifier::NONE), 1 << KeyModifier::GUI));
	EXPECT_TRUE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_RSHIFT, KeyModifier::NONE), 1 << KeyModifier::GUI));
	EXPECT_FALSE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_C, KeyModifier::NONE), 1 << KeyModifier::CTRL));
	EXPECT_FALSE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_LSHIFT, 1 << KeyModifier::SHIFT), 1 << KeyModifier::CTRL));
	EXPECT_FALSE(CBinds::ShouldReleaseUnmodifiedModifierBindOnModifierPress(CBindSlot(KEY_LSHIFT, KeyModifier::NONE), (1 << KeyModifier::CTRL) | (1 << KeyModifier::ALT)));
}
