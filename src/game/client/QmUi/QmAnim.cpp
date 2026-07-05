/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmAnim.h"

#include "QmMotion.h"

#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

static bool ColorChanged(const ColorRGBA &A, const ColorRGBA &B)
{
	constexpr float COLOR_EPSILON = 0.0001f;
	return std::abs(A.r - B.r) > COLOR_EPSILON ||
	       std::abs(A.g - B.g) > COLOR_EPSILON ||
	       std::abs(A.b - B.b) > COLOR_EPSILON ||
	       std::abs(A.a - B.a) > COLOR_EPSILON;
}

void CUiV2AnimationRuntime::Reset()
{
	m_TimeSec = 0.0f;
	m_CompletedEvents.clear();
	m_GroupCompletedEvents.clear();
	m_NextTrackId = 1;
	m_NextGroupId = 1;
	m_Values.clear();
	m_ActiveTracks.clear();
	m_QueuedTracks.clear();
	m_CustomEasings.clear();
	m_LastTargets.clear();
	m_ResolveUseCounter = 0;
	m_ColorTargets.clear();
	m_ColorUseCounter = 0;
	m_TrackAwaitGroups.clear();
	m_AwaitGroups.clear();
}

static float SolveBezierY(float TargetX, const SUiBezier &Bezier)
{
	auto SampleX = [&](float t) {
		const float OneMinusT = 1.0f - t;
		return 3.0f * OneMinusT * OneMinusT * t * Bezier.m_X1 + 3.0f * OneMinusT * t * t * Bezier.m_X2 + t * t * t;
	};
	auto SampleXPrime = [&](float t) {
		const float OneMinusT = 1.0f - t;
		return 3.0f * OneMinusT * OneMinusT * Bezier.m_X1 + 6.0f * OneMinusT * t * (Bezier.m_X2 - Bezier.m_X1) + 3.0f * t * t * (1.0f - Bezier.m_X2);
	};

	float t = TargetX;
	for(int i = 0; i < 8; ++i)
	{
		const float CurrentX = SampleX(t);
		const float Err = CurrentX - TargetX;
		if(std::abs(Err) < 1e-5f)
			break;
		const float Slope = SampleXPrime(t);
		if(std::abs(Slope) < 1e-6f)
			break;
		t -= Err / Slope;
		t = std::clamp(t, 0.0f, 1.0f);
	}

	const float OneMinusT = 1.0f - t;
	return 3.0f * OneMinusT * OneMinusT * t * Bezier.m_Y1 + 3.0f * OneMinusT * t * t * Bezier.m_Y2 + t * t * t;
}

float CUiV2AnimationRuntime::ApplyEasing(float t, const SUiAnimTransition &Transition) const
{
	const float Clamped = std::clamp(t, 0.0f, 1.0f);
	switch(Transition.m_Easing)
	{
	case EEasing::LINEAR:
		return Clamped;
	case EEasing::EASE_IN:
		return Clamped * Clamped;
	case EEasing::EASE_OUT:
		return Clamped * (2.0f - Clamped);
	case EEasing::EASE_IN_OUT:
		return Clamped < 0.5f ? 2.0f * Clamped * Clamped : -1.0f + (4.0f - 2.0f * Clamped) * Clamped;
	case EEasing::EASE_OUT_QUART:
	{
		const float OneMinusT = 1.0f - Clamped;
		return 1.0f - OneMinusT * OneMinusT * OneMinusT * OneMinusT;
	}
	case EEasing::EASE_OUT_BACK:
	{
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.0f;
		const float Shifted = Clamped - 1.0f;
		return 1.0f + C3 * Shifted * Shifted * Shifted + C1 * Shifted * Shifted;
	}
	case EEasing::EASE_IN_OUT_CUBIC:
		return Clamped < 0.5f ? 4.0f * Clamped * Clamped * Clamped : 1.0f - std::pow(-2.0f * Clamped + 2.0f, 3.0f) / 2.0f;
	case EEasing::CUBIC_BEZIER:
		return SolveBezierY(Clamped, Transition.m_Bezier);
	case EEasing::CUSTOM:
	{
		const auto ItCustom = m_CustomEasings.find(Transition.m_CustomEasingId);
		if(ItCustom != m_CustomEasings.end() && ItCustom->second.m_pfnEasing != nullptr)
			return std::clamp(ItCustom->second.m_pfnEasing(Clamped, ItCustom->second.m_pUser), 0.0f, 1.0f);
		return Clamped;
	}
	}
	return Clamped;
}

