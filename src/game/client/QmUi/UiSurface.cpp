/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiSurface.h"

#include <engine/graphics.h>

#include <game/client/ui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>

namespace
{
	void DrawFallbackBorderRing(IGraphics *pGraphics, const CUIRect &Rect, const ColorRGBA &Border, const int Corners, const float Radius, const float BorderWidth, const float PixelSize)
	{
		if(BorderWidth <= 0.0f)
			return;
		if(BorderWidth * 2.0f >= std::min(Rect.w, Rect.h))
		{
			pGraphics->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, Border, Corners, Radius);
			return;
		}

		const float TlRadius = Corners & IGraphics::CORNER_TL ? Radius : 0.0f;
		const float TrRadius = Corners & IGraphics::CORNER_TR ? Radius : 0.0f;
		const float BlRadius = Corners & IGraphics::CORNER_BL ? Radius : 0.0f;
		const float BrRadius = Corners & IGraphics::CORNER_BR ? Radius : 0.0f;
		const float TlInnerRadius = std::max(0.0f, TlRadius - BorderWidth);
		const float TrInnerRadius = std::max(0.0f, TrRadius - BorderWidth);
		const float BlInnerRadius = std::max(0.0f, BlRadius - BorderWidth);
		const float BrInnerRadius = std::max(0.0f, BrRadius - BorderWidth);
		const IGraphics::CFreeformItem aSides[] = {
			IGraphics::CFreeformItem(vec2(Rect.x + BorderWidth + TlInnerRadius, Rect.y + BorderWidth), vec2(Rect.x + TlRadius, Rect.y), vec2(Rect.x + Rect.w - TrRadius, Rect.y), vec2(Rect.x + Rect.w - BorderWidth - TrInnerRadius, Rect.y + BorderWidth)),
			IGraphics::CFreeformItem(vec2(Rect.x + Rect.w - BorderWidth, Rect.y + BorderWidth + TrInnerRadius), vec2(Rect.x + Rect.w, Rect.y + TrRadius), vec2(Rect.x + Rect.w, Rect.y + Rect.h - BrRadius), vec2(Rect.x + Rect.w - BorderWidth, Rect.y + Rect.h - BorderWidth - BrInnerRadius)),
			IGraphics::CFreeformItem(vec2(Rect.x + Rect.w - BorderWidth - BrInnerRadius, Rect.y + Rect.h - BorderWidth), vec2(Rect.x + Rect.w - BrRadius, Rect.y + Rect.h), vec2(Rect.x + BlRadius, Rect.y + Rect.h), vec2(Rect.x + BorderWidth + BlInnerRadius, Rect.y + Rect.h - BorderWidth)),
			IGraphics::CFreeformItem(vec2(Rect.x + BorderWidth, Rect.y + Rect.h - BorderWidth - BlInnerRadius), vec2(Rect.x, Rect.y + Rect.h - BlRadius), vec2(Rect.x, Rect.y + TlRadius), vec2(Rect.x + BorderWidth, Rect.y + BorderWidth + TlInnerRadius)),
		};

