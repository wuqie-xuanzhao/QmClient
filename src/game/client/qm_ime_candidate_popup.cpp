/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "qm_ime_candidate_popup.h"

#include "QmUi/QmAnimResolve.h"
#include "QmUi/QmMotion.h"
#include "QmUi/QmTheme.h"
#include "QmUi/QmTree.h"
#include "gameclient.h"
#include "lineinput.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr int MAX_VISIBLE_CANDIDATES = 16;
	constexpr float POPUP_IN_DURATION = 0.12f;
	constexpr float POPUP_OUT_DURATION = 0.08f;
	constexpr float SELECTED_DURATION = 0.08f;

	struct SImeCandidateCell
	{
		int m_Index = -1;
		CUIRect m_Rect = {};
	};

	struct SImeTextMetrics
	{
		float m_Width = 0.0f;
		float m_Height = 0.0f;
		float m_VisualTop = 0.0f;
		float m_VisualHeight = 0.0f;
		float m_DrawOffsetX = 0.0f;
	};

	struct SImeCandidateMetrics
	{
		SImeTextMetrics m_Num;
		SImeTextMetrics m_Text;
	};

	bool HasPopupContent(const SQmImePopupState &State)
	{
		return State.m_Visible && !State.m_Disabled && !State.m_Composition.empty() && !State.m_vCandidates.empty();
	}

	ColorRGBA WithAlpha(ColorRGBA Color, float Alpha)
	{
		Color.a *= Alpha;
		return Color;
	}

	float ResolveMotionValue(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, EUiAnimProperty Property, float Target, float DurationSec)
	{
		SUiAnimTransition Transition;
		Transition.m_DurationSec = DurationSec;
		Transition.m_Easing = EEasing::EASE_OUT;
		Transition = qm_motion::ApplyMotionLevel(Transition, g_Config.m_QmUiMotionLevel);
		return ResolveUiAnimValue(AnimRuntime, NodeKey, Property, Target, Transition.m_DurationSec, Transition.m_Easing);
	}

	CUIRect ResolveMotionRect(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, const CUIRect &Target, float DurationSec)
	{
		SUiAnimTransition Transition;
		Transition.m_DurationSec = DurationSec;
		Transition.m_Easing = EEasing::EASE_OUT;
		Transition = qm_motion::ApplyMotionLevel(Transition, g_Config.m_QmUiMotionLevel);
		return ResolveUiAnimValueRect(AnimRuntime, NodeKey, Target, Transition.m_DurationSec, Transition.m_Easing);
	}

	SImeTextMetrics MeasureImeText(ITextRender *pTextRender, float FontSize, const char *pText, const qm_theme::SImeTheme &Ime)
	{
		SImeTextMetrics Metrics;
		if(pTextRender == nullptr || pText == nullptr || pText[0] == '\0')
			return Metrics;

		float TextHeight = 0.0f;
		float VisualTop = 0.0f;
		float VisualBottom = 0.0f;
		STextSizeProperties TextSizeProps{};
		TextSizeProps.m_pHeight = &TextHeight;
		TextSizeProps.m_pVisualTop = &VisualTop;
		TextSizeProps.m_pVisualBottom = &VisualBottom;
		const float AdvanceWidth = pTextRender->TextWidth(FontSize, pText, -1, -1.0f, TEXTFLAG_DISALLOW_NEWLINE, TextSizeProps);
		Metrics.m_Width = maximum(0.0f, AdvanceWidth) + 2.0f * Ime.m_TextSafePaddingX;
		Metrics.m_Height = maximum(0.0f, TextHeight);
		Metrics.m_VisualTop = VisualTop;
		Metrics.m_VisualHeight = maximum(0.0f, VisualBottom - VisualTop);
		Metrics.m_DrawOffsetX = Ime.m_TextSafePaddingX;
		return Metrics;
	}

	void DrawImeText(ITextRender *pTextRender, float VisualX, float RectY, float RectH, float FontSize, const char *pText, const SImeTextMetrics &Metrics, ColorRGBA Color, float Alpha)
	{
		if(pTextRender == nullptr || pText == nullptr || pText[0] == '\0')
			return;

		pTextRender->TextColor(WithAlpha(Color, Alpha));
		CTextCursor Cursor;
		const float VisualHeight = Metrics.m_VisualHeight > 0.0f ? Metrics.m_VisualHeight : Metrics.m_Height;
		const float TextY = RectY + (RectH - VisualHeight) * 0.5f - Metrics.m_VisualTop;
		Cursor.SetPosition(vec2(VisualX + Metrics.m_DrawOffsetX, TextY));
		Cursor.m_FontSize = FontSize;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_DISALLOW_NEWLINE;
		pTextRender->TextEx(&Cursor, pText);
	}

	float CandidateCellWidth(const qm_theme::SImeTheme &Ime, const SImeCandidateMetrics &Metrics, bool Selected)
	{
		const float PaddingX = Selected ? Ime.m_SelectedPaddingX : Ime.m_CandidatePaddingX;
		return 2.0f * PaddingX + Metrics.m_Num.m_Width + Ime.m_CandidateNumPaddingX + Metrics.m_Text.m_Width;
	}

	int CandidatePageCount(const SQmImePopupState &State)
	{
		if(State.m_PageCount > 1)
			return State.m_PageCount;
		return 0;
	}

	int CandidatePageIndex(const SQmImePopupState &State)
	{
		if(State.m_PageCount <= 0)
			return -1;
		return std::clamp(State.m_PageIndex, 0, State.m_PageCount - 1);
	}
} // namespace

