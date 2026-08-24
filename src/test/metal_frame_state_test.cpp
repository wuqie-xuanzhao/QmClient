#include <engine/client/backend/metal/metal_frame_state.h>

#include <gtest/gtest.h>

TEST(MetalFrameState, FinalizesAFrameOnlyOnce)
{
	CMetalFrameState State;
	ASSERT_TRUE(State.BeginFrame(0));
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::PRESENTED);
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::ALREADY_FINALIZED);
	EXPECT_EQ(State.LastPresentedFrameId(), 1U);
}

TEST(MetalFrameState, CannotFinalizeBeforeStartingAFrame)
{
	CMetalFrameState State;
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::FAILED);
	EXPECT_FALSE(State.CaptureRetained());
}

TEST(MetalFrameState, ScreenshotAndReadPixelSharePresentedFrame)
{
	CMetalFrameState State;
	CMetalFrameState::SFrameCapture Capture;
	ASSERT_TRUE(State.BeginFrame(1));
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::PRESENTED);
	ASSERT_TRUE(State.ReadLastPresentedFrame(Capture));
	EXPECT_EQ(Capture.m_FrameId, State.CurrentFrameId());
	EXPECT_EQ(Capture.m_Slot, 1U);
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::ALREADY_FINALIZED);
}

TEST(MetalFrameState, DrawableFailureDoesNotPresentOrRetainCapture)
{
	CMetalFrameState State;
	ASSERT_TRUE(State.BeginFrame(0));
	EXPECT_EQ(State.FinalizeFrameForPresent(false), CMetalFrameState::EFinalizeResult::FAILED);
	EXPECT_TRUE(State.CurrentFrameFailed());
	EXPECT_FALSE(State.CaptureRetained());
	CMetalFrameState::SFrameCapture Capture;
	EXPECT_FALSE(State.ReadLastPresentedFrame(Capture));
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::FAILED);
}

TEST(MetalFrameState, PreviousCaptureLivesUntilNextSuccessfulFinalize)
{
	CMetalFrameState State;
	CMetalFrameState::SFrameCapture Capture;
	ASSERT_TRUE(State.BeginFrame(0));
	ASSERT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::PRESENTED);
	ASSERT_TRUE(State.ReadLastPresentedFrame(Capture));
	EXPECT_EQ(Capture.m_FrameId, 1U);

	ASSERT_TRUE(State.BeginFrame(1));
	EXPECT_TRUE(State.CaptureRetained());
	EXPECT_EQ(State.FinalizeFrameForPresent(false), CMetalFrameState::EFinalizeResult::FAILED);
	EXPECT_TRUE(State.CaptureRetained());
	ASSERT_TRUE(State.BeginFrame(2));
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::PRESENTED);
	ASSERT_TRUE(State.ReadLastPresentedFrame(Capture));
	EXPECT_EQ(Capture.m_FrameId, 3U);
	EXPECT_EQ(Capture.m_Slot, 2U);
}

TEST(MetalFrameState, CannotStartAnotherFrameBeforeFinalization)
{
	CMetalFrameState State;
	ASSERT_TRUE(State.BeginFrame(0));
	EXPECT_FALSE(State.BeginFrame(1));
	EXPECT_EQ(State.FinalizeFrameForPresent(true), CMetalFrameState::EFinalizeResult::PRESENTED);
	EXPECT_TRUE(State.BeginFrame(1));
}