float CUiV2AnimationRuntime::ApplyTrackEasing(float t, const SActiveTrack &Track) const
{
	const float Clamped = std::clamp(t, 0.0f, 1.0f);
	if(Track.m_Transition.m_Easing == EEasing::CUSTOM)
	{
		if(Track.m_pfnCustomEasing != nullptr)
			return std::clamp(Track.m_pfnCustomEasing(Clamped, Track.m_pCustomEasingUser), 0.0f, 1.0f);
		return Clamped;
	}
	return ApplyEasing(Clamped, Track.m_Transition);
}

float CUiV2AnimationRuntime::TrackProgress(const SActiveTrack &Track) const
{
	const float Duration = std::max(0.0f, Track.m_Transition.m_DurationSec);
	const float Delay = std::max(0.0f, Track.m_Transition.m_DelaySec);
	const float LocalElapsed = std::max(0.0f, Track.m_ElapsedSec - Delay);
	if(Duration <= 0.0f)
		return LocalElapsed > 0.0f ? 1.0f : 0.0f;
	return std::clamp(LocalElapsed / Duration, 0.0f, 1.0f);
}

bool CUiV2AnimationRuntime::StartTrack(const STrackKey &Key, const SUiAnimRequest &Request, float StartValue)
{
	SActiveTrack Track;
	Track.m_Start = StartValue;
	Track.m_Target = Request.m_Target;
	Track.m_Current = StartValue;
	Track.m_ElapsedSec = 0.0f;
	Track.m_Velocity = 0.0f;
	Track.m_RestTimerSec = 0.0f;
	Track.m_Transition = Request.m_Transition;
	if(Track.m_Transition.m_DurationSec < 0.0f)
		Track.m_Transition.m_DurationSec = 0.0f;
	if(Track.m_Transition.m_DelaySec < 0.0f)
		Track.m_Transition.m_DelaySec = 0.0f;
	Track.m_TrackId = Request.m_TrackId != 0 ? Request.m_TrackId : m_NextTrackId++;
	if(Track.m_Transition.m_Easing == EEasing::CUSTOM)
	{
		const auto ItCustom = m_CustomEasings.find(Track.m_Transition.m_CustomEasingId);
		if(ItCustom != m_CustomEasings.end())
		{
			Track.m_pfnCustomEasing = ItCustom->second.m_pfnEasing;
			Track.m_pCustomEasingUser = ItCustom->second.m_pUser;
		}
	}

	const bool IsSpring = Track.m_Transition.m_Driver == EUiAnimDriver::SPRING;
	if(!IsSpring && Track.m_Transition.m_DurationSec <= 0.0f && Track.m_Transition.m_DelaySec <= 0.0f)
	{
		m_Values[Key] = Track.m_Target;
		m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, Track.m_TrackId});
		CompleteAwaitedTrack(Track.m_TrackId);
		return false;
	}

	if(IsSpring && std::abs(Track.m_Current - Track.m_Target) < Track.m_Transition.m_Spring.m_RestEpsilon)
	{
		m_Values[Key] = Track.m_Target;
		m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, Track.m_TrackId});
		CompleteAwaitedTrack(Track.m_TrackId);
		return false;
	}

	m_ActiveTracks[Key] = Track;
	m_Values[Key] = StartValue;
	return true;
}

