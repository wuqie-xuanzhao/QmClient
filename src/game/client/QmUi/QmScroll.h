/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMSCROLL_H
#define GAME_CLIENT_QMUI_QMSCROLL_H

#include <game/client/ui_rect.h>

constexpr float QmScrollAltMultiplier()
{
	return 3.0f;
}

struct SQmScrollMetrics
{
	float m_ViewportSize = 0.0f;
	float m_ContentSize = 0.0f;

	float MaxOffset() const;
};

struct SQmScrollConfig
{
	float m_WheelScale = 10.0f;
	bool m_NativeWheelStep = true;
	float m_NativeWheelAnimationTime = 0.0f;
	float m_Friction = 9.0f;
	float m_OverscrollStiffness = 120.0f;
	float m_OverscrollDamping = 22.0f;
	float m_MaxOverscroll = 0.0f;
	float m_RestVelocity = 0.25f;
	float m_RestDistance = 0.25f;
};

struct SQmScrollContainerInput
{
	float m_WheelDelta = 0.0f;
	float m_MouseX = 0.0f;
	float m_MouseY = 0.0f;
	bool m_Hovered = false;
	bool m_MouseValid = false;
	bool m_MouseDown = false;
	bool m_MousePressed = false;
	bool m_ThumbHovered = false;
	bool m_TrackHovered = false;
	bool m_ModifierPressed = false;
	bool m_AltPressed = false;
	bool m_ContentDragAllowed = false;
	bool m_ContentDragBlocked = false;
};

enum class EQmScrollAxis
{
	VERTICAL,
	HORIZONTAL,
};

struct SQmScrollContainerStyle
{
	EQmScrollAxis m_Axis = EQmScrollAxis::VERTICAL;
	float m_ScrollbarWidth = 10.0f;
	float m_ScrollbarMargin = 2.0f;
	float m_MinThumbHeight = 24.0f;
	float m_ContentDragThreshold = 6.0f;
};

enum class EQmScrollSize
{
	SMALL,
	MEDIUM,
	LARGE,
};

enum class EQmScrollProfile
{
	SETTINGS_PAGE,
	MENU_LIST,
	POPUP_LIST,
	FILTER_GRID,
};

enum class EQmScrollRailVisibility
{
	AUTO,
	HIDDEN,
};

struct SQmScrollRequest
{
	EQmScrollProfile m_Profile = EQmScrollProfile::MENU_LIST;
	EQmScrollAxis m_Axis = EQmScrollAxis::VERTICAL;
	float m_RowExtent = 0.0f;
	int m_RowsPerStep = 0;
};

struct SQmResolvedScrollPolicy
{
	SQmScrollContainerStyle m_Style;
	SQmScrollConfig m_Config;
	EQmScrollRailVisibility m_RailVisibility = EQmScrollRailVisibility::AUTO;
	float m_AltMultiplier = QmScrollAltMultiplier();
	int m_MaxVisibleItems = 0;
	bool m_ContentDragAllowed = true;
};

SQmScrollContainerStyle QmScrollContainerStyleForSize(EQmScrollSize Size, float UiScale = 1.0f);
SQmScrollConfig QmNativeWheelScrollConfig(float UiScale, float SmoothScrollTimeSec);
SQmScrollConfig QmSettingsScrollConfig(float UiScale, float SmoothScrollTimeSec);
SQmResolvedScrollPolicy QmResolveScrollPolicy(const SQmScrollRequest &Request, float UiScale = 1.0f, float SmoothScrollTimeSec = 0.0f);

class CQmScrollState
{
public:
	void Reset();
	void SetOffset(float Offset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig(), bool AllowOverscroll = false);
	void ScrollTo(float TargetOffset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig());
	void AddWheelImpulse(float WheelDelta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig());
	void Advance(float Dt, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig(), bool PauseNativeWheelAnimation = false);

	float Offset() const { return m_Offset; }
	float Velocity() const { return m_Velocity; }
	bool Animating() const { return m_AnimTime > 0.0f; }

private:
	float m_Offset = 0.0f;
	float m_Velocity = 0.0f;
	float m_LastMaxOffset = 0.0f;
	float m_AnimTime = 0.0f;
	float m_AnimTimeMax = 0.0f;
	float m_AnimStartOffset = 0.0f;
	float m_AnimTargetOffset = 0.0f;
};

struct SQmScrollContainerFrame
{
	CUIRect m_ClipRect{};
	CUIRect m_ContentRect{};
	CUIRect m_ScrollbarTrackRect{};
	CUIRect m_ScrollbarThumbRect{};
	float m_Offset = 0.0f;
	bool m_Scrollable = false;
	bool m_ScrollbarVisible = false;
};

class CQmScrollController
{
public:
	void Reset();
	void ScrollByWheel(float WheelDelta, float ViewportHeight, float ContentHeight, const SQmScrollConfig &Config = SQmScrollConfig());
	SQmScrollContainerFrame PreviewFrame(const CUIRect &ViewRect, float ContentHeight, const SQmScrollContainerStyle &Style = SQmScrollContainerStyle()) const;
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollConfig &Config = SQmScrollConfig());
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style = SQmScrollContainerStyle(), const SQmScrollConfig &Config = SQmScrollConfig());
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollRequest &Request, float UiScale, float SmoothScrollTimeSec);

	float Offset() const { return m_State.Offset(); }
	float Velocity() const { return m_State.Velocity(); }
	bool ScrollbarDragActive() const { return m_ScrollbarDragActive; }
	bool ContentDragActive() const { return m_ContentDragActive; }

private:
	SQmScrollContainerFrame UpdateInternal(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style, const SQmScrollConfig &Config, bool RenderRail);
	CQmScrollState m_State;
	bool m_ScrollbarDragActive = false;
	float m_ScrollbarGrabY = 0.0f;
	bool m_ContentDragActive = false;
	bool m_ContentDragCandidate = false;
	float m_ContentDragPressMousePos = 0.0f;
	float m_ContentDragPressOffset = 0.0f;
	float m_ContentDragLastMousePos = 0.0f;
};

using CQmScrollContainer = CQmScrollController;

#endif
