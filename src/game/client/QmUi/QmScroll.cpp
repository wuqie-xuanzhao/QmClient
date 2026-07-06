/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmScroll.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool ScrollAxisHorizontal(const SQmScrollContainerStyle &Style)
	{
		return Style.m_Axis == EQmScrollAxis::HORIZONTAL;
	}

	float ScrollAxisViewportSize(const CUIRect &ViewRect, const SQmScrollContainerStyle &Style)
	{
		return ScrollAxisHorizontal(Style) ? ViewRect.w : ViewRect.h;
	}

	SQmScrollContainerFrame BuildScrollContainerFrame(const CUIRect &ViewRect, float ContentSize, float Offset, const SQmScrollContainerStyle &Style, bool ReserveScrollbarWidth)
	{
		const bool Horizontal = ScrollAxisHorizontal(Style);
		SQmScrollMetrics Metrics;
		Metrics.m_ViewportSize = ScrollAxisViewportSize(ViewRect, Style);
		Metrics.m_ContentSize = ContentSize;

		const bool ScrollbarVisible = Metrics.MaxOffset() > 0.0f;
		CUIRect ClipRect = ViewRect;
		CUIRect ScrollbarRect;
		if(ScrollbarVisible)
		{
			if(ReserveScrollbarWidth)
			{
				if(Horizontal)
					ClipRect.h = std::max(0.0f, ViewRect.h - Style.m_ScrollbarWidth);
				else
					ClipRect.w = std::max(0.0f, ViewRect.w - Style.m_ScrollbarWidth);
			}
			if(Horizontal)
			{
				ScrollbarRect.x = ViewRect.x;
				ScrollbarRect.y = ViewRect.y + ClipRect.h;
				ScrollbarRect.w = ViewRect.w;
				ScrollbarRect.h = Style.m_ScrollbarWidth;
			}
			else
			{
				ScrollbarRect.x = ViewRect.x + ClipRect.w;
				ScrollbarRect.y = ViewRect.y;
				ScrollbarRect.w = Style.m_ScrollbarWidth;
				ScrollbarRect.h = ViewRect.h;
			}
		}

		SQmScrollContainerFrame Frame;
		Frame.m_ClipRect = ClipRect;
		Frame.m_Offset = Offset;
		Frame.m_ScrollbarVisible = ScrollbarVisible;
		Frame.m_ContentRect = ClipRect;
		if(Horizontal)
		{
			Frame.m_ContentRect.x -= Frame.m_Offset;
			Frame.m_ContentRect.w = ContentSize;
		}
		else
		{
			Frame.m_ContentRect.y -= Frame.m_Offset;
			Frame.m_ContentRect.h = ContentSize;
		}
		if(ScrollbarVisible)
		{
			const float ScrollbarMargin = std::max(0.0f, Style.m_ScrollbarMargin);
			Frame.m_ScrollbarTrackRect.x = ScrollbarRect.x + ScrollbarMargin;
			Frame.m_ScrollbarTrackRect.y = ScrollbarRect.y + ScrollbarMargin;
			Frame.m_ScrollbarTrackRect.w = std::max(0.0f, ScrollbarRect.w - ScrollbarMargin * 2.0f);
			Frame.m_ScrollbarTrackRect.h = std::max(0.0f, ScrollbarRect.h - ScrollbarMargin * 2.0f);
			const float TrackSize = std::max(0.0f, Horizontal ? Frame.m_ScrollbarTrackRect.w : Frame.m_ScrollbarTrackRect.h);
			const float ViewportSize = std::max(0.0f, Horizontal ? ClipRect.w : ClipRect.h);
			const float RawThumbSize = ContentSize > 0.0f ? ViewportSize / ContentSize * TrackSize : TrackSize;
			const float ThumbSize = std::min(TrackSize, std::max(0.0f, std::max(Style.m_MinThumbHeight, RawThumbSize)));
			const float MaxThumbTravel = std::max(0.0f, TrackSize - ThumbSize);
			const float ScrollbarOffset = std::clamp(Frame.m_Offset, 0.0f, Metrics.MaxOffset());
			const float ThumbOffset = Metrics.MaxOffset() > 0.0f ? ScrollbarOffset / Metrics.MaxOffset() * MaxThumbTravel : 0.0f;
			Frame.m_ScrollbarThumbRect = Frame.m_ScrollbarTrackRect;
			if(Horizontal)
			{
				Frame.m_ScrollbarThumbRect.x += ThumbOffset;
				Frame.m_ScrollbarThumbRect.w = ThumbSize;
			}
			else
			{
				Frame.m_ScrollbarThumbRect.y += ThumbOffset;
				Frame.m_ScrollbarThumbRect.h = ThumbSize;
			}
		}
		return Frame;
	}
} // namespace

