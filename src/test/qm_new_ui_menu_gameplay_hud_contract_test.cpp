// QmNewUi 菜单源码合同：gameplay HUD 与媒体岛。运行时行为保留在 qm_new_ui_menu_branch_test.cpp。
#include <engine/client/backend/vulkan/backend_vulkan.h>
#include <engine/client/backend_sdl.h>
#include <engine/client/plausible_sizes.h>
#include <engine/client/rounded_rect_geometry.h>
#include <engine/storage.h>

#include <game/client/QmUi/UiSurface.h>
#include <game/client/components/camera.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/nameplate_text_effects.h>
#include <game/client/components/nameplates.h>
#include <game/client/components/qmclient/axiom_auto_login.h>
#include <game/client/components/tclient/statusbar.h>
#include <game/client/components/tooltips.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <gtest/gtest.h>
#include <test/qmclient_source_contract_test.h>
#include <test/test.h>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <string>

TEST(QmNewUiMenuGameplayHudContract, MediaIslandLyricsUsesNeteaseIntegration)
{
	const std::string HudSource = ReadTextFile("src/game/client/components/hud.cpp");
	const std::string MediaIslandCache = FunctionBody(HudSource, "void CHud::EnsureMediaIslandFrameCache() const");
	const std::string IntegrationSource = ReadTextFile("src/game/client/components/qmclient/netease/netease_integration.cpp");
	EXPECT_NE(MediaIslandCache.find("GameClient()->m_NeteaseIntegration.GetCurrentLyric"), std::string::npos);
	EXPECT_NE(IntegrationSource.find("m_QmLyrics"), std::string::npos);
	EXPECT_NE(IntegrationSource.find("m_QmLyricsInMediaIsland"), std::string::npos);
}

TEST(QmNewUiMenuGameplayHudContract, HudNotificationsKeepEdgeGeometryStableDuringSlide)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/hud_notifications/hud_notifications.cpp");
	const std::string MeasureVisibleRect = FunctionBody(Source, "CUIRect CQmHudNotifications::MeasureVisibleRect");
	const std::string RenderNotifications = FunctionBody(Source, "void CQmHudNotifications::RenderNotifications");

	EXPECT_EQ(MeasureVisibleRect.find("MaxSlideOffset"), std::string::npos);
	EXPECT_NE(MeasureVisibleRect.find("return QmHudNotifications::NotificationVisibleRect(BaseRect, MaxWidth, UsedHeight, Flow);"), std::string::npos);
	EXPECT_NE(RenderNotifications.find("Alpha = SmoothStep(ElapsedMs / (float)AnimMs);"), std::string::npos);
	EXPECT_NE(RenderNotifications.find("Alpha = 1.0f - SmoothStep((ElapsedMs - AnimMs - HoldMs) / (float)AnimMs);"), std::string::npos);
	EXPECT_NE(RenderNotifications.find("OffsetX = (1.0f - Alpha) * 14.0f * QmHudNotifications::SmallTextScale(FontSize);"), std::string::npos);
	EXPECT_EQ(RenderNotifications.find("OffsetX = (1.0f - Alpha) * 32.0f;"), std::string::npos);
}

TEST(QmNewUiMenuGameplayHudContract, SpectatorSpecTeeDoesNotFallbackToMissingSkin)
{
	const std::string Source = ReadTextFile("src/game/client/components/players.cpp");
	const std::string Render = FunctionBody(Source, "void CPlayers::OnRender()");
	const std::string SkinsSource = ReadTextFile("src/game/client/components/skins.cpp");
	const std::string Refresh = FunctionBody(SkinsSource, "void CSkins::Refresh(TSkinLoadedCallback &&SkinLoadedCallback)");

	EXPECT_NE(Refresh.find("LoadSpecialSkinDirect(\"x_ninja\");"), std::string::npos);
	EXPECT_NE(Refresh.find("LoadSpecialSkinDirect(\"x_spec\");"), std::string::npos);
	EXPECT_NE(Refresh.find("GameClient()->OnSkinUpdate(pName);"), std::string::npos);
	EXPECT_NE(Render.find("GameClient()->m_Skins.FindOrNullptr(\"x_spec\") == nullptr"), std::string::npos);
	EXPECT_NE(Render.find("!SpectatorTeeRenderInfo() || !SpectatorTeeRenderInfo()->TeeRenderInfo().Valid()"), std::string::npos);
	EXPECT_NE(Source.find("SpectatorTeeRenderInfo.m_TeeRenderFlags = TEE_PREVIEW_LAYER_BODY_OUTLINE;"), std::string::npos);
	EXPECT_NE(Render.find("const bool LocalSpecChar = GameClient()->IsLocalClientId(ClientId);"), std::string::npos);
	EXPECT_NE(Render.find("const bool OtherSpecChar = !LocalSpecChar && (GameClient()->IsOtherTeam(ClientId) || ClientId < 0);"), std::string::npos);
	EXPECT_NE(Render.find("Alpha = OtherSpecChar ? g_Config.m_ClShowOthersAlpha / 100.f : 1.f;"), std::string::npos);
	EXPECT_NE(Render.find("continue;\n\t\tRenderTools()->RenderTee(CAnimState::GetIdle(), &SpectatorTeeRenderInfo()->TeeRenderInfo()"), std::string::npos);
}

