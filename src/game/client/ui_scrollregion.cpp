/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_scrollregion.h"

#include <base/system.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/keys.h>
#include <engine/shared/config.h>

#include <cmath>

CScrollRegion::CScrollRegion()
{
	Reset();
}

void CScrollRegion::StartScrollAnimation(float TargetScrollPos)
{
	const float ScrollMax = MaxScroll();
	m_AnimInitScrollPos = std::clamp(m_ScrollPos, 0.0f, ScrollMax);
	m_AnimTargetScrollPos = std::clamp(TargetScrollPos, 0.0f, ScrollMax);
	m_ScrollPos = m_AnimInitScrollPos;
	m_AnimTimeMax = g_Config.m_UiSmoothScrollTime / 1000.0f;
	m_AnimTime = m_AnimTimeMax;
	if(m_AnimTimeMax <= 0.0f || absolute(m_AnimInitScrollPos - m_AnimTargetScrollPos) < 0.5f)
	{
		m_ScrollPos = m_AnimTargetScrollPos;
		m_AnimTime = 0.0f;
	}
}

void CScrollRegion::Reset()
{
	m_ScrollPos = 0.0f;
	m_ContentSize = 0.0f;
	m_RequestScrollPos = -1.0f;
	m_ScrollDirection = SCROLLRELATIVE_NONE;
	m_ScrollSpeedMultiplier = 1.0f;

	m_AnimTimeMax = 0.0f;
	m_AnimTime = 0.0f;
	m_AnimInitScrollPos = 0.0f;
	m_AnimTargetScrollPos = 0.0f;

	m_ClipRect = m_RailRect = m_LastAddedRect = CUIRect{0.0f, 0.0f, 0.0f, 0.0f};
	m_SliderGrabPos = 0.0f;
	m_ContentScrollOff = vec2(0.0f, 0.0f);
	m_Params = CScrollRegionParams();
}

void CScrollRegion::Begin(CUIRect *pClipRect, vec2 *pOutOffset, const CScrollRegionParams *pParams)
{
	if(pParams)
		m_Params = *pParams;
	m_ClipRect = *pClipRect;

	// m_ContentSize 来自上一帧 End/AddRect 的测量结果。Begin 先用这个
	// 上一帧尺寸预留滚动条空间，随后本帧再重新测量内容尺寸。
	CUIRect ScrollbarBg = SplitContentArea();
	DrawBackground(ScrollbarBg);

	if(!ContentOverflows())
	{
		if(m_Params.m_ScrollHorizontal)
			m_ContentScrollOff.x = 0.0f;
		else
			m_ContentScrollOff.y = 0.0f;
	}
	m_ContentSize = 0.0f;

	Ui()->ClipEnable(&m_ClipRect);

	*pClipRect = m_ClipRect;
	m_ContentSize = 0.0f;
	*pOutOffset = m_ContentScrollOff;
}

void CScrollRegion::End()
{
	Ui()->ClipDisable();

	if(!ScrollbarShown())
		return;

	DoScrollInput();
	UpdateHotScrollRegion();
	AdvanceAnimation();
	DoSlider();
}

bool CScrollRegion::AddRect(const CUIRect &Rect, bool ShouldScrollHere)
{
	m_LastAddedRect = Rect;
	if(m_Params.m_ScrollHorizontal)
		m_ContentSize = maximum(std::ceil(Rect.x + Rect.w - (m_ClipRect.x + m_ContentScrollOff.x)), m_ContentSize);
	else
		m_ContentSize = maximum(Rect.y + Rect.h - (m_ClipRect.y + m_ContentScrollOff.y), m_ContentSize);
	if(ShouldScrollHere)
		ScrollHere();
	return !RectClipped(Rect);
}

void CScrollRegion::ScrollHere(EScrollOption Option)
{
	const float ClipPos = m_Params.m_ScrollHorizontal ? m_ClipRect.x : m_ClipRect.y;
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	const float LastAddedPos = m_Params.m_ScrollHorizontal ? m_LastAddedRect.x : m_LastAddedRect.y;
	const float LastAddedSize = m_Params.m_ScrollHorizontal ? m_LastAddedRect.w : m_LastAddedRect.h;
	const float ContentScrollOffset = m_Params.m_ScrollHorizontal ? m_ContentScrollOff.x : m_ContentScrollOff.y;
	const float MinHeight = minimum(ClipSize, LastAddedSize);
	const float TopScroll = LastAddedPos - (ClipPos + ContentScrollOffset);

	switch(Option)
	{
	case SCROLLHERE_TOP:
		m_RequestScrollPos = TopScroll;
		break;

	case SCROLLHERE_BOTTOM:
		m_RequestScrollPos = TopScroll - (ClipSize - MinHeight);
		break;

	case SCROLLHERE_KEEP_IN_VIEW:
	default:
		const float DeltaY = LastAddedPos - ClipPos;
		if(DeltaY < 0)
			m_RequestScrollPos = TopScroll;
		else if(DeltaY > (ClipSize - MinHeight))
			m_RequestScrollPos = TopScroll - (ClipSize - MinHeight);
		break;
	}
}

