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
	bool PointInRect(const CUIRect &Rect, float X, float Y)
	{
		return X >= Rect.x && X <= Rect.x + Rect.w && Y >= Rect.y && Y <= Rect.y + Rect.h;
	}

	void OffsetRectY(CUIRect &Rect, float OffsetY)
	{
		Rect.y += OffsetY;
	}

	uint64_t SettingsCardEntryNodeKey(const char *pTab)
	{
		const uint64_t TabKey = str_quickhash(pTab != nullptr ? pTab : "");
		return BuildUiAnimNodeKey(str_quickhash("settings-card-deck-entry"), TabKey);
	}

	uint64_t SettingsCardReflowNodeKey(const char *pTab, const char *pStableId)
	{
		const uint64_t TabKey = str_quickhash(pTab != nullptr ? pTab : "");
		return BuildUiAnimNodeKey(BuildUiAnimNodeKey(str_quickhash("settings-card-reflow"), TabKey), str_quickhash(pStableId != nullptr ? pStableId : ""));
	}

	uint64_t SettingsCardHeightNodeKey(const char *pTab, const char *pStableId)
	{
		const uint64_t TabKey = str_quickhash(pTab != nullptr ? pTab : "");
		return BuildUiAnimNodeKey(BuildUiAnimNodeKey(str_quickhash("settings-card-height"), TabKey), str_quickhash(pStableId != nullptr ? pStableId : ""));
	}
}

void CSettingsCardDeck::PrepareDefinitions(const std::vector<SSettingsCardDefinition> &vCards, const qm_card_order::CModel &Model)
{
	if(m_vDefinitionsByState.size() != (size_t)Model.Count())
	{
		m_vDefinitionsByState.assign(Model.Count(), nullptr);
		m_vBoundDefinitionStateIndices.clear();
	}
	else
	{
		for(const int StateIndex : m_vBoundDefinitionStateIndices)
			m_vDefinitionsByState[StateIndex] = nullptr;
		m_vBoundDefinitionStateIndices.clear();
	}
	m_vBoundDefinitionStateIndices.reserve(vCards.size());
	for(const SSettingsCardDefinition &Definition : vCards)
	{
		if(Definition.m_Spec.m_pStableId == nullptr)
			continue;
		const int StateIndex = Model.StateIndexForStableId(Definition.m_Spec.m_pStableId);
		if(StateIndex >= 0)
		{
			m_vDefinitionsByState[StateIndex] = &Definition;
			m_vBoundDefinitionStateIndices.push_back(StateIndex);
		}
	}
	m_vRuntimeStates.resize(Model.Count());
	m_vContentHeights.resize(Model.Count(), -1.0f);
	m_vContentWidths.resize(Model.Count(), -1.0f);
	m_vMeasureRevisions.resize(Model.Count(), UINT64_MAX);
}

void CSettingsCardDeck::RequestReveal(const char *pStableId)
{
	m_PendingRevealStableId = pStableId != nullptr ? pStableId : "";
}

void CSettingsCardDeck::BeginDisplayCycle(uint64_t DisplayCycle, bool AnimateEntry)
{
	if(m_DisplayCycle != DisplayCycle)
	{
		m_Drag.Reset();
		m_SuppressHoverFeedbackOnce = true;
		m_HasScrollOffset = false;
		m_vLastRenderedActiveStateIndices.clear();
		m_EntryDisplayCycle = UINT64_MAX;
		m_EntryWasActive = false;
		for(SRuntimeState &Runtime : m_vRuntimeStates)
		{
			Runtime.m_ReflowInitialized = false;
			Runtime.m_ReflowWasActive = false;
			Runtime.m_ContentHeightInitialized = false;
			Runtime.m_ContentHeightWasActive = false;
			Runtime.m_CollapsedInitialized = false;
		}
	}
	m_DisplayCycle = DisplayCycle;
	m_AnimateEntry = AnimateEntry;
}

SSettingsCardDeckResult CSettingsCardDeck::Render(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions)
{
	return RenderInternal(Ctx, Layout, pTab, vCards, Model, pScrollRegion, Input, Motion, VisualOptions, true);
}

