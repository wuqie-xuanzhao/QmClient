/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmScroll.h"

#include <algorithm>
#include <cmath>

namespace
{
	SQmScrollContainerFrame BuildScrollContainerFrame(const CUIRect &ViewRect, float ContentHeight, float Offset, const SQmScrollContainerStyle &Style, bool ReserveScrollbarWidth)
	{
		SQmScrollMetrics Metrics;
		Metrics.m_ViewportSize = ViewRect.h;
		Metrics.m_ContentSize = ContentHeight;

		const bool ScrollbarVisible = Metrics.MaxOffset() > 0.0f;
		CUIRect ClipRect = ViewRect;
		CUIRect ScrollbarRect;
		if(ScrollbarVisible)
		{
			if(ReserveScrollbarWidth)
				ClipRect.w = std::max(0.0f, ViewRect.w - Style.m_ScrollbarWidth);
			ScrollbarRect.x = ViewRect.x + ClipRect.w;
			ScrollbarRect.y = ViewRect.y;
			ScrollbarRect.w = Style.m_ScrollbarWidth;
			ScrollbarRect.h = ViewRect.h;
		}

		SQmScrollContainerFrame Frame;
		Frame.m_ClipRect = ClipRect;
		Frame.m_Offset = Offset;
		Frame.m_ScrollbarVisible = ScrollbarVisible;
		Frame.m_ContentRect = ClipRect;
		Frame.m_ContentRect.y -= Frame.m_Offset;
		Frame.m_ContentRect.h = ContentHeight;
		if(ScrollbarVisible)
		{
			const float ScrollbarMargin = std::max(0.0f, Style.m_ScrollbarMargin);
			Frame.m_ScrollbarTrackRect.x = ScrollbarRect.x + ScrollbarMargin;
			Frame.m_ScrollbarTrackRect.y = ScrollbarRect.y + ScrollbarMargin;
			Frame.m_ScrollbarTrackRect.w = std::max(0.0f, ScrollbarRect.w - ScrollbarMargin * 2.0f);
			Frame.m_ScrollbarTrackRect.h = std::max(0.0f, ScrollbarRect.h - ScrollbarMargin * 2.0f);
			const float TrackHeight = std::max(0.0f, Frame.m_ScrollbarTrackRect.h);
			const float RawThumbHeight = ContentHeight > 0.0f ? ClipRect.h / ContentHeight * TrackHeight : TrackHeight;
			const float ThumbHeight = std::min(TrackHeight, std::max(0.0f, std::max(Style.m_MinThumbHeight, RawThumbHeight)));
			const float MaxThumbTravel = std::max(0.0f, TrackHeight - ThumbHeight);
			const float ScrollbarOffset = std::clamp(Frame.m_Offset, 0.0f, Metrics.MaxOffset());
			const float ThumbOffset = Metrics.MaxOffset() > 0.0f ? ScrollbarOffset / Metrics.MaxOffset() * MaxThumbTravel : 0.0f;
			Frame.m_ScrollbarThumbRect = Frame.m_ScrollbarTrackRect;
			Frame.m_ScrollbarThumbRect.y += ThumbOffset;
			Frame.m_ScrollbarThumbRect.h = ThumbHeight;
		}
		return Frame;
	}
} // namespace

float SQmScrollMetrics::MaxOffset() const
{
	return std::max(0.0f, m_ContentSize - m_ViewportSize);
}

void CQmScrollState::Reset()
{
	m_Offset = 0.0f;
	m_Velocity = 0.0f;
	m_LastMaxOffset = 0.0f;
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
}

void CQmScrollState::AddWheelImpulse(float WheelDelta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config)
{
	const float MaxOffset = Metrics.MaxOffset();
	if(MaxOffset <= 0.0f)
	{
		Reset();
		return;
	}

	m_Velocity += -WheelDelta * Config.m_WheelScale;
	m_Offset = std::clamp(m_Offset + m_Velocity * (1.0f / 60.0f), -Config.m_MaxOverscroll, MaxOffset + Config.m_MaxOverscroll);
	m_LastMaxOffset = MaxOffset;
}

void CQmScrollState::Advance(float Dt, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config)
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
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = ViewRect.h;
	Metrics.m_ContentSize = ContentHeight;
	m_State.Advance(Dt, Metrics, Config);

	return BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), SQmScrollContainerStyle(), false);
}

SQmScrollContainerFrame CQmScrollContainer::Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style, const SQmScrollConfig &Config)
{
	SQmScrollMetrics Metrics;
	Metrics.m_ViewportSize = ViewRect.h;
	Metrics.m_ContentSize = ContentHeight;
	if(Input.m_Hovered && Input.m_WheelDelta != 0.0f)
		m_State.AddWheelImpulse(Input.m_WheelDelta, Metrics, Config);
	m_State.Advance(Dt, Metrics, Config);

	SQmScrollContainerFrame Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
	if(Frame.m_ScrollbarVisible)
	{
		const float TrackHeight = std::max(0.0f, Frame.m_ScrollbarTrackRect.h);
		const float MaxThumbTravel = std::max(0.0f, TrackHeight - Frame.m_ScrollbarThumbRect.h);
		const CUIRect ClipRect = Frame.m_ClipRect;
		bool JustStartedTrackDrag = false;
		auto SetOffsetFromThumbY = [&](float ThumbY) {
			const float ThumbTop = std::clamp(ThumbY, Frame.m_ScrollbarTrackRect.y, Frame.m_ScrollbarTrackRect.y + MaxThumbTravel);
			const float Ratio = MaxThumbTravel > 0.0f ? (ThumbTop - Frame.m_ScrollbarTrackRect.y) / MaxThumbTravel : 0.0f;
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
			m_ScrollbarGrabY = std::clamp(Input.m_MouseY - Frame.m_ScrollbarThumbRect.y, 0.0f, Frame.m_ScrollbarThumbRect.h);
		}
		else if(Input.m_MousePressed && Input.m_TrackHovered)
		{
			const float PageDirection = Input.m_MouseY < Frame.m_ScrollbarThumbRect.y ? -1.0f : 1.0f;
			m_State.SetOffset(m_State.Offset() + PageDirection * ClipRect.h, Metrics, Config);
			Frame = BuildScrollContainerFrame(ViewRect, ContentHeight, m_State.Offset(), Style, true);
			m_ScrollbarDragActive = true;
			m_ScrollbarGrabY = Frame.m_ScrollbarThumbRect.h * 0.5f;
			JustStartedTrackDrag = true;
		}

		if(m_ScrollbarDragActive && Input.m_MouseDown && !JustStartedTrackDrag)
		{
			SetOffsetFromThumbY(Input.m_MouseY - m_ScrollbarGrabY);
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