void CUiV2AnimationRuntime::StartQueuedTracks(const STrackKey &Key, float StartValue)
{
	float CurrentStartValue = StartValue;
	while(true)
	{
		auto ItQueue = m_QueuedTracks.find(Key);
		if(ItQueue == m_QueuedTracks.end() || ItQueue->second.empty())
			return;

		SUiAnimRequest Next = ItQueue->second.front();
		ItQueue->second.pop_front();
		if(ItQueue->second.empty())
			m_QueuedTracks.erase(ItQueue);

		if(StartTrack(Key, Next, CurrentStartValue))
			return;

		CurrentStartValue = Next.m_Target;
	}
}

void CUiV2AnimationRuntime::CompleteTrack(const STrackKey &Key, const SActiveTrack &Track)
{
	const float Target = Track.m_Target;
	const uint32_t TrackId = Track.m_TrackId;
	m_Values[Key] = Target;
	m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, TrackId});
	CompleteAwaitedTrack(TrackId);
	m_ActiveTracks.erase(Key);
	StartQueuedTracks(Key, Target);
}

void CUiV2AnimationRuntime::CompleteAwaitedTrack(uint32_t TrackId)
{
	auto ItTrackGroup = m_TrackAwaitGroups.find(TrackId);
	if(ItTrackGroup != m_TrackAwaitGroups.end())
	{
		const std::vector<uint32_t> vGroupIds = ItTrackGroup->second;
		m_TrackAwaitGroups.erase(ItTrackGroup);
		for(const uint32_t GroupId : vGroupIds)
		{
			auto ItGroup = m_AwaitGroups.find(GroupId);
			if(ItGroup != m_AwaitGroups.end())
			{
				--ItGroup->second.m_Remaining;
				if(ItGroup->second.m_Remaining <= 0)
				{
					m_GroupCompletedEvents.push_back({GroupId});
					m_AwaitGroups.erase(ItGroup);
				}
			}
		}
	}
}

void CUiV2AnimationRuntime::CancelAwaitedTrack(uint32_t TrackId)
{
	auto ItTrackGroup = m_TrackAwaitGroups.find(TrackId);
	if(ItTrackGroup == m_TrackAwaitGroups.end())
		return;
	const std::vector<uint32_t> vGroupIds = ItTrackGroup->second;
	for(const uint32_t GroupId : vGroupIds)
		CancelAwaitGroup(GroupId);
}

void CUiV2AnimationRuntime::CancelAwaitGroup(uint32_t GroupId)
{
	if(m_AwaitGroups.erase(GroupId) == 0)
		return;

	for(auto ItTrackGroup = m_TrackAwaitGroups.begin(); ItTrackGroup != m_TrackAwaitGroups.end();)
	{
		std::vector<uint32_t> &vGroupIds = ItTrackGroup->second;
		vGroupIds.erase(std::remove(vGroupIds.begin(), vGroupIds.end(), GroupId), vGroupIds.end());
		if(vGroupIds.empty())
			ItTrackGroup = m_TrackAwaitGroups.erase(ItTrackGroup);
		else
			++ItTrackGroup;
	}
}

void CUiV2AnimationRuntime::CancelQueuedTracksForKey(const STrackKey &Key)
{
	const auto ItQueued = m_QueuedTracks.find(Key);
	if(ItQueued == m_QueuedTracks.end())
		return;
	for(const SUiAnimRequest &Request : ItQueued->second)
		CancelAwaitedTrack(Request.m_TrackId);
	m_QueuedTracks.erase(ItQueued);
}

bool CUiV2AnimationRuntime::IsTrackPending(uint32_t TrackId) const
{
	if(TrackId == 0)
		return false;
	for(const auto &Pair : m_ActiveTracks)
	{
		if(Pair.second.m_TrackId == TrackId)
			return true;
	}
	for(const auto &Pair : m_QueuedTracks)
	{
		for(const SUiAnimRequest &Request : Pair.second)
		{
			if(Request.m_TrackId == TrackId)
				return true;
		}
	}
	return false;
}