float SQmScrollMetrics::MaxOffset() const
{
	return std::max(0.0f, m_ContentSize - m_ViewportSize);
}

SQmScrollContainerStyle QmScrollContainerStyleForSize(EQmScrollSize Size, float UiScale)
{
	SQmScrollContainerStyle Style;
	switch(Size)
	{
	case EQmScrollSize::SMALL:
		Style.m_ScrollbarWidth = std::clamp(10.0f * UiScale, 8.0f, 10.0f);
		Style.m_ScrollbarMargin = std::clamp(2.0f * UiScale, 1.0f, 2.0f);
		Style.m_MinThumbHeight = std::clamp(36.0f * UiScale, 24.0f, 36.0f);
		break;
	case EQmScrollSize::MEDIUM:
		Style.m_ScrollbarWidth = std::clamp(20.0f * UiScale, 18.0f, 20.0f);
		Style.m_ScrollbarMargin = std::clamp(5.0f * UiScale, 4.0f, 5.0f);
		Style.m_MinThumbHeight = std::clamp(42.0f * UiScale, 30.0f, 42.0f);
		break;
	case EQmScrollSize::LARGE:
		Style.m_ScrollbarWidth = std::clamp(28.0f * UiScale, 24.0f, 28.0f);
		Style.m_ScrollbarMargin = std::clamp(8.0f * UiScale, 6.0f, 8.0f);
		Style.m_MinThumbHeight = std::clamp(48.0f * UiScale, 36.0f, 48.0f);
		break;
	}
	Style.m_ContentDragThreshold = std::clamp(6.0f * UiScale, 4.0f, 6.0f);
	return Style;
}

SQmScrollConfig QmNativeWheelScrollConfig(float UiScale, float SmoothScrollTimeSec)
{
	SQmScrollConfig Config;
	(void)UiScale;
	Config.m_WheelScale = 10.0f;
	Config.m_NativeWheelStep = true;
	Config.m_NativeWheelAnimationTime = std::max(0.0f, SmoothScrollTimeSec);
	Config.m_MaxOverscroll = 0.0f;
	return Config;
}

void CQmScrollState::Reset()
{
	m_Offset = 0.0f;
	m_Velocity = 0.0f;
	m_LastMaxOffset = 0.0f;
	m_AnimTime = 0.0f;
	m_AnimTimeMax = 0.0f;
	m_AnimStartOffset = 0.0f;
	m_AnimTargetOffset = 0.0f;
}

void CQmScrollState::SetOffset(float Offset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config, bool AllowOverscroll)
{
	const float MaxOffset = Metrics.MaxOffset();
	if(MaxOffset <= 0.0f)
	{
		Reset();
		return;
	}

	const float MinAllowed = AllowOverscroll ? -Config.m_MaxOverscroll : 0.0f;
	const float MaxAllowed = AllowOverscroll ? MaxOffset + Config.m_MaxOverscroll : MaxOffset;
	m_Offset = std::clamp(Offset, MinAllowed, MaxAllowed);
	m_Velocity = 0.0f;
	m_LastMaxOffset = MaxOffset;
	m_AnimTime = 0.0f;
	m_AnimTimeMax = 0.0f;
	m_AnimStartOffset = m_Offset;
	m_AnimTargetOffset = m_Offset;
}