void CScrollRegion::ScrollRelative(EScrollRelative Direction, float SpeedMultiplier)
{
	m_ScrollDirection = Direction;
	m_ScrollSpeedMultiplier = SpeedMultiplier;
}

void CScrollRegion::ScrollRelativeDirect(vec2 ScrollAmount)
{
	const float ScrollAmountAxis = m_Params.m_ScrollHorizontal ? ScrollAmount.x : ScrollAmount.y;
	m_RequestScrollPos = std::clamp(m_ScrollPos + ScrollAmountAxis, 0.0f, MaxScroll());
}

void CScrollRegion::ScrollRelativeDirect(float ScrollAmount)
{
	ScrollRelativeDirect(vec2(0.0f, ScrollAmount));
}

void CScrollRegion::SetScrollOffsetY(float OffsetY)
{
	m_ScrollPos = maximum(0.0f, -OffsetY);
	m_AnimInitScrollPos = m_ScrollPos;
	m_AnimTargetScrollPos = m_ScrollPos;
	m_AnimTime = 0.0f;
	m_RequestScrollPos = -1.0f;
	m_ContentScrollOff.y = -m_ScrollPos;
}

void CScrollRegion::SetContentHeightForNextFrame(float ContentHeight)
{
	m_ContentSize = maximum(0.0f, ContentHeight);
}

void CScrollRegion::DoEdgeScrolling()
{
	if(!ContentOverflows())
		return;

	const float ClipPos = m_Params.m_ScrollHorizontal ? m_ClipRect.x : m_ClipRect.y;
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	const float ScrollBorderSize = 20.0f;
	const float MaxScrollMultiplier = 2.0f;
	const float ScrollSpeedFactor = MaxScrollMultiplier / ScrollBorderSize;
	const float TopScrollPosition = ClipPos + ScrollBorderSize;
	const float BottomScrollPosition = ClipPos + ClipSize - ScrollBorderSize;
	const float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	if(MousePos < TopScrollPosition)
		ScrollRelative(SCROLLRELATIVE_UP, minimum(MaxScrollMultiplier, (TopScrollPosition - MousePos) * ScrollSpeedFactor));
	else if(MousePos > BottomScrollPosition)
		ScrollRelative(SCROLLRELATIVE_DOWN, minimum(MaxScrollMultiplier, (MousePos - BottomScrollPosition) * ScrollSpeedFactor));
}

bool CScrollRegion::RectClipped(const CUIRect &Rect) const
{
	return (m_ClipRect.x > (Rect.x + Rect.w) || (m_ClipRect.x + m_ClipRect.w) < Rect.x || m_ClipRect.y > (Rect.y + Rect.h) || (m_ClipRect.y + m_ClipRect.h) < Rect.y);
}

bool CScrollRegion::ContentOverflows() const
{
	return m_Params.m_ScrollHorizontal ? m_ContentSize > m_ClipRect.w : m_ContentSize > m_ClipRect.h;
}

bool CScrollRegion::ScrollbarShown() const
{
	return ContentOverflows() || m_Params.m_ForceShowScrollbar;
}

bool CScrollRegion::Animating() const
{
	return m_AnimTime > 0.0f;
}

float CScrollRegion::ContentAreaPos() const
{
	return m_Params.m_ScrollHorizontal ? m_ClipRect.x : m_ClipRect.y;
}

float CScrollRegion::ContentAreaSize() const
{
	return m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
}

float CScrollRegion::MaxScroll() const
{
	return maximum(0.0f, m_ContentSize - ContentAreaSize());
}

bool CScrollRegion::Active() const
{
	return Ui()->ActiveItem() == &m_ScrollPos;
}

