// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <engine/storage.h>

#include <game/client/components/camera.h>
#include <game/client/components/qmclient/axiom_auto_login.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>

namespace
{

	std::string ReadTextFile(const char *pPath)
	{
		std::string Content = ReadTestSourceFile(pPath);
		// menus_settings.cpp ships with CRLF line terminators; normalize so the
		// multi-line BlockBodyAfter anchors below match regardless of source EOL.
		Content.erase(std::remove(Content.begin(), Content.end(), '\r'), Content.end());
		return Content;
	}

	std::string FunctionBody(const std::string &Source, const std::string &Signature)
	{
		const size_t FunctionStart = Source.find(Signature);
		EXPECT_NE(FunctionStart, std::string::npos) << Signature;
		const size_t BodyStart = Source.find("{", FunctionStart);
		EXPECT_NE(BodyStart, std::string::npos) << Signature;
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}')
			{
				--Depth;
				if(Depth == 0)
					return Source.substr(BodyStart, Index - BodyStart);
			}
		}
		ADD_FAILURE() << Signature;
		return {};
	}

	std::string BlockBodyAfter(const std::string &Source, const std::string &Anchor)
	{
		const size_t AnchorPos = Source.find(Anchor);
		EXPECT_NE(AnchorPos, std::string::npos) << Anchor;
		const size_t BodyStart = Source.find("{", AnchorPos);
		EXPECT_NE(BodyStart, std::string::npos) << Anchor;
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}')
			{
				--Depth;
				if(Depth == 0)
					return Source.substr(BodyStart, Index - BodyStart);
			}
		}
		ADD_FAILURE() << Anchor;
		return {};
	}

	size_t MatchingBrace(const std::string &Source, size_t BodyStart)
	{
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}')
			{
				--Depth;
				if(Depth == 0)
					return Index;
			}
		}
		return std::string::npos;
	}

} // namespace

