#include "QmCardCatalogInternal.h"

#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>

namespace qm_card_catalog
{
	namespace
	{
		// 三个分类的卡片清单（stableId 与 QmCardRegistry 的默认 Placement 表同源）。
		// 页面从这里取"该有哪些卡"，卡片实现则分派到对应的卡片模块文件。
		const std::vector<const char *> s_vVisualCards = {
			"qm:chat_bubble",
			"qm:camera_view",
			"qm:skin_appearance",
			"qm:skin_transition",
			"qm:weapon_animation",
			"qm:streamer",
			"qm:entity_overlay",
			"qm:collision_hitbox",
			// 本地专属：远程目录不含禅模式卡，吸收目录时补登记，避免切换后从 UI 消失。
			"qm:focus_mode",
		};

		const std::vector<const char *> s_vFunctionCards = {
			"qm:gores_actor",
			"qm:gores",
			"qm:key_binds",
			"qm:emoticons",
			"qm:mini_features",
			"qm:jump_hint",
			"qm:weapon_trajectory",
			"qm:friend_notify",
			"qm:block_words",
			"qm:translate",
			"qm:translate_ui",
			"qm:qiafen",
			"qm:pie_menu",
			"qm:map_upload",
			"qm:favorite_maps",
			"qm:hj_assist",
			// 本地专属：远程目录不含单机分割卡，吸收目录时补登记。
			"qm:solo_split",
		};

		const std::vector<const char *> s_vHudCards = {
			"qm:dummy_miniview",
			"qm:coords",
			"qm:player_stats",
			"qm:debug_graph",
			"qm:debug_mode",
			"qm:input_overlay",
			"qm:hud_notifications",
			"qm:voice",
			"qm:dynamic_island",
			"qm:system_media_controls",
			"qm:lyrics",
			"qm:background_3d",
			"qm:bind_status_hud",
			// 本地专属：远程目录不含速通计时器卡，吸收目录时补登记。
			"qm:speedrun_timer",
		};

		bool ContainsStableId(const std::vector<const char *> &vStableIds, const char *pStableId)
		{
			if(pStableId == nullptr)
				return false;
			return std::any_of(vStableIds.begin(), vStableIds.end(), [pStableId](const char *pCandidate) { return str_comp(pCandidate, pStableId) == 0; });
		}

		uint64_t FoldRevision(uint64_t Hash, const uint64_t Revision)
		{
			return Hash * 1099511628211ULL ^ Revision;
		}
	} // namespace

	const std::vector<const char *> &VisualCardStableIds()
	{
		return s_vVisualCards;
	}

	const std::vector<const char *> &FunctionCardStableIds()
	{
		return s_vFunctionCards;
	}

	const std::vector<const char *> &HudCardStableIds()
	{
		return s_vHudCards;
	}

	bool HasCardModule(const char *pStableId)
	{
		return ContainsStableId(s_vVisualCards, pStableId) || ContainsStableId(s_vFunctionCards, pStableId) || ContainsStableId(s_vHudCards, pStableId);
	}

	uint64_t MeasureContentRevision()
	{
		uint64_t Revision = FoldRevision(0, (uint64_t)s_vVisualCards.size());
		Revision = FoldRevision(Revision, (uint64_t)s_vFunctionCards.size());
		Revision = FoldRevision(Revision, (uint64_t)s_vHudCards.size());
		return Revision;
	}

	void MakeModuleCard(
		const SQmCardBuildContext &Ctx,
		const qm_module::EQmModuleId Id,
		const char *pStableId,
		const char *pTitle,
		const char *pSubtitle,
		const FSettingsCardRenderMeasured &Render,
		FSettingsCardMeasure Measure,
		const uint64_t MeasureRevision,
		FSettingsCardPreLayoutInput PreLayoutInput,
		SSettingsCardDefinition &Out)
	{
		// 标题与描述以 QmCardRegistry 为唯一事实源；构建期未注册时退回卡片模块自带的默认串。
		const qm_card_registry::SCardDefault *pDefault = qm_card_registry::FindByStableId(pStableId);
		const char *pRegisteredTitle = pDefault != nullptr && pDefault->m_pTitle != nullptr ? Localize(pDefault->m_pTitle) : nullptr;
		const char *pRegisteredSubtitle = qm_card_registry::ResolveLocalizedDescription(pStableId);

		Out = {};
		Out.m_Spec = {pStableId, pRegisteredTitle != nullptr ? pRegisteredTitle : Localize(pTitle), pRegisteredSubtitle != nullptr ? pRegisteredSubtitle : Localize(pSubtitle)};
		Out.m_Measure = std::move(Measure);
		Out.m_Render = [Render](CUIRect Content) { Render(Content); };
		if(Ctx.m_pCollapsed != nullptr)
		{
			const int Index = std::clamp((int)Id, 0, (int)qm_module::QmModuleCount - 1);
			Out.m_IsCollapsed = [pCollapsed = Ctx.m_pCollapsed, Index] { return pCollapsed[Index]; };
		}
		if(Ctx.m_pCollapseButtons != nullptr && Ctx.m_pToggleCollapsed != nullptr)
		{
			const int Index = std::clamp((int)Id, 0, (int)qm_module::QmModuleCount - 1);
			CButtonContainer *pCollapseButtons = Ctx.m_pCollapseButtons;
			void (*pToggleCollapsed)(void *, qm_module::EQmModuleId) = Ctx.m_pToggleCollapsed;
			void *pToggleUser = Ctx.m_pToggleCollapsedUser;
			void (*pOnCardExpanded)(void *, qm_module::EQmModuleId) = Ctx.m_pOnCardExpanded;
			void *pExpandedUser = Ctx.m_pOnCardExpandedUser;
			const bool ReadOnly = Ctx.m_ReadOnly;
			CMenus *pMenus = Ctx.m_pMenus;
			Out.m_PreLayoutHeaderInput = [Ctx, pMenus, pCollapseButtons, Index, pToggleCollapsed, pToggleUser, pOnCardExpanded, pExpandedUser, ReadOnly, Id](const SSettingsCardFrame &Frame, const bool IsCollapsed) {
				if(ReadOnly || !QmCardRenderHook::DoButtonLogic(Ctx.m_pMenus, &pCollapseButtons[Index], IsCollapsed, &Frame.m_HandleRect, BUTTONFLAG_LEFT))
					return false;
				pToggleCollapsed(pToggleUser, Id);
				// 展开时让页面按需让测量缓存失效（折叠态与展开态的行数口径不同）。
				if(IsCollapsed && pOnCardExpanded != nullptr)
					pOnCardExpanded(pExpandedUser, Id);
				return true;
			};
			// 头动作（折叠按钮）与上面的输入处理必须同条件：本地页面沿用 deck 自带的默认折叠控件
			// （SettingsCardDeck 的 m_ShowDefaultCollapseButton / m_DefaultCollapseButtonId），
			// 若在此无条件提供 HeaderAction，卡头会同时出现默认按钮与目录按钮——两个折叠按钮。
			const IUiContext CardCtx = Ctx.m_UiContext;
			Out.m_HeaderAction = [CardCtx](const SSettingsCardFrame &Frame, const bool Collapsed) { RenderSettingsCardCollapseButton(CardCtx, Frame.m_HandleRect, Collapsed); };
		}
		Out.m_MeasureRevision = MeasureRevision;
		Out.m_PreLayoutInput = std::move(PreLayoutInput);
	}

