#include "test.h"

#include <engine/shared/config.h>

#include <game/client/ui_scrollregion.h>

#include <gtest/gtest.h>

namespace
{
	void BeginRegion(CScrollRegion &Region, CUIRect &Clip, vec2 &Offset)
	{
		Region.Begin(&Clip, &Offset);
	}
}

TEST(UiScrollRegion, DirectScrollUpdatesOffset)
{
	CScrollRegion Region;
	CUIRect Clip{0.0f, 0.0f, 200.0f, 100.0f};
	vec2 Offset{0.0f, 0.0f};

	BeginRegion(Region, Clip, Offset);
	Region.SetContentHeightForNextFrame(400.0f);
	Region.ScrollRelativeDirect(50.0f);
	Region.AddRect(CUIRect{0.0f, 0.0f, 100.0f, 400.0f});
	Region.End();

	EXPECT_LT(Region.ContentScrollOffsetY(), 0.0f);
	EXPECT_NEAR(Region.ContentScrollOffsetY(), -50.0f, 1.0f);
}

TEST(UiScrollRegion, SetScrollOffsetSynchronizesAnimatedState)
{
	CScrollRegion Region;
	CUIRect Clip{0.0f, 0.0f, 200.0f, 100.0f};
	vec2 Offset{0.0f, 0.0f};

	BeginRegion(Region, Clip, Offset);
	Region.SetContentHeightForNextFrame(400.0f);
	Region.SetScrollOffsetY(-60.0f);
	Region.AddRect(CUIRect{0.0f, 0.0f, 100.0f, 400.0f});
	Region.End();

	EXPECT_NEAR(Region.ContentScrollOffsetY(), -60.0f, 1.0f);
	EXPECT_FALSE(Region.Animating());
}

TEST(UiScrollRegion, DirectScrollUsesAnimatedTransition)
{
	CScrollRegion Region;
	CUIRect Clip{0.0f, 0.0f, 200.0f, 100.0f};
	vec2 Offset{0.0f, 0.0f};

	g_Config.m_UiSmoothScrollTime = 500;
	BeginRegion(Region, Clip, Offset);
	Region.SetContentHeightForNextFrame(400.0f);
	Region.ScrollRelativeDirect(80.0f);
	Region.AddRect(CUIRect{0.0f, 0.0f, 100.0f, 400.0f});
	Region.End();

	EXPECT_GT(Region.ContentScrollOffsetY(), -80.0f);
	EXPECT_LT(Region.ContentScrollOffsetY(), 0.0f);
	EXPECT_TRUE(Region.Animating());
}
