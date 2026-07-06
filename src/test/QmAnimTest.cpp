#include "test.h"

#include <engine/shared/config.h>

#include <game/client/QmUi/QmAnim.h>
#include <game/client/QmUi/QmAnimCurves.h>
#include <game/client/QmUi/QmAnimResolve.h>
#include <game/client/QmUi/QmDropdown.h>
#include <game/client/QmUi/QmScroll.h>
#include <game/client/QmUi/QmTree.h>
#include <game/client/QmUi/UiContext.h>
#include <game/client/QmUi/UiForms.h>
#include <game/client/QmUi/UiMotion.h>
#include <game/client/QmUi/UiOverlays.h>
#include <game/client/QmUi/UiTokens.h>
#include <game/client/ui_rect.h>
#include <game/client/ui_scrollregion.h>

#include <gtest/gtest.h>

#include <cmath>

namespace
{
	void AdvanceFor(CUiV2AnimationRuntime &Runtime, float Seconds)
	{
		g_Config.m_QmUiMotionLevel = 2;
		const float Dt = 1.0f / 60.0f;
		int Steps = static_cast<int>(Seconds / Dt) + 1;
		for(int i = 0; i < Steps; ++i)
			Runtime.Advance(Dt);
	}

	SUiAnimRequest MakeRequest(uint64_t NodeKey, EUiAnimProperty Property, float Target, float DurationSec, int Priority, EUiAnimInterruptPolicy Interrupt, uint32_t TrackId)
	{
		g_Config.m_QmUiMotionLevel = 2;
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition.m_DurationSec = DurationSec;
		Request.m_Transition.m_Priority = Priority;
		Request.m_Transition.m_Interrupt = Interrupt;
		Request.m_Transition.m_Easing = EEasing::LINEAR;
		Request.m_TrackId = TrackId;
		return Request;
	}
}

TEST(UiV2Anim, ReplacePolicyReplacesCurrentTrack)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(1, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(1, EUiAnimProperty::POS_X, 10.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 11)));
	AdvanceFor(Runtime, 0.2f);
	const float MidValue = Runtime.GetValue(1, EUiAnimProperty::POS_X);
	EXPECT_GT(MidValue, 0.0f);
	EXPECT_LT(MidValue, 10.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(1, EUiAnimProperty::POS_X, 20.0f, 0.4f, 2, EUiAnimInterruptPolicy::REPLACE, 12)));
	EXPECT_EQ(Runtime.ActiveTrackCount(), 1);

	AdvanceFor(Runtime, 0.5f);
	EXPECT_NEAR(Runtime.GetValue(1, EUiAnimProperty::POS_X), 20.0f, 0.001f);

	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 12u);
	EXPECT_EQ(Event.m_NodeKey, 1u);
	EXPECT_EQ(Event.m_Property, EUiAnimProperty::POS_X);
	EXPECT_FALSE(Runtime.PollCompletedEvent(Event));
}

TEST(UiV2Anim, QueuePolicyRunsInOrder)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(7, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(7, EUiAnimProperty::ALPHA, 10.0f, 0.2f, 1, EUiAnimInterruptPolicy::QUEUE, 21)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(7, EUiAnimProperty::ALPHA, 20.0f, 0.2f, 1, EUiAnimInterruptPolicy::QUEUE, 22)));
	EXPECT_EQ(Runtime.ActiveTrackCount(), 1);
	EXPECT_EQ(Runtime.QueuedTrackCount(), 1);

	AdvanceFor(Runtime, 0.25f);
	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 21u);
	EXPECT_TRUE(Runtime.HasActiveAnimation(7, EUiAnimProperty::ALPHA));

	AdvanceFor(Runtime, 0.25f);
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 22u);
	EXPECT_FALSE(Runtime.HasActiveAnimation(7, EUiAnimProperty::ALPHA));
	EXPECT_NEAR(Runtime.GetValue(7, EUiAnimProperty::ALPHA), 20.0f, 0.001f);
}

TEST(UiV2Anim, KeepHigherPriorityRejectsLowerPriorityRequest)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(3, EUiAnimProperty::SCALE, 1.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(3, EUiAnimProperty::SCALE, 2.0f, 0.4f, 10, EUiAnimInterruptPolicy::REPLACE, 31)));
	AdvanceFor(Runtime, 0.1f);

	EXPECT_FALSE(Runtime.RequestAnimation(MakeRequest(3, EUiAnimProperty::SCALE, 5.0f, 0.3f, 5, EUiAnimInterruptPolicy::KEEP_HIGHER_PRIORITY, 32)));

	const float AfterRejected = Runtime.GetValue(3, EUiAnimProperty::SCALE);
	EXPECT_LT(AfterRejected, 2.5f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(3, EUiAnimProperty::SCALE, 5.0f, 0.3f, 20, EUiAnimInterruptPolicy::KEEP_HIGHER_PRIORITY, 33)));
	AdvanceFor(Runtime, 0.4f);
	EXPECT_NEAR(Runtime.GetValue(3, EUiAnimProperty::SCALE), 5.0f, 0.001f);

	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 33u);
}

TEST(UiV2Anim, MergeTargetKeepsContinuity)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(5, EUiAnimProperty::WIDTH, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(5, EUiAnimProperty::WIDTH, 10.0f, 1.0f, 1, EUiAnimInterruptPolicy::REPLACE, 41)));
	AdvanceFor(Runtime, 0.25f);
	const float BeforeMerge = Runtime.GetValue(5, EUiAnimProperty::WIDTH);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(5, EUiAnimProperty::WIDTH, 20.0f, 1.0f, 1, EUiAnimInterruptPolicy::MERGE_TARGET, 42)));
	const float AfterMerge = Runtime.GetValue(5, EUiAnimProperty::WIDTH);
	EXPECT_NEAR(BeforeMerge, AfterMerge, 0.0001f);

	AdvanceFor(Runtime, 1.25f);
	EXPECT_NEAR(Runtime.GetValue(5, EUiAnimProperty::WIDTH), 20.0f, 0.001f);

	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 42u);
}

TEST(UiV2Anim, DelayDefersAnimationStart)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(8, EUiAnimProperty::POS_Y, 0.0f);

	SUiAnimRequest Request = MakeRequest(8, EUiAnimProperty::POS_Y, 8.0f, 0.2f, 1, EUiAnimInterruptPolicy::REPLACE, 51);
	Request.m_Transition.m_DelaySec = 0.2f;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));

	AdvanceFor(Runtime, 0.1f);
	EXPECT_NEAR(Runtime.GetValue(8, EUiAnimProperty::POS_Y), 0.0f, 0.0001f);

	AdvanceFor(Runtime, 0.15f);
	EXPECT_GT(Runtime.GetValue(8, EUiAnimProperty::POS_Y), 0.0f);

	AdvanceFor(Runtime, 0.2f);
	EXPECT_NEAR(Runtime.GetValue(8, EUiAnimProperty::POS_Y), 8.0f, 0.001f);
}

TEST(UiV2Anim, ZeroDeltaTimeDoesNotAdvance)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(9, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(9, EUiAnimProperty::POS_X, 9.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 61)));
	const float Before = Runtime.GetValue(9, EUiAnimProperty::POS_X);
	Runtime.Advance(0.0f);
	const float After = Runtime.GetValue(9, EUiAnimProperty::POS_X);
	EXPECT_NEAR(Before, After, 0.0001f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(9, EUiAnimProperty::POS_X));
}

TEST(UiV2Anim, ZeroDurationCompletesImmediately)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(10, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(10, EUiAnimProperty::ALPHA, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::REPLACE, 71)));
	EXPECT_NEAR(Runtime.GetValue(10, EUiAnimProperty::ALPHA), 1.0f, 0.0001f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(10, EUiAnimProperty::ALPHA));

	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 71u);
	EXPECT_EQ(Event.m_NodeKey, 10u);
	EXPECT_EQ(Event.m_Property, EUiAnimProperty::ALPHA);
	EXPECT_FALSE(Runtime.PollCompletedEvent(Event));
}

TEST(UiV2Anim, AwaitTracksCompletesAfterAllTrackedIds)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(11, EUiAnimProperty::POS_X, 0.0f);
	Runtime.SetValue(11, EUiAnimProperty::POS_Y, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(11, EUiAnimProperty::POS_X, 10.0f, 0.1f, 1, EUiAnimInterruptPolicy::REPLACE, 91)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(11, EUiAnimProperty::POS_Y, 20.0f, 0.3f, 1, EUiAnimInterruptPolicy::REPLACE, 92)));
	const uint32_t aTrackIds[] = {91, 92};
	const uint32_t GroupId = Runtime.AwaitTracks(aTrackIds, 2);
	EXPECT_NE(GroupId, 0u);

	AdvanceFor(Runtime, 0.15f);
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 91u);
	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));

	AdvanceFor(Runtime, 0.25f);
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 92u);
	ASSERT_TRUE(Runtime.PollGroupCompletedEvent(GroupEvent));
	EXPECT_EQ(GroupEvent.m_GroupId, GroupId);
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, AwaitTracksIgnoresZeroAndDuplicateTrackIds)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(12, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(12, EUiAnimProperty::ALPHA, 1.0f, 0.1f, 1, EUiAnimInterruptPolicy::REPLACE, 93)));
	const uint32_t aTrackIds[] = {0, 93, 93};
	const uint32_t GroupId = Runtime.AwaitTracks(aTrackIds, 3);
	EXPECT_NE(GroupId, 0u);

	AdvanceFor(Runtime, 0.15f);
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 93u);

	SUiAnimGroupCompleteEvent GroupEvent;
	ASSERT_TRUE(Runtime.PollGroupCompletedEvent(GroupEvent));
	EXPECT_EQ(GroupEvent.m_GroupId, GroupId);
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, ReplacedAwaitedTrackDoesNotCompleteGroup)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(13, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(13, EUiAnimProperty::POS_X, 10.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 94)));
	const uint32_t aTrackIds[] = {94};
	EXPECT_NE(Runtime.AwaitTracks(aTrackIds, 1), 0u);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(13, EUiAnimProperty::POS_X, 20.0f, 0.1f, 1, EUiAnimInterruptPolicy::REPLACE, 95)));
	AdvanceFor(Runtime, 0.2f);

	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 95u);
	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, SetValueCancelsAwaitedActiveAndQueuedTracks)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(18, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(18, EUiAnimProperty::POS_X, 10.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 102)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(18, EUiAnimProperty::POS_X, 20.0f, 0.5f, 1, EUiAnimInterruptPolicy::QUEUE, 103)));
	const uint32_t aCancelledTrackIds[] = {102, 103};
	EXPECT_NE(Runtime.AwaitTracks(aCancelledTrackIds, 2), 0u);

	Runtime.SetValue(18, EUiAnimProperty::POS_X, 5.0f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(18, EUiAnimProperty::POS_X));
	EXPECT_EQ(Runtime.QueuedTrackCount(), 0);

	Runtime.SetValue(19, EUiAnimProperty::POS_X, 0.0f);
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(19, EUiAnimProperty::POS_X, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::REPLACE, 102)));
	Runtime.SetValue(20, EUiAnimProperty::POS_X, 0.0f);
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(20, EUiAnimProperty::POS_X, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::REPLACE, 103)));

	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, ReplacePolicyCancelsAwaitedQueuedTracks)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(21, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(21, EUiAnimProperty::POS_X, 10.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 104)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(21, EUiAnimProperty::POS_X, 20.0f, 0.5f, 1, EUiAnimInterruptPolicy::QUEUE, 105)));
	const uint32_t aQueuedTrackIds[] = {105};
	EXPECT_NE(Runtime.AwaitTracks(aQueuedTrackIds, 1), 0u);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(21, EUiAnimProperty::POS_X, 30.0f, 0.1f, 2, EUiAnimInterruptPolicy::REPLACE, 106)));
	EXPECT_EQ(Runtime.QueuedTrackCount(), 0);
	AdvanceFor(Runtime, 0.2f);

	Runtime.SetValue(22, EUiAnimProperty::POS_X, 0.0f);
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(22, EUiAnimProperty::POS_X, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::REPLACE, 105)));

	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, CancellingOneAwaitedTrackCancelsWholeGroup)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(23, EUiAnimProperty::POS_X, 0.0f);
	Runtime.SetValue(23, EUiAnimProperty::POS_Y, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(23, EUiAnimProperty::POS_X, 10.0f, 0.5f, 1, EUiAnimInterruptPolicy::REPLACE, 107)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(23, EUiAnimProperty::POS_Y, 20.0f, 0.2f, 1, EUiAnimInterruptPolicy::REPLACE, 108)));
	const uint32_t aTrackIds[] = {107, 108};
	EXPECT_NE(Runtime.AwaitTracks(aTrackIds, 2), 0u);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(23, EUiAnimProperty::POS_X, 30.0f, 0.1f, 2, EUiAnimInterruptPolicy::REPLACE, 109)));
	AdvanceFor(Runtime, 0.3f);

	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, AwaitTracksHandlesQueuedImmediateCompletion)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(14, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(14, EUiAnimProperty::ALPHA, 0.5f, 0.1f, 1, EUiAnimInterruptPolicy::QUEUE, 96)));
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(14, EUiAnimProperty::ALPHA, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::QUEUE, 97)));
	const uint32_t aTrackIds[] = {97};
	const uint32_t GroupId = Runtime.AwaitTracks(aTrackIds, 1);
	EXPECT_NE(GroupId, 0u);

	AdvanceFor(Runtime, 0.15f);
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 96u);
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 97u);

	SUiAnimGroupCompleteEvent GroupEvent;
	ASSERT_TRUE(Runtime.PollGroupCompletedEvent(GroupEvent));
	EXPECT_EQ(GroupEvent.m_GroupId, GroupId);
}

