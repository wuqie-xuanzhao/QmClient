#include "test.h"

#include <engine/shared/config.h>

#include <game/client/QmUi/QmAnim.h>
#include <game/client/QmUi/QmAnimCurves.h>

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

	AdvanceFor(Runtime, 1.0f);
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
}