float CUiV2AnimationRuntime::CurrentValueFor(const STrackKey &Key, float DefaultValue) const
{
	const auto ItActive = m_ActiveTracks.find(Key);
	if(ItActive != m_ActiveTracks.end())
		return ItActive->second.m_Current;

	const auto ItValue = m_Values.find(Key);
	if(ItValue != m_Values.end())
		return ItValue->second;

	return DefaultValue;
}

void CUiV2AnimationRuntime::SetValue(uint64_t NodeKey, EUiAnimProperty Property, float Value)
{
	const STrackKey Key{NodeKey, Property};
	m_Values[Key] = Value;
	const auto ItActive = m_ActiveTracks.find(Key);
	if(ItActive != m_ActiveTracks.end())
	{
		CancelAwaitedTrack(ItActive->second.m_TrackId);
		m_ActiveTracks.erase(ItActive);
	}
	const auto ItQueued = m_QueuedTracks.find(Key);
	if(ItQueued != m_QueuedTracks.end())
	{
		CancelQueuedTracksForKey(Key);
	}
}

float CUiV2AnimationRuntime::GetValue(uint64_t NodeKey, EUiAnimProperty Property, float DefaultValue) const
{
	const STrackKey Key{NodeKey, Property};
	return CurrentValueFor(Key, DefaultValue);
}

bool CUiV2AnimationRuntime::RequestAnimation(const SUiAnimRequest &Request)
{
	SUiAnimRequest EffectiveRequest = Request;
	EffectiveRequest.m_Transition = qm_motion::ApplyMotionLevel(Request.m_Transition, g_Config.m_QmUiMotionLevel);

	const STrackKey Key{EffectiveRequest.m_NodeKey, EffectiveRequest.m_Property};
	const float BaseValue = CurrentValueFor(Key, 0.0f);
	auto ItActive = m_ActiveTracks.find(Key);
	if(ItActive == m_ActiveTracks.end())
	{
		StartTrack(Key, EffectiveRequest, BaseValue);
		return true;
	}

	SActiveTrack &Active = ItActive->second;
	switch(EffectiveRequest.m_Transition.m_Interrupt)
	{
	case EUiAnimInterruptPolicy::REPLACE:
	{
		CancelQueuedTracksForKey(Key);
		CancelAwaitedTrack(Active.m_TrackId);
		StartTrack(Key, EffectiveRequest, Active.m_Current);
		return true;
	}
	case EUiAnimInterruptPolicy::QUEUE:
	{
		m_QueuedTracks[Key].push_back(EffectiveRequest);
		return true;
	}
	case EUiAnimInterruptPolicy::KEEP_HIGHER_PRIORITY:
	{
		if(Active.m_Transition.m_Priority > EffectiveRequest.m_Transition.m_Priority)
			return false;
		CancelQueuedTracksForKey(Key);
		CancelAwaitedTrack(Active.m_TrackId);
		StartTrack(Key, EffectiveRequest, Active.m_Current);
		return true;
	}
	case EUiAnimInterruptPolicy::MERGE_TARGET:
	{
		if(Active.m_Transition.m_Priority > EffectiveRequest.m_Transition.m_Priority)
			return false;
		const bool RequestIsTween = EffectiveRequest.m_Transition.m_Driver == EUiAnimDriver::TWEEN;
		if(RequestIsTween && EffectiveRequest.m_Transition.m_DurationSec <= 0.0f && EffectiveRequest.m_Transition.m_DelaySec <= 0.0f)
		{
			const uint32_t TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
			m_Values[Key] = EffectiveRequest.m_Target;
			m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, TrackId});
			if(TrackId != Active.m_TrackId)
				CancelAwaitedTrack(Active.m_TrackId);
			m_ActiveTracks.erase(Key);
			StartQueuedTracks(Key, EffectiveRequest.m_Target);
			return true;
		}

		if(Active.m_Transition.m_Driver == EUiAnimDriver::SPRING)
		{
			const uint32_t OldTrackId = Active.m_TrackId;
			Active.m_Target = EffectiveRequest.m_Target;
			Active.m_Transition.m_Priority = EffectiveRequest.m_Transition.m_Priority;
			if(!RequestIsTween)
				Active.m_Transition.m_Spring = EffectiveRequest.m_Transition.m_Spring;
			Active.m_RestTimerSec = 0.0f;
			Active.m_TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
			if(Active.m_TrackId != OldTrackId)
				CancelAwaitedTrack(OldTrackId);
			return true;
		}

		const float Current = Active.m_Current;
		Active.m_Start = Current;
		Active.m_Target = EffectiveRequest.m_Target;
		Active.m_ElapsedSec = 0.0f;
		Active.m_Transition = EffectiveRequest.m_Transition;
		if(Active.m_Transition.m_DurationSec < 0.0f)
			Active.m_Transition.m_DurationSec = 0.0f;
		if(Active.m_Transition.m_DelaySec < 0.0f)
			Active.m_Transition.m_DelaySec = 0.0f;
		Active.m_pfnCustomEasing = nullptr;
		Active.m_pCustomEasingUser = nullptr;
		if(Active.m_Transition.m_Easing == EEasing::CUSTOM)
		{
			const auto ItCustom = m_CustomEasings.find(Active.m_Transition.m_CustomEasingId);
			if(ItCustom != m_CustomEasings.end())
			{
				Active.m_pfnCustomEasing = ItCustom->second.m_pfnEasing;
				Active.m_pCustomEasingUser = ItCustom->second.m_pUser;
			}
		}
		const uint32_t OldTrackId = Active.m_TrackId;
		Active.m_TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
		if(Active.m_TrackId != OldTrackId)
			CancelAwaitedTrack(OldTrackId);
		return true;
	}
	}

	return false;
}