TEST(UiV2Anim, AwaitTracksSupportsMultipleGroupsForSameTrack)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(15, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(15, EUiAnimProperty::ALPHA, 1.0f, 0.1f, 1, EUiAnimInterruptPolicy::REPLACE, 98)));
	const uint32_t aTrackIds[] = {98};
	const uint32_t GroupA = Runtime.AwaitTracks(aTrackIds, 1);
	const uint32_t GroupB = Runtime.AwaitTracks(aTrackIds, 1);
	EXPECT_NE(GroupA, 0u);
	EXPECT_NE(GroupB, 0u);
	EXPECT_NE(GroupA, GroupB);

	AdvanceFor(Runtime, 0.15f);
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 98u);

	SUiAnimGroupCompleteEvent GroupEvent;
	ASSERT_TRUE(Runtime.PollGroupCompletedEvent(GroupEvent));
	EXPECT_EQ(GroupEvent.m_GroupId, GroupA);
	ASSERT_TRUE(Runtime.PollGroupCompletedEvent(GroupEvent));
	EXPECT_EQ(GroupEvent.m_GroupId, GroupB);
}

TEST(UiV2Anim, MergeTargetCancelsAwaitForReplacedTrackId)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(16, EUiAnimProperty::WIDTH, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(16, EUiAnimProperty::WIDTH, 10.0f, 0.3f, 1, EUiAnimInterruptPolicy::REPLACE, 99)));
	const uint32_t aTrackIds[] = {99};
	EXPECT_NE(Runtime.AwaitTracks(aTrackIds, 1), 0u);
	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(16, EUiAnimProperty::WIDTH, 20.0f, 0.3f, 1, EUiAnimInterruptPolicy::MERGE_TARGET, 100)));

	AdvanceFor(Runtime, 0.4f);
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 100u);
	SUiAnimGroupCompleteEvent GroupEvent;
	EXPECT_FALSE(Runtime.PollGroupCompletedEvent(GroupEvent));
}

TEST(UiV2Anim, AwaitTracksRejectsAlreadyCompletedOrUnknownTrackIds)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(17, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(17, EUiAnimProperty::ALPHA, 1.0f, 0.0f, 1, EUiAnimInterruptPolicy::REPLACE, 101)));
	SUiAnimCompleteEvent TrackEvent;
	ASSERT_TRUE(Runtime.PollCompletedEvent(TrackEvent));
	EXPECT_EQ(TrackEvent.m_TrackId, 101u);

	const uint32_t aTrackIds[] = {101, 99999};
	EXPECT_EQ(Runtime.AwaitTracks(aTrackIds, 2), 0u);
}

namespace
{
	SUiAnimRequest MakeSpringRequest(uint64_t NodeKey, EUiAnimProperty Property, float Target, uint32_t TrackId)
	{
		g_Config.m_QmUiMotionLevel = 2;
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition.m_Driver = EUiAnimDriver::SPRING;
		Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::REPLACE;
		Request.m_TrackId = TrackId;
		return Request;
	}
}

TEST(UiV2AnimSpring, ConvergesToTarget)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(101, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeSpringRequest(101, EUiAnimProperty::ALPHA, 1.0f, 81)));
	AdvanceFor(Runtime, 2.0f);
	EXPECT_NEAR(Runtime.GetValue(101, EUiAnimProperty::ALPHA), 1.0f, 0.02f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(101, EUiAnimProperty::ALPHA));
}

TEST(UiV2AnimSpring, AutoCompletesAndEmitsEvent)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(102, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeSpringRequest(102, EUiAnimProperty::ALPHA, 1.0f, 82)));
	AdvanceFor(Runtime, 2.0f);

	SUiAnimCompleteEvent Event;
	ASSERT_TRUE(Runtime.PollCompletedEvent(Event));
	EXPECT_EQ(Event.m_TrackId, 82u);
	EXPECT_EQ(Event.m_NodeKey, 102u);
	EXPECT_EQ(Event.m_Property, EUiAnimProperty::ALPHA);
	EXPECT_EQ(Runtime.ActiveTrackCount(), 0);
}

TEST(UiV2AnimSpring, ZeroDtSpringStaysPut)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(103, EUiAnimProperty::ALPHA, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeSpringRequest(103, EUiAnimProperty::ALPHA, 1.0f, 83)));
	const float Before = Runtime.GetValue(103, EUiAnimProperty::ALPHA);
	Runtime.Advance(0.0f);
	const float After = Runtime.GetValue(103, EUiAnimProperty::ALPHA);
	EXPECT_NEAR(Before, After, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(103, EUiAnimProperty::ALPHA));
}

TEST(UiV2AnimSpring, ClampedDtNoExplosion)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(104, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeSpringRequest(104, EUiAnimProperty::POS_X, 100.0f, 84)));

	Runtime.Advance(1.0f);
	const float After = Runtime.GetValue(104, EUiAnimProperty::POS_X);

	EXPECT_GE(After, 0.0f);
	EXPECT_LE(After, 150.0f);
}

TEST(UiV2AnimSpring, MergeTargetPreservesVelocity)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(301, EUiAnimProperty::POS_X, 0.0f);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeSpringRequest(301, EUiAnimProperty::POS_X, 100.0f, 121)));

	AdvanceFor(Runtime, 0.15f);
	const float Before = Runtime.GetValue(301, EUiAnimProperty::POS_X);
	EXPECT_GT(Before, 0.0f);
	EXPECT_LT(Before, 100.0f);

	SUiAnimRequest MergeReq = MakeSpringRequest(301, EUiAnimProperty::POS_X, -100.0f, 122);
	MergeReq.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	EXPECT_TRUE(Runtime.RequestAnimation(MergeReq));

	EXPECT_NEAR(Before, Runtime.GetValue(301, EUiAnimProperty::POS_X), 1e-3f);

	AdvanceFor(Runtime, 3.0f);
	EXPECT_NEAR(Runtime.GetValue(301, EUiAnimProperty::POS_X), -100.0f, 0.5f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(301, EUiAnimProperty::POS_X));
}

TEST(UiV2AnimSpring, ResolveSpringValueUsesRuntimeSpringTrack)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(401, EUiAnimProperty::POS_X, 0.0f);

	SUiSpringConfig Spring;
	Spring.m_Stiffness = 280.0f;
	Spring.m_Damping = 18.0f;
	Spring.m_RestEpsilon = 0.01f;
	Spring.m_RestVelocity = 0.05f;

	EXPECT_NEAR(ResolveUiAnimSpringValue(Runtime, 401, EUiAnimProperty::POS_X, 100.0f, Spring), 0.0f, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(401, EUiAnimProperty::POS_X));

	AdvanceFor(Runtime, 0.15f);
	const float BeforeMerge = Runtime.GetValue(401, EUiAnimProperty::POS_X);
	EXPECT_GT(BeforeMerge, 0.0f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(401, EUiAnimProperty::POS_X));

	EXPECT_NEAR(ResolveUiAnimSpringValue(Runtime, 401, EUiAnimProperty::POS_X, -50.0f, Spring), BeforeMerge, 1e-3f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(401, EUiAnimProperty::POS_X));

	AdvanceFor(Runtime, 3.0f);
	EXPECT_NEAR(Runtime.GetValue(401, EUiAnimProperty::POS_X), -50.0f, 0.5f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(401, EUiAnimProperty::POS_X));
}

TEST(UiV2AnimSpring, ResolveSpringRectXYAnimatesOnlyPosition)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 402;
	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_X, 0.0f);
	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_Y, 0.0f);

	CUIRect Target;
	Target.x = 40.0f;
	Target.y = 80.0f;
	Target.w = 120.0f;
	Target.h = 240.0f;

	const CUIRect Resolved = ResolveUiAnimSpringRectXY(Runtime, NodeKey, Target, ui_spring::SNAPPY);
	EXPECT_NEAR(Resolved.x, 0.0f, 1e-6f);
	EXPECT_NEAR(Resolved.y, 0.0f, 1e-6f);
	EXPECT_NEAR(Resolved.w, Target.w, 1e-6f);
	EXPECT_NEAR(Resolved.h, Target.h, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_X));
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::WIDTH));
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::HEIGHT));
}

TEST(UiV2TreeLayoutTransition, StartsInstantlyAndAnimatesOnChange)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 403;
	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_X, 0.0f);
	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_Y, 0.0f);

	CUIRect Target;
	Target.x = 40.0f;
	Target.y = 80.0f;
	Target.w = 120.0f;
	Target.h = 240.0f;

	const CUIRect FirstResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, Target, ui_spring::SNAPPY);
	EXPECT_NEAR(FirstResolved.x, Target.x, 1e-6f);
	EXPECT_NEAR(FirstResolved.y, Target.y, 1e-6f);
	EXPECT_NEAR(FirstResolved.w, Target.w, 1e-6f);
	EXPECT_NEAR(FirstResolved.h, Target.h, 1e-6f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_X));
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));

	AdvanceFor(Runtime, 0.15f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_X), Target.x, 1e-6f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), Target.y, 1e-6f);

	CUIRect NextTarget = Target;
	NextTarget.x = 90.0f;
	NextTarget.y = 120.0f;

	const CUIRect SecondResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, NextTarget, ui_spring::SNAPPY);
	EXPECT_NEAR(SecondResolved.x, Target.x, 1e-3f);
	EXPECT_NEAR(SecondResolved.y, Target.y, 1e-3f);
	EXPECT_NEAR(SecondResolved.w, NextTarget.w, 1e-6f);
	EXPECT_NEAR(SecondResolved.h, NextTarget.h, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_X));
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));

	Runtime.Advance(1.0f / 60.0f);
	const CUIRect ThirdResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, NextTarget, ui_spring::SNAPPY);
	EXPECT_GT(ThirdResolved.x, Target.x);
	EXPECT_LT(ThirdResolved.x, NextTarget.x);
	EXPECT_GT(ThirdResolved.y, Target.y);
	EXPECT_LT(ThirdResolved.y, NextTarget.y);
	EXPECT_NEAR(ThirdResolved.w, NextTarget.w, 1e-6f);
	EXPECT_NEAR(ThirdResolved.h, NextTarget.h, 1e-6f);

	AdvanceFor(Runtime, 3.0f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_X), NextTarget.x, 0.5f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), NextTarget.y, 0.5f);
}

TEST(UiV2AnimSpring, CardReorderFlipKeepsReleaseMotionVisible)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 404;

	CUIRect Target;
	Target.x = 100.0f;
	Target.y = 200.0f;
	Target.w = 180.0f;
	Target.h = 90.0f;

	const CUIRect FirstResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, Target, ui_token::motion::CARD_REORDER);
	EXPECT_NEAR(FirstResolved.x, Target.x, 1e-6f);
	EXPECT_NEAR(FirstResolved.y, Target.y, 1e-6f);

	CUIRect NextTarget = Target;
	NextTarget.x = 220.0f;
	NextTarget.y = 320.0f;

	const CUIRect StartResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, NextTarget, ui_token::motion::CARD_REORDER);
	EXPECT_NEAR(StartResolved.x, Target.x, 1e-3f);
	EXPECT_NEAR(StartResolved.y, Target.y, 1e-3f);

	AdvanceFor(Runtime, 0.15f);
	const CUIRect MidResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, NextTarget, ui_token::motion::CARD_REORDER);
	EXPECT_NEAR(MidResolved.w, NextTarget.w, 1e-6f);
	EXPECT_NEAR(MidResolved.h, NextTarget.h, 1e-6f);
	EXPECT_LT(std::abs(MidResolved.x - NextTarget.x), 4.0f);
	EXPECT_LT(std::abs(MidResolved.y - NextTarget.y), 4.0f);
	EXPECT_NE(MidResolved.x, StartResolved.x);
	EXPECT_NE(MidResolved.y, StartResolved.y);

	AdvanceFor(Runtime, 8.0f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_X), NextTarget.x, 1.0f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), NextTarget.y, 1.0f);
}

