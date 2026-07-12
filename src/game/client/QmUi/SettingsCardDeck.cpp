#include "SettingsCardDeck.h"

#include "QmAnimResolve.h"
#include "UiContext.h"

#include <base/system.h>

#include <game/client/ui_scrollregion.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	struct SPreparedSettingsCard
	{
		const SSettingsCardDefinition *m_pDefinition = nullptr;
		int m_StateIndex = -1;
		int m_Column = 0;
		SSettingsCardFrame m_Frame;
	};

	bool PointInRect(const CUIRect &Rect, float X, float Y)
	{
		return X >= Rect.x && X <= Rect.x + Rect.w && Y >= Rect.y && Y <= Rect.y + Rect.h;
	}

	void OffsetRectY(CUIRect &Rect, float OffsetY)
	{
		Rect.y += OffsetY;
	}

	uint64_t SettingsCardEntryNodeKey(const char *pTab, const char *pStableId, uint64_t DisplayCycle)
	{
		const uint64_t TabKey = str_quickhash(pTab != nullptr ? pTab : "");
		const uint64_t CycleKey = BuildUiAnimNodeKey(str_quickhash("settings-card-entry"), DisplayCycle);
		return BuildUiAnimNodeKey(BuildUiAnimNodeKey(CycleKey, TabKey), str_quickhash(pStableId != nullptr ? pStableId : ""));
	}

	uint64_t SettingsCardReflowNodeKey(const char *pTab, const char *pStableId)
	{
		const uint64_t TabKey = str_quickhash(pTab != nullptr ? pTab : "");
		return BuildUiAnimNodeKey(BuildUiAnimNodeKey(str_quickhash("settings-card-reflow"), TabKey), str_quickhash(pStableId != nullptr ? pStableId : ""));
	}
}

void CSettingsCardDeck::PrepareDefinitions(const std::vector<SSettingsCardDefinition> &vCards, const qm_card_order::CModel &Model)
{
	m_vDefinitionsByState.assign(Model.Count(), nullptr);
	for(const SSettingsCardDefinition &Definition : vCards)
	{
		if(Definition.m_Spec.m_pStableId == nullptr)
			continue;
		const int StateIndex = Model.StateIndexForStableId(Definition.m_Spec.m_pStableId);
		if(StateIndex >= 0)
			m_vDefinitionsByState[StateIndex] = &Definition;
	}
	m_vRuntimeStates.resize(Model.Count());
	m_vContentHeights.assign(Model.Count(), -1.0f);
}

void CSettingsCardDeck::RequestReveal(const char *pStableId)
{
	m_PendingRevealStableId = pStableId != nullptr ? pStableId : "";
}

void CSettingsCardDeck::BeginDisplayCycle(uint64_t DisplayCycle)
{
	if(m_DisplayCycle != DisplayCycle)
		m_Drag.Reset();
	m_DisplayCycle = DisplayCycle;
}