CUIRect CScrollRegion::SplitContentArea()
{
	CUIRect ScrollbarBg;
	if(m_Params.m_ScrollHorizontal)
		m_ClipRect.HSplitBottom(m_Params.m_ScrollbarThickness, ScrollbarShown() ? &m_ClipRect : nullptr, &ScrollbarBg);
	else
		m_ClipRect.VSplitRight(m_Params.m_ScrollbarThickness, ScrollbarShown() ? &m_ClipRect : nullptr, &ScrollbarBg);
	if(m_Params.m_ScrollbarNoOuterMargin)
	{
		if(m_Params.m_ScrollHorizontal)
		{
			ScrollbarBg.VMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.HSplitTop(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
		else
		{
			ScrollbarBg.HMargin(m_Params.m_ScrollbarMargin, &m_RailRect);
			m_RailRect.VSplitLeft(m_Params.m_ScrollbarMargin, nullptr, &m_RailRect);
		}
	}
	else
	{
		ScrollbarBg.Margin(m_Params.m_ScrollbarMargin, &m_RailRect);
	}

	return ScrollbarBg;
}

void CScrollRegion::DrawBackground(const CUIRect &ScrollbarBg)
{
	// only show scrollbar if required
	if(ScrollbarShown())
	{
		if(m_Params.m_ScrollbarBgColor.a > 0.0f)
		{
			int Corners = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_B : IGraphics::CORNER_R;
			ScrollbarBg.Draw(m_Params.m_ScrollbarBgColor, Corners, 4.0f);
		}
		if(m_Params.m_RailBgColor.a > 0.0f)
		{
			float Rounding = m_Params.m_ScrollHorizontal ? m_RailRect.h / 2.0f : m_RailRect.w / 2.0f;
			m_RailRect.Draw(m_Params.m_RailBgColor, IGraphics::CORNER_ALL, Rounding);
		}
	}
	if(m_Params.m_ClipBgColor.a > 0.0f)
	{
		int CornersPartial = m_Params.m_ScrollHorizontal ? IGraphics::CORNER_T : IGraphics::CORNER_L;
		m_ClipRect.Draw(m_Params.m_ClipBgColor, ScrollbarShown() ? CornersPartial : IGraphics::CORNER_ALL, 4.0f);
	}
}

void CScrollRegion::DoScrollInput()
{
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;

	if(m_ScrollDirection != SCROLLRELATIVE_NONE || Ui()->HotScrollRegion() == this)
	{
		bool ProgrammaticScroll = false;
		if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_UP))
			m_ScrollDirection = SCROLLRELATIVE_UP;
		else if(Ui()->ConsumeHotkey(CUi::HOTKEY_SCROLL_DOWN))
			m_ScrollDirection = SCROLLRELATIVE_DOWN;
		else
			ProgrammaticScroll = true;

		if(!ProgrammaticScroll)
			m_ScrollSpeedMultiplier = 1.0f;

		if(Input()->ModifierIsPressed())
			m_ScrollDirection = SCROLLRELATIVE_NONE;

		if(m_ScrollDirection != SCROLLRELATIVE_NONE)
		{
			const bool IsPageScroll = Input()->AltIsPressed();
			const float ScrollUnit = IsPageScroll && !ProgrammaticScroll ? ClipSize : m_Params.m_ScrollUnit;

			m_AnimTimeMax = g_Config.m_UiSmoothScrollTime / 1000.0f;
			m_AnimTime = m_AnimTimeMax;
			m_AnimInitScrollPos = m_ScrollPos;
			m_AnimTargetScrollPos = (ProgrammaticScroll ? m_ScrollPos : m_AnimTargetScrollPos) + (int)m_ScrollDirection * ScrollUnit * m_ScrollSpeedMultiplier;
			m_ScrollDirection = SCROLLRELATIVE_NONE;
			m_ScrollSpeedMultiplier = 1.0f;
		}
	}
}

void CScrollRegion::UpdateHotScrollRegion()
{
	CUIRect RegionRect = m_ClipRect;
	if(m_Params.m_ScrollHorizontal)
		RegionRect.h += m_Params.m_ScrollbarThickness;
	else
		RegionRect.w += m_Params.m_ScrollbarThickness;

	if(Ui()->Enabled() && Ui()->MouseHovered(&RegionRect))
	{
		Ui()->SetHotScrollRegion(this);
	}
}