TEST(UiV2TreeLayoutTransition, FirstTargetSyncsImmediately)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;

	CUIRect Target;
	Target.x = 24.0f;
	Target.y = 48.0f;
	Target.w = 140.0f;
	Target.h = 72.0f;

	const CUIRect FirstResolved = Tree.ResolveLayoutTransition(Runtime, 901, Target, ui_spring::SNAPPY);
	EXPECT_NEAR(FirstResolved.x, Target.x, 1e-6f);
	EXPECT_NEAR(FirstResolved.y, Target.y, 1e-6f);
	EXPECT_NEAR(FirstResolved.w, Target.w, 1e-6f);
	EXPECT_NEAR(FirstResolved.h, Target.h, 1e-6f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(901, EUiAnimProperty::POS_X));
	EXPECT_FALSE(Runtime.HasActiveAnimation(901, EUiAnimProperty::POS_Y));
}

TEST(UiV2TreeLayoutTransition, ReusesStableNodeAcrossFrames)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;

	CUIRect Target;
	Target.x = 18.0f;
	Target.y = 36.0f;
	Target.w = 110.0f;
	Target.h = 60.0f;

	const CUIRect FirstResolved = Tree.ResolveLayoutTransition(Runtime, 902, Target, ui_spring::SNAPPY);
	EXPECT_NEAR(FirstResolved.x, Target.x, 1e-6f);
	EXPECT_NEAR(FirstResolved.y, Target.y, 1e-6f);

	CUIRect NextTarget = Target;
	NextTarget.x = 58.0f;
	NextTarget.y = 96.0f;

	const CUIRect SecondResolved = Tree.ResolveLayoutTransition(Runtime, 902, NextTarget, ui_spring::SNAPPY);
	EXPECT_NEAR(SecondResolved.x, Target.x, 1e-3f);
	EXPECT_NEAR(SecondResolved.y, Target.y, 1e-3f);
	EXPECT_NEAR(SecondResolved.w, NextTarget.w, 1e-6f);
	EXPECT_NEAR(SecondResolved.h, NextTarget.h, 1e-6f);

	AdvanceFor(Runtime, 0.2f);
	const CUIRect MidResolved = Tree.ResolveLayoutTransition(Runtime, 902, NextTarget, ui_spring::SNAPPY);
	EXPECT_GT(MidResolved.x, Target.x);
	EXPECT_LT(MidResolved.x, NextTarget.x);
	EXPECT_GT(MidResolved.y, Target.y);
	EXPECT_LT(MidResolved.y, NextTarget.y);
	EXPECT_NEAR(MidResolved.w, NextTarget.w, 1e-6f);
	EXPECT_NEAR(MidResolved.h, NextTarget.h, 1e-6f);
}

TEST(UiV2TreeLayoutTransition, CanSyncTargetWithoutAnimating)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 903;

	CUIRect Target;
	Target.x = 10.0f;
	Target.y = 20.0f;
	Target.w = 120.0f;
	Target.h = 60.0f;

	EXPECT_EQ(Tree.ResolveLayoutTransition(Runtime, NodeKey, Target, ui_spring::SNAPPY).x, Target.x);

	CUIRect DragTarget = Target;
	DragTarget.x = 80.0f;
	DragTarget.y = 160.0f;

	const CUIRect DragResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, DragTarget, ui_spring::SNAPPY, 1, false);
	EXPECT_NEAR(DragResolved.x, DragTarget.x, 1e-6f);
	EXPECT_NEAR(DragResolved.y, DragTarget.y, 1e-6f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_X), DragTarget.x, 1e-6f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), DragTarget.y, 1e-6f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_X));
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));
}

TEST(UiV2TreeLayoutTransition, ScrollOffsetDoesNotDriveCardLayoutSpring)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 905;

	auto ToAnimRect = [](CUIRect Rect, float ScrollOffsetY) {
		Rect.y -= ScrollOffsetY;
		return Rect;
	};
	auto ToScreenRect = [](CUIRect Rect, float ScrollOffsetY) {
		Rect.y += ScrollOffsetY;
		return Rect;
	};

	CUIRect Target;
	Target.x = 32.0f;
	Target.y = 96.0f;
	Target.w = 140.0f;
	Target.h = 72.0f;

	const CUIRect FirstScreen = ToScreenRect(Tree.ResolveLayoutTransition(Runtime, NodeKey, ToAnimRect(Target, 0.0f), ui_token::motion::CARD_REORDER), 0.0f);
	EXPECT_NEAR(FirstScreen.y, Target.y, 1e-6f);

	const float ScrollOffsetY = -64.0f;
	CUIRect ScrolledTarget = Target;
	ScrolledTarget.y += ScrollOffsetY;
	const CUIRect ScrolledScreen = ToScreenRect(Tree.ResolveLayoutTransition(Runtime, NodeKey, ToAnimRect(ScrolledTarget, ScrollOffsetY), ui_token::motion::CARD_REORDER), ScrollOffsetY);
	EXPECT_NEAR(ScrolledScreen.y, ScrolledTarget.y, 1e-6f);
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));

	CUIRect ReorderedTarget = Target;
	ReorderedTarget.y += 120.0f;
	CUIRect ReorderedScreenTarget = ReorderedTarget;
	ReorderedScreenTarget.y += ScrollOffsetY;
	const CUIRect ReorderedStart = ToScreenRect(Tree.ResolveLayoutTransition(Runtime, NodeKey, ToAnimRect(ReorderedScreenTarget, ScrollOffsetY), ui_token::motion::CARD_REORDER), ScrollOffsetY);
	EXPECT_LT(ReorderedStart.y, ReorderedScreenTarget.y);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));
}

TEST(UiV2TreeLayoutTransition, DragReleaseCanAnimateFromPointerPosition)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 904;

	CUIRect InitialTarget;
	InitialTarget.x = 40.0f;
	InitialTarget.y = 80.0f;
	InitialTarget.w = 180.0f;
	InitialTarget.h = 90.0f;
	Tree.ResolveLayoutTransition(Runtime, NodeKey, InitialTarget, ui_token::motion::CARD_REORDER);

	CUIRect DragTarget = InitialTarget;
	DragTarget.x = 260.0f;
	DragTarget.y = 360.0f;
	Tree.SyncLayoutTransition(Runtime, NodeKey, DragTarget);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_X), DragTarget.x, 1e-6f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), DragTarget.y, 1e-6f);

	CUIRect ReleaseTarget = InitialTarget;
	ReleaseTarget.x = 80.0f;
	ReleaseTarget.y = 120.0f;
	const CUIRect ReleaseResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, ReleaseTarget, ui_token::motion::CARD_REORDER);
	EXPECT_NEAR(ReleaseResolved.x, DragTarget.x, 1e-3f);
	EXPECT_NEAR(ReleaseResolved.y, DragTarget.y, 1e-3f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_X));
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));

	Runtime.Advance(1.0f / 60.0f);
	const CUIRect MidResolved = Tree.ResolveLayoutTransition(Runtime, NodeKey, ReleaseTarget, ui_token::motion::CARD_REORDER);
	EXPECT_LT(MidResolved.x, DragTarget.x);
	EXPECT_GT(MidResolved.x, ReleaseTarget.x);
	EXPECT_LT(MidResolved.y, DragTarget.y);
	EXPECT_GT(MidResolved.y, ReleaseTarget.y);
	EXPECT_NEAR(MidResolved.w, ReleaseTarget.w, 1e-6f);
	EXPECT_NEAR(MidResolved.h, ReleaseTarget.h, 1e-6f);
}

TEST(UiV2TreePresence, EnterStartsFromTransparentAndAnimatesToVisible)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 1001;

	Tree.BeginFrame();
	const float FirstAlpha = Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	EXPECT_NEAR(FirstAlpha, 0.0f, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::ALPHA));
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 1);

	AdvanceFor(Runtime, 0.12f);
	Tree.BeginFrame();
	const float MidAlpha = Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	EXPECT_GT(MidAlpha, 0.0f);
	EXPECT_LT(MidAlpha, 1.0f);
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	const float FinalAlpha = Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	EXPECT_NEAR(FinalAlpha, 1.0f, 0.001f);
	Tree.EndFrame(Runtime);
}

TEST(UiV2TreePresence, MissingNodeExitsBeforeRemoval)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 1002;

	Tree.BeginFrame();
	Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);

	Tree.BeginFrame();
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 1);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::ALPHA));

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 0);
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::ALPHA));
}

TEST(UiV2TreePresence, RetouchingExitingNodeCancelsRemoval)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 1003;

	Tree.BeginFrame();
	Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);

	Tree.BeginFrame();
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 0.08f);
	const float ExitingAlpha = Runtime.GetValue(NodeKey, EUiAnimProperty::ALPHA, 1.0f);
	EXPECT_LT(ExitingAlpha, 1.0f);

	Tree.BeginFrame();
	const float RetouchedAlpha = Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	EXPECT_NEAR(RetouchedAlpha, ExitingAlpha, 0.05f);
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 1);

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	const float FinalAlpha = Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	EXPECT_NEAR(FinalAlpha, 1.0f, 0.001f);
	Tree.EndFrame(Runtime);
}

TEST(UiV2TreePresence, ExitCleansUpWhenHigherPriorityAlphaWasActive)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 1004;

	Tree.BeginFrame();
	Tree.ResolvePresenceAlpha(Runtime, NodeKey, ui_token::motion::HOVER_FADE);
	Tree.EndFrame(Runtime);

	EXPECT_TRUE(Runtime.RequestAnimation(MakeRequest(NodeKey, EUiAnimProperty::ALPHA, 1.0f, 0.2f, 5, EUiAnimInterruptPolicy::REPLACE, 110)));

	Tree.BeginFrame();
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 1);

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	Tree.EndFrame(Runtime);
	EXPECT_EQ(Tree.NodeCount(), 0);
	EXPECT_FALSE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::ALPHA));
}

TEST(UiV2WidgetPresence, AnimatePresenceRendersWhileExiting)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	IUiContext Ctx;
	Ctx.m_pTree = &Tree;
	Ctx.m_pAnim = &Runtime;
	Ctx.m_ScopeHash = MakeUiScopeHash("presence_widget_test");
	const void *pId = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x7654));

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult First = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::HOVER_FADE);
	EXPECT_TRUE(First.m_Render);
	EXPECT_NEAR(First.m_Alpha, 0.0f, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(First.m_NodeKey, EUiAnimProperty::ALPHA));
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult ExitStart = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	EXPECT_TRUE(ExitStart.m_Render);
	EXPECT_GT(ExitStart.m_Alpha, 0.99f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(ExitStart.m_NodeKey, EUiAnimProperty::ALPHA));
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 0.08f);
	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult Exiting = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	EXPECT_TRUE(Exiting.m_Render);
	EXPECT_GT(Exiting.m_Alpha, 0.0f);
	EXPECT_LT(Exiting.m_Alpha, 1.0f);
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult Gone = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	EXPECT_FALSE(Gone.m_Render);
	EXPECT_NEAR(Gone.m_Alpha, 0.0f, 1e-6f);
	Tree.EndFrame(Runtime);
}

TEST(UiV2WidgetPresence, FreshEnterIsReportedOnlyAfterFullRemoval)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	IUiContext Ctx;
	Ctx.m_pTree = &Tree;
	Ctx.m_pAnim = &Runtime;
	Ctx.m_ScopeHash = MakeUiScopeHash("presence_fresh_enter_test");
	const void *pId = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x7655));

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult First = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::HOVER_FADE);
	EXPECT_TRUE(First.m_FreshEnter);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult ExitStart = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	EXPECT_FALSE(ExitStart.m_FreshEnter);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 0.08f);

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult ReenterWhileExiting = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::HOVER_FADE);
	EXPECT_FALSE(ReenterWhileExiting.m_FreshEnter);
	Tree.EndFrame(Runtime);

	Tree.BeginFrame();
	ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);
	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult Gone = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::HOVER_FADE);
	EXPECT_FALSE(Gone.m_Render);
	Tree.EndFrame(Runtime);

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult Reopened = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::HOVER_FADE);
	EXPECT_TRUE(Reopened.m_FreshEnter);
	Tree.EndFrame(Runtime);
}