SSettingsCardDeckResult CSettingsCardDeck::Render(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions)
{
	SSettingsCardDeckResult Result;
	if(pTab == nullptr)
		return Result;

	PrepareDefinitions(vCards, Model);
	auto RebuildActiveStateIndices = [&]() {
		m_vActiveStateIndices.clear();
		m_vActiveStateIndices.reserve(vCards.size());
		for(int StateIndex = 0; StateIndex < (int)m_vDefinitionsByState.size(); ++StateIndex)
		{
			const SSettingsCardDefinition *pDefinition = m_vDefinitionsByState[StateIndex];
			if(pDefinition != nullptr && (!pDefinition->m_IsVisible || pDefinition->m_IsVisible()))
				m_vActiveStateIndices.push_back(StateIndex);
		}
	};

	CUIRect ScrollViewport = Layout.m_ScrollViewport;
	vec2 ScrollOffset(0.0f, 0.0f);
	if(pScrollRegion != nullptr)
		pScrollRegion->Begin(&ScrollViewport, &ScrollOffset, Input.m_pScrollParams);

	SSettingsPageLayoutFrame DrawLayout = ResolveSettingsPageLayoutForScrollViewport(Layout, ScrollViewport, Ctx.m_UiScale);
	OffsetRectY(DrawLayout.m_ContentViewport, ScrollOffset.y);
	OffsetRectY(DrawLayout.m_aColumns[0], ScrollOffset.y);
	OffsetRectY(DrawLayout.m_aColumns[1], ScrollOffset.y);

	auto BuildPreparedCards = [&](const std::array<std::vector<int>, 3> &aDisplayColumns) {
		std::vector<SPreparedSettingsCard> vPrepared;
		vPrepared.reserve(m_vActiveStateIndices.size());
		auto AppendCard = [&](int StateIndex, int Column, CUIRect ColumnRect, float &CursorY) {
			if(StateIndex < 0 || StateIndex >= (int)m_vDefinitionsByState.size())
				return;
			const SSettingsCardDefinition *pDefinition = m_vDefinitionsByState[StateIndex];
			if(pDefinition == nullptr)
				return;
			CUIRect Slot{ColumnRect.x, CursorY, ColumnRect.w, 0.0f};
			float &ContentHeight = m_vContentHeights[StateIndex];
			const bool Collapsed = pDefinition->m_IsCollapsed && pDefinition->m_IsCollapsed();
			if(Collapsed)
				ContentHeight = 0.0f;
			else if(SettingsCardDeckNeedsContentMeasure(Collapsed, pDefinition->m_MeasureEachFrame, ContentHeight))
			{
				const float ContentWidth = std::max(0.0f, Slot.w - 28.0f * (Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f));
				ContentHeight = pDefinition->m_Measure ? std::max(0.0f, pDefinition->m_Measure(ContentWidth)) : 0.0f;
			}
			const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, pDefinition->m_Spec, ContentHeight, Ctx.m_UiScale);
			vPrepared.push_back({pDefinition, StateIndex, Column, Frame});
			CursorY = Frame.m_Rect.y + Frame.m_Rect.h + DrawLayout.m_CardGap;
		};
		auto AppendColumn = [&](const std::vector<int> &vStateIndices, int Column, CUIRect ColumnRect, float &CursorY) {
			for(const int StateIndex : vStateIndices)
				AppendCard(StateIndex, Column, ColumnRect, CursorY);
		};

		if(DrawLayout.m_TwoColumns && !aDisplayColumns[0].empty())
		{
			const size_t NumLayers = std::max({aDisplayColumns[0].size(), aDisplayColumns[1].size(), aDisplayColumns[2].size()});
			float CursorY = DrawLayout.m_ContentViewport.y;
			for(size_t Layer = 0; Layer < NumLayers; ++Layer)
			{
				if(Layer < aDisplayColumns[0].size())
					AppendCard(aDisplayColumns[0][Layer], 0, DrawLayout.m_ContentViewport, CursorY);

				float LeftY = std::max(DrawLayout.m_aColumns[0].y, CursorY);
				float RightY = std::max(DrawLayout.m_aColumns[1].y, CursorY);
				if(Layer < aDisplayColumns[1].size())
					AppendCard(aDisplayColumns[1][Layer], 1, DrawLayout.m_aColumns[0], LeftY);
				if(Layer < aDisplayColumns[2].size())
					AppendCard(aDisplayColumns[2][Layer], 2, DrawLayout.m_aColumns[1], RightY);
				CursorY = std::max(LeftY, RightY);
			}
		}
		else if(DrawLayout.m_TwoColumns)
		{
			float LeftY = DrawLayout.m_aColumns[0].y;
			float RightY = DrawLayout.m_aColumns[1].y;
			AppendColumn(aDisplayColumns[1], 1, DrawLayout.m_aColumns[0], LeftY);
			AppendColumn(aDisplayColumns[2], 2, DrawLayout.m_aColumns[1], RightY);
		}
		else
		{
			float CursorY = DrawLayout.m_ContentViewport.y;
			for(const int Column : {0, 1, 2})
				AppendColumn(aDisplayColumns[Column], 0, DrawLayout.m_ContentViewport, CursorY);
		}
		return vPrepared;
	};

	RebuildActiveStateIndices();
	std::array<std::vector<int>, 3> aColumns = m_ProjectionCache.Resolve(Model, pTab, m_vActiveStateIndices);
	std::vector<SPreparedSettingsCard> vPrepared = BuildPreparedCards(aColumns);

	// 先用当前 active snapshot 的几何处理控制器输入，再为最终 active snapshot 重新计算布局。
	const std::vector<int> PreviousActiveStateIndices = m_vActiveStateIndices;
	for(const SPreparedSettingsCard &Card : vPrepared)
	{
		const bool ControllerVisible = pScrollRegion == nullptr || !pScrollRegion->RectClipped(Card.m_Frame.m_Rect) || Card.m_pDefinition->m_RenderWhenClipped;
		if(ControllerVisible && Card.m_pDefinition->m_VisibilityController && Card.m_pDefinition->m_PreLayoutInput)
			Card.m_pDefinition->m_PreLayoutInput(Card.m_Frame.m_ContentRect);
	}
	RebuildActiveStateIndices();
	if(m_vActiveStateIndices != PreviousActiveStateIndices)
	{
		std::fill(m_vContentHeights.begin(), m_vContentHeights.end(), -1.0f);
		aColumns = m_ProjectionCache.Resolve(Model, pTab, m_vActiveStateIndices);
		vPrepared = BuildPreparedCards(aColumns);
	}
	if(m_Drag.Active() && std::find(m_vActiveStateIndices.begin(), m_vActiveStateIndices.end(), m_Drag.m_StateIndex) == m_vActiveStateIndices.end())
		m_Drag.Reset();
	if(m_Drag.Active() && !Input.m_MouseDown && !Input.m_MouseReleased)
		m_Drag.Reset();
	if(!m_Drag.Active() && DrawLayout.m_TwoColumns && (Input.m_CtrlPressed || Input.m_AllowHeaderDrag) && Input.m_MousePressed)
	{
		for(const SPreparedSettingsCard &Card : vPrepared)
		{
			const bool InHeader = PointInRect(Card.m_Frame.m_HeaderRect, Input.m_MouseX, Input.m_MouseY);
			const bool InHeaderAction = Card.m_pDefinition->m_HeaderAction && PointInRect(Card.m_Frame.m_HandleRect, Input.m_MouseX, Input.m_MouseY);
			if((Card.m_Column == 1 || Card.m_Column == 2) && InHeader && !InHeaderAction)
			{
				m_Drag.m_StateIndex = Card.m_StateIndex;
				m_Drag.m_SourceColumn = Card.m_Column;
				m_Drag.m_TargetColumn = Card.m_Column;
				m_Drag.m_GrabOffsetX = Input.m_MouseX - Card.m_Frame.m_Rect.x;
				m_Drag.m_GrabOffsetY = Input.m_MouseY - Card.m_Frame.m_Rect.y;
				break;
			}
		}
	}

	if(m_Drag.Active() && DrawLayout.m_TwoColumns)
	{
		const bool MouseInScrollViewport = PointInRect(ScrollViewport, Input.m_MouseX, Input.m_MouseY);
		if(MouseInScrollViewport && Input.m_MouseX >= DrawLayout.m_aColumns[0].x && Input.m_MouseX <= DrawLayout.m_aColumns[0].x + DrawLayout.m_aColumns[0].w)
			m_Drag.m_TargetColumn = 1;
		else if(MouseInScrollViewport && Input.m_MouseX >= DrawLayout.m_aColumns[1].x && Input.m_MouseX <= DrawLayout.m_aColumns[1].x + DrawLayout.m_aColumns[1].w)
			m_Drag.m_TargetColumn = 2;
		std::vector<SSettingsCardDeckItemGeometry> vGeometry;
		vGeometry.reserve(vPrepared.size());
		for(const SPreparedSettingsCard &Card : vPrepared)
			vGeometry.push_back({Card.m_StateIndex, Card.m_Column, Card.m_Frame.m_Rect});
		m_Drag.m_TargetOrder = ResolveSettingsCardDeckDropOrder(Input.m_MouseY, m_Drag.m_TargetColumn, vGeometry, m_Drag.m_StateIndex);
		std::array<std::vector<int>, 3> aDragColumns = aColumns;
		ApplySettingsCardDeckDragPlacement(aDragColumns, m_Drag.m_StateIndex, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder);
		vPrepared = BuildPreparedCards(aDragColumns);

		if(pScrollRegion != nullptr && MouseInScrollViewport)
		{
			Result.m_AutoScrollDelta = SettingsCardDeckAutoScrollDelta(Input.m_MouseY, ScrollViewport, Ctx.m_UiScale);
			if(Result.m_AutoScrollDelta != 0.0f)
				pScrollRegion->ScrollRelativeDirect(Result.m_AutoScrollDelta * std::max(0.0f, Input.m_FrameDt));
		}

		if(Input.m_MouseReleased)
		{
			const char *pStableId = m_Drag.m_StateIndex >= 0 && m_Drag.m_StateIndex < Model.Count() ? Model.Entry(m_Drag.m_StateIndex).m_pStableId : nullptr;
			Result.m_OrderChanged = CommitSettingsCardDeckDrop(Model, pTab, pStableId, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder, &m_vActiveStateIndices);
			if(Result.m_OrderChanged && m_Drag.m_StateIndex >= 0 && m_Drag.m_StateIndex < (int)m_vRuntimeStates.size())
			{
				SRuntimeState &Runtime = m_vRuntimeStates[m_Drag.m_StateIndex];
				Runtime.m_DropFeedbackRemaining = Motion.m_KeepDropFeedback ? Motion.m_DropFeedbackDuration : 0.0f;
				Result.m_DropFeedbackConsumed = Runtime.m_DropFeedbackRemaining > 0.0f;
				if(Motion.m_ReflowDuration <= 0.0f && Motion.m_KeepReflowCompleteFeedback)
				{
					Runtime.m_ReflowCompleteFeedbackRemaining = Motion.m_ReflowCompleteFeedbackDuration;
					Result.m_ReflowCompleteFeedbackConsumed = Runtime.m_ReflowCompleteFeedbackRemaining > 0.0f;
				}
			}
			m_Drag.Reset();
		}
	}
	else if(m_Drag.Active() && Input.m_MouseReleased)
	{
		m_Drag.Reset();
	}

	Result.m_vFrames.reserve(vPrepared.size());
	for(const SPreparedSettingsCard &Card : vPrepared)
	{
		SRuntimeState &Runtime = m_vRuntimeStates[Card.m_StateIndex];
		const char *pStableId = Card.m_pDefinition->m_Spec.m_pStableId;
		SSettingsCardVisualState State;
		if(Ctx.m_pAnim != nullptr)
		{
			const uint64_t EntryKey = SettingsCardEntryNodeKey(pTab, pStableId, m_DisplayCycle);
			if(Runtime.m_EntryDisplayCycle != m_DisplayCycle)
			{
				Runtime.m_EntryDisplayCycle = m_DisplayCycle;
				Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::POS_Y, Motion.m_EntryDistance);
				Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::ALPHA, 0.0f);
			}
			if(Motion.m_EntryDuration > 0.0f)
			{
				State.m_DrawOffsetY += ResolveUiAnimValue(*Ctx.m_pAnim, EntryKey, EUiAnimProperty::POS_Y, 0.0f, Motion.m_EntryDuration, EEasing::EASE_OUT);
				State.m_DrawAlpha = ResolveUiAnimValue(*Ctx.m_pAnim, EntryKey, EUiAnimProperty::ALPHA, 1.0f, Motion.m_EntryDuration, EEasing::EASE_OUT);
			}
			else
			{
				Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::POS_Y, 0.0f);
				Ctx.m_pAnim->SetValue(EntryKey, EUiAnimProperty::ALPHA, 1.0f);
			}

			const uint64_t ReflowKey = SettingsCardReflowNodeKey(pTab, pStableId);
			const float ReflowTargetY = Card.m_Frame.m_Rect.y - ScrollOffset.y;
			const bool TargetChanged = Runtime.m_ReflowInitialized && std::abs(Runtime.m_LastReflowTargetY - ReflowTargetY) > 0.001f;
			if(!Runtime.m_ReflowInitialized)
			{
				Runtime.m_ReflowInitialized = true;
				Ctx.m_pAnim->SetValue(ReflowKey, EUiAnimProperty::POS_Y, ReflowTargetY);
			}
			if(Motion.m_ReflowDuration > 0.0f)
			{
				const float ReflowY = ResolveUiAnimValue(*Ctx.m_pAnim, ReflowKey, EUiAnimProperty::POS_Y, ReflowTargetY, Motion.m_ReflowDuration, EEasing::EASE_OUT);
				State.m_DrawOffsetY += ReflowY - ReflowTargetY;
				const bool ReflowActive = Ctx.m_pAnim->HasActiveAnimation(ReflowKey, EUiAnimProperty::POS_Y);
				if(Runtime.m_ReflowWasActive && !ReflowActive && Motion.m_KeepReflowCompleteFeedback)
					Runtime.m_ReflowCompleteFeedbackRemaining = Motion.m_ReflowCompleteFeedbackDuration;
				Runtime.m_ReflowWasActive = ReflowActive;
			}
			else
			{
				Ctx.m_pAnim->SetValue(ReflowKey, EUiAnimProperty::POS_Y, ReflowTargetY);
				if(TargetChanged && Motion.m_KeepReflowCompleteFeedback)
					Runtime.m_ReflowCompleteFeedbackRemaining = Motion.m_ReflowCompleteFeedbackDuration;
				Runtime.m_ReflowWasActive = false;
			}
			Runtime.m_LastReflowTargetY = ReflowTargetY;
		}
		State.m_Dragged = m_Drag.Active() && Card.m_StateIndex == m_Drag.m_StateIndex;
		if(State.m_Dragged)
		{
			State.m_DrawOffsetX = Input.m_MouseX - m_Drag.m_GrabOffsetX - Card.m_Frame.m_Rect.x;
			State.m_DrawOffsetY = Input.m_MouseY - m_Drag.m_GrabOffsetY - Card.m_Frame.m_Rect.y;
		}
		State.m_DropFeedback = Runtime.m_DropFeedbackRemaining > 0.0f;
		State.m_ReflowCompleteFeedback = Runtime.m_ReflowCompleteFeedbackRemaining > 0.0f;
		Result.m_DropFeedbackConsumed = Result.m_DropFeedbackConsumed || State.m_DropFeedback;
		Result.m_ReflowCompleteFeedbackConsumed = Result.m_ReflowCompleteFeedbackConsumed || State.m_ReflowCompleteFeedback;
		const bool Reveal = !m_PendingRevealStableId.empty() && str_comp(pStableId, m_PendingRevealStableId.c_str()) == 0;
		const bool Visible = pScrollRegion == nullptr || pScrollRegion->AddRect(Card.m_Frame.m_Rect, Reveal);
		if(Reveal)
		{
			Result.m_pRevealedStableId = Model.Entry(Card.m_StateIndex).m_pStableId;
			m_PendingRevealStableId.clear();
		}
		if(Visible || Card.m_pDefinition->m_RenderWhenClipped)
		{
			const bool Collapsed = Card.m_pDefinition->m_IsCollapsed && Card.m_pDefinition->m_IsCollapsed();
			State.m_Collapsed = Collapsed;
			SettingsCard(Ctx, Card.m_Frame, Card.m_pDefinition->m_Spec, State, VisualOptions,
				SettingsCardDeckRendersContent(Collapsed) ? Card.m_pDefinition->m_Render : FSettingsCardRender{}, Card.m_pDefinition->m_HeaderAction,
				SettingsCardDeckRendersContent(Collapsed) ? Card.m_pDefinition->m_RenderMeasured : FSettingsCardRenderMeasured{});
		}
		Result.m_vFrames.push_back(Card.m_Frame);
	}

	if(pScrollRegion != nullptr)
		pScrollRegion->End();
	for(SRuntimeState &Runtime : m_vRuntimeStates)
	{
		Runtime.m_DropFeedbackRemaining = std::max(0.0f, Runtime.m_DropFeedbackRemaining - std::max(0.0f, Input.m_FrameDt));
		Runtime.m_ReflowCompleteFeedbackRemaining = std::max(0.0f, Runtime.m_ReflowCompleteFeedbackRemaining - std::max(0.0f, Input.m_FrameDt));
	}
	return Result;
}