void CUiV2AnimationRuntime::AdvanceSpring(SActiveTrack &Track, float Dt) const
{
	const float Delay = std::max(0.0f, Track.m_Transition.m_DelaySec);
	if(Track.m_ElapsedSec < Delay)
		return;

	const SUiSpringConfig &Cfg = Track.m_Transition.m_Spring;
	const float Mass = std::max(Cfg.m_Mass, 1e-4f);
	const float Stiffness = std::max(Cfg.m_Stiffness, 0.0f);
	const float Damping = std::max(Cfg.m_Damping, 0.0f);

	constexpr float KFixedSubStep = 1.0f / 240.0f;
	constexpr int KMaxSubSteps = 8;
	int SubSteps = static_cast<int>(std::ceil(Dt / KFixedSubStep));
	SubSteps = std::clamp(SubSteps, 1, KMaxSubSteps);
	const float SubDt = Dt / static_cast<float>(SubSteps);

	for(int i = 0; i < SubSteps; ++i)
	{
		const float Disp = Track.m_Current - Track.m_Target;
		const float Accel = (-Stiffness * Disp - Damping * Track.m_Velocity) / Mass;
		Track.m_Velocity += Accel * SubDt;
		Track.m_Current += Track.m_Velocity * SubDt;
	}

	const bool AtRest = std::abs(Track.m_Current - Track.m_Target) < Cfg.m_RestEpsilon && std::abs(Track.m_Velocity) < Cfg.m_RestVelocity;
	if(AtRest)
		Track.m_RestTimerSec += Dt;
	else
		Track.m_RestTimerSec = 0.0f;
}