TEST(UiV2WidgetPresence, ModalScaleCanResetOnFreshEnterAfterExit)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	IUiContext Ctx;
	Ctx.m_pTree = &Tree;
	Ctx.m_pAnim = &Runtime;
	Ctx.m_ScopeHash = MakeUiScopeHash("presence_modal_scale_test");
	const void *pId = reinterpret_cast<const void *>(static_cast<uintptr_t>(0x7656));

	Tree.BeginFrame();
	ui_widget::SAnimatePresenceResult Presence = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::MODAL_IN);
	ASSERT_TRUE(Presence.m_FreshEnter);
	Runtime.SetValue(Presence.m_NodeKey, EUiAnimProperty::SCALE, 0.92f);
	ResolveUiAnimValue(Runtime, Presence.m_NodeKey, EUiAnimProperty::SCALE, 1.0f, ui_token::motion::MODAL_IN.m_DurationSec, ui_token::motion::MODAL_IN.m_Easing);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);

	Tree.BeginFrame();
	Presence = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::MODAL_IN);
	EXPECT_FALSE(Presence.m_FreshEnter);
	ResolveUiAnimValue(Runtime, Presence.m_NodeKey, EUiAnimProperty::SCALE, 0.96f, ui_token::motion::MODAL_IN.m_DurationSec, ui_token::motion::MODAL_IN.m_Easing);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 1.0f);

	Tree.BeginFrame();
	const ui_widget::SAnimatePresenceResult Gone = ui_widget::AnimatePresence(Ctx, pId, false, ui_token::motion::MODAL_IN);
	EXPECT_FALSE(Gone.m_Render);
	Tree.EndFrame(Runtime);

	Tree.BeginFrame();
	Presence = ui_widget::AnimatePresence(Ctx, pId, true, ui_token::motion::MODAL_IN);
	ASSERT_TRUE(Presence.m_FreshEnter);
	Runtime.SetValue(Presence.m_NodeKey, EUiAnimProperty::SCALE, 0.92f);
	const float ReopenedScale = ResolveUiAnimValue(Runtime, Presence.m_NodeKey, EUiAnimProperty::SCALE, 1.0f, ui_token::motion::MODAL_IN.m_DurationSec, ui_token::motion::MODAL_IN.m_Easing);
	EXPECT_NEAR(ReopenedScale, 0.92f, 1e-6f);
	Tree.EndFrame(Runtime);
}

TEST(UiV2WidgetStateAnimation, MissingRuntimeReturnsTarget)
{
	IUiContext Ctx;
	Ctx.m_ScopeHash = MakeUiScopeHash("widget_state_test");
	int Id = 0;
	SUiAnimTransition Transition = ui_curve::DECELERATE;

	EXPECT_FLOAT_EQ(ui_widget::AnimateStateValue(Ctx, &Id, EUiAnimProperty::ALPHA, 0.75f, Transition), 0.75f);
}

TEST(UiV2WidgetStateAnimation, PreservesFullTransitionSemantics)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2AnimationRuntime Runtime;
	IUiContext Ctx;
	Ctx.m_pAnim = &Runtime;
	Ctx.m_ScopeHash = MakeUiScopeHash("widget_state_test");
	int Id = 0;
	const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(&Id));
	Runtime.SetValue(NodeKey, EUiAnimProperty::ALPHA, 0.0f);

	SUiAnimTransition Transition;
	Transition.m_Driver = EUiAnimDriver::TWEEN;
	Transition.m_DurationSec = 1.0f;
	Transition.m_DelaySec = 0.5f;
	Transition.m_Easing = EEasing::LINEAR;
	Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	Transition.m_Priority = 7;

	EXPECT_FLOAT_EQ(ui_widget::AnimateStateValue(Ctx, &Id, EUiAnimProperty::ALPHA, 1.0f, Transition), 0.0f);
	AdvanceFor(Runtime, 0.25f);
	EXPECT_FLOAT_EQ(Runtime.GetValue(NodeKey, EUiAnimProperty::ALPHA, 0.0f), 0.0f);

	AdvanceFor(Runtime, 0.5f);
	const float MidValue = Runtime.GetValue(NodeKey, EUiAnimProperty::ALPHA, 0.0f);
	EXPECT_GT(MidValue, 0.0f);
	EXPECT_LT(MidValue, 1.0f);
}

TEST(UiV2ImePresence, ExitKeepsRenderingUntilPresenceCompletes)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t PopupKey = BuildUiAnimNodeKey(str_quickhash("qm_ime_popup_test"), 1);
	SUiAnimTransition Transition;
	Transition.m_DurationSec = 0.12f;
	Transition.m_Easing = EEasing::EASE_OUT;

	Tree.BeginFrame();
	SUiPresenceResult Presence = Tree.ResolvePresence(Runtime, PopupKey, true, Transition);
	EXPECT_TRUE(Presence.m_Render);
	EXPECT_TRUE(Presence.m_FreshEnter);
	Runtime.SetValue(PopupKey, EUiAnimProperty::POS_Y, 1.4f);
	const float InitialOffset = ResolveUiAnimValue(Runtime, PopupKey, EUiAnimProperty::POS_Y, 0.0f, 0.12f, EEasing::EASE_OUT);
	EXPECT_NEAR(InitialOffset, 1.4f, 1e-6f);
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 0.2f);
	Tree.BeginFrame();
	Presence = Tree.ResolvePresence(Runtime, PopupKey, false, Transition);
	EXPECT_TRUE(Presence.m_Render);
	EXPECT_GT(Presence.m_Alpha, 0.99f);
	const float ExitOffset = ResolveUiAnimValue(Runtime, PopupKey, EUiAnimProperty::POS_Y, -0.8f, 0.08f, EEasing::EASE_OUT);
	EXPECT_GT(ExitOffset, -0.8f);
	Tree.EndFrame(Runtime);

	AdvanceFor(Runtime, 0.04f);
	Tree.BeginFrame();
	Presence = Tree.ResolvePresence(Runtime, PopupKey, false, Transition);
	EXPECT_TRUE(Presence.m_Render);
	EXPECT_GT(Presence.m_Alpha, 0.0f);
	EXPECT_LT(Presence.m_Alpha, 1.0f);
	Tree.EndFrame(Runtime);
}

TEST(UiV2ImePresence, MotionLevelZeroHiddenPopupStopsRenderingImmediately)
{
	g_Config.m_QmUiMotionLevel = 0;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t PopupKey = BuildUiAnimNodeKey(str_quickhash("qm_ime_popup_test"), 2);
	SUiAnimTransition Transition;
	Transition.m_DurationSec = 0.08f;
	Transition.m_Easing = EEasing::EASE_OUT;

	Tree.BeginFrame();
	SUiPresenceResult Presence = Tree.ResolvePresence(Runtime, PopupKey, true, Transition);
	EXPECT_TRUE(Presence.m_Render);
	Tree.EndFrame(Runtime);

	Tree.BeginFrame();
	Presence = Tree.ResolvePresence(Runtime, PopupKey, false, Transition);
	EXPECT_FALSE(Presence.m_Render);
	EXPECT_NEAR(Presence.m_Alpha, 0.0f, 1e-6f);
	Tree.EndFrame(Runtime);

	g_Config.m_QmUiMotionLevel = 2;
}

TEST(UiV2ImePresence, ReenterWhileExitingKeepsPresenceContinuous)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2Tree Tree;
	CUiV2AnimationRuntime Runtime;
	const uint64_t PopupKey = BuildUiAnimNodeKey(str_quickhash("qm_ime_popup_test"), 3);
	SUiAnimTransition Transition;
	Transition.m_DurationSec = 0.12f;
	Transition.m_Easing = EEasing::EASE_OUT;

	Tree.BeginFrame();
	SUiPresenceResult Presence = Tree.ResolvePresence(Runtime, PopupKey, true, Transition);
	EXPECT_TRUE(Presence.m_FreshEnter);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 0.2f);

	Tree.BeginFrame();
	Presence = Tree.ResolvePresence(Runtime, PopupKey, false, Transition);
	Tree.EndFrame(Runtime);
	AdvanceFor(Runtime, 0.04f);

	Tree.BeginFrame();
	const SUiPresenceResult Reentered = Tree.ResolvePresence(Runtime, PopupKey, true, Transition);
	EXPECT_TRUE(Reentered.m_Render);
	EXPECT_FALSE(Reentered.m_FreshEnter);
	EXPECT_GT(Reentered.m_Alpha, 0.0f);
	EXPECT_LT(Reentered.m_Alpha, 1.0f);
	Tree.EndFrame(Runtime);
}

namespace
{
	SUiAnimRequest MakeTweenRequest(uint64_t NodeKey, EUiAnimProperty Property, float Target, float DurationSec, EEasing Easing, uint32_t TrackId)
	{
		g_Config.m_QmUiMotionLevel = 2;
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition.m_DurationSec = DurationSec;
		Request.m_Transition.m_Easing = Easing;
		Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::REPLACE;
		Request.m_TrackId = TrackId;
		return Request;
	}
}

TEST(UiV2AnimEasing, OutBackOvershoots)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(201, EUiAnimProperty::ALPHA, 0.0f);
	EXPECT_TRUE(Runtime.RequestAnimation(MakeTweenRequest(201, EUiAnimProperty::ALPHA, 1.0f, 1.0f, EEasing::EASE_OUT_BACK, 91)));

	AdvanceFor(Runtime, 0.7f);
	EXPECT_GT(Runtime.GetValue(201, EUiAnimProperty::ALPHA), 1.0f);

	AdvanceFor(Runtime, 0.5f);
	EXPECT_NEAR(Runtime.GetValue(201, EUiAnimProperty::ALPHA), 1.0f, 1e-3f);
}

TEST(UiV2AnimEasing, CubicBezierMatchesReference)
{
	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(202, EUiAnimProperty::ALPHA, 0.0f);
	SUiAnimRequest Request = MakeTweenRequest(202, EUiAnimProperty::ALPHA, 1.0f, 1.0f, EEasing::CUBIC_BEZIER, 92);
	Request.m_Transition.m_Bezier = {0.2f, 0.0f, 0.0f, 1.0f};
	EXPECT_TRUE(Runtime.RequestAnimation(Request));

	AdvanceFor(Runtime, 0.25f);
	const float At25 = Runtime.GetValue(202, EUiAnimProperty::ALPHA);
	EXPECT_GT(At25, 0.55f);
	EXPECT_LT(At25, 0.75f);

	AdvanceFor(Runtime, 0.25f);
	const float At50 = Runtime.GetValue(202, EUiAnimProperty::ALPHA);
	EXPECT_GT(At50, 0.82f);
	EXPECT_LT(At50, 0.95f);

	AdvanceFor(Runtime, 0.25f);
	const float At75 = Runtime.GetValue(202, EUiAnimProperty::ALPHA);
	EXPECT_GT(At75, 0.93f);
	EXPECT_LT(At75, 1.0f);
}

TEST(UiV2AnimEasing, NewEnumsRoundTrip)
{
	const EEasing aEasings[] = {EEasing::EASE_OUT_QUART, EEasing::EASE_OUT_BACK, EEasing::EASE_IN_OUT_CUBIC};
	uint32_t NextTrackId = 100;
	for(EEasing Easing : aEasings)
	{
		CUiV2AnimationRuntime Runtime;
		Runtime.SetValue(1, EUiAnimProperty::ALPHA, 0.0f);
		EXPECT_TRUE(Runtime.RequestAnimation(MakeTweenRequest(1, EUiAnimProperty::ALPHA, 1.0f, 1.0f, Easing, NextTrackId++)));

		EXPECT_NEAR(Runtime.GetValue(1, EUiAnimProperty::ALPHA), 0.0f, 1e-3f);

		AdvanceFor(Runtime, 1.5f);
		EXPECT_NEAR(Runtime.GetValue(1, EUiAnimProperty::ALPHA), 1.0f, 1e-3f);
	}
}

TEST(UiV2AnimEasing, CurvePresetsExposed)
{
	EXPECT_EQ(ui_curve::STANDARD.m_Easing, EEasing::EASE_IN_OUT_CUBIC);
	EXPECT_EQ(ui_curve::EMPHASIZED.m_Easing, EEasing::CUBIC_BEZIER);
	EXPECT_NEAR(ui_curve::EMPHASIZED.m_Bezier.m_X1, 0.2f, 1e-6f);
	EXPECT_NEAR(ui_curve::EMPHASIZED.m_Bezier.m_Y2, 1.0f, 1e-6f);
	EXPECT_EQ(ui_curve::BOUNCE_OUT.m_Easing, EEasing::EASE_OUT_BACK);
	EXPECT_NEAR(ui_spring::SNAPPY.m_Stiffness, 280.0f, 1e-6f);
	EXPECT_NEAR(ui_spring::GENTLE.m_Damping, 14.0f, 1e-6f);
	EXPECT_NEAR(ui_token::motion::CARD_REORDER.m_Stiffness, 900.0f, 1e-6f);
	EXPECT_NEAR(ui_token::motion::CARD_REORDER.m_Damping, 48.0f, 1e-6f);
}