	bool BuildCard(const SQmCardBuildContext &Ctx, const char *pStableId, SSettingsCardDefinition &Out)
	{
		qm_module::EQmModuleId Id = qm_module::EQmModuleId::Info;
		if(!qm_module::QmModuleIdFromStableId(pStableId, &Id))
			return false;
		if(ContainsStableId(s_vVisualCards, pStableId))
			return BuildVisualCard(Ctx, Id, Out);
		if(ContainsStableId(s_vFunctionCards, pStableId))
			return BuildFunctionCard(Ctx, Id, Out);
		if(ContainsStableId(s_vHudCards, pStableId))
			return BuildHudCard(Ctx, Id, Out);
		return false;
	}

	void BuildCards(const SQmCardBuildContext &Ctx, const std::vector<const char *> &vStableIds, std::vector<SSettingsCardDefinition> &vOut)
	{
		vOut.reserve(vOut.size() + vStableIds.size());
		for(const char *pStableId : vStableIds)
		{
			SSettingsCardDefinition Definition;
			if(BuildCard(Ctx, pStableId, Definition))
				vOut.push_back(std::move(Definition));
		}
	}

	std::vector<SQmSearchResultEntry> SearchResultEntries(const char *pQuery, const qm_card_order::CModel &Model)
	{
		std::vector<SQmSearchResultEntry> vEntries;
		std::vector<qm_card_registry::SCardSearchResult> vMatches;
		if(pQuery != nullptr && pQuery[0] != '\0')
		{
			vMatches = qm_card_registry::SearchCards(pQuery, Model);
		}
		else
		{
			// 空查询列出全部可构造卡片，供用户浏览（搜索页即卡片目录本身）。
			vMatches.reserve(qm_card_registry::Defaults().size());
			for(const qm_card_registry::SCardDefault &Default : qm_card_registry::Defaults())
			{
				qm_card_registry::SCardSearchResult Match;
				Match.m_pStableId = Default.m_pStableId;
				Match.m_Title = Default.m_pTitle != nullptr ? Localize(Default.m_pTitle) : "";
				Match.m_Description = Default.m_pDescription != nullptr ? Localize(Default.m_pDescription) : "";
				Match.m_Target = qm_card_registry::ResolveCardNavigationTarget(Default, Model);
				vMatches.push_back(std::move(Match));
			}
		}

		vEntries.reserve(vMatches.size());
		for(qm_card_registry::SCardSearchResult &Match : vMatches)
		{
			// 只有真正有卡片模块的 stableId 才能在搜索页就地渲染，其余（已下线/纯占位）不展示。
			if(!HasCardModule(Match.m_pStableId))
				continue;
			const char *pTab = Match.m_Target.m_pTab;
			if(pTab != nullptr && str_comp(pTab, "global-search") == 0)
				continue;
			SQmSearchResultEntry Entry;
			Entry.m_pStableId = Match.m_pStableId;
			Entry.m_pTab = pTab;
			Entry.m_Title = std::move(Match.m_Title);
			Entry.m_Description = std::move(Match.m_Description);
			vEntries.push_back(std::move(Entry));
		}
		return vEntries;
	}

	std::vector<qm_card_order::SEntry> BuildSearchModelEntries(const std::vector<SQmSearchResultEntry> &vResults)
	{
		// 搜索结果每张卡独占整行：Full 列 + 顺序即匹配顺序，搜索页不参与分类内的拖拽排序。
		std::vector<qm_card_order::SEntry> vEntries;
		vEntries.reserve(vResults.size());
		for(size_t Index = 0; Index < vResults.size(); ++Index)
			vEntries.push_back({vResults[Index].m_pStableId, "global-search", 0, (int)Index});
		return vEntries;
	}
} // namespace qm_card_catalog
