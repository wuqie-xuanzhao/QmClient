/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UISURFACE_H
#define GAME_CLIENT_QMUI_UISURFACE_H

#include "UiContext.h"

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>

class CUi;

struct SRoundedSurfacePlan
{
	CUIRect m_Rect{};
	float m_Radius = 0.0f;
	float m_BorderWidth = 0.0f;
	float m_PixelSize = 0.0001f;
	vec4 m_CornerRadii{};
	bool m_UseSdf = false;
};

inline vec4 ResolveRoundedSurfaceCornerRadii(const float Radius, const int Corners)
{
	return vec4(
		Corners & IGraphics::CORNER_TL ? Radius : 0.0f,
		Corners & IGraphics::CORNER_TR ? Radius : 0.0f,
		Corners & IGraphics::CORNER_BR ? Radius : 0.0f,
		Corners & IGraphics::CORNER_BL ? Radius : 0.0f);
}

inline SRoundedSurfacePlan ResolveRoundedSurfacePlan(const CUIRect &Rect, const float Radius, const float BorderWidth, const float PixelSize, const int Corners, const bool HasSdf)
{
	const auto AlignToPixel = [](const float Value, const float Size) {
		return Size > 0.0f ? std::round(Value / Size) * Size : Value;
	};
	SRoundedSurfacePlan Plan;
	Plan.m_PixelSize = std::max(PixelSize, 0.0001f);
	const float Right = AlignToPixel(Rect.x + Rect.w, Plan.m_PixelSize);
	const float Bottom = AlignToPixel(Rect.y + Rect.h, Plan.m_PixelSize);
	Plan.m_Rect = {AlignToPixel(Rect.x, Plan.m_PixelSize), AlignToPixel(Rect.y, Plan.m_PixelSize), 0.0f, 0.0f};
	Plan.m_Rect.w = Right - Plan.m_Rect.x;
	Plan.m_Rect.h = Bottom - Plan.m_Rect.y;
	if(Plan.m_Rect.w <= 0.0f || Plan.m_Rect.h <= 0.0f)
		Plan.m_Rect = Rect;
	const float MaxRadius = std::max(0.0f, std::min(Plan.m_Rect.w, Plan.m_Rect.h) * 0.5f);
	Plan.m_Radius = std::clamp(AlignToPixel(Radius, Plan.m_PixelSize), 0.0f, MaxRadius);
	const float MaxBorderWidth = std::max(0.0f, std::min(Plan.m_Rect.w, Plan.m_Rect.h) * 0.5f);
	Plan.m_BorderWidth = std::clamp(AlignToPixel(BorderWidth, Plan.m_PixelSize), 0.0f, MaxBorderWidth);
	Plan.m_CornerRadii = ResolveRoundedSurfaceCornerRadii(Plan.m_Radius, Corners);
	Plan.m_UseSdf = HasSdf && Plan.m_Rect.w > 0.0f && Plan.m_Rect.h > 0.0f;
	return Plan;
}
bool DrawRoundedSurface(CUi *pUi, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, float Radius, float BorderWidth = 0.0f, int Corners = IGraphics::CORNER_ALL);
bool DrawRoundedSurface(const IUiContext &Ctx, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, float Radius, float BorderWidth = 0.0f, int Corners = IGraphics::CORNER_ALL);

#endif