TEST(UiV2AnimEasing, CustomEasingCanBeRegisteredAndReset)
{
	struct SCustomEasingState
	{
		float m_Multiplier = 1.0f;
		int m_Calls = 0;
	};
	SCustomEasingState State{2.0f, 0};
	auto CustomEase = [](float Progress, void *pUser) -> float {
		SCustomEasingState *pState = static_cast<SCustomEasingState *>(pUser);
		++pState->m_Calls;
		return std::min(1.0f, Progress * pState->m_Multiplier);
	};

	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(203, EUiAnimProperty::ALPHA, 0.0f);
	Runtime.RegisterCustomEasing(7, CustomEase, &State);

	SUiAnimRequest Request = MakeTweenRequest(203, EUiAnimProperty::ALPHA, 1.0f, 1.0f, EEasing::CUSTOM, 93);
	Request.m_Transition.m_CustomEasingId = 7;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	AdvanceFor(Runtime, 0.25f);

	EXPECT_GT(State.m_Calls, 0);
	EXPECT_GT(Runtime.GetValue(203, EUiAnimProperty::ALPHA), 0.45f);
	EXPECT_LT(Runtime.GetValue(203, EUiAnimProperty::ALPHA), 0.65f);

	Runtime.Reset();
	Runtime.SetValue(204, EUiAnimProperty::ALPHA, 0.0f);
	Request.m_NodeKey = 204;
	Request.m_TrackId = 94;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	AdvanceFor(Runtime, 0.25f);

	EXPECT_NEAR(Runtime.GetValue(204, EUiAnimProperty::ALPHA), 0.25f, 0.05f);
}

TEST(UiV2AnimEasing, CustomEasingIsSnapshottedForActiveTrack)
{
	struct SCustomEasingState
	{
		float m_Multiplier = 1.0f;
		int m_Calls = 0;
	};
	SCustomEasingState State{2.0f, 0};
	auto CustomEase = [](float Progress, void *pUser) -> float {
		SCustomEasingState *pState = static_cast<SCustomEasingState *>(pUser);
		++pState->m_Calls;
		return std::min(1.0f, Progress * pState->m_Multiplier);
	};

	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(205, EUiAnimProperty::ALPHA, 0.0f);
	Runtime.RegisterCustomEasing(8, CustomEase, &State);

	SUiAnimRequest Request = MakeTweenRequest(205, EUiAnimProperty::ALPHA, 1.0f, 1.0f, EEasing::CUSTOM, 95);
	Request.m_Transition.m_CustomEasingId = 8;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	AdvanceFor(Runtime, 0.1f);
	Runtime.UnregisterCustomEasing(8);
	State.m_Calls = 0;

	AdvanceFor(Runtime, 0.2f);

	EXPECT_GT(State.m_Calls, 0);
	EXPECT_GT(Runtime.GetValue(205, EUiAnimProperty::ALPHA), 0.45f);
}

TEST(UiV2AnimEasing, MergeTargetRefreshesCustomEasing)
{
	struct SCustomEasingState
	{
		float m_Multiplier = 1.0f;
		int m_Calls = 0;
	};
	SCustomEasingState SlowState{0.25f, 0};
	SCustomEasingState FastState{2.0f, 0};
	auto CustomEase = [](float Progress, void *pUser) -> float {
		SCustomEasingState *pState = static_cast<SCustomEasingState *>(pUser);
		++pState->m_Calls;
		return std::min(1.0f, Progress * pState->m_Multiplier);
	};

	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(206, EUiAnimProperty::ALPHA, 0.0f);
	Runtime.RegisterCustomEasing(9, CustomEase, &SlowState);
	Runtime.RegisterCustomEasing(10, CustomEase, &FastState);

	SUiAnimRequest Request = MakeTweenRequest(206, EUiAnimProperty::ALPHA, 1.0f, 1.0f, EEasing::CUSTOM, 96);
	Request.m_Transition.m_CustomEasingId = 9;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	AdvanceFor(Runtime, 0.2f);
	SlowState.m_Calls = 0;
	FastState.m_Calls = 0;

	Request.m_Target = 2.0f;
	Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	Request.m_Transition.m_CustomEasingId = 10;
	Request.m_TrackId = 97;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	SlowState.m_Calls = 0;
	FastState.m_Calls = 0;
	AdvanceFor(Runtime, 0.2f);

	EXPECT_EQ(SlowState.m_Calls, 0);
	EXPECT_GT(FastState.m_Calls, 0);
}

TEST(UiV2AnimEasing, MergeTargetRestartsTweenFromCurrentValue)
{
	struct SCustomEasingState
	{
		float m_Multiplier = 1.0f;
	};
	SCustomEasingState SlowState{0.25f};
	SCustomEasingState FastState{2.0f};
	auto CustomEase = [](float Progress, void *pUser) -> float {
		const SCustomEasingState *pState = static_cast<const SCustomEasingState *>(pUser);
		return std::min(1.0f, Progress * pState->m_Multiplier);
	};

	CUiV2AnimationRuntime Runtime;
	Runtime.SetValue(207, EUiAnimProperty::POS_X, 0.0f);
	Runtime.RegisterCustomEasing(11, CustomEase, &SlowState);
	Runtime.RegisterCustomEasing(12, CustomEase, &FastState);

	SUiAnimRequest Request = MakeTweenRequest(207, EUiAnimProperty::POS_X, 100.0f, 1.0f, EEasing::CUSTOM, 110);
	Request.m_Transition.m_CustomEasingId = 11;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	AdvanceFor(Runtime, 0.2f);
	const float BeforeMerge = Runtime.GetValue(207, EUiAnimProperty::POS_X);

	Request.m_Target = 200.0f;
	Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	Request.m_Transition.m_CustomEasingId = 12;
	Request.m_TrackId = 111;
	EXPECT_TRUE(Runtime.RequestAnimation(Request));
	EXPECT_NEAR(Runtime.GetValue(207, EUiAnimProperty::POS_X), BeforeMerge, 1e-6f);

	Runtime.Advance(1.0f / 60.0f);
	const float AfterOneFrame = Runtime.GetValue(207, EUiAnimProperty::POS_X);
	EXPECT_GE(AfterOneFrame, BeforeMerge);
	EXPECT_LT(AfterOneFrame - BeforeMerge, 10.0f);
}

TEST(UiV2AnimColor, DefaultInterpolationUsesLinearSrgb)
{
	g_Config.m_QmUiColorInterpolation = 0;
	const ColorRGBA From(1.0f, 0.0f, 0.0f, 0.25f);
	const ColorRGBA To(0.0f, 1.0f, 0.0f, 0.75f);

	const ColorRGBA Mid = ResolveUiAnimInterpolatedColor(From, To, 0.5f);

	EXPECT_NEAR(Mid.r, 0.5f, 1e-6f);
	EXPECT_NEAR(Mid.g, 0.5f, 1e-6f);
	EXPECT_NEAR(Mid.b, 0.0f, 1e-6f);
	EXPECT_NEAR(Mid.a, 0.5f, 1e-6f);
}

TEST(UiV2AnimColor, OklabInterpolationKeepsMidColorPerceptuallyBrighter)
{
	g_Config.m_QmUiColorInterpolation = 1;
	const ColorRGBA From(1.0f, 0.0f, 0.0f, 0.25f);
	const ColorRGBA To(0.0f, 1.0f, 0.0f, 0.75f);

	const ColorRGBA Mid = ResolveUiAnimInterpolatedColor(From, To, 0.5f);

	EXPECT_GT(Mid.r, 0.5f);
	EXPECT_GT(Mid.g, 0.5f);
	EXPECT_GT(Mid.b, 0.0f);
	EXPECT_NEAR(Mid.a, 0.5f, 1e-6f);
}

TEST(UiV2AnimColor, ResolveValueColorUsesConfiguredInterpolation)
{
	g_Config.m_QmUiMotionLevel = 2;
	g_Config.m_QmUiColorInterpolation = 1;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 700;
	const ColorRGBA From(1.0f, 0.0f, 0.0f, 0.25f);
	const ColorRGBA To(0.0f, 1.0f, 0.0f, 0.75f);

	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_R, From.r);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_G, From.g);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_B, From.b);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_A, From.a);

	EXPECT_NEAR(ResolveUiAnimValueColor(Runtime, NodeKey, To, 1.0f, EEasing::LINEAR).r, From.r, 1e-6f);
	AdvanceFor(Runtime, 0.5f);
	const ColorRGBA Mid = ResolveUiAnimValueColor(Runtime, NodeKey, To, 1.0f, EEasing::LINEAR);

	EXPECT_GT(Mid.r, 0.5f);
	EXPECT_GT(Mid.g, 0.5f);
	EXPECT_GT(Mid.b, 0.0f);
	EXPECT_GT(Mid.a, 0.45f);
	EXPECT_LT(Mid.a, 0.55f);
}

TEST(UiV2AnimColor, OklabTargetChangeKeepsContinuity)
{
	g_Config.m_QmUiMotionLevel = 2;
	g_Config.m_QmUiColorInterpolation = 1;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 701;
	const ColorRGBA From(1.0f, 0.0f, 0.0f, 1.0f);
	const ColorRGBA Green(0.0f, 1.0f, 0.0f, 1.0f);
	const ColorRGBA Blue(0.0f, 0.0f, 1.0f, 1.0f);

	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_R, From.r);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_G, From.g);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_B, From.b);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_A, From.a);

	ResolveUiAnimValueColor(Runtime, NodeKey, Green, 1.0f, EEasing::LINEAR);
	AdvanceFor(Runtime, 0.25f);
	const ColorRGBA BeforeChange = ResolveUiAnimValueColor(Runtime, NodeKey, Green, 1.0f, EEasing::LINEAR);

	const ColorRGBA AfterChange = ResolveUiAnimValueColor(Runtime, NodeKey, Blue, 1.0f, EEasing::LINEAR);

	EXPECT_NEAR(AfterChange.r, BeforeChange.r, 0.02f);
	EXPECT_NEAR(AfterChange.g, BeforeChange.g, 0.02f);
	EXPECT_NEAR(AfterChange.b, BeforeChange.b, 0.02f);
}

TEST(UiV2AnimColor, ResolveTargetCacheKeepsPropertiesIsolated)
{
	g_Config.m_QmUiMotionLevel = 2;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 702;

	SUiAnimTransition PosTransition;
	PosTransition.m_Driver = EUiAnimDriver::SPRING;
	PosTransition.m_Spring = ui_spring::SNAPPY;
	PosTransition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;

	SUiAnimTransition MixTransition;
	MixTransition.m_DurationSec = 1.0f;
	MixTransition.m_Easing = EEasing::LINEAR;
	MixTransition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	MixTransition.m_Driver = EUiAnimDriver::TWEEN;

	Runtime.SetValue(NodeKey, EUiAnimProperty::POS_Y, 0.0f);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_MIX, 0.0f);

	Runtime.ResolveTargetValue(NodeKey, EUiAnimProperty::POS_Y, 80.0f, PosTransition);
	Runtime.ResolveTargetValue(NodeKey, EUiAnimProperty::COLOR_MIX, 1.0f, MixTransition);
	AdvanceFor(Runtime, 0.1f);
	const float PosBefore = Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y);
	const float MixBefore = Runtime.GetValue(NodeKey, EUiAnimProperty::COLOR_MIX);

	Runtime.ResolveTargetValue(NodeKey, EUiAnimProperty::POS_Y, 80.0f, PosTransition);
	Runtime.ResolveTargetValue(NodeKey, EUiAnimProperty::COLOR_MIX, 1.0f, MixTransition);

	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::POS_Y), PosBefore, 1e-6f);
	EXPECT_NEAR(Runtime.GetValue(NodeKey, EUiAnimProperty::COLOR_MIX), MixBefore, 1e-6f);
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::POS_Y));
	EXPECT_TRUE(Runtime.HasActiveAnimation(NodeKey, EUiAnimProperty::COLOR_MIX));
}

TEST(UiV2AnimColor, OklabPerFrameResolveUsesStableSegmentStart)
{
	g_Config.m_QmUiMotionLevel = 2;
	g_Config.m_QmUiColorInterpolation = 1;
	CUiV2AnimationRuntime Runtime;
	const uint64_t NodeKey = 703;
	const ColorRGBA From(1.0f, 0.0f, 0.0f, 1.0f);
	const ColorRGBA To(0.0f, 1.0f, 0.0f, 1.0f);
	const ColorRGBA ExpectedMid = ResolveUiAnimInterpolatedColor(From, To, 0.5f);

	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_R, From.r);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_G, From.g);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_B, From.b);
	Runtime.SetValue(NodeKey, EUiAnimProperty::COLOR_A, From.a);

	ColorRGBA Mid = From;
	for(int i = 0; i < 30; ++i)
	{
		Mid = ResolveUiAnimValueColor(Runtime, NodeKey, To, 1.0f, EEasing::LINEAR);
		Runtime.Advance(1.0f / 60.0f);
	}
	Mid = ResolveUiAnimValueColor(Runtime, NodeKey, To, 1.0f, EEasing::LINEAR);

	EXPECT_NEAR(Mid.r, ExpectedMid.r, 0.03f);
	EXPECT_NEAR(Mid.g, ExpectedMid.g, 0.03f);
	EXPECT_NEAR(Mid.b, ExpectedMid.b, 0.03f);
	EXPECT_NEAR(Mid.a, ExpectedMid.a, 0.03f);
}

