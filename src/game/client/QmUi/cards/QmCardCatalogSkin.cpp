#include "QmCardCatalogInternal.h"
#include "QmCardCatalogSkinMetrics.h"

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/menus.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

// 两张皮肤卡共用标准卡片外观，构造、测量和预布局输入均由本模块负责。
namespace qm_card_catalog
{
	bool BuildSkinCard(const SQmCardBuildContext &Ctx, const qm_module::EQmModuleId Id, SSettingsCardDefinition &Out)
	{
		using qm_module::EQmModuleId;
		CMenus *pMenus = Ctx.m_pMenus;
		const SSettingsContentMetrics Metrics = Ctx.m_Metrics;
		const float LineHeight = Metrics.m_LineHeight;
		const float BodySize = Metrics.m_BodySize;
		const float LineSpacing = Metrics.m_LineSpacing;
		const float LabelWidth = Ctx.m_LabelWidth;
		const bool ReadOnly = Ctx.m_ReadOnly;
		FSettingsCardPreLayoutInput PreLayoutInput;

		switch(Id)
		{
		case EQmModuleId::SkinAppearance:
			if(!ReadOnly)
			{
				PreLayoutInput = [pMenus, LineHeight, LineSpacing](CUIRect Content) {
					bool Changed = QmCardRenderHook::HandleQmHudCheckboxInput(pMenus, Content, LineHeight, LineSpacing, &g_Config.m_QmSkinOutlineLocal, &g_Config.m_QmSkinOutlineLocal);
					Changed = QmCardRenderHook::HandleQmHudCheckboxInput(pMenus, Content, LineHeight, LineSpacing, &g_Config.m_QmSkinOutlineOthers, &g_Config.m_QmSkinOutlineOthers) || Changed;
					return Changed;
				};
			}
			MakeModuleCard(
				Ctx, Id, "qm:skin_appearance", "Tee appearance", "Configure Tee appearance and skins",
				[pMenus, LineHeight, BodySize, LineSpacing, LabelWidth, ReadOnly](CUIRect &Content) { QmCardRenderHook::RenderQmVisualSkinAppearanceContent(pMenus, Content, LineHeight, BodySize, LineSpacing, LabelWidth, ReadOnly); },
				[Metrics](float) { return ResolveQmVisualSkinAppearanceHeight(Metrics); },
				0, std::move(PreLayoutInput), Out);
			return true;
		case EQmModuleId::SkinTransition:
			if(!ReadOnly)
			{
				PreLayoutInput = [pMenus, LineHeight, LineSpacing](CUIRect Content) {
					// 偷皮保持独立，只有动画开关影响其下方五行高级参数的高度。
					Content.HSplitTop(LineHeight + LineSpacing, nullptr, &Content);
					return QmCardRenderHook::HandleQmHudCheckboxInput(pMenus, Content, LineHeight, LineSpacing, &g_Config.m_QmSkinChangeTransition, &g_Config.m_QmSkinChangeTransition);
				};
			}
			MakeModuleCard(
				Ctx, Id, "qm:skin_transition", "Skin transition animation", "Configure hammer skin steal and skin transition animations",
				[pMenus, LineHeight, BodySize, LineSpacing, LabelWidth, ReadOnly](CUIRect &Content) { QmCardRenderHook::RenderQmVisualSkinTransitionContent(pMenus, Content, LineHeight, BodySize, LineSpacing, LabelWidth, ReadOnly); },
				[Metrics](float) { return ResolveQmVisualSkinTransitionHeight(Metrics, g_Config.m_QmSkinChangeTransition != 0); },
				g_Config.m_QmSkinChangeTransition ? 1u : 0u, std::move(PreLayoutInput), Out);
			return true;
		default:
			return false;
		}
	}
} // namespace qm_card_catalog