TEST(QmNewUiMenuGameplayHudContract, SkinLookupReusesExistingNamesIgnoringCase)
{
	const std::string Source = ReadTextFile("src/game/client/components/skins.cpp");
	const std::string FindContainer = FunctionBody(Source, "const CSkins::CSkinContainer *CSkins::FindContainerImpl(");

	EXPECT_NE(FindContainer.find("str_comp_nocase(Entry.first.c_str(), pName) == 0"), std::string::npos);
	EXPECT_NE(FindContainer.find("Entry.second->State() != CSkinContainer::EState::ERROR"), std::string::npos);
	EXPECT_NE(FindContainer.find("Entry.second->State() != CSkinContainer::EState::NOT_FOUND"), std::string::npos);
}

TEST(QmNewUiMenuGameplayHudContract, FastPracticeSurfacesPracticeStateInHud)
{
	const std::string HudSource = ReadTextFile("src/game/client/components/hud.cpp");
	const std::string HudHeader = ReadTextFile("src/game/client/components/hud.h");

	const std::string PlayerStateBody = FunctionBody(HudSource, "void CHud::RenderPlayerState(");
	ASSERT_FALSE(PlayerStateBody.empty());
	EXPECT_NE(PlayerStateBody.find("const bool FastPracticeParticipant = GameClient()->m_FastPractice.IsPracticeParticipant(ClientId);"), std::string::npos);
	EXPECT_NE(PlayerStateBody.find("|| FastPracticeParticipant"), std::string::npos);
	EXPECT_EQ(PlayerStateBody.find("m_FastPractice.Enabled()"), std::string::npos);
	EXPECT_NE(PlayerStateBody.find("m_PracticeModeOffset"), std::string::npos);

	const std::string MovementBody = FunctionBody(HudSource, "void CHud::RenderMovementInformation()");
	ASSERT_FALSE(MovementBody.empty());
	EXPECT_NE(MovementBody.find("const bool FastPracticeParticipant = GameClient()->m_FastPractice.IsPracticeParticipant(ClientId);"), std::string::npos);
	EXPECT_NE(MovementBody.find("const bool ShowSpeed = !PosOnly && (g_Config.m_ClShowhudPlayerSpeed || FastPracticeParticipant);"), std::string::npos);
	EXPECT_EQ(MovementBody.find("m_FastPractice.Enabled()"), std::string::npos);
	EXPECT_NE(HudHeader.find("void RenderMovementInformation();"), std::string::npos);
}