TEST(UiV2ScrollPhysics, WheelImpulseDecaysAndClampsToRange)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;

	SQmScrollConfig Config;
	Config.m_WheelScale = 1.0f;
	Config.m_NativeWheelStep = false;
	Config.m_MaxOverscroll = 72.0f;

	CQmScrollState State;
	State.AddWheelImpulse(-120.0f, Metrics, Config);
	EXPECT_GT(State.Offset(), 0.0f);
	EXPECT_GT(State.Velocity(), 0.0f);

	const float InitialOffset = State.Offset();
	State.Advance(0.2f, Metrics, Config);
	EXPECT_GT(State.Offset(), InitialOffset);
	EXPECT_LT(State.Offset(), Metrics.MaxOffset());
	EXPECT_GT(State.Velocity(), 0.0f);

	for(int i = 0; i < 240; ++i)
		State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_GE(State.Offset(), 0.0f);
	EXPECT_LE(State.Offset(), Metrics.MaxOffset());
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.5f);
}

TEST(UiV2ScrollPhysics, NativeWheelStepMatchesDdnetScrollUnit)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 10.0f;
	Config.m_NativeWheelStep = true;
	Config.m_NativeWheelAnimationTime = 0.5f;

	CQmScrollState State;
	State.AddWheelImpulse(-120.0f, Metrics, Config);

	EXPECT_NEAR(State.Offset(), 0.0f, 0.01f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.01f);

	State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_GT(State.Offset(), 0.0f);
	EXPECT_LT(State.Offset(), 10.0f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.01f);

	for(int i = 0; i < 40; ++i)
		State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 10.0f, 0.01f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.01f);

	State.AddWheelImpulse(-120.0f, Metrics, Config);
	for(int i = 0; i < 40; ++i)
		State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 20.0f, 0.01f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.01f);
}

TEST(UiV2ScrollPhysics, NativeWheelStepConsumesOneDirectionPerFrameLikeDdnetHotkey)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.0f);

	CQmScrollState State;
	State.AddWheelImpulse(-360.0f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 10.0f, 0.01f);

	State.AddWheelImpulse(240.0f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 0.0f, 0.01f);
}

TEST(UiV2ScrollPhysics, NativeWheelAnimationMatchesScrollRegionEaseOut)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 10.0f;
	Config.m_NativeWheelStep = true;
	Config.m_NativeWheelAnimationTime = 0.5f;

	CQmScrollState State;
	State.AddWheelImpulse(-120.0f, Metrics, Config);
	State.Advance(0.125f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 5.78125f, 0.001f);

	State.Advance(0.125f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 8.75f, 0.001f);

	State.Advance(0.25f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 10.0f, 0.001f);

	State.AddWheelImpulse(-120.0f, Metrics, Config);
	State.Advance(0.25f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 18.75f, 0.001f);
}

TEST(UiV2ScrollPhysics, NativeWheelAnimationPausesWhileModifierIsPressed)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 10.0f;
	Config.m_NativeWheelStep = true;
	Config.m_NativeWheelAnimationTime = 0.5f;

	CQmScrollState State;
	State.AddWheelImpulse(-120.0f, Metrics, Config);
	State.Advance(0.125f, Metrics, Config);
	const float OffsetBeforeModifier = State.Offset();

	State.Advance(0.25f, Metrics, Config, true);
	EXPECT_NEAR(State.Offset(), OffsetBeforeModifier, 0.001f);

	State.Advance(0.125f, Metrics, Config);
	State.Advance(0.25f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), 10.0f, 0.001f);
}

TEST(UiV2ScrollPhysics, NativeWheelAnimationConsumesLargeFrameDeltaLikeScrollRegion)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 500.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 10.0f;
	Config.m_NativeWheelStep = true;
	Config.m_NativeWheelAnimationTime = 0.5f;

	CQmScrollState State;
	State.AddWheelImpulse(-120.0f, Metrics, Config);
	State.Advance(0.5f, Metrics, Config);

	EXPECT_NEAR(State.Offset(), 10.0f, 0.001f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.01f);
}

TEST(UiV2ScrollContainer, ModifierSuppressesNativeWheelInputLikeDdnetScrollRegion)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_ModifierPressed = true;
	Input.m_WheelDelta = -120.0f;
	const SQmScrollContainerFrame ModifierFrame = Container.Update(View, 300.0f, 1.0f / 60.0f, Input);
	EXPECT_NEAR(ModifierFrame.m_Offset, 0.0f, 0.001f);

	Input.m_ModifierPressed = false;
	const SQmScrollContainerFrame WheelFrame = Container.Update(View, 300.0f, 0.5f, Input);
	EXPECT_NEAR(WheelFrame.m_Offset, 10.0f, 0.001f);

	Input.m_WheelDelta = 0.0f;
	const SQmScrollContainerFrame DoneFrame = Container.Update(View, 300.0f, 0.5f, Input);
	EXPECT_NEAR(DoneFrame.m_Offset, 10.0f, 0.001f);
}

TEST(UiV2ScrollPhysics, PresetsExposeSharedSmallMediumLargeGeometry)
{
	const SQmScrollContainerStyle Small = QmScrollContainerStyleForSize(EQmScrollSize::SMALL, 1.0f);
	EXPECT_NEAR(Small.m_ScrollbarWidth, 10.0f, 0.01f);
	EXPECT_NEAR(Small.m_ScrollbarMargin, 2.0f, 0.01f);
	EXPECT_NEAR(Small.m_MinThumbHeight, 36.0f, 0.01f);

	const SQmScrollContainerStyle Medium = QmScrollContainerStyleForSize(EQmScrollSize::MEDIUM, 1.0f);
	EXPECT_NEAR(Medium.m_ScrollbarWidth, 20.0f, 0.01f);
	EXPECT_NEAR(Medium.m_ScrollbarMargin, 5.0f, 0.01f);
	EXPECT_NEAR(Medium.m_MinThumbHeight, 42.0f, 0.01f);

	const SQmScrollContainerStyle Large = QmScrollContainerStyleForSize(EQmScrollSize::LARGE, 1.0f);
	EXPECT_NEAR(Large.m_ScrollbarWidth, 28.0f, 0.01f);
	EXPECT_NEAR(Large.m_ScrollbarMargin, 8.0f, 0.01f);
	EXPECT_NEAR(Large.m_MinThumbHeight, 48.0f, 0.01f);

	const SQmScrollConfig NativeWheel = QmNativeWheelScrollConfig(1.0f, 0.5f);
	EXPECT_NEAR(NativeWheel.m_WheelScale, 10.0f, 0.01f);
	EXPECT_TRUE(NativeWheel.m_NativeWheelStep);
	EXPECT_NEAR(NativeWheel.m_NativeWheelAnimationTime, 0.5f, 0.01f);
	EXPECT_NEAR(NativeWheel.m_MaxOverscroll, 0.0f, 0.01f);

	const SQmScrollConfig ScaledNativeWheel = QmNativeWheelScrollConfig(2.0f, 0.5f);
	EXPECT_NEAR(ScaledNativeWheel.m_WheelScale, 10.0f, 0.01f);

	const SQmScrollConfig InstantNativeWheel = QmNativeWheelScrollConfig(1.0f, 0.0f);
	EXPECT_NEAR(InstantNativeWheel.m_NativeWheelAnimationTime, 0.0f, 0.01f);
}

TEST(UiV2ScrollPhysics, ScrollRegionParamsUseSharedQmScrollPreset)
{
	const SQmScrollContainerStyle Medium = QmScrollContainerStyleForSize(EQmScrollSize::MEDIUM, 1.0f);
	const CScrollRegionParams Params = QmScrollRegionParamsForSize(EQmScrollSize::MEDIUM, 1.0f);
	const CScrollRegionParams DefaultParams;

	EXPECT_NEAR(Params.m_ScrollbarThickness, Medium.m_ScrollbarWidth, 0.01f);
	EXPECT_NEAR(Params.m_ScrollbarMargin, Medium.m_ScrollbarMargin, 0.01f);
	EXPECT_NEAR(Params.m_SliderMinSize, Medium.m_MinThumbHeight, 0.01f);
	EXPECT_NEAR(Params.m_ScrollUnit, QmNativeWheelScrollConfig(1.0f, 0.0f).m_WheelScale, 0.01f);
	EXPECT_FALSE(Params.m_ScrollHorizontal);
	EXPECT_NEAR(DefaultParams.m_ScrollbarThickness, Medium.m_ScrollbarWidth, 0.01f);
	EXPECT_NEAR(DefaultParams.m_ScrollbarMargin, Medium.m_ScrollbarMargin, 0.01f);
	EXPECT_NEAR(DefaultParams.m_SliderMinSize, 25.0f, 0.01f);
	EXPECT_NEAR(DefaultParams.m_ScrollUnit, QmNativeWheelScrollConfig(1.0f, 0.0f).m_WheelScale, 0.01f);

	const CScrollRegionParams Horizontal = QmScrollRegionParamsForSize(EQmScrollSize::SMALL, 1.0f, EQmScrollAxis::HORIZONTAL);
	EXPECT_TRUE(Horizontal.m_ScrollHorizontal);
	EXPECT_NEAR(Horizontal.m_ScrollbarThickness, QmScrollContainerStyleForSize(EQmScrollSize::SMALL, 1.0f).m_ScrollbarWidth, 0.01f);
}

TEST(UiV2ScrollPhysics, OverscrollSpringsBackIntoRange)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = 100.0f;
	Metrics.m_ContentSize = 300.0f;
	SQmScrollConfig Config;
	Config.m_NativeWheelStep = false;
	Config.m_MaxOverscroll = 72.0f;

	CQmScrollState State;
	State.SetOffset(Metrics.MaxOffset() + 40.0f, Metrics, Config, true);

	State.Advance(0.1f, Metrics, Config);
	EXPECT_GT(State.Offset(), Metrics.MaxOffset());
	EXPECT_LT(State.Offset(), Metrics.MaxOffset() + 40.0f);

	for(int i = 0; i < 240; ++i)
		State.Advance(1.0f / 60.0f, Metrics, Config);
	EXPECT_NEAR(State.Offset(), Metrics.MaxOffset(), 0.75f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 0.75f);
}

TEST(UiV2ScrollPhysics, NonScrollableContentResetsState)
{
	SQmScrollMetrics ScrollableMetrics;
	ScrollableMetrics.m_ViewportSize = 100.0f;
	ScrollableMetrics.m_ContentSize = 500.0f;
	SQmScrollMetrics NonScrollableMetrics;
	NonScrollableMetrics.m_ViewportSize = 300.0f;
	NonScrollableMetrics.m_ContentSize = 120.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 1.0f;
	Config.m_NativeWheelStep = false;
	Config.m_MaxOverscroll = 72.0f;

	CQmScrollState State;
	State.SetOffset(80.0f, ScrollableMetrics);
	State.AddWheelImpulse(-120.0f, ScrollableMetrics, Config);
	EXPECT_GT(State.Offset(), 0.0f);
	EXPECT_GT(State.Velocity(), 0.0f);

	State.Advance(0.0f, NonScrollableMetrics, Config);
	EXPECT_NEAR(State.Offset(), 0.0f, 1e-6f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 1e-6f);

	State.SetOffset(80.0f, ScrollableMetrics);
	State.AddWheelImpulse(-120.0f, ScrollableMetrics, Config);
	EXPECT_GT(State.Offset(), 0.0f);
	EXPECT_GT(State.Velocity(), 0.0f);

	State.Advance(0.2f, NonScrollableMetrics, Config);
	EXPECT_NEAR(State.Offset(), 0.0f, 1e-6f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 1e-6f);
}

TEST(UiV2ScrollPhysics, ShrinkingScrollableContentClampsOffsetToNewRange)
{
	SQmScrollMetrics TallMetrics;
	TallMetrics.m_ViewportSize = 100.0f;
	TallMetrics.m_ContentSize = 500.0f;
	SQmScrollMetrics ShortMetrics;
	ShortMetrics.m_ViewportSize = 100.0f;
	ShortMetrics.m_ContentSize = 220.0f;

	CQmScrollState State;
	State.SetOffset(400.0f, TallMetrics);
	EXPECT_NEAR(State.Offset(), TallMetrics.MaxOffset(), 1e-6f);

	State.Advance(0.0f, ShortMetrics);
	EXPECT_NEAR(State.Offset(), ShortMetrics.MaxOffset(), 1e-6f);
	EXPECT_NEAR(State.Velocity(), 0.0f, 1e-6f);

	State.SetOffset(400.0f, TallMetrics);
	State.AddWheelImpulse(-120.0f, TallMetrics);
	EXPECT_GT(State.Offset(), ShortMetrics.MaxOffset());

	State.Advance(1.0f / 60.0f, ShortMetrics);
	EXPECT_LE(State.Offset(), ShortMetrics.MaxOffset());
}