// 保留菜单内容助手，通过 QmCardRenderHook 桥接供分类页与搜索页复用。
void CMenus::RenderQmVisualSkinAppearanceContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)
{
	const float SmallSize = CurrentSettingsContentMetrics().m_SmallSize;
	CUIRect Row, LabelColumn, ControlColumn;
	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmSkinOutlineLocal, "Skin outline for self and dummy", Localize("Skin outline for self and dummy"), &g_Config.m_QmSkinOutlineLocal);
	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmSkinOutlineOthers, "Skin outline for other players", Localize("Skin outline for other players"), &g_Config.m_QmSkinOutlineOthers);
	static CButtonContainer s_SkinOutlineColorId;
	DoLine_ColorPicker(&s_SkinOutlineColorId, CurrentSettingsContentMetrics(), &Content, Localize("Skin outline color"), &g_Config.m_QmSkinOutlineColor, color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmSkinOutlineColor)), false);
	Content.HSplitTop(LineHeight, &Row, &Content);
	Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
	RenderQmVisualLabel("qmclient-skin-outline-width", &LabelColumn, Localize("Skin outline width"), BodySize);
	static int s_SkinOutlineWidthInputId;
	RenderQmSettingsSliderWithValueInput(&s_SkinOutlineWidthInputId, ControlColumn, &g_Config.m_QmSkinOutlineWidth, 1, 6, "", PrewarmOnly);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	Content.HSplitTop(LineHeight, &Row, &Content);
	Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
	RenderQmVisualLabel("qmclient-skin-outline-opacity", &LabelColumn, Localize("Skin outline opacity"), BodySize);
	static int s_SkinOutlineAlphaInputId;
	RenderQmSettingsSliderWithValueInput(&s_SkinOutlineAlphaInputId, ControlColumn, &g_Config.m_QmSkinOutlineAlpha, 0, 100, "%", PrewarmOnly);
	Content.HSplitTop(LineSpacing, nullptr, &Content);

	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmCycleTeeHue, "Cycle custom Tee hue", Localize("Cycle custom Tee hue"), &g_Config.m_QmCycleTeeHue);
	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmCycleTeeHueDummy, "Also apply to dummy", Localize("Also apply to dummy"), &g_Config.m_QmCycleTeeHueDummy);

	Content.HSplitTop(LineHeight, &Row, &Content);
	Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
	SLabelProperties CycleHueSpeedLabelProps;
	CycleHueSpeedLabelProps.m_DisallowNewline = true;
	CycleHueSpeedLabelProps.m_StopAtEnd = true;
	CycleHueSpeedLabelProps.m_MinimumFontSize = 6.0f;
	if(!g_Config.m_QmCycleTeeHue)
		TextRender()->TextColor(ColorRGBA(0.8f, 0.8f, 0.8f, 0.55f));
	RenderQmVisualLabel("qmclient-cycle-tee-hue-speed", &LabelColumn, Localize("Hue speed"), BodySize, TEXTALIGN_ML, CycleHueSpeedLabelProps);
	static int s_QmCycleTeeHueSpeedInputId;
	int DisabledSpeedPreview = g_Config.m_QmCycleTeeHueSpeed;
	RenderQmSettingsSliderWithValueInput(&s_QmCycleTeeHueSpeedInputId, ControlColumn, g_Config.m_QmCycleTeeHue ? &g_Config.m_QmCycleTeeHueSpeed : &DisabledSpeedPreview, 0, 360, "°/s", PrewarmOnly);
	if(!g_Config.m_QmCycleTeeHue)
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	Content.HSplitTop(LineSpacing, nullptr, &Content);

	Content.HSplitTop(SmallSize, &Row, &Content);
	TextRender()->TextColor(ColorRGBA(0.85f, 0.85f, 0.85f, 0.72f));
	RenderQmVisualLabel("qmclient-cycle-tee-hue-custom-note", &Row, Localize("Only affects custom Tee colors."), SmallSize);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Content.HSplitTop(LineSpacing, nullptr, &Content);

	Content.HSplitTop(SmallSize, &Row, &Content);
	TextRender()->TextColor(g_Config.m_TcRainbowTees ? ColorRGBA(1.0f, 0.78f, 0.45f, 0.9f) : ColorRGBA(0.85f, 0.85f, 0.85f, 0.72f));
	RenderQmVisualLabel("qmclient-cycle-tee-hue-tclient-note", &Row, Localize("When TClient rainbow Tee is enabled, this feature has no effect."), SmallSize);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Content.HSplitTop(LineSpacing, nullptr, &Content);

	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmEmoticonShadow, "Emoticon shadow", Localize("Emoticon shadow"), &g_Config.m_QmEmoticonShadow);
}

