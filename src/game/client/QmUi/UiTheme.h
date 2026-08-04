/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UITHEME_H
#define GAME_CLIENT_QMUI_UITHEME_H

#include <base/color.h>

#include <algorithm>

struct SUiTheme
{
	ColorRGBA m_Surface;
	ColorRGBA m_SurfaceHovered;
	ColorRGBA m_SurfaceFocused;
	ColorRGBA m_Border;
	ColorRGBA m_BorderHovered;
	ColorRGBA m_BorderFocused;
	ColorRGBA m_InputSurface;
	ColorRGBA m_InputSurfaceFocused;
	ColorRGBA m_FocusRing;
	ColorRGBA m_Accent;
	ColorRGBA m_TextTitle;
	ColorRGBA m_TextBody;
	ColorRGBA m_TextSmall;
	float m_FocusRingWidth = 2.0f;
	float m_FocusRingInset = 1.0f;
};

inline SUiTheme ResolveUiTheme(const ColorHSLA BaseColor, float Opacity, const ColorHSLA FocusColor = ColorHSLA(0.60f, 0.78f, 0.52f, 1.0f))
{
	Opacity = std::clamp(Opacity, 0.0f, 1.0f);
	const ColorRGBA SurfaceBase = color_cast<ColorRGBA>(BaseColor.UnclampLighting(0.42f));
	const ColorRGBA AccentBase = color_cast<ColorRGBA>(BaseColor.UnclampLighting(0.48f));
	SUiTheme Theme;
	Theme.m_Surface = SurfaceBase.WithAlpha(std::clamp(std::max(SurfaceBase.a, 0.70f) * Opacity, 0.0f, 1.0f));
	Theme.m_SurfaceHovered = ColorRGBA(
		std::clamp(Theme.m_Surface.r * 1.06f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.g * 1.06f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.b * 1.06f, 0.0f, 1.0f), Theme.m_Surface.a);
	Theme.m_SurfaceFocused = Theme.m_SurfaceHovered;
	Theme.m_Border = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f * Opacity);
	Theme.m_BorderHovered = AccentBase.WithAlpha(0.45f * Opacity);
	Theme.m_BorderFocused = AccentBase.WithAlpha(0.75f * Opacity);
	Theme.m_InputSurface = ColorRGBA(
		std::clamp(Theme.m_Surface.r * 0.88f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.g * 0.88f, 0.0f, 1.0f),
		std::clamp(Theme.m_Surface.b * 0.88f, 0.0f, 1.0f), Theme.m_Surface.a);
	Theme.m_InputSurfaceFocused = Theme.m_InputSurface;
	// 焦点环是键盘与输入焦点的唯一稳定反馈，不随低表面透明度弱化到不可辨认。
	Theme.m_FocusRing = color_cast<ColorRGBA>(FocusColor).WithAlpha(std::clamp(std::max(0.60f, 0.90f * Opacity), 0.0f, 1.0f));
	Theme.m_Accent = AccentBase.WithAlpha(std::clamp(std::max(AccentBase.a, 0.85f) * Opacity, 0.0f, 1.0f));
	Theme.m_TextTitle = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	Theme.m_TextBody = ColorRGBA(0.92f, 0.92f, 0.94f, 1.0f);
	Theme.m_TextSmall = ColorRGBA(0.72f, 0.74f, 0.78f, 1.0f);
	return Theme;
}

inline SUiTheme ResolveInputFallbackTheme(const unsigned FocusColor)
{
	return ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f, ColorHSLA(FocusColor));
}

#endif