TEST(UiV2ScrollContainer, ComputesContentRectAndScrollbarVisibility)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	Container.ScrollByWheel(-120.0f, View.h, 300.0f);
	const SQmScrollContainerFrame Frame = Container.Update(View, 300.0f, 1.0f / 60.0f);

	EXPECT_TRUE(Frame.m_ScrollbarVisible);
	EXPECT_NEAR(Frame.m_ClipRect.x, View.x, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.y, View.y, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.w, View.w, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.h, View.h, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.x, View.x, 1e-6f);
	EXPECT_LT(Frame.m_ContentRect.y, View.y);
	EXPECT_NEAR(Frame.m_ContentRect.w, View.w, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.h, 300.0f, 1e-6f);
	EXPECT_GT(Frame.m_Offset, 0.0f);
}

TEST(UiV2ScrollContainer, DefaultWheelInputUsesDdnetNativeStep)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	Container.ScrollByWheel(-120.0f, View.h, 300.0f);
	const SQmScrollContainerFrame FirstFrame = Container.Update(View, 300.0f, 0.0f);
	EXPECT_NEAR(FirstFrame.m_Offset, 10.0f, 0.001f);
	EXPECT_NEAR(Container.Velocity(), 0.0f, 0.001f);
}

TEST(UiV2ScrollContainer, ExplicitDdnetSmoothTimeUsesEaseOutStep)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;
	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.5f);

	Container.ScrollByWheel(-120.0f, View.h, 300.0f, Config);
	const SQmScrollContainerFrame FirstFrame = Container.Update(View, 300.0f, 0.0f, Config);
	EXPECT_NEAR(FirstFrame.m_Offset, 0.0f, 0.001f);

	const SQmScrollContainerFrame HalfFrame = Container.Update(View, 300.0f, 0.25f, Config);
	EXPECT_NEAR(HalfFrame.m_Offset, 8.75f, 0.001f);

	const SQmScrollContainerFrame DoneFrame = Container.Update(View, 300.0f, 0.25f, Config);
	EXPECT_NEAR(DoneFrame.m_Offset, 10.0f, 0.001f);

	Container.ScrollByWheel(-360.0f, View.h, 300.0f, Config);
	Container.Update(View, 300.0f, 0.5f, Config);
	Container.Update(View, 300.0f, 0.125f, Config);
	Container.Update(View, 300.0f, 0.25f, Config);
	Container.Update(View, 300.0f, 0.25f, Config);
	EXPECT_NEAR(Container.Offset(), 20.0f, 0.001f);
	EXPECT_NEAR(Container.Velocity(), 0.0f, 0.001f);
}

TEST(UiV2ScrollContainer, NonScrollableContentKeepsContentAtViewOrigin)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 4.0f;
	View.y = 8.0f;
	View.w = 180.0f;
	View.h = 120.0f;

	const SQmScrollConfig Config = QmNativeWheelScrollConfig(1.0f, 0.0f);
	Container.ScrollByWheel(-120.0f, View.h, 400.0f, Config);
	EXPECT_GT(Container.Offset(), 0.0f);

	const SQmScrollContainerFrame Frame = Container.Update(View, 80.0f, 0.0f, Config);
	EXPECT_FALSE(Frame.m_ScrollbarVisible);
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.x, View.x, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.y, View.y, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.w, View.w, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.h, 80.0f, 1e-6f);
	EXPECT_NEAR(Container.Offset(), 0.0f, 1e-6f);
}

TEST(UiV2ScrollContainer, WheelInputOnlyMovesWhenHovered)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = false;
	Input.m_WheelDelta = -120.0f;
	SQmScrollContainerFrame Frame = Container.Update(View, 300.0f, 1.0f / 60.0f, Input);
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_Hovered = true;
	Frame = Container.Update(View, 300.0f, 1.0f / 60.0f, Input);
	EXPECT_GT(Frame.m_Offset, 0.0f);
	EXPECT_LT(Frame.m_ContentRect.y, View.y);
}

TEST(UiV2ScrollContainer, ComputesScrollbarTrackAndThumbGeometry)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_WheelDelta = -120.0f;
	const SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 1.0f / 60.0f, Input, Style);

	EXPECT_TRUE(Frame.m_ScrollbarVisible);
	EXPECT_NEAR(Frame.m_ClipRect.w, View.w - Style.m_ScrollbarWidth, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.x, View.x + View.w - Style.m_ScrollbarWidth + Style.m_ScrollbarMargin, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.y, View.y + Style.m_ScrollbarMargin, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.w, Style.m_ScrollbarWidth - Style.m_ScrollbarMargin * 2.0f, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.h, View.h - Style.m_ScrollbarMargin * 2.0f, 1e-6f);
	EXPECT_GE(Frame.m_ScrollbarThumbRect.h, Style.m_MinThumbHeight);
	EXPECT_GE(Frame.m_ScrollbarThumbRect.y, Frame.m_ScrollbarTrackRect.y);
	EXPECT_LE(Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h, Frame.m_ScrollbarTrackRect.y + Frame.m_ScrollbarTrackRect.h);
}

TEST(UiV2ScrollContainer, ComputesHorizontalContentRectAndScrollbarGeometry)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_Axis = EQmScrollAxis::HORIZONTAL;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_WheelDelta = -120.0f;
	const SQmScrollContainerFrame Frame = Container.Update(View, 500.0f, 1.0f / 60.0f, Input, Style);

	EXPECT_TRUE(Frame.m_ScrollbarVisible);
	EXPECT_NEAR(Frame.m_ClipRect.x, View.x, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.y, View.y, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.w, View.w, 1e-6f);
	EXPECT_NEAR(Frame.m_ClipRect.h, View.h - Style.m_ScrollbarWidth, 1e-6f);
	EXPECT_LT(Frame.m_ContentRect.x, View.x);
	EXPECT_NEAR(Frame.m_ContentRect.y, View.y, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.w, 500.0f, 1e-6f);
	EXPECT_NEAR(Frame.m_ContentRect.h, View.h - Style.m_ScrollbarWidth, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.x, View.x + Style.m_ScrollbarMargin, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.y, View.y + View.h - Style.m_ScrollbarWidth + Style.m_ScrollbarMargin, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.w, View.w - Style.m_ScrollbarMargin * 2.0f, 1e-6f);
	EXPECT_NEAR(Frame.m_ScrollbarTrackRect.h, Style.m_ScrollbarWidth - Style.m_ScrollbarMargin * 2.0f, 1e-6f);
	EXPECT_GE(Frame.m_ScrollbarThumbRect.w, Style.m_MinThumbHeight);
	EXPECT_GE(Frame.m_ScrollbarThumbRect.x, Frame.m_ScrollbarTrackRect.x);
	EXPECT_LE(Frame.m_ScrollbarThumbRect.x + Frame.m_ScrollbarThumbRect.w, Frame.m_ScrollbarTrackRect.x + Frame.m_ScrollbarTrackRect.w);
}

TEST(UiV2ScrollContainer, OverscrollKeepsScrollbarThumbInsideTrack)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_WheelDelta = 1000.0f;
	SQmScrollConfig Config;
	Config.m_WheelScale = 1.0f;
	Config.m_NativeWheelStep = false;
	Config.m_MaxOverscroll = 72.0f;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 1.0f / 60.0f, Input, Style, Config);
	EXPECT_LT(Frame.m_Offset, 0.0f);
	EXPECT_GE(Frame.m_ScrollbarThumbRect.y, Frame.m_ScrollbarTrackRect.y);

	Container.Reset();
	Input.m_WheelDelta = -100000.0f;
	Frame = Container.Update(View, 400.0f, 1.0f / 60.0f, Input, Style, Config);
	EXPECT_GT(Frame.m_Offset, 300.0f);
	EXPECT_LE(Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h, Frame.m_ScrollbarTrackRect.y + Frame.m_ScrollbarTrackRect.h);
}

TEST(UiV2ScrollContainer, DraggingScrollbarThumbMapsMouseToOffset)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Frame.m_ScrollbarVisible);

	Input.m_MouseY = Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h * 0.5f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ThumbHovered = true;
	Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	EXPECT_TRUE(Container.ScrollbarDragActive());

	Input.m_MousePressed = false;
	Input.m_MouseY = Frame.m_ScrollbarTrackRect.y + Frame.m_ScrollbarTrackRect.h - Frame.m_ScrollbarThumbRect.h * 0.5f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	EXPECT_NEAR(Frame.m_Offset, 300.0f, 1.0f);

	Input.m_MouseDown = false;
	Container.Update(View, 400.0f, 0.0f, Input, Style);
	EXPECT_FALSE(Container.ScrollbarDragActive());
}

TEST(UiV2ScrollContainer, DraggingHorizontalScrollbarThumbMapsMouseToOffset)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_Axis = EQmScrollAxis::HORIZONTAL;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Frame.m_ScrollbarVisible);

	Input.m_MouseX = Frame.m_ScrollbarThumbRect.x + Frame.m_ScrollbarThumbRect.w * 0.5f;
	Input.m_MouseY = Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h * 0.5f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ThumbHovered = true;
	Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_TRUE(Container.ScrollbarDragActive());

	Input.m_MousePressed = false;
	Input.m_MouseX = Frame.m_ScrollbarTrackRect.x + Frame.m_ScrollbarTrackRect.w - Frame.m_ScrollbarThumbRect.w * 0.5f;
	Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_NEAR(Frame.m_Offset, 300.0f, 1.0f);

	Input.m_MouseDown = false;
	Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_FALSE(Container.ScrollbarDragActive());
}

TEST(UiV2ScrollContainer, ClickingScrollbarTrackPagesTowardMouse)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Frame.m_ScrollbarVisible);

	Input.m_MouseY = Frame.m_ScrollbarTrackRect.y + Frame.m_ScrollbarTrackRect.h - 2.0f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_TrackHovered = true;
	Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);

	EXPECT_NEAR(Frame.m_Offset, 100.0f, 1e-6f);
	EXPECT_TRUE(Container.ScrollbarDragActive());
}

TEST(UiV2ScrollContainer, PreviewFrameDoesNotCancelActiveScrollbarDrag)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Frame.m_ScrollbarVisible);

	Input.m_MouseY = Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h * 0.5f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ThumbHovered = true;
	Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Container.ScrollbarDragActive());

	const SQmScrollContainerFrame Preview = Container.PreviewFrame(View, 400.0f, Style);
	EXPECT_TRUE(Preview.m_ScrollbarVisible);
	EXPECT_TRUE(Container.ScrollbarDragActive());

	Input.m_MousePressed = false;
	Input.m_ThumbHovered = false;
	Input.m_MouseY = Frame.m_ScrollbarTrackRect.y + Frame.m_ScrollbarTrackRect.h - Frame.m_ScrollbarThumbRect.h * 0.5f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	EXPECT_NEAR(Frame.m_Offset, 300.0f, 1.0f);
}

TEST(UiV2ScrollContainer, DraggingContentMovesOffsetWithPointer)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_MouseY = 70.0f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ContentDragAllowed = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MousePressed = false;
	Input.m_MouseY = 67.0f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MouseY = 40.0f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_NEAR(Frame.m_Offset, 30.0f, 1e-6f);
	EXPECT_TRUE(Container.ContentDragActive());

	Input.m_MouseDown = false;
	Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
}

TEST(UiV2ScrollContainer, DraggingHorizontalContentUsesPointerX)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_Axis = EQmScrollAxis::HORIZONTAL;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_MouseX = 80.0f;
	Input.m_MouseY = 70.0f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ContentDragAllowed = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MousePressed = false;
	Input.m_MouseX = 76.0f;
	Input.m_MouseY = 30.0f;
	Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MouseX = 40.0f;
	Frame = Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_NEAR(Frame.m_Offset, 40.0f, 1e-6f);
	EXPECT_TRUE(Container.ContentDragActive());
	EXPECT_LT(Frame.m_ContentRect.x, View.x);
	EXPECT_NEAR(Frame.m_ContentRect.y, View.y, 1e-6f);

	Input.m_MouseDown = false;
	Container.Update(View, 500.0f, 0.0f, Input, Style);
	EXPECT_FALSE(Container.ContentDragActive());
}

