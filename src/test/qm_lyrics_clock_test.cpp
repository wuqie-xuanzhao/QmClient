// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_clock.h>
#include <game/client/components/system_media_controls.h>
#include <game/client/components/system_media_controls_timeline.h>

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

using namespace QmLyrics;

namespace
{

	constexpr int64_t TICK_FREQ = 1000000; // 1 MHz fake clock
	constexpr int64_t MsToTicks(int64_t Ms) { return Ms * TICK_FREQ / 1000; }

	SPlaybackSnapshot PlaybackSnapshot(int64_t PositionMs, int64_t PositionUpdatedMs, uint64_t TimelineGeneration, bool Playing, bool IdentityChanged = false, double PlaybackRate = 1.0)
	{
		SPlaybackSnapshot Snapshot;
		Snapshot.m_PositionMs = PositionMs;
		Snapshot.m_PositionUpdatedTick = MsToTicks(PositionUpdatedMs);
		Snapshot.m_TimelineGeneration = TimelineGeneration;
		Snapshot.m_PlaybackRate = PlaybackRate;
		Snapshot.m_Playing = Playing;
		Snapshot.m_IdentityChanged = IdentityChanged;
		return Snapshot;
	}

} // namespace

TEST(QmLyricsClock, DelayedLyricsLoadUsesCurrentPlayerPosition)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);

	// The lyric request takes five seconds. Publishing the track must not restart
	// the playback clock or require intermediate lyric-render calls.
	EXPECT_EQ(C.Now(MsToTicks(15000), TICK_FREQ), 65000);
}

TEST(SystemMediaTimeline, NormalizesNonZeroStartAndMapsSampleTick)
{
	SystemMediaControls::STimelineProperties Properties;
	Properties.m_Start100ns = 30LL * 10000000;
	Properties.m_End100ns = 210LL * 10000000;
	Properties.m_Position100ns = 60LL * 10000000;
	Properties.m_LastUpdatedUtc100ns = 1000LL * 10000000;

	const SystemMediaControls::STimelineSnapshot Snapshot = SystemMediaControls::NormalizeTimelineProperties(
		Properties,
		1002LL * 10000000,
		MsToTicks(5000),
		TICK_FREQ);
	EXPECT_EQ(Snapshot.m_PositionMs, 30000);
	EXPECT_EQ(Snapshot.m_DurationMs, 180000);
	EXPECT_EQ(Snapshot.m_PositionUpdatedTick, MsToTicks(3000));
}

TEST(SystemMediaAlbumArt, DecodeSizeCapsLargeCoversAndPreservesAspectRatio)
{
	const SystemMediaControls::SAlbumArtDecodeSize Square = SystemMediaControls::CalculateAlbumArtDecodeSize(2048, 2048);
	EXPECT_EQ(Square.m_Width, 256u);
	EXPECT_EQ(Square.m_Height, 256u);

	const SystemMediaControls::SAlbumArtDecodeSize Landscape = SystemMediaControls::CalculateAlbumArtDecodeSize(1200, 600);
	EXPECT_EQ(Landscape.m_Width, 256u);
	EXPECT_EQ(Landscape.m_Height, 128u);

	const SystemMediaControls::SAlbumArtDecodeSize Portrait = SystemMediaControls::CalculateAlbumArtDecodeSize(600, 1200);
	EXPECT_EQ(Portrait.m_Width, 128u);
	EXPECT_EQ(Portrait.m_Height, 256u);
}

TEST(SystemMediaAlbumArt, DecodeSizeKeepsSmallCoversAndRejectsEmptyDimensions)
{
	const SystemMediaControls::SAlbumArtDecodeSize Small = SystemMediaControls::CalculateAlbumArtDecodeSize(200, 100);
	EXPECT_EQ(Small.m_Width, 200u);
	EXPECT_EQ(Small.m_Height, 100u);

	const SystemMediaControls::SAlbumArtDecodeSize EmptyWidth = SystemMediaControls::CalculateAlbumArtDecodeSize(0, 100);
	EXPECT_EQ(EmptyWidth.m_Width, 0u);
	EXPECT_EQ(EmptyWidth.m_Height, 0u);
	const SystemMediaControls::SAlbumArtDecodeSize EmptyHeight = SystemMediaControls::CalculateAlbumArtDecodeSize(100, 0);
	EXPECT_EQ(EmptyHeight.m_Width, 0u);
	EXPECT_EQ(EmptyHeight.m_Height, 0u);
}