SSettingsCardDeckResult CSettingsCardDeck::RenderInternal(const IUiContext &Ctx, const SSettingsPageLayoutFrame &Layout, const char *pTab, const std::vector<SSettingsCardDefinition> &vCards, qm_card_order::CModel &Model, CScrollRegion *pScrollRegion, const SSettingsCardDeckInput &Input, const SCardMotionSpec &Motion, const SSettingsCardDeckVisualOptions &VisualOptions, bool PersistentDefinitions)
{
	SSettingsCardDeckResult Result;
	if(pTab == nullptr)
		return Result;
	if(m_LastRenderedTab != pTab)
	{
		m_LastRenderedTab = pTab;
		m_SuppressHoverFeedbackOnce = true;
		m_vLastRenderedActiveStateIndices.clear();
		m_EntryDisplayCycle = UINT64_MAX;
		m_EntryWasActive = false;
		for(SRuntimeState &Runtime : m_vRuntimeStates)
		{
			Runtime.m_ReflowInitialized = false;
			Runtime.m_ReflowWasActive = false;
			Runtime.m_ContentHeightInitialized = false;
			Runtime.m_ContentHeightWasActive = false;
			Runtime.m_CollapsedInitialized = false;
		}
	}

	bool StableIdsChanged = m_vPreparedStableIds.size() != vCards.size();
	if(!StableIdsChanged)
	{
		for(size_t i = 0; i < vCards.size(); ++i)
		{
			if(m_vPreparedStableIds[i] != vCards[i].m_Spec.m_pStableId)
			{
				StableIdsChanged = true;
				break;
			}
		}
	}
	if(!PersistentDefinitions || m_CachedDefinitionsDirty || m_PreparedDefinitionModelCount != Model.Count() || m_pPreparedDefinitionData != vCards.data() || m_PreparedDefinitionCount != vCards.size() || m_PreparedDefinitionTab != pTab || StableIdsChanged)
	{
		PrepareDefinitions(vCards, Model);
		m_CachedDefinitionsDirty = false;
		m_PreparedDefinitionModelCount = Model.Count();
		m_pPreparedDefinitionData = vCards.data();
		m_PreparedDefinitionCount = vCards.size();
		m_PreparedDefinitionTab = pTab;
		m_vPreparedStableIds.resize(vCards.size());
		for(size_t i = 0; i < vCards.size(); ++i)
			m_vPreparedStableIds[i] = vCards[i].m_Spec.m_pStableId;
	}
	auto RebuildActiveStateIndices = [&]() {
		m_vActiveStateIndices.clear();
		m_vActiveStateIndices.reserve(vCards.size());
		for(const int StateIndex : m_vBoundDefinitionStateIndices)
		{
			const SSettingsCardDefinition *pDefinition = m_vDefinitionsByState[StateIndex];
			if(pDefinition != nullptr && (!pDefinition->m_IsVisible || pDefinition->m_IsVisible()))
				m_vActiveStateIndices.push_back(StateIndex);
		}
	};

	// 有真实滚动容器时由 Begin 扣除固定槽位；预热阶段没有容器，直接使用壳层提供的有效 viewport。
	CUIRect ScrollViewport = pScrollRegion != nullptr ? Layout.m_UnreservedScrollViewport : Layout.m_ScrollViewport;
	vec2 ScrollOffset(0.0f, 0.0f);
	if(pScrollRegion != nullptr)
		pScrollRegion->Begin(&ScrollViewport, &ScrollOffset, Input.m_pScrollParams);
	if(std::abs(m_LastViewportHeight - ScrollViewport.h) > 0.01f)
	{
		m_LastViewportHeight = ScrollViewport.h;
		std::fill(m_vContentHeights.begin(), m_vContentHeights.end(), -1.0f);
	}
	bool ScrollMovedThisFrame = false;
	if(pScrollRegion != nullptr)
	{
		ScrollMovedThisFrame = SettingsCardDeckScrollMoved(m_HasScrollOffset, m_LastScrollOffsetY, ScrollOffset.y);
		m_LastScrollOffsetY = ScrollOffset.y;
		m_HasScrollOffset = true;
	}

	SSettingsPageLayoutFrame DrawLayout = ResolveSettingsPageLayoutForScrollViewport(Layout, ScrollViewport, Ctx.m_UiScale);
	OffsetRectY(DrawLayout.m_ContentViewport, ScrollOffset.y);
	OffsetRectY(DrawLayout.m_aColumns[0], ScrollOffset.y);
	OffsetRectY(DrawLayout.m_aColumns[1], ScrollOffset.y);

	bool MeasuredGeometryChanged = false;
	bool ContentHeightTargetChanged = false;
	bool ContentHeightAnimationActive = false;
	auto BuildPreparedCards = [&](const std::array<std::vector<int>, 3> &aDisplayColumns) {
		m_vPreparedCards.clear();
		m_vPreparedCards.reserve(m_vActiveStateIndices.size());
		auto AppendCard = [&](int StateIndex, int Column, CUIRect ColumnRect, CSettingsCardColumnFramePlan &ColumnPlan) {
			if(StateIndex < 0 || StateIndex >= (int)m_vDefinitionsByState.size())
				return;
			const SSettingsCardDefinition *pDefinition = m_vDefinitionsByState[StateIndex];
			if(pDefinition == nullptr)
				return;
			CUIRect Slot{ColumnRect.x, ColumnPlan.CursorY(), ColumnRect.w, 0.0f};
			float &CachedContentHeight = m_vContentHeights[StateIndex];
			float &CachedContentWidth = m_vContentWidths[StateIndex];
			uint64_t &CachedMeasureRevision = m_vMeasureRevisions[StateIndex];
			const float PreviousContentHeight = CachedContentHeight;
			const bool Collapsed = pDefinition->m_IsCollapsed && pDefinition->m_IsCollapsed();
			const float ContentWidth = std::max(0.0f, Slot.w - 2.0f * ui_token::settings::CARD_PADDING * (Ctx.m_UiScale > 0.0f ? Ctx.m_UiScale : 1.0f));
			if(std::abs(CachedContentWidth - ContentWidth) > 0.01f)
			{
				MeasuredGeometryChanged = MeasuredGeometryChanged || PreviousContentHeight >= 0.0f;
				CachedContentWidth = ContentWidth;
				CachedContentHeight = -1.0f;
			}
			if(CachedMeasureRevision != pDefinition->m_MeasureRevision)
			{
				CachedMeasureRevision = pDefinition->m_MeasureRevision;
				CachedContentHeight = -1.0f;
			}
			if(SettingsCardDeckNeedsContentMeasure(Collapsed, pDefinition->m_MeasureEachFrame, CachedContentHeight))
			{
				CachedContentHeight = pDefinition->m_Measure ? std::max(0.0f, pDefinition->m_Measure(ContentWidth)) : 0.0f;
				MeasuredGeometryChanged = MeasuredGeometryChanged || SettingsCardDeckContentHeightChanged(PreviousContentHeight, CachedContentHeight);
			}
			const float TargetContentHeight = Collapsed ? 0.0f : std::max(0.0f, CachedContentHeight);
			SRuntimeState &Runtime = m_vRuntimeStates[StateIndex];
			const bool HeightInitializedThisFrame = !Runtime.m_ContentHeightInitialized;
			const bool HeightTargetChanged = Runtime.m_ContentHeightInitialized && std::abs(Runtime.m_LastContentHeightTarget - TargetContentHeight) > 0.01f;
			const SSettingsCardHeightAnimationWork HeightWork = ResolveSettingsCardHeightAnimationWork(HeightInitializedThisFrame, HeightTargetChanged, Runtime.m_ContentHeightWasActive, Motion.m_ReflowDuration, m_Drag.Active() || Ctx.m_pAnim == nullptr);
			if(HeightInitializedThisFrame)
			{
				Runtime.m_ContentHeightInitialized = true;
				Runtime.m_AnimatedContentHeight = TargetContentHeight;
				if(Ctx.m_pAnim != nullptr)
					Ctx.m_pAnim->SetValue(SettingsCardHeightNodeKey(pTab, pDefinition->m_Spec.m_pStableId), EUiAnimProperty::HEIGHT, TargetContentHeight);
			}
			else if(HeightWork.m_ResolveHeight)
			{
				const uint64_t HeightKey = SettingsCardHeightNodeKey(pTab, pDefinition->m_Spec.m_pStableId);
				Runtime.m_AnimatedContentHeight = ResolveUiAnimValue(*Ctx.m_pAnim, HeightKey, EUiAnimProperty::HEIGHT, TargetContentHeight, Motion.m_ReflowDuration, EEasing::EASE_OUT);
				Runtime.m_ContentHeightWasActive = Ctx.m_pAnim->HasActiveAnimation(HeightKey, EUiAnimProperty::HEIGHT);
			}
			else if(HeightWork.m_SetHeightTarget)
			{
				Runtime.m_AnimatedContentHeight = TargetContentHeight;
				if(Ctx.m_pAnim != nullptr)
					Ctx.m_pAnim->SetValue(SettingsCardHeightNodeKey(pTab, pDefinition->m_Spec.m_pStableId), EUiAnimProperty::HEIGHT, TargetContentHeight);
				Runtime.m_ContentHeightWasActive = false;
			}
			Runtime.m_LastContentHeightTarget = TargetContentHeight;
			ContentHeightTargetChanged = ContentHeightTargetChanged || HeightTargetChanged;
			ContentHeightAnimationActive = ContentHeightAnimationActive || Runtime.m_ContentHeightWasActive;
			const float ContentHeight = std::max(0.0f, Runtime.m_AnimatedContentHeight);
			const SSettingsCardFrame Frame = BuildSettingsCardFrame(Slot, pDefinition->m_Spec, ContentHeight, Ctx.m_UiScale);
			m_vPreparedCards.push_back({pDefinition, StateIndex, Column, Frame, Runtime.m_ContentHeightWasActive});
			ColumnPlan.Append(Frame.m_Rect.h);
		};
		auto AppendColumn = [&](const std::vector<int> &vStateIndices, int Column, CUIRect ColumnRect, CSettingsCardColumnFramePlan &ColumnPlan) {
			for(const int StateIndex : vStateIndices)
				AppendCard(StateIndex, Column, ColumnRect, ColumnPlan);
		};

		if(DrawLayout.m_TwoColumns && !aDisplayColumns[0].empty())
		{
			const size_t NumLayers = std::max({aDisplayColumns[0].size(), aDisplayColumns[1].size(), aDisplayColumns[2].size()});
			CSettingsCardColumnFramePlan LeftPlan(DrawLayout.m_aColumns[0].y, DrawLayout.m_CardGap);
			CSettingsCardColumnFramePlan RightPlan(DrawLayout.m_aColumns[1].y, DrawLayout.m_CardGap);
			for(size_t Layer = 0; Layer < NumLayers; ++Layer)
			{
				if(Layer < aDisplayColumns[1].size())
					AppendCard(aDisplayColumns[1][Layer], 1, DrawLayout.m_aColumns[0], LeftPlan);
				if(Layer < aDisplayColumns[2].size())
					AppendCard(aDisplayColumns[2][Layer], 2, DrawLayout.m_aColumns[1], RightPlan);

				if(Layer < aDisplayColumns[0].size())
				{
					CSettingsCardColumnFramePlan FullPlan(std::max(LeftPlan.CursorY(), RightPlan.CursorY()), DrawLayout.m_CardGap);
					AppendCard(aDisplayColumns[0][Layer], 0, DrawLayout.m_ContentViewport, FullPlan);
					LeftPlan.SetCursorY(FullPlan.CursorY());
					RightPlan.SetCursorY(FullPlan.CursorY());
				}
			}
		}
		else if(DrawLayout.m_TwoColumns)
		{
			CSettingsCardColumnFramePlan LeftPlan(DrawLayout.m_aColumns[0].y, DrawLayout.m_CardGap);
			CSettingsCardColumnFramePlan RightPlan(DrawLayout.m_aColumns[1].y, DrawLayout.m_CardGap);
			AppendColumn(aDisplayColumns[1], 1, DrawLayout.m_aColumns[0], LeftPlan);
			AppendColumn(aDisplayColumns[2], 2, DrawLayout.m_aColumns[1], RightPlan);
		}
		else
		{
			CSettingsCardColumnFramePlan ColumnPlan(DrawLayout.m_ContentViewport.y, DrawLayout.m_CardGap);
			ForEachSettingsCardDeckVisualOrder(aDisplayColumns, [&](int StateIndex, int Column) {
				AppendCard(StateIndex, Column, DrawLayout.m_ContentViewport, ColumnPlan);
			});
		}
	};

	RebuildActiveStateIndices();
	bool GeometryStateChanged = m_vActiveStateIndices != m_vLastRenderedActiveStateIndices;
	const std::array<std::vector<int>, 3> *pColumns = &m_ProjectionCache.Resolve(Model, pTab, m_vActiveStateIndices);
	BuildPreparedCards(*pColumns);
	GeometryStateChanged = GeometryStateChanged || MeasuredGeometryChanged;

	// 先用当前 active snapshot 的几何处理控制器输入，再为最终 active snapshot 重新计算布局。
	m_vPreviousActiveStateIndices = m_vActiveStateIndices;
	bool PreLayoutGeometryChanged = false;
	for(const SPreparedCard &Card : m_vPreparedCards)
	{
		const bool ControllerVisible = pScrollRegion == nullptr || !pScrollRegion->RectClipped(Card.m_Frame.m_Rect) || Card.m_pDefinition->m_RenderWhenClipped;
		if(ControllerVisible && Card.m_pDefinition->m_VisibilityController && Card.m_pDefinition->m_PreLayoutInput && Card.m_pDefinition->m_PreLayoutInput(Card.m_Frame.m_ContentRect))
		{
			m_vContentHeights[Card.m_StateIndex] = -1.0f;
			PreLayoutGeometryChanged = true;
		}
	}
	RebuildActiveStateIndices();
	if(m_vActiveStateIndices != m_vPreviousActiveStateIndices || PreLayoutGeometryChanged)
	{
		GeometryStateChanged = true;
		pColumns = &m_ProjectionCache.Resolve(Model, pTab, m_vActiveStateIndices);
		BuildPreparedCards(*pColumns);
	}
	GeometryStateChanged = GeometryStateChanged || MeasuredGeometryChanged;
	for(const SPreparedCard &Card : m_vPreparedCards)
	{
		SRuntimeState &Runtime = m_vRuntimeStates[Card.m_StateIndex];
		const bool Collapsed = Card.m_pDefinition->m_IsCollapsed && Card.m_pDefinition->m_IsCollapsed();
		if(Runtime.m_CollapsedInitialized && Runtime.m_LastCollapsed != Collapsed)
			GeometryStateChanged = true;
		Runtime.m_CollapsedInitialized = true;
		Runtime.m_LastCollapsed = Collapsed;
	}
	if(m_Drag.Active() && std::find(m_vActiveStateIndices.begin(), m_vActiveStateIndices.end(), m_Drag.m_StateIndex) == m_vActiveStateIndices.end())
		m_Drag.Reset();
	if(m_Drag.Active() && !Input.m_MouseDown && !Input.m_MouseReleased)
		m_Drag.Reset();
	bool EntryPending = m_AnimateEntry && m_EntryDisplayCycle != m_DisplayCycle;
	bool EntryPositionActive = false;
	float DeckEntryOffsetY = 0.0f;
	bool ReflowTargetChanged = false;
	bool ReflowPositionActive = false;
	// 入场由整个 Deck 共享一个偏移；高度变化按当前动画底边顺排后续卡片，始终保持无重叠几何。
	bool SnapReflow = SettingsCardDeckShouldSnapReflow(GeometryStateChanged, m_Drag.Active()) || ContentHeightTargetChanged || ContentHeightAnimationActive;
	if(Ctx.m_pAnim != nullptr)
	{
		uint64_t EntryKey = 0;
		bool HasEntryKey = false;
		const auto ResolveEntryKey = [&]() {
			if(!HasEntryKey)
			{
				EntryKey = SettingsCardEntryNodeKey(pTab);
				HasEntryKey = true;
			}
			return EntryKey;
		};
		if(m_EntryDisplayCycle != m_DisplayCycle)
		{
			m_EntryDisplayCycle = m_DisplayCycle;
			Ctx.m_pAnim->SetValue(ResolveEntryKey(), EUiAnimProperty::POS_Y, m_AnimateEntry ? Motion.m_EntryDistance : 0.0f);
			m_EntryWasActive = m_AnimateEntry && Motion.m_EntryDuration > 0.0f;
		}
		if(m_EntryWasActive && Motion.m_EntryDuration > 0.0f)
		{
			const uint64_t ResolvedEntryKey = ResolveEntryKey();
			DeckEntryOffsetY = ResolveUiAnimValue(*Ctx.m_pAnim, ResolvedEntryKey, EUiAnimProperty::POS_Y, 0.0f, Motion.m_EntryDuration, EEasing::EASE_OUT);
			EntryPositionActive = Ctx.m_pAnim->HasActiveAnimation(ResolvedEntryKey, EUiAnimProperty::POS_Y);
			m_EntryWasActive = EntryPositionActive;
		}
		else if(m_EntryWasActive)
		{
			Ctx.m_pAnim->SetValue(ResolveEntryKey(), EUiAnimProperty::POS_Y, 0.0f);
			m_EntryWasActive = false;
		}
		EntryPending = false;
		for(const SPreparedCard &Card : m_vPreparedCards)
		{
			const SRuntimeState &Runtime = m_vRuntimeStates[Card.m_StateIndex];
			const char *pStableId = Card.m_pDefinition->m_Spec.m_pStableId;
			if(!ContentHeightTargetChanged && !ContentHeightAnimationActive && Motion.m_ReflowDuration > 0.0f && Runtime.m_ReflowInitialized)
			{
				const float ReflowTargetY = Card.m_Frame.m_Rect.y - ScrollOffset.y;
				ReflowTargetChanged = ReflowTargetChanged || std::abs(Runtime.m_LastReflowTargetY - ReflowTargetY) > 0.001f;
				if(Runtime.m_ReflowWasActive)
				{
					const uint64_t ReflowKey = SettingsCardReflowNodeKey(pTab, pStableId);
					ReflowPositionActive = ReflowPositionActive || Ctx.m_pAnim->HasActiveAnimation(ReflowKey, EUiAnimProperty::POS_Y);
				}
			}
		}
	}
	if(!m_Drag.Active() && SettingsCardDeckAllowsDragStart(EntryPending, EntryPositionActive, ReflowTargetChanged, ReflowPositionActive || ContentHeightAnimationActive) && (Input.m_CtrlPressed || Input.m_AllowHeaderDrag) && Input.m_MousePressed)
	{
		for(const SPreparedCard &Card : m_vPreparedCards)
		{
			const bool InHeader = PointInRect(Card.m_Frame.m_HeaderRect, Input.m_MouseX, Input.m_MouseY);
			const bool InHeaderAction = Card.m_pDefinition->m_HeaderAction && PointInRect(Card.m_Frame.m_HandleRect, Input.m_MouseX, Input.m_MouseY);
			if(InHeader && !InHeaderAction)
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

	if(m_Drag.Active())
	{
		const bool MouseInScrollViewport = PointInRect(ScrollViewport, Input.m_MouseX, Input.m_MouseY);
		if(DrawLayout.m_TwoColumns && m_Drag.m_SourceColumn != 0)
		{
			if(MouseInScrollViewport && Input.m_MouseX >= DrawLayout.m_aColumns[0].x && Input.m_MouseX <= DrawLayout.m_aColumns[0].x + DrawLayout.m_aColumns[0].w)
				m_Drag.m_TargetColumn = 1;
			else if(MouseInScrollViewport && Input.m_MouseX >= DrawLayout.m_aColumns[1].x && Input.m_MouseX <= DrawLayout.m_aColumns[1].x + DrawLayout.m_aColumns[1].w)
				m_Drag.m_TargetColumn = 2;
		}
		m_vDragGeometry.clear();
		m_vDragGeometry.reserve(m_vPreparedCards.size());
		for(const SPreparedCard &Card : m_vPreparedCards)
		{
			const int GeometryColumn = !DrawLayout.m_TwoColumns && m_Drag.m_SourceColumn != 0 && Card.m_Column != 0 ? 1 : Card.m_Column;
			m_vDragGeometry.push_back({Card.m_StateIndex, GeometryColumn, Card.m_Frame.m_Rect});
		}
		const int GeometryTargetColumn = !DrawLayout.m_TwoColumns && m_Drag.m_SourceColumn != 0 ? 1 : m_Drag.m_TargetColumn;
		m_Drag.m_TargetOrder = ResolveSettingsCardDeckDropOrder(Input.m_MouseY, GeometryTargetColumn, m_vDragGeometry, m_Drag.m_StateIndex);
		m_aDragColumns = *pColumns;
		if(DrawLayout.m_TwoColumns)
			ApplySettingsCardDeckDragPlacement(m_aDragColumns, m_Drag.m_StateIndex, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder);
		else
			ApplySettingsCardDeckSingleColumnDragPlacement(m_aDragColumns, m_Drag.m_StateIndex, m_Drag.m_TargetOrder);
		BuildPreparedCards(m_aDragColumns);

		if(pScrollRegion != nullptr && MouseInScrollViewport)
		{
			Result.m_AutoScrollDelta = SettingsCardDeckAutoScrollDelta(Input.m_MouseY, ScrollViewport, Ctx.m_UiScale);
			if(Result.m_AutoScrollDelta != 0.0f)
				pScrollRegion->ScrollRelativeDirect(Result.m_AutoScrollDelta * std::max(0.0f, Input.m_FrameDt));
		}

		if(Input.m_MouseReleased)
		{
			const char *pStableId = m_Drag.m_StateIndex >= 0 && m_Drag.m_StateIndex < Model.Count() ? Model.Entry(m_Drag.m_StateIndex).m_pStableId : nullptr;
			Result.m_OrderChanged = DrawLayout.m_TwoColumns ? CommitSettingsCardDeckDrop(Model, pTab, pStableId, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder, &m_vActiveStateIndices) : CommitSettingsCardDeckSingleColumnDrop(Model, pTab, pStableId, m_Drag.m_TargetOrder, m_vActiveStateIndices);
			if(Result.m_OrderChanged && m_Drag.m_StateIndex >= 0 && m_Drag.m_StateIndex < (int)m_vRuntimeStates.size())
			{
				SRuntimeState &Runtime = m_vRuntimeStates[m_Drag.m_StateIndex];
				Runtime.m_DropFeedbackRemaining = Motion.m_KeepDropFeedback ? Motion.m_DropFeedbackDuration : 0.0f;
				Result.m_DropFeedbackConsumed = Runtime.m_DropFeedbackRemaining > 0.0f;
			}
			m_Drag.Reset();
		}
	}
	for(const SPreparedCard &Card : m_vPreparedCards)
	{
		SRuntimeState &Runtime = m_vRuntimeStates[Card.m_StateIndex];
		const char *pStableId = Card.m_pDefinition->m_Spec.m_pStableId;
		SSettingsCardVisualState State;
		State.m_DrawOffsetY = DeckEntryOffsetY;
		State.m_ClipContent = SettingsCardDeckShouldClipContent(Card.m_ContentHeightAnimationActive);
		if(Ctx.m_pAnim != nullptr)
		{
			const bool ReflowInitializedThisFrame = !Runtime.m_ReflowInitialized;
			const float ReflowTargetY = Card.m_Frame.m_Rect.y - ScrollOffset.y;
			const bool TargetChanged = Runtime.m_ReflowInitialized && std::abs(Runtime.m_LastReflowTargetY - ReflowTargetY) > 0.001f;
			const SSettingsCardAnimationWork AnimationWork = ResolveSettingsCardAnimationWork(0.0f, false, ReflowInitializedThisFrame, SnapReflow, Motion.m_ReflowDuration, TargetChanged, Runtime.m_ReflowWasActive);
			uint64_t ReflowKey = 0;
			bool HasReflowKey = false;
			const auto ResolveReflowKey = [&]() {
				if(!HasReflowKey)
				{
					ReflowKey = SettingsCardReflowNodeKey(pTab, pStableId);
					HasReflowKey = true;
				}
				return ReflowKey;
			};
			if(ContentHeightAnimationActive || ContentHeightTargetChanged)
			{
				// 当前帧的位置已经由动画高度的底边推导，独立 POS_Y 动画会重新制造重叠。
				Runtime.m_ReflowInitialized = false;
				Runtime.m_ReflowWasActive = false;
			}
			else if(ReflowInitializedThisFrame)
			{
				Runtime.m_ReflowInitialized = true;
				Ctx.m_pAnim->SetValue(ResolveReflowKey(), EUiAnimProperty::POS_Y, ReflowTargetY);
			}
			if(ContentHeightAnimationActive || ContentHeightTargetChanged)
			{
				// 下一稳定帧再用最终位置初始化 reflow target。
			}
			else if(SnapReflow)
			{
				if(AnimationWork.m_SetReflowTarget)
					Ctx.m_pAnim->SetValue(ResolveReflowKey(), EUiAnimProperty::POS_Y, ReflowTargetY);
				Runtime.m_ReflowWasActive = false;
			}
			else if(AnimationWork.m_ResolveReflow)
			{
				const uint64_t ResolvedReflowKey = ResolveReflowKey();
				const float ReflowY = ResolveUiAnimValue(*Ctx.m_pAnim, ResolvedReflowKey, EUiAnimProperty::POS_Y, ReflowTargetY, Motion.m_ReflowDuration, EEasing::EASE_OUT);
				State.m_DrawOffsetY += ReflowY - ReflowTargetY;
				const bool ReflowActive = Ctx.m_pAnim->HasActiveAnimation(ResolvedReflowKey, EUiAnimProperty::POS_Y);
				Runtime.m_ReflowWasActive = ReflowActive;
			}
			else
			{
				if(AnimationWork.m_SetReflowTarget)
					Ctx.m_pAnim->SetValue(ResolveReflowKey(), EUiAnimProperty::POS_Y, ReflowTargetY);
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
		State.m_ReflowCompleteFeedback = false;
		Result.m_DropFeedbackConsumed = Result.m_DropFeedbackConsumed || State.m_DropFeedback;
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
			State.m_HoverFeedbackEnabled = !m_SuppressHoverFeedbackOnce && !ScrollMovedThisFrame && !EntryPositionActive &&
						       !ContentHeightAnimationActive && !ReflowTargetChanged && !ReflowPositionActive;
			SettingsCard(Ctx, Card.m_Frame, Card.m_pDefinition->m_Spec, State, VisualOptions,
				SettingsCardDeckRendersContent(Collapsed) ? Card.m_pDefinition->m_Render : FSettingsCardRender{}, Card.m_pDefinition->m_HeaderAction,
				SettingsCardDeckRendersContent(Collapsed) ? Card.m_pDefinition->m_RenderMeasured : FSettingsCardRenderMeasured{});
		}
	}

	if(pScrollRegion != nullptr)
		pScrollRegion->End();
	for(SRuntimeState &Runtime : m_vRuntimeStates)
	{
		Runtime.m_DropFeedbackRemaining = std::max(0.0f, Runtime.m_DropFeedbackRemaining - std::max(0.0f, Input.m_FrameDt));
	}
	// 入场或重排期间鼠标可能仍停在卡片最终位置；只有布局稳定后再次移动鼠标才恢复 hover 亮度，
	// 避免动画结束的首帧从普通背景突然跳到高亮背景。
	const bool LayoutStable = !EntryPending && !EntryPositionActive && !ContentHeightAnimationActive && !ReflowTargetChanged && !ReflowPositionActive;
	if(m_SuppressHoverFeedbackOnce && LayoutStable && m_HasPointerPosition &&
		(std::abs(Input.m_MouseX - m_LastPointerX) > 0.001f || std::abs(Input.m_MouseY - m_LastPointerY) > 0.001f))
		m_SuppressHoverFeedbackOnce = false;
	m_LastPointerX = Input.m_MouseX;
	m_LastPointerY = Input.m_MouseY;
	m_HasPointerPosition = true;
	m_vLastRenderedActiveStateIndices = m_vActiveStateIndices;
	return Result;
}