		std::array<IGraphics::CFreeformItem, 64> aCornerSegments;
		int NumCornerSegments = 0;
		const auto AddCorner = [&](const vec2 OuterCenter, const vec2 InnerCenter, const float CornerRadius, const float InnerRadius, const float StartAngle) {
			if(CornerRadius <= 0.0f)
				return;
			const int Segments = std::clamp(static_cast<int>(std::ceil(CornerRadius / std::max(PixelSize, 0.5f))), 4, 16);
			for(int Segment = 0; Segment < Segments; ++Segment)
			{
				const float Angle0 = StartAngle + (pi / 2.0f) * Segment / Segments;
				const float Angle1 = StartAngle + (pi / 2.0f) * (Segment + 1) / Segments;
				const vec2 Inner0 = InnerCenter + vec2(std::cos(Angle0), std::sin(Angle0)) * InnerRadius;
				const vec2 Outer0 = OuterCenter + vec2(std::cos(Angle0), std::sin(Angle0)) * CornerRadius;
				const vec2 Outer1 = OuterCenter + vec2(std::cos(Angle1), std::sin(Angle1)) * CornerRadius;
				const vec2 Inner1 = InnerCenter + vec2(std::cos(Angle1), std::sin(Angle1)) * InnerRadius;
				aCornerSegments[NumCornerSegments++] = IGraphics::CFreeformItem(Inner0, Outer0, Outer1, Inner1);
			}
		};
		AddCorner({Rect.x + TlRadius, Rect.y + TlRadius}, {Rect.x + BorderWidth + TlInnerRadius, Rect.y + BorderWidth + TlInnerRadius}, TlRadius, TlInnerRadius, pi);
		AddCorner({Rect.x + Rect.w - TrRadius, Rect.y + TrRadius}, {Rect.x + Rect.w - BorderWidth - TrInnerRadius, Rect.y + BorderWidth + TrInnerRadius}, TrRadius, TrInnerRadius, -pi / 2.0f);
		AddCorner({Rect.x + Rect.w - BrRadius, Rect.y + Rect.h - BrRadius}, {Rect.x + Rect.w - BorderWidth - BrInnerRadius, Rect.y + Rect.h - BorderWidth - BrInnerRadius}, BrRadius, BrInnerRadius, 0.0f);
		AddCorner({Rect.x + BlRadius, Rect.y + Rect.h - BlRadius}, {Rect.x + BorderWidth + BlInnerRadius, Rect.y + Rect.h - BorderWidth - BlInnerRadius}, BlRadius, BlInnerRadius, pi / 2.0f);

		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(Border);
		pGraphics->QuadsDrawFreeform(aSides, std::size(aSides));
		if(NumCornerSegments > 0)
			pGraphics->QuadsDrawFreeform(aCornerSegments.data(), NumCornerSegments);
		pGraphics->QuadsEnd();
		pGraphics->DrawRoundedRectAntialias(Rect.x, Rect.y, Rect.w, Rect.h, Radius, Corners, Border);
	}
}

bool DrawRoundedSurface(IGraphics *pGraphics, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const float Radius, const float BorderWidth, const float PixelSize, const int Corners)
{
	if(pGraphics == nullptr || Rect.w <= 0.0f || Rect.h <= 0.0f)
		return false;

	const SRoundedSurfacePlan Plan = ResolveRoundedSurfacePlan(Rect, Radius, BorderWidth, PixelSize, Corners, pGraphics->HasRoundedRectSdf());
	if(Plan.m_UseSdf)
	{
		IGraphics::SRoundedRectSdfParams Params;
		Params.m_Rect = vec4(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h);
		Params.m_FillColor = vec4(Fill.r, Fill.g, Fill.b, Fill.a);
		const ColorRGBA ResolvedBorder = Plan.m_BorderWidth > 0.0f ? Border : Fill;
		Params.m_BorderColor = vec4(ResolvedBorder.r, ResolvedBorder.g, ResolvedBorder.b, ResolvedBorder.a);
		Params.m_CornerRadii = Plan.m_CornerRadii;
		Params.m_Params = vec4(Plan.m_BorderWidth, Plan.m_PixelSize, Plan.m_PixelSize, 0.0f);
		pGraphics->RenderRoundedRectSdf(Params);
		return true;
	}

	if(Plan.m_BorderWidth <= 0.0f)
	{
		pGraphics->DrawRect(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h, Fill, Corners, Plan.m_Radius);
		return true;
	}

	DrawFallbackBorderRing(pGraphics, Plan.m_Rect, Border, Corners, Plan.m_Radius, Plan.m_BorderWidth, Plan.m_PixelSize);
	CUIRect Inner = Plan.m_Rect;
	Inner.Margin(Plan.m_BorderWidth, &Inner);
	if(Inner.w > 0.0f && Inner.h > 0.0f)
		pGraphics->DrawRect(Inner.x, Inner.y, Inner.w, Inner.h, Fill, Corners, std::max(0.0f, Plan.m_Radius - Plan.m_BorderWidth));
	return true;
}

bool DrawRoundedSurface(CUi *pUi, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const float Radius, const float BorderWidth, const int Corners)
{
	return pUi != nullptr ? DrawRoundedSurface(pUi->Graphics(), Rect, Fill, Border, Radius, BorderWidth, pUi->PixelSize(), Corners) : false;
}

bool DrawRoundedSurface(const IUiContext &Ctx, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const float Radius, const float BorderWidth, const int Corners)
{
	return DrawRoundedSurface(Ctx.m_pUi, Rect, Fill, Border, Radius, BorderWidth, Corners);
}