void CScrollRegion::AdvanceAnimation()
{
	const float ScrollMax = MaxScroll();

	if(m_RequestScrollPos >= 0.0f)
	{
		StartScrollAnimation(m_RequestScrollPos);
		m_RequestScrollPos = -1.0f;
	}

	m_AnimTargetScrollPos = std::clamp(m_AnimTargetScrollPos, 0.0f, ScrollMax);

	if(absolute(m_AnimInitScrollPos - m_AnimTargetScrollPos) < 0.5f)
		m_AnimTime = 0.0f;

	if(m_AnimTime > 0.0f && !Input()->ModifierIsPressed())
	{
		m_AnimTime -= Client()->RenderFrameTime();
		if(m_AnimTime < 0.0f)
		{
			m_AnimTime = 0.0f;
		}
		const float AnimProgress = (1.0f - std::pow(m_AnimTime / m_AnimTimeMax, 3.0f));
		m_ScrollPos = m_AnimInitScrollPos + (m_AnimTargetScrollPos - m_AnimInitScrollPos) * AnimProgress;
	}
	else
	{
		m_ScrollPos = m_AnimTargetScrollPos;
	}
}

void CScrollRegion::DoSlider()
{
	const float ClipSize = m_Params.m_ScrollHorizontal ? m_ClipRect.w : m_ClipRect.h;
	const float RailSize = m_Params.m_ScrollHorizontal ? m_RailRect.w : m_RailRect.h;
	const float ScrollMax = MaxScroll();
	const bool CanScroll = m_ContentSize > 0.0f && ScrollMax > 0.0f && RailSize > 0.0f;
	const float SliderMaxSize = maximum(0.0f, RailSize);
	const float SliderMinSize = minimum(m_Params.m_SliderMinSize, SliderMaxSize);
	const float SliderSize = CanScroll ? std::clamp(ClipSize / m_ContentSize * RailSize, SliderMinSize, SliderMaxSize) : SliderMaxSize;

	CUIRect Slider = m_RailRect;
	float &SliderPos = m_Params.m_ScrollHorizontal ? Slider.x : Slider.y;
	if(m_Params.m_ScrollHorizontal)
		Slider.w = SliderSize;
	else
		Slider.h = SliderSize;

	const float MaxSlider = RailSize - SliderSize;
	const void *pId = &m_ScrollPos;

	if(!CanScroll || MaxSlider <= 0.0f)
	{
		m_ScrollPos = 0.0f;
		m_AnimInitScrollPos = 0.0f;
		m_AnimTargetScrollPos = 0.0f;
		m_AnimTime = 0.0f;
		m_RequestScrollPos = -1.0f;
		if(m_Params.m_ScrollHorizontal)
			m_ContentScrollOff.x = 0.0f;
		else
			m_ContentScrollOff.y = 0.0f;
		const bool WasActive = Ui()->IsActiveItem(pId);
		const bool Active = ScrollRegionShouldKeepNoScrollSliderActive(WasActive, Ui()->MouseButton(0));
		if(Active)
			Ui()->SetActiveItem(pId);
		else if(WasActive)
		{
			Ui()->SetActiveItem(nullptr);
		}
		const float Rounding = m_Params.m_ScrollHorizontal ? Slider.h / 2.0f : Slider.w / 2.0f;
		Slider.Draw(m_Params.SliderColor(Active, Ui()->HotItem() == pId), IGraphics::CORNER_ALL, Rounding);
		return;
	}

	SliderPos += m_ScrollPos / ScrollMax * MaxSlider;

	const float MousePos = m_Params.m_ScrollHorizontal ? Ui()->MouseX() : Ui()->MouseY();
	const bool WasActive = Ui()->ActiveItem() == pId;
	Ui()->DoButtonLogic(pId, 0, &m_RailRect, BUTTONFLAG_LEFT); // Result ignored, we only care about the button becoming and being active
	if(Ui()->CheckActiveItem(pId))
	{
		if(!WasActive)
		{
			m_SliderGrabPos = Ui()->MouseHovered(&Slider) ? (MousePos - SliderPos) : (SliderSize / 2.0f);
			m_SliderGrabPos = std::clamp(m_SliderGrabPos, 0.0f, SliderSize);
		}
		m_ScrollPos += (MousePos - (SliderPos + m_SliderGrabPos)) / MaxSlider * ScrollMax;
		m_AnimTargetScrollPos = m_ScrollPos;
		m_AnimTime = 0.0f;
	}

	m_ScrollPos = std::clamp(m_ScrollPos, 0.0f, ScrollMax);
	if(m_Params.m_ScrollHorizontal)
		m_ContentScrollOff.x = -m_ScrollPos;
	else
		m_ContentScrollOff.y = -m_ScrollPos;

	const float Rounding = m_Params.m_ScrollHorizontal ? Slider.h / 2.0f : Slider.w / 2.0f;
	Slider.Draw(m_Params.SliderColor(Ui()->CheckActiveItem(pId), Ui()->HotItem() == pId), IGraphics::CORNER_ALL, Rounding);
}