TEST(SystemMediaAlbumArt, CircularMaskKeepsCenterAndRejectsOutside)
{
	constexpr float Feather = 4.0f;
	EXPECT_FLOAT_EQ(SystemMediaControls::AlbumArtCircleMaskAlpha(128.0f, 128.0f, 256, 256, Feather), 1.0f);
	EXPECT_FLOAT_EQ(SystemMediaControls::AlbumArtCircleMaskAlpha(0.0f, 0.0f, 256, 256, Feather), 0.0f);
	EXPECT_FLOAT_EQ(SystemMediaControls::AlbumArtCircleMaskAlpha(256.0f, 128.0f, 256, 256, Feather), 0.0f);
	EXPECT_FLOAT_EQ(SystemMediaControls::AlbumArtCircleMaskAlpha(128.0f, 128.0f, 0, 256, Feather), 0.0f);
}

TEST(SystemMediaAlbumArt, CircularMaskUsesSmoothInwardFeather)
{
	constexpr float Feather = 4.0f;
	const float TransparentEdge = SystemMediaControls::AlbumArtCircleMaskAlpha(255.5f, 128.0f, 256, 256, Feather);
	const float FeatherMiddle = SystemMediaControls::AlbumArtCircleMaskAlpha(253.5f, 128.0f, 256, 256, Feather);
	const float OpaqueInside = SystemMediaControls::AlbumArtCircleMaskAlpha(251.0f, 128.0f, 256, 256, Feather);

	EXPECT_FLOAT_EQ(TransparentEdge, 0.0f);
	EXPECT_GT(FeatherMiddle, 0.0f);
	EXPECT_LT(FeatherMiddle, 1.0f);
	EXPECT_FLOAT_EQ(OpaqueInside, 1.0f);
}

