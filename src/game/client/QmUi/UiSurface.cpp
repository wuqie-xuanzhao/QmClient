/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "UiSurface.h"

#include <engine/graphics.h>

#include <game/client/ui.h>

#include <algorithm>

bool DrawRoundedSurface(IGraphics *pGraphics, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const SRoundedSurfaceParams &Params)
{
	if(pGraphics == nullptr || Rect.w <= 0.0f || Rect.h <= 0.0f)
		return false;

	const SRoundedSurfacePlan Plan = ResolveRoundedSurfacePlan(Rect, Params, pGraphics->HasRoundedRectSdf());
	if(Plan.m_UseSdf)
	{
		IGraphics::SRoundedRectSdfParams SdfParams;
		SdfParams.m_Rect = vec4(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h);
		SdfParams.m_FillColor = vec4(Fill.r, Fill.g, Fill.b, Fill.a);
		const ColorRGBA ResolvedBorder = Plan.m_BorderWidth > 0.0f ? Border : Fill;
		SdfParams.m_BorderColor = vec4(ResolvedBorder.r, ResolvedBorder.g, ResolvedBorder.b, ResolvedBorder.a);
		SdfParams.m_CornerRadii = Plan.m_CornerRadii;
		SdfParams.m_Params = vec4(Plan.m_BorderWidth, Plan.m_PixelSize, Plan.m_PixelSize * 2.0f, 0.0f);
		pGraphics->RenderRoundedRectSdf(SdfParams);
		return true;
	}

	if(Plan.m_BorderWidth <= 0.0f)
	{
		pGraphics->DrawRect(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h, Fill, Params.m_Corners, Plan.m_Radius);
		return true;
	}

	pGraphics->DrawRect(Plan.m_Rect.x, Plan.m_Rect.y, Plan.m_Rect.w, Plan.m_Rect.h, Border, Params.m_Corners, Plan.m_Radius);
	CUIRect Inner = Plan.m_Rect;
	Inner.Margin(Plan.m_BorderWidth, &Inner);
	if(Inner.w > 0.0f && Inner.h > 0.0f)
		pGraphics->DrawRect(Inner.x, Inner.y, Inner.w, Inner.h, Fill, Params.m_Corners, std::max(0.0f, Plan.m_Radius - Plan.m_BorderWidth));
	return true;
}

bool DrawRoundedSurface(CUi *pUi, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const float Radius, const float BorderWidth, const int Corners)
{
	if(pUi == nullptr)
		return false;
	SRoundedSurfaceParams Params;
	Params.m_Radius = Radius;
	Params.m_BorderWidth = BorderWidth;
	Params.m_PixelSize = pUi->PixelSize();
	Params.m_Corners = Corners;
	return DrawRoundedSurface(pUi->Graphics(), Rect, Fill, Border, Params);
}

bool DrawRoundedSurface(const IUiContext &Ctx, const CUIRect &Rect, const ColorRGBA &Fill, const ColorRGBA &Border, const float Radius, const float BorderWidth, const int Corners)
{
	return DrawRoundedSurface(Ctx.m_pUi, Rect, Fill, Border, Radius, BorderWidth, Corners);
}