void CQmScrollState::AddWheelImpulse(float WheelDelta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config)
{
	const float MaxOffset = Metrics.MaxOffset();
	if(MaxOffset <= 0.0f)
	{
		Reset();
		return;
	}

	if(Config.m_NativeWheelStep)
	{
		const float WheelSteps = WheelDelta < 0.0f ? 1.0f : -1.0f;
		const float AnimationTime = std::max(0.0f, Config.m_NativeWheelAnimationTime);
		const float BaseOffset = m_AnimTime > 0.0f ? m_AnimTargetOffset : m_Offset;
		const float TargetOffset = std::clamp(BaseOffset + WheelSteps * Config.m_WheelScale, 0.0f, MaxOffset);
		m_Velocity = 0.0f;
		m_AnimStartOffset = m_Offset;
		m_AnimTargetOffset = TargetOffset;
		m_AnimTimeMax = AnimationTime;
		m_AnimTime = AnimationTime;
		if(AnimationTime <= 0.0f || std::abs(m_AnimStartOffset - m_AnimTargetOffset) < 0.5f)
		{
			m_Offset = TargetOffset;
			m_AnimTimeMax = 0.0f;
			m_AnimTime = 0.0f;
		}
		m_LastMaxOffset = MaxOffset;
		return;
	}

	m_Velocity += -WheelDelta * Config.m_WheelScale;
	m_Offset = std::clamp(m_Offset + m_Velocity * (1.0f / 60.0f), -Config.m_MaxOverscroll, MaxOffset + Config.m_MaxOverscroll);
	m_LastMaxOffset = MaxOffset;
}

void CQmScrollState::Advance(float Dt, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config, bool PauseNativeWheelAnimation)
{
	const float MaxOffset = Metrics.MaxOffset();
	if(MaxOffset <= 0.0f)
	{
		Reset();
		return;
	}
	if(m_LastMaxOffset > MaxOffset && m_Offset > MaxOffset)
	{
		m_Offset = MaxOffset;
		if(m_Velocity > 0.0f)
			m_Velocity = 0.0f;
	}
	else if(m_LastMaxOffset > MaxOffset && m_Offset < 0.0f)
	{
		m_Offset = 0.0f;
		if(m_Velocity < 0.0f)
			m_Velocity = 0.0f;
	}
	m_LastMaxOffset = MaxOffset;
	if(Dt <= 0.0f)
		return;

	const float ClampedDt = std::min(Dt, 1.0f / 15.0f);

	if(Config.m_NativeWheelStep && m_AnimTime > 0.0f)
	{
		m_AnimTargetOffset = std::clamp(m_AnimTargetOffset, 0.0f, MaxOffset);
		m_AnimStartOffset = std::clamp(m_AnimStartOffset, 0.0f, MaxOffset);
		if(PauseNativeWheelAnimation)
			return;
		m_AnimTime -= Dt;
		if(m_AnimTime < 0.0f)
			m_AnimTime = 0.0f;
		const float AnimProgress = m_AnimTimeMax > 0.0f ? 1.0f - std::pow(m_AnimTime / m_AnimTimeMax, 3.0f) : 1.0f;
		m_Offset = m_AnimStartOffset + (m_AnimTargetOffset - m_AnimStartOffset) * AnimProgress;
		if(m_AnimTime <= 0.0f)
			m_Offset = m_AnimTargetOffset;
		return;
	}

	const float LowerOverscroll = std::min(0.0f, m_Offset);
	const float UpperOverscroll = std::max(0.0f, m_Offset - MaxOffset);
	const float Overscroll = LowerOverscroll != 0.0f ? LowerOverscroll : UpperOverscroll;

	if(Overscroll != 0.0f)
	{
		const float Accel = -Overscroll * Config.m_OverscrollStiffness - m_Velocity * Config.m_OverscrollDamping;
		m_Velocity += Accel * ClampedDt;
	}
	else
	{
		const float FrictionFactor = std::exp(-std::max(0.0f, Config.m_Friction) * ClampedDt);
		m_Velocity *= FrictionFactor;
	}

	m_Offset += m_Velocity * ClampedDt;
	m_Offset = std::clamp(m_Offset, -Config.m_MaxOverscroll, MaxOffset + Config.m_MaxOverscroll);

	const bool InRange = m_Offset >= 0.0f && m_Offset <= MaxOffset;
	if(InRange && std::abs(m_Velocity) < Config.m_RestVelocity)
		m_Velocity = 0.0f;

	if(m_Offset < 0.0f && std::abs(m_Offset) < Config.m_RestDistance && std::abs(m_Velocity) < Config.m_RestVelocity)
	{
		m_Offset = 0.0f;
		m_Velocity = 0.0f;
	}
	else if(m_Offset > MaxOffset && std::abs(m_Offset - MaxOffset) < Config.m_RestDistance && std::abs(m_Velocity) < Config.m_RestVelocity)
	{
		m_Offset = MaxOffset;
		m_Velocity = 0.0f;
	}
}