TEST(SystemMediaAlbumArt, ExpensivePixelMaskRunsBeforeWorkerPublishesCover)
{
	std::ifstream File(TestSourcePath("src/game/client/components/system_media_controls.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t DecodeBegin = Source.find("static void UpdateAlbumArtData");
	const size_t MainThreadBegin = Source.find("static void ApplySharedAlbumArt");
	ASSERT_NE(DecodeBegin, std::string::npos);
	ASSERT_NE(MainThreadBegin, std::string::npos);
	ASSERT_LT(DecodeBegin, MainThreadBegin);
	const std::string DecodeBody = Source.substr(DecodeBegin, MainThreadBegin - DecodeBegin);
	const std::string MainThreadBody = Source.substr(MainThreadBegin);
	EXPECT_NE(DecodeBody.find("BitmapAlphaMode::Straight"), std::string::npos);
	EXPECT_NE(DecodeBody.find("ApplyCircularFeatherMask(CircularCopy"), std::string::npos);
	EXPECT_LT(DecodeBody.find("ApplyCircularFeatherMask(CircularCopy"), DecodeBody.find("SetSharedAlbumArt"));
	EXPECT_NE(DecodeBody.find("ApplyRoundedMask(Copy"), std::string::npos);
	EXPECT_LT(DecodeBody.find("ApplyRoundedMask(Copy"), DecodeBody.find("SetSharedAlbumArt"));
	EXPECT_EQ(MainThreadBody.find("ApplyCircularFeatherMask"), std::string::npos);
	EXPECT_EQ(MainThreadBody.find("ApplyRoundedMask"), std::string::npos);
}

TEST(SystemMediaAlbumArt, CircularMediaIslandTextureDoesNotReplaceLegacyCover)
{
	std::ifstream File(TestSourcePath("src/game/client/components/hud.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("TrackInput.m_Cover = MediaState.m_AlbumArtCircular;"), std::string::npos);
	EXPECT_NE(Source.find("Graphics()->TextureSet(MediaState.m_AlbumArt);"), std::string::npos);
}

TEST(SystemMediaTimeline, LastUpdatedChangeAdvancesTimelineGeneration)
{
	SystemMediaControls::CTimelineGenerationTracker Tracker;
	SystemMediaControls::STimelineProperties Properties;
	Properties.m_End100ns = 180LL * 10000000;
	Properties.m_Position100ns = 60LL * 10000000;
	Properties.m_LastUpdatedUtc100ns = 1000LL * 10000000;

	EXPECT_EQ(Tracker.Update(Properties), 1u);
	EXPECT_EQ(Tracker.Update(Properties), 1u);
	Properties.m_LastUpdatedUtc100ns += 10000000;
	EXPECT_EQ(Tracker.Update(Properties), 2u);
}

TEST(SystemMediaTimeline, ClampsPositionAndRejectsFutureSampleAge)
{
	SystemMediaControls::STimelineProperties Properties;
	Properties.m_Start100ns = 30LL * 10000000;
	Properties.m_End100ns = 210LL * 10000000;
	Properties.m_Position100ns = 250LL * 10000000;
	Properties.m_LastUpdatedUtc100ns = 1005LL * 10000000;

	SystemMediaControls::STimelineSnapshot Snapshot = SystemMediaControls::NormalizeTimelineProperties(
		Properties,
		1002LL * 10000000,
		MsToTicks(5000),
		TICK_FREQ);
	EXPECT_EQ(Snapshot.m_PositionMs, 180000);
	EXPECT_EQ(Snapshot.m_PositionUpdatedTick, MsToTicks(5000));

	Properties.m_Position100ns = 20LL * 10000000;
	Snapshot = SystemMediaControls::NormalizeTimelineProperties(
		Properties,
		1002LL * 10000000,
		MsToTicks(5000),
		TICK_FREQ);
	EXPECT_EQ(Snapshot.m_PositionMs, 0);
}

TEST(SystemMediaTimeline, SampleBeforeProcessStartPreservesFullAge)
{
	SystemMediaControls::STimelineProperties Properties;
	Properties.m_End100ns = 180LL * 10000000;
	Properties.m_Position100ns = 60LL * 10000000;
	Properties.m_LastUpdatedUtc100ns = 940LL * 10000000;
	const SystemMediaControls::STimelineSnapshot TimelineSnapshot = SystemMediaControls::NormalizeTimelineProperties(
		Properties,
		1000LL * 10000000,
		MsToTicks(5000),
		TICK_FREQ);
	EXPECT_EQ(TimelineSnapshot.m_PositionUpdatedTick, MsToTicks(-55000));

	SPlaybackSnapshot Playback;
	Playback.m_PositionMs = TimelineSnapshot.m_PositionMs;
	Playback.m_PositionUpdatedTick = TimelineSnapshot.m_PositionUpdatedTick;
	Playback.m_TimelineGeneration = 1;
	Playback.m_PlaybackRate = 1.0;
	Playback.m_Playing = true;
	Playback.m_IdentityChanged = true;
	CClockInterpolator C;
	C.Update(Playback, MsToTicks(5000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(5000), TICK_FREQ), 120000);
}

TEST(QmLyricsClock, ResumeWithStaleTimelineDoesNotCountPausedTime)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 61000);

	// PlaybackInfo changes independently while the timeline sample remains stale.
	C.Update(PlaybackSnapshot(60000, 10000, 1, false), MsToTicks(11000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 61000);

	C.Update(PlaybackSnapshot(60000, 10000, 1, true), MsToTicks(20000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 61000);
	EXPECT_EQ(C.Now(MsToTicks(21000), TICK_FREQ), 62000);
}

TEST(QmLyricsClock, FreshPauseUsesReportedTimelinePosition)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 61000);

	C.Update(PlaybackSnapshot(61100, 11000, 2, false), MsToTicks(11200), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 61100);
}

TEST(QmLyricsClock, FreshResumeUsesTimelineSampleAge)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(61100, 11000, 1, false, true), MsToTicks(11200), TICK_FREQ);
	C.Update(PlaybackSnapshot(61100, 20000, 2, true), MsToTicks(20200), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20200), TICK_FREQ), 61300);
}

TEST(QmLyricsClock, StaleResumeAppliesNewPlaybackRate)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, false, true), MsToTicks(10000), TICK_FREQ);
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, false, 2.0), MsToTicks(20000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 60000);
	EXPECT_EQ(C.Now(MsToTicks(21000), TICK_FREQ), 62000);
}

TEST(QmLyricsClock, RepeatedStaleSnapshotKeepsLocalClockAdvancing)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	C.Update(PlaybackSnapshot(60000, 10000, 1, true), MsToTicks(15000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(15000), TICK_FREQ), 65000);
}

TEST(QmLyricsClock, FreshTimelineHardSnapsForwardAndBackwardSeek)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(500);
	C.Update(PlaybackSnapshot(10000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 11000);

	C.Update(PlaybackSnapshot(60000, 12000, 2, true), MsToTicks(12500), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(12500), TICK_FREQ), 60500);

	C.Update(PlaybackSnapshot(5000, 13000, 3, true), MsToTicks(13000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(13000), TICK_FREQ), 5000);
}

