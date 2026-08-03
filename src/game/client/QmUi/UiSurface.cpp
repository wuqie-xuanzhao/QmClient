/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiSurface.h"

#include <engine/graphics.h>

#include <game/client/ui.h>

#include <algorithm>

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
		Params.m_Params = vec4(Plan.m_BorderWidth, Plan.m_PixelSize, Plan.m_PixelSize * 2.0f, 0.0f);
		pGraphics->RenderRoundedRectSdf(Params);
		return true;
	}

	if(Plan.m_BorderWidth <= 0.0f)
	{
		pGraphics->DrawRect(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h, Fill, Corners, Plan.m_Radius);
		return true;
	}

	pGraphics->DrawRect(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h, Border, Corners, Plan.m_Radius);
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