TEST(UiV2ScrollContainer, ScrollbarDragDoesNotStartContentDrag)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerStyle Style;
	Style.m_ScrollbarWidth = 8.0f;
	Style.m_ScrollbarMargin = 2.0f;
	Style.m_MinThumbHeight = 24.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_ContentDragAllowed = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input, Style);
	ASSERT_TRUE(Frame.m_ScrollbarVisible);

	Input.m_MouseY = Frame.m_ScrollbarThumbRect.y + Frame.m_ScrollbarThumbRect.h * 0.5f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ThumbHovered = true;
	Container.Update(View, 400.0f, 0.0f, Input, Style);

	EXPECT_TRUE(Container.ScrollbarDragActive());
	EXPECT_FALSE(Container.ContentDragActive());
}

TEST(UiV2ScrollContainer, BlockedContentDragDoesNotMoveOffset)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_MouseY = 70.0f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ContentDragAllowed = true;
	Input.m_ContentDragBlocked = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input);

	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MousePressed = false;
	Input.m_MouseY = 40.0f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);
}

TEST(UiV2ScrollContainer, BlockedContentDragCancelsPendingCandidate)
{
	CQmScrollContainer Container;
	CUIRect View;
	View.x = 10.0f;
	View.y = 20.0f;
	View.w = 200.0f;
	View.h = 100.0f;

	SQmScrollContainerInput Input;
	Input.m_Hovered = true;
	Input.m_MouseY = 70.0f;
	Input.m_MousePressed = true;
	Input.m_MouseDown = true;
	Input.m_ContentDragAllowed = true;
	SQmScrollContainerFrame Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_MousePressed = false;
	Input.m_ContentDragBlocked = true;
	Input.m_MouseY = 40.0f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);

	Input.m_ContentDragBlocked = false;
	Input.m_MouseY = 20.0f;
	Frame = Container.Update(View, 400.0f, 0.0f, Input);
	EXPECT_FALSE(Container.ContentDragActive());
	EXPECT_NEAR(Frame.m_Offset, 0.0f, 1e-6f);
}

TEST(UiV2InputField, BuildsSharedResultForCommitAndClearStates)
{
	const ui_widget::SInputFieldResult ClickAwayCommit = ui_widget::BuildInputFieldResult(true, false, false, false, false, false, false);
	EXPECT_FALSE(ClickAwayCommit.m_Changed);
	EXPECT_TRUE(ClickAwayCommit.m_Deactivated);
	EXPECT_TRUE(ClickAwayCommit.m_Committed);
	EXPECT_FALSE(ClickAwayCommit.m_Submitted);
	EXPECT_FALSE(ClickAwayCommit.m_Cleared);

	const ui_widget::SInputFieldResult EnterSubmit = ui_widget::BuildInputFieldResult(true, false, false, true, false, false, false);
	EXPECT_TRUE(EnterSubmit.m_Deactivated);
	EXPECT_TRUE(EnterSubmit.m_Committed);
	EXPECT_TRUE(EnterSubmit.m_Submitted);

	const ui_widget::SInputFieldResult ClearButton = ui_widget::BuildInputFieldResult(true, true, true, false, false, true, true);
	EXPECT_TRUE(ClearButton.m_Changed);
	EXPECT_FALSE(ClearButton.m_Deactivated);
	EXPECT_FALSE(ClearButton.m_Committed);
	EXPECT_TRUE(ClearButton.m_Cleared);
}

TEST(UiV2InputField, DeclaresEditingCapabilitiesPerPublicFieldType)
{
	const auto HasCapability = [](unsigned Capabilities, ui_widget::EInputFieldCapability Capability) {
		return (Capabilities & static_cast<unsigned>(Capability)) != 0u;
	};

	const unsigned TextCaps = ui_widget::InputFieldCapabilities();
	EXPECT_TRUE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::DOUBLE_CLICK_SELECT_ALL));
	EXPECT_TRUE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::CLICK_AWAY_COMMIT));
	EXPECT_TRUE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::CURSOR_INSERTION));
	EXPECT_TRUE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::MOUSE_DRAG_SELECTION));
	EXPECT_FALSE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::CLEAR_BUTTON));
	EXPECT_FALSE(HasCapability(TextCaps, ui_widget::EInputFieldCapability::SEARCH_HOTKEY));

	const unsigned ClearableCaps = ui_widget::ClearableInputFieldCapabilities();
	EXPECT_TRUE(HasCapability(ClearableCaps, ui_widget::EInputFieldCapability::CLEAR_BUTTON));
	EXPECT_FALSE(HasCapability(ClearableCaps, ui_widget::EInputFieldCapability::SEARCH_HOTKEY));
	EXPECT_EQ((ClearableCaps & TextCaps), TextCaps);

	const unsigned SearchCaps = ui_widget::SearchFieldCapabilities();
	EXPECT_TRUE(HasCapability(SearchCaps, ui_widget::EInputFieldCapability::CLEAR_BUTTON));
	EXPECT_TRUE(HasCapability(SearchCaps, ui_widget::EInputFieldCapability::SEARCH_HOTKEY));
	EXPECT_EQ((SearchCaps & ClearableCaps), ClearableCaps);
}

TEST(UiV2DropdownGeometry, PositionsPopupRelativeToScrolledAnchor)
{
	CUIRect Viewport;
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.w = 320.0f;
	Viewport.h = 240.0f;
	CUIRect Anchor;
	Anchor.x = 48.0f;
	Anchor.y = -18.0f;
	Anchor.w = 120.0f;
	Anchor.h = 24.0f;
	SQmDropdownGeometryConfig Config;
	Config.m_Width = Anchor.w;
	Config.m_Height = 80.0f;
	Config.m_Gap = 4.0f;
	Config.m_Margin = 8.0f;

	const SQmDropdownGeometryResult Result = QmComputeDropdownPopupGeometry(Anchor, Viewport, Config);

	EXPECT_TRUE(Result.m_AnchorVisible);
	EXPECT_TRUE(Result.m_PlacedBelow);
	EXPECT_NEAR(Result.m_Rect.x, Anchor.x, 0.001f);
	EXPECT_NEAR(Result.m_Rect.y, Anchor.y + Anchor.h + Config.m_Gap, 0.001f);
	EXPECT_NEAR(Result.m_Rect.w, Anchor.w, 0.001f);
	EXPECT_NEAR(Result.m_Rect.h, Config.m_Height, 0.001f);
}

TEST(UiV2DropdownGeometry, FlipsAboveWhenBelowWouldOverflow)
{
	CUIRect Viewport;
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.w = 320.0f;
	Viewport.h = 240.0f;
	CUIRect Anchor;
	Anchor.x = 48.0f;
	Anchor.y = 210.0f;
	Anchor.w = 120.0f;
	Anchor.h = 24.0f;
	SQmDropdownGeometryConfig Config;
	Config.m_Width = Anchor.w;
	Config.m_Height = 96.0f;
	Config.m_Gap = 4.0f;
	Config.m_Margin = 8.0f;

	const SQmDropdownGeometryResult Result = QmComputeDropdownPopupGeometry(Anchor, Viewport, Config);

	EXPECT_TRUE(Result.m_AnchorVisible);
	EXPECT_FALSE(Result.m_PlacedBelow);
	EXPECT_NEAR(Result.m_Rect.y, Anchor.y - Config.m_Gap - Config.m_Height, 0.001f);
}

TEST(UiV2DropdownGeometry, ClampsOversizedPopupInsideViewportMargins)
{
	CUIRect Viewport;
	Viewport.x = 10.0f;
	Viewport.y = 20.0f;
	Viewport.w = 120.0f;
	Viewport.h = 90.0f;
	CUIRect Anchor;
	Anchor.x = 100.0f;
	Anchor.y = 130.0f;
	Anchor.w = 80.0f;
	Anchor.h = 20.0f;
	SQmDropdownGeometryConfig Config;
	Config.m_Width = 200.0f;
	Config.m_Height = 120.0f;
	Config.m_Gap = 4.0f;
	Config.m_Margin = 6.0f;

	const SQmDropdownGeometryResult Result = QmComputeDropdownPopupGeometry(Anchor, Viewport, Config);

	EXPECT_FALSE(Result.m_AnchorVisible);
	EXPECT_TRUE(Result.m_Clamped);
	EXPECT_NEAR(Result.m_Rect.x, Viewport.x + Config.m_Margin, 0.001f);
	EXPECT_NEAR(Result.m_Rect.y, Viewport.y + Config.m_Margin, 0.001f);
	EXPECT_NEAR(Result.m_Rect.w, Viewport.w - Config.m_Margin * 2.0f, 0.001f);
	EXPECT_NEAR(Result.m_Rect.h, Viewport.h - Config.m_Margin * 2.0f, 0.001f);
}

TEST(UiV2DropdownGeometry, KeepsPopupVisibleWhenAnchorScrolledOut)
{
	CUIRect Viewport;
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.w = 320.0f;
	Viewport.h = 240.0f;
	CUIRect Anchor;
	Anchor.x = 48.0f;
	Anchor.y = -160.0f;
	Anchor.w = 120.0f;
	Anchor.h = 24.0f;
	SQmDropdownGeometryConfig Config;
	Config.m_Width = Anchor.w;
	Config.m_Height = 80.0f;
	Config.m_Gap = 4.0f;
	Config.m_Margin = 8.0f;

	const SQmDropdownGeometryResult Result = QmComputeDropdownPopupGeometry(Anchor, Viewport, Config);

	EXPECT_FALSE(Result.m_AnchorVisible);
	EXPECT_TRUE(Result.m_PopupVisible);
	EXPECT_TRUE(Result.m_Clamped);
	EXPECT_GE(Result.m_Rect.y, Viewport.y + Config.m_Margin);
	EXPECT_LE(Result.m_Rect.y + Result.m_Rect.h, Viewport.y + Viewport.h - Config.m_Margin);
}

TEST(UiV2DropdownGeometry, MarksPopupInvisibleWhenViewportHasNoUsableArea)
{
	CUIRect Viewport;
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.w = 12.0f;
	Viewport.h = 12.0f;
	CUIRect Anchor;
	Anchor.x = 4.0f;
	Anchor.y = 4.0f;
	Anchor.w = 16.0f;
	Anchor.h = 16.0f;
	SQmDropdownGeometryConfig Config;
	Config.m_Width = 120.0f;
	Config.m_Height = 80.0f;
	Config.m_Margin = 8.0f;

	const SQmDropdownGeometryResult Result = QmComputeDropdownPopupGeometry(Anchor, Viewport, Config);

	EXPECT_FALSE(Result.m_PopupVisible);
	EXPECT_NEAR(Result.m_Rect.w, 0.0f, 0.001f);
	EXPECT_NEAR(Result.m_Rect.h, 0.0f, 0.001f);
}

TEST(UiV2DropdownState, OpensWithFirstItemAndClosesOnEscape)
{
	CQmDropdownState State;
	SQmDropdownInput Input;
	Input.m_TogglePressed = true;

	SQmDropdownUpdateResult Result = State.Update(Input, 3);
	EXPECT_TRUE(Result.m_Opened);
	EXPECT_TRUE(State.IsOpen());
	EXPECT_EQ(State.ActiveIndex(), 0);

	Input = {};
	Input.m_KeyEscape = true;
	Result = State.Update(Input, 3);
	EXPECT_TRUE(Result.m_Closed);
	EXPECT_FALSE(State.IsOpen());
	EXPECT_EQ(State.ActiveIndex(), -1);
}

TEST(UiV2DropdownState, KeyboardNavigationWrapsAndEnterSelects)
{
	CQmDropdownState State;
	SQmDropdownInput Input;
	Input.m_TogglePressed = true;
	State.Update(Input, 3);

	Input = {};
	Input.m_KeyUp = true;
	SQmDropdownUpdateResult Result = State.Update(Input, 3);
	EXPECT_FALSE(Result.m_Selected);
	EXPECT_EQ(State.ActiveIndex(), 2);

	Input = {};
	Input.m_KeyDown = true;
	Result = State.Update(Input, 3);
	EXPECT_EQ(State.ActiveIndex(), 0);

	Input = {};
	Input.m_KeyEnter = true;
	Result = State.Update(Input, 3);
	EXPECT_TRUE(Result.m_Selected);
	EXPECT_EQ(Result.m_SelectedIndex, 0);
	EXPECT_TRUE(Result.m_Closed);
	EXPECT_FALSE(State.IsOpen());
}

TEST(UiV2DropdownState, MouseHoverAndClickSelectsHoveredItem)
{
	CQmDropdownState State;
	SQmDropdownInput Input;
	Input.m_TogglePressed = true;
	State.Update(Input, 4);

	Input = {};
	Input.m_HoveredIndex = 2;
	SQmDropdownUpdateResult Result = State.Update(Input, 4);
	EXPECT_FALSE(Result.m_Selected);
	EXPECT_EQ(State.ActiveIndex(), 2);

	Input.m_MouseSelectPressed = true;
	Result = State.Update(Input, 4);
	EXPECT_TRUE(Result.m_Selected);
	EXPECT_EQ(Result.m_SelectedIndex, 2);
	EXPECT_FALSE(State.IsOpen());
}