void CUiV2AnimationRuntime::Advance(float Dt)
{
	if(Dt <= 0.0f)
		return;
	const float ClampedDt = std::min(Dt, 1.0f / 15.0f);
	m_TimeSec += ClampedDt;

	constexpr float KSpringRestHoldSec = 0.033f;

	std::deque<STrackKey> vCompleted;
	for(auto &Pair : m_ActiveTracks)
	{
		const STrackKey &Key = Pair.first;
		SActiveTrack &Track = Pair.second;
		Track.m_ElapsedSec += ClampedDt;

		if(Track.m_Transition.m_Driver == EUiAnimDriver::SPRING)
		{
			AdvanceSpring(Track, ClampedDt);
			if(Track.m_RestTimerSec >= KSpringRestHoldSec)
			{
				Track.m_Current = Track.m_Target;
				Track.m_Velocity = 0.0f;
				m_Values[Key] = Track.m_Current;
				vCompleted.push_back(Key);
			}
			else
			{
				m_Values[Key] = Track.m_Current;
			}
		}
		else
		{
			const float RawProgress = TrackProgress(Track);
			const float Progress = ApplyTrackEasing(RawProgress, Track);
			Track.m_Current = Track.m_Start + (Track.m_Target - Track.m_Start) * Progress;
			m_Values[Key] = Track.m_Current;

			if(RawProgress >= 1.0f)
				vCompleted.push_back(Key);
		}
	}

	for(const STrackKey &Key : vCompleted)
	{
		auto ItTrack = m_ActiveTracks.find(Key);
		if(ItTrack != m_ActiveTracks.end())
			CompleteTrack(Key, ItTrack->second);
	}
}

bool CUiV2AnimationRuntime::HasActiveAnimation(uint64_t NodeKey, EUiAnimProperty Property) const
{
	return m_ActiveTracks.contains({NodeKey, Property});
}

void CUiV2AnimationRuntime::PruneResolveTargetCache(uint64_t CurrentUseCounter)
{
	if(m_LastTargets.empty())
		m_LastTargets.reserve(4096);

	if((CurrentUseCounter % 1024) != 0 || m_LastTargets.size() <= 4096)
		return;

	for(auto It = m_LastTargets.begin(); It != m_LastTargets.end();)
	{
		if(CurrentUseCounter - It->second.m_LastUseCounter > 8192)
			It = m_LastTargets.erase(It);
		else
			++It;
	}
	if(m_LastTargets.size() > 4096 * 2)
		m_LastTargets.clear();
}

float CUiV2AnimationRuntime::ResolveTargetValue(uint64_t NodeKey, EUiAnimProperty Property, float Target, const SUiAnimTransition &Transition)
{
	constexpr float ANIM_EPSILON = 0.0001f;
	const STrackKey Key{NodeKey, Property};
	const float CurrentValue = GetValue(NodeKey, Property, Target);
	const uint64_t CurrentUseCounter = ++m_ResolveUseCounter;

	PruneResolveTargetCache(CurrentUseCounter);

	auto [ItLastTarget, Inserted] = m_LastTargets.try_emplace(Key, SResolveTargetState{Target, Transition.m_Driver, CurrentUseCounter});
	const bool HasLastTarget = !Inserted;
	const bool TargetChanged = !HasLastTarget || std::abs(Target - ItLastTarget->second.m_Target) > ANIM_EPSILON;
	const bool DriverChanged = !HasLastTarget || ItLastTarget->second.m_Driver != Transition.m_Driver;
	const bool NeedsSync = !HasActiveAnimation(NodeKey, Property) && std::abs(Target - CurrentValue) > ANIM_EPSILON;
	if(TargetChanged || DriverChanged || NeedsSync)
	{
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition = Transition;
		RequestAnimation(Request);
		ItLastTarget->second.m_Target = Target;
		ItLastTarget->second.m_Driver = Transition.m_Driver;
	}
	ItLastTarget->second.m_LastUseCounter = CurrentUseCounter;

	return GetValue(NodeKey, Property, Target);
}

void CUiV2AnimationRuntime::PruneColorTargetCache(uint64_t CurrentUseCounter)
{
	if(m_ColorTargets.empty())
		m_ColorTargets.reserve(4096);

	if((CurrentUseCounter % 1024) != 0 || m_ColorTargets.size() <= 4096)
		return;

	for(auto It = m_ColorTargets.begin(); It != m_ColorTargets.end();)
	{
		if(CurrentUseCounter - It->second.m_LastUseCounter > 8192)
			It = m_ColorTargets.erase(It);
		else
			++It;
	}
	if(m_ColorTargets.size() > 4096 * 2)
		m_ColorTargets.clear();
}

