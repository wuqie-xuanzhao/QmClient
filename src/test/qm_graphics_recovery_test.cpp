#include <engine/client/backend/graphics_backend_contract.h>

#include <gtest/gtest.h>

TEST(QmGraphicsRecovery, WindowedModeRemainsWindowed)
{
	EXPECT_EQ(graphics_backend::RecoveryFullscreenMode(0), 0);
}

TEST(QmGraphicsRecovery, FullscreenModesUseDesktopFullscreen)
{
	EXPECT_EQ(graphics_backend::RecoveryFullscreenMode(1), 2);
	EXPECT_EQ(graphics_backend::RecoveryFullscreenMode(2), 2);
	EXPECT_EQ(graphics_backend::RecoveryFullscreenMode(3), 2);
}
