#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_clock.h>

#include <gtest/gtest.h>

using namespace QmLyrics;

namespace
{

	constexpr int64_t TICK_FREQ = 1000000; // 1 MHz fake clock
	constexpr int64_t MsToTicks(int64_t Ms) { return Ms * TICK_FREQ / 1000; }

} // namespace

TEST(QmLyricsClock, ResetIsZero)
{
	CClockInterpolator C;
	EXPECT_EQ(C.Now(0, TICK_FREQ), 0);
}

TEST(QmLyricsClock, TimelineAnchorRequiresRealPlaybackChange)
{
	EXPECT_TRUE(ShouldUpdateTimelineAnchor(-1, 1.0, 0, 1.0, false));
	EXPECT_TRUE(ShouldUpdateTimelineAnchor(0, 1.0, 1000, 1.0, false));
	EXPECT_TRUE(ShouldUpdateTimelineAnchor(0, 1.0, 0, 1.25, false));
	EXPECT_TRUE(ShouldUpdateTimelineAnchor(0, 1.0, 0, 1.0, true));

	EXPECT_FALSE(ShouldUpdateTimelineAnchor(0, 1.0, 0, 1.0, false));
}

TEST(QmLyricsClock, RepeatedStaleTimelineSampleKeepsLocalClockAdvancing)
{
	CClockInterpolator C;
	C.Anchor(0, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));

	ASSERT_FALSE(ShouldUpdateTimelineAnchor(0, 1.0, 0, 1.0, false));
	EXPECT_EQ(C.Now(MsToTicks(1000), TICK_FREQ), 1000);
}

TEST(QmLyricsClock, AnchorReturnsPositionWhenNotPlaying)
{
	CClockInterpolator C;
	C.Anchor(10000, MsToTicks(100), 1.0);
	C.SetPlaying(false, MsToTicks(100));
	EXPECT_EQ(C.Now(MsToTicks(5000), TICK_FREQ), 10000);
}

TEST(QmLyricsClock, AdvancesWhilePlaying)
{
	CClockInterpolator C;
	C.Anchor(10000, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));
	// 第一次 Now 初始化 lastActual=target
	EXPECT_EQ(C.Now(MsToTicks(100), TICK_FREQ), 10100);
	// 正常本地推进不做平滑，否则歌词会天然落后。
	EXPECT_EQ(C.Now(MsToTicks(200), TICK_FREQ), 10200);
}

TEST(QmLyricsClock, AppliesOffset)
{
	CClockInterpolator C;
	C.Anchor(10000, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));
	C.SetOffsetMs(500);
	EXPECT_EQ(C.Now(MsToTicks(0), TICK_FREQ), 10500);
}

TEST(QmLyricsClock, HardSnapOnBigDrift)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(500);
	C.Anchor(0, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));
	C.Now(MsToTicks(100), TICK_FREQ); // 初始化
	// 重新 anchor 到大跳：当前 actual=100，target=10100，差 10000 > 500 → 硬切
	C.Anchor(10000, MsToTicks(100), 1.0);
	const int64_t Result = C.Now(MsToTicks(100), TICK_FREQ);
	EXPECT_EQ(Result, 10000);
}

TEST(QmLyricsClock, SmoothLerpOnSmallDrift)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(1000);
	C.Anchor(0, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));
	C.Now(MsToTicks(100), TICK_FREQ); // actual=100
	// 重 anchor 到偏移 +200ms：target=300，actual=100，差 200 < 1000 → 平滑
	C.Anchor(300, MsToTicks(100), 1.0);
	const int64_t Result = C.Now(MsToTicks(100), TICK_FREQ);
	// actual += (300-100)/5 = 40 → 140
	EXPECT_EQ(Result, 140);
}

TEST(QmLyricsClock, PauseFreezesPosition)
{
	CClockInterpolator C;
	C.Anchor(10000, MsToTicks(0), 1.0);
	C.SetPlaying(true, MsToTicks(0));
	C.Now(MsToTicks(500), TICK_FREQ); // 推进到 10500
	C.SetPlaying(false, MsToTicks(500));
	// 暂停后再过 2s
	EXPECT_EQ(C.Now(MsToTicks(2500), TICK_FREQ), 10500);
}

TEST(QmLyricsClock, RateScalesAdvance)
{
	CClockInterpolator C;
	C.Anchor(0, MsToTicks(0), 2.0); // 2x 速度
	C.SetPlaying(true, MsToTicks(0));
	EXPECT_EQ(C.Now(MsToTicks(1000), TICK_FREQ), 2000);
}

TEST(QmLyricsClock, AnchorUsesTimelineSampleTick)
{
	CClockInterpolator C;
	C.Anchor(60000, MsToTicks(10000), 1.0);
	C.SetPlaying(true, MsToTicks(10000));
	EXPECT_EQ(C.Now(MsToTicks(12500), TICK_FREQ), 62500);
}