ColorRGBA CUiV2AnimationRuntime::ResolveColorFromValue(uint64_t NodeKey, const ColorRGBA &Current, const ColorRGBA &Target)
{
	const uint64_t CurrentUseCounter = ++m_ColorUseCounter;
	PruneColorTargetCache(CurrentUseCounter);

	auto [ItTarget, Inserted] = m_ColorTargets.try_emplace(NodeKey, SColorTargetState{Current, Target, CurrentUseCounter});
	if(Inserted || ColorChanged(ItTarget->second.m_Target, Target))
	{
		SetValue(NodeKey, EUiAnimProperty::COLOR_MIX, 0.0f);
		ItTarget->second.m_From = Current;
		ItTarget->second.m_Target = Target;
	}
	ItTarget->second.m_LastUseCounter = CurrentUseCounter;
	return ItTarget->second.m_From;
}

float CUiV2AnimationRuntime::ResolveColorMixValue(uint64_t NodeKey, const ColorRGBA &Target, float DurationSec, EEasing Easing)
{
	SUiAnimTransition Transition;
	Transition.m_DurationSec = DurationSec;
	Transition.m_DelaySec = 0.0f;
	Transition.m_Priority = 1;
	Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
	Transition.m_Easing = Easing;
	Transition.m_Driver = EUiAnimDriver::TWEEN;
	return ResolveTargetValue(NodeKey, EUiAnimProperty::COLOR_MIX, 1.0f, Transition);
}

int CUiV2AnimationRuntime::ActiveTrackCount() const
{
	return static_cast<int>(m_ActiveTracks.size());
}

int CUiV2AnimationRuntime::QueuedTrackCount() const
{
	int Count = 0;
	for(const auto &Pair : m_QueuedTracks)
		Count += static_cast<int>(Pair.second.size());
	return Count;
}

void CUiV2AnimationRuntime::RegisterCustomEasing(uint32_t EasingId, FCustomEasing pfnEasing, void *pUser)
{
	if(EasingId == 0 || pfnEasing == nullptr)
		return;
	m_CustomEasings[EasingId] = SCustomEasing{pfnEasing, pUser};
}

void CUiV2AnimationRuntime::UnregisterCustomEasing(uint32_t EasingId)
{
	m_CustomEasings.erase(EasingId);
}

bool CUiV2AnimationRuntime::PollCompletedEvent(SUiAnimCompleteEvent &EventOut)
{
	if(m_CompletedEvents.empty())
		return false;
	EventOut = m_CompletedEvents.front();
	m_CompletedEvents.pop_front();
	return true;
}

uint32_t CUiV2AnimationRuntime::AwaitTracks(const uint32_t *pTrackIds, int NumTrackIds)
{
	if(pTrackIds == nullptr || NumTrackIds <= 0)
		return 0;

	const uint32_t GroupId = m_NextGroupId++;
	int Registered = 0;
	std::unordered_set<uint32_t> vSeenTrackIds;
	vSeenTrackIds.reserve(static_cast<size_t>(NumTrackIds));
	for(int i = 0; i < NumTrackIds; ++i)
	{
		const uint32_t TrackId = pTrackIds[i];
		if(TrackId == 0)
			continue;
		if(!vSeenTrackIds.insert(TrackId).second)
			continue;
		if(!IsTrackPending(TrackId))
			continue;
		m_TrackAwaitGroups[TrackId].push_back(GroupId);
		++Registered;
	}
	if(Registered <= 0)
		return 0;

	m_AwaitGroups[GroupId] = SAwaitGroup{Registered};
	return GroupId;
}

bool CUiV2AnimationRuntime::PollGroupCompletedEvent(SUiAnimGroupCompleteEvent &EventOut)
{
	if(m_GroupCompletedEvents.empty())
		return false;
	EventOut = m_GroupCompletedEvents.front();
	m_GroupCompletedEvents.pop_front();
	return true;
}

float CUiV2AnimationRuntime::TimeSec() const
{
	return m_TimeSec;
}