TEST(QmLyricsClock, SamePositionWithFreshTimelineCanCorrectDrift)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(500);
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 61000);

	C.Update(PlaybackSnapshot(60000, 11000, 2, true), MsToTicks(11000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 60000);
}

TEST(QmLyricsClock, ZeroPlaybackRateDoesNotAdvance)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true, 0.0), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 60000);
}

TEST(QmLyricsClock, IdentityChangeHardSnapsEvenWhenTimelineGenerationMatches)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(60000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 61000);

	C.Update(PlaybackSnapshot(5000, 20000, 1, true, true), MsToTicks(20000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 5000);
}

TEST(QmLyricsClock, SmallCorrectionDependsOnElapsedTimeNotRenderCount)
{
	CClockInterpolator SparseReads;
	CClockInterpolator FrequentReads;
	for(CClockInterpolator *pClock : {&SparseReads, &FrequentReads})
	{
		pClock->SetDriftCorrectMs(1000);
		pClock->Update(PlaybackSnapshot(0, 0, 1, true, true), MsToTicks(0), TICK_FREQ);
		EXPECT_EQ(pClock->Now(MsToTicks(100), TICK_FREQ), 100);
		pClock->Update(PlaybackSnapshot(300, 100, 2, true), MsToTicks(100), TICK_FREQ);
	}

	for(int Ms = 200; Ms <= 500; Ms += 100)
		FrequentReads.Now(MsToTicks(Ms), TICK_FREQ);

	EXPECT_EQ(SparseReads.Now(MsToTicks(600), TICK_FREQ), FrequentReads.Now(MsToTicks(600), TICK_FREQ));
}

TEST(QmLyricsClock, ResetIsZero)
{
	CClockInterpolator C;
	EXPECT_EQ(C.Now(0, TICK_FREQ), 0);
}

TEST(QmLyricsClock, PausedSnapshotStaysFixed)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(10000, 100, 1, false, true), MsToTicks(500), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(5000), TICK_FREQ), 10000);
}

TEST(QmLyricsClock, AppliesOffsetAtReadTime)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(10000, 0, 1, true, true), MsToTicks(0), TICK_FREQ);
	C.SetOffsetMs(500);
	EXPECT_EQ(C.Now(MsToTicks(0), TICK_FREQ), 10500);
}

TEST(QmLyricsClock, PlaybackRateScalesAdvance)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(0, 0, 1, true, true, 2.0), MsToTicks(0), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(1000), TICK_FREQ), 2000);
}

TEST(QmLyricsClock, SmallDriftPreservesContinuityAndConvergesOverTime)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(1000);
	C.Update(PlaybackSnapshot(0, 0, 1, true, true), MsToTicks(0), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(100), TICK_FREQ), 100);
	// Re-anchor to +200ms drift. The displayed position stays continuous at the
	// sample instant and then converges according to elapsed wall time.
	C.Update(PlaybackSnapshot(300, 100, 2, true), MsToTicks(100), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(100), TICK_FREQ), 100);
	const int64_t Later = C.Now(MsToTicks(400), TICK_FREQ);
	EXPECT_GT(Later, 400);
	EXPECT_LT(Later, 600);
}

TEST(QmLyricsClock, SeekWhilePausedResumesFromSeekPosition)
{
	CClockInterpolator C;
	C.Update(PlaybackSnapshot(10000, 10000, 1, false, true), MsToTicks(10000), TICK_FREQ);
	C.Update(PlaybackSnapshot(45000, 12000, 2, false), MsToTicks(12000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 45000);
	C.Update(PlaybackSnapshot(45000, 12000, 2, true), MsToTicks(20000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(21000), TICK_FREQ), 46000);
}

TEST(QmLyricsClock, SmallSeekWhilePausedSnapsImmediately)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(1000);
	C.Update(PlaybackSnapshot(10000, 10000, 1, false, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(10000), TICK_FREQ), 10000);
	C.Update(PlaybackSnapshot(10200, 11000, 2, false), MsToTicks(11000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 10200);
	EXPECT_EQ(C.Now(MsToTicks(20000), TICK_FREQ), 10200);
}

TEST(QmLyricsClock, FreshSeekOverThresholdSnapsImmediately)
{
	CClockInterpolator C;
	C.SetDriftCorrectMs(300);
	C.Update(PlaybackSnapshot(10000, 10000, 1, true, true), MsToTicks(10000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 11000);

	C.Update(PlaybackSnapshot(11500, 11000, 2, true), MsToTicks(11000), TICK_FREQ);
	EXPECT_EQ(C.Now(MsToTicks(11000), TICK_FREQ), 11500);
}