void CQmScrollContainer::Reset()
{
	m_State.Reset();
	m_ScrollbarDragActive = false;
	m_ScrollbarGrabY = 0.0f;
	m_ContentDragActive = false;
	m_ContentDragCandidate = false;
	m_ContentDragPressMouseY = 0.0f;
	m_ContentDragPressOffset = 0.0f;
	m_ContentDragLastMouseY = 0.0f;
}

void CQmScrollContainer::ScrollByWheel(float WheelDelta, float ViewportHeight, float ContentHeight, const SQmScrollConfig &Config)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = ViewportHeight;
	Metrics.m_ContentSize = ContentHeight;
	m_State.AddWheelImpulse(WheelDelta, Metrics, Config);
}

SQmScrollContainerFrame CQmScrollContainer::PreviewFrame(const CUIRect &ViewRect, float ContentHeight, const SQmScrollContainerStyle &Style) const
{
	return BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
}

SQmScrollContainerFrame CQmScrollContainer::Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollConfig &Config)
{
	SQmScrollContainerStyle Style;
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = ScrollAxisViewportSize(ViewRect, Style);
	Metrics.m_ContentSize = ContentHeight;
	m_State.Advance(Dt, Metrics, Config);

	return BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, false);
}

SQmScrollContainerFrame CQmScrollContainer::Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style, const SQmScrollConfig &Config)
{
	const bool Horizontal = ScrollAxisHorizontal(Style);
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = ScrollAxisViewportSize(ViewRect, Style);
	Metrics.m_ContentSize = ContentHeight;
	if(Input.m_Hovered && !Input.m_ModifierPressed && Input.m_WheelDelta != 0.0f)
		m_State.AddWheelImpulse(Input.m_WheelDelta, Metrics, Config);
	m_State.Advance(Dt, Metrics, Config, Input.m_ModifierPressed);

	SQmScrollContainerFrame Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
	if(Frame.m_ScrollbarVisible)
	{
		const float TrackSize = std::max(0.0f, Horizontal ? Frame.m_ScrollbarTrackRect.w : Frame.m_ScrollbarTrackRect.h);
		const float ThumbSize = std::max(0.0f, Horizontal ? Frame.m_ScrollbarThumbRect.w : Frame.m_ScrollbarThumbRect.h);
		const float MaxThumbTravel = std::max(0.0f, TrackSize - ThumbSize);
		const CUIRect ClipRect = Frame.m_ClipRect;
		bool JustStartedTrackDrag = false;
		auto SetOffsetFromThumbPosition = [&](float ThumbPosition) {
			const float TrackStart = Horizontal ? Frame.m_ScrollbarTrackRect.x : Frame.m_ScrollbarTrackRect.y;
			const float ThumbStart = std::clamp(ThumbPosition, TrackStart, TrackStart + MaxThumbTravel);
			const float Ratio = MaxThumbTravel > 0.0f ? (ThumbStart - TrackStart) / MaxThumbTravel : 0.0f;
			m_State.SetOffset(Ratio * Metrics.MaxOffset(), Metrics, Config);
		};

		if(!Input.m_MouseDown)
		{
			m_ScrollbarDragActive = false;
			m_ScrollbarGrabY = 0.0f;
		}
		else if(Input.m_MousePressed && Input.m_ThumbHovered)
		{
			m_ScrollbarDragActive = true;
			const float MousePosition = Horizontal ? Input.m_MouseX : Input.m_MouseY;
			const float ThumbPosition = Horizontal ? Frame.m_ScrollbarThumbRect.x : Frame.m_ScrollbarThumbRect.y;
			m_ScrollbarGrabY = std::clamp(MousePosition - ThumbPosition, 0.0f, ThumbSize);
		}
		else if(Input.m_MousePressed && Input.m_TrackHovered)
		{
			const float MousePosition = Horizontal ? Input.m_MouseX : Input.m_MouseY;
			const float ThumbPosition = Horizontal ? Frame.m_ScrollbarThumbRect.x : Frame.m_ScrollbarThumbRect.y;
			const float PageDirection = MousePosition < ThumbPosition ? -1.0f : 1.0f;
			m_State.SetOffset(m_State.Offset() + PageDirection * (Horizontal ? ClipRect.w : ClipRect.h), Metrics, Config);
			Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
			m_ScrollbarDragActive = true;
			m_ScrollbarGrabY = (Horizontal ? Frame.m_ScrollbarThumbRect.w : Frame.m_ScrollbarThumbRect.h) * 0.5f;
			JustStartedTrackDrag = true;
		}

		if(m_ScrollbarDragActive && Input.m_MouseDown && !JustStartedTrackDrag)
		{
			SetOffsetFromThumbPosition((Horizontal ? Input.m_MouseX : Input.m_MouseY) - m_ScrollbarGrabY);
			Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
		}
	}
	else
	{
		m_ScrollbarDragActive = false;
		m_ScrollbarGrabY = 0.0f;
	}
	if(!Input.m_MouseDown)
	{
		m_ContentDragActive = false;
		m_ContentDragCandidate = false;
		m_ContentDragPressMouseY = 0.0f;
		m_ContentDragPressOffset = 0.0f;
		m_ContentDragLastMouseY = 0.0f;
	}
	else if(Input.m_ContentDragBlocked)
	{
		m_ContentDragActive = false;
		m_ContentDragCandidate = false;
		m_ContentDragPressMouseY = 0.0f;
		m_ContentDragPressOffset = 0.0f;
		m_ContentDragLastMouseY = 0.0f;
	}
	else if(Input.m_ContentDragAllowed && !Input.m_ContentDragBlocked && Input.m_Hovered && Input.m_MousePressed && !Input.m_ThumbHovered && !Input.m_TrackHovered)
	{
		m_ContentDragCandidate = true;
		m_ContentDragPressMouseY = Input.m_MouseY;
		m_ContentDragPressOffset = m_State.Offset();
		m_ContentDragLastMouseY = Input.m_MouseY;
	}
	else if((m_ContentDragCandidate || m_ContentDragActive) && Input.m_MouseDown)
	{
		const float DragDistance = Input.m_MouseY - m_ContentDragPressMouseY;
		if(!m_ContentDragActive && std::abs(DragDistance) >= std::max(0.0f, Style.m_ContentDragThreshold))
			m_ContentDragActive = true;
		if(m_ContentDragActive)
		{
			m_State.SetOffset(m_ContentDragPressOffset - DragDistance, Metrics, Config, true);
			Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
		}
		m_ContentDragLastMouseY = Input.m_MouseY;
	}
	return Frame;
}