void CQmImeCandidatePopup::Reset()
{
	m_LastState = {};
	m_WasVisible = false;
	++m_PresenceGeneration;
	if(m_PresenceGeneration == 0)
		m_PresenceGeneration = 1;
}

void CQmImeCandidatePopup::Render(CGameClient *pGameClient, const SQmImePopupState &State)
{
	if(pGameClient == nullptr || pGameClient->Graphics() == nullptr || pGameClient->TextRender() == nullptr)
		return;

	const bool TargetVisible = HasPopupContent(State);
	if(TargetVisible)
		m_LastState = State;
	if(!TargetVisible && !m_WasVisible)
		return;

	const SQmImePopupState &DrawState = TargetVisible ? State : m_LastState;
	if(!HasPopupContent(DrawState))
		return;

	IGraphics *pGraphics = pGameClient->Graphics();
	ITextRender *pTextRender = pGameClient->TextRender();
	const qm_theme::SImeTheme &Ime = qm_theme::ImeTheme(true);
	const unsigned OldRenderFlags = pTextRender->GetRenderFlags();
	pTextRender->SetRenderFlags(OldRenderFlags | TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);

	const bool HasCandidates = !DrawState.m_vCandidates.empty();
	const int CandidateCount = minimum((int)DrawState.m_vCandidates.size(), MAX_VISIBLE_CANDIDATES);
	const int PageCount = CandidatePageCount(DrawState);

	float OldScreenX0, OldScreenY0, OldScreenX1, OldScreenY1;
	pGraphics->GetScreen(&OldScreenX0, &OldScreenY0, &OldScreenX1, &OldScreenY1);

	const float Height = Ime.m_ScreenHeight;
	const float Width = Height * pGraphics->ScreenAspect();
	const int ScreenWidth = maximum(pGraphics->ScreenWidth(), 1);
	const int ScreenHeight = maximum(pGraphics->ScreenHeight(), 1);
	const float Margin = Ime.m_ScreenMargin;
	const float ScreenMaxPanelWidth = maximum(Ime.m_MinWidth, Width - 2.0f * Margin);
	const float PreferredMaxPanelWidth = std::clamp(Ime.m_MaxWidth, Ime.m_MinWidth, ScreenMaxPanelWidth);

	pGraphics->MapScreen(0.0f, 0.0f, Width, Height);

	char aPageText[16] = "";
	SImeTextMetrics PageTextMetrics;
	float TrailingWidth = 0.0f;
	const auto SetTrailingText = [&](const char *pText) {
		str_copy(aPageText, pText, sizeof(aPageText));
		PageTextMetrics = MeasureImeText(pTextRender, Ime.m_FontComposition, aPageText, Ime);
		TrailingWidth = maximum(Ime.m_TrailingWidth, PageTextMetrics.m_Width + Ime.m_CompositionTextPaddingX * 2.0f);
	};
	if(PageCount > 1)
	{
		char aFormattedPageText[16];
		str_format(aFormattedPageText, sizeof(aFormattedPageText), "%d/%d", CandidatePageIndex(DrawState) + 1, PageCount);
		SetTrailingText(aFormattedPageText);
	}

	const int SelectedIndex = qm_ime_overlay::NormalizeSelectedCandidateIndex(DrawState.m_SelectedIndex, CandidateCount);

	std::array<SImeCandidateMetrics, MAX_VISIBLE_CANDIDATES> aCandidateMetrics;
	float CandidateTextHeight = MeasureImeText(pTextRender, Ime.m_FontCandidate, "国g", Ime).m_VisualHeight;
	for(int i = 0; i < CandidateCount; ++i)
	{
		char aNum[4];
		str_format(aNum, sizeof(aNum), "%d", (i + 1) % 10);
		aCandidateMetrics[i].m_Num = MeasureImeText(pTextRender, Ime.m_FontCandidate, aNum, Ime);
		aCandidateMetrics[i].m_Text = MeasureImeText(pTextRender, Ime.m_FontCandidate, DrawState.m_vCandidates[i].c_str(), Ime);
		CandidateTextHeight = maximum(CandidateTextHeight, maximum(aCandidateMetrics[i].m_Num.m_VisualHeight, aCandidateMetrics[i].m_Text.m_VisualHeight));
	}

	const auto CandidateNaturalWidthForWindow = [&](int Start, int Count) {
		float CandidateNaturalWidth = 0.0f;
		for(int Offset = 0; Offset < Count; ++Offset)
		{
			const int Index = Start + Offset;
			if(Offset > 0)
				CandidateNaturalWidth += Ime.m_CandidateGap;
			CandidateNaturalWidth += CandidateCellWidth(Ime, aCandidateMetrics[Index], Index == SelectedIndex);
		}
		return CandidateNaturalWidth;
	};
	const auto CandidateStartForCount = [&](int Count) {
		if(Count <= 0 || SelectedIndex < 0)
			return 0;
		return std::clamp(SelectedIndex - Count + 1, 0, maximum(0, CandidateCount - Count));
	};
	const float CandidateFitPanelWidth = PreferredMaxPanelWidth;
	const auto FitCandidates = [&](float FitTrailingWidth, int &CandidateStart, int &CandidateDisplayCount) {
		CandidateDisplayCount = CandidateCount;
		CandidateStart = CandidateStartForCount(CandidateDisplayCount);
		while(CandidateDisplayCount > 1)
		{
			CandidateStart = CandidateStartForCount(CandidateDisplayCount);
			const float NeededWidth = CandidateNaturalWidthForWindow(CandidateStart, CandidateDisplayCount) + FitTrailingWidth + 2.0f * Ime.m_PaddingX;
			if(NeededWidth <= CandidateFitPanelWidth)
				break;
			--CandidateDisplayCount;
		}
		CandidateStart = CandidateStartForCount(CandidateDisplayCount);
	};

	int CandidateStart = 0;
	int CandidateDisplayCount = CandidateCount;
	if(HasCandidates)
	{
		FitCandidates(TrailingWidth, CandidateStart, CandidateDisplayCount);
		if(PageCount <= 1 && CandidateDisplayCount < CandidateCount)
		{
			SetTrailingText(">");
			FitCandidates(TrailingWidth, CandidateStart, CandidateDisplayCount);
		}
	}

	const float CandidateNaturalWidth = HasCandidates ? CandidateNaturalWidthForWindow(CandidateStart, CandidateDisplayCount) + TrailingWidth : 0.0f;
	const float ContentWidth = HasCandidates ? CandidateNaturalWidth : 0.0f;
	const float NeededPanelWidth = ContentWidth + 2.0f * Ime.m_PaddingX;
	const bool SingleLongCandidate = HasCandidates && CandidateDisplayCount <= 1 && NeededPanelWidth > PreferredMaxPanelWidth;
	const float PanelMaxWidth = SingleLongCandidate ? ScreenMaxPanelWidth : PreferredMaxPanelWidth;
	const float PanelWidth = std::clamp(NeededPanelWidth, Ime.m_MinWidth, PanelMaxWidth);
	const float RowHeight = maximum(Ime.m_RowHeight, CandidateTextHeight + 2.0f * Ime.m_TextSafePaddingY);
	const float PanelHeight = 2.0f * Ime.m_PaddingY + RowHeight;

	vec2 Anchor = DrawState.m_AnchorScreen / vec2((float)ScreenWidth, (float)ScreenHeight) * vec2(Width, Height);
	const float PopupGap = 2.2f;
	vec2 Position = vec2(Anchor.x, Anchor.y + PopupGap);
	const float AboveY = Anchor.y - PanelHeight - PopupGap;
	if(Position.y + PanelHeight + Margin > Height && AboveY >= Margin)
		Position.y = AboveY;

	if(Position.x + PanelWidth + Margin > Width)
		Position.x = Width - PanelWidth - Margin;
	Position.x = std::clamp(Position.x, Margin, maximum(Margin, Width - PanelWidth - Margin));
	Position.y = std::clamp(Position.y, Margin, maximum(Margin, Height - PanelHeight - Margin));

	const auto PixelAlign = [](float Value, float UiToPixel) {
		return UiToPixel > 0.0f ? std::round(Value * UiToPixel) / UiToPixel : Value;
	};
	Position.x = PixelAlign(Position.x, ScreenWidth / Width);
	Position.y = PixelAlign(Position.y, ScreenHeight / Height);

	CUiV2AnimationRuntime &AnimRuntime = pGameClient->UiRuntimeV2()->AnimRuntime();
	CUiV2Tree &Tree = pGameClient->UiRuntimeV2()->Tree();
	const uint64_t PopupKey = BuildUiAnimNodeKey(str_quickhash("qm_ime_popup"), m_PresenceGeneration);
	if(TargetVisible && !m_WasVisible && g_Config.m_QmUiMotionLevel != 0)
	{
		AnimRuntime.SetValue(PopupKey, EUiAnimProperty::POS_Y, 1.4f);
	}

	const float AlphaDuration = TargetVisible ? POPUP_IN_DURATION : POPUP_OUT_DURATION;
	SUiAnimTransition PresenceTransition;
	PresenceTransition.m_DurationSec = AlphaDuration;
	PresenceTransition.m_Easing = EEasing::EASE_OUT;
	PresenceTransition = qm_motion::ApplyMotionLevel(PresenceTransition, g_Config.m_QmUiMotionLevel);
	const SUiPresenceResult Presence = Tree.ResolvePresence(AnimRuntime, PopupKey, TargetVisible, PresenceTransition);
	if(!Presence.m_Render)
	{
		m_WasVisible = false;
		pTextRender->SetRenderFlags(OldRenderFlags);
		pGraphics->MapScreen(OldScreenX0, OldScreenY0, OldScreenX1, OldScreenY1);
		return;
	}
	const float Alpha = std::clamp(Presence.m_Alpha, 0.0f, 1.0f);
	const float OffsetY = ResolveMotionValue(AnimRuntime, PopupKey, EUiAnimProperty::POS_Y, TargetVisible ? 0.0f : -0.8f, AlphaDuration);
	m_WasVisible = true;

	CUIRect Panel = {Position.x, Position.y + OffsetY, PanelWidth, PanelHeight};
	CUIRect ShadowNear = Panel;
	ShadowNear.x += Ime.m_ShadowX;
	ShadowNear.y += Ime.m_ShadowY * 0.65f;
	ShadowNear.Draw(WithAlpha(Ime.m_PanelShadow, Alpha * 0.46f), IGraphics::CORNER_ALL, Ime.m_Radius);
	CUIRect ShadowFar = Panel;
	ShadowFar.y += Ime.m_ShadowY * 1.7f;
	ShadowFar.Draw(WithAlpha(Ime.m_PanelShadow, Alpha * 0.28f), IGraphics::CORNER_ALL, Ime.m_Radius);

	Panel.Draw(WithAlpha(Ime.m_PanelBorder, Alpha), IGraphics::CORNER_ALL, Ime.m_Radius);
	CUIRect PanelInner;
	Panel.Margin(Ime.m_BorderInset, &PanelInner);
	PanelInner.Draw(WithAlpha(Ime.m_PanelBg, Alpha), IGraphics::CORNER_ALL, maximum(0.0f, Ime.m_Radius - Ime.m_BorderInset));

	CUIRect TopGlow = PanelInner;
	TopGlow.h = 0.45f;
	TopGlow.x += Ime.m_Radius * 0.35f;
	TopGlow.w -= Ime.m_Radius * 0.70f;
	TopGlow.Draw(WithAlpha(ColorRGBA(1.0f, 1.0f, 1.0f, 0.11f), Alpha), IGraphics::CORNER_T, 0.0f);

	const ColorRGBA OldTextColor = pTextRender->GetTextColor();
	const ColorRGBA OldOutlineColor = pTextRender->GetTextOutlineColor();
	pTextRender->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.0f);

	CUIRect Content;
	Panel.Margin(vec2(Ime.m_PaddingX, Ime.m_PaddingY), &Content);

	if(HasCandidates)
	{
		CUIRect CandidateRow;
		Content.HSplitTop(RowHeight, &CandidateRow, &Content);
		CUIRect Candidates = CandidateRow;
		CUIRect More = {};
		if(TrailingWidth > 0.0f)
			CandidateRow.VSplitRight(TrailingWidth, &Candidates, &More);

		std::array<SImeCandidateCell, MAX_VISIBLE_CANDIDATES> aCells;
		int CellCount = 0;
		float CursorX = Candidates.x;
		const float Right = Candidates.x + Candidates.w;
		for(int Offset = 0; Offset < CandidateDisplayCount; ++Offset)
		{
			const int CandidateIndex = CandidateStart + Offset;
			if(Offset > 0)
				CursorX += Ime.m_CandidateGap;
			if(CursorX >= Right)
				break;

			const bool Selected = CandidateIndex == SelectedIndex;
			const float CellWidth = CandidateCellWidth(Ime, aCandidateMetrics[CandidateIndex], Selected);
			if(CursorX + CellWidth > Right && Offset > 0)
				break;

			SImeCandidateCell &Cell = aCells[CellCount++];
			Cell.m_Index = CandidateIndex;
			Cell.m_Rect = {CursorX, CandidateRow.y, CellWidth, CandidateRow.h};
			CursorX += CellWidth;
		}

		for(int CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			if(aCells[CellIndex].m_Index != SelectedIndex)
				continue;
			CUIRect SelectedRect = aCells[CellIndex].m_Rect;
			SelectedRect.y += 0.75f;
			SelectedRect.h -= 1.5f;
			const uint64_t SelectedKey = BuildUiAnimNodeKey(str_quickhash("qm_ime_popup_selected"), 1);
			const CUIRect DrawRect = ResolveMotionRect(AnimRuntime, SelectedKey, SelectedRect, SELECTED_DURATION);
			DrawRect.Draw(WithAlpha(Ime.m_SelectedBg, Alpha), IGraphics::CORNER_ALL, maximum(1.0f, DrawRect.h * 0.5f));
			break;
		}

		for(int CellIndex = 0; CellIndex < CellCount; ++CellIndex)
		{
			const SImeCandidateCell &Cell = aCells[CellIndex];
			const bool Selected = Cell.m_Index == SelectedIndex;
			const float PaddingX = Selected ? Ime.m_SelectedPaddingX : Ime.m_CandidatePaddingX;
			const SImeCandidateMetrics &Metrics = aCandidateMetrics[Cell.m_Index];
			char aNum[4];
			str_format(aNum, sizeof(aNum), "%d", (Cell.m_Index + 1) % 10);
			const float NumX = Cell.m_Rect.x + PaddingX;
			const float TextX = NumX + Metrics.m_Num.m_Width + Ime.m_CandidateNumPaddingX;
			DrawImeText(pTextRender, NumX, CandidateRow.y, CandidateRow.h, Ime.m_FontCandidate, aNum, Metrics.m_Num, Selected ? Ime.m_TextSelected : Ime.m_TextMuted, Alpha);
			DrawImeText(pTextRender, TextX, CandidateRow.y, CandidateRow.h, Ime.m_FontCandidate, DrawState.m_vCandidates[Cell.m_Index].c_str(), Metrics.m_Text, Selected ? Ime.m_TextSelected : Ime.m_Text, Alpha);
		}

		if(TrailingWidth > 0.0f)
		{
			CUIRect Divider = More;
			Divider.x += 0.4f;
			Divider.y += 2.0f;
			Divider.w = 0.35f;
			Divider.h = maximum(0.0f, Divider.h - 4.0f);
			Divider.Draw(WithAlpha(Ime.m_PanelBorder, Alpha * 1.25f), IGraphics::CORNER_ALL, 0.25f);

			DrawImeText(pTextRender,
				More.x + (More.w - PageTextMetrics.m_Width) * 0.5f,
				More.y,
				More.h,
				Ime.m_FontComposition,
				aPageText,
				PageTextMetrics,
				Ime.m_TextMuted,
				Alpha);
		}
	}

	pTextRender->TextColor(OldTextColor);
	pTextRender->TextOutlineColor(OldOutlineColor);
	pTextRender->SetRenderFlags(OldRenderFlags);
	pGraphics->MapScreen(OldScreenX0, OldScreenY0, OldScreenX1, OldScreenY1);
}
