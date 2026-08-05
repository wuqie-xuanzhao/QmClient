/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ui_rect.h"

#include <base/vmath.h>

#include <engine/client/rounded_rect_geometry.h>
#include <engine/graphics.h>

#include <algorithm>
#include <cmath>

IGraphics *CUIRect::ms_pGraphics = nullptr;

namespace
{
	float CurrentPixelSize(IGraphics *pGraphics)
	{
		float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
		pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
		const int ScreenWidth = pGraphics->ScreenWidth();
		return ScreenWidth > 0 ? std::max(std::abs(ScreenX1 - ScreenX0) / ScreenWidth, 0.0001f) : 0.0001f;
	}

	vec4 CornerRadii(const float Radius, const int Corners)
	{
		return vec4(
			Corners & IGraphics::CORNER_TL ? Radius : 0.0f,
			Corners & IGraphics::CORNER_TR ? Radius : 0.0f,
			Corners & IGraphics::CORNER_BR ? Radius : 0.0f,
			Corners & IGraphics::CORNER_BL ? Radius : 0.0f);
	}
}

CUIRect CUIRect::Intersection(const CUIRect &Other) const
{
	CUIRect Result;
	Result.x = std::max(x, Other.x);
	Result.y = std::max(y, Other.y);
	Result.w = std::max(0.0f, std::min(x + w, Other.x + Other.w) - Result.x);
	Result.h = std::max(0.0f, std::min(y + h, Other.y + Other.h) - Result.y);
	return Result;
}

void CUIRect::HSplitMid(CUIRect *pTop, CUIRect *pBottom, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.h / 2;
	const float HalfSpacing = Spacing / 2;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut - HalfSpacing;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut + HalfSpacing;
		pBottom->w = r.w;
		pBottom->h = Cut - HalfSpacing;
	}
}

void CUIRect::HSplitTop(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + Cut;
		pBottom->w = r.w;
		pBottom->h = r.h - Cut;
	}
}

void CUIRect::HSplitBottom(float Cut, CUIRect *pTop, CUIRect *pBottom) const
{
	CUIRect r = *this;

	if(pTop)
	{
		pTop->x = r.x;
		pTop->y = r.y;
		pTop->w = r.w;
		pTop->h = r.h - Cut;
	}

	if(pBottom)
	{
		pBottom->x = r.x;
		pBottom->y = r.y + r.h - Cut;
		pBottom->w = r.w;
		pBottom->h = Cut;
	}
}

void CUIRect::VSplitMid(CUIRect *pLeft, CUIRect *pRight, float Spacing) const
{
	CUIRect r = *this;
	const float Cut = r.w / 2;
	const float HalfSpacing = Spacing / 2;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut - HalfSpacing;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut + HalfSpacing;
		pRight->y = r.y;
		pRight->w = Cut - HalfSpacing;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitLeft(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + Cut;
		pRight->y = r.y;
		pRight->w = r.w - Cut;
		pRight->h = r.h;
	}
}

void CUIRect::VSplitRight(float Cut, CUIRect *pLeft, CUIRect *pRight) const
{
	CUIRect r = *this;

	if(pLeft)
	{
		pLeft->x = r.x;
		pLeft->y = r.y;
		pLeft->w = r.w - Cut;
		pLeft->h = r.h;
	}

	if(pRight)
	{
		pRight->x = r.x + r.w - Cut;
		pRight->y = r.y;
		pRight->w = Cut;
		pRight->h = r.h;
	}
}

void CUIRect::Margin(vec2 Cut, CUIRect *pOtherRect) const
{
	CUIRect r = *this;

	pOtherRect->x = r.x + Cut.x;
	pOtherRect->y = r.y + Cut.y;
	pOtherRect->w = r.w - 2 * Cut.x;
	pOtherRect->h = r.h - 2 * Cut.y;
}

void CUIRect::Margin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(Cut, Cut), pOtherRect);
}

void CUIRect::VMargin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(Cut, 0.0f), pOtherRect);
}

void CUIRect::HMargin(float Cut, CUIRect *pOtherRect) const
{
	Margin(vec2(0.0f, Cut), pOtherRect);
}

bool CUIRect::Inside(vec2 Point) const
{
	return Point.x >= x && Point.x < x + w && Point.y >= y && Point.y < y + h;
}

void CUIRect::Draw(ColorRGBA Color, int Corners, float Rounding) const
{
	if(ms_pGraphics->HasRoundedRectSdf() && Rounding > 0.0f && Corners != IGraphics::CORNER_NONE)
	{
		const float PixelSize = CurrentPixelSize(ms_pGraphics);
		const SRoundedRectGeometry Geometry = ResolveRoundedRectGeometry(x, y, w, h, Rounding, PixelSize);
		IGraphics::SRoundedRectSdfParams Params;
		Params.m_Rect = vec4(Geometry.m_X, Geometry.m_Y, Geometry.m_W, Geometry.m_H);
		Params.m_FillColor = vec4(Color.r, Color.g, Color.b, Color.a);
		Params.m_BorderColor = Params.m_FillColor;
		Params.m_CornerRadii = CornerRadii(Geometry.m_Rounding, Corners);
		Params.m_Params = vec4(0.0f, PixelSize, PixelSize * 2.0f, 0.0f);
		ms_pGraphics->RenderRoundedRectSdf(Params);
		return;
	}
	ms_pGraphics->DrawRect(x, y, w, h, Color, Corners, Rounding);
}

void CUIRect::Draw4(ColorRGBA ColorTopLeft, ColorRGBA ColorTopRight, ColorRGBA ColorBottomLeft, ColorRGBA ColorBottomRight, int Corners, float Rounding) const
{
	ms_pGraphics->DrawRect4(x, y, w, h, ColorTopLeft, ColorTopRight, ColorBottomLeft, ColorBottomRight, Corners, Rounding);
}

void CUIRect::DrawOutline(ColorRGBA Color) const
{
	const IGraphics::CLineItem aArray[] = {
		IGraphics::CLineItem(x, y, x + w, y),
		IGraphics::CLineItem(x + w, y, x + w, y + h),
		IGraphics::CLineItem(x + w, y + h, x, y + h),
		IGraphics::CLineItem(x, y + h, x, y)};
	ms_pGraphics->TextureClear();
	ms_pGraphics->LinesBegin();
	ms_pGraphics->SetColor(Color);
	ms_pGraphics->LinesDraw(aArray, std::size(aArray));
	ms_pGraphics->LinesEnd();
}