TEST(QmNewUiMenuGameplayHudContract, NameplateTextEffectsUseSharedRenderHelper)
{
	const std::string RenderHeader = ReadTextFile("src/game/client/render.h");
	const std::string RenderSource = ReadTextFile("src/game/client/render.cpp");
	const std::string NameplatesSource = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string ModesHeader = ReadTextFile("src/game/client/components/qmclient/modes.h");
	const std::string ModesSource = ReadTextFile("src/game/client/components/qmclient/modes.cpp");
	const std::string QmConfigHeader = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string QmMenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(RenderHeader.find("struct SQmTextEffectRenderStyle"), std::string::npos);
	EXPECT_NE(RenderHeader.find("m_OutlineColor"), std::string::npos);
	EXPECT_NE(RenderHeader.find("RenderTextContainerWithEffects"), std::string::npos);
	EXPECT_NE(RenderSource.find("void CRenderTools::RenderTextContainerWithEffects"), std::string::npos);
	EXPECT_NE(RenderSource.find("ColorRGBA OutlineColor = Style.m_OutlineColor.WithMultipliedAlpha(Alpha);"), std::string::npos);
	EXPECT_NE(RenderSource.find("if(BorderEnabled)\n\t\tOutlineColor = Style.m_BorderColor.WithMultipliedAlpha(Alpha);"), std::string::npos);
	EXPECT_NE(RenderSource.find("QM_TEXT_EFFECT_RAINBOW"), std::string::npos);
	EXPECT_NE(RenderSource.find("QM_TEXT_EFFECT_GLOW"), std::string::npos);
	EXPECT_NE(RenderSource.find("for(int Pass = 0; Pass < GlowPasses; ++Pass)"), std::string::npos);

	EXPECT_NE(QmConfigHeader.find("QmNameplateTextEffects"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextBorderColor"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextGradientColor"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextGlowColor"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextGlowRange"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextPlayingScope"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextSpectateScope"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextDemoMode"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmNameplateTextDemoTarget"), std::string::npos);

	EXPECT_NE(ModesHeader.find("enum EQmNameplateTextPlayingScope"), std::string::npos);
	EXPECT_NE(ModesHeader.find("enum EQmNameplateTextSpectateScope"), std::string::npos);
	EXPECT_NE(ModesHeader.find("enum EQmNameplateTextDemoMode"), std::string::npos);
	EXPECT_NE(ModesHeader.find("bool ShouldUseQmNameplateTextEffects("), std::string::npos);
	EXPECT_NE(ModesSource.find("bool ShouldUseQmNameplateTextEffects("), std::string::npos);
	EXPECT_NE(ModesSource.find("case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS:"), std::string::npos);
	EXPECT_NE(ModesSource.find("case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET:"), std::string::npos);
	EXPECT_NE(ModesSource.find("case QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET:"), std::string::npos);

	EXPECT_NE(NameplatesSource.find("BuildQmNameplateTextStyle("), std::string::npos);
	EXPECT_NE(NameplatesSource.find("bool UseEffects"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("Style.m_Effects = UseEffects ? (g_Config.m_QmNameplateTextEffects & ~QM_TEXT_EFFECT_GRADIENT) : 0;"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("AddNameplateGradientSplits(Cursor, m_aText, m_Color, m_GradientColor);"), std::string::npos);
	EXPECT_EQ(NameplatesSource.find("QmRainbowName"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("Data.m_UseTextEffects = ShouldUseQmNameplateTextEffects("), std::string::npos);
	EXPECT_NE(NameplatesSource.find("Style.m_OutlineColor = s_OutlineColor;"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("QmNameplateTextEffectPadding(g_Config.m_QmNameplateTextEffects"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("m_Size = m_RenderSize + vec2(EffectPadding * 2.0f, EffectPadding * 2.0f);"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("Pos.x - m_RenderSize.x / 2.0f"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("Data.m_UseTextEffects = g_Config.m_QmNameplateTextEffects != 0;"), std::string::npos);
	EXPECT_NE(NameplatesSource.find("RenderTools()->RenderTextContainerWithEffects"), std::string::npos);
	EXPECT_EQ(NameplatesSource.find("Rainbow name for local player"), std::string::npos);

	const std::string AppearanceSettings = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string NamePlateBranch = BlockBodyAfter(AppearanceSettings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	ASSERT_FALSE(NamePlateBranch.empty());
	EXPECT_EQ(QmMenusSource.find("RenderNameplateTextSettings(CardContent);"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Nameplate text"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QmNameplateTextEffects"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QmNameplateTextGlowRange"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("DoLine_ColorPicker(&s_NameplateTextBorderColorId"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Playing effects"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Spectate effects"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Demo effects"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Demo target"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("auto RenderNameplateTextControlRow = [&](const char *pTextId, const char *pLabel, const auto &RenderControl)"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("DoAppearanceNumericField(APPEARANCE_TAB_NAME_PLATE, \"appearance-nameplate-text-border-range\""), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("DoAppearanceNumericField(APPEARANCE_TAB_NAME_PLATE, \"appearance-nameplate-text-glow-range\""), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Glow\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("NameplateTextLabelProps.m_DisallowNewline = true"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("NameplateTextLabelProps.m_MinimumFontSize = 6.0f"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("s_NameplateTextDemoTargetDropDownNames"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("bool DemoTargetListed = g_Config.m_QmNameplateTextDemoTarget < 0;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("if(!DemoTargetListed)"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("g_Config.m_QmNameplateTextDemoTarget = DemoTargetNew - 1;"), std::string::npos);
	EXPECT_NE(AppearanceSettings.find("DoAppearanceNumericField(APPEARANCE_TAB_NAME_PLATE, \"appearance-hook-strength-size\""), std::string::npos);
}