namespace settings_card_deck
{
	void CDeck::Load(const char *pDeckId, char *pGlobalOrder, int GlobalOrderSize)
	{
		m_DeckId = pDeckId != nullptr ? pDeckId : "";
		m_pGlobalOrder = pGlobalOrder;
		m_GlobalOrderSize = GlobalOrderSize;
		m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
		RebuildProjection();
	}

	bool CDeck::CommitDrop(const char *pStableId, int Column, int Order)
	{
		if(m_pGlobalOrder == nullptr || m_GlobalOrderSize <= 0 || !m_Logic.Move(pStableId, Column, Order))
			return false;
		std::vector<char> vMergedGlobalOrder(m_GlobalOrderSize);
		if(!m_Logic.SerializeMerged(m_pGlobalOrder, vMergedGlobalOrder.data(), (int)vMergedGlobalOrder.size()))
		{
			m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
			RebuildProjection();
			return false;
		}
		str_copy(m_pGlobalOrder, vMergedGlobalOrder.data(), m_GlobalOrderSize);
		m_Logic.Load(m_DeckId.c_str(), m_pGlobalOrder);
		RebuildProjection();
		return true;
	}

	int CDeck::ColumnForStableId(const char *pStableId) const
	{
		return m_Logic.ColumnForStableId(pStableId);
	}

	void CDeck::RebuildProjection()
	{
		m_vOrderedStableIds.clear();
		for(const int Column : {1, 2, 0})
		{
			const std::vector<std::string> vColumnIds = m_Logic.StableIdOrder(Column);
			m_vOrderedStableIds.insert(m_vOrderedStableIds.end(), vColumnIds.begin(), vColumnIds.end());
		}
	}
} // namespace settings_card_deck