TEST(QmNewUiMenuBranches, MenubarUsesExplicitQmNewUiColorBranch)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string DoMenuTabV2 = FunctionBody(Source, "int CMenus::DoMenuTabV2(");
	const std::string RenderMenubar = FunctionBody(Source, "void CMenus::RenderMenubar(");
	const size_t UseNewUiIfPos = RenderMenubar.find("if(UseNewUi)");
	ASSERT_NE(UseNewUiIfPos, std::string::npos);
	const size_t UseNewUiBodyStart = RenderMenubar.find("{", UseNewUiIfPos);
	ASSERT_NE(UseNewUiBodyStart, std::string::npos);
	const size_t UseNewUiBodyEnd = MatchingBrace(RenderMenubar, UseNewUiBodyStart);
	ASSERT_NE(UseNewUiBodyEnd, std::string::npos);
	const std::string UseNewUiBlock = RenderMenubar.substr(UseNewUiBodyStart, UseNewUiBodyEnd - UseNewUiBodyStart);
	const size_t OldUiElsePos = RenderMenubar.find("else", UseNewUiBodyEnd);
	ASSERT_NE(OldUiElsePos, std::string::npos);
	const size_t OldUiBodyStart = RenderMenubar.find("{", OldUiElsePos);
	ASSERT_NE(OldUiBodyStart, std::string::npos);
	const size_t OldUiBodyEnd = MatchingBrace(RenderMenubar, OldUiBodyStart);
	ASSERT_NE(OldUiBodyEnd, std::string::npos);
	const std::string OldUiBlock = RenderMenubar.substr(OldUiBodyStart, OldUiBodyEnd - OldUiBodyStart);
	const size_t HoverBranch = DoMenuTabV2.find("if(Hover)");
	const size_t ActiveBranch = DoMenuTabV2.find("else if(Active)");

	EXPECT_NE(Source.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(Source.find("MenuTabDefaultColor("), std::string::npos);
	EXPECT_NE(Source.find("MenuTabActiveColor("), std::string::npos);
	EXPECT_NE(Source.find("MenuTabHoverColor("), std::string::npos);
	EXPECT_NE(Source.find("MenuIconButtonDefaultColor("), std::string::npos);
	ASSERT_NE(HoverBranch, std::string::npos);
	ASSERT_NE(ActiveBranch, std::string::npos);
	EXPECT_LT(HoverBranch, ActiveBranch);
	EXPECT_NE(DoMenuTabV2.find("Target = pCustomHover != nullptr ? *pCustomHover : HoverColor;"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("Target = pCustomActive != nullptr ? *pCustomActive : ActiveColor;"), std::string::npos);
	EXPECT_NE(Source.find("return UseNewUi ? MenuTabDefaultColor() : ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);"), std::string::npos);
	EXPECT_NE(Source.find("const ColorRGBA DefaultColor = UseNewUi ? MenuTabDefaultColor() : ms_ColorTabbarInactive;"), std::string::npos);
	EXPECT_NE(Source.find("const ColorRGBA ActiveColor = UseNewUi ? MenuTabActiveColor() : ms_ColorTabbarActive;"), std::string::npos);
	EXPECT_NE(Source.find("const ColorRGBA HoverColor = UseNewUi ? MenuTabHoverColor() : ms_ColorTabbarHover;"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("const bool UseLargeMenubarControl = UseNewUi && LargeMenubarControl;"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("pRect->Draw(Resolved, Corners, ControlRadius);"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("const float LabelFontSize = UseNewUi ? minimum(Label.h * CUi::ms_FontmodHeight, NewUiLabelFontSize) : Label.h * CUi::ms_FontmodHeight;"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("Ui()->DoLabel(&Label, pText, LabelFontSize, TEXTALIGN_MC);"), std::string::npos);
	EXPECT_NE(Source.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA InactiveColor = MenuTabDefaultColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA ActiveColor = MenuTabActiveColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA HoverColor = MenuMenubarHoverColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA InactiveColor = ms_ColorTabbarInactive;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA ActiveColor = ms_ColorTabbarActive;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA HoverColor = ms_ColorTabbarHover;"), std::string::npos);
	EXPECT_NE(Source.find("const ColorRGBA IndicatorColor = g_Config.m_QmNewUi != 0 ? MenuUiColorAccent(1.0f) : ui_token::color::ACCENT_PRIMARY;"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("if(!UseNewUi && MenubarHaveActive)"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("if(UseNewUi)"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.12f)"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.VMargin(MenubarOuterInsetX, &Box);"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.HMargin(MenubarOuterInsetY, &Box);"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float BrowserButtonWidth = 58.0f;"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float GameButtonWidth = CompactOnlineMenuTabs ? 56.0f : 64.0f;"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float ServerInfoButtonWidth = CompactOnlineMenuTabs ? 94.0f : 104.0f;"), std::string::npos);
	EXPECT_EQ(UseNewUiBlock.find("GameButtonWidth = (CompactOnlineMenuTabs ? 56.0f : 64.0f) * MENU_MENUBAR_CONTROL_SCALE"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float OnlineTabGap = 4.0f;"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("if(DoMenuTabV2(&s_SettingsButton"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("if(DoMenuTabV2(&s_InternetButton"), std::string::npos);
	EXPECT_EQ(UseNewUiBlock.find("DoButton_MenuTab(&s_SettingsButton"), std::string::npos);
	EXPECT_EQ(UseNewUiBlock.find("DoButton_MenuTab(&s_InternetButton"), std::string::npos);
	EXPECT_EQ(OldUiBlock.find("Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.12f)"), std::string::npos);
	EXPECT_EQ(OldUiBlock.find("Box.VMargin(MenubarOuterInsetX, &Box);"), std::string::npos);
	EXPECT_EQ(OldUiBlock.find("Box.HMargin(MenubarOuterInsetY, &Box);"), std::string::npos);
	EXPECT_NE(OldUiBlock.find("if(DoButton_MenuTab(&s_SettingsButton"), std::string::npos);
	EXPECT_NE(OldUiBlock.find("if(DoButton_MenuTab(&s_InternetButton"), std::string::npos);
	EXPECT_EQ(OldUiBlock.find("DoMenuTabV2(&s_SettingsButton"), std::string::npos);
	EXPECT_EQ(OldUiBlock.find("DoMenuTabV2(&s_InternetButton"), std::string::npos);
}

TEST(QmNewUiMenuBranches, MenubarControlsScaleVerticallyWithoutIncreasingPageTabWidths)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string RenderMenubar = FunctionBody(Source, "void CMenus::RenderMenubar(");
	const std::string DoIngameMenuTab = FunctionBody(Source, "int CMenus::DoIngameMenuTab(");

	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_CONTROL_SCALE = 1.20f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_CONTROL_HEIGHT = 19.0f * MENU_MENUBAR_CONTROL_SCALE;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_CONTENT_INSET = 2.0f * MENU_MENUBAR_CONTROL_SCALE;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_LABEL_FONT_SIZE = 13.0f * MENU_MENUBAR_CONTROL_SCALE;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_CONTROL_RADIUS = 7.0f * MENU_MENUBAR_CONTROL_SCALE;"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("const float MenubarOuterInsetY = maximum(0.0f, (Box.h - MENU_MENUBAR_CONTROL_HEIGHT) * 0.5f);"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("const float MenubarIconButtonSize = Box.h;"), std::string::npos);
	EXPECT_NE(Header.find("bool LargeMenubarControl = false"), std::string::npos);
	EXPECT_NE(Source.find("const float ContentInset = UseLargeMenubarControl ? MENU_MENUBAR_CONTENT_INSET : 2.0f;"), std::string::npos);
	EXPECT_NE(DoIngameMenuTab.find("Text.HMargin(g_Config.m_QmNewUi != 0 ? MENU_MENUBAR_CONTENT_INSET : 2.0f, &Text);"), std::string::npos);
	EXPECT_NE(DoIngameMenuTab.find("DoMenuTabV2(pButtonContainer, pText, Checked != 0, pRect, Corners, true"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("DoMenuTabV2(&s_SettingsButton, FONT_ICON_GEAR, ActivePage == PAGE_SETTINGS, &SettingsButton, IGraphics::CORNER_ALL, true"), std::string::npos);
	EXPECT_NE(DoIngameMenuTab.find("minimum(Text.h * CUi::ms_FontmodHeight, MENU_MENUBAR_LABEL_FONT_SIZE)"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float MENU_MENUBAR_HEIGHT_NEW = 24.0f;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, MenubarIndicatorSplitsACompactSatelliteTowardHoveredPageTabs)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string RenderMenubar = FunctionBody(Source, "void CMenus::RenderMenubar(");

	EXPECT_NE(Source.find("constexpr float MENU_TAB_INDICATOR_WIDTH_RATIO = 0.35f;"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandBlobPose(SatelliteProgress)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandBlobConnectionStrength(BlobPose.m_Travel)"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("Page != ActivePage && Ui()->HotItem() == pButtonId"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("const CUIRect SatelliteTarget = MenubarIndicatorTargetRect(MenubarHaveHovered ? MenubarHoveredRect : MenubarActiveRect);"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("if(MenubarHaveHovered || SatelliteProgress > MENU_TAB_ANIM_EPSILON)"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("MenubarTrackPage(PAGE_GAME, Button, &s_GameButton);"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("DoIngameMenuTab(&s_GameButton, PAGE_GAME, \"ingame-tab-game\", Localize(\"Game\"), ActivePage == PAGE_GAME, &Button, IGraphics::CORNER_ALL)"), std::string::npos);
}

TEST(QmCameraEffects, DynamicFovRemovalKeepsBaseZoomStable)
{
	EXPECT_FLOAT_EQ(QmCameraEffects::ZoomWithoutDynamicFov(2.0f, 1.25f), 1.6f);
	EXPECT_FLOAT_EQ(QmCameraEffects::ZoomWithoutDynamicFov(1.6f, 1.0f), 1.6f);
	EXPECT_FLOAT_EQ(QmCameraEffects::ZoomWithoutDynamicFov(1.6f, 0.0f), 1.6f);
}

TEST(QmCameraEffects, CinematicFreeviewSmoothingIsFrameRateIndependent)
{
	const vec2 Start(10.0f, 20.0f);
	const vec2 Target(30.0f, 60.0f);
	vec2 At30Fps = Start;
	vec2 At60Fps = Start;
	for(int Frame = 0; Frame < 30; ++Frame)
		At30Fps = QmCameraEffects::SmoothCinematicPosition(At30Fps, Target, 1.0f / 30.0f);
	for(int Frame = 0; Frame < 60; ++Frame)
		At60Fps = QmCameraEffects::SmoothCinematicPosition(At60Fps, Target, 1.0f / 60.0f);

	EXPECT_FLOAT_EQ(QmCameraEffects::SmoothCinematicPosition(Start, Target, 0.0f).x, Start.x);
	EXPECT_NEAR(At30Fps.x, At60Fps.x, 0.0001f);
	EXPECT_NEAR(At30Fps.y, At60Fps.y, 0.0001f);
	EXPECT_GT(At30Fps.x, Start.x);
	EXPECT_LT(At30Fps.x, Target.x);
}

TEST(QmCameraEffectsSource, CinematicCameraAndDynamicFovKeepScopedState)
{
	const std::string Config = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string Header = ReadTextFile("src/game/client/components/camera.h");
	const std::string Source = ReadTextFile("src/game/client/components/camera.cpp");
	const std::string OnRender = FunctionBody(Source, "void CCamera::OnRender()");
	const std::string ScaleZoom = FunctionBody(Source, "void CCamera::ScaleZoom(");
	const std::string ChangeZoom = FunctionBody(Source, "void CCamera::ChangeZoom(");
	const std::string UpdateCamera = FunctionBody(Source, "void CCamera::UpdateCamera()");
	const std::string OnReset = FunctionBody(Source, "void CCamera::OnReset()");
	const std::string GameClient = ReadTextFile("src/game/client/gameclient.cpp");

	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmCinematicCamera, qm_cinematic_camera"), std::string::npos);
	EXPECT_NE(Header.find("m_CinematicCameraSmoothing"), std::string::npos);
	EXPECT_NE(OnRender.find("GameClient()->m_Snap.m_SpecInfo.m_Active && !GameClient()->m_Snap.m_SpecInfo.m_UsePosition"), std::string::npos);
	EXPECT_NE(OnRender.find("if(g_Config.m_QmCinematicCamera)"), std::string::npos);
	EXPECT_NE(OnRender.find("m_CinematicCameraSmoothing = false;"), std::string::npos);
	EXPECT_NE(ScaleZoom.find("RemoveDynamicFovZoom();"), std::string::npos);
	EXPECT_NE(ChangeZoom.find("RemoveDynamicFovZoom();"), std::string::npos);
	EXPECT_NE(UpdateCamera.find("RemoveDynamicFovZoom();"), std::string::npos);
	EXPECT_NE(OnReset.find("m_DynamicFovAppliedFactor = 1.0f;"), std::string::npos);
	EXPECT_EQ(UpdateCamera.find("m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy] += m_DriftCurrentOffset;"), std::string::npos);
	EXPECT_NE(OnRender.find("m_aDyncamCurrentCameraOffset[g_Config.m_ClDummy] + m_DriftCurrentOffset"), std::string::npos);
	EXPECT_NE(Header.find("float BaseZoom() const"), std::string::npos);
	EXPECT_NE(GameClient.find("m_Camera.BaseZoom()"), std::string::npos);
	EXPECT_EQ(GameClient.find("float ShowDistanceZoom = m_Camera.m_Zoom;"), std::string::npos);
}

TEST(QmStoragePath, BuildsCandidatesRelativeToExecutable)
{
	char aPath[IO_MAX_PATH_LENGTH];
	EXPECT_TRUE(StoragePathFromExecutable("C:\\QmClient\\DDNet.exe", "data/mapres", aPath, sizeof(aPath)));
	EXPECT_STREQ(aPath, "C:\\QmClient/data/mapres");
	EXPECT_TRUE(StoragePathFromExecutable("/opt/qmclient/DDNet", "storage.cfg", aPath, sizeof(aPath)));
	EXPECT_STREQ(aPath, "/opt/qmclient/storage.cfg");
	EXPECT_FALSE(StoragePathFromExecutable("DDNet.exe", "data", aPath, sizeof(aPath)));
}

TEST(QmStoragePathSource, ExecutableCandidatesPrecedeCurrentDirectoryFallbacks)
{
	const std::string Source = ReadTextFile("src/engine/shared/storage.cpp");
	const std::string LoadPaths = FunctionBody(Source, "bool LoadPathsFromFile(");
	const std::string FindData = FunctionBody(Source, "void FindDataDirectory(");

	const size_t ExecutableStorage = LoadPaths.find("StoragePathFromExecutable");
	const size_t CurrentDirectoryStorage = LoadPaths.find("io_open(\"storage.cfg\"");
	const size_t ExecutableData = FindData.find("StoragePathFromExecutable");
	const size_t CurrentDirectoryData = FindData.find("fs_is_dir(\"data/mapres\"");
	ASSERT_NE(ExecutableStorage, std::string::npos);
	ASSERT_NE(CurrentDirectoryStorage, std::string::npos);
	ASSERT_NE(ExecutableData, std::string::npos);
	ASSERT_NE(CurrentDirectoryData, std::string::npos);
	EXPECT_LT(ExecutableStorage, CurrentDirectoryStorage);
	EXPECT_LT(ExecutableData, CurrentDirectoryData);
}

TEST(QmLocalization, ContextRequiresOpeningAndClosingBrackets)
{
	EXPECT_TRUE(LocalizationIsContextLine("[menu]"));
	EXPECT_FALSE(LocalizationIsContextLine("[%s] %s (Map: %s, Time: %s)"));
	EXPECT_FALSE(LocalizationIsContextLine("[broken"));
	EXPECT_FALSE(LocalizationIsContextLine("plain"));
}

TEST(QmLocalizationSource, LocalizedDropdownNamesAreNotStaticHeapPointers)
{
	const std::string Chat = ReadTextFile("src/game/client/components/chat.cpp");
	const std::string Menus = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Localization = ReadTextFile("src/game/localization.cpp");

	EXPECT_EQ(Chat.find("static const char *s_apBackendNames[] = {Localize"), std::string::npos);
	EXPECT_EQ(Menus.find("static std::vector<const char *> s_LlmProviderDropDownNames ="), std::string::npos);
	EXPECT_EQ(Menus.find("static const char *s_apSourceNames[] ="), std::string::npos);
	EXPECT_EQ(Menus.find("static const char *s_apOutgoingModeNames[] ="), std::string::npos);
	EXPECT_NE(Localization.find("if(LocalizationIsContextLine(pLine))"), std::string::npos);
	EXPECT_NE(Localization.find("Couldn't open language file '%s'"), std::string::npos);
}

TEST(QmUiScaleSource, ScaleChangesResetContainersAndInvalidateScaleKeys)
{
	const std::string Config = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string UiSource = ReadTextFile("src/game/client/ui.cpp");
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string TClientMenusSource = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string QmMenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Update = FunctionBody(UiSource, "void CUi::Update(");
	const std::string Screen = FunctionBody(UiSource, "const CUIRect *CUi::Screen()");

	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmUiScale, qm_ui_scale, 100, 50, 200"), std::string::npos);
	EXPECT_NE(Update.find("Client()->OnWindowResize();"), std::string::npos);
	EXPECT_NE(Screen.find("QmUiVirtualScreenHeight(g_Config.m_QmUiScale)"), std::string::npos);
	EXPECT_NE(MenusSource.find("StyleKey.m_UiScaleBucket = std::clamp(g_Config.m_QmUiScale, 50, 200);"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("std::clamp(g_Config.m_QmUiScale, 50, 200)"), std::string::npos);
	EXPECT_NE(QmMenusSource.find("CUi::SCROLLBAR_OPTION_DELAYUPDATE"), std::string::npos);
}

TEST(QmUiScale, VirtualHeightUsesClampedPercentage)
{
	EXPECT_FLOAT_EQ(QmUiVirtualScreenHeight(50), 1200.0f);
	EXPECT_FLOAT_EQ(QmUiVirtualScreenHeight(100), 600.0f);
	EXPECT_FLOAT_EQ(QmUiVirtualScreenHeight(200), 300.0f);
	EXPECT_FLOAT_EQ(QmUiVirtualScreenHeight(0), 1200.0f);
	EXPECT_FLOAT_EQ(QmUiVirtualScreenHeight(300), 300.0f);
}

TEST(QmUiScale, CenteredPopupMarginKeepsUsableContentAtTwoHundredPercent)
{
	const CUIRect DefaultScreen = {0.0f, 0.0f, 1066.0f, 600.0f};
	const CUIRect ScaledScreen = {0.0f, 0.0f, 533.0f, 300.0f};
	const CUIRect NarrowScaledScreen = {0.0f, 0.0f, 375.0f, 300.0f};

	EXPECT_FLOAT_EQ(QmUiCenteredMargin(DefaultScreen, 150.0f, 300.0f, 180.0f), 150.0f);
	EXPECT_FLOAT_EQ(QmUiCenteredMargin(ScaledScreen, 150.0f, 300.0f, 180.0f), 60.0f);
	EXPECT_FLOAT_EQ(QmUiCenteredMargin(NarrowScaledScreen, 150.0f, 300.0f, 180.0f), 37.5f);
	EXPECT_FLOAT_EQ(QmUiCenteredMargin({0.0f, 0.0f, 200.0f, 120.0f}, 150.0f, 300.0f, 180.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmUiCenteredMargin(ScaledScreen, 150.0f, 300.0f, 300.0f), 0.0f);
	EXPECT_EQ(QmUiVisibleRows(52.0f, 20.0f, 20.0f, 4, 4), 1);
	EXPECT_EQ(QmUiVisibleRows(126.0f, 20.0f, 20.0f, 8, 4), 4);
	EXPECT_EQ(QmUiVisibleRows(19.0f, 20.0f, 20.0f, 4, 4), 0);
}

TEST(QmUiScaleSource, TouchMenusRespectCallerProvidedScaledHeight)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_ingame_touch_controls.cpp");
	EXPECT_EQ(Source.find("MainView.h = 600.0f - 40.0f - MainView.y;"), std::string::npos);
	EXPECT_NE(Source.find("void CMenusIngameTouchControls::RenderTouchButtonEditor(CUIRect MainView)"), std::string::npos);
	EXPECT_NE(Source.find("void CMenusIngameTouchControls::RenderTouchButtonBrowser(CUIRect MainView)"), std::string::npos);
	EXPECT_NE(Source.find("void CMenusIngameTouchControls::RenderPreviewSettings(CUIRect MainView)"), std::string::npos);
}

TEST(QmUiScaleSource, BlockingPopupsAndDemoRowsFitScaledScreen)
{
	const std::string Menus = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string DemoMenus = ReadTextFile("src/game/client/components/menus_demo.cpp");
	EXPECT_NE(Menus.find("QmUiCenteredMargin(Box, 150.0f, 300.0f, 300.0f)"), std::string::npos);
	EXPECT_NE(Menus.find("QmUiCenteredMargin(Screen, 150.0f, 300.0f, 300.0f)"), std::string::npos);
	EXPECT_NE(DemoMenus.find("QmUiVisibleRows(SegmentsArea.h"), std::string::npos);
	EXPECT_NE(DemoMenus.find("VerticalExpansion = std::min(60.0f, PopupMargin)"), std::string::npos);
}

TEST(QmDemoCutRender, UsesExportedCutAsRenderSource)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_demo.cpp");
	const std::string SlicePopup = FunctionBody(Source, "void CMenus::RenderDemoPlayerSliceSavePopup(CUIRect MainView)");

	EXPECT_NE(SlicePopup.find("str_format(m_aPendingDemoRenderSelectionName, sizeof(m_aPendingDemoRenderSelectionName), \"%s.demo\", m_DemoSliceInput.GetString());"), std::string::npos);
	EXPECT_EQ(SlicePopup.find("str_copy(m_aPendingDemoRenderSelectionName, m_aCurrentDemoSelectionName"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BrowserUsesExplicitQmNewUiShellBranch)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string RenderServerbrowser = FunctionBody(Source, "void CMenus::RenderServerbrowser(");
	const size_t TopUseNewUiIfPos = RenderServerbrowser.find("if(UseNewUi)\n\t\tView.Margin(6.0f, &View);");
	ASSERT_NE(TopUseNewUiIfPos, std::string::npos);
	const size_t TopOldUiElsePos = RenderServerbrowser.find("else\n\t{", TopUseNewUiIfPos);
	ASSERT_NE(TopOldUiElsePos, std::string::npos);
	const size_t TopOldUiBodyStart = RenderServerbrowser.find("{", TopOldUiElsePos);
	ASSERT_NE(TopOldUiBodyStart, std::string::npos);
	const size_t TopOldUiBodyEnd = MatchingBrace(RenderServerbrowser, TopOldUiBodyStart);
	ASSERT_NE(TopOldUiBodyEnd, std::string::npos);
	const std::string TopOldUiBlock = RenderServerbrowser.substr(TopOldUiBodyStart, TopOldUiBodyEnd - TopOldUiBodyStart);

	const size_t UseNewUiIfPos = RenderServerbrowser.find("if(UseNewUi)", TopOldUiBodyEnd);
	ASSERT_NE(UseNewUiIfPos, std::string::npos);
	const size_t UseNewUiBodyStart = RenderServerbrowser.find("{", UseNewUiIfPos);
	ASSERT_NE(UseNewUiBodyStart, std::string::npos);
	const size_t UseNewUiBodyEnd = MatchingBrace(RenderServerbrowser, UseNewUiBodyStart);
	ASSERT_NE(UseNewUiBodyEnd, std::string::npos);
	const size_t OldUiElsePos = RenderServerbrowser.find("else", UseNewUiBodyEnd);
	ASSERT_NE(OldUiElsePos, std::string::npos);
	const size_t OldUiBodyStart = RenderServerbrowser.find("{", OldUiElsePos);
	ASSERT_NE(OldUiBodyStart, std::string::npos);
	const size_t OldUiBodyEnd = MatchingBrace(RenderServerbrowser, OldUiBodyStart);
	ASSERT_NE(OldUiBodyEnd, std::string::npos);
	const std::string OldUiBlock = RenderServerbrowser.substr(OldUiBodyStart, OldUiBodyEnd - OldUiBodyStart);

	EXPECT_NE(Source.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(Source.find("if(UseNewUi)"), std::string::npos);
	EXPECT_NE(Source.find("ServerListBase.Draw(BrowserPanelColor()"), std::string::npos);
	EXPECT_NE(Source.find("(void)DrawBackground;"), std::string::npos);
	EXPECT_NE(Source.find("const float ToolBoxWidth = UseNewUi ? 205.0f : 188.0f;"), std::string::npos);
	EXPECT_NE(Source.find("const float ColumnGap = UseNewUi ? 10.0f : 6.0f;"), std::string::npos);
	EXPECT_NE(Source.find("const float StatusHeight = UseNewUi ? 84.0f : 76.0f;"), std::string::npos);
	EXPECT_NE(Source.find("CUIRect ServerListStackBase = ServerListBase;"), std::string::npos);
	EXPECT_NE(Source.find("ServerListStackBase.HSplitBottom(StatusHeight, &ServerListBase, &StatusBox);"), std::string::npos);
	EXPECT_NE(Source.find("StatusBox.y = ServerListStackBase.y + ServerListStackBase.h - StatusHeight;"), std::string::npos);
	EXPECT_NE(Source.find("ServerListBase.h = maximum(StatusBox.y - ColumnGap - ServerListBase.y, 0.0f);"), std::string::npos);
	EXPECT_EQ(Source.find("ServerListBase.HSplitBottom(ColumnGap, &ServerListBase, nullptr);"), std::string::npos);
	EXPECT_NE(Source.find("ServerListBase.Margin(std::clamp(ServerListBase.w * 0.006f, 1.0f, 4.0f), &ServerListBase);"), std::string::npos);
	EXPECT_NE(TopOldUiBlock.find("View.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);"), std::string::npos);
	EXPECT_NE(TopOldUiBlock.find("View.Margin(10.0f, &View);"), std::string::npos);
	EXPECT_EQ(TopOldUiBlock.find("View.Margin(std::clamp(View.w * 0.008f, 4.0f, 8.0f), &View);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BrowserInteriorBackgroundsUseMapBrowserOpacity)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");

	EXPECT_NE(Source.find("g_Config.m_QmMapBrowserOpacity / 100.0f"), std::string::npos);
	EXPECT_NE(Source.find("Headers.Draw(BrowserOpacityColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f))"), std::string::npos);
	EXPECT_NE(Source.find("View.Draw(BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f))"), std::string::npos);
	EXPECT_NE(Source.find("View.Draw(BrowserPanelColor(0.82f)"), std::string::npos);
	EXPECT_NE(Source.find("Tab.Draw(BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f))"), std::string::npos);
	EXPECT_EQ(Source.find("BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.3f))"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DemoBrowserUsesExplicitLegacyShellBranches)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_demo.cpp");
	const std::string RenderDemoBrowser = FunctionBody(Source, "void CMenus::RenderDemoBrowser(CUIRect MainView)");
	const std::string RenderDemoBrowserList = FunctionBody(Source, "void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)");
	const std::string RenderDemoBrowserDetails = FunctionBody(Source, "void CMenus::RenderDemoBrowserDetails(CUIRect DetailsView)");
	const std::string RenderDemoBrowserButtons = FunctionBody(Source, "void CMenus::RenderDemoBrowserButtons(CUIRect ButtonsView, bool WasListboxItemActivated)");
	const size_t UseNewUiButtonsPos = RenderDemoBrowserButtons.find("if(UseNewUi)");
	ASSERT_NE(UseNewUiButtonsPos, std::string::npos);
	const size_t UseNewUiButtonsBodyStart = RenderDemoBrowserButtons.find("{", UseNewUiButtonsPos);
	ASSERT_NE(UseNewUiButtonsBodyStart, std::string::npos);
	const size_t UseNewUiButtonsBodyEnd = MatchingBrace(RenderDemoBrowserButtons, UseNewUiButtonsBodyStart);
	ASSERT_NE(UseNewUiButtonsBodyEnd, std::string::npos);
	const std::string UseNewUiButtonsBranch = RenderDemoBrowserButtons.substr(UseNewUiButtonsBodyStart, UseNewUiButtonsBodyEnd - UseNewUiButtonsBodyStart);
	const size_t LegacyButtonsElsePos = RenderDemoBrowserButtons.find("CUIRect ButtonBarTop, ButtonBarBottom;", UseNewUiButtonsBodyEnd);
	ASSERT_NE(LegacyButtonsElsePos, std::string::npos);
	const std::string LegacyButtonsBranch = RenderDemoBrowserButtons.substr(LegacyButtonsElsePos);

	EXPECT_NE(Source.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(RenderDemoBrowser.find("if(UseNewUi)"), std::string::npos);
	EXPECT_NE(RenderDemoBrowser.find("MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowser.find("MainView.Margin(10.0f, &MainView);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowser.find("MainView.HSplitBottom(44.0f, &ListView, &ButtonsView);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowser.find("MainView.HSplitBottom(22.0f * 2.0f + 5.0f, &ListView, &ButtonsView);"), std::string::npos);
	EXPECT_EQ(RenderDemoBrowser.find("MainView.HSplitBottom(22.0f * 2.0f + 10.0f, &ListView, &ButtonsView);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("ListBox.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("const float HeaderGap = UseNewUi ? 4.0f : 2.0f;"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("const float RowHeight = UseNewUi ? ms_ListheaderHeight + 1.0f : ms_ListheaderHeight;"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("CColumn aCols[] = {"), std::string::npos);
	EXPECT_EQ(RenderDemoBrowserList.find("static CColumn s_aCols[] = {"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("{COL_MARKERS, SORT_MARKERS, FONT_ICON_BOOKMARK, 1, true, UseNewUi ? 34.0f : 30.0f, {0}, Localizable(\"Markers\")}"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("{COL_LENGTH, SORT_LENGTH, Localizable(\"Length\"), 1, false, UseNewUi ? 84.0f : 75.0f, {0}, nullptr}"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("{COL_DATE, SORT_DATE, Localizable(\"Date\"), 1, false, UseNewUi ? 156.0f : 150.0f, {0}, nullptr}"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("aCols[9].m_Width = BrowsingScreenshots ? (UseNewUi ? 176.0f : 170.0f) : (UseNewUi ? 156.0f : 150.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserList.find("s_ListBox.DoStart(UseNewUi ? RowHeight : ms_ListheaderHeight, m_vpFilteredDemos.size(), 1, 3, m_DemolistSelectedIndex, &ListBox, false, IGraphics::CORNER_ALL, true);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserDetails.find("Header.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserDetails.find("Contents.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.15f), IGraphics::CORNER_B, 5.0f);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserDetails.find("Contents.Margin(5.0f, &Contents);"), std::string::npos);
	EXPECT_NE(RenderDemoBrowserButtons.find("if(UseNewUi)"), std::string::npos);
	EXPECT_NE(UseNewUiButtonsBranch.find("CUIRect MainRow = ButtonsView;"), std::string::npos);
	EXPECT_NE(UseNewUiButtonsBranch.find("const float ButtonWidth = MainRow.h * 1.55f;"), std::string::npos);
	EXPECT_NE(UseNewUiButtonsBranch.find("const float RowHeight = minimum(22.0f, ButtonsView.h);"), std::string::npos);
	EXPECT_NE(UseNewUiButtonsBranch.find("ButtonsView.HSplitTop(3.0f, nullptr, &ButtonsView);"), std::string::npos);
	EXPECT_NE(UseNewUiButtonsBranch.find("ButtonsView.HSplitBottom(3.0f, &ButtonsView, nullptr);"), std::string::npos);
	EXPECT_NE(LegacyButtonsBranch.find("ButtonsView.HSplitMid(&ButtonBarTop, &ButtonBarBottom, 5.0f);"), std::string::npos);
	EXPECT_EQ(RenderDemoBrowser.find("MainView.Draw(MenuPanelColor()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BrowserFavoriteMapsEarlyReturnAvoidsLegacyDoubleInset)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string RenderServerbrowser = FunctionBody(Source, "void CMenus::RenderServerbrowser(");
	const size_t FavoriteMapsPos = RenderServerbrowser.find("if(g_Config.m_UiPage == PAGE_FAVORITE_MAPS)");
	ASSERT_NE(FavoriteMapsPos, std::string::npos);
	const size_t DrawPos = RenderServerbrowser.find("View.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);");
	ASSERT_NE(DrawPos, std::string::npos);
	EXPECT_LT(FavoriteMapsPos, DrawPos);
	EXPECT_NE(RenderServerbrowser.find("RenderServerbrowserFavoriteMaps(MainView);"), std::string::npos);
	EXPECT_NE(RenderServerbrowser.find("View.Margin(6.0f, &View);\n\t\t\tRenderServerbrowserFavoriteMaps(View);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, MapHistoryUsesRelativeWorkspaceAndCardLayout)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string RenderFavoriteMaps = FunctionBody(Source, "void CMenus::RenderServerbrowserFavoriteMaps(");

	EXPECT_EQ(RenderFavoriteMaps.find("SplitHistoryPanel"), std::string::npos);
	EXPECT_EQ(RenderFavoriteMaps.find("SplitHistoryColumns"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("s_aFavoriteMapsWorkspaceTabButtons"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("QmMapHistoryUi::WorkspaceMetrics(View.h)"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("QmMapHistoryUi::CardRowHeight(HistoryPanel.h)"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("QmMapHistoryUi::GridColumns(HistoryPanel.w - QmMapHistoryUi::LIST_SCROLLBAR_WIDTH, CardRowHeight)"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("QmMapHistoryUi::StackControls(HistoryPanel.w, Layout.m_ControlHeight)"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("const float ClearAreaWidth = ControlsRow.w * 0.40f;"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("s_MapHistoryListBox.DoStart(CardRowHeight"), std::string::npos);
	EXPECT_NE(RenderFavoriteMaps.find("CUIRect CardHeader, MetricsRow"), std::string::npos);
	EXPECT_EQ(RenderFavoriteMaps.find("QmMapHistoryUi::CARD_ROW_HEIGHT"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmLocalizationEnglishOverlayUsesExplicitEnglishFile)
{
	const std::string Source = ReadTextFile("src/game/client/gameclient.cpp");

	EXPECT_EQ(Source.find("str_format(aBuf, sizeof(aBuf), \"qmclient/%s\", g_Config.m_ClLanguagefile);"), std::string::npos);
	EXPECT_EQ(Source.find("static void LoadQmClientLanguageOverlay("), std::string::npos);
	EXPECT_EQ(Source.find("const char *pQmLanguageFile = g_Config.m_ClLanguagefile[0] != '\\0' ? g_Config.m_ClLanguagefile : \"english.txt\";"), std::string::npos);
	EXPECT_EQ(Source.find("const char *pQmLanguageFile = pLanguageFile[0] != '\\0' ? pLanguageFile : \"english.txt\";"), std::string::npos);
	EXPECT_EQ(Source.find("if(str_comp(pLanguageFile, \"languages/simplified_chinese.txt\") == 0)"), std::string::npos);
	EXPECT_EQ(Source.find("const char *pQmLanguageFile = pLanguageFile[0] != '\\0' ? pLanguageFile : \"languages/english.txt\";"), std::string::npos);
	EXPECT_EQ(Source.find("str_format(aBuf, sizeof(aBuf), \"qmclient/%s\", pQmLanguageFile);"), std::string::npos);
	EXPECT_EQ(Source.find("LoadQmClientLanguageOverlay(g_Localization, g_Config.m_ClLanguagefile, Storage(), Console());"), std::string::npos);
	EXPECT_NE(Source.find("g_Localization.Load(g_Config.m_ClLanguagefile, Storage(), Console());"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmClientTabLabelsDoNotCacheLocalizedPointers)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string RenderSettingsQmClient = FunctionBody(Source, "void CMenus::RenderSettingsQmClient(");

	EXPECT_EQ(RenderSettingsQmClient.find("static const char *s_apQmTabNames"), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClient.find("s_aQmLanguageFile"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("const char *apQmTabNames[NUMBER_OF_QMCLIENT_SETTINGS_TABS] = {};"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("apQmTabNames[QMCLIENT_SETTINGS_TAB_VISUAL] = Localize(\"Visuals\");"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("apQmTabNames[QMCLIENT_SETTINGS_TAB_FUNCTION] = Localize(\"Functions\");"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("apQmTabNames[QMCLIENT_SETTINGS_TAB_HUD] = Localize(\"HUD\");"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("apQmTabNames[QMCLIENT_SETTINGS_TAB_CONTRIBUTORS] = Localize(\"Contributors\");"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("apQmTabNames[QMCLIENT_SETTINGS_TAB_CONFIG] = Localize(\"Config\");"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TranslateTargetRatioDoesNotRenderSkipNotes)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TranslateModule = BlockBodyAfter(Source, "case EQmModuleId::Translate:\n\t\t\t{");

	EXPECT_NE(TranslateModule.find("RenderSliderWithNumberInput(&s_LocalDetectRatioSelectorId"), std::string::npos);
	EXPECT_EQ(TranslateModule.find("qmclient-translate-skip-target-language-note"), std::string::npos);
	EXPECT_EQ(TranslateModule.find("qmclient-translate-skip-numeric-note"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmClientUpdateFlowUsesQmClientNamingAndComparisonHelper)
{
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string TClientHeader = ReadTextFile("src/game/client/components/tclient/tclient.h");
	const std::string MenusStartSource = ReadTextFile("src/game/client/components/menus_start.cpp");

	EXPECT_NE(TClientSource.find("#include <game/client/components/qmclient/update_version.h>"), std::string::npos);
	EXPECT_NE(TClientSource.find("static constexpr const char *QMCLIENT_INFO_URL"), std::string::npos);
	EXPECT_NE(TClientSource.find("static constexpr const char *QMCLIENT_UPDATE_EXE_URL"), std::string::npos);
	EXPECT_NE(TClientSource.find("FetchQmClientUpdateInfo();"), std::string::npos);
	EXPECT_NE(TClientSource.find("FinishQmClientUpdateInfo();"), std::string::npos);
	EXPECT_NE(TClientSource.find("ResetQmClientUpdateInfoTask();"), std::string::npos);
	EXPECT_NE(TClientSource.find("NeedQmClientUpdate()"), std::string::npos);
	EXPECT_NE(TClientSource.find("RequestQmClientUpdateCheckAndUpdate()"), std::string::npos);
	EXPECT_NE(TClientSource.find("IsQmClientRemoteVersionNewer(pLatestVersion, QMCLIENT_VERSION)"), std::string::npos);
	EXPECT_EQ(TClientSource.find("NeedUpdate()"), std::string::npos);
	EXPECT_EQ(TClientSource.find("FetchTClientInfo()"), std::string::npos);
	EXPECT_EQ(TClientSource.find("FinishTClientInfo()"), std::string::npos);
	EXPECT_EQ(TClientSource.find("ResetTClientInfoTask()"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TCLIENT_INFO_URL"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TCLIENT_UPDATE_EXE_URL"), std::string::npos);

	EXPECT_NE(TClientHeader.find("m_pQmClientUpdateInfoTask"), std::string::npos);
	EXPECT_NE(TClientHeader.find("m_FetchedQmClientUpdateInfo"), std::string::npos);
	EXPECT_NE(TClientHeader.find("m_QmClientAutoUpdateAfterCheck"), std::string::npos);
	EXPECT_NE(TClientHeader.find("m_aQmClientLatestVersionStr"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("m_pTClientInfoTask"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("m_FetchedTClientInfo"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("m_AutoUpdateAfterCheck"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("m_aVersionStr"), std::string::npos);

	EXPECT_NE(MenusStartSource.find("m_FetchedQmClientUpdateInfo"), std::string::npos);
	EXPECT_NE(MenusStartSource.find("NeedQmClientUpdate()"), std::string::npos);
	EXPECT_EQ(MenusStartSource.find("m_FetchedTClientInfo"), std::string::npos);
	EXPECT_EQ(MenusStartSource.find("NeedUpdate()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, StartMenuKeepsExplicitUseV2AndLegacyButtonPaths)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_start.cpp");
	const std::string RenderStartMenuImpl = FunctionBody(Source, "void CMenusStart::RenderStartMenuImpl(");
	const std::string UseV2Block = BlockBodyAfter(RenderStartMenuImpl, "if(UseV2Layout)");

	EXPECT_NE(Source.find("void CMenusStart::RenderStartMenu(CUIRect MainView)"), std::string::npos);
	EXPECT_NE(Source.find("RenderStartMenuImpl(MainView, false);"), std::string::npos);
	EXPECT_NE(Source.find("void CMenusStart::RenderStartMenuV2(CUIRect MainView)"), std::string::npos);
	EXPECT_NE(Source.find("RenderStartMenuImpl(MainView, true);"), std::string::npos);
	EXPECT_NE(RenderStartMenuImpl.find("if(UseV2Layout)"), std::string::npos);
	EXPECT_NE(UseV2Block.find("ui_widget::PrimaryButton"), std::string::npos);
	EXPECT_NE(UseV2Block.find("ui_widget::SecondaryButton"), std::string::npos);
	EXPECT_EQ(UseV2Block.find("DoButton_Menu("), std::string::npos);
	EXPECT_NE(RenderStartMenuImpl.find("static float s_aMenuButtonScale[MenuButtonCount] = {};"), std::string::npos);
	EXPECT_NE(RenderStartMenuImpl.find("const auto ScaleButtonRect = [](const CUIRect &Base, float Scale) {"), std::string::npos);
	EXPECT_NE(RenderStartMenuImpl.find("GameClient()->m_Menus.DoButton_Menu(&s_QuitButton"), std::string::npos);
	EXPECT_NE(RenderStartMenuImpl.find("GameClient()->m_Menus.DoButton_Menu(&s_PlayButton"), std::string::npos);
}

TEST(QmNewUiMenuBranches, StartMenuEntryKeepsLegacyStartPageWithQmNewUi)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	EXPECT_NE(Source.find("else if(m_ShowStart)"), std::string::npos);
	const std::string Render = FunctionBody(Source, "void CMenus::Render()");
	const size_t StartMenuPos = Render.find("else if(m_ShowStart)");
	ASSERT_NE(StartMenuPos, std::string::npos);
	const size_t StartMenuBodyStart = Render.find("{", StartMenuPos);
	ASSERT_NE(StartMenuBodyStart, std::string::npos);
	const size_t StartMenuBodyEnd = MatchingBrace(Render, StartMenuBodyStart);
	ASSERT_NE(StartMenuBodyEnd, std::string::npos);
	const std::string StartMenuBlock = Render.substr(StartMenuBodyStart, StartMenuBodyEnd - StartMenuBodyStart);
	EXPECT_NE(StartMenuBlock.find("m_MenusStart.RenderStartMenu(Screen);"), std::string::npos);
	EXPECT_EQ(StartMenuBlock.find("m_MenusStart.RenderStartMenuV2(Screen);"), std::string::npos);
	EXPECT_EQ(StartMenuBlock.find("g_Config.m_QmNewUi"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsShellKeepsExplicitQmNewUiContainerBranch)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettings = FunctionBody(Source, "void CMenus::RenderSettings(CUIRect MainView)");
	const size_t UseNewSettingsUiIfPos = RenderSettings.find("if(UseNewSettingsUi)");
	ASSERT_NE(UseNewSettingsUiIfPos, std::string::npos);
	const size_t UseNewSettingsUiBodyStart = RenderSettings.find("{", UseNewSettingsUiIfPos);
	ASSERT_NE(UseNewSettingsUiBodyStart, std::string::npos);
	const size_t UseNewSettingsUiBodyEnd = MatchingBrace(RenderSettings, UseNewSettingsUiBodyStart);
	ASSERT_NE(UseNewSettingsUiBodyEnd, std::string::npos);
	const std::string UseNewSettingsUiBlock = RenderSettings.substr(UseNewSettingsUiBodyStart, UseNewSettingsUiBodyEnd - UseNewSettingsUiBodyStart);
	const size_t OldSettingsUiElsePos = RenderSettings.find("else", UseNewSettingsUiBodyEnd);
	ASSERT_NE(OldSettingsUiElsePos, std::string::npos);
	const size_t OldSettingsUiBodyStart = RenderSettings.find("{", OldSettingsUiElsePos);
	ASSERT_NE(OldSettingsUiBodyStart, std::string::npos);
	const size_t OldSettingsUiBodyEnd = MatchingBrace(RenderSettings, OldSettingsUiBodyStart);
	ASSERT_NE(OldSettingsUiBodyEnd, std::string::npos);
	const std::string OldSettingsUiBlock = RenderSettings.substr(OldSettingsUiBodyStart, OldSettingsUiBodyEnd - OldSettingsUiBodyStart);
	const std::string SettingsHeaderBranch = BlockBodyAfter(RenderSettings, "if(UseNewSettingsUi)\n\t{\n\t\tTabBar.Margin(10.0f, &TabBar);");
	const std::string SettingsHeaderLegacyBranch = BlockBodyAfter(RenderSettings, "else\n\t{\n\t\tTabBar.HSplitTop(50.0f, &Button, &TabBar);");

	EXPECT_NE(Source.find("const bool UseNewSettingsUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(UseNewSettingsUiBlock.find("TabBar.Draw(SettingsTabbarColor()"), std::string::npos);
	EXPECT_NE(UseNewSettingsUiBlock.find("MainView.Draw(MenuPanelColor()"), std::string::npos);
	EXPECT_EQ(UseNewSettingsUiBlock.find("MainView.Draw(ms_ColorTabbarActive"), std::string::npos);
	EXPECT_NE(OldSettingsUiBlock.find("MainView.Draw(ms_ColorTabbarActive"), std::string::npos);
	EXPECT_EQ(OldSettingsUiBlock.find("SettingsTabbarColor()"), std::string::npos);
	EXPECT_EQ(OldSettingsUiBlock.find("MenuPanelColor()"), std::string::npos);
	EXPECT_EQ(SettingsHeaderBranch.find("Button.Draw(ms_ColorTabbarActive"), std::string::npos);
	EXPECT_NE(SettingsHeaderLegacyBranch.find("Button.Draw(ms_ColorTabbarActive"), std::string::npos);
}

TEST(QmNewUiMenuBranches, LegacyMenusKeepTabAndPanelShellConnected)
{
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string MenuShellSplit = "const bool UseNewUi = g_Config.m_QmNewUi != 0;\n\t\t\tScreen.HSplitTop(MenuMenubarHeight(UseNewUi), &TabBar, &MainView);\n\t\t\tif(UseNewUi)\n\t\t\t\tMainView.HSplitTop(6.0f, nullptr, &MainView);";
	EXPECT_NE(MenusSource.find("constexpr float MENU_MENUBAR_HEIGHT_NEW = 24.0f;"), std::string::npos);
	EXPECT_NE(MenusSource.find("constexpr float MENU_MENUBAR_HEIGHT_LEGACY = 30.0f;"), std::string::npos);
	EXPECT_NE(MenusSource.find("constexpr float MenuMenubarHeight(bool UseNewUi)"), std::string::npos);
	EXPECT_NE(MenusSource.find(MenuShellSplit), std::string::npos);
	EXPECT_NE(MenusSource.find("case IClient::STATE_ONLINE:"), std::string::npos);
	EXPECT_NE(MenusSource.find(MenuShellSplit, MenusSource.find("case IClient::STATE_ONLINE:")), std::string::npos);

	const std::string QmClientSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	EXPECT_NE(QmClientSource.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(QmClientSource.find("if(UseNewUi)\n\t\t\tMainView.HSplitTop(Margin, nullptr, &MainView);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, AssetsPreviewUsesInnerFrameRectForPreviewImage)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings_assets.cpp");

	EXPECT_NE(Source.find("auto DrawPreviewFrame = [&](const CUIRect &TextureRect) -> CUIRect {"), std::string::npos);
	EXPECT_NE(Source.find("PreviewFrame.Margin(3.0f, &PreviewFrame);"), std::string::npos);
	EXPECT_NE(Source.find("return PreviewFrame;"), std::string::npos);
	EXPECT_NE(Source.find("auto ComputeAssetPreviewContentSize = [&](bool WorkshopCard)"), std::string::npos);
	EXPECT_NE(Source.find("CUIRect PreviewFrameRect = DrawPreviewFrame(Shell.m_TextureRect);"), std::string::npos);
	EXPECT_NE(Source.find("const auto [PreviewContentWidth, PreviewContentHeight] = ComputeAssetPreviewContentSize(WorkshopCard);"), std::string::npos);
	EXPECT_NE(Source.find("const auto [PreviewContentWidth, PreviewContentHeight] = ComputeAssetPreviewContentSize(true);"), std::string::npos);
	EXPECT_NE(Source.find("const CUIRect PreviewRect = ComputePreviewDrawRect(PreviewFrameRect, PreviewContentWidth, PreviewContentHeight);"), std::string::npos);
	EXPECT_EQ(Source.find("const CUIRect PreviewRect = ComputePreviewDrawRect(HeaderLayout.m_TextureRect, TextureWidth, TextureHeight);"), std::string::npos);
	EXPECT_EQ(Source.find("const CUIRect PreviewRect = ComputePreviewDrawRect(HeaderLayout.m_TextureRect, TextureWidth, TextureWidth);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BetterScoreboardSettingIsOptInLocalizedAndVersioned)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t MiniFeaturesCase = MenusSource.rfind("case EQmModuleId::MiniFeatures:");
	ASSERT_NE(MiniFeaturesCase, std::string::npos);
	const std::string MiniFeatures = BlockBodyAfter(MenusSource.substr(MiniFeaturesCase), "case EQmModuleId::MiniFeatures:");
	const std::string MenusToml = ReadTextFile("qmclient_scripts/languages_qmclient/translations/i18n/menus.toml");
	const std::string VersionSource = ReadTextFile("src/game/version.h");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmBetterScoreboard, qm_better_scoreboard, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_NE(MiniFeatures.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmBetterScoreboard, \"Better scoreboard\", Localize(\"Better scoreboard\"), &g_Config.m_QmBetterScoreboard, &Row, LgLineHeight);"), std::string::npos);
	EXPECT_NE(MenusToml.find("key = \"Better scoreboard\""), std::string::npos);
	EXPECT_NE(MenusToml.find("simplified_chinese = \"更好的计分板\""), std::string::npos);
	EXPECT_NE(VersionSource.find("#define QMCLIENT_VERSION \"2.79.21\""), std::string::npos);
}

TEST(QmNewUiMenuBranches, SwitchCountdownIsAnIndependentHudModuleWithCompatibleDefaults)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Translations = ReadTextFile("qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml");
	const std::string SwitchModule = BlockBodyAfter(MenusSource, "case EQmModuleId::SwitchCountdown:\n\t\t\t{");
	const std::string DynamicIslandModule = BlockBodyAfter(MenusSource, "case EQmModuleId::DynamicIsland:\n\t\t\t{");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSwitchCountdown, qm_switch_countdown, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSwitchCountdownMode, qm_switch_countdown_mode, 1, 0, 2"), std::string::npos);
	EXPECT_NE(MenusSource.find("{EQmModuleId::SwitchCountdown, EQmModuleColumn::Right"), std::string::npos);
	EXPECT_NE(SwitchModule.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmSwitchCountdown"), std::string::npos);
	EXPECT_NE(SwitchModule.find("if(g_Config.m_QmSwitchCountdown || PrewarmOnly)"), std::string::npos);
	EXPECT_EQ(SwitchModule.find("DoSettingsLine_RadioMenu"), std::string::npos);
	EXPECT_NE(SwitchModule.find("DoQmSettingsCheckboxAuto(&s_SwitchCountdownFollowTeeId"), std::string::npos);
	EXPECT_NE(SwitchModule.find("DoQmSettingsCheckboxAuto(&s_SwitchCountdownMediaIslandId"), std::string::npos);
	EXPECT_NE(SwitchModule.find("QmHudSwitchCountdownModeFromLocations"), std::string::npos);
	EXPECT_NE(SwitchModule.find("Localize(\"Follow Tee\")"), std::string::npos);
	EXPECT_NE(SwitchModule.find("Localize(\"In Dynamic Island\")"), std::string::npos);
	EXPECT_EQ(DynamicIslandModule.find("m_QmSwitchCountdown"), std::string::npos);
	EXPECT_NE(Translations.find("key = \"Switch countdown\""), std::string::npos);
	EXPECT_NE(Translations.find("simplified_chinese = \"开关倒计时\""), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsColorLabelsUseQmLocalizedKeys)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string MenusToml = ReadTextFile("qmclient_scripts/languages_qmclient/translations/i18n/menus.toml");

	EXPECT_EQ(Source.find("Localize(\"UI Color\")"), std::string::npos);
	EXPECT_EQ(Source.find("Localize(\"Menu panel color\")"), std::string::npos);
	EXPECT_EQ(Source.find("Localize(\"Menu panel opacity\")"), std::string::npos);
	EXPECT_EQ(Source.find("Localize(\"Menu panel elevated opacity\")"), std::string::npos);
	EXPECT_EQ(Source.find("s_MenuPanelColorResetId"), std::string::npos);
	EXPECT_EQ(Source.find("g_Config.m_ClMenuPanelColor"), std::string::npos);
	EXPECT_EQ(Source.find("g_Config.m_UiColor"), std::string::npos);
	EXPECT_NE(Source.find("DoLine_ColorPicker(&s_UiColorResetId"), std::string::npos);
	EXPECT_NE(Source.find("DoLine_ColorPicker(&s_MapBrowserColorResetId"), std::string::npos);
	EXPECT_NE(Source.find("DoLine_ColorPicker(&s_ScoreboardColorResetId"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmUiColor"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmMapBrowserColor"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmScoreboardColor"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmUiOpacity"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmMapBrowserOpacity"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmScoreboardOpacity"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"UI color\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Map browser color\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Scoreboard color\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"UI opacity\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Map browser opacity\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Scoreboard opacity\")"), std::string::npos);
	EXPECT_NE(MenusToml.find("key = \"UI opacity\""), std::string::npos);
	EXPECT_NE(MenusToml.find("simplified_chinese = \"界面不透明度\""), std::string::npos);
	EXPECT_NE(MenusToml.find("key = \"Map browser opacity\""), std::string::npos);
	EXPECT_NE(MenusToml.find("simplified_chinese = \"地图浏览器不透明度\""), std::string::npos);
	EXPECT_NE(MenusToml.find("key = \"Scoreboard opacity\""), std::string::npos);
	EXPECT_NE(MenusToml.find("simplified_chinese = \"计分板不透明度\""), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsGraphicsOpacitySlidersExposeIndependentUiDomains)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");

	EXPECT_NE(Source.find("DoSliderWithValueInput(&g_Config.m_QmUiOpacity, &g_Config.m_QmUiOpacity, Button, Localize(\"UI opacity\")"), std::string::npos);
	EXPECT_NE(Source.find("DoSliderWithValueInput(&g_Config.m_QmMapBrowserOpacity, &g_Config.m_QmMapBrowserOpacity, Button, Localize(\"Map browser opacity\")"), std::string::npos);
	EXPECT_NE(Source.find("DoSliderWithValueInput(&g_Config.m_QmScoreboardOpacity, &g_Config.m_QmScoreboardOpacity, Button, Localize(\"Scoreboard opacity\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DefaultUiSurfacesUseBlackThirtyPercent)
{
	const std::string QmConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables.h");

	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_COL(QmUiColor, qm_ui_color, 0x000000"), std::string::npos);
	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_COL(QmMapBrowserColor, qm_map_browser_color, 0x000000"), std::string::npos);
	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_COL(QmScoreboardColor, qm_scoreboard_color, 0x000000"), std::string::npos);
	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_INT(QmUiOpacity, qm_ui_opacity, 30"), std::string::npos);
	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_INT(QmMapBrowserOpacity, qm_map_browser_opacity, 30"), std::string::npos);
	EXPECT_NE(QmConfigSource.find("MACRO_CONFIG_INT(QmScoreboardOpacity, qm_scoreboard_opacity, 30"), std::string::npos);

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_COL(UiColor, ui_color, 0x4D000000"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_COL(ClMenuPanelColor, cl_menu_panel_color, 0x000000"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(ClMenuPanelOpacity, cl_menu_panel_opacity, 30"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(ClMenuPanelElevatedOpacity, cl_menu_panel_elevated_opacity, 30"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(ClSettingsTabbarOpacity, cl_settings_tabbar_opacity, 30"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmFeatureDefaultsAreDisabledExceptRequiredLyricsDefaults)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::regex BinaryQmDefaultOn(R"(MACRO_CONFIG_INT\([^,]+,\s*qm_[^,]+,\s*1,\s*0,\s*1,)");
	std::istringstream Lines(ConfigSource);
	std::string Line;
	while(std::getline(Lines, Line))
	{
		if(Line.find("MACRO_CONFIG_INT(QmLyricsSearchType,") != std::string::npos ||
			Line.find("MACRO_CONFIG_INT(QmLyricsCacheEnable,") != std::string::npos ||
			Line.find("MACRO_CONFIG_INT(QmHudIslandShowTuneZoneEffects,") != std::string::npos ||
			Line.find("MACRO_CONFIG_INT(QmSwitchCountdown,") != std::string::npos ||
			Line.find("MACRO_CONFIG_INT(QmSwitchCountdownMode,") != std::string::npos)
			continue;
		EXPECT_FALSE(std::regex_search(Line, BinaryQmDefaultOn)) << Line;
	}

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmLyricsSearchType, qm_lyrics_search_type, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmLyricsCacheEnable, qm_lyrics_cache_enable, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmHudIslandShowTuneZoneEffects, qm_hud_island_show_tune_zone_effects, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSwitchCountdown, qm_switch_countdown, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSwitchCountdownMode, qm_switch_countdown_mode, 1, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmUiMotionLevel, qm_ui_motion_level, 0, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponTrajectory, qm_weapon_trajectory, 0, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmVoiceNoiseSuppressEnable, qm_voice_noise_suppress_enable, 0, 0, 2"), std::string::npos);
	EXPECT_EQ(ConfigSource.find("MACRO_CONFIG_INT(QmUiMotionLevel, qm_ui_motion_level, 2, 0, 2"), std::string::npos);
	EXPECT_EQ(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponTrajectory, qm_weapon_trajectory, 1, 0, 2"), std::string::npos);
	EXPECT_EQ(ConfigSource.find("MACRO_CONFIG_INT(QmVoiceNoiseSuppressEnable, qm_voice_noise_suppress_enable, 2, 0, 2"), std::string::npos);
}

TEST(QmNewUiMenuBranches, LyricsAdvancedToggleUsesLyricsSpecificChineseTranslation)
{
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Translations = ReadTextFile("qmclient_scripts/languages_qmclient/translations/i18n/qmclient.toml");
	const size_t LyricsAdvancedContext = Translations.find("context = \"Lyrics HUD\"");
	const size_t LyricsAdvancedTranslation = Translations.find("simplified_chinese = \"高级\"", LyricsAdvancedContext);
	const size_t NextMessage = Translations.find("[[message]]", LyricsAdvancedContext + 1);

	EXPECT_NE(MenusSource.find("Localize(\"Advanced\", \"Lyrics HUD\")"), std::string::npos);
	ASSERT_NE(LyricsAdvancedContext, std::string::npos);
	ASSERT_NE(LyricsAdvancedTranslation, std::string::npos);
	ASSERT_NE(NextMessage, std::string::npos);
	EXPECT_LT(LyricsAdvancedTranslation, NextMessage);
}

TEST(QmNewUiMenuBranches, SkinTransitionTeeHueControlsDoNotRenderExplanatoryNotes)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string SkinTransitionModule = BlockBodyAfter(Source, "case EQmModuleId::SkinTransition:\n\t\t\t{");

	EXPECT_NE(SkinTransitionModule.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmCycleTeeHue"), std::string::npos);
	EXPECT_NE(SkinTransitionModule.find("RenderSliderWithValueInput(&s_QmCycleTeeHueSpeedInputId"), std::string::npos);
	EXPECT_EQ(SkinTransitionModule.find("qmclient-cycle-tee-hue-custom-note"), std::string::npos);
	EXPECT_EQ(SkinTransitionModule.find("qmclient-cycle-tee-hue-tclient-note"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SkinTransitionAnimationToggleOwnsAdvancedControls)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string GameClientSource = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string PlayersSource = ReadTextFile("src/game/client/components/players.cpp");
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string LanguageSource = ReadTextFile("data/languages/simplified_chinese.txt");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSkinChangeTransition, qm_skin_change_transition, 0, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmCycleTeeHueDummy, qm_cycle_tee_hue_dummy, 0, 0, 1"), std::string::npos);
	EXPECT_NE(MenusSource.find("pSkinTransitionAnimationFeatureId = \"qm_2_72_0_skin_transition_animation_toggle\""), std::string::npos);
	EXPECT_NE(MenusSource.find("pSkinTransitionAnimationFeatureId,\n\t\t\t\t\t\t\"qm_2_62_8_weapon_animation\""), std::string::npos);
	EXPECT_NE(MenusSource.find("return \"皮肤切换 pifu qiehuan skin transition 换皮 huanpi 动画 donghua 开关 kaiguan 类型 leixing 时长 shichang 锤中偷皮 chuizhong toupi Tee外观 tee waiguan 循环色调 xunhuan sediao hue 速度 sudu 分身 fenshen dummy\";"), std::string::npos);
	EXPECT_NE(MenusSource.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmCycleTeeHueDummy, \"Also apply to dummy\""), std::string::npos);
	EXPECT_NE(PlayersSource.find("LocalDummy == 0 || g_Config.m_QmCycleTeeHueDummy != 0"), std::string::npos);
	EXPECT_NE(PlayersSource.find("LocalDummy != 0 ? g_Config.m_ClDummyUseCustomColor != 0 : g_Config.m_ClPlayerUseCustomColor != 0"), std::string::npos);

	const size_t Toggle = MenusSource.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmSkinChangeTransition");
	ASSERT_NE(Toggle, std::string::npos);
	const size_t AdvancedIf = MenusSource.find("if(g_Config.m_QmSkinChangeTransition)", Toggle);
	const size_t TypeLabel = MenusSource.find("Localize(\"Skin transition type\")", Toggle);
	const size_t DurationLabel = MenusSource.find("Localize(\"Skin transition duration\")", Toggle);
	ASSERT_NE(AdvancedIf, std::string::npos);
	ASSERT_NE(TypeLabel, std::string::npos);
	ASSERT_NE(DurationLabel, std::string::npos);
	EXPECT_LT(Toggle, AdvancedIf);
	EXPECT_LT(AdvancedIf, TypeLabel);
	EXPECT_LT(TypeLabel, DurationLabel);

	EXPECT_NE(GameClientSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0)"), std::string::npos);
	EXPECT_NE(GameClientSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0 || !m_SkinTransitionStart.has_value()"), std::string::npos);
	EXPECT_NE(SettingsSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0)"), std::string::npos);
	EXPECT_NE(SettingsSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0 || !m_StartTime.has_value()"), std::string::npos);
	EXPECT_NE(LanguageSource.find("Skin transition animation\n== 皮肤切换动画"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplateOthersModeSuppressesLocalIdentityRows)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string UpdateCoordXAlignFrameState = FunctionBody(Source, "void CNamePlates::UpdateCoordXAlignFrameState");
	const std::string RenderNamePlateGame = FunctionBody(Source, "void CNamePlates::RenderNamePlateGame");

	EXPECT_EQ(UpdateCoordXAlignFrameState.find("FrameState.m_LocalRoundedX = RoundCoordToCentitiles(GameClient()->m_LocalCharacterPos.x / 32.0f);\n\tFrameState.m_LocalAligned = true;"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("std::array<SCoordXAlignReference, NUM_DUMMIES> aLocalRefs{};"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("if(aLocalRefs[i].m_ClientId == ClientId)"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("CoordXAlignState.m_ReferenceClientId != ReferenceClientId"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("FrameState.m_aClientAligned[ReferenceClientId] = true;"), std::string::npos);
	EXPECT_EQ(UpdateCoordXAlignFrameState.find("if(ClientId == FrameState.m_LocalClientId"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("if(CoordXAlignState.m_Aligned)\n\t\t{"), std::string::npos);
	EXPECT_NE(UpdateCoordXAlignFrameState.find("FrameState.m_LocalAligned = true;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("const bool IsAnyLocalClient = GameClient()->IsLocalClientId(ClientId);"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("IsAnyLocalClient &&\n\t\tm_pData->m_CoordXAlignFrame.m_aClientAligned[ClientId];"), std::string::npos);
	EXPECT_EQ(RenderNamePlateGame.find("CoordXAlignState.m_Aligned || m_pData->m_CoordXAlignFrame.m_LocalAligned"), std::string::npos);
	EXPECT_EQ(RenderNamePlateGame.find("IsLocalClient &&\n\t\tm_pData->m_CoordXAlignFrame.m_LocalAligned"), std::string::npos);
	EXPECT_EQ(RenderNamePlateGame.find("const bool OwnNameplateScopeVisible"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowName = pPlayerInfo->m_Local ? g_Config.m_ClNamePlatesOwn : g_Config.m_ClNamePlates;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds) && !HideIdentity;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan && !HideIdentity;"), std::string::npos);
	EXPECT_EQ(RenderNamePlateGame.find("const bool NameplateScopeAllowsCoords"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("const bool CoordModuleAllowsCoords = IsAnyLocalClient ? g_Config.m_QmNameplateCoordsOwn : g_Config.m_QmNameplateCoords;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("const bool ShowLocalAlignedCoordX = CoordModuleAllowsCoords && CoordXAlignHintEnabled && LocalCoordXAligned;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowCoordX = (CoordModuleAllowsCoords && g_Config.m_QmNameplateCoordX != 0) || ShowLocalAlignedCoordX;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowCoordY = CoordModuleAllowsCoords && g_Config.m_QmNameplateCoordY != 0;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowCoords = CoordModuleAllowsCoords || ShowLocalAlignedCoordX;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("TrackCoordXAlign &&\n\t\tGameClient()->m_Snap.m_LocalClientId >= 0"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_CoordXAligned = IsAnyLocalClient ? LocalCoordXAligned : CoordXAlignState.m_Aligned;"), std::string::npos);
	EXPECT_EQ(RenderNamePlateGame.find("!IsAnyLocalClient &&\n\t\tGameClient()->m_Snap.m_LocalClientId >= 0"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("if(Data.m_ShowName && !HideIdentity && g_Config.m_TcWarList && g_Config.m_TcWarListShowClan"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_Local = pPlayerInfo->m_Local;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DeveloperBadgePrecedesInlineClientIdAndNameWithoutOverridingIdSettings)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string AddNameRow = FunctionBody(Source, "void AddNameRow(");
	const std::string RenderNamePlateGame = FunctionBody(Source, "void CNamePlates::RenderNamePlateGame");

	const size_t FriendMark = AddNameRow.find("AddPart<CNamePlatePartFriendMark>(This);");
	const size_t Developer = AddNameRow.find("AddPart<CNamePlatePartDeveloper>(This);");
	const size_t InlineClientId = AddNameRow.find("AddPart<CNamePlatePartClientId>(This, false);");
	const size_t Name = AddNameRow.find("AddPart<CNamePlatePartName>(This);");
	ASSERT_NE(FriendMark, std::string::npos);
	ASSERT_NE(Developer, std::string::npos);
	ASSERT_NE(InlineClientId, std::string::npos);
	ASSERT_NE(Name, std::string::npos);
	EXPECT_LT(FriendMark, Developer);
	EXPECT_LT(Developer, InlineClientId);
	EXPECT_LT(InlineClientId, Name);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds) && !HideIdentity;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplatePreviewShowsPlayerStrongHookMarker)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RenderNamePlatePreview = FunctionBody(Source, "void CNamePlates::RenderNamePlatePreview");

	EXPECT_NE(RenderNamePlatePreview.find("const bool IsPlayerPreview = IsOwnPreview;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_HookStrongWeakState = IsPlayerPreview ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeak = NameplateScopeAllowsPreview && g_Config.m_ClNamePlatesStrong > 0;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("if(DummyIdx == g_Config.m_ClDummy)"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplateStrongHookRowReservesLayoutWithoutContentWidth)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RangeSize = FunctionBody(Source, "vec2 RangeSize(");
	const std::string AddHookRow = FunctionBody(Source, "void AddHookRow(");
	const std::string RenderNamePlateGame = FunctionBody(Source, "void CNamePlates::RenderNamePlateGame");

	EXPECT_NE(Source.find("bool m_ReserveHookStrongWeakRow;"), std::string::npos);
	EXPECT_NE(Source.find("bool m_ReserveLineHeight = false;"), std::string::npos);
	EXPECT_NE(Source.find("bool ReserveLineHeight() const { return m_ReserveLineHeight; }"), std::string::npos);
	EXPECT_NE(Source.find("class CNamePlatePartHookStrongWeakRowReserve"), std::string::npos);
	EXPECT_NE(RangeSize.find("else if(Part.ReserveLineHeight())\n\t\t\t{"), std::string::npos);
	EXPECT_NE(RangeSize.find("LineSize.y = std::max(LineSize.y, Part.Size().y + Part.Padding().y);"), std::string::npos);
	EXPECT_NE(AddHookRow.find("AddPart<CNamePlatePartHookStrongWeakRowReserve>(This);"), std::string::npos);
	EXPECT_LT(AddHookRow.find("AddPart<CNamePlatePartHookStrongWeakRowReserve>(This);"), AddHookRow.find("AddPart<CNamePlatePartHookStrongWeak>(This);"));
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ReserveHookStrongWeakRow = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong > 0;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowHookStrongWeak = false;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowHookStrongWeak = g_Config.m_Debug || g_Config.m_ClNamePlatesStrong > 0;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplatePreviewNameScopeGatesPlateExceptDirectionKeys)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RenderNamePlatePreview = FunctionBody(Source, "void CNamePlates::RenderNamePlatePreview");

	EXPECT_NE(RenderNamePlatePreview.find("const bool IsOwnPreview = DummyIdx == 0;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("const bool NameplateScopeAllowsPreview = ForceNameplateScopeAll || (IsOwnPreview ? g_Config.m_ClNamePlatesOwn : g_Config.m_ClNamePlates);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("const bool CoordModuleAllowsPreview = IsOwnPreview ? g_Config.m_QmNameplateCoordsOwn : g_Config.m_QmNameplateCoords;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowName = NameplateScopeAllowsPreview;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowClientId = Data.m_ShowName && (g_Config.m_Debug || g_Config.m_ClNamePlatesIds);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowClan = Data.m_ShowName && g_Config.m_ClNamePlatesClan;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowCoords = CoordModuleAllowsPreview;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowCoordX = Data.m_ShowCoords && g_Config.m_QmNameplateCoordX != 0;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowCoordY = Data.m_ShowCoords && g_Config.m_QmNameplateCoordY != 0;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("case 1: // Others\n\t\t\tData.m_ShowDirection = !IsOwnPreview;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("case 2: // Everyone\n\t\t\tData.m_ShowDirection = true;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("case 3: // Only self\n\t\t\tData.m_ShowDirection = IsOwnPreview;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeakId = NameplateScopeAllowsPreview && g_Config.m_ClNamePlatesStrong == 2;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowName = g_Config.m_ClNamePlates || g_Config.m_ClNamePlatesOwn;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowDirection = NameplateScopeAllowsPreview && !IsOwnPreview;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowDirection = NameplateScopeAllowsPreview;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowDirection = NameplateScopeAllowsPreview && IsOwnPreview;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowDirection = g_Config.m_ClShowDirection != 0 ? true : false;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplatePreviewUsesFullScopeReferenceFrame)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RenderNamePlatePreview = FunctionBody(Source, "void CNamePlates::RenderNamePlatePreview");

	EXPECT_NE(RenderNamePlatePreview.find("auto BuildPreviewData = [&](int DummyIdx, CNamePlateData &Data, bool ForceNameplateScopeAll = false)"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("BuildPreviewData(Dummy, Data);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("BuildPreviewData(Dummy, FrameData, true);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("BuildPreviewData(Dummy == 0 ? 1 : 0, OtherFrameData, true);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("pFrameNamePlate->ComputeBaselineFrame(NameplateBottomMiddle"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("NamePlate.CollectCoreRowRects(Position, aEditorRects, pFrameNamePlate);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("DragHasRow, DragRowCenter, DragRowSize, pFrameNamePlate);"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("NamePlate.Render(*GameClient(), Position, pFrameNamePlate);"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("NamePlate.ComputeBaselineFrame(NameplateBottomMiddle"), std::string::npos);
	EXPECT_NE(Source.find("LayoutCoreRowSize(const SCoreRowParts &CoreRow, const CNamePlate *pLayoutReference) const"), std::string::npos);
	EXPECT_NE(Source.find("Position.y -= LayoutSize.y;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NameplateGameUsesFullScopeReferenceFrame)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RenderNamePlateGame = FunctionBody(Source, "void CNamePlates::RenderNamePlateGame");
	const std::string ResetNamePlates = FunctionBody(Source, "void CNamePlates::ResetNamePlates");

	EXPECT_NE(Source.find("CNamePlate m_aNamePlateFrameReferences[MAX_CLIENTS];"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("CNamePlate *pLayoutReference = nullptr;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("if(Alpha > 0.0f && NameplateFreeMoveEnabled() && (!g_Config.m_ClNamePlates || !g_Config.m_ClNamePlatesOwn))"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("CNamePlateData FrameData = Data;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("FrameData.m_ShowName = true;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("const bool FrameShowLocalAlignedCoordX = CoordModuleAllowsCoords && CoordXAlignHintEnabled && LocalCoordXAligned;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("FrameData.m_ShowCoords = CoordModuleAllowsCoords || FrameShowLocalAlignedCoordX;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("CNamePlate &FrameNamePlate = m_pData->m_aNamePlateFrameReferences[ClientId];"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("FrameNamePlate.Update(*GameClient(), FrameData);"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("pLayoutReference = &FrameNamePlate;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("NamePlate.Render(*GameClient(), Position - vec2(0.0f, (float)g_Config.m_ClNamePlatesOffset), pLayoutReference);"), std::string::npos);
	EXPECT_NE(ResetNamePlates.find("for(CNamePlate &NamePlate : m_pData->m_aNamePlateFrameReferences)\n\t\tNamePlate.Reset(*GameClient());"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowDirection = !pPlayerInfo->m_Local;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowDirection = true;"), std::string::npos);
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowDirection = pPlayerInfo->m_Local;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, MediaIslandLyricsUsesKaraokeRenderer)
{
	const std::string HudSource = ReadTextFile("src/game/client/components/hud.cpp");
	const std::string LyricsHeader = ReadTextFile("src/game/client/components/qmclient/qm_lyrics/qm_lyrics.h");
	const std::string LyricsSource = ReadTextFile("src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp");
	const std::string RenderMediaIsland = FunctionBody(HudSource, "void CHud::RenderMediaIsland()");
	const std::string RenderMediaIslandLine = FunctionBody(LyricsSource, "bool CQmLyrics::RenderMediaIslandLine");

	EXPECT_NE(LyricsHeader.find("bool RenderMediaIslandLine(const CUIRect &Rect, float FontSize, float Alpha);"), std::string::npos);
	EXPECT_NE(RenderMediaIsland.find("GameClient()->m_QmLyrics.RenderMediaIslandLine(LyricsRect, BottomFontSize, VisibleBottomAlpha)"), std::string::npos);
	EXPECT_NE(RenderMediaIslandLine.find("DrawKaraokeLine(TextRender(), Graphics(), m_pImpl->m_Track.m_vLines[Active], NowMs, Rect, FontSize, TextY, Played, Unplayed, Opacity);"), std::string::npos);
	EXPECT_NE(RenderMediaIslandLine.find("QmLyrics::ResolveDisplayLineIndex(m_pImpl->m_Track, m_pImpl->m_ActiveLineIndex, NowMs);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, HudNotificationsKeepEdgeGeometryStableDuringSlide)
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

TEST(QmNewUiMenuBranches, SpectatorSpecTeeDoesNotFallbackToMissingSkin)
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

TEST(QmNewUiMenuBranches, WeaponImpactEventsUseInferredOwnerAlpha)
{
	const std::string Source = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string ProcessEvents = FunctionBody(Source, "void CGameClient::ProcessEvents()");
	const std::string FinalizeHammerHitEvents = FunctionBody(Source, "void CGameClient::FinalizeHammerHitEvents()");

	EXPECT_NE(Source.find("float QmKnownOwnerEventAlpha(CGameClient *pGameClient, int Owner)"), std::string::npos);
	EXPECT_NE(Source.find("int QmInferExplosionOwner(CGameClient *pGameClient, vec2 Pos)"), std::string::npos);
	EXPECT_NE(Source.find("SQmHammerHitMatch QmInferHammerHit(CGameClient *pGameClient, vec2 Pos, int EventTick)"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("const float ExplosionAlpha = QmKnownOwnerEventAlpha(this, QmInferExplosionOwner(this, ExplosionPos));"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("m_Effects.Explosion(ExplosionPos, ExplosionAlpha);"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("m_vPendingHammerHitEvents.push_back({"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("QmInferHammerHit(this"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("m_HammerHitTracker.Record(Hit)"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("m_Effects.HammerHit("), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("const SQmHammerHitMatch Match = QmInferHammerHit(this, Event.m_Pos, Event.m_SnapshotTick);"), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("m_HammerHitTracker.Record(Hit)"), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("const float HammerHitAlpha = QmKnownOwnerEventAlpha(this, Match.m_AttackerId);"), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("m_Effects.HammerHit(Event.m_Pos, HammerHitAlpha, 1.0f);"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("m_Effects.Explosion(vec2(pEvent->m_X, pEvent->m_Y), Alpha);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, HammerHitConsumersUseDeferredServerEvidenceOnly)
{
	const std::string CharacterSource = ReadTextFile("src/game/client/prediction/entities/character.cpp");
	const std::string GameClientSource = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string GameWorldSource = ReadTextFile("src/game/client/prediction/gameworld.cpp");
	const std::string FastPracticeSource = ReadTextFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string InferHammerHit = FunctionBody(GameClientSource, "SQmHammerHitMatch QmInferHammerHit(");
	const std::string FinalizeHammerHitEvents = FunctionBody(GameClientSource, "void CGameClient::FinalizeHammerHitEvents()");
	const std::string OnNewSnapshot = FunctionBody(GameClientSource, "void CGameClient::OnNewSnapshot()");
	const std::string WakeupActions = FunctionBody(TClientSource, "void CTClient::CheckHammerWakeupActions()");

	EXPECT_EQ(CharacterSource.find("CreateHammerHitEvent"), std::string::npos);
	EXPECT_EQ(GameWorldSource.find("HammerHitEvents"), std::string::npos);
	EXPECT_EQ(GameWorldSource.find("BeginHammerHitEventBatch"), std::string::npos);
	EXPECT_EQ(GameClientSource.find("RecordPredictedHammerHits"), std::string::npos);
	EXPECT_EQ(GameClientSource.find("ConfirmPredictedEvent"), std::string::npos);
	EXPECT_EQ(GameClientSource.find("MatchPredictedEvent"), std::string::npos);
	EXPECT_EQ(InferHammerHit.find("m_PredictedWorld"), std::string::npos);
	EXPECT_NE(InferHammerHit.find("QmIsHammerSuperTeam(DDTeam, pGameClient->m_Teams.m_IsDDRace16)"), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("m_HammerHitTracker.Record(Hit)"), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("QmIsHammerWakeupTransition("), std::string::npos);
	EXPECT_NE(FinalizeHammerHitEvents.find("HandleConfirmedHammerHit(Hit);"), std::string::npos);
	EXPECT_NE(GameClientSource.find("const bool Online = Client()->State() == IClient::STATE_ONLINE;"), std::string::npos);
	EXPECT_NE(GameClientSource.find("if(Online)\n\t\tHandleHammerSkinSwap(Hit);"), std::string::npos);
	EXPECT_NE(FastPracticeSource.find("void CFastPractice::MaybePlayHammerHitEffect(CCharacter *pChar)"), std::string::npos);
	EXPECT_NE(FastPracticeSource.find("closest_point_on_line(StartPos, EndPos"), std::string::npos);
	EXPECT_EQ(FastPracticeSource.find("HammerHitTracker"), std::string::npos);
	EXPECT_NE(TClientSource.find("FindTargetHitsAtTick("), std::string::npos);
	EXPECT_NE(TClientSource.find("if(!Hit.m_TargetWoke)"), std::string::npos);
	EXPECT_NE(TClientSource.find("CheckHammerWakeupActions();"), std::string::npos);
	EXPECT_NE(TClientSource.find("m_aaComboLastHammerHitSnapshotTick[Dummy][TargetId]"), std::string::npos);
	EXPECT_EQ(GameClientSource.find("QmJellyHammerHitRadius"), std::string::npos);

	const size_t FinalizePos = OnNewSnapshot.find("FinalizeHammerHitEvents();");
	const size_t ComponentSnapshotPos = OnNewSnapshot.find("pComponent->OnNewSnapshot();");
	ASSERT_NE(FinalizePos, std::string::npos);
	ASSERT_NE(ComponentSnapshotPos, std::string::npos);
	EXPECT_LT(FinalizePos, ComponentSnapshotPos);

	const size_t UnspecPos = WakeupActions.find("Client()->SendPackMsg(Input.m_ActiveConnection");
	const size_t CloseChatPos = WakeupActions.find("GameClient()->m_Chat.DisableMode();");
	const size_t SwitchPos = WakeupActions.find("Console()->ExecuteLine(aCommand);");
	ASSERT_NE(UnspecPos, std::string::npos);
	ASSERT_NE(CloseChatPos, std::string::npos);
	ASSERT_NE(SwitchPos, std::string::npos);
	EXPECT_LT(UnspecPos, CloseChatPos);
	EXPECT_LT(CloseChatPos, SwitchPos);
}

TEST(QmNewUiMenuBranches, NewOpacityControlsDoNotChainLegacyPanelOpacity)
{
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");

	EXPECT_EQ(MenusSource.find("m_ClMenuPanelOpacity / 100.0f) * (g_Config.m_QmUiOpacity"), std::string::npos);
	EXPECT_EQ(MenusSource.find("m_ClMenuPanelElevatedOpacity / 100.0f) * (g_Config.m_QmUiOpacity"), std::string::npos);
	EXPECT_EQ(MenusSource.find("m_ClSettingsTabbarOpacity / 100.0f) * (g_Config.m_QmUiOpacity"), std::string::npos);
	EXPECT_EQ(MenusSource.find("m_ClMenuPanelOpacity / 100.0f) * (g_Config.m_QmMapBrowserOpacity"), std::string::npos);
	EXPECT_EQ(MenusSource.find("m_ClMenuPanelElevatedOpacity / 100.0f) * (g_Config.m_QmMapBrowserOpacity"), std::string::npos);
}

TEST(QmNewUiMenuBranches, NewColorControlsUseIndependentUiDomains)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string BrowserSource = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string ScoreboardSource = ReadTextFile("src/game/client/components/scoreboard.cpp");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_COL(QmUiColor, qm_ui_color"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_COL(QmMapBrowserColor, qm_map_browser_color"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_COL(QmScoreboardColor, qm_scoreboard_color"), std::string::npos);
	EXPECT_NE(MenusSource.find("ColorHSLA(g_Config.m_QmUiColor)"), std::string::npos);
	EXPECT_NE(MenusSource.find("ColorHSLA(g_Config.m_QmMapBrowserColor)"), std::string::npos);
	EXPECT_EQ(MenusSource.find("ColorHSLA(g_Config.m_ClMenuPanelColor)"), std::string::npos);
	EXPECT_EQ(MenusSource.find("ColorHSLA(g_Config.m_UiColor"), std::string::npos);
	EXPECT_NE(BrowserSource.find("ColorHSLA(g_Config.m_QmMapBrowserColor)"), std::string::npos);
	EXPECT_NE(ScoreboardSource.find("ColorHSLA(g_Config.m_QmScoreboardColor)"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ScoreboardBackgroundsUseScoreboardOpacity)
{
	const std::string Source = ReadTextFile("src/game/client/components/scoreboard.cpp");

	EXPECT_NE(Source.find("Color.a = ScoreboardUiAlpha(AlphaScale);"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmScoreboardOpacity / 100.0f"), std::string::npos);
	EXPECT_NE(Source.find("ScoreboardDecorationColor(GameClient()->GetDDTeamColor(DDTeam).WithAlpha(0.5f * ItemAlpha))"), std::string::npos);
	EXPECT_NE(Source.find("Row.Draw(ScoreboardDecorationColor(ui_token::color::ACCENT_PRIMARY_DIM.WithMultipliedAlpha(ItemAlpha * 1.45f))"), std::string::npos);
	EXPECT_NE(Source.find("Row.Draw(ScoreboardDecorationColor(ColorRGBA(0.7f, 0.7f, 0.7f, 0.7f * ItemAlpha))"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ScoreboardDdTeamLabelUsesUnifiedBelowRowLayout)
{
	const std::string Source = ReadTextFile("src/game/client/components/scoreboard.cpp");
	const std::string RenderScoreboard = FunctionBody(Source, "void CScoreboard::RenderScoreboard(");

	EXPECT_NE(RenderScoreboard.find("ResolveScoreboardTeamLabelLayout("), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("if(EndsDDTeam)"), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("TextRender()->Text(TeamLabelLayout.m_X, TeamLabelLayout.m_Y, TeamFontSize, aBuf);"), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("TeamLabelLayout.m_IconY"), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("SCOREBOARD_TEAM_MODE_ICON_SIZE"), std::string::npos);
	EXPECT_EQ(RenderScoreboard.find("TeamLabelLayout.m_Y,\n\t\t\t\t\tTeamFontSize"), std::string::npos);
	EXPECT_EQ(RenderScoreboard.find("NumPlayers > 8"), std::string::npos);
	EXPECT_EQ(RenderScoreboard.find("State.m_TeamStartX"), std::string::npos);
	EXPECT_EQ(RenderScoreboard.find("Row.x + Row.w / 2.0f - TextRender()->TextWidth(TeamFontSize, aBuf) / 2.0f + 5.0f"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ScoreboardMediaButtonSymbolsFollowContentAlpha)
{
	const std::string Source = ReadTextFile("src/game/client/components/scoreboard.cpp");
	const std::string Helper = FunctionBody(Source, "int DoScoreboardMediaIconButton(");

	EXPECT_NE(Helper.find("const float IconAlpha = std::clamp(ContentAlpha"), std::string::npos);
	EXPECT_NE(Helper.find("DefaultTextColor().WithMultipliedAlpha(IconAlpha)"), std::string::npos);
	EXPECT_NE(Helper.find("ColorRGBA(1.0f, 0.0f, 0.0f, IconAlpha)"), std::string::npos);
	EXPECT_NE(Helper.find("FontIcons::FONT_ICON_SLASH"), std::string::npos);
	EXPECT_NE(Source.find("DoScoreboardMediaIconButton(Ui(), TextRender(), &s_SmtcPrevButton"), std::string::npos);
	EXPECT_NE(Source.find("DoScoreboardMediaIconButton(Ui(), TextRender(), &s_SmtcPlayButton"), std::string::npos);
	EXPECT_NE(Source.find("DoScoreboardMediaIconButton(Ui(), TextRender(), &s_SmtcNextButton"), std::string::npos);
	EXPECT_EQ(Source.find("Ui()->DoButton_FontIcon(&s_SmtcPrevButton"), std::string::npos);
	EXPECT_EQ(Source.find("Ui()->DoButton_FontIcon(&s_SmtcPlayButton"), std::string::npos);
	EXPECT_EQ(Source.find("Ui()->DoButton_FontIcon(&s_SmtcNextButton"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BetterScoreboardUsesOneRowPlanAndDenseTeeLod)
{
	const std::string Source = ReadTextFile("src/game/client/components/scoreboard.cpp");
	const std::string OnRender = FunctionBody(Source, "void CScoreboard::OnRender()");
	const std::string RenderScoreboard = FunctionBody(Source, "void CScoreboard::RenderScoreboard(");

	EXPECT_NE(OnRender.find("BuildPlayerRowPlan"), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("TEE_PREVIEW_LAYER_BODY"), std::string::npos);
	EXPECT_EQ(RenderScoreboard.find("for(int j ="), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("const bool HasWar ="), std::string::npos);

	// The rendering optimization must not alter point lookup or display behavior.
	EXPECT_NE(OnRender.find("m_PlayerPoints.EnsureQueried"), std::string::npos);
	EXPECT_NE(RenderScoreboard.find("m_PlayerPoints.GetPoints"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BetterScoreboardBuildsOneSharedBlurWithFallback)
{
	const std::string Source = ReadTextFile("src/game/client/components/scoreboard.cpp");
	const std::string PrepareBlur = FunctionBody(Source, "bool CScoreboard::PrepareBetterScoreboardBlur()");
	const std::string RenderBlur = FunctionBody(Source, "void CScoreboard::RenderBetterScoreboardBlur(");
	const std::string OnRelease = FunctionBody(Source, "void CScoreboard::OnRelease()");

	EXPECT_NE(PrepareBlur.find("IsBackbufferCaptureSupported"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("IsRenderTargetGaussianBlurSupported"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("CaptureBackbufferToRenderTarget"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("GaussianBlurRenderTarget"), std::string::npos);
	EXPECT_NE(RenderBlur.find("m_BetterScoreboardBlurTarget"), std::string::npos);
	EXPECT_NE(RenderBlur.find("m_Visibility"), std::string::npos);
	EXPECT_NE(OnRelease.find("DestroyBetterScoreboardBlurTargets"), std::string::npos);

	// Existing translucent surfaces remain the unsupported-backend fallback.
	EXPECT_NE(Source.find("Scoreboard.Draw(ScoreboardGlassSurface(BackgroundAlphaFinal)"), std::string::npos);
}

TEST(QmNewUiMenuBranches, IngameMenuPrimaryActionLabelsUseEnglishKeys)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_ingame.cpp");

	EXPECT_NE(Source.find("pDisconnectButtonLabel = Localize(\"Disconnect\")"), std::string::npos);
	EXPECT_NE(Source.find("pDummyButtonLabel = Localize(\"Connect dummy\")"), std::string::npos);
	EXPECT_NE(Source.find("pDummyButtonLabel = Localize(\"Connecting dummy\")"), std::string::npos);
	EXPECT_NE(Source.find("pDummyButtonLabel = Localize(\"Disconnect dummy\")"), std::string::npos);
	EXPECT_NE(Source.find("pEditHudButtonLabel = Localize(\"Edit HUD\")"), std::string::npos);
	EXPECT_NE(Source.find("pDemoButtonLabel = Recording ? Localize(\"Stop record\") : Localize(\"Record demo\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Save last %d min\")"), std::string::npos);
	EXPECT_NE(Source.find("pDemoMarkerButtonLabel = Localize(\"Mark demo\")"), std::string::npos);
	EXPECT_NE(Source.find("pJoinRedButtonLabel = Localize(\"Join red\")"), std::string::npos);
	EXPECT_NE(Source.find("pJoinBlueButtonLabel = Localize(\"Join blue\")"), std::string::npos);
	EXPECT_NE(Source.find("pJoinGameButtonLabel = Localize(\"Join game\")"), std::string::npos);
	EXPECT_NE(Source.find("pKillButtonLabel = Localize(\"Kill\")"), std::string::npos);
	EXPECT_NE(Source.find("pPauseButtonLabel = (!Paused && !Spec) ? Localize(\"Pause\") : Localize(\"Join game\")"), std::string::npos);
	EXPECT_NE(Source.find("pFastPracticeLabel = FastPracticeEnabled ? Localize(\"Stop practice\") : Localize(\"Fast practice\")"), std::string::npos);
	EXPECT_NE(Source.find("DoToolTip(&s_DummyButton, &Button, Localize(\"Please wait…\"))"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DummyAndSpectateBindLabelsUseEnglishKeys)
{
	const std::string ControlsSource = ReadTextFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string TouchSource = ReadTextFile("src/game/client/components/touch_controls.cpp");

	EXPECT_NE(ControlsSource.find("Localizable(\"Toggle dummy\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Dummy jump\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Dummy fire\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Dummy hook\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Dummy copy\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Dummy hammer fly\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Control dummy\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Spectate mode\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Spectate teleport\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Spectate next\")"), std::string::npos);
	EXPECT_NE(ControlsSource.find("Localizable(\"Spectate previous\")"), std::string::npos);
	EXPECT_NE(TouchSource.find("Localizable(\"Toggle dummy\")"), std::string::npos);
	EXPECT_NE(TouchSource.find("Localizable(\"Spectate mode\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ConsoleChatExportLabelsUseEnglishKeys)
{
	const std::string Source = ReadTextFile("src/game/client/components/console.cpp");

	EXPECT_NE(Source.find("Localize(\"QmClient chat log\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Total\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Messages\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"No chat log selected\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Chat export failed\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Exported %d chat messages\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Selected %d\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Cancel\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Export selected\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Clear\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Select all chat\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Select export\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, HudDummyStatusLabelsUseEnglishKeys)
{
	const std::string Source = ReadTextFile("src/game/client/components/hud.cpp");

	EXPECT_NE(Source.find("Localize(\"Dummy mini view\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Connect dummy to enable\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Key Sticking: ?\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Key Sticking: On\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Key Sticking: Off\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Key Sticking: Reset Self\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Hammer: %s\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Dummy Control: %s\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Dummy sync: %s\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TranslationAndDemoUiLabelsUseEnglishKeys)
{
	const std::string ChatSource = ReadTextFile("src/game/client/components/chat.cpp");
	const std::string DemoSource = ReadTextFile("src/game/client/components/menus_demo.cpp");
	const std::string BrowserSource = ReadTextFile("src/game/client/components/menus_browser.cpp");

	EXPECT_NE(ChatSource.find("Localize(\"Translation Settings\")"), std::string::npos);
	EXPECT_NE(ChatSource.find("Localize(\"Auto-translate incoming messages\")"), std::string::npos);
	EXPECT_NE(ChatSource.find("Localize(\"Auto-translate outgoing messages\")"), std::string::npos);
	EXPECT_NE(ChatSource.find("Localize(\"Incoming language\")"), std::string::npos);
	EXPECT_NE(ChatSource.find("Localize(\"Outgoing language\")"), std::string::npos);
	EXPECT_NE(ChatSource.find("Localize(\"Translation service\")"), std::string::npos);
	EXPECT_NE(DemoSource.find("Localize(\"Could not preview this image\")"), std::string::npos);
	EXPECT_NE(DemoSource.find("BrowsingScreenshots ? Localize(\"Open the folder containing screenshots\") : Localize(\"Open the folder containing demo files\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Map\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Category\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Difficulty stars\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Note\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Has save\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"None\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ClientSourceDoesNotUseChineseLocalizeKeys)
{
	const std::string HudEditorSource = ReadTextFile("src/game/client/components/hud_editor.cpp");
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string BrowserSource = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string DemoSource = ReadTextFile("src/game/client/components/menus_demo.cpp");
	const std::string IngameTouchSource = ReadTextFile("src/game/client/components/menus_ingame_touch_controls.cpp");
	const std::string IngameSource = ReadTextFile("src/game/client/components/menus_ingame.cpp");
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string SettingsControlsSource = ReadTextFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string Settings7Source = ReadTextFile("src/game/client/components/menus_settings7.cpp");
	const std::string StartSource = ReadTextFile("src/game/client/components/menus_start.cpp");
	const std::string PieMenuSource = ReadTextFile("src/game/client/components/pie_menu.cpp");
	const std::string ScoreboardSource = ReadTextFile("src/game/client/components/scoreboard.cpp");

	EXPECT_NE(HudEditorSource.find("Localize(\"Position jump tip\")"), std::string::npos);
	EXPECT_NE(MenusSource.find("m_apSettingsTabs[SETTINGS_SOUND] = Localize(\"Sound\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"DDmaX Easy\")"), std::string::npos);
	EXPECT_NE(BrowserSource.find("Localize(\"Favorite map\")"), std::string::npos);
	EXPECT_NE(DemoSource.find("Localize(\"Screenshots directory\")"), std::string::npos);
	EXPECT_NE(IngameTouchSource.find("Localize(\"Allow dummy\", \"Touch button visibilities\")"), std::string::npos);
	EXPECT_NE(IngameTouchSource.find("Localize(\"Dummy connected\", \"Touch button visibilities\")"), std::string::npos);
	EXPECT_NE(IngameTouchSource.find("Localize(\"Spectate\", \"Predefined touch button behaviors\")"), std::string::npos);
	EXPECT_NE(IngameSource.find("Localize(\"Spectate\")"), std::string::npos);
	EXPECT_NE(IngameSource.find("Localize(\"Dummies are not allowed on this server\")"), std::string::npos);
	EXPECT_NE(SettingsSource.find("Localize(\"Show spectator cursor\")"), std::string::npos);
	EXPECT_NE(SettingsSource.find("Localize(\"Auto save chat log\")"), std::string::npos);
	EXPECT_NE(SettingsControlsSource.find("Localize(\"Dummy\")"), std::string::npos);
	EXPECT_NE(Settings7Source.find("Localize(\"Dummy\")"), std::string::npos);
	EXPECT_NE(StartSource.find("Localize(\"(Update required)\")"), std::string::npos);
	EXPECT_NE(PieMenuSource.find("Localize(\"Spectate\")"), std::string::npos);
	EXPECT_NE(ScoreboardSource.find("Localize(\"Spectators\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, PieMenuSeparatesSelfRenameFromOtherPlayerActions)
{
	const std::string Source = ReadTextFile("src/game/client/components/pie_menu.cpp");
	const std::string FindNearestPlayer = FunctionBody(Source, "int CPieMenu::FindNearestPlayer()");
	const std::string OpenMenu = FunctionBody(Source, "void CPieMenu::OpenMenu()");
	const std::string OnInput = FunctionBody(Source, "bool CPieMenu::OnInput(");
	const std::string UpdateSelection = FunctionBody(Source, "void CPieMenu::UpdateSelection()");
	const std::string OnRender = FunctionBody(Source, "void CPieMenu::OnRender()");
	const std::string RenderCenterInfo = FunctionBody(Source, "void CPieMenu::RenderCenterInfo()");
	const std::string ExecuteRenameOption = FunctionBody(Source, "void CPieMenu::ExecuteRenameOption(");

	// Both local connections belong to the user and must never become inner-ring targets.
	EXPECT_NE(FindNearestPlayer.find("GameClient()->IsLocalClientId(i)"), std::string::npos);

	// A connected local identity and at least one usable ring are required to open the menu.
	EXPECT_NE(OpenMenu.find("Client()->State() != IClient::STATE_ONLINE"), std::string::npos);
	EXPECT_NE(OpenMenu.find("LocalClientId < 0 || LocalClientId >= MAX_CLIENTS"), std::string::npos);
	EXPECT_NE(OpenMenu.find("if(TargetId < 0 && m_vRenameQueue.empty())"), std::string::npos);

	// Without another player the hidden inner ring cannot be selected or triggered by number keys.
	EXPECT_NE(OnInput.find("if(!HasTargetPlayer())"), std::string::npos);
	EXPECT_NE(UpdateSelection.find("if(HasTargetPlayer() && MouseDistance <= OuterRadius)"), std::string::npos);
	EXPECT_NE(OnRender.find("if(HasTargetPlayer())"), std::string::npos);

	// Targetless mode displays self, and hovering the outer ring identifies rename as a self action.
	EXPECT_NE(RenderCenterInfo.find("const int DisplayClientId = HasTargetPlayer() ? m_TargetClientId : LocalClientId;"), std::string::npos);
	EXPECT_NE(RenderCenterInfo.find("Localize(\"Self\")"), std::string::npos);
	EXPECT_EQ(ExecuteRenameOption.find("m_TargetClientId"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmClientAxiomAutoLoginLivesInQmClientComponent)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/axiom_auto_login.cpp");
	const std::string Header = ReadTextFile("src/game/client/components/qmclient/axiom_auto_login.h");
	const std::string TClientHeader = ReadTextFile("src/game/client/components/tclient/tclient.h");
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string GameClientHeader = ReadTextFile("src/game/client/gameclient.h");

	EXPECT_NE(Header.find("class CQmAxiomAutoLogin : public CComponent"), std::string::npos);
	EXPECT_NE(GameClientHeader.find("CQmAxiomAutoLogin m_QmAxiomAutoLogin;"), std::string::npos);
	EXPECT_NE(Source.find("void CQmAxiomAutoLogin::TrySendLogin()"), std::string::npos);
	EXPECT_NE(Source.find("void CQmAxiomAutoLogin::TrySendDummyLogin()"), std::string::npos);
	EXPECT_NE(Source.find("bool CQmAxiomAutoLogin::IsAxiomCommunity() const"), std::string::npos);
	EXPECT_NE(Header.find("QMCLIENT_AXIOM_AUTO_LOGIN_SLOW_RETRY_SECONDS"), std::string::npos);
	EXPECT_NE(Header.find("SQmAxiomAutoLoginState m_AutoLoginState;"), std::string::npos);
	EXPECT_NE(Header.find("m_SlowRetryMode"), std::string::npos);
	EXPECT_NE(Header.find("m_HardFailed"), std::string::npos);
	EXPECT_NE(Header.find("QmScheduleAxiomAutoLoginRetry"), std::string::npos);
	EXPECT_NE(Source.find("QmClassifyAxiomLoginReply(pText)"), std::string::npos);
	EXPECT_NE(Source.find("QmApplyAxiomLoginReply"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Trying Axiom auto login\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Axiom auto login succeeded\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Axiom auto login failed, retrying\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Axiom auto login failed\")"), std::string::npos);

	EXPECT_EQ(TClientHeader.find("IsAxiomCommunity() const"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("ResetAxiomAutoLoginState"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("UpdateAxiomAutoLogin"), std::string::npos);
	EXPECT_EQ(TClientHeader.find("HandleAxiomAutoLoginMessage"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TrySendAxiomLogin"), std::string::npos);
	EXPECT_EQ(TClientSource.find("HandleAxiomAutoLoginMessage"), std::string::npos);
}

TEST(QmAxiomAutoLogin, ClassifiesOfficialServerReplies)
{
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 验证中..."), EQmAxiomLoginReply::PENDING);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 欢迎回来 Q1menG"), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 重连成功！欢迎回来 Q1menG"), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 您已经登录过了"), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登入] 您已經登入過了"), EQmAxiomLoginReply::SUCCESS);
}

TEST(QmAxiomAutoLogin, DistinguishesFailuresAndUnrelatedMessages)
{
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 密码错误"), EQmAxiomLoginReply::HARD_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[login] invalid token"), EQmAxiomLoginReply::HARD_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[login] not authenticated"), EQmAxiomLoginReply::HARD_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 服务器错误，请稍后重试"), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[login] login was unsuccessful"), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[login] Something went wrong, please retry"), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 1 无法连接到中心服务器，请稍后重试"), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("[登录] 该凭证已有玩家在线，超时重连请等待目标玩家超时后重新登录"), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("welcome to the server"), EQmAxiomLoginReply::IGNORE);
	EXPECT_EQ(QmClassifyAxiomLoginReply(nullptr), EQmAxiomLoginReply::IGNORE);
}

TEST(QmAxiomAutoLogin, KeepsVerificationPendingThroughServerTimeoutWindow)
{
	constexpr int64_t TimeFreq = 10;
	SQmAxiomAutoLoginState State;

	EXPECT_TRUE(QmUpdateAxiomAutoLoginState(State, 100, TimeFreq));
	QmMarkAxiomAutoLoginAttempt(State, 100, TimeFreq);
	EXPECT_EQ(State.m_Attempts, 1);
	EXPECT_TRUE(State.m_WaitingReply);
	EXPECT_EQ(State.m_NextTryTick, 400);

	EXPECT_EQ(QmApplyAxiomLoginReply(State, EQmAxiomLoginReply::PENDING, 200, TimeFreq), EQmAxiomLoginReply::PENDING);
	EXPECT_EQ(State.m_NextTryTick, 500);
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 499, TimeFreq));
	EXPECT_TRUE(State.m_WaitingReply);
}

TEST(QmAxiomAutoLogin, SchedulesFastThenSlowRetriesAtExactDeadlines)
{
	constexpr int64_t TimeFreq = 10;
	SQmAxiomAutoLoginState State;
	State.m_Attempts = 1;
	State.m_WaitingReply = true;
	State.m_NextTryTick = 500;

	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 500, TimeFreq));
	EXPECT_FALSE(State.m_WaitingReply);
	EXPECT_FALSE(State.m_SlowRetryMode);
	EXPECT_EQ(State.m_NextTryTick, 520);
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 519, TimeFreq));
	EXPECT_TRUE(QmUpdateAxiomAutoLoginState(State, 520, TimeFreq));

	State = {};
	State.m_Attempts = QMCLIENT_AXIOM_AUTO_LOGIN_MAX_ATTEMPTS;
	State.m_WaitingReply = true;
	State.m_NextTryTick = 800;
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 800, TimeFreq));
	EXPECT_TRUE(State.m_SlowRetryMode);
	EXPECT_EQ(State.m_NextTryTick, 1100);
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 1099, TimeFreq));
	EXPECT_TRUE(QmUpdateAxiomAutoLoginState(State, 1100, TimeFreq));
}

TEST(QmAxiomAutoLogin, DelayedSuccessClosesBackoffAndStopsFurtherAttempts)
{
	constexpr int64_t TimeFreq = 10;
	SQmAxiomAutoLoginState State;
	State.m_Attempts = 1;
	State.m_NextTryTick = 200;

	EXPECT_EQ(QmApplyAxiomLoginReply(State, EQmAxiomLoginReply::SUCCESS, 150, TimeFreq), EQmAxiomLoginReply::SUCCESS);
	EXPECT_TRUE(State.m_Succeeded);
	EXPECT_FALSE(State.m_WaitingReply);
	EXPECT_EQ(State.m_NextTryTick, 0);
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 200, TimeFreq));
}

TEST(QmAxiomAutoLogin, AppliesRetryableAndHardFailuresToState)
{
	constexpr int64_t TimeFreq = 10;
	SQmAxiomAutoLoginState State;
	State.m_Attempts = 1;
	State.m_WaitingReply = true;

	EXPECT_EQ(QmApplyAxiomLoginReply(State, EQmAxiomLoginReply::RETRYABLE_FAILURE, 100, TimeFreq), EQmAxiomLoginReply::RETRYABLE_FAILURE);
	EXPECT_FALSE(State.m_WaitingReply);
	EXPECT_EQ(State.m_NextTryTick, 120);

	State.m_WaitingReply = false;
	EXPECT_EQ(QmApplyAxiomLoginReply(State, EQmAxiomLoginReply::HARD_FAILURE, 110, TimeFreq), EQmAxiomLoginReply::HARD_FAILURE);
	EXPECT_TRUE(State.m_HardFailed);
	EXPECT_EQ(State.m_NextTryTick, 0);
	EXPECT_FALSE(QmUpdateAxiomAutoLoginState(State, 120, TimeFreq));
}

TEST(QmAxiomAutoLogin, AcceptsDelayedTerminalRepliesOnlyAfterAnAttempt)
{
	EXPECT_TRUE(QmShouldHandleAxiomLoginReply(EQmAxiomLoginReply::SUCCESS, 1, false));
	EXPECT_TRUE(QmShouldHandleAxiomLoginReply(EQmAxiomLoginReply::HARD_FAILURE, 1, false));
	EXPECT_FALSE(QmShouldHandleAxiomLoginReply(EQmAxiomLoginReply::PENDING, 1, false));
	EXPECT_FALSE(QmShouldHandleAxiomLoginReply(EQmAxiomLoginReply::SUCCESS, 0, false));
	EXPECT_TRUE(QmShouldHandleAxiomLoginReply(EQmAxiomLoginReply::PENDING, 1, true));
}

TEST(QmAxiomAutoLogin, WaitsLongEnoughForAxiomVerificationAndUsesMainConnection)
{
	EXPECT_GE(QMCLIENT_AXIOM_AUTO_LOGIN_REPLY_TIMEOUT_SECONDS, 25);

	const std::string Source = ReadTextFile("src/game/client/components/qmclient/axiom_auto_login.cpp");
	EXPECT_NE(Source.find("SendChatOnConn(IClient::CONN_MAIN, 0, aLoginCommand)"), std::string::npos);
	EXPECT_NE(Source.find("SendChatOnConn(IClient::CONN_DUMMY, 0, aLoginCommand)"), std::string::npos);
	EXPECT_EQ(Source.find("m_Chat.SendChat(0, aLoginCommand)"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TClientSettingsTabsPreserveHiddenStateAndVisibleCorners)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string RenderSettingsTClient = FunctionBody(Source, "void CMenus::RenderSettingsTClient(CUIRect MainView, bool PrewarmOnly)");
	const std::string RenderSettingsTClientInfo = FunctionBody(Source, "void CMenus::RenderSettingsTClientInfo(CUIRect MainView)");

	EXPECT_NE(RenderSettingsTClient.find("if(TabCount <= 0)"), std::string::npos);
	EXPECT_NE(RenderSettingsTClient.find("FirstVisibleTab"), std::string::npos);
	EXPECT_NE(RenderSettingsTClient.find("VisibleTabIndex"), std::string::npos);
	EXPECT_NE(RenderSettingsTClient.find("VisibleTabIndex == 0"), std::string::npos);
	EXPECT_NE(RenderSettingsTClient.find("VisibleTabIndex == TabCount - 1"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientInfo.find("s_aShowTabs[i] = IsFlagSet(g_Config.m_TcTClientSettingsTabs, i);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TClientWarListDefersDeletesAndValidatesSelections)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string RenderSettingsTClientWarList = FunctionBody(Source, "void CMenus::RenderSettingsTClientWarList(CUIRect MainView)");

	EXPECT_NE(RenderSettingsTClientWarList.find("static CWarType *s_pSelectedType = nullptr;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("WarTypeExists"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("WarEntryExists"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("CWarEntry *pEntryToRemove = nullptr;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("RemoveWarEntry(pEntryToRemove);"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("s_pSelectedEntry = nullptr;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("NewSelectedEntry < (int)s_vFilteredEntries.size()"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientWarList.find("NewSelectedType < (int)GameClient()->m_WarList.m_WarTypes.size()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TClientProfilesAndStatusBarClampUiIndices)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string RenderSettingsTClientProfiles = FunctionBody(Source, "void CMenus::RenderSettingsTClientProfiles(CUIRect MainView)");
	const std::string RenderSettingsTClientStatusBar = FunctionBody(Source, "void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)");

	EXPECT_NE(RenderSettingsTClientProfiles.find("Profile.m_FeetColor >= 0"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientProfiles.find("ProfilesPerRow = maximum(1"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("StatusItemTypeCount"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("s_TypeSelectedOld < StatusItemTypeCount"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("s_SelectedItem < (int)GameClient()->m_StatusBar.m_StatusBarItems.size()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsListSelectionsClampBeforeIndexing)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsPlayer = FunctionBody(Source, "void CMenus::RenderSettingsPlayer(CUIRect MainView)");
	const std::string RenderSettingsGraphics = FunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	const std::string PopupMapPicker = FunctionBody(Source, "CUi::EPopupMenuFunctionResult CMenus::PopupMapPicker(void *pContext, CUIRect View, bool Active)");

	EXPECT_NE(RenderSettingsPlayer.find("NewSelected >= 0 && NewSelected < (int)s_vpFilteredFlags.size()"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("NewSelected >= 0 && NewSelected < s_NumNodes"), std::string::npos);
	EXPECT_NE(PopupMapPicker.find("const int ItemIndex = MapIndex++;"), std::string::npos);
	EXPECT_NE(PopupMapPicker.find("ItemIndex == pPopupContext->m_Selection"), std::string::npos);
	EXPECT_NE(PopupMapPicker.find("NewSelected >= 0 && NewSelected < (int)pPopupContext->m_vMaps.size()"), std::string::npos);
	EXPECT_NE(PopupMapPicker.find("pPopupContext->m_Selection >= 0"), std::string::npos);
}

TEST(QmNewUiMenuBranches, BackgroundMapPickerUsesMapsRootAndSupportedFiles)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsDDNet = FunctionBody(Source, "void CMenus::RenderSettingsDDNet(CUIRect MainView)");
	const std::string MapListPopulate = FunctionBody(Source, "void CMenus::CPopupMapPickerContext::MapListPopulate()");
	const std::string MapListFetchCallback = FunctionBody(Source, "int CMenus::CPopupMapPickerContext::MapListFetchCallback");

	EXPECT_NE(RenderSettingsDDNet.find("str_copy(s_PopupMapPickerContext.m_aRootPath, \"maps\""), std::string::npos);
	EXPECT_NE(MapListPopulate.find("ListRoot(m_aRootPath[0] != '\\0' ? m_aRootPath : \"maps\", m_aValuePrefix);"), std::string::npos);
	EXPECT_EQ(MapListPopulate.find("m_aFallbackRootPath"), std::string::npos);
	EXPECT_EQ(MapListPopulate.find("m_aFallbackValuePrefix"), std::string::npos);
	EXPECT_NE(MapListFetchCallback.find("FindBackgroundFileExtension(pInfo->m_pName)"), std::string::npos);
	EXPECT_EQ(MapListFetchCallback.find("str_endswith(pInfo->m_pName, \".map\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, EditorSaveFileDialogKeepsFilenameInputInControl)
{
	const std::string Source = ReadTextFile("src/game/editor/file_browser.cpp");
	const std::string OnRender = FunctionBody(Source, "void CFileBrowser::OnRender(CUIRect _)");

	EXPECT_NE(OnRender.find("m_ListBox.SetActive(!Ui()->IsPopupOpen() && (!m_SaveAction || !m_FilenameInput.IsActive()))"), std::string::npos);
	EXPECT_NE(OnRender.find("const bool ListChoseItem = m_ListBox.WasItemSelected() || m_ListBox.WasItemActivated();"), std::string::npos);
	EXPECT_NE(OnRender.find("const bool SyncFilenameInput = !m_SaveAction || (ListChoseItem && m_SelectedFileIndex >= 0);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmClientLanguageReadmeDescribesChineseSourceKeys)
{
	EXPECT_FALSE(fs_is_dir(TestSourcePath("data/qmclient/languages").c_str()));
}

TEST(QmNewUiMenuBranches, KcpLogUsesBoundedFormatting)
{
	const std::string Source = ReadTextFile("src/engine/external/kcp/ikcp.c");

	EXPECT_EQ(Source.find("vsprintf(buffer, fmt, argptr);"), std::string::npos);
	EXPECT_NE(Source.find("vsnprintf(buffer, sizeof(buffer), fmt, argptr);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DisplayChangedDoesNotUseDisplayUnionData)
{
	const std::string Source = ReadTextFile("src/engine/client/input.cpp");
	const size_t CaseStart = Source.find("case SDL_WINDOWEVENT_DISPLAY_CHANGED:");
	ASSERT_NE(CaseStart, std::string::npos);
	const size_t Break = Source.find("break;", CaseStart);
	ASSERT_NE(Break, std::string::npos);
	const std::string Body = Source.substr(CaseStart, Break - CaseStart);

	EXPECT_EQ(Body.find("Event.display.data1"), std::string::npos);
	EXPECT_NE(Body.find("Event.window.data1"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->SwitchWindowScreen(DisplayIndex, false);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, GraphicsDriverCrashRecoveryUsesSafeStartupFallback)
{
	const std::string Source = ReadTextFile("src/engine/client/client.cpp");
	const std::string Detector = FunctionBody(Source, "static bool QmCrashTextHasGraphicsDriverFault");
	const std::string Recovery = FunctionBody(Source, "static bool ApplyQmSafeGraphicsRecovery");
	const std::string StartupHook = FunctionBody(Source, "static void RecoverQmGraphicsSettingsAfterDriverCrash");

	EXPECT_NE(Detector.find("Exception module: nvoglv64.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module nvoglv64.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: vulkan-1.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: D3D12Core.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module D3D12Core.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: d3d12.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: dxgi.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module opengl32.dll"), std::string::npos);

	EXPECT_NE(StartupHook.find("gs_pQmLifecycleMarkerFile"), std::string::npos);
	EXPECT_NE(StartupHook.find("ListDirectoryInfo"), std::string::npos);
	EXPECT_NE(StartupHook.find("ReadFileStr"), std::string::npos);

	EXPECT_NE(Recovery.find("str_copy(g_Config.m_GfxBackend, \"OpenGL\");"), std::string::npos);
	EXPECT_NE(Recovery.find("g_Config.m_GfxGLMajor = 3;"), std::string::npos);
	EXPECT_NE(Recovery.find("g_Config.m_GfxGLMinor = 0;"), std::string::npos);
	EXPECT_NE(Recovery.find("g_Config.m_GfxFsaaSamples = 0;"), std::string::npos);
	EXPECT_NE(Recovery.find("g_Config.m_GfxFullscreen = 0;"), std::string::npos);

	const size_t HookCall = Source.find("RecoverQmGraphicsSettingsAfterDriverCrash(pStorage);");
	const size_t CommandLineParse = Source.find("pConsole->ParseArguments(argc - 1, &argv[1]);");
	ASSERT_NE(HookCall, std::string::npos);
	ASSERT_NE(CommandLineParse, std::string::npos);
	EXPECT_LT(HookCall, CommandLineParse);
}

TEST(QmNewUiMenuBranches, GraphicsDriverCrashRecoveryIgnoresStaleAndConsumedReports)
{
	const std::string Source = ReadTextFile("src/engine/client/client.cpp");
	const std::string FindLatest = FunctionBody(Source, "static int FindLatestQmCrashReportCallback");
	const std::string ReadStartedAt = FunctionBody(Source, "static bool ReadQmLifecycleMarkerStartedAt");
	const std::string FormatFingerprint = FunctionBody(Source, "static void FormatQmGraphicsCrashReportFingerprint");
	const std::string StartupHook = FunctionBody(Source, "static void RecoverQmGraphicsSettingsAfterDriverCrash");

	EXPECT_NE(Source.find("gs_pQmGraphicsRecoveryStateFile"), std::string::npos);
	EXPECT_NE(ReadStartedAt.find("started_at="), std::string::npos);
	EXPECT_NE(FindLatest.find("pInfo->m_TimeModified < pLatest->m_MinTimeModified"), std::string::npos);
	EXPECT_NE(FormatFingerprint.find("Report.m_TimeModified"), std::string::npos);
	EXPECT_NE(FormatFingerprint.find("Report.m_aPath"), std::string::npos);
	EXPECT_NE(StartupHook.find("ReadQmLifecycleMarkerStartedAt"), std::string::npos);
	EXPECT_NE(StartupHook.find("WasQmGraphicsCrashReportRecovered"), std::string::npos);
	EXPECT_NE(StartupHook.find("MarkQmGraphicsCrashReportRecovered"), std::string::npos);
	EXPECT_LT(StartupHook.find("WasQmGraphicsCrashReportRecovered"), StartupHook.find("ApplyQmSafeGraphicsRecovery"));
	EXPECT_GT(StartupHook.find("MarkQmGraphicsCrashReportRecovered"), StartupHook.find("ApplyQmSafeGraphicsRecovery"));
}

TEST(QmNewUiMenuBranches, ImplausibleRefreshRatesAreNotPersisted)
{
	const std::string Backend = ReadTextFile("src/engine/client/backend_sdl.cpp");
	const std::string Graphics = ReadTextFile("src/engine/client/graphics_threaded.cpp");

	EXPECT_NE(Backend.find("static bool IsPlausibleRefreshRate(int RefreshRate)"), std::string::npos);
	EXPECT_NE(Backend.find("static bool IsPlausibleWindowSize(int Width, int Height)"), std::string::npos);
	EXPECT_NE(Backend.find("Ignoring implausible configured window size"), std::string::npos);
	EXPECT_NE(Backend.find("*pWidth = DisplayMode.w;"), std::string::npos);
	EXPECT_NE(Backend.find("*pHeight = DisplayMode.h;"), std::string::npos);
	EXPECT_NE(Backend.find("Ignoring implausible configured refresh rate"), std::string::npos);
	EXPECT_NE(Backend.find("*pRefreshRate = 0;"), std::string::npos);
	EXPECT_NE(Graphics.find("static bool IsPlausibleWindowRefreshRate(int RefreshRate)"), std::string::npos);
	EXPECT_NE(Graphics.find("Ignoring implausible refresh rate during resize"), std::string::npos);
	EXPECT_NE(Graphics.find("RefreshRate = m_ScreenRefreshRate;"), std::string::npos);
	EXPECT_NE(Graphics.find("static bool IsPlausibleWindowSize(int Width, int Height)"), std::string::npos);
	EXPECT_NE(Graphics.find("static int LogicalWindowSizeFromViewport(int ViewportSize, float HiDPIScale)"), std::string::npos);
	EXPECT_NE(Graphics.find("Ignoring implausible resize dimensions"), std::string::npos);
	EXPECT_NE(Graphics.find("if(IsPlausibleWindowSize(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight))"), std::string::npos);
	EXPECT_EQ(Graphics.find("w = g_Config.m_GfxScreenWidth > 0 ? g_Config.m_GfxScreenWidth : m_ScreenWidth;"), std::string::npos);
	EXPECT_EQ(Graphics.find("h = g_Config.m_GfxScreenHeight > 0 ? g_Config.m_GfxScreenHeight : m_ScreenHeight;"), std::string::npos);
	EXPECT_NE(Graphics.find("w = LogicalWindowSizeFromViewport(m_ScreenWidth, m_ScreenHiDPIScale);"), std::string::npos);
	EXPECT_NE(Graphics.find("h = LogicalWindowSizeFromViewport(m_ScreenHeight, m_ScreenHiDPIScale);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, GraphicsCurrentModeLabelSanitizesScaleAndAspectRatio)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");

	EXPECT_NE(Source.find("const float HiDPIScale = std::isfinite(RawHiDPIScale) && RawHiDPIScale > 0.0f ? RawHiDPIScale : 1.0f;"), std::string::npos);
	EXPECT_NE(Source.find("const int AspectGcd = G > 0 ? G : 1;"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_GfxScreenWidth / AspectGcd"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_GfxScreenHeight / AspectGcd"), std::string::npos);
}

TEST(QmCameraAspectRatio, KeepsUiAspectPhysicalAndOverridesOnlyGameWorld)
{
	const std::string GraphicsHeader = ReadTextFile("src/engine/graphics.h");
	const std::string GraphicsSource = ReadTextFile("src/engine/graphics.cpp");
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string GameClientSource = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string ControlsSource = ReadTextFile("src/game/client/components/controls.cpp");
	const std::string HudSource = ReadTextFile("src/game/client/components/hud.cpp");
	const std::string TouchControlsSource = ReadTextFile("src/game/client/components/touch_controls.cpp");
	const std::string CollisionHitboxSource = ReadTextFile("src/game/client/components/qmclient/collision_hitbox.cpp");
	const std::string BackgroundParticlesSource = ReadTextFile("src/game/client/components/tclient/background_particles.cpp");
	const std::string RenderLayerSource = ReadTextFile("src/game/map/render_layer.cpp");
	const std::string MapRendererSource = ReadTextFile("src/game/map/map_renderer.cpp");
	const std::string MovingTilesSource = ReadTextFile("src/game/client/components/tclient/moving_tiles.cpp");
	const std::string NameplatesSource = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string UiSource = ReadTextFile("src/game/client/ui.cpp");

	EXPECT_NE(GraphicsHeader.find("float ScreenAspect() const { return (float)ScreenWidth() / (float)ScreenHeight(); }"), std::string::npos);
	EXPECT_NE(GraphicsHeader.find("float GameScreenAspect() const { return m_GameScreenAspectOverride > 0.0f ? m_GameScreenAspectOverride : ScreenAspect(); }"), std::string::npos);
	EXPECT_NE(TClientSource.find("Graphics()->SetGameScreenAspectOverride(GameScreenAspectOverride);"), std::string::npos);
	EXPECT_EQ(TClientSource.find("SetScreenAspectOverride"), std::string::npos);
	EXPECT_NE(RenderLayerSource.find("Graphics()->GameScreenAspect()"), std::string::npos);
	EXPECT_NE(RenderLayerSource.find("Graphics()->MapScreenToGameInterface("), std::string::npos);
	EXPECT_NE(MapRendererSource.find("Graphics()->MapScreenToGameInterface("), std::string::npos);
	EXPECT_NE(MovingTilesSource.find("Graphics()->MapScreenToGameInterface("), std::string::npos);
	EXPECT_NE(NameplatesSource.find("This.Graphics()->MapScreenToGameInterface("), std::string::npos);
	EXPECT_NE(GameClientSource.find("CalcScreenParams(Graphics()->GameScreenAspect(), ShowDistanceZoom"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_LastScreenAspect = Graphics()->GameScreenAspect();"), std::string::npos);
	EXPECT_NE(GameClientSource.find("CalcScreenParams(Graphics()->GameScreenAspect(), m_Camera.m_Zoom"), std::string::npos);
	EXPECT_NE(ControlsSource.find("CalcScreenParams(Graphics()->GameScreenAspect(), 1.0f"), std::string::npos);
	EXPECT_NE(HudSource.find("CalcScreenParams(pGraphics->GameScreenAspect(), GameClient.m_Camera.m_Zoom"), std::string::npos);
	EXPECT_NE(HudSource.find("Graphics()->GameScreenAspect(), MiniZoom, aPoints"), std::string::npos);
	EXPECT_NE(HudSource.find("Graphics()->GameScreenAspect(), 1.0f, aPoints"), std::string::npos);
	EXPECT_NE(TouchControlsSource.find("CalcScreenParams(m_pTouchControls->Graphics()->GameScreenAspect()"), std::string::npos);
	EXPECT_NE(TouchControlsSource.find("CalcScreenParams(Graphics()->GameScreenAspect(), Zoom"), std::string::npos);
	EXPECT_NE(CollisionHitboxSource.find("Graphics()->GameScreenAspect(), GameClient()->m_Camera.m_Zoom"), std::string::npos);
	EXPECT_NE(BackgroundParticlesSource.find("Graphics()->GameScreenAspect(), Zoom, aPoints"), std::string::npos);

	const std::string MapScreenToInterface = FunctionBody(GraphicsSource, "void IGraphics::MapScreenToInterface(");
	const std::string MapScreenToGameInterface = FunctionBody(GraphicsSource, "void IGraphics::MapScreenToGameInterface(");
	EXPECT_NE(MapScreenToInterface.find("ScreenAspect()"), std::string::npos);
	EXPECT_EQ(MapScreenToInterface.find("GameScreenAspect()"), std::string::npos);
	EXPECT_NE(MapScreenToGameInterface.find("GameScreenAspect()"), std::string::npos);
	EXPECT_NE(UiSource.find("Graphics()->ScreenAspect()"), std::string::npos);
	EXPECT_EQ(UiSource.find("GameScreenAspect()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, GraphicsBackendDropdownUsesQmClientDisplayNames)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string Formatter = FunctionBody(Source, "void FormatQmGraphicsBackendDisplayName(char *pBuf, int BufSize, const char *pBackendName, int Major, int Minor, int Patch, bool IsDefault)");
	const std::string RenderSettingsGraphics = FunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");

	ASSERT_FALSE(Formatter.empty());
	ASSERT_FALSE(RenderSettingsGraphics.empty());
	EXPECT_NE(Formatter.find("\"OpenGL_QmClient_%d_%d\""), std::string::npos);
	EXPECT_NE(Formatter.find("\"Vulkan_QmClient\""), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Localize(\"Graphics backend\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("FormatQmGraphicsBackendDisplayName(aTmpBackendName"), std::string::npos);
	EXPECT_NE(Source.find("static std::vector<const CCountryFlags::CCountryFlag *> s_vpFilteredFlags"), std::string::npos);
	EXPECT_NE(Source.find("s_vpFilteredFlags.reserve(GameClient()->m_CountryFlags.Num());"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("s_aScreenNamesCacheLanguage"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("const bool RefreshScreenNames"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("s_vSupportedBackendNames"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("s_BackendListCacheDriverBlocked"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("s_vGpuIdNames"), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("s_vpGpuIdNames[i] = aCurDeviceName"), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("Localize(\"Renderer\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, GlobalFormControlsUseCompactNewUiStyle)
{
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string UiSource = ReadTextFile("src/game/client/ui.cpp");
	const std::string PixelSnap = FunctionBody(MenusSource, "float SnapUiToPixel(");
	const std::string MaskRasterizer = FunctionBody(MenusSource, "void RasterizeQmToggleCapsuleMask(");
	const std::string MaskTexture = FunctionBody(MenusSource, "IGraphics::CTextureHandle CreateQmToggleMaskTexture(");
	const std::string MaskDraw = FunctionBody(MenusSource, "void DrawQmToggleMasks(");
	const std::string CheckboxLayout = FunctionBody(MenusSource, "void SplitMenuCheckboxRects(");
	const std::string Checkbox = FunctionBody(MenusSource, "int CMenus::DoButton_CheckBox_Common_WithLabelElement(");
	const std::string TristateCheckbox = FunctionBody(MenusSource, "int CMenus::DoButton_CheckBox_Tristate(");
	const std::string Slider = FunctionBody(UiSource, "float CUi::DoScrollbarH(");
	const std::string OnInit = FunctionBody(MenusSource, "void CMenus::OnInit(");
	const std::string OnShutdown = FunctionBody(MenusSource, "void CMenus::OnShutdown(");

	EXPECT_NE(PixelSnap.find("std::round(Value / PixelSize) * PixelSize"), std::string::npos);
	EXPECT_NE(MaskRasterizer.find("QmToggleMaskSupersampling"), std::string::npos);
	EXPECT_NE(MaskTexture.find("LoadTextureRawMove"), std::string::npos);
	EXPECT_NE(MaskDraw.find("QuadsSetSubset"), std::string::npos);
	EXPECT_NE(CheckboxLayout.find("Rect.VSplitRight(ToggleWidth, &Label, &Box)"), std::string::npos);
	EXPECT_NE(CheckboxLayout.find("Rect.h * 0.68f"), std::string::npos);
	EXPECT_NE(Checkbox.find("const bool UseQmToggleStyle = g_Config.m_QmNewUi != 0 && !HasCustomGlyph"), std::string::npos);
	EXPECT_NE(Checkbox.find("ToggleTrack.Draw"), std::string::npos);
	EXPECT_NE(Checkbox.find("ToggleKnob.Draw"), std::string::npos);
	EXPECT_NE(Checkbox.find("const ColorRGBA OffTrackColor(1.0f, 1.0f, 1.0f, 0.06f"), std::string::npos);
	EXPECT_NE(Checkbox.find("const ColorRGBA OnTrackColor(1.0f, 1.0f, 1.0f, 0.38f"), std::string::npos);
	EXPECT_EQ(Checkbox.find("Box.Margin(2.0f, &ToggleTrack)"), std::string::npos);
	EXPECT_NE(Checkbox.find("const float UiPixelSize = Ui()->PixelSize()"), std::string::npos);
	EXPECT_NE(Checkbox.find("ToggleTrack.x = SnapUiToPixel"), std::string::npos);
	EXPECT_NE(Checkbox.find("ToggleKnob.x = SnapUiToPixel"), std::string::npos);
	EXPECT_NE(Checkbox.find("std::abs(CheckStrength - TargetCheckStrength) <= MENU_TAB_ANIM_EPSILON ? TargetCheckStrength"), std::string::npos);
	EXPECT_NE(Checkbox.find("if(m_QmToggleMaskTexture.IsValid())"), std::string::npos);
	EXPECT_NE(Checkbox.find("DrawQmToggleMasks"), std::string::npos);
	EXPECT_NE(TristateCheckbox.find("BUTTONFLAG_LEFT, true"), std::string::npos);
	EXPECT_NE(OnInit.find("m_QmToggleMaskTexture = CreateQmToggleMaskTexture(Graphics())"), std::string::npos);
	EXPECT_NE(OnShutdown.find("Graphics()->UnloadTexture(&m_QmToggleMaskTexture)"), std::string::npos);
	EXPECT_NE(Slider.find("const bool UseQmSliderStyle = g_Config.m_QmNewUi != 0"), std::string::npos);
	EXPECT_NE(Slider.find("VisualRail.h = std::clamp("), std::string::npos);
	EXPECT_NE(Slider.find("Handle.Draw"), std::string::npos);
	EXPECT_NE(Slider.find("HandleSize * 0.5f"), std::string::npos);
}
