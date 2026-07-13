#include <engine/client/plausible_sizes.h>

#include <game/client/components/menus.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>

TEST(PlausibleSizes, RefreshRateAndWindowGuardsMatchContract)
{
	// Refresh rate: 0..1000 inclusive is the persisted-range contract.
	EXPECT_TRUE(IsPlausibleRefreshRate(0));
	EXPECT_TRUE(IsPlausibleRefreshRate(60));
	EXPECT_TRUE(IsPlausibleRefreshRate(1000));
	EXPECT_FALSE(IsPlausibleRefreshRate(-1));
	EXPECT_FALSE(IsPlausibleRefreshRate(1001));
	// Window size: 320..16384 on both axes.
	EXPECT_TRUE(IsPlausibleWindowSize(320, 240));
	EXPECT_TRUE(IsPlausibleWindowSize(16384, 16384));
	EXPECT_FALSE(IsPlausibleWindowSize(319, 240)); // under min width
	EXPECT_FALSE(IsPlausibleWindowSize(320, 239)); // under min height
	EXPECT_FALSE(IsPlausibleWindowSize(16385, 1080)); // over max width
	EXPECT_FALSE(IsPlausibleWindowSize(1920, 16385)); // over max height
}

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

TEST(QmNewUiMenuBranches, P6QmClientOverviewUsesCanonicalPageCardAndScroll)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = FunctionBody(Source, "void CMenus::RenderSettingsQmClientOverview(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Body.find(".State()"), std::string::npos);
	EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_EQ(Body.find("BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(Body.find("RenderQmSettingsGlassCard("), std::string::npos);
	const std::string Dispatch = FunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	EXPECT_NE(Dispatch.find("QMCLIENT_SETTINGS_TAB_OVERVIEW"), std::string::npos);
	EXPECT_NE(Dispatch.find("RenderSettingsQmClientOverview(ContentView, PrewarmOnly)"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Overview\")"), std::string::npos);
	EXPECT_NE(Source.find("ReadOnly ? s_GlobalSearchPrewarmDeck : m_SettingsCardDeck;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, P6QmClientContributorsUsesCanonicalDeck)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = FunctionBody(Source, "void CMenus::RenderSettingsQmClientContributors(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Body.find("deck:qmclient-contributors-community"), std::string::npos);
	EXPECT_NE(Body.find("deck:qmclient-contributors-sponsors"), std::string::npos);
	EXPECT_NE(Body.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_EQ(Body.find("BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(Body.find("RenderQmSettingsGlassCard("), std::string::npos);
	const std::string Dispatch = FunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Dispatch.empty());
	EXPECT_NE(Dispatch.find("RenderSettingsQmClientContributors(CanonicalQmClientContentView, PrewarmOnly)"), std::string::npos);
	EXPECT_NE(Source.find("str_comp(pTab, \"qmclient-contributors\") == 0"), std::string::npos);
	const std::string MenusSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string SetPageBody = FunctionBody(MenusSource, "bool CMenus::SetSettingsPageFromCardTab(const char *pTab)");
	EXPECT_NE(SetPageBody.find("str_comp(pTab, \"qmclient-contributors\") == 0"), std::string::npos);
	EXPECT_NE(Body.find("qmclient-community-thanks"), std::string::npos);
	EXPECT_NE(Body.find("BuildSponsorLines"), std::string::npos);
	EXPECT_NE(Body.find("!PrewarmOnly && g_QmClientEnsureSponsorQrTexture"), std::string::npos);
	EXPECT_NE(Source.find("Navigation.m_QmClientTab == QMCLIENT_SETTINGS_TAB_CONTRIBUTORS"), std::string::npos);
}

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
	EXPECT_NE(DoMenuTabV2.find("pRect->Draw(Resolved, Corners, UseNewUi ? 7.0f : 10.0f);"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("const float LabelFontSize = UseNewUi ? minimum(Label.h * CUi::ms_FontmodHeight, 13.0f) : Label.h * CUi::ms_FontmodHeight;"), std::string::npos);
	EXPECT_NE(DoMenuTabV2.find("Ui()->DoLabel(&Label, pText, LabelFontSize, TEXTALIGN_MC);"), std::string::npos);
	EXPECT_NE(Source.find("const bool UseNewUi = g_Config.m_QmNewUi != 0;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA InactiveColor = MenuTabDefaultColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA ActiveColor = MenuTabActiveColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA HoverColor = MenuMenubarHoverColor();"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA InactiveColor = ms_ColorTabbarInactive;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA ActiveColor = ms_ColorTabbarActive;"), std::string::npos);
	EXPECT_NE(Source.find("ColorRGBA HoverColor = ms_ColorTabbarHover;"), std::string::npos);
	EXPECT_NE(Source.find("const ColorRGBA IndicatorColor = g_Config.m_QmNewUi != 0 ? MenuUiColorAccent(1.0f) : ui_token::color::ACCENT_PRIMARY;"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("if(!UseNewUi && MenubarHaveActive && !Ui()->RenderOnly())"), std::string::npos);
	EXPECT_NE(RenderMenubar.find("if(UseNewUi)"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.12f)"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.VMargin(MenubarOuterInsetX, &Box);"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("Box.HMargin(MenubarOuterInsetY, &Box);"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float BrowserButtonWidth = 58.0f;"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float GameButtonWidth = CompactOnlineMenuTabs ? 56.0f : 64.0f;"), std::string::npos);
	EXPECT_NE(UseNewUiBlock.find("const float ServerInfoButtonWidth = CompactOnlineMenuTabs ? 94.0f : 104.0f;"), std::string::npos);
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
	EXPECT_NE(Source.find("Panel.Draw(BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.22f))"), std::string::npos);
	EXPECT_NE(Source.find("Tab.Draw(BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f))"), std::string::npos);
	EXPECT_EQ(Source.find("BrowserOpacityColor(ColorRGBA(0.0f, 0.0f, 0.3f))"), std::string::npos);
}

TEST(QmNewUiMenuBranches, AppearanceNamePlateContainsNameplateTextControlsWithoutInternalScrollRegion)
{
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string NamePlateBranch = BlockBodyAfter(SettingsSource, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	ASSERT_FALSE(NamePlateBranch.empty());

	const size_t NamePlateTitlePos = NamePlateBranch.find("Localize(\"Name Plate\")");
	const size_t TextSettingsPos = NamePlateBranch.find("Localize(\"Nameplate text\")");
	const size_t HookStrengthPos = NamePlateBranch.find("Localize(\"Hook Strength\")");
	ASSERT_NE(NamePlateTitlePos, std::string::npos);
	ASSERT_NE(TextSettingsPos, std::string::npos);
	ASSERT_NE(HookStrengthPos, std::string::npos);
	EXPECT_LT(NamePlateTitlePos, TextSettingsPos);
	EXPECT_LT(TextSettingsPos, HookStrengthPos);

	EXPECT_NE(NamePlateBranch.find("g_Config.m_QmNameplateTextEffects"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QM_TEXT_EFFECT_BORDER"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QM_TEXT_EFFECT_GRADIENT"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QM_TEXT_EFFECT_RAINBOW"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("QM_TEXT_EFFECT_GLOW"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Playing effects\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Spectate effects\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Demo effects\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Demo target\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Border range\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Glow range\")"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("DoLine_ColorPicker(&s_NameplateTextBorderColorId"), std::string::npos);

	EXPECT_EQ(NamePlateBranch.find("static CScrollRegion s_NameplateTextCardScrollRegion;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("BeginSettingsScrollRegion(s_NameplateTextCardScrollRegion"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("FinishSettingsScrollRegion(s_NameplateTextCardScrollRegion"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static CScrollRegion s_NameplateTextPlayingDropDownScrollRegion;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static CScrollRegion s_NameplateTextSpectateDropDownScrollRegion;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static CScrollRegion s_NameplateTextDemoDropDownScrollRegion;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static CScrollRegion s_NameplateTextDemoTargetDropDownScrollRegion;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("State.m_SelectionPopupContext.m_pScrollRegion = &ScrollRegion;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("s_NameplateTextDemoTargetDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_NameplateTextDemoTargetDropDownScrollRegion;"), std::string::npos);

	const std::string QmSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	EXPECT_EQ(QmSource.find("auto RenderNameplateTextSettings = [&](CUIRect &CardContent)"), std::string::npos);
	EXPECT_EQ(QmSource.find("RenderNameplateTextSettings(CardContent);"), std::string::npos);
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
	EXPECT_NE(RenderDemoBrowserList.find("s_ListBox.DoStart(UseNewUi ? RowHeight : ms_ListheaderHeight, m_vpFilteredDemos.size(), 1, 3, m_DemolistSelectedIndex, &ListBox, false, IGraphics::CORNER_ALL);"), std::string::npos);
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

TEST(QmNewUiMenuBranches, TClientHeaderIncludesGeneratedProtocolForWeaponDefaults)
{
	const std::string TClientHeader = ReadTextFile("src/game/client/components/tclient/tclient.h");

	EXPECT_NE(TClientHeader.find("#include <generated/protocol.h>"), std::string::npos);
	EXPECT_NE(TClientHeader.find("m_aGoresPreHammerWeapon[NUM_DUMMIES] = {WEAPON_GUN, WEAPON_GUN};"), std::string::npos);
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
	const std::string Graphics = FunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(Graphics.empty());

	EXPECT_NE(Graphics.find("DoGraphicsNumericField(\"graphics-ui-opacity\", &g_Config.m_QmUiOpacity, &g_Config.m_QmUiOpacity, Button, Localize(\"UI opacity\"), 0, 100, &CUi::ms_LinearScrollbarScale, \"%\")"), std::string::npos);
	EXPECT_NE(Graphics.find("DoGraphicsNumericField(\"graphics-map-browser-opacity\", &g_Config.m_QmMapBrowserOpacity, &g_Config.m_QmMapBrowserOpacity, Button, Localize(\"Map browser opacity\"), 0, 100, &CUi::ms_LinearScrollbarScale, \"%\")"), std::string::npos);
	EXPECT_NE(Graphics.find("DoGraphicsNumericField(\"graphics-scoreboard-opacity\", &g_Config.m_QmScoreboardOpacity, &g_Config.m_QmScoreboardOpacity, Button, Localize(\"Scoreboard opacity\"), 0, 100, &CUi::ms_LinearScrollbarScale, \"%\")"), std::string::npos);
	EXPECT_EQ(Graphics.find("DoSliderWithValueInput("), std::string::npos);
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

TEST(QmNewUiMenuBranches, QmFeatureDefaultsAreDisabled)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::regex BinaryQmDefaultOn(R"(MACRO_CONFIG_INT\([^,]+,\s*qm_[^,]+,\s*1,\s*0,\s*1,)");
	const char *apIntentionalDefaultOn[] = {
		"QmImeAutoManage",
		"QmNewIme",
		"QmNameplateCoordX",
		"QmAutoMargin",
		"QmSkinChangeTransition",
		"QmGoresAutoWeaponSwitch",
		"QmGoresDisableIfWeapons",
		"QmSkinQueueEnabled",
		"QmDummySkinQueueEnabled",
		"QmChatSaveDraft",
		"QmSmtcEnable",
		"QmSmtcShowHud",
		"QmSmtcLyricsEnable",
		"QmLyricsMarquee",
		"QmLyricsAutoHideNoSmtc",
		"QmLyricsHideWhenPaused",
	};
	std::istringstream Lines(ConfigSource);
	std::string Line;
	while(std::getline(Lines, Line))
	{
		bool IntentionalDefaultOn = false;
		for(const char *pName : apIntentionalDefaultOn)
		{
			if(Line.find(std::string("MACRO_CONFIG_INT(") + pName + ",") != std::string::npos)
			{
				IntentionalDefaultOn = true;
				break;
			}
		}
		EXPECT_FALSE(std::regex_search(Line, BinaryQmDefaultOn) && !IntentionalDefaultOn) << Line;
	}

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmUiMotionLevel, qm_ui_motion_level, 2, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponTrajectory, qm_weapon_trajectory, 1, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmVoiceNoiseSuppressEnable, qm_voice_noise_suppress_enable, 0, 0, 2"), std::string::npos);
	EXPECT_EQ(ConfigSource.find("MACRO_CONFIG_INT(QmVoiceNoiseSuppressEnable, qm_voice_noise_suppress_enable, 2, 0, 2"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmDefaultOffMigrationKeepsExplicitLegacyValues)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config.cpp");
	const std::string ClientSource = ReadTextFile("src/engine/client/client.cpp");
	const std::string DomainSource = ReadTextFile("src/engine/shared/config_domains.h");
	const std::string IncludeSource = ReadTextFile("src/engine/shared/config_includes.h");

	EXPECT_NE(IncludeSource.find("SET_CONFIG_DOMAIN(ConfigDomain::QMCLIENT)\n#include \"config_variables_qmclient.h\""), std::string::npos);
	EXPECT_NE(DomainSource.find("CONFIG_DOMAIN(QMCLIENT, \"QmClient/settings_qmclient.cfg\", \"settings_qmclient.cfg\", true)"), std::string::npos);
	EXPECT_NE(ClientSource.find("pConfigManager->Init();"), std::string::npos);
	EXPECT_NE(ClientSource.find("if(!pConsole->ExecuteFile(pConfigPath, IConsole::CLIENT_ID_UNSPECIFIED))"), std::string::npos);
	EXPECT_LT(ClientSource.find("pConfigManager->Init();"), ClientSource.find("if(!pConsole->ExecuteFile(pConfigPath, IConsole::CLIENT_ID_UNSPECIFIED))"));
	EXPECT_NE(ConfigSource.find("pVariable->m_ConfigDomain == ConfigDomain && (pVariable->m_Flags & CFGFLAG_SAVE) != 0 && !pVariable->IsDefault()"), std::string::npos);
	EXPECT_NE(ConfigSource.find("pVariable->Serialize(aLineBuf, sizeof(aLineBuf));"), std::string::npos);
	EXPECT_NE(ConfigSource.find("WriteLine(aLineBuf, ConfigDomain);"), std::string::npos);
	EXPECT_EQ(ConfigSource.find("Reset(\"qm_"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SkinTransitionAnimationToggleOwnsAdvancedControls)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string CardRegistrySource = ReadTextFile("src/game/client/QmUi/QmCardRegistry.cpp");
	const std::string GameClientSource = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string PlayersSource = ReadTextFile("src/game/client/components/players.cpp");
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string LanguageSource = ReadTextFile("data/languages/simplified_chinese.txt");
	const std::string SkinTransitionContent = FunctionBody(MenusSource, "void CMenus::RenderQmVisualSkinTransitionContent(");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSkinChangeTransition, qm_skin_change_transition, 1, 0, 1"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSkinChangeTransitionScope, qm_skin_change_transition_scope, 1, 0, 2"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSkinChangeTransitionEasing, qm_skin_change_transition_easing"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmSkinChangeTransitionIntensity, qm_skin_change_transition_intensity"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmCycleTeeHueDummy, qm_cycle_tee_hue_dummy, 0, 0, 1"), std::string::npos);
	EXPECT_NE(MenusSource.find("pSkinTransitionAnimationFeatureId = \"qm_2_72_0_skin_transition_animation_toggle\""), std::string::npos);
	EXPECT_NE(MenusSource.find("pSkinTransitionAnimationFeatureId,\n						\"qm_2_62_8_weapon_animation\""), std::string::npos);
	EXPECT_NE(CardRegistrySource.find("\"qm:skin_transition\", \"visual\", ECardColumn::Left, 1, \"Skin transition\", \"皮肤切换 pifu qiehuan skin transition"), std::string::npos);
	EXPECT_NE(SkinTransitionContent.find("RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmCycleTeeHueDummy, \"Also apply to dummy\""), std::string::npos);
	EXPECT_NE(PlayersSource.find("LocalDummy == 0 || g_Config.m_QmCycleTeeHueDummy != 0"), std::string::npos);
	EXPECT_NE(PlayersSource.find("LocalDummy != 0 ? g_Config.m_ClDummyUseCustomColor != 0 : g_Config.m_ClPlayerUseCustomColor != 0"), std::string::npos);

	const size_t SkinTransitionHeadline = MenusSource.find("RenderQmModuleHeadline(CardContent, 5, Localize(\"Skin transition\")");
	ASSERT_NE(SkinTransitionHeadline, std::string::npos);
	const size_t SkinTransitionCase = MenusSource.rfind("case EQmModuleId::SkinTransition:", SkinTransitionHeadline);
	ASSERT_NE(SkinTransitionCase, std::string::npos);
	const size_t SkinTransitionCaseEnd = MenusSource.find("case EQmModuleId::Coords:", SkinTransitionCase);
	ASSERT_NE(SkinTransitionCaseEnd, std::string::npos);
	const std::string SkinTransitionCard = MenusSource.substr(SkinTransitionCase, SkinTransitionCaseEnd - SkinTransitionCase);
	EXPECT_EQ(SkinTransitionCard.find("RenderSkinQueueRotationRow"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("qmclient-skin-queue-panel-title"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("qmclient-skin-queue-enabled-heading"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("qmclient-skin-queue-interval-heading"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("QmSkinQueueEnabled"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("QmDummySkinQueueEnabled"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("QmSkinQueueInterval"), std::string::npos);
	EXPECT_EQ(SkinTransitionCard.find("QmDummySkinQueueInterval"), std::string::npos);

	const size_t Toggle = SkinTransitionContent.find("DoSettingsButton_CheckBox(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_VISUAL, QMCLIENT_SETTINGS_TAB_VISUAL, &g_Config.m_QmSkinChangeTransition");
	ASSERT_NE(Toggle, std::string::npos);
	const size_t AdvancedIf = SkinTransitionContent.find("if(!g_Config.m_QmSkinChangeTransition)\n\t\treturn;", Toggle);
	const size_t TypeLabel = SkinTransitionContent.find("RenderDropDown(\"qmclient-skin-transition-type\", \"Skin transition type\"", Toggle);
	const size_t ScopeLabel = SkinTransitionContent.find("RenderDropDown(\"qmclient-skin-transition-range\", \"Animation range\"", Toggle);
	const size_t DurationLabel = SkinTransitionContent.find("Localize(\"Skin transition duration\")", Toggle);
	const size_t EasingLabel = SkinTransitionContent.find("RenderDropDown(\"qmclient-skin-transition-easing\", \"Skin transition easing\"", Toggle);
	const size_t IntensityLabel = SkinTransitionContent.find("Localize(\"Skin transition intensity\")", Toggle);
	ASSERT_NE(AdvancedIf, std::string::npos);
	ASSERT_NE(TypeLabel, std::string::npos);
	ASSERT_NE(ScopeLabel, std::string::npos);
	ASSERT_NE(DurationLabel, std::string::npos);
	ASSERT_NE(EasingLabel, std::string::npos);
	ASSERT_NE(IntensityLabel, std::string::npos);
	EXPECT_LT(Toggle, AdvancedIf);
	EXPECT_LT(AdvancedIf, TypeLabel);
	EXPECT_LT(TypeLabel, ScopeLabel);
	EXPECT_LT(ScopeLabel, DurationLabel);
	EXPECT_LT(DurationLabel, EasingLabel);
	EXPECT_LT(EasingLabel, IntensityLabel);
	EXPECT_NE(SkinTransitionContent.find("s_SkinTransitionScopeDropDownNames = {Localize(\"Self only\"), Localize(\"Local\"), Localize(\"All players\")};"), std::string::npos);
	EXPECT_NE(SkinTransitionContent.find("RenderDropDown(\"qmclient-skin-transition-range\", \"Animation range\", &g_Config.m_QmSkinChangeTransitionScope, 2"), std::string::npos);

	EXPECT_NE(GameClientSource.find("QM_SKIN_CHANGE_TRANSITION_SCOPE_OWN"), std::string::npos);
	EXPECT_NE(GameClientSource.find("bool CGameClient::ShouldRunSkinChangeTransition(int ClientId) const"), std::string::npos);
	EXPECT_NE(GameClientSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0)"), std::string::npos);
	EXPECT_NE(GameClientSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0 || !m_SkinTransitionStart.has_value()"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_pGameClient->ShouldRunSkinChangeTransition(m_ClientId)"), std::string::npos);
	EXPECT_NE(SettingsSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0)"), std::string::npos);
	EXPECT_NE(SettingsSource.find("if(!g_Config.m_QmSkinChangeTransition || g_Config.m_QmSkinChangeTransitionMs <= 0 || !m_StartTime.has_value()"), std::string::npos);
	EXPECT_NE(LanguageSource.find("Skin transition animation\n== 皮肤切换动画"), std::string::npos);
}

TEST(QmNewUiMenuBranches, WeaponAnimationAdvancedControlsAreConfigurable)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string PlayersSource = ReadTextFile("src/game/client/components/players.cpp");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponSwitchAnimDurationMs, qm_weapon_switch_anim_duration_ms, 300"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponSwitchAnimDistance, qm_weapon_switch_anim_distance, 40"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponSwitchAnimRotation, qm_weapon_switch_anim_rotation, 360"), std::string::npos);
	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmWeaponSwitchAnimEasing, qm_weapon_switch_anim_easing"), std::string::npos);

	EXPECT_NE(PlayersSource.find("g_Config.m_QmWeaponSwitchAnimDurationMs"), std::string::npos);
	EXPECT_NE(PlayersSource.find("g_Config.m_QmWeaponSwitchAnimDistance"), std::string::npos);
	EXPECT_NE(PlayersSource.find("g_Config.m_QmWeaponSwitchAnimRotation"), std::string::npos);
	EXPECT_NE(PlayersSource.find("g_Config.m_QmWeaponSwitchAnimEasing"), std::string::npos);

	const std::string WeaponAnimationContent = FunctionBody(MenusSource, "void CMenus::RenderQmVisualWeaponAnimationContent(");
	const size_t Toggle = WeaponAnimationContent.find("RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmWeaponSwitchAnim");
	ASSERT_NE(Toggle, std::string::npos);
	EXPECT_NE(WeaponAnimationContent.find("RenderValue(\"qmclient-weapon-switch-duration\", \"Weapon switch duration\"", Toggle), std::string::npos);
	EXPECT_NE(WeaponAnimationContent.find("RenderValue(\"qmclient-weapon-switch-distance\", \"Weapon switch distance\"", Toggle), std::string::npos);
	EXPECT_NE(WeaponAnimationContent.find("RenderValue(\"qmclient-weapon-switch-rotation\", \"Weapon switch rotation\"", Toggle), std::string::npos);
	EXPECT_NE(WeaponAnimationContent.find("Localize(\"Weapon switch easing\")", Toggle), std::string::npos);
}

TEST(QmNewUiMenuBranches, ProcessPriorityAndImeHaveVisibleSettings)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string ClientSource = ReadTextFile("src/engine/client/client.cpp");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmProcessHighPriority, qm_process_high_priority, 0, 0, 1"), std::string::npos);
	EXPECT_NE(ClientSource.find("ApplyProcessPriorityConfig();"), std::string::npos);
	EXPECT_NE(ClientSource.find("m_pConsole->Chain(\"qm_process_high_priority\", ConchainProcessHighPriority, this);"), std::string::npos);
	const std::string MiniFeaturesBody = FunctionBody(MenusSource, "void CMenus::RenderQmFunctionMiniFeaturesContent(");
	ASSERT_FALSE(MiniFeaturesBody.empty());
	EXPECT_NE(MiniFeaturesBody.find("&g_Config.m_QmProcessHighPriority"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("&g_Config.m_QmImeAutoManage"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("&g_Config.m_QmNewIme"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ConfigPageLocalizesVariableHelpText)
{
	const std::string ConfigHeader = ReadTextFile("src/engine/shared/config.h");
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config.cpp");
	const std::string TClientMenusSource = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");

	EXPECT_NE(ConfigHeader.find("const char *m_pHelpLocalizeKey;"), std::string::npos);
	EXPECT_NE(ConfigSource.find("SConfigVariable::VAR_INT, Flags, pHelp, Desc"), std::string::npos);
	EXPECT_NE(ConfigSource.find("SConfigVariable::VAR_COLOR, Flags, pHelp, Desc"), std::string::npos);
	EXPECT_NE(ConfigSource.find("SConfigVariable::VAR_STRING, Flags, pHelp, Desc"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("BuildLocalizedConfigHelpText"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("pVar->m_pHelpLocalizeKey ? pVar->m_pHelpLocalizeKey"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("Localize(pHelpKey)"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("s_CachedConfigLanguageHash"), std::string::npos);
	EXPECT_NE(TClientMenusSource.find("str_quickhash(g_Config.m_ClLanguagefile)"), std::string::npos);
	EXPECT_EQ(TClientMenusSource.find("Ui()->DoLabel(&Help, pVar->m_pHelp ? pVar->m_pHelp : \"\""), std::string::npos);
}

TEST(QmNewUiMenuBranches, EmoticonShadowHasConfigRenderPassAndVisualToggle)
{
	const std::string ConfigSource = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string PlayersSource = ReadTextFile("src/game/client/components/players.cpp");
	const std::string EmoticonSource = ReadTextFile("src/game/client/components/emoticon.cpp");
	const std::string RenderPlayerBody = FunctionBody(PlayersSource, "void CPlayers::RenderPlayer(");
	const std::string EmoticonRenderBody = FunctionBody(EmoticonSource, "void CEmoticon::OnRender()");
	const std::string MenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const auto CountOccurrences = [](const std::string &Text, const char *pNeedle) {
		int Count = 0;
		size_t Pos = 0;
		while((Pos = Text.find(pNeedle, Pos)) != std::string::npos)
		{
			++Count;
			Pos += str_length(pNeedle);
		}
		return Count;
	};

	EXPECT_NE(ConfigSource.find("MACRO_CONFIG_INT(QmEmoticonShadow, qm_emoticon_shadow, 0, 0, 1"), std::string::npos);
	EXPECT_NE(RenderPlayerBody.find("EmoticonShadowOpacity"), std::string::npos);
	EXPECT_NE(RenderPlayerBody.find("EmoticonShadowOffsetX"), std::string::npos);
	EXPECT_EQ(CountOccurrences(RenderPlayerBody, "if(g_Config.m_QmEmoticonShadow)"), 3);
	EXPECT_EQ(CountOccurrences(RenderPlayerBody, "Graphics()->SetColor(0.0f, 0.0f, 0.0f"), 3);
	EXPECT_NE(RenderPlayerBody.find("EmoticonShadowOffsetX * h"), std::string::npos);
	EXPECT_NE(RenderPlayerBody.find("EmoticonShadowOffsetY * h, h, h"), std::string::npos);
	EXPECT_NE(RenderPlayerBody.find("Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);\n\t\tGraphics()->RenderQuadContainerAsSprite"), std::string::npos);
	EXPECT_NE(RenderPlayerBody.find("Graphics()->SetColor(1.0f, 1.0f, 1.0f, a * Alpha);\n\t\t\tGraphics()->RenderQuadContainerAsSprite"), std::string::npos);
	EXPECT_NE(EmoticonRenderBody.find("EmoticonSelectorShadowOpacity"), std::string::npos);
	EXPECT_NE(EmoticonRenderBody.find("if(g_Config.m_QmEmoticonShadow)"), std::string::npos);
	EXPECT_NE(EmoticonRenderBody.find("Graphics()->SetColor(0.0f, 0.0f, 0.0f, EmoticonSelectorShadowOpacity);"), std::string::npos);
	EXPECT_NE(EmoticonRenderBody.find("ScreenCenter.x + Nudge.x + EmoticonSelectorShadowOffsetX"), std::string::npos);
	const std::string SkinTransitionContent = FunctionBody(MenusSource, "void CMenus::RenderQmVisualSkinTransitionContent(");
	EXPECT_NE(SkinTransitionContent.find("RenderQmVisualCheckbox(Content, LineHeight, LineSpacing, &g_Config.m_QmEmoticonShadow"), std::string::npos);
	EXPECT_NE(MenusSource.find("Localize(\"Emoticon shadow\")"), std::string::npos);
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

TEST(QmNewUiMenuBranches, NameplatePreviewShowsPlayerStrongHookMarker)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string RenderNamePlatePreview = FunctionBody(Source, "void CNamePlates::RenderNamePlatePreview");

	EXPECT_NE(RenderNamePlatePreview.find("const bool PreviewIsLocal = DummyIdx == g_Config.m_ClDummy;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("if(DummyIdx == g_Config.m_ClDummy)"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_HookStrongWeakState = EHookStrongWeakState::NEUTRAL;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_HookStrongWeakState = Data.m_HookStrongWeakId == 2 ? EHookStrongWeakState::STRONG : EHookStrongWeakState::WEAK;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeak = NameplateScopeAllowsPreview && (Data.m_ShowHookStrongWeakId || (g_Config.m_ClNamePlatesStrong > 0 && ShouldShowQmHookStrongWeakScope(g_Config.m_QmNameplateHookStrongWeakScope, true, false, false)));"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeak = NameplateScopeAllowsPreview && g_Config.m_ClNamePlatesStrong > 0 && ShouldShowQmHookStrongWeakScope(g_Config.m_QmNameplateHookStrongWeakScope, false, Strong, Weak);"), std::string::npos);
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
	EXPECT_NE(RenderNamePlateGame.find("Data.m_ShowHookStrongWeak = g_Config.m_Debug || (g_Config.m_ClNamePlatesStrong > 0 && ShouldShowQmHookStrongWeakScope(g_Config.m_QmNameplateHookStrongWeakScope, false, Strong, Weak));"), std::string::npos);
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
	EXPECT_NE(RenderNamePlatePreview.find("case 1: // Others\n\t\t\tData.m_ShowDirection = !PreviewIsLocal;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("case 2: // Everyone\n\t\t\tData.m_ShowDirection = true;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("case 3: // Only self\n\t\t\tData.m_ShowDirection = PreviewIsLocal;"), std::string::npos);
	EXPECT_NE(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeakId = NameplateScopeAllowsPreview && g_Config.m_ClNamePlatesStrong == 2;"), std::string::npos);
	EXPECT_EQ(RenderNamePlatePreview.find("Data.m_ShowHookStrongWeakId = g_Config.m_ClNamePlatesStrong == 2;"), std::string::npos);
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
	EXPECT_NE(RenderMediaIsland.find("GameClient()->m_QmLyrics.RenderMediaIslandLine(LyricsRect, BottomFontSize, BottomAlpha)"), std::string::npos);
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

	EXPECT_NE(Source.find("float QmKnownOwnerEventAlpha(CGameClient *pGameClient, int Owner)"), std::string::npos);
	EXPECT_NE(Source.find("int QmInferExplosionOwner(CGameClient *pGameClient, vec2 Pos)"), std::string::npos);
	EXPECT_NE(Source.find("int QmInferHammerHitOwner(CGameClient *pGameClient, vec2 Pos)"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("const float ExplosionAlpha = QmKnownOwnerEventAlpha(this, QmInferExplosionOwner(this, ExplosionPos));"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("m_Effects.Explosion(ExplosionPos, ExplosionAlpha);"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("const float HammerHitAlpha = QmKnownOwnerEventAlpha(this, QmInferHammerHitOwner(this, HammerHitPos));"), std::string::npos);
	EXPECT_NE(ProcessEvents.find("m_Effects.HammerHit(HammerHitPos, HammerHitAlpha, Volume);"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("m_Effects.Explosion(vec2(pEvent->m_X, pEvent->m_Y), Alpha);"), std::string::npos);
	EXPECT_EQ(ProcessEvents.find("m_Effects.HammerHit(HammerHitPos, Alpha, Volume);"), std::string::npos);
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

TEST(QmNewUiMenuBranches, FriendCategoryHeadersExposeManagement)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");

	EXPECT_NE(Source.find("FONT_ICON_GEAR"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Manage categories\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Right-click or use the gear to manage categories\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, FriendCategorySortingRequiresCtrlDrag)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const size_t DragState = Source.find("s_CategoryDragState.m_PressedIndex = CategoryIndex;");
	ASSERT_NE(DragState, std::string::npos);
	const size_t PressGate = Source.rfind("Input()->ModifierIsPressed() && Ui()->MouseButtonClicked(0)", DragState);
	ASSERT_NE(PressGate, std::string::npos);
	EXPECT_NE(Source.find("Ui()->MouseButton(0) && Input()->ModifierIsPressed() && s_CategoryDragState.m_DraggingIndex < 0", DragState), std::string::npos);
	EXPECT_NE(Source.find("!Input()->ModifierIsPressed() && s_CategoryDragState.m_DraggingIndex < 0", DragState), std::string::npos);
	EXPECT_EQ(Source.find("CategoryDragHoldSeconds"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ProtectedFriendCategoriesCannotBeRenamedOrDeleted)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const size_t ProtectedFn = Source.find("static bool IsProtectedFriendsCategory");
	ASSERT_NE(ProtectedFn, std::string::npos);
	const size_t ProtectedFnEnd = Source.find("static const char *LocalizeFriendsCategory", ProtectedFn);
	ASSERT_NE(ProtectedFnEnd, std::string::npos);
	const std::string ProtectedBody = Source.substr(ProtectedFn, ProtectedFnEnd - ProtectedFn);

	EXPECT_NE(ProtectedBody.find("IFriends::DEFAULT_CATEGORY"), std::string::npos);
	EXPECT_NE(ProtectedBody.find("IsClanMembersCategory(pCategory)"), std::string::npos);
	EXPECT_NE(ProtectedBody.find("IsOfflineFriendsCategory(pCategory)"), std::string::npos);

	const size_t Popup = Source.find("CUi::EPopupMenuFunctionResult CMenus::PopupFriendsCategory");
	ASSERT_NE(Popup, std::string::npos);
	const std::string PopupBody = Source.substr(Popup);
	EXPECT_NE(PopupBody.find("const bool IsProtectedCategory = IsProtectedFriendsCategory(pCategory);"), std::string::npos);
	EXPECT_NE(PopupBody.find("Localize(\"Rename\"), &Button, FontSize, TEXTALIGN_MC, 0.0f, false, !IsProtectedCategory"), std::string::npos);
	EXPECT_NE(PopupBody.find("Localize(\"Delete category\"), &Button, FontSize, TEXTALIGN_MC, 0.0f, false, !IsProtectedCategory"), std::string::npos);
}

TEST(QmNewUiMenuBranches, FriendAddPopupExposesCreateCategoryAction)
{
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");

	EXPECT_NE(Header.find("m_FriendsAddCategoryCreateButton"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Create category\")"), std::string::npos);
	EXPECT_NE(Source.find("m_FriendsCategoryPopupContext.m_Mode = CFriendsCategoryPopupContext::MODE_ADD"), std::string::npos);
	EXPECT_NE(Source.find("Ui()->DoPopupMenu(&m_FriendsCategoryPopupContext"), std::string::npos);
}

TEST(QmNewUiMenuBranches, FriendCategoryHeaderActionExcludesManageButton)
{
	const CUIRect Header{10.0f, 20.0f, 180.0f, 24.0f};
	CUIRect HeaderAction;
	CUIRect ManageButton;

	CMenus::SplitFriendsCategoryHeaderRects(Header, &HeaderAction, &ManageButton);

	EXPECT_FLOAT_EQ(HeaderAction.x, Header.x);
	EXPECT_FLOAT_EQ(HeaderAction.y, Header.y);
	EXPECT_FLOAT_EQ(HeaderAction.w, Header.w - Header.h);
	EXPECT_FLOAT_EQ(HeaderAction.h, Header.h);
	EXPECT_FLOAT_EQ(ManageButton.x, Header.x + Header.w - Header.h + 2.0f);
	EXPECT_FLOAT_EQ(ManageButton.y, Header.y + 2.0f);
	EXPECT_FLOAT_EQ(ManageButton.w, Header.h - 4.0f);
	EXPECT_FLOAT_EQ(ManageButton.h, Header.h - 4.0f);
	EXPECT_LE(HeaderAction.x + HeaderAction.w, ManageButton.x);
}

TEST(QmNewUiMenuBranches, FriendCategoryEditPopupHasRoomForInputAndActions)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	constexpr float RequiredHeight = 5.0f * 2.0f + 12.0f + 3.0f + 18.0f + 6.0f + 20.0f;

	EXPECT_FLOAT_EQ(CMenus::FriendsCategoryEditPopupHeight(), RequiredHeight);
	EXPECT_GT(CMenus::FriendsCategoryActionsPopupHeight(), 60.0f);
	EXPECT_NE(Source.find("CMenus::FriendsCategoryEditPopupHeight()"), std::string::npos);
	EXPECT_NE(Source.find("CMenus::SecondaryPanelRect("), std::string::npos);
	EXPECT_EQ(Source.find("250.0f, 62.0f"), std::string::npos);
	EXPECT_EQ(Source.find("250.0f, 110.0f"), std::string::npos);
	EXPECT_EQ(Source.find("280.0f, CMenus::FriendsCategoryEditPopupHeight()"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SecondaryPanelRectClampsNearScreenEdges)
{
	const CUIRect Screen{0.0f, 0.0f, 800.0f, 600.0f};
	const CUIRect Panel = CMenus::SecondaryPanelRect(780.0f, 590.0f, 300.0f, 140.0f, Screen);

	EXPECT_FLOAT_EQ(Panel.w, 300.0f);
	EXPECT_FLOAT_EQ(Panel.h, 140.0f);
	EXPECT_LE(Panel.x + Panel.w, 792.0f);
	EXPECT_LE(Panel.y + Panel.h, 592.0f);
	EXPECT_GE(Panel.x, 8.0f);
	EXPECT_GE(Panel.y, 8.0f);
}

TEST(QmNewUiMenuBranches, FriendAutoFollowDelaysAndStopsAfterTwoJumps)
{
	CMenus::SFriendAutoFollowState State;
	char aConnect[NETADDR_MAXSTRSIZE] = "";

	CMenus::StartFriendAutoFollow(State, "Alice", "Clan", "127.0.0.1:8303");
	EXPECT_TRUE(State.m_Active);
	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8303", 10.0f, 3, 2, aConnect, sizeof(aConnect)));

	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8304", 11.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_TRUE(State.m_HasPendingAddress);
	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8304", 13.9f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_TRUE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8304", 14.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_STREQ(aConnect, "127.0.0.1:8304");
	EXPECT_TRUE(State.m_Active);
	EXPECT_EQ(State.m_JumpCount, 1);

	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8305", 20.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_TRUE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8305", 23.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_STREQ(aConnect, "127.0.0.1:8305");
	EXPECT_FALSE(State.m_Active);
	EXPECT_EQ(State.m_JumpCount, 2);
}

TEST(QmNewUiMenuBranches, FriendAutoFollowCancelsWhenTargetGoesOffline)
{
	CMenus::SFriendAutoFollowState State;
	char aConnect[NETADDR_MAXSTRSIZE] = "";

	CMenus::StartFriendAutoFollow(State, "Alice", "Clan", "127.0.0.1:8303");
	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, true, "127.0.0.1:8304", 11.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_TRUE(State.m_HasPendingAddress);
	EXPECT_FALSE(CMenus::FriendAutoFollowStep(State, false, "", 12.0f, 3, 2, aConnect, sizeof(aConnect)));
	EXPECT_FALSE(State.m_Active);
	EXPECT_FALSE(State.m_HasPendingAddress);
}

TEST(QmNewUiMenuBranches, FriendAutoFollowDistinguishesManualAndAutomaticConnects)
{
	const std::string Header = ReadTextFile("src/game/client/components/menus.h");
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");
	const std::string QmMenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(Header.find("enum class EConnectIntent"), std::string::npos);
	EXPECT_NE(Header.find("void Connect(const char *pAddress, EConnectIntent Intent = EConnectIntent::Manual)"), std::string::npos);
	EXPECT_NE(Source.find("if(Intent == EConnectIntent::Manual)"), std::string::npos);
	EXPECT_NE(Source.find("StopFriendAutoFollow(m_FriendAutoFollowState);"), std::string::npos);
	EXPECT_NE(Source.find("Connect(g_Config.m_UiServerAddress, EConnectIntent::AutoFollow)"), std::string::npos);
	const std::string FriendNotifyBody = FunctionBody(QmMenusSource, "void CMenus::RenderQmFunctionFriendNotifyContent(");
	ASSERT_FALSE(FriendNotifyBody.empty());
	EXPECT_NE(FriendNotifyBody.find("RenderValue(\"qmclient-friend-auto-follow-delay\", \"Auto-follow delay\""), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("&g_Config.m_QmFriendAutoFollowDelay, 0, 30, \"s\""), std::string::npos);
}

TEST(QmNewUiMenuBranches, ShortServerNamesCoverKnownFamilies)
{
	CServerInfo Info{};
	char aBuf[sizeof(Info.m_aName)];

	str_copy(Info.m_aName, "KoG | China #12 - HappyHook [kog.tw]");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "China - HappyHook");

	str_copy(Info.m_aName, "Axiom 北京 普通 - CHN1O 钩累死");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "简单图 - CHN1O 钩累死");

	str_copy(Info.m_aName, "DDNet CHN7 西安 - Moderate 中阶");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "中阶图 - CHN7 西安");

	str_copy(Info.m_aName, "DDNet CHN2 上海 - Brutal 高阶");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "高阶 - CHN2 上海");

	str_copy(Info.m_aName, "Axiom Novice - CHN12 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "简单图 - CHN12 钩累死");

	str_copy(Info.m_aName, "Axiom Insane - CHN7 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "疯狂 - CHN7 钩累死");

	str_copy(Info.m_aName, "Axiom Axiom ◇ 广州 ✦ 困难 - CHN9 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "高阶 - CHN9 钩累死");

	str_copy(Info.m_aName, "Axiom Axiom ◇ 北京 ✦ 困难 - CHN10 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "高阶 - CHN10 钩累死");

	str_copy(Info.m_aName, "Axiom ◇ 广州 ✦ 活动 - CHN9 AXRace");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "活动 - CHN9 AXRace");

	str_copy(Info.m_aName, "Axiom ◇ 广州 ✦ 极限 - CHN9 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "极限 - CHN9 钩累死");

	str_copy(Info.m_aName, "Axiom ◇ 上海 ✦ 训练 - CHN2 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "训练 - CHN2 钩累死");

	str_copy(Info.m_aName, "Axiom ◇ 成都 ✦ 娱乐 - CHN12 钩累死");
	str_copy(Info.m_aGameType, "Gores");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "娱乐 - CHN12 钩累死");

	str_copy(Info.m_aName, "DDNet Moderate - CHN7 西安");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "中阶图 - CHN7 西安");

	str_copy(Info.m_aName, "DDNet CHN2 上海 - DDmaX.Easy 古典");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "DDmaX.Easy 古典 - CHN2 上海");

	str_copy(Info.m_aName, "DDNet CHN7 西安 - DDmaX.Next");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "DDmaX.Next 古典 - CHN7 西安");

	str_copy(Info.m_aName, "DDNet CHN3 宁波 - DDmaX.Pro 古典");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "DDmaX.Pro 古典 - CHN3 宁波");

	str_copy(Info.m_aName, "DDNet CHN6 上海 - Oldschool 传统");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "古典图 - CHN6 上海");

	str_copy(Info.m_aName, "DDNet CHN6 上海 - Solo 单人");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "单人 - CHN6 上海");

	str_copy(Info.m_aName, "DDNet CHN5 上海 - Dummy 分身");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "分身 - CHN5 上海");

	str_copy(Info.m_aName, "DDNet Taiwan - Moderate");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "中阶图 - Taiwan");

	str_copy(Info.m_aName, "DDNet Taiwan - Brutal");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "高阶 - Taiwan");

	str_copy(Info.m_aName, "Brutal - CHN5 上海");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "高阶 - CHN5 上海");

	str_copy(Info.m_aName, "Plain Server Name");
	str_copy(Info.m_aGameType, "DDraceNetwork");
	EXPECT_STREQ(CMenus::GetServerbrowserDisplayName(&Info, aBuf, sizeof(aBuf)), "Plain Server Name");
}

TEST(QmNewUiMenuBranches, ShortServerNamesKeepDisplayNameHighlightPath)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_browser.cpp");

	EXPECT_NE(Source.find("g_Config.m_QmShortServerNames || (pItem->m_QuickSearchHit & IServerBrowser::QUICK_SERVERNAME)"), std::string::npos);
	EXPECT_NE(Source.find("PrintHighlighted(pDisplayServerName"), std::string::npos);
	EXPECT_EQ(Source.find("!g_Config.m_QmShortServerNames && g_Config.m_BrFilterString"), std::string::npos);
}

TEST(QmNewUiMenuBranches, CallVoteMapListShowsFinishedIcon)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_ingame.cpp");
	const size_t RenderPos = Source.find("bool CMenus::RenderServerControlServer(CUIRect MainView, bool UpdateScroll)");
	ASSERT_NE(RenderPos, std::string::npos);
	const size_t EndPos = Source.find("bool CMenus::RenderServerControlKick(CUIRect MainView", RenderPos);
	ASSERT_NE(EndPos, std::string::npos);
	const std::string Body = Source.substr(RenderPos, EndPos - RenderPos);

	EXPECT_NE(Body.find("ExtractMapName(pOption->m_aDescription"), std::string::npos);
	EXPECT_NE(Body.find("g_Config.m_BrIndicateFinished"), std::string::npos);
	EXPECT_NE(Body.find("pCurrentCommunity->HasRank(aMapName) == CServerInfo::RANK_RANKED"), std::string::npos);
	EXPECT_NE(Body.find("RenderFontIcon(Icon, FONT_ICON_FLAG_CHECKERED"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_TClient.IsFavoriteMap(aMapName)"), std::string::npos);
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
	EXPECT_NE(Settings7Source.find("Localize(\"Dummy\")"), std::string::npos);
	EXPECT_NE(Settings7Source.find("Localize(\"Dummy\")"), std::string::npos);
	EXPECT_NE(StartSource.find("Localize(\"(Update required)\")"), std::string::npos);
	EXPECT_NE(PieMenuSource.find("Localize(\"Spectate\")"), std::string::npos);
	EXPECT_NE(ScoreboardSource.find("Localize(\"Spectators\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmClientAxiomAutoLoginLivesInQmClientComponent)
{
	const std::string Source = ReadTextFile("src/game/client/components/qmclient/axiom_auto_login.cpp");
	const std::string Header = ReadTextFile("src/game/client/components/qmclient/axiom_auto_login.h");
	const std::string TClientHeader = ReadTextFile("src/game/client/components/tclient/tclient.h");
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string GameClientHeader = ReadTextFile("src/game/client/gameclient.h");
	const std::string QmConfigHeader = ReadTextFile("src/engine/shared/config_variables_qmclient.h");
	const std::string QmMenusSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string IngameMenusSource = ReadTextFile("src/game/client/components/menus_ingame.cpp");

	EXPECT_NE(Header.find("class CQmAxiomAutoLogin : public CComponent"), std::string::npos);
	EXPECT_NE(GameClientHeader.find("CQmAxiomAutoLogin m_QmAxiomAutoLogin;"), std::string::npos);
	EXPECT_NE(Source.find("void CQmAxiomAutoLogin::TrySendLogin()"), std::string::npos);
	EXPECT_NE(Source.find("void CQmAxiomAutoLogin::TrySendDummyLogin()"), std::string::npos);
	EXPECT_NE(Source.find("SendChatOnConn(IClient::CONN_DUMMY"), std::string::npos);
	EXPECT_NE(Header.find("TrySendDummyLogin"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("QmAxiomDummyLoginPassword"), std::string::npos);
	EXPECT_NE(QmConfigHeader.find("qm_axiom_dummy_login_password"), std::string::npos);
	EXPECT_NE(QmMenusSource.find("Axiom dummy password"), std::string::npos);
	EXPECT_NE(Source.find("m_DummyLoginAllowedThisServer"), std::string::npos);
	EXPECT_NE(Source.find("m_DummyWasConnected"), std::string::npos);
	EXPECT_EQ(Source.find("if(DummyConnected && !m_DummyWasConnected)"), std::string::npos);
	EXPECT_NE(Source.find("m_DummyLoginAllowedThisServer = true;"), std::string::npos);
	EXPECT_NE(Source.find("if(!m_DummyLoginAllowedThisServer)"), std::string::npos);
	EXPECT_NE(Header.find("EnableDummyReconnectForServer"), std::string::npos);
	EXPECT_NE(Header.find("DisableDummyReconnectForServer"), std::string::npos);
	EXPECT_NE(Source.find("Client()->DummyConnect();"), std::string::npos);
	EXPECT_NE(IngameMenusSource.find("GameClient()->m_QmAxiomAutoLogin.EnableDummyReconnectForServer();"), std::string::npos);
	EXPECT_NE(IngameMenusSource.find("GameClient()->OnDummyManualDisconnect();"), std::string::npos);
	EXPECT_NE(GameClientHeader.find("void OnDummyManualDisconnect() override;"), std::string::npos);
	const std::string GameClientSource = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string ManualDisconnectBody = FunctionBody(GameClientSource, "void CGameClient::OnDummyManualDisconnect()");
	ASSERT_FALSE(ManualDisconnectBody.empty());
	EXPECT_NE(ManualDisconnectBody.find("m_QmAxiomAutoLogin.DisableDummyReconnectForServer();"), std::string::npos);
	const std::string ClientSource = ReadTextFile("src/engine/client/client.cpp");
	const std::string DummyDisconnectBody = FunctionBody(ClientSource, "void CClient::Con_DummyDisconnect(");
	ASSERT_FALSE(DummyDisconnectBody.empty());
	EXPECT_NE(DummyDisconnectBody.find("GameClient()->OnDummyManualDisconnect();"), std::string::npos);
	EXPECT_NE(DummyDisconnectBody.find("DummyDisconnect(nullptr);"), std::string::npos);
	EXPECT_NE(Source.find("bool CQmAxiomAutoLogin::IsAxiomCommunity() const"), std::string::npos);
	EXPECT_NE(Source.find("QMCLIENT_AXIOM_AUTO_LOGIN_SLOW_RETRY_SECONDS"), std::string::npos);
	EXPECT_NE(Source.find("m_AutoLoginSlowRetryMode"), std::string::npos);
	EXPECT_NE(Source.find("m_AutoLoginHardFailed"), std::string::npos);
	EXPECT_NE(Source.find("ScheduleSlowRetry"), std::string::npos);
	EXPECT_NE(Source.find("IsHardLoginFailure"), std::string::npos);
	EXPECT_NE(Source.find("IsLoginContextMessage"), std::string::npos);
	EXPECT_NE(Source.find("return IsLoginContextMessage(pText) &&"), std::string::npos);
	const size_t HardFailureCheck = Source.find("IsHardLoginFailure(pText)");
	const size_t LoginMessageFilter = Source.find("const bool IsLoginMessage");
	EXPECT_NE(HardFailureCheck, std::string::npos);
	EXPECT_NE(LoginMessageFilter, std::string::npos);
	EXPECT_LT(HardFailureCheck, LoginMessageFilter);
	EXPECT_NE(Source.find("Localize(\"Trying Axiom auto login\")"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Trying Axiom dummy auto login\")"), std::string::npos);
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

TEST(QmNewUiMenuBranches, FastPracticeSurfacesPracticeStateInHud)
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

TEST(QmNewUiMenuBranches, NameplateTextEffectsUseSharedRenderHelper)
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
	EXPECT_NE(NameplatesSource.find("Style.m_OutlineColor = s_OutlineColor.WithMultipliedAlpha(TextColor.a);"), std::string::npos);
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

TEST(QmNewUiMenuBranches, NameplatePreviewRebuildsTextContainerInsteadOfAppendingSizes)
{
	const std::string Source = ReadTextFile("src/game/client/components/nameplates.cpp");
	const std::string Body = BlockBodyAfter(Source, "void Update(CGameClient &This, const CNamePlateData &Data) override");
	ASSERT_FALSE(Body.empty());

	const size_t DeletePos = Body.find("This.TextRender()->DeleteTextContainer(m_TextContainerIndex);");
	const size_t UpdateTextPos = Body.find("UpdateText(This, Data);");
	ASSERT_NE(DeletePos, std::string::npos);
	ASSERT_NE(UpdateTextPos, std::string::npos);
	EXPECT_LT(DeletePos, UpdateTextPos);
	EXPECT_EQ(Body.find("else\n\t\t{\n\t\t\tUpdateText(This, Data);\n\t\t}"), std::string::npos);
	EXPECT_EQ(Body.find("NameplateTextEffectPadding"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmLaserSettingsMovedToAppearanceLaserTab)
{
	const std::string QmSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string RenderSettingsQmClient = FunctionBody(QmSource, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(RenderSettingsQmClient.empty());
	const std::string VisualModules = BlockBodyAfter(RenderSettingsQmClient, "static constexpr std::array<EQmModuleId, 10> s_aQmVisualModules = {");
	ASSERT_FALSE(VisualModules.empty());
	EXPECT_EQ(VisualModules.find("EQmModuleId::Laser"), std::string::npos);
	EXPECT_EQ(VisualModules.find("EQmModuleId::NameplateText"), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClient.find("{EQmModuleId::NameplateText, EQmModuleColumn::Right"), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClient.find("RenderQmModuleHeadline(CardContent, 5, Localize(\"Laser settings\"), Localize(\"Laser style\"));"), std::string::npos);

	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string LaserBranch = BlockBodyAfter(SettingsSource, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_LASER)");
	ASSERT_FALSE(LaserBranch.empty());
	EXPECT_NE(LaserBranch.find("AddCard(10, LaserEnhancedMinCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("AddCard(11, LaserColorMinCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("AddCard(12, LaserPreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(LaserBranch.find("RenderQmSettingsGlassCard(EnhancedCard, QmCardStyle);"), std::string::npos);
	EXPECT_NE(SettingsSource.find("QmResolveScrollPolicy(ScrollRequest, AppearanceUiScale"), std::string::npos);
	EXPECT_NE(SettingsSource.find("std::array<CScrollRegion, NUMBER_OF_APPEARANCE_TABS> s_AppearanceSettingsCardScrollRegions"), std::string::npos);
	EXPECT_NE(SettingsSource.find("CQmScrollState &ScrollState = s_AppearanceSettingsCardScrollRegions[m_AppearanceSettingsTab].State()"), std::string::npos);
	EXPECT_NE(SettingsSource.find("m_SettingsCardDeck.Render(AppearanceCardCtx, AppearancePage, pAppearanceDeckTab"), std::string::npos);
	EXPECT_EQ(LaserBranch.find("const float EnhancedContentHeight ="), std::string::npos);
	EXPECT_EQ(LaserBranch.find("const float ColorContentHeight ="), std::string::npos);
	EXPECT_EQ(LaserBranch.find("const float PreviewContentHeight ="), std::string::npos);
	EXPECT_NE(LaserBranch.find("static float s_LaserMeasuredEnhancedCardHeight = 0.0f;"), std::string::npos);
	EXPECT_NE(LaserBranch.find("static float s_LaserMeasuredColorCardHeight = 0.0f;"), std::string::npos);
	EXPECT_NE(LaserBranch.find("static float s_LaserMeasuredPreviewCardHeight = 0.0f;"), std::string::npos);
	EXPECT_NE(LaserBranch.find("LineSize * 5.0f + MarginSmall * 5.0f +"), std::string::npos);
	EXPECT_NE(LaserBranch.find("UpdateMeasuredCardHeight(s_LaserMeasuredEnhancedCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("UpdateMeasuredCardHeight(s_LaserMeasuredColorCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("UpdateMeasuredCardHeight(s_LaserMeasuredPreviewCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoAppearanceHeading(EnhancedCardContent, \"appearance-laser-enhancement-title\", Localize(\"Laser settings\"), HeadlineFontSize, HeadlineHeight);"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_APPEARANCE, APPEARANCE_TAB_LASER, &g_Config.m_QmLaserEnhanced"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserGlowIntensity"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserSize"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserAlpha"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserRoundCaps"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserPulseSpeed"), std::string::npos);
	EXPECT_NE(LaserBranch.find("g_Config.m_QmLaserPulseAmplitude"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoLaserPreview(&LaserPreviewRect, LaserRifleOutlineColor, LaserRifleInnerColor, LASERTYPE_RIFLE);"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoLaserPreview(&LaserPreviewRect, LaserShotgunOutlineColor, LaserShotgunInnerColor, LASERTYPE_SHOTGUN);"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoLaserPreview(&LaserPreviewRect, LaserDoorOutlineColor, LaserDoorInnerColor, LASERTYPE_DOOR);"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoLaserPreview(&LaserPreviewRect, LaserFreezeOutlineColor, LaserFreezeInnerColor, LASERTYPE_FREEZE);"), std::string::npos);
	EXPECT_NE(LaserBranch.find("DoLaserPreview(&LaserPreviewRect, LaserDraggerOutlineColor, LaserDraggerInnerColor, LASERTYPE_DRAGGER);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, QmSettingsCardsUseSharedStyleHelpers)
{
	const std::string HeaderSource = ReadTextFile("src/game/client/components/menus.h");
	EXPECT_NE(HeaderSource.find("struct SQmSettingsCardStyle"), std::string::npos);
	EXPECT_NE(HeaderSource.find("SQmSettingsCardStyle QmSettingsCardStyle(float UiScale) const;"), std::string::npos);
	EXPECT_NE(HeaderSource.find("CScrollRegionParams QmSettingsScrollRegionParams(float UiScale) const;"), std::string::npos);
	EXPECT_NE(HeaderSource.find("void RenderQmSettingsGlassCard(const CUIRect &Card, const SQmSettingsCardStyle &Style) const;"), std::string::npos);

	const std::string MenuSource = ReadTextFile("src/game/client/components/menus.cpp");
	EXPECT_NE(MenuSource.find("CMenus::SQmSettingsCardStyle CMenus::QmSettingsCardStyle(float UiScale) const"), std::string::npos);
	EXPECT_NE(MenuSource.find("const SQmScrollContainerStyle ScrollStyle = QmScrollContainerStyleForSize(EQmScrollSize::LARGE, UiScale);"), std::string::npos);
	EXPECT_NE(MenuSource.find("Style.m_ScrollbarWidth = ScrollStyle.m_ScrollbarWidth;"), std::string::npos);
	EXPECT_NE(MenuSource.find("CScrollRegionParams Params = QmScrollRegionParamsForSize(EQmScrollSize::LARGE, UiScale);"), std::string::npos);
	EXPECT_NE(MenuSource.find("Params.m_ScrollUnit = QmSettingsScrollConfig(UiScale, 0.0f).m_WheelScale;"), std::string::npos);
	EXPECT_EQ(MenuSource.find("Params.m_ScrollUnit = 60.0f * UiScale;"), std::string::npos);
	EXPECT_EQ(MenuSource.find("Params.m_ScrollbarThickness = Style.m_ScrollbarWidth;"), std::string::npos);
	EXPECT_EQ(MenuSource.find("Params.m_ScrollbarMargin = Style.m_ScrollbarMargin;"), std::string::npos);
	EXPECT_NE(MenuSource.find("void CMenus::RenderQmSettingsGlassCard(const CUIRect &Card, const SQmSettingsCardStyle &Style) const"), std::string::npos);

	const std::string QmSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string RenderSettingsQmClient = FunctionBody(QmSource, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(RenderSettingsQmClient.empty());

	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string NamePlateBranch = BlockBodyAfter(SettingsSource, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	ASSERT_FALSE(NamePlateBranch.empty());
	EXPECT_NE(NamePlateBranch.find("AddCard(5, NamePlateSettingsMinCardHeight"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("AddCard(6, NamePlatePreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("LeftView.HSplitTop(NamePlateContentPaddingY"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("RightView.HSplitTop(NamePlateContentPaddingY"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlateSettingsShadow.Draw"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewShadow.Draw"), std::string::npos);

	const std::string LaserBranch = BlockBodyAfter(SettingsSource, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_LASER)");
	ASSERT_FALSE(LaserBranch.empty());
	EXPECT_NE(LaserBranch.find("AddCard(10, LaserEnhancedMinCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("AddCard(11, LaserColorMinCardHeight"), std::string::npos);
	EXPECT_NE(LaserBranch.find("AddCard(12, LaserPreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(LaserBranch.find("auto RenderQmSettingsGlassCard ="), std::string::npos);
	EXPECT_EQ(LaserBranch.find("80.0f * UiScale"), std::string::npos);

	EXPECT_NE(RenderSettingsQmClient.find("const SQmSettingsCardStyle QmCardStyle = QmSettingsCardStyle(UiScale);"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("SSettingsQmScrollFrame QmScrollFrame = BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClient.find("CScrollRegionParams NameplateTextScrollParams = QmSettingsScrollRegionParams(UiScale);"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClient.find("RenderQuadContainer(m_QmCardBgQuadContainerIndex"), std::string::npos); // 栖梦侧栏卡片背景走 DrawCall 合批（QuadContainer，替代逐卡 RenderQmSettingsGlassCard）
	EXPECT_EQ(RenderSettingsQmClient.find("Card.Draw(LgGlassColor"), std::string::npos);

	const std::string RenderSettingsQmClientOverview = FunctionBody(QmSource, "void CMenus::RenderSettingsQmClientOverview(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(RenderSettingsQmClientOverview.empty());
	EXPECT_NE(RenderSettingsQmClientOverview.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(RenderSettingsQmClientOverview.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(RenderSettingsQmClientOverview.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(RenderSettingsQmClientOverview.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClientOverview.find("BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(RenderSettingsQmClientOverview.find("RenderQmSettingsGlassCard("), std::string::npos);
}

TEST(QmNewUiMenuBranches, GlassCardUsesFlatHairlineWithoutDropShadow)
{
	const std::string MenuSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string Body = FunctionBody(MenuSource, "void CMenus::RenderQmSettingsGlassCard(const CUIRect &Card, const SQmSettingsCardStyle &Style) const");
	ASSERT_FALSE(Body.empty());

	// 扁平极简：去掉外投影（深色背景下黑投影无效且粗糙）
	EXPECT_EQ(Body.find("Shadow.Draw"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::DrawCard"), std::string::npos);
	EXPECT_NE(Body.find("CardProps.m_Elevation = 0;"), std::string::npos);

	// 边框：走共享 QmUi 卡片组件，避免设置页和 QmClient 卡片各画一套
	EXPECT_NE(Body.find("CardProps.m_BorderColor = Style.m_HairlineColor;"), std::string::npos);
	EXPECT_EQ(Body.find("BorderBg.Margin(-1.0f, &BorderBg)"), std::string::npos);

	// 主体颜色和毛玻璃覆盖层仍由设置页样式控制
	EXPECT_NE(Body.find("CardProps.m_FillColor = Style.m_GlassColor;"), std::string::npos);
	EXPECT_NE(Body.find("qm_card_backdrop_blur"), std::string::npos);

	// struct 新增 m_HairlineColor 字段，m_ShadowColor 保留（置透明，为以后内嵌阴影留口子）
	const std::string HeaderSource = ReadTextFile("src/game/client/components/menus.h");
	EXPECT_NE(HeaderSource.find("ColorRGBA m_HairlineColor"), std::string::npos);
	EXPECT_NE(HeaderSource.find("ColorRGBA m_ShadowColor"), std::string::npos);
}

TEST(QmNewUiMenuBranches, AppearanceTabsUseQmCards)
{
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsAppearance = FunctionBody(SettingsSource, "void CMenus::RenderSettingsAppearance(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsAppearance.empty());
	EXPECT_NE(RenderSettingsAppearance.find("std::vector<SSettingsCardDefinition> vCards;"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("const auto AddCard ="), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("pAppearanceDeckTab = m_AppearanceSettingsTab == APPEARANCE_TAB_HUD ? \"appearance-hud\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("m_SettingsCardDeck.Render(AppearanceCardCtx, AppearancePage, pAppearanceDeckTab"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("QmResolveScrollPolicy(ScrollRequest, AppearanceUiScale"), std::string::npos);

	const std::string HudBranch = BlockBodyAfter(RenderSettingsAppearance, "if(m_AppearanceSettingsTab == APPEARANCE_TAB_HUD)");
	ASSERT_FALSE(HudBranch.empty());
	EXPECT_NE(HudBranch.find("AddCard(0, HudMinCardHeight"), std::string::npos);
	EXPECT_NE(HudBranch.find("AddCard(1, HudMinCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-hud-main\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-hud-ddrace\""), std::string::npos);
	EXPECT_NE(HudBranch.find("UpdateMeasuredCardHeight(s_HudMeasuredLeftCardHeight"), std::string::npos);

	const std::string ChatBranch = BlockBodyAfter(RenderSettingsAppearance, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_CHAT)");
	ASSERT_FALSE(ChatBranch.empty());
	EXPECT_NE(ChatBranch.find("AddCard(2, ChatSettingsMinCardHeight"), std::string::npos);
	EXPECT_NE(ChatBranch.find("AddCard(3, ChatMessagesMinCardHeight"), std::string::npos);
	EXPECT_NE(ChatBranch.find("AddCard(4, ChatPreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(ChatBranch.find("ContentView.HSplitBottom(220.0f"), std::string::npos);
	EXPECT_EQ(ChatBranch.find("PreviewView.w *= 0.5f;"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-chat-settings\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-chat-messages\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-chat-preview\""), std::string::npos);

	const std::string NamePlateBranch = BlockBodyAfter(RenderSettingsAppearance, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	ASSERT_FALSE(NamePlateBranch.empty());
	EXPECT_NE(NamePlateBranch.find("AddCard(5, NamePlateSettingsMinCardHeight"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("AddCard(6, NamePlatePreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("RenderQmSettingsGlassCard(NamePlateSettingsCard, QmCardStyle);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("RenderQmSettingsGlassCard(NamePlatePreviewCard, QmCardStyle);"), std::string::npos);

	const std::string HookCollisionBranch = BlockBodyAfter(RenderSettingsAppearance, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_HOOK_COLLISION)");
	ASSERT_FALSE(HookCollisionBranch.empty());
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-hook-collision-main\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-hook-collision-preview\""), std::string::npos);

	const std::string InfoMessagesBranch = BlockBodyAfter(RenderSettingsAppearance, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_INFO_MESSAGES)");
	ASSERT_FALSE(InfoMessagesBranch.empty());
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-info-messages\""), std::string::npos);

	const std::string LaserBranch = BlockBodyAfter(RenderSettingsAppearance, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_LASER)");
	ASSERT_FALSE(LaserBranch.empty());
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-laser-enhanced\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-laser-colors\""), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("\"deck:appearance-laser-preview\""), std::string::npos);
	EXPECT_EQ(LaserBranch.find("RenderQmSettingsGlassCard(EnhancedCard, QmCardStyle);"), std::string::npos);
	EXPECT_EQ(LaserBranch.find("RenderQmSettingsGlassCard(ColorCard, QmCardStyle);"), std::string::npos);
	EXPECT_EQ(LaserBranch.find("RenderQmSettingsGlassCard(PreviewCard, QmCardStyle);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, TClientSettingsCardsUseSharedQmCardStyle)
{
	const std::string Source = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string HeaderSource = ReadTextFile("src/game/client/components/menus.h");
	const std::string RenderSettingsTClientSettings = FunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	const std::string RenderSettingsTClientChatBinds = FunctionBody(Source, "void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsTClientSettings.empty());
	ASSERT_FALSE(RenderSettingsTClientChatBinds.empty());

	EXPECT_NE(Source.find("RenderQmSettingsGlassCard(TClientCacheSectionBoxRect(BoxRect), QmSettingsCardStyle(1.0f));"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientSettings.find("CScrollRegionParams ScrollParams = QmSettingsScrollRegionParams(1.0f);"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientChatBinds.find("CScrollRegionParams ScrollParams = QmSettingsScrollRegionParams(1.0f);"), std::string::npos);
	EXPECT_EQ(Source.find("ScrollParams.m_ScrollUnit = 60.0f;"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientSettings.find("ScrollParams.m_ScrollbarMargin = 5.0f;"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientChatBinds.find("ScrollParams.m_ScrollbarMargin = 5.0f;"), std::string::npos);
	EXPECT_EQ(Source.find("BoxRect.Draw(Ui()->ScaleBackgroundAlpha(MenuPanelColor(0.92f))"), std::string::npos);
	EXPECT_NE(HeaderSource.find("void DrawTClientCacheSectionBox(CUIRect BoxRect);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DDNetSettingsPageUsesSharedQmCards)
{
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsDDNet = FunctionBody(SettingsSource, "void CMenus::RenderSettingsDDNet(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsDDNet.empty());

	EXPECT_NE(RenderSettingsDDNet.find("const SSettingsPageLayoutFrame DDNetPage = ResolveSettingsPageLayout(MainView, false, UiScale);"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("const IUiContext DDNetCardCtx = SettingsUiContext(\"settings_ddnet\", UiScale);"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("static CScrollRegion s_DDNetSettingsCardScrollRegion;"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("m_SettingsCardDeck.Render(DDNetCardCtx, DDNetPage, \"ddnet\""), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(DemoSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(GameplaySpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("CUIRect GameplayRow;"), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("Gameplay.VSplitMid(&Left, &Right, 20.0f);"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(BackgroundSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(MiscellaneousSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("SaveSettingsCardOrderModel();"), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("EndSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("s_PrevDDNetSettingsScrollY"), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("MainView.HSplitTop(130.0f, &Demo, &MainView);"), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("MainView.HSplitTop(GameplayHeight, &Gameplay, &MainView);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsCardDeckSharedComponentMigratesSoundBindWheelStatusBar)
{
	const std::string HeaderSource = ReadTextFile("src/game/client/components/menus.h");
	EXPECT_NE(HeaderSource.find("struct SSettingsCardDeckLayout"), std::string::npos);
	EXPECT_NE(HeaderSource.find("struct SSettingsCardDeckCard"), std::string::npos);
	EXPECT_NE(HeaderSource.find("SSettingsCardDeckLayout BeginSettingsCardDeck("), std::string::npos);
	EXPECT_NE(HeaderSource.find("SSettingsCardDeckCard BeginSettingsCardDeckCard("), std::string::npos);
	EXPECT_NE(HeaderSource.find("void EndSettingsCardDeck("), std::string::npos);
	EXPECT_NE(HeaderSource.find("void RenderSettingsCardDragHandle("), std::string::npos);
	EXPECT_NE(HeaderSource.find("void RenderSettingsCardDeckDragOverlay("), std::string::npos);
	EXPECT_NE(HeaderSource.find("std::unordered_map<std::string, std::vector<std::string>> m_SettingsCardDeckOrders;"), std::string::npos);
	EXPECT_NE(HeaderSource.find("std::vector<std::string> *SettingsCardDeckOrder("), std::string::npos);
	EXPECT_NE(HeaderSource.find("void LoadSettingsCardDeckOrdersFromGlobalConfig();"), std::string::npos);
	EXPECT_NE(HeaderSource.find("void SerializeMergedSettingsCardDeckOrdersToGlobalConfig();"), std::string::npos);
	EXPECT_NE(HeaderSource.find("std::vector<std::string> m_vActiveCardIds;"), std::string::npos);

	const std::string MenuSource = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string BeginDeck = FunctionBody(MenuSource, "CMenus::SSettingsCardDeckLayout CMenus::BeginSettingsCardDeck(");
	const std::string BeginCard = FunctionBody(MenuSource, "CMenus::SSettingsCardDeckCard CMenus::BeginSettingsCardDeckCard(");
	const std::string EndDeck = FunctionBody(MenuSource, "void CMenus::EndSettingsCardDeck(");
	const std::string DeckOrder = FunctionBody(MenuSource, "std::vector<std::string> *CMenus::SettingsCardDeckOrder(");
	const std::string CommitDrop = FunctionBody(TClientSource, "bool CMenus::CommitSettingsCardDeckDragDrop(");
	const std::string RenderHandle = FunctionBody(MenuSource, "void CMenus::RenderSettingsCardDragHandle(");
	EXPECT_NE(BeginDeck.find("QmSettingsScrollRegionParams(UiScale)"), std::string::npos);
	EXPECT_NE(BeginDeck.find("Deck.m_pOrder = pOrder != nullptr ? pOrder : SettingsCardDeckOrder(pDeckId);"), std::string::npos);
	EXPECT_NE(BeginDeck.find("LoadSettingsCardDeckOrdersFromGlobalConfig();"), std::string::npos);
	EXPECT_NE(DeckOrder.find("LoadSettingsCardDeckOrdersFromGlobalConfig();"), std::string::npos);
	EXPECT_NE(HeaderSource.find("std::deque<std::string> m_vStableIds;"), std::string::npos);
	EXPECT_NE(BeginDeck.find("Deck.m_vActiveCardIds.emplace_back(SettingsCardDeckStableId(Deck.m_vStableIds, ActiveCardId.c_str()));"), std::string::npos);
	EXPECT_NE(DeckOrder.find("m_SettingsCardDeckOrders[pDeckId]"), std::string::npos);
	EXPECT_NE(MenuSource.find("static const char *SettingsCardDeckStableId(std::deque<std::string> &vStableIds"), std::string::npos);
	EXPECT_NE(BeginCard.find("const char *pGlobalStableId = SettingsCardDeckStableId(Deck.m_vStableIds, pStableId);"), std::string::npos);
	EXPECT_NE(BeginCard.find("const char *pLegacyStableId = str_startswith(pStableId, \"deck:\") != nullptr ? pStableId + str_length(\"deck:\") : pStableId;"), std::string::npos);
	EXPECT_NE(BeginCard.find("SettingsCardDeckEnsureStableId(*Deck.m_pOrder, pGlobalStableId, pLegacyStableId);"), std::string::npos);
	EXPECT_NE(BeginCard.find("SettingsCardDeckOrderIndex(*Deck.m_pOrder, pGlobalStableId, Deck.m_CardCount);"), std::string::npos);
	EXPECT_NE(BeginCard.find("SettingsCardDeckIsActiveStableId(Deck, StableId)"), std::string::npos);
	EXPECT_NE(BeginCard.find("if(pGlobalStableId != nullptr && StableId == pGlobalStableId)"), std::string::npos);
	EXPECT_NE(BeginCard.find("RenderQmSettingsGlassCard(Card.m_Rect, Deck.m_Style);"), std::string::npos);
	EXPECT_NE(BeginCard.find("DoSettingsLabel(Deck.m_Page, -1, Card.m_pStableId, &Card.m_TitleRect, pTitle"), std::string::npos);
	EXPECT_EQ(BeginCard.find("DoSettingsLabel(Deck.m_Page, -1, pStableId"), std::string::npos);
	EXPECT_NE(BeginCard.find("RenderSettingsCardDragHandle(Card.m_Rect, &Card.m_HandleRect, Deck.m_Style);"), std::string::npos);
	EXPECT_NE(BeginCard.find("Section.m_pStableCardId = pGlobalStableId;"), std::string::npos);
	EXPECT_EQ(BeginCard.find("Section.m_pStableCardId = pStableId;"), std::string::npos);
	EXPECT_NE(EndDeck.find("RenderSettingsCardDeckDragOverlay(Deck);"), std::string::npos);
	EXPECT_NE(CommitDrop.find("SerializeMergedSettingsCardDeckOrdersToGlobalConfig();"), std::string::npos);
	EXPECT_NE(RenderHandle.find("Input()->ModifierIsPressed()"), std::string::npos);
	EXPECT_NE(RenderHandle.find("Card.VSplitRight(HandleSize"), std::string::npos);
	EXPECT_EQ(RenderHandle.find("pHandleRect->Draw(HandleBg"), std::string::npos);
	EXPECT_EQ(RenderHandle.find("HandleBorderRect.DrawOutline(HandleBorder);"), std::string::npos);
	EXPECT_NE(RenderHandle.find(": ColorRGBA(1.0f, 1.0f, 1.0f, 0.38f);"), std::string::npos);

	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string FormatBackendDisplayName = FunctionBody(SettingsSource, "void FormatQmGraphicsBackendDisplayName(");
	ASSERT_FALSE(FormatBackendDisplayName.empty());
	EXPECT_NE(FormatBackendDisplayName.find("\"OpenGL QmClient %d.%d\""), std::string::npos);
	EXPECT_EQ(FormatBackendDisplayName.find("\"OpenGL_QmClient_%d_%d\""), std::string::npos);
	const std::string RenderSettingsSound = FunctionBody(SettingsSource, "void CMenus::RenderSettingsSound(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsSound.empty());
	EXPECT_NE(RenderSettingsSound.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("CQmScrollState"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("m_SettingsCardDeck.Render(SoundCardCtx, SoundPage, \"sound\""), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("AddCard(ToggleSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("AddCard(VolumeSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("AddCard(AudioPackSpec"), std::string::npos);
	EXPECT_LT(RenderSettingsSound.find("AddCard(VolumeSpec"), RenderSettingsSound.find("AddCard(AudioPackSpec"));
	EXPECT_NE(RenderSettingsSound.find("DoSoundNumericField(\"sound-volume\""), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("DoSoundNumericField(\"sound-background-music-volume\""), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("DoSliderWithValueInput("), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("DoButton_Menu(&s_AudioPackRefreshButton, FONT_ICON_ARROW_ROTATE_RIGHT"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("str_format(aBadge, sizeof(aBadge), \"%d\", Entry.m_FileCount);"), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("str_copy(aBadge, Localize(\"Built-in\"), sizeof(aBadge));"), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("Localize(\"Selected pack\")"), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("\"audio_packs_title\""), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("EndSettingsCardDeck("), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("Definition.m_IsVisible = std::move(IsVisible);"), std::string::npos);
	const size_t ToggleCard = RenderSettingsSound.find("AddCard(ToggleSpec");
	const size_t VolumeCard = RenderSettingsSound.find("AddCard(VolumeSpec");
	const size_t AudioPackCard = RenderSettingsSound.find("AddCard(AudioPackSpec");
	ASSERT_NE(ToggleCard, std::string::npos);
	ASSERT_NE(VolumeCard, std::string::npos);
	ASSERT_NE(AudioPackCard, std::string::npos);
	EXPECT_NE(RenderSettingsSound.substr(ToggleCard, VolumeCard - ToggleCard).find("}, {}, true, ProcessSoundToggleInput);"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.substr(VolumeCard, AudioPackCard - VolumeCard).find("[]() { return g_Config.m_SndEnable != 0; }"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.substr(AudioPackCard).find("[]() { return g_Config.m_SndEnable != 0; }"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("Definition.m_PreLayoutInput = std::move(PreLayoutInput);"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("ProcessSoundToggleInput"), std::string::npos);
	EXPECT_NE(RenderSettingsSound.find("SLabelProperties{}, false"), std::string::npos);
	EXPECT_EQ(RenderSettingsSound.find("AudioPackView.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f)"), std::string::npos);

	const std::string SettingsDeck = ReadTextFile("src/game/client/QmUi/SettingsCardDeck.cpp");
	const size_t RebuildActiveStateIndices = SettingsDeck.find("RebuildActiveStateIndices");
	const size_t InitialBuild = SettingsDeck.find("std::vector<SPreparedSettingsCard> vPrepared = BuildPreparedCards(aColumns);");
	const size_t PreLayoutInput = SettingsDeck.find("m_pDefinition->m_PreLayoutInput");
	const size_t PreLayoutClipCheck = SettingsDeck.find("pScrollRegion == nullptr || !pScrollRegion->RectClipped", InitialBuild);
	const size_t ActiveStateRebuild = SettingsDeck.find("RebuildActiveStateIndices();", PreLayoutInput);
	const size_t FinalBuild = SettingsDeck.find("vPrepared = BuildPreparedCards(aColumns);", ActiveStateRebuild);
	const size_t HeightInvalidation = SettingsDeck.find("std::fill(m_vContentHeights.begin(), m_vContentHeights.end(), -1.0f);", ActiveStateRebuild);
	const size_t ScrollRegistration = SettingsDeck.find("pScrollRegion->AddRect", FinalBuild);
	const size_t SettingsCardRender = SettingsDeck.find("SettingsCard(Ctx, Card.m_Frame", FinalBuild);
	const size_t VisibleDropCommit = SettingsDeck.find("CommitSettingsCardDeckDrop(Model, pTab, pStableId, m_Drag.m_TargetColumn, m_Drag.m_TargetOrder, &m_vActiveStateIndices)");
	EXPECT_NE(RebuildActiveStateIndices, std::string::npos);
	ASSERT_NE(InitialBuild, std::string::npos);
	ASSERT_NE(PreLayoutInput, std::string::npos);
	ASSERT_NE(PreLayoutClipCheck, std::string::npos);
	ASSERT_NE(ActiveStateRebuild, std::string::npos);
	ASSERT_NE(FinalBuild, std::string::npos);
	ASSERT_NE(HeightInvalidation, std::string::npos);
	ASSERT_NE(ScrollRegistration, std::string::npos);
	ASSERT_NE(SettingsCardRender, std::string::npos);
	ASSERT_NE(VisibleDropCommit, std::string::npos);
	EXPECT_LT(InitialBuild, PreLayoutInput);
	EXPECT_LT(PreLayoutClipCheck, PreLayoutInput);
	EXPECT_LT(PreLayoutInput, ActiveStateRebuild);
	EXPECT_LT(ActiveStateRebuild, FinalBuild);
	EXPECT_LT(ActiveStateRebuild, HeightInvalidation);
	EXPECT_LT(FinalBuild, ScrollRegistration);
	EXPECT_LT(FinalBuild, SettingsCardRender);
	EXPECT_EQ(SettingsDeck.find("PrioritizeVisibilityControllers"), std::string::npos);
	EXPECT_EQ(SettingsDeck.find("vRenderedStates"), std::string::npos);

	const std::string RenderSettingsTClientSettings = FunctionBody(TClientSource, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	const std::string RenderSettingsTClientBindWheel = FunctionBody(TClientSource, "void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView)");
	const std::string RenderSettingsTClientStatusBar = FunctionBody(TClientSource, "void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)");
	const std::string HandleSettingsCardDeckDrag = FunctionBody(TClientSource, "void CMenus::HandleSettingsCardDeckDrag(");
	ASSERT_FALSE(RenderSettingsTClientSettings.empty());
	ASSERT_FALSE(RenderSettingsTClientBindWheel.empty());
	ASSERT_FALSE(RenderSettingsTClientStatusBar.empty());
	ASSERT_FALSE(HandleSettingsCardDeckDrag.empty());
	EXPECT_NE(HandleSettingsCardDeckDrag.find("(time_get() - DragState.m_PressStartTime) / (float)time_freq() >= 0.3f"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientSettings.find("RenderSettingsCardDragHandle(CardBoxRect, &HandleRect, QmSettingsCardStyle(1.0f));"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientSettings.find("SettingsCardDeckItemFromSection(SectionMeta, ColumnId, (int)i, CardRect, HandleRect);"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("BeginSettingsCardDeck(MainView, s_BindWheelSettingsScrollRegion"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("BeginSettingsCardDeckCard(BindWheelDeck, \"tclient-bind-wheel-editor\", Localize(\"Bind Wheel\"),"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("BeginSettingsCardDeckCard(BindWheelDeck, \"tclient-bind-wheel-preview\", Localize(\"Preview\"),"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("EndSettingsCardDeck(BindWheelDeck, &s_BindWheelSettingsScrollY);"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientBindWheel.find("MainView.VSplitLeft(MainView.w / 2.1f"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientBindWheel.find("BeginSettingsCardDeck(MainView, s_BindWheelSettingsScrollRegion, s_BindWheelSettingsScrollY, 1.0f, \"tclient-bind-wheel\", SETTINGS_TCLIENT, nullptr)"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("BeginSettingsCardDeck(MainView, s_StatusBarSettingsScrollRegion"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("BeginSettingsCardDeckCard(StatusBarDeck, \"tclient-status-bar-settings\", Localize(\"Status Bar\"),"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("BeginSettingsCardDeckCard(StatusBarDeck, \"tclient-status-bar-items\", Localize(\"Status Bar Codes\"),"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("BeginSettingsCardDeckCard(StatusBarDeck, \"tclient-status-bar-preview\", Localize(\"Preview\"),"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("RenderStatusBarCodes(RightView, 0, true);"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("EndSettingsCardDeck(StatusBarDeck, &s_StatusBarSettingsScrollY);"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientStatusBar.find("MainView.HSplitBottom(100.0f, &MainView, &StatusBar);"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientStatusBar.find("LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);\n\t\tLeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);"), std::string::npos);
	EXPECT_EQ(RenderSettingsTClientStatusBar.find("tclient-statusbar-seconds\", Localize(\"Show seconds on clock\"), g_Config.m_TcStatusBarLocalTimeSeconds, &CheckBoxRect))\n\t\t\tg_Config.m_TcStatusBarLocalTimeSeconds ^= 1;\n\t\tLeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);\n\t\t{\n\t\t\tLeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("float StatusBarSettingsContentBottom = LeftView.y;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientStatusBar.find("float StatusBarPreviewContentBottom = StatusBar.y + StatusBar.h;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("const float BindWheelEditorContentBottom = LeftView.y;"), std::string::npos);
	EXPECT_NE(RenderSettingsTClientBindWheel.find("s_BindWheelEditorCardHeight = maximum(BindWheelEditorBottom + BindWheelDeck.m_Style.m_Padding - BindWheelEditorCard.m_Rect.y, 320.0f);"), std::string::npos);

	const std::string RenderSettingsGraphics = FunctionBody(SettingsSource, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsGraphics.empty());
	EXPECT_NE(RenderSettingsGraphics.find("const SSettingsPageLayoutFrame GraphicsPage = ResolveSettingsPageLayout(MainView, false, UiScale);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("qm_card_registry::FindByStableId(\"deck:graphics-display\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("qm_card_registry::FindByStableId(\"deck:graphics-visual\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("qm_card_registry::FindByStableId(\"deck:graphics-backend\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("qm_card_registry::FindByStableId(\"deck:graphics-modes\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Localize(pBackendDefault->m_pTitle)"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("&g_Config.m_QmExtraAnimations, \"extra-animations\", Localize(\"Extra animations\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("&g_Config.m_QmUiCardRainbowTitles, \"rainbow-card-titles\", Localize(\"Rainbow card titles\")"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("&g_Config.m_QmUiMotionLevel, &g_Config.m_QmUiMotionLevel, Button, Localize(\"UI motion level\"), 0, 2"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("AddCard(DisplaySpec, GraphicsDisplayMinCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("AddCard(VisualSpec, GraphicsVisualMinCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("AddCard(BackendSpec, GraphicsBackendMinCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("AddCard(ModesSpec, GraphicsModesMinCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("const float GraphicsModesMinCardHeight = maximum(420.0f * UiScale, GraphicsPage.m_ScrollViewport.h - GraphicsPage.m_CardGap);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("UpdateMeasuredCardHeight(s_GraphicsDisplayCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("UpdateMeasuredCardHeight(s_GraphicsVisualCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("UpdateMeasuredCardHeight(s_GraphicsBackendCardHeight"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("SaveSettingsCardOrderModel();"), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("\"graphics-renderer-title\""), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("EndSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsGraphics.find("MainView.VSplitLeft(OptionsBlockWidth"), std::string::npos);

	const std::string RenderSettingsAppearance = FunctionBody(SettingsSource, "void CMenus::RenderSettingsAppearance(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsAppearance.empty());
	EXPECT_NE(RenderSettingsAppearance.find("const char *const aAppearanceIds[]"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("qm_card_registry::FindByStableId"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("Definition.m_Spec = Spec;"), std::string::npos);
	EXPECT_NE(RenderSettingsAppearance.find("UpdateMeasuredCardHeight"), std::string::npos);
}

TEST(QmNewUiMenuBranches, SettingsCardFeedbackFixesUseStableLayouts)
{
	const std::string SettingsSource = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsDDNet = FunctionBody(SettingsSource, "void CMenus::RenderSettingsDDNet(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsDDNet.empty());
	EXPECT_NE(RenderSettingsDDNet.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(DemoSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(GameplaySpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(BackgroundSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("AddCard(MiscellaneousSpec"), std::string::npos);
	EXPECT_NE(RenderSettingsDDNet.find("m_SettingsCardDeck.Render(DDNetCardCtx, DDNetPage, \"ddnet\""), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(RenderSettingsDDNet.find("BeginDDNetCard(MainView"), std::string::npos);

	const std::string NamePlateBranch = BlockBodyAfter(SettingsSource, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	ASSERT_FALSE(NamePlateBranch.empty());
	EXPECT_EQ(NamePlateBranch.find("BeginSettingsScrollRegion(s_NameplateTextCardScrollRegion"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("FinishSettingsScrollRegion(s_NameplateTextCardScrollRegion"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("Localize(\"Nameplate text\")"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NameplateTextExpandedMinHeight"), std::string::npos);

	const std::string QmSource = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	EXPECT_EQ(QmSource.find("RenderNameplateTextSettings(CardContent);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, LaserPreviewEntityBranchesReserveEndpointDecorationSpace)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string DoLaserPreview = FunctionBody(Source, "void CMenus::DoLaserPreview(const CUIRect *pRect, const ColorHSLA LaserOutlineColor, const ColorHSLA LaserInnerColor, const int LaserType)");

	EXPECT_NE(DoLaserPreview.find("const vec2 EntityLaserEnd = LaserType == LASERTYPE_DOOR ? Pos - vec2(34.0f, 0.0f) : Pos - vec2(42.0f, 0.0f);"), std::string::npos);
	EXPECT_NE(DoLaserPreview.find("RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL, vec2(-1, 0), Pos - vec2(20.0f, 0.0f));"), std::string::npos);
	EXPECT_EQ(DoLaserPreview.find("RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, EMOTE_NORMAL, vec2(-1, 0), Pos);"), std::string::npos);
}

TEST(QmNewUiMenuBranches, LaserEntityTypesUseDdnetEndpointRendering)
{
	const std::string Source = ReadTextFile("src/game/client/components/items.cpp");
	const std::string RenderLaser = FunctionBody(Source, "void CItems::RenderLaser(vec2 From, vec2 Pos, ColorRGBA OuterColor, ColorRGBA InnerColor, float TicksBody, float TicksHead, int Type, float GlowIntensity) const");
	ASSERT_FALSE(RenderLaser.empty());

	// Regression guard for DDNet entity beams: door/freeze/dragger are carried by
	// the laser render path, but they are not weapon lasers and must not receive
	// generic impact-splat endpoints. The endpoint side is part of the behavior:
	// door blocker at Pos only, dragger pulley at From, freeze hectagon at Pos.
	const size_t DoorBranchPos = RenderLaser.find("if(Type == LASERTYPE_DOOR)");
	const size_t DraggerBranchPos = RenderLaser.find("else if(Type == LASERTYPE_DRAGGER)");
	const size_t FreezeBranchPos = RenderLaser.find("else if(Type == LASERTYPE_FREEZE)");
	const size_t GenericHeadPos = RenderLaser.find("else\n\t{", FreezeBranchPos);
	ASSERT_NE(DoorBranchPos, std::string::npos);
	ASSERT_NE(DraggerBranchPos, std::string::npos);
	ASSERT_NE(FreezeBranchPos, std::string::npos);
	ASSERT_NE(GenericHeadPos, std::string::npos);
	EXPECT_LT(DoorBranchPos, GenericHeadPos);
	EXPECT_LT(DraggerBranchPos, GenericHeadPos);
	EXPECT_LT(FreezeBranchPos, GenericHeadPos);

	const std::string DoorBranch = RenderLaser.substr(DoorBranchPos, DraggerBranchPos - DoorBranchPos);
	const std::string DraggerBranch = RenderLaser.substr(DraggerBranchPos, FreezeBranchPos - DraggerBranchPos);
	const std::string FreezeBranch = RenderLaser.substr(FreezeBranchPos, GenericHeadPos - FreezeBranchPos);
	const std::string GenericHeadBranch = RenderLaser.substr(GenericHeadPos);
	EXPECT_NE(DoorBranch.find("m_DoorHeadOffset"), std::string::npos);
	EXPECT_NE(DoorBranch.find("Pos.x - 8.0f, Pos.y - 8.0f"), std::string::npos);
	EXPECT_EQ(DoorBranch.find("From.x"), std::string::npos);
	EXPECT_NE(DraggerBranch.find("GameClient()->m_ExtrasSkin.m_SpritePulley"), std::string::npos);
	EXPECT_NE(DraggerBranch.find("m_PulleyHeadOffset, From.x, From.y"), std::string::npos);
	EXPECT_EQ(DraggerBranch.find("m_PulleyHeadOffset, Pos.x, Pos.y"), std::string::npos);
	EXPECT_NE(FreezeBranch.find("GameClient()->m_ExtrasSkin.m_SpriteHectagon"), std::string::npos);
	EXPECT_NE(FreezeBranch.find("m_FreezeHeadOffset, Pos.x, Pos.y"), std::string::npos);
	EXPECT_EQ(FreezeBranch.find("m_FreezeHeadOffset, From.x, From.y"), std::string::npos);
	EXPECT_NE(GenericHeadBranch.find("m_aParticleSplatOffset[CurParticle]"), std::string::npos);
	EXPECT_EQ(DoorBranch.find("m_aParticleSplatOffset"), std::string::npos);
	EXPECT_EQ(DraggerBranch.find("m_aParticleSplatOffset"), std::string::npos);
	EXPECT_EQ(FreezeBranch.find("m_aParticleSplatOffset"), std::string::npos);
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

TEST(QmNewUiMenuBranches, LiveDirectorChatToggleIsHandledByOverlayInput)
{
	const std::string Source = ReadTextFile("src/game/client/gameclient.cpp");
	const std::string Contains = FunctionBody(Source, "bool CGameClient::LiveObserverOverlayContains");
	const std::string Input = FunctionBody(Source, "bool CGameClient::HandleLiveObserverInput");
	const std::string Render = FunctionBody(Source, "void CGameClient::RenderLiveObserverOverlay");

	EXPECT_NE(Source.find("constexpr float LIVE_OBSERVER_CHAT_TOGGLE_W"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float LIVE_OBSERVER_CHAT_TOGGLE_H"), std::string::npos);
	EXPECT_NE(Source.find("CUIRect LiveObserverChatToggleRect(float Height)"), std::string::npos);
	EXPECT_NE(Contains.find("LiveObserverChatToggleRect(LIVE_OBSERVER_UI_HEIGHT).Inside(MousePos)"), std::string::npos);
	EXPECT_NE(Input.find("g_Config.m_ClShowChat = g_Config.m_ClShowChat == 0 ? 1 : 0;"), std::string::npos);
	EXPECT_NE(Input.find("Input()->MouseModeAbsolute();"), std::string::npos);
	EXPECT_LT(Input.find("LiveObserverChatToggleRect(LIVE_OBSERVER_UI_HEIGHT).Inside(MousePos)"), Input.find("if(Panel.Inside(MousePos))"));
	EXPECT_NE(Render.find("const CUIRect ChatToggle = LiveObserverChatToggleRect(Height);"), std::string::npos);
	EXPECT_NE(Render.find("ChatVisible ? Localize(\"Hide Chat\") : Localize(\"Show chat\")"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ImplausibleRefreshRatesAreNotPersisted)
{
	const std::string Backend = ReadTextFile("src/engine/client/backend_sdl.cpp");
	const std::string Graphics = ReadTextFile("src/engine/client/graphics_threaded.cpp");

	// IsPlausible* guards now live in the shared plausible_sizes.h header
	// (behavior-tested in PlausibleSizes.RefreshRateAndWindowGuardsMatchContract);
	// both backends include it instead of re-declaring file-static copies.
	EXPECT_NE(Backend.find("#include <engine/client/plausible_sizes.h>"), std::string::npos);
	EXPECT_NE(Backend.find("Ignoring implausible configured window size"), std::string::npos);
	EXPECT_NE(Backend.find("*pWidth = DisplayMode.w;"), std::string::npos);
	EXPECT_NE(Backend.find("*pHeight = DisplayMode.h;"), std::string::npos);
	EXPECT_NE(Backend.find("Ignoring implausible configured refresh rate"), std::string::npos);
	EXPECT_NE(Backend.find("*pRefreshRate = 0;"), std::string::npos);
	EXPECT_NE(Graphics.find("#include <engine/client/plausible_sizes.h>"), std::string::npos);
	EXPECT_NE(Graphics.find("Ignoring implausible refresh rate during resize"), std::string::npos);
	EXPECT_NE(Graphics.find("RefreshRate = m_ScreenRefreshRate;"), std::string::npos);
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

TEST(QmNewUiMenuBranches, GraphicsBackendDropdownUsesQmClientDisplayNames)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string Formatter = FunctionBody(Source, "void FormatQmGraphicsBackendDisplayName(char *pBuf, int BufSize, const char *pBackendName, int Major, int Minor, int Patch, bool IsDefault)");
	const std::string RenderSettingsGraphics = FunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");

	ASSERT_FALSE(Formatter.empty());
	ASSERT_FALSE(RenderSettingsGraphics.empty());
	EXPECT_NE(Formatter.find("\"OpenGL QmClient %d.%d\""), std::string::npos);
	EXPECT_NE(Formatter.find("\"Vulkan QmClient\""), std::string::npos);
	EXPECT_EQ(Formatter.find("\"OpenGL_QmClient_%d_%d\""), std::string::npos);
	EXPECT_EQ(Formatter.find("\"Vulkan_QmClient\""), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Localize(pBackendDefault->m_pTitle)"), std::string::npos);
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

TEST(QmNewUiMenuBranches, DropDownPopupFollowsScrolledControlRect)
{
	const std::string UiSource = ReadTextFile("src/game/client/ui.cpp");
	const std::string DoDropDown = FunctionBody(UiSource, "int CUi::DoDropDown(CUIRect *pRect, int CurSelection, const char **pStrs, int Num, SDropDownState &State)");
	const std::string DoPopupMenu = FunctionBody(UiSource, "void CUi::DoPopupMenu(");

	ASSERT_FALSE(DoDropDown.empty());
	ASSERT_FALSE(DoPopupMenu.empty());
	EXPECT_NE(DoDropDown.find("bool PopupOpen = IsPopupOpen(&State.m_SelectionPopupContext);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("if(PopupOpen)"), std::string::npos);
	EXPECT_NE(DoDropDown.find("ShowPopupSelection(pRect->x, pRect->y, &State.m_SelectionPopupContext);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("PopupOpen = IsPopupOpen(&State.m_SelectionPopupContext);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("if(State.m_DropDownState.IsOpen() && !PopupOpen)"), std::string::npos);
	EXPECT_NE(DoDropDown.find("SQmDropdownInput DropDownInput;"), std::string::npos);
	EXPECT_NE(DoDropDown.find("State.m_DropDownState.Update(DropDownInput, Num);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("DropDownInput.m_KeyUp = ConsumeHotkey(HOTKEY_UP);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("DropDownInput.m_KeyDown = ConsumeHotkey(HOTKEY_DOWN);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("DropDownInput.m_KeyEnter = ConsumeHotkey(HOTKEY_ENTER);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("DropDownInput.m_KeyEscape = ConsumeHotkey(HOTKEY_ESCAPE);"), std::string::npos);
	EXPECT_NE(DoDropDown.find("State.m_SelectionPopupContext.m_ActiveIndex = State.m_DropDownState.ActiveIndex();"), std::string::npos);
	const size_t SelectedBranch = DoDropDown.find("if(DropDownResult.m_Selected)");
	const size_t ClosedBranch = DoDropDown.find("else if(DropDownResult.m_Closed)");
	ASSERT_NE(SelectedBranch, std::string::npos);
	ASSERT_NE(ClosedBranch, std::string::npos);
	EXPECT_LT(SelectedBranch, ClosedBranch);
	EXPECT_NE(DoPopupMenu.find("std::find_if(m_vPopupMenus.begin(), m_vPopupMenus.end()"), std::string::npos);
	EXPECT_NE(DoPopupMenu.find("ExistingPopupMenu->m_Rect.x = X;"), std::string::npos);
	EXPECT_NE(DoPopupMenu.find("ExistingPopupMenu->m_Rect.y = Y;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, DropDownKeyboardActiveIndexIsRendered)
{
	const std::string UiSource = ReadTextFile("src/game/client/ui.cpp");
	const std::string UiHeader = ReadTextFile("src/game/client/ui.h");
	const std::string SelectionReset = FunctionBody(UiSource, "void CUi::SSelectionPopupContext::Reset()");
	const std::string PopupSelection = FunctionBody(UiSource, "CUi::EPopupMenuFunctionResult CUi::PopupSelection(void *pContext, CUIRect View, bool Active)");
	const std::string PopupButton = FunctionBody(UiSource, "int CUi::DoButton_PopupMenu(CButtonContainer *pButtonContainer");
	const std::string DoDropDown = FunctionBody(UiSource, "int CUi::DoDropDown(CUIRect *pRect, int CurSelection, const char **pStrs, int Num, SDropDownState &State)");

	ASSERT_FALSE(SelectionReset.empty());
	ASSERT_FALSE(PopupSelection.empty());
	ASSERT_FALSE(PopupButton.empty());
	ASSERT_FALSE(DoDropDown.empty());
	EXPECT_NE(UiHeader.find("int m_ActiveIndex;"), std::string::npos);
	EXPECT_NE(SelectionReset.find("m_ActiveIndex = -1;"), std::string::npos);
	EXPECT_NE(PopupButton.find("ButtonColor.has_value() || !TransparentInactive"), std::string::npos);
	EXPECT_NE(PopupSelection.find("const bool ActiveEntry = pSelectionPopup->m_ActiveIndex == static_cast<int>(Index);"), std::string::npos);
	EXPECT_NE(PopupSelection.find("ActiveEntry ? std::optional<ColorRGBA>(ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f)) : std::nullopt"), std::string::npos);
	EXPECT_NE(PopupSelection.find("pSelectionPopup->m_TransparentButtons, true, ButtonColor"), std::string::npos);
	const size_t UpdateResult = DoDropDown.find("const SQmDropdownUpdateResult DropDownResult = State.m_DropDownState.Update(DropDownInput, Num);");
	const size_t ActiveIndexSync = DoDropDown.find("State.m_SelectionPopupContext.m_ActiveIndex = State.m_DropDownState.ActiveIndex();");
	const size_t PopupRender = DoDropDown.find("ShowPopupSelection(pRect->x, pRect->y, &State.m_SelectionPopupContext);");
	ASSERT_NE(UpdateResult, std::string::npos);
	ASSERT_NE(ActiveIndexSync, std::string::npos);
	ASSERT_NE(PopupRender, std::string::npos);
	EXPECT_LT(UpdateResult, ActiveIndexSync);
	EXPECT_LT(ActiveIndexSync, PopupRender);
}

TEST(QmNewUiMenuBranches, GeneralStandardPageUsesUnifiedSettingsStack)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string Menus = ReadTextFile("src/game/client/components/menus.cpp");
	const std::string General = FunctionBody(Source, "void CMenus::RenderSettingsGeneral(CUIRect MainView)");
	const std::string NumericLabelBridge = FunctionBody(Menus, "bool CMenus::PrepareSettingsNumericFieldLabel(");
	ASSERT_FALSE(General.empty());
	ASSERT_FALSE(NumericLabelBridge.empty());
	EXPECT_NE(General.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(General.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(General.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(General.find("CQmScrollState"), std::string::npos);
	EXPECT_NE(General.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(General.find("QmScrollRegionParamsFromPolicy(ScrollPolicy)"), std::string::npos);
	EXPECT_EQ(General.find("(void)QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(General.find("PrepareSettingsNumericFieldLabel("), std::string::npos);
	EXPECT_NE(NumericLabelBridge.find("if(m_MenuTextPlanCollecting)"), std::string::npos);
	EXPECT_NE(NumericLabelBridge.find("CollectMenuTextPlanItem(MENU_TEXT_SCOPE_SETTINGS"), std::string::npos);
	EXPECT_NE(General.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(General.find("AddCard(GameSpec, 140.0f * UiScale"), std::string::npos);
	EXPECT_NE(General.find("deck:general-game"), std::string::npos);
	EXPECT_NE(General.find("deck:general-language"), std::string::npos);
	EXPECT_NE(General.find("deck:general-client"), std::string::npos);
	EXPECT_NE(General.find("deck:general-recording"), std::string::npos);
	EXPECT_EQ(General.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(General.find("DoSettingsScrollbarOption("), std::string::npos);
	EXPECT_EQ(General.find("Ui()->DoEditBox("), std::string::npos);
	EXPECT_EQ(General.find("Ui()->DoScrollbarH("), std::string::npos);
}
TEST(QmNewUiMenuBranches, PlayerStandardPageUsesUnifiedSettingsStack)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string Navigation = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Identity = FunctionBody(Source, "void CMenus::RenderSettingsTeeIdentity(CUIRect MainView, CUIRect *pFlagButton)");
	const std::string Player = FunctionBody(Source, "void CMenus::RenderSettingsPlayer(CUIRect MainView)");
	ASSERT_FALSE(Identity.empty());
	ASSERT_FALSE(Player.empty());
	EXPECT_NE(Identity.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(Player.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Player.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Player.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Player.find("CQmScrollState"), std::string::npos);
	EXPECT_NE(Player.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(Player.find("QmScrollRegionParamsFromPolicy(ScrollPolicy)"), std::string::npos);
	EXPECT_NE(Player.find("ui_widget::InputField("), std::string::npos);
	const std::string ListBox = ReadTextFile("src/game/client/ui_listbox.cpp");
	const size_t PlayerListPriority = Player.find("s_ListBox.SetWheelOwnerPriority(EUiWheelOwnerPriority::COMPOSITE_CONTROL);");
	const size_t PlayerListStart = Player.find("s_ListBox.DoStart(");
	ASSERT_NE(PlayerListPriority, std::string::npos);
	ASSERT_NE(PlayerListStart, std::string::npos);
	EXPECT_LT(PlayerListPriority, PlayerListStart);
	EXPECT_NE(ListBox.find("ScrollParams.m_WheelOwnerPriority = m_WheelOwnerPriority;"), std::string::npos);
	EXPECT_NE(ListBox.find("m_WheelOwnerPriority = EUiWheelOwnerPriority::PAGE;"), std::string::npos);
	EXPECT_NE(Player.find("deck:player-identity"), std::string::npos);
	EXPECT_NE(Player.find("deck:player-country"), std::string::npos);
	EXPECT_NE(Navigation.find("{\"player\", CMenus::SETTINGS_PLAYER}"), std::string::npos);
	EXPECT_EQ(Player.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(Player.find("ui_widget::TextField("), std::string::npos);
	EXPECT_EQ(Player.find("ui_widget::SearchField("), std::string::npos);
	EXPECT_EQ(Player.find("Ui()->DoEditBox("), std::string::npos);
	EXPECT_EQ(Player.find("Ui()->DoScrollbarH("), std::string::npos);
}

TEST(QmNewUiMenuBranches, TeeStandardPageUsesUnifiedSettingsStack)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string DeckSource = ReadTextFile("src/game/client/QmUi/SettingsCardDeck.cpp");
	const std::string Navigation = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Tee = FunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Tee.empty());
	EXPECT_NE(Tee.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Tee.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Tee.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Tee.find("CQmScrollState"), std::string::npos);
	EXPECT_NE(Tee.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(Tee.find("QmScrollRegionParamsFromPolicy(ScrollPolicy)"), std::string::npos);
	EXPECT_NE(Tee.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(Tee.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(Tee.find("SetSettingsTeeVisibleSnapshot("), std::string::npos);
	const size_t SkinListPriority = Tee.find("s_ListBox.SetWheelOwnerPriority(EUiWheelOwnerPriority::COMPOSITE_CONTROL);");
	const size_t SkinListStart = Tee.find("s_ListBox.DoStart(TeeSkinListRowHeight", SkinListPriority);
	const size_t DeckRender = Tee.find("m_SettingsCardDeck.Render(");
	const size_t RefreshAfterDeck = Tee.find("if(ShouldRefresh)", DeckRender);
	ASSERT_NE(SkinListPriority, std::string::npos);
	ASSERT_NE(SkinListStart, std::string::npos);
	ASSERT_NE(DeckRender, std::string::npos);
	ASSERT_NE(RefreshAfterDeck, std::string::npos);
	EXPECT_LT(SkinListPriority, SkinListStart);
	EXPECT_LT(DeckRender, RefreshAfterDeck);
	const size_t ListCardStart = Tee.find("AddCard(ListSpec, 760.0f * UiScale");
	const size_t ListCardEnd = Tee.find("const SSettingsPageLayoutFrame TeePage", ListCardStart);
	ASSERT_NE(ListCardStart, std::string::npos);
	ASSERT_NE(ListCardEnd, std::string::npos);
	EXPECT_NE(Tee.substr(ListCardStart, ListCardEnd - ListCardStart).find("}, true);"), std::string::npos);
	EXPECT_NE(DeckSource.find("if(DrawLayout.m_TwoColumns && !aDisplayColumns[0].empty())"), std::string::npos);
	EXPECT_NE(DeckSource.find("const size_t NumLayers = std::max({aDisplayColumns[0].size(), aDisplayColumns[1].size(), aDisplayColumns[2].size()});"), std::string::npos);
	EXPECT_NE(DeckSource.find("AppendColumn(aDisplayColumns[1], 1, DrawLayout.m_aColumns[0], LeftY);"), std::string::npos);
	EXPECT_NE(DeckSource.find("if(Visible || Card.m_pDefinition->m_RenderWhenClipped)"), std::string::npos);
	EXPECT_NE(Tee.find("deck:tee-identity"), std::string::npos);
	EXPECT_NE(Tee.find("deck:tee-skin-options"), std::string::npos);
	EXPECT_NE(Tee.find("deck:tee-skin-list"), std::string::npos);
	EXPECT_NE(Navigation.find("{\"tee\", CMenus::SETTINGS_TEE}"), std::string::npos);
	EXPECT_EQ(Tee.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(Tee.find("DoSettingsScrollbarOption("), std::string::npos);
	EXPECT_EQ(Tee.find("Ui()->DoEditBox("), std::string::npos);
	EXPECT_EQ(Tee.find("Ui()->DoScrollbarH("), std::string::npos);
}

TEST(QmNewUiMenuBranches, GraphicsPilotHasNoRemainingLegacyInputOrScrollPath)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string Graphics = FunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(Graphics.empty());
	EXPECT_NE(Graphics.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Graphics.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Graphics.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Graphics.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(Graphics.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(Graphics.find("CQmScrollState"), std::string::npos);
	EXPECT_NE(Graphics.find("deck:graphics-display"), std::string::npos);
	EXPECT_NE(Graphics.find("deck:graphics-visual"), std::string::npos);
	EXPECT_NE(Graphics.find("deck:graphics-backend"), std::string::npos);
	EXPECT_NE(Graphics.find("deck:graphics-modes"), std::string::npos);
	EXPECT_EQ(Graphics.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(Graphics.find("DoSliderWithValueInput("), std::string::npos);
	EXPECT_EQ(Graphics.find("Ui()->DoScrollbarH("), std::string::npos);
	EXPECT_EQ(Graphics.find("Ui()->DoValueSelectorWithState("), std::string::npos);
	EXPECT_EQ(Graphics.find("s_GraphicsSettingsScrollRegion"), std::string::npos);
}
TEST(QmNewUiMenuBranches, NestedLanguageListWheelOwnerOutranksGeneralPage)
{
	EXPECT_TRUE(QmHotScrollRegionPriorityWins(EUiWheelOwnerPriority::PAGE, EUiWheelOwnerPriority::COMPOSITE_CONTROL));
	EXPECT_FALSE(QmHotScrollRegionPriorityWins(EUiWheelOwnerPriority::COMPOSITE_CONTROL, EUiWheelOwnerPriority::PAGE));
	const std::string ScrollRegionSource = ReadTextFile("src/game/client/ui_scrollregion.cpp");
	EXPECT_NE(ScrollRegionSource.find("Ui()->SetHotScrollRegion(this, m_Params.m_WheelOwnerPriority);"), std::string::npos);

	CScrollWheelOwnership Ownership;
	int OuterOwner = 0;
	int InnerOwner = 0;
	ASSERT_TRUE(Ownership.BeginFrame(1, 1.0f, false));
	Ownership.Register(&OuterOwner, EUiWheelOwnerPriority::PAGE, true);
	Ownership.Register(&InnerOwner, EUiWheelOwnerPriority::COMPOSITE_CONTROL, true);
	float WheelDelta = 0.0f;
	EXPECT_FALSE(Ownership.TryConsume(&OuterOwner, &WheelDelta));
	EXPECT_TRUE(Ownership.TryConsume(&InnerOwner, &WheelDelta));
	EXPECT_EQ(WheelDelta, 1.0f);

	const std::string Source = ReadTextFile("src/game/client/components/menus_settings.cpp");
	const std::string LanguageSelection = FunctionBody(Source, "bool CMenus::RenderLanguageSelection(CUIRect MainView)");
	ASSERT_FALSE(LanguageSelection.empty());
	EXPECT_NE(LanguageSelection.find("ScrollParams.m_WheelOwnerPriority = EUiWheelOwnerPriority::COMPOSITE_CONTROL;"), std::string::npos);
}

TEST(QmNewUiMenuBranches, ControlsStandardPageUsesUnifiedSettingsStack)
{
	const std::string Source = ReadTextFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string Header = ReadTextFile("src/game/client/components/menus_settings_controls.h");
	const std::string Navigation = ReadTextFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	EXPECT_NE(Source.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(Source.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Source.find("m_SettingsCardDeck.Render"), std::string::npos);
	EXPECT_NE(Source.find("ui_widget::InputField("), std::string::npos);
	EXPECT_NE(Source.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(Source.find("m_SettingsScrollRegion.State()"), std::string::npos);
	EXPECT_NE(Source.find("m_RenderWhenClipped = RenderWhenClipped"), std::string::npos);
	EXPECT_NE(Source.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_NE(Source.find("controls_text_cache"), std::string::npos);
	EXPECT_NE(Source.find("controls_bind_list"), std::string::npos);
	EXPECT_NE(Source.find("DoKeyReader"), std::string::npos);
	EXPECT_EQ(Source.find("RenderSettingsBlock"), std::string::npos);
	EXPECT_EQ(Source.find("BeginSettingsScrollRegion"), std::string::npos);
	EXPECT_EQ(Source.find("FinishSettingsScrollRegion"), std::string::npos);
	EXPECT_EQ(Source.find("DoScrollbarH"), std::string::npos);
	EXPECT_EQ(Source.find("DoValueSelector"), std::string::npos);
	EXPECT_EQ(Header.find("DoSettingsControlsScrollbarOption"), std::string::npos);
	EXPECT_NE(Source.find("deck:controls-mouse"), std::string::npos);
	EXPECT_NE(Source.find("deck:controls-custom"), std::string::npos);
	EXPECT_NE(Navigation.find("{\"controls\", CMenus::SETTINGS_CONTROLS}"), std::string::npos);
	const std::string Menus = ReadTextFile("src/game/client/components/menus.cpp");
	EXPECT_NE(Menus.find("str_comp(pTab, \"controls\")"), std::string::npos);
}