void CMenus::RenderQmVisualSkinTransitionContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)
{
	CUIRect Row, LabelColumn, ControlColumn;
	RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmHammerSwapSkin, "Hammer skin steal", Localize("Hammer skin steal"), &g_Config.m_QmHammerSwapSkin);

	Content.HSplitTop(LineHeight, &Row, &Content);
	if(DoSettingsButton_CheckBox(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_VISUAL, QMCLIENT_SETTINGS_TAB_VISUAL, &g_Config.m_QmSkinChangeTransition, "Skin transition animation", Localize("Skin transition animation"), g_Config.m_QmSkinChangeTransition, &Row))
		g_Config.m_QmSkinChangeTransition ^= 1;
	Content.HSplitTop(LineSpacing, nullptr, &Content);
	if(!g_Config.m_QmSkinChangeTransition)
		return;

	auto RenderDropDown = [&](const char *pTextId, const char *pText, int *pValue, int MaxValue, const char **ppNames, int NumNames, CUi::SDropDownState &State, CScrollRegion &ScrollRegion) {
		Content.HSplitTop(LineHeight, &Row, &Content);
		Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
		RenderQmVisualLabel(pTextId, &LabelColumn, pText, BodySize);
		State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;
		const int NewValue = DoSettingsDropDown(&ControlColumn, std::clamp(*pValue, 0, MaxValue), ppNames, NumNames, State);
		if(*pValue != NewValue)
			*pValue = NewValue;
		Content.HSplitTop(LineSpacing, nullptr, &Content);
	};
	static CUi::SDropDownState s_SkinTransitionTypeDropDownState;
	static CScrollRegion s_SkinTransitionTypeDropDownScrollRegion;
	const char *apSkinTransitionTypeNames[] = {Localize("Afterimage pop"), Localize("Smooth fade"), Localize("Slide left"), Localize("Spin pop"), Localize("Brightness shift"), Localize("Glitch"), Localize("Elastic")};
	RenderDropDown("qmclient-skin-transition-type", Localize("Skin transition type"), &g_Config.m_QmSkinChangeTransitionType, 6, apSkinTransitionTypeNames, std::size(apSkinTransitionTypeNames), s_SkinTransitionTypeDropDownState, s_SkinTransitionTypeDropDownScrollRegion);
	static CUi::SDropDownState s_SkinTransitionScopeDropDownState;
	static CScrollRegion s_SkinTransitionScopeDropDownScrollRegion;
	static std::vector<const char *> s_SkinTransitionScopeDropDownNames;
	s_SkinTransitionScopeDropDownNames = {Localize("Self only"), Localize("Local"), Localize("All players")};
	RenderDropDown("qmclient-skin-transition-range", Localize("Animation range"), &g_Config.m_QmSkinChangeTransitionScope, 2, s_SkinTransitionScopeDropDownNames.data(), (int)s_SkinTransitionScopeDropDownNames.size(), s_SkinTransitionScopeDropDownState, s_SkinTransitionScopeDropDownScrollRegion);

	Content.HSplitTop(LineHeight, &Row, &Content);
	Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
	SLabelProperties DurationLabelProps;
	DurationLabelProps.m_DisallowNewline = true;
	DurationLabelProps.m_StopAtEnd = true;
	DurationLabelProps.m_MinimumFontSize = 6.0f;
	RenderQmVisualLabel("qmclient-skin-transition-duration", &LabelColumn, Localize("Skin transition duration"), BodySize, TEXTALIGN_ML, DurationLabelProps);
	static int s_QmSkinChangeTransitionMsInputId;
	RenderQmSettingsSliderWithValueInput(&s_QmSkinChangeTransitionMsInputId, ControlColumn, &g_Config.m_QmSkinChangeTransitionMs, 0, 2000, "ms", PrewarmOnly);
	Content.HSplitTop(LineSpacing, nullptr, &Content);

	static CUi::SDropDownState s_SkinTransitionEasingDropDownState;
	static CScrollRegion s_SkinTransitionEasingDropDownScrollRegion;
	const char *apSkinTransitionEasingNames[] = {Localize("Ease out cubic"), Localize("Elastic back"), Localize("Linear"), Localize("Ease in out quad")};
	RenderDropDown("qmclient-skin-transition-easing", Localize("Skin transition easing"), &g_Config.m_QmSkinChangeTransitionEasing, 3, apSkinTransitionEasingNames, std::size(apSkinTransitionEasingNames), s_SkinTransitionEasingDropDownState, s_SkinTransitionEasingDropDownScrollRegion);

	Content.HSplitTop(LineHeight, &Row, &Content);
	Row.VSplitLeft(LabelWidth, &LabelColumn, &ControlColumn);
	RenderQmVisualLabel("qmclient-skin-transition-intensity", &LabelColumn, Localize("Skin transition intensity"), BodySize);
	static int s_QmSkinChangeTransitionIntensityInputId;
	RenderQmSettingsSliderWithValueInput(&s_QmSkinChangeTransitionIntensityInputId, ControlColumn, &g_Config.m_QmSkinChangeTransitionIntensity, 0, 300, "%", PrewarmOnly);
	Content.HSplitTop(LineSpacing, nullptr, &Content);
}
