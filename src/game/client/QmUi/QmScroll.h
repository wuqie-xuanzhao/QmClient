/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMSCROLL_H
#define GAME_CLIENT_QMUI_QMSCROLL_H

#include <game/client/ui_rect.h>

struct SQmScrollMetrics
{
	float m_ViewportSize = 0.0f;
	float m_ContentSize = 0.0f;

	float MaxOffset() const;
};

struct SQmScrollConfig
{
	float m_WheelScale = 1.0f;
	bool m_NativeWheelStep = false;
	float m_NativeWheelAnimationTime = 0.5f;
	float m_Friction = 9.0f;
	float m_OverscrollStiffness = 120.0f;
	float m_OverscrollDamping = 22.0f;
	float m_MaxOverscroll = 72.0f;
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
	bool m_ContentDragAllowed = false;
	bool m_ContentDragBlocked = false;
};

struct SQmScrollContainerStyle
{
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

SQmScrollContainerStyle QmScrollContainerStyleForSize(EQmScrollSize Size, float UiScale = 1.0f);
SQmScrollConfig QmNativeWheelScrollConfig(float UiScale, float SmoothScrollTimeSec);

class CQmScrollState
{
public:
	void Reset();
	void SetOffset(float Offset, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig(), bool AllowOverscroll = false);
	void AddWheelImpulse(float WheelDelta, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig());
	void Advance(float Dt, const SQmScrollMetrics &Metrics, const SQmScrollConfig &Config = SQmScrollConfig());

	float Offset() const { return m_Offset; }
	float Velocity() const { return m_Velocity; }

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
	bool m_ScrollbarVisible = false;
};

class CQmScrollContainer
{
public:
	void Reset();
	void ScrollByWheel(float WheelDelta, float ViewportHeight, float ContentHeight, const SQmScrollConfig &Config = SQmScrollConfig());
	SQmScrollContainerFrame PreviewFrame(const CUIRect &ViewRect, float ContentHeight, const SQmScrollContainerStyle &Style = SQmScrollContainerStyle()) const;
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollConfig &Config = SQmScrollConfig());
	SQmScrollContainerFrame Update(const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style = SQmScrollContainerStyle(), const SQmScrollConfig &Config = SQmScrollConfig());

	float Offset() const { return m_State.Offset(); }
	float Velocity() const { return m_State.Velocity(); }
	bool ScrollbarDragActive() const { return m_ScrollbarDragActive; }
	bool ContentDragActive() const { return m_ContentDragActive; }

private:
	CQmScrollState m_State;
	bool m_ScrollbarDragActive = false;
	float m_ScrollbarGrabY = 0.0f;
	bool m_ContentDragActive = false;
	bool m_ContentDragCandidate = false;
	float m_ContentDragPressMouseY = 0.0f;
	float m_ContentDragPressOffset = 0.0f;
	float m_ContentDragLastMouseY = 0.0f;
};

#endif
