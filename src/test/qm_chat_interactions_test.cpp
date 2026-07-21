// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <base/system.h>

#include <game/client/components/chat.h>
#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/tclient/fast_practice.h>
#include <game/client/components/tclient/warlist.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <iterator>
#include <string>

namespace
{
	int64_t TestTicks(float Seconds)
	{
		return (int64_t)(Seconds * time_freq());
	}

	std::string SourceFunctionBody(const std::string &Source, const std::string &Signature)
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
}

TEST(QmChatPresentation, NewLineEntersThenBecomesVisible)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(10.0f);

	CChat::BeginLinePresentation(Presentation, Start, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::ENTERING);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_LT(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetY, 0.0f);
	EXPECT_LT(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.31f), 0.10f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetY, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmWarListEnemyChat, OnlyBuiltInEnemyGroupMatches)
{
	CWarDataCache WarData;
	ASSERT_GE(WarData.m_WarGroupMatches.size(), 3u);

	WarData.m_WarGroupMatches[2] = true;
	EXPECT_FALSE(CWarList::MatchesEnemyGroup(WarData));

	WarData.m_WarGroupMatches[2] = false;
	WarData.m_WarGroupMatches[1] = true;
	EXPECT_TRUE(CWarList::MatchesEnemyGroup(WarData));
}

TEST(QmWarListEnemyChat, ShortGroupDataDoesNotMatchEnemy)
{
	CWarDataCache WarData;
	WarData.m_WarGroupMatches.resize(1);
	EXPECT_FALSE(CWarList::MatchesEnemyGroup(WarData));
}

TEST(QmDummySyncChatCommand, MatchesOnlySupportedCommands)
{
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand(nullptr));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/team 2"));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/TEAM 2"));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/TeAm 63"));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/vote particle"));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/VOTE PARTICLE"));
	EXPECT_TRUE(CChat::ShouldSyncDummyCommand("/VoTe PaRtIcLe"));

	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("/team"));
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("/team "));
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("/teamwork 2"));
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("/vote particles"));
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("/vote particle on"));
	EXPECT_FALSE(CChat::ShouldSyncDummyCommand("particle"));
}

TEST(QmWarListEnemyChat, FilteringKeepsChatLogPersistenceIndependent)
{
	const std::string Config = ReadTestSourceFile("src/engine/shared/config_variables_qmclient.h");
	const std::string Chat = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Menus = ReadTestSourceFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string AddLine = SourceFunctionBody(Chat, "void CChat::AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible, std::optional");
	const std::string OnMessage = SourceFunctionBody(Chat, "void CChat::OnMessage(");
	const std::string WarListSettings = SourceFunctionBody(Menus, "void CMenus::RenderSettingsTClientWarList(");

	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmWarListBlockEnemyChat, qm_warlist_block_enemy_chat, 0, 0, 1"), std::string::npos);
	EXPECT_NE(AddLine.find("g_Config.m_QmWarListBlockEnemyChat"), std::string::npos);
	EXPECT_NE(AddLine.find("GameClient()->m_WarList.IsEnemy(ClientId)"), std::string::npos);
	EXPECT_NE(AddLine.find("GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_QmWarListBlockEnemyChat"), std::string::npos);
	EXPECT_EQ(AddLine.find("m_TcWarList && g_Config.m_QmWarListBlockEnemyChat"), std::string::npos);
	EXPECT_EQ(OnMessage.find("m_QmWarListBlockEnemyChat"), std::string::npos);
	EXPECT_NE(WarListSettings.find("&g_Config.m_QmWarListBlockEnemyChat"), std::string::npos);
	EXPECT_NE(WarListSettings.find("\"tclient-warlist-block-enemy-chat\""), std::string::npos);
	EXPECT_NE(WarListSettings.find("\"Block enemy chat\""), std::string::npos);

	const size_t AddLineCall = OnMessage.find("AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage)");
	const size_t SaveLogCall = OnMessage.find("SaveChatLogLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage)");
	ASSERT_NE(AddLineCall, std::string::npos);
	ASSERT_NE(SaveLogCall, std::string::npos);
	EXPECT_LT(AddLineCall, SaveLogCall);
}

TEST(QmChatPresentation, InactiveOldLineKeepsFullOpacity)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(20.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.31f), 0.10f, false, false, 0, 0.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.05f), 0.05f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, InactiveExpiredLineFadesAndCollapses)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(30.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::EXITING);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 0.5f, 0.001f);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 1.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, -12.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetY, 0.0f, 0.001f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.3f), 0.20f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 0.0f, 0.001f);
}

TEST(QmChatPresentation, DisabledExtraAnimationsUseImmediateVisibilityStates)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(35.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.01f), 0.01f, false, false, 0, 0.0f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_EntryProgress, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);
}

TEST(QmChatPresentation, DisabledExtraAnimationsRecallHistoryImmediately)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(38.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(30.0f), 0.10f, true, false, Start + TestTicks(30.0f), 0.2f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);
}

TEST(QmChatPresentation, ReenablingExtraAnimationsDoesNotReplaySettledStates)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(39.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.01f), 0.01f, false, false, 0, 0.0f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.02f), 0.01f, false, false, 0, 0.0f, true);
	EXPECT_FALSE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.11f), 0.01f, false, false, 0, 0.0f, true);
	EXPECT_TRUE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);
}

TEST(QmChatPresentation, ReenablingExtraAnimationsDoesNotReplayExpandedHistory)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(39.0f);
	const int64_t OpenTick = Start + TestTicks(30.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, OpenTick, 0.10f, true, false, OpenTick, 0.2f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, OpenTick + TestTicks(0.01f), 0.01f, true, false, OpenTick, 0.2f, true);
	EXPECT_TRUE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);
}

TEST(QmChatPresentation, InputKeepsOldLineOpaque)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(40.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.20f), 0.18f, true, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 1.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, ClosingInputKeepsOldLineVisible)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(50.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.70f), 0.18f, true, false, 0, 0.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.72f), 0.02f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, ForceVisibleLineDoesNotAutoDecay)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(60.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(30.0f), 0.10f, false, true, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmLocalSaveJoinHint, UsesExpiringEchoMessages)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/tclient.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CTClient::MaybeShowLocalSaveJoinHint()");

	EXPECT_NE(Body.find("GameClient()->Echo(aMessage);"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->Echo(PlayersLine.c_str());"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->Echo(CodesLine.c_str());"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(aMessage, true);"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(PlayersLine.c_str(), true);"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(CodesLine.c_str(), true);"), std::string::npos);
}

TEST(QmChatPresentation, ResetAndTimeRollbackKeepFiniteFreshState)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(70.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.50f), 0.20f, false, false, 0, 0.0f);
	ASSERT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	ASSERT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);

	CChat::ResetPresentationState(Presentation);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);

	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start - TestTicks(1.0f), -1.0f, false, false, 0, 0.0f);
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderAlpha));
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderOffsetX));
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderOffsetY));
}

TEST(QmChatPresentation, SmoothYApproachesTargetWithoutOvershoot)
{
	float Y = 200.0f;
	for(int i = 0; i < 16; ++i)
	{
		const float NextY = CChat::SmoothPresentationY(Y, 120.0f, 1.0f / 60.0f);
		EXPECT_TRUE(std::isfinite(NextY));
		EXPECT_LE(NextY, Y);
		EXPECT_GE(NextY, 120.0f);
		Y = NextY;
	}

	for(int i = 0; i < 16; ++i)
	{
		const float NextY = CChat::SmoothPresentationY(Y, 180.0f, 1.0f / 30.0f);
		EXPECT_TRUE(std::isfinite(NextY));
		EXPECT_GE(NextY, Y);
		EXPECT_LE(NextY, 180.0f);
		Y = NextY;
	}
}

TEST(QmWindowModes, WindowedFullscreenRemainsABorderlessNonResizableWindow)
{
	const std::string Backend = ReadTestSourceFile("src/engine/client/backend_sdl.cpp");
	const std::string SetWindowParams = SourceFunctionBody(Backend, "void CGraphicsBackend_SDL_GL::SetWindowParams(");
	const size_t WindowedFullscreenStart = SetWindowParams.find("else // Windowed fullscreen");
	const size_t WindowedStart = SetWindowParams.find("else // Windowed", WindowedFullscreenStart + 1);
	ASSERT_NE(WindowedFullscreenStart, std::string::npos);
	ASSERT_NE(WindowedStart, std::string::npos);
	const std::string WindowedFullscreen = SetWindowParams.substr(WindowedFullscreenStart, WindowedStart - WindowedFullscreenStart);

	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowFullscreen(m_pWindow, 0);"), std::string::npos);
	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowBordered(m_pWindow, SDL_FALSE);"), std::string::npos);
	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowResizable(m_pWindow, SDL_FALSE);"), std::string::npos);
}

TEST(QmWindowModes, StartupMarksWindowedFullscreenAsBorderless)
{
	const std::string Backend = ReadTestSourceFile("src/engine/client/backend_sdl.cpp");
	const std::string Graphics = ReadTestSourceFile("src/engine/client/graphics_threaded.cpp");
	const std::string IssueInit = SourceFunctionBody(Graphics, "int CGraphics_Threaded::IssueInit()");

	const size_t WindowedFullscreenStart = IssueInit.find("else // Windowed fullscreen");
	const size_t VSyncStart = IssueInit.find("if(g_Config.m_GfxVsync)", WindowedFullscreenStart + 1);
	ASSERT_NE(WindowedFullscreenStart, std::string::npos);
	ASSERT_NE(VSyncStart, std::string::npos);
	const std::string WindowedFullscreen = IssueInit.substr(WindowedFullscreenStart, VSyncStart - WindowedFullscreenStart);

	EXPECT_NE(IssueInit.find("if(IsExclusiveFullscreen)"), std::string::npos);
	EXPECT_NE(IssueInit.find("else if(IsDesktopFullscreen)"), std::string::npos);
	EXPECT_NE(IssueInit.find("else if(IsPurelyWindowed)"), std::string::npos);
	EXPECT_NE(WindowedFullscreen.find("Flags |= IGraphicsBackend::INITFLAG_BORDERLESS;"), std::string::npos);
	EXPECT_NE(Backend.find("const bool IsWindowedFullscreen = g_Config.m_GfxFullscreen == 3;"), std::string::npos);
	EXPECT_NE(Backend.find("if(IsWindowedFullscreen || (IsFullscreen && !SupportedResolution)"), std::string::npos);
}

TEST(QmWindowModes, GraphicsMenuMapsAllFiveModesToDistinctBackendStates)
{
	const std::string Menus = ReadTestSourceFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsGraphics = SourceFunctionBody(Menus, "void CMenus::RenderSettingsGraphics(");

	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(0, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(0, true);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(3, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(2, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(1, false);"), std::string::npos);
}

TEST(QmChatInteractions, ClampBacklogLine)
{
	EXPECT_EQ(CChat::ClampBacklogLine(-3, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(0, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(6, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(7, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(20, 10, 4), 6);
}

TEST(QmChatCompletion, ParsesSupportedFirstArguments)
{
	QmChatCompletion::SContext Context;
	EXPECT_TRUE(QmChatCompletion::ParseContext("/w qi", 5, Context));
	EXPECT_EQ(Context.m_Provider, QmChatCompletion::EProvider::PLAYER);
	EXPECT_EQ(Context.m_Query, "qi");
	EXPECT_EQ(Context.m_ReplaceStart, 3u);
	EXPECT_EQ(Context.m_ReplaceEnd, 5u);

	EXPECT_TRUE(QmChatCompletion::ParseContext("/map go", 7, Context));
	EXPECT_EQ(Context.m_Provider, QmChatCompletion::EProvider::MAP);
	EXPECT_EQ(Context.m_Query, "go");
	EXPECT_FALSE(QmChatCompletion::ParseContext("/unknown qi", 11, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("/team 1", 7, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("hello", 5, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("", 0, Context));
	EXPECT_TRUE(QmChatCompletion::ParseContext("/w", 2, Context));
	EXPECT_TRUE(Context.m_Query.empty());
}

TEST(QmChatCompletion, ParsesPlayerCandidatesRequestedByTab)
{
	QmChatCompletion::SContext Context;
	EXPECT_TRUE(QmChatCompletion::ParsePlayerTabContext("qi", 2, Context));
	EXPECT_EQ(Context.m_Provider, QmChatCompletion::EProvider::PLAYER);
	EXPECT_EQ(Context.m_Query, "qi");
	EXPECT_EQ(Context.m_ReplaceStart, 0u);
	EXPECT_EQ(Context.m_ReplaceEnd, 2u);
	EXPECT_TRUE(Context.m_AppendColon);

	EXPECT_TRUE(QmChatCompletion::ParsePlayerTabContext("hello qi", 8, Context));
	EXPECT_EQ(Context.m_Query, "qi");
	EXPECT_EQ(Context.m_ReplaceStart, 6u);
	EXPECT_EQ(Context.m_ReplaceEnd, 8u);
	EXPECT_FALSE(Context.m_AppendColon);
	EXPECT_FALSE(QmChatCompletion::ParsePlayerTabContext("/unknown", 8, Context));
}

TEST(QmChatCompletion, HidesAfterFirstArgumentIsComplete)
{
	QmChatCompletion::SContext Context;
	EXPECT_FALSE(QmChatCompletion::ParseContext("/w qi hello", 11, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("/w qi hello", 5, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("/w \"Qi Men\" hello", 17, Context));
	EXPECT_FALSE(QmChatCompletion::ParseContext("/w \"Qi Men\"", 7, Context));
	EXPECT_TRUE(QmChatCompletion::ParseContext("/w \"Qi M", 8, Context));
	EXPECT_EQ(Context.m_Query, "Qi M");
}

TEST(QmChatCompletion, CompletesAndQuotesCandidateWithoutSubmitting)
{
	QmChatCompletion::SContext Context;
	ASSERT_TRUE(QmChatCompletion::ParseContext("/w qi", 5, Context));
	char aOutput[256];
	size_t CursorOffset = 0;
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("/w qi", Context, "Qi Men", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "/w \"Qi Men\" ");
	EXPECT_EQ(CursorOffset, str_length(aOutput));

	ASSERT_TRUE(QmChatCompletion::ParseContext("/w q", 4, Context));
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("/w q", Context, "Qi \"Men\"", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "/w \"Qi \\\"Men\\\"\" ");
	EXPECT_EQ(CursorOffset, str_length(aOutput));

	ASSERT_TRUE(QmChatCompletion::ParseContext("/w foo", 6, Context));
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("/w foo", Context, "foo\\bar", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "/w \"foo\\\\bar\" ");
	EXPECT_EQ(CursorOffset, str_length(aOutput));

	ASSERT_TRUE(QmChatCompletion::ParsePlayerTabContext("qi", 2, Context));
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("qi", Context, "Qi Men", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "Qi Men: ");
	EXPECT_EQ(CursorOffset, str_length(aOutput));

	ASSERT_TRUE(QmChatCompletion::ParsePlayerTabContext("qi hello", 2, Context));
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("qi hello", Context, "Qi Men", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "Qi Men: hello");

	ASSERT_TRUE(QmChatCompletion::ParsePlayerTabContext("foo", 3, Context));
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("foo", Context, "foo\\bar", aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "foo\\bar: ");
}

TEST(QmChatCompletion, RanksPrefixBeforeContains)
{
	std::vector<QmChatCompletion::SCandidate> vCandidates;
	QmChatCompletion::AddMatchingCandidate(vCandidates, "aQi", "qi");
	QmChatCompletion::AddMatchingCandidate(vCandidates, "Qimen", "qi");
	QmChatCompletion::AddMatchingCandidate(vCandidates, "奇门", "qi", true);
	QmChatCompletion::SortCandidates(vCandidates);
	ASSERT_EQ(vCandidates.size(), 3u);
	EXPECT_EQ(vCandidates[0].m_Value, "Qimen");
	EXPECT_EQ(vCandidates[0].m_MatchOffset, 0);
	EXPECT_EQ(vCandidates[1].m_Value, "aQi");
	EXPECT_EQ(vCandidates[2].m_Value, "奇门");
}

TEST(QmChatCompletion, MatchesChinesePlayerNamesByFullPinyinAndInitials)
{
	std::vector<QmChatCompletion::SCandidate> vCandidates;
	QmChatCompletion::AddMatchingCandidate(vCandidates, "奇门", "qimen", true);
	QmChatCompletion::AddMatchingCandidate(vCandidates, "测试玩家", "cswj", true);
	ASSERT_EQ(vCandidates.size(), 2u);
	EXPECT_EQ(vCandidates[0].m_MatchOffset, -1);
	EXPECT_EQ(vCandidates[1].m_MatchOffset, -1);
}

TEST(QmChatCompletion, KeepsMapTypeAsDisplayOnlyMetadata)
{
	std::vector<QmChatCompletion::SCandidate> vCandidates;
	QmChatCompletion::AddMatchingCandidate(vCandidates, "Kobra 3", "kob", false, "Moderate");
	QmChatCompletion::AddMatchingCandidate(vCandidates, "Aip-Gores", "", false, "Brutal");
	ASSERT_EQ(vCandidates.size(), 2u);
	EXPECT_EQ(vCandidates[0].m_Value, "Kobra 3");
	EXPECT_EQ(vCandidates[0].m_Detail, "Moderate");
	EXPECT_EQ(vCandidates[1].m_Value, "Aip-Gores");
	EXPECT_EQ(vCandidates[1].m_Detail, "Brutal");

	QmChatCompletion::SContext Context;
	ASSERT_TRUE(QmChatCompletion::ParseContext("/map kob", 8, Context));
	char aOutput[256];
	size_t CursorOffset = 0;
	ASSERT_TRUE(QmChatCompletion::ApplyCandidate("/map kob", Context, vCandidates[0].m_Value.c_str(), aOutput, sizeof(aOutput), CursorOffset));
	EXPECT_STREQ(aOutput, "/map \"Kobra 3\" ");
}

TEST(QmChatCompletion, SizesCandidatePopupToContent)
{
	EXPECT_FLOAT_EQ(QmChatCompletion::CalculateCandidatePopupWidth(500.0f, 42.0f, false), 80.0f);
	EXPECT_FLOAT_EQ(QmChatCompletion::CalculateCandidatePopupWidth(500.0f, 170.0f, false), 180.0f);
	EXPECT_FLOAT_EQ(QmChatCompletion::CalculateCandidatePopupWidth(500.0f, 170.0f, true), 184.0f);
	EXPECT_FLOAT_EQ(QmChatCompletion::CalculateCandidatePopupWidth(160.0f, 170.0f, true), 160.0f);
}

TEST(QmChatCompletion, ExtractsDifficultyCategoryInsteadOfGameType)
{
	std::string Category;
	EXPECT_TRUE(QmChatCompletion::ExtractMapCategory("Moderate", "DDNet GER", Category));
	EXPECT_EQ(Category, "Moderate");
	EXPECT_TRUE(QmChatCompletion::ExtractMapCategory("None", "DDNet GER - Brutal", Category));
	EXPECT_EQ(Category, "Brutal");
	EXPECT_TRUE(QmChatCompletion::ExtractMapCategory("", "DDNet CHN - Novice", Category));
	EXPECT_EQ(Category, "Novice");
	EXPECT_FALSE(QmChatCompletion::ExtractMapCategory("None", "DDNet GER", Category));
	EXPECT_TRUE(Category.empty());
}

TEST(QmChatCompletion, UsesOfficialDdnetMapRepositoryCategories)
{
	std::string Category;
	EXPECT_TRUE(QmChatCompletion::FindOfficialDdnetMapCategory("#wontfix", Category));
	EXPECT_EQ(Category, "Moderate");
	EXPECT_TRUE(QmChatCompletion::FindOfficialDdnetMapCategory("Away", Category));
	EXPECT_EQ(Category, "DDmaX Next");
	EXPECT_TRUE(QmChatCompletion::FindOfficialDdnetMapCategory("kobra 3", Category));
	EXPECT_EQ(Category, "Novice");
	EXPECT_TRUE(QmChatCompletion::FindOfficialDdnetMapCategory("Experiment", Category));
	EXPECT_EQ(Category, "DDmaX Next");
	EXPECT_TRUE(QmChatCompletion::FindOfficialDdnetMapCategory("experiment", Category));
	EXPECT_EQ(Category, "Oldschool");
	EXPECT_FALSE(QmChatCompletion::FindOfficialDdnetMapCategory("EXPERIMENT", Category));
	EXPECT_TRUE(Category.empty());
	EXPECT_FALSE(QmChatCompletion::FindOfficialDdnetMapCategory("001", Category));
	EXPECT_TRUE(Category.empty());
}

TEST(QmChatCompletion, LabelsUnknownMapsAsOtherOnlyInDdnetMode)
{
	std::string Category;
	QmChatCompletion::ResolveMapCompletionCategory("#wontfix", true, "Insane", Category);
	EXPECT_EQ(Category, "Moderate");
	QmChatCompletion::ResolveMapCompletionCategory("001", true, "Insane", Category);
	EXPECT_EQ(Category, "Other");
	QmChatCompletion::ResolveMapCompletionCategory("001", false, "Insane", Category);
	EXPECT_EQ(Category, "Insane");
}

TEST(QmChatCompletion, PrefersKnownCategoryForDuplicateMaps)
{
	std::vector<QmChatCompletion::SCandidate> vCandidates;
	QmChatCompletion::AddMatchingCandidate(vCandidates, "Kobra 3", "", false);
	QmChatCompletion::AddMatchingCandidate(vCandidates, "Kobra 3", "", false, "Moderate");
	QmChatCompletion::SortCandidates(vCandidates);
	ASSERT_EQ(vCandidates.size(), 2u);
	EXPECT_EQ(vCandidates[0].m_Detail, "Moderate");
}

TEST(QmChatCompletion, ExtractsOrdinaryPlayerMapVoteNames)
{
	std::string MapName;
	EXPECT_TRUE(QmChatCompletion::ExtractMapNameFromVoteOption("Map: gores", MapName));
	EXPECT_EQ(MapName, "gores");
	EXPECT_TRUE(QmChatCompletion::ExtractMapNameFromVoteOption("Kobra 3 by Fňokurka | 3/5 ★", MapName));
	EXPECT_EQ(MapName, "Kobra 3");
	EXPECT_FALSE(QmChatCompletion::ExtractMapNameFromVoteOption("Change server settings", MapName));
	EXPECT_TRUE(MapName.empty());
}

TEST(QmChatInteractions, ScrollbarValueToBacklogLine)
{
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(1.0f, 12), 0);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.0f, 12), 12);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.5f, 12), 6);
}

TEST(QmChatInteractions, BacklogLineToScrollbarValue)
{
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(0, 12), 1.0f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(12, 12), 0.0f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(6, 12), 0.5f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(20, 12), 0.0f);
}

TEST(QmFastPracticeCommands, TeleCursorTargetMatchesPracticeCursorWorldConversion)
{
	const vec2 CharacterPos(100.0f, 200.0f);
	const vec2 Target(400.0f, 0.0f);
	const vec2 Result = CFastPractice::PracticeTeleCursorTarget(CharacterPos, Target, 2.0f, 100, 50);

	EXPECT_FLOAT_EQ(Result.x, 750.0f);
	EXPECT_FLOAT_EQ(Result.y, 200.0f);
}

TEST(QmFastPracticeCommands, TeleportDefaultsToAimingOrSpectatingPosition)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const size_t CommandBlock = Source.find("if(Cmd == \"tp\" || Cmd == \"teleport\" || Cmd == \"tc\" || Cmd == \"telecursor\")");
	ASSERT_NE(CommandBlock, std::string::npos);
	const size_t TelecursorBranch = Source.find("if(Cmd == \"tc\" || Cmd == \"telecursor\")", CommandBlock);
	ASSERT_NE(TelecursorBranch, std::string::npos);
	const std::string DefaultTargetBlock = Source.substr(CommandBlock, TelecursorBranch - CommandBlock);

	EXPECT_NE(DefaultTargetBlock.find("vec2 Target = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];"), std::string::npos);
	EXPECT_EQ(DefaultTargetBlock.find("PracticeTeleCursorTarget"), std::string::npos);
}

TEST(QmFastPracticeCommands, SpectatorCommandKeepsPracticeStateOnSnapshotMiss)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CFastPractice::ConsumeSpectatorCommand()");

	EXPECT_EQ(Body.find("Disable();"), std::string::npos);
	EXPECT_NE(Body.find("m_PracticeWorldInitialized = false;"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_PredictedDummyId = -1;"), std::string::npos);
}

TEST(QmFastPracticeCommands, PredictionLoopsReuseNormalPreInputAndFreezeSemantics)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string OverrideBody = SourceFunctionBody(Source, "bool CFastPractice::OverridePredict()");
	const std::string VisualBody = SourceFunctionBody(Source, "int CFastPractice::ApplyVisualFastInputPrediction(");

	EXPECT_NE(OverrideBody.find("GameClient()->ApplyPreInputs(Tick, true, GameClient()->m_PredictedWorld);"), std::string::npos);
	EXPECT_NE(OverrideBody.find("GameClient()->ApplyPreInputs(Tick, false, GameClient()->m_PredictedWorld);"), std::string::npos);
	EXPECT_NE(OverrideBody.find("g_Config.m_ClPredictFreeze == 2"), std::string::npos);
	EXPECT_NE(VisualBody.find("GameClient()->ApplyPreInputs(Tick, true, VisualWorld);"), std::string::npos);
	EXPECT_NE(VisualBody.find("GameClient()->ApplyPreInputs(Tick, false, VisualWorld);"), std::string::npos);
	EXPECT_NE(VisualBody.find("VisualWorld.m_WorldConfig.m_PredictEvents = false;"), std::string::npos);
}

TEST(QmFastPracticeCommands, PracticeWorldKeepsDDRaceTeleportPredictionEnabled)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CFastPractice::SyncPracticeWorldConfig()");

	EXPECT_NE(Body.find("m_PracticeBaseWorld.m_WorldConfig.m_PredictDDRace = true;"), std::string::npos);
	EXPECT_NE(Body.find("m_PracticeBaseWorld.m_WorldConfig.m_PredictTeleport = true;"), std::string::npos);
}

TEST(QmFastPracticeCommands, BaseWorldTickTracksTileFeedbackForMainAndDummy)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CFastPractice::AdvanceBaseWorldToTick(");

	EXPECT_NE(Source.find("void CFastPractice::TrackPracticeTileFeedback(int ClientId, CCharacter *pChar, const vec2 &BeforePos)"), std::string::npos);
	EXPECT_NE(Body.find("const vec2 LocalPosBeforeTick = pLocalChar->Core()->m_Pos;"), std::string::npos);
	EXPECT_NE(Body.find("const vec2 DummyPosBeforeTick = pDummyChar ? pDummyChar->Core()->m_Pos : vec2(0.0f, 0.0f);"), std::string::npos);
	EXPECT_EQ(Body.find("LocalPrevPosBeforeTick"), std::string::npos);
	EXPECT_EQ(Body.find("DummyPrevPosBeforeTick"), std::string::npos);
	EXPECT_NE(Body.find("TrackPracticeTileFeedback(LocalClientId, pLocalChar, LocalPosBeforeTick);"), std::string::npos);
	EXPECT_NE(Body.find("TrackPracticeTileFeedback(DummyClientId, pDummyChar, DummyPosBeforeTick);"), std::string::npos);
}

TEST(QmFastPracticeCommands, TileFeedbackRecordsTeleportsAndOnlyNewDeathTileEntries)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CFastPractice::TrackPracticeTileFeedback(");

	EXPECT_NE(Body.find("PracticePathContainsTeleport(Collision(), BeforePos, AfterPos)"), std::string::npos);
	EXPECT_NE(Body.find("EvaluatePracticeTileFeedback("), std::string::npos);
	EXPECT_NE(Body.find("StoreLastTeleport(ClientId, AfterPos);"), std::string::npos);
	EXPECT_NE(Body.find("const bool WasInDeath = IsCharacterTouchingDeathTile(Collision(), BeforeTickPos, pChar->GetProximityRadius());"), std::string::npos);
	EXPECT_NE(Body.find("const bool IsInDeath = IsCharacterTouchingDeathTile(Collision(), AfterPos, pChar->GetProximityRadius());"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_Effects.PlayerDeath(AfterPos, ClientId, 1.0f);"), std::string::npos);
}

TEST(QmFastPracticeCommands, TileFeedbackDecisionMatchesGameplayEvents)
{
	const auto TeleportDecision = CFastPractice::EvaluatePracticeTileFeedback(true, false, false, 12.0f);
	EXPECT_TRUE(TeleportDecision.m_RecordTeleport);
	EXPECT_FALSE(TeleportDecision.m_RecordDeath);
	EXPECT_FALSE(TeleportDecision.m_PlayDeathFeedback);

	const auto StationaryTeleportDecision = CFastPractice::EvaluatePracticeTileFeedback(true, false, false, 0.0f);
	EXPECT_FALSE(StationaryTeleportDecision.m_RecordTeleport);

	const auto EnterDeathDecision = CFastPractice::EvaluatePracticeTileFeedback(false, false, true, 12.0f);
	EXPECT_FALSE(EnterDeathDecision.m_RecordTeleport);
	EXPECT_TRUE(EnterDeathDecision.m_RecordDeath);
	EXPECT_TRUE(EnterDeathDecision.m_PlayDeathFeedback);

	const auto StayInDeathDecision = CFastPractice::EvaluatePracticeTileFeedback(false, true, true, 12.0f);
	EXPECT_FALSE(StayInDeathDecision.m_RecordDeath);
	EXPECT_FALSE(StayInDeathDecision.m_PlayDeathFeedback);
}

TEST(QmChatInteractions, ClickDragThreshold)
{
	EXPECT_TRUE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(12.0f, 12.0f)));
	EXPECT_FALSE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(30.0f, 10.0f)));
}

TEST(QmChatInteractions, ChatInputClipPaddingDoesNotExpandContentScrollArea)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CChat::OnRender()");

	EXPECT_NE(Body.find("const float InputContentHeight = 2.25f * InputCursor.m_FontSize;"), std::string::npos);
	EXPECT_NE(Body.find("const float InputClipPaddingTop = maximum(1.0f, InputCursor.m_FontSize * 0.18f);"), std::string::npos);
	EXPECT_NE(Body.find("const float InputClipPaddingBottom = maximum(1.0f, InputCursor.m_FontSize * 0.10f);"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect InputContentRect"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect InputClippingRect"), std::string::npos);
	EXPECT_NE(Body.find("InputContentRect.y + InputClipPaddingTop - ScrollOffset"), std::string::npos);
	EXPECT_NE(Body.find("m_Input.GetCaretPosition().y - InputClipPaddingTop - ScrollOffsetChange"), std::string::npos);
	EXPECT_NE(Body.find("CaretPositionY < InputContentRect.y"), std::string::npos);
	EXPECT_NE(Body.find("InputContentRect.y + InputContentRect.h"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->ClipEnable((int)(InputClippingRect.x * XScale)"), std::string::npos);
	EXPECT_EQ(Body.find("CaretPositionY < InputClippingRect.y"), std::string::npos);
}

TEST(QmChatInteractions, LiveDirectorBlocksOnlyPauseCommand)
{
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("   /pause"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause "));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause 1"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/PAUSE"));

	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand(nullptr));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand(""));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("please /pause"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("/paused"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("/team 1"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("hello"));
}

TEST(QmChatInteractions, AppendsBlockWordsWithSeparator)
{
	char aList[32] = "";

	EXPECT_TRUE(CChat::AppendBlockWordToList(aList, sizeof(aList), "spam"));
	EXPECT_STREQ(aList, "spam");

	EXPECT_TRUE(CChat::AppendBlockWordToList(aList, sizeof(aList), "eggs"));
	EXPECT_STREQ(aList, "spam;eggs");
}

TEST(QmChatInteractions, DoesNotAppendEmptyOrFullBlockWords)
{
	char aList[8] = "filled";

	EXPECT_FALSE(CChat::AppendBlockWordToList(aList, sizeof(aList), ""));
	EXPECT_STREQ(aList, "filled");

	EXPECT_FALSE(CChat::AppendBlockWordToList(aList, sizeof(aList), "x"));
	EXPECT_STREQ(aList, "filled");
}

TEST(QmChatBlockWords, HideActionOnlySuppressesMatchedRemotePlayerMessages)
{
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::REPLACE, true, 5, false, 0));
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, false, 5, false, 0));

	EXPECT_TRUE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, 5, false, 0));
	EXPECT_TRUE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, 5, false, 1));
	EXPECT_TRUE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, 5, false, TEAM_WHISPER_RECV));
}

TEST(QmChatBlockWords, HideActionKeepsLocalAndNonPlayerMessagesVisible)
{
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, 5, true, 0));
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, -1, false, 0));
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, -2, false, 0));
	EXPECT_FALSE(CChat::ShouldHideBlockWordsMessage(CChat::EBlockWordsAction::HIDE_MESSAGE, true, 5, false, TEAM_WHISPER_SEND));
}

TEST(QmChatBlockWords, MatchedMessageKeepsRawConsoleAndChatLogPaths)
{
	const std::string Config = ReadTestSourceFile("src/engine/shared/config_variables_qmclient.h");
	const std::string Chat = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Menus = ReadTestSourceFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string AddLine = SourceFunctionBody(Chat, "void CChat::AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible, std::optional");
	const std::string OnMessage = SourceFunctionBody(Chat, "void CChat::OnMessage(");

	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmBlockWordsAction, qm_block_words_action, 0, 0, 1"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmBlockWordsAction == 0"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmBlockWordsAction = 0;"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmBlockWordsAction == 1"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmBlockWordsAction = 1;"), std::string::npos);
	EXPECT_NE(Menus.find("qmclient-word-filter-match-mode\", &LabelCol, Localize(\"Mode\")"), std::string::npos);
	const size_t RawConsoleCall = AddLine.find("PrintBlockedMessageToConsole(ClientId, Team, pLine);");
	const size_t HideBranch = AddLine.find("if(CanHideBlockWordsMessage)");
	ASSERT_NE(RawConsoleCall, std::string::npos);
	ASSERT_NE(HideBranch, std::string::npos);
	EXPECT_LT(RawConsoleCall, HideBranch);
	EXPECT_NE(AddLine.find("BlockWordsConsolePrinted = true;"), std::string::npos);
	EXPECT_NE(AddLine.find("if(BlockWordsConsolePrinted)"), std::string::npos);
	EXPECT_NE(AddLine.find("ShouldHideBlockWordsMessage("), std::string::npos);
	EXPECT_NE(AddLine.find("BlockWordsAction == EBlockWordsAction::REPLACE || CanHideBlockWordsMessage"), std::string::npos);
	EXPECT_NE(AddLine.find("Client()->State() == IClient::STATE_DEMOPLAYBACK"), std::string::npos);
	EXPECT_NE(AddLine.find("ClientId == GameClient()->m_Snap.m_LocalClientId"), std::string::npos);
	EXPECT_NE(AddLine.find("GameClient()->IsLocalClientId(ClientId)"), std::string::npos);
	EXPECT_NE(OnMessage.find("SaveChatLogLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage)"), std::string::npos);
}

TEST(QmChatInteractions, BuildsEscapedWhisperCommand)
{
	char aCommand[128];

	EXPECT_TRUE(CChat::BuildWhisperCommand(aCommand, sizeof(aCommand), "Name \"A\"", "hello"));
	EXPECT_STREQ(aCommand, "/w \"Name \\\"A\\\"\" hello");
}

TEST(QmChatInteractions, BuildsEscapedSpectateCommand)
{
	char aCommand[128];

	EXPECT_TRUE(CChat::BuildSpectateCommand(aCommand, sizeof(aCommand), "Name \"A\""));
	EXPECT_STREQ(aCommand, "say /spec \"Name \\\"A\\\"\"");
}

TEST(QmChatInteractions, ServerSystemMessagesDoNotUseVisibleStarPrefix)
{
	EXPECT_STREQ(CChat::MessageNamePrefixForClientId(-1), "");
	EXPECT_STREQ(CChat::MessageNamePrefixForClientId(-2), "— ");
}

TEST(QmChatInteractions, ServerSystemMessagePathsDoNotReintroduceStarPrefix)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string OnMessage = SourceFunctionBody(Source, "void CChat::OnMessage(int MsgType, void *pRawMsg)");
	const std::string AddLine = SourceFunctionBody(Source, "void CChat::AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible, std::optional<QmHudNotifications::EServerMessageClass> KnownServerMessageClass)");

	EXPECT_EQ(OnMessage.find("*** %s"), std::string::npos);
	EXPECT_EQ(AddLine.find("\"*** \""), std::string::npos);
	EXPECT_NE(AddLine.find("MessageNamePrefixForClientId(CurrentLine.m_ClientId)"), std::string::npos);
}

TEST(QmChatInteractions, TranslateButtonSitsBeforeInputAndPopupCanCloseItself)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string RenderBody = SourceFunctionBody(Source, "void CChat::OnRender()");
	const std::string PopupBody = SourceFunctionBody(Source, "CUi::EPopupMenuFunctionResult CChat::PopupLanguageMenu(");

	EXPECT_NE(RenderBody.find("CUIRect TranslateButtonRect = {InputContentRect.x + InputContentRect.w + TranslateButtonGap"), std::string::npos);
	EXPECT_NE(RenderBody.find("InputCursor.SetPosition(vec2(x + TranslateButtonSize + TranslateButtonGap, y));"), std::string::npos);
	EXPECT_NE(PopupBody.find("FONT_ICON_XMARK"), std::string::npos);
	EXPECT_NE(PopupBody.find("TitleRect.VSplitRight(22.0f, &TitleRect, &CloseButton);"), std::string::npos);
	EXPECT_EQ(PopupBody.find("View.VSplitRight(22.0f, &View, &CloseButton);"), std::string::npos);
	EXPECT_NE(PopupBody.find("return CUi::POPUP_CLOSE_CURRENT;"), std::string::npos);
}

TEST(QmChatInteractions, ScrollbarFollowsChatEdgeAndCutoffAnimationIsConfigurable)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string RenderBody = SourceFunctionBody(Source, "void CChat::OnRender()");
	const std::string OffsetBody = SourceFunctionBody(Source, "float CChat::CalculateCutOffOffsetX(");

	EXPECT_NE(RenderBody.find("const bool ChatScrollbarOnRight = ChatAnchoredRight;"), std::string::npos);
	EXPECT_NE(RenderBody.find("ChatScrollbarOnRight ? ChatRect.w - CHAT_SCROLLBAR_WIDTH - CHAT_SCROLLBAR_MARGIN : CHAT_SCROLLBAR_MARGIN"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmChatAnimFadeDurationMs"), std::string::npos);
	EXPECT_NE(OffsetBody.find("g_Config.m_QmChatAnimSlideOut"), std::string::npos);
}

TEST(QmChatInteractions, ChatLineMenuKeepsSpectateAction)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/chat.h");
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");

	EXPECT_NE(Header.find("CButtonContainer m_SpectateButton;"), std::string::npos);
	EXPECT_NE(Header.find("void SpectateChatLine(const CChatLinePopupContext &Context);"), std::string::npos);
	EXPECT_NE(Source.find("DoEntry(&pPopupContext->m_SpectateButton, FontIcons::FONT_ICON_EYE, Localize(\"Spectate\")"), std::string::npos);
	EXPECT_NE(Source.find("GameClient()->m_Spectator.Spectate(Context.m_ClientId);"), std::string::npos);
	EXPECT_NE(Source.find("Console()->ExecuteLine(aCommand);"), std::string::npos);
}

TEST(QmChatInteractions, CommandUsagePreviewUsesLocalizableSourceText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CChat::BuildCommandUsagePreview(");
	const std::string LocalizeCommandPreviewBody = SourceFunctionBody(Source, "const char *CChat::LocalizeCommandPreviewText(");

	EXPECT_NE(Body.find("Localize(\"Query points for %s\")"), std::string::npos);
	EXPECT_NE(Body.find("Localize(\"Invite %s to the locked team\")"), std::string::npos);
	EXPECT_NE(Body.find("Localize(\"Usage: /%s %s\")"), std::string::npos);
	EXPECT_NE(Body.find("Localize(\"%s (/%s %s)\")"), std::string::npos);
	EXPECT_EQ(Body.find("%s（/%s %s）"), std::string::npos);
	EXPECT_EQ(LocalizeCommandPreviewBody.find("languages/simplified_chinese.txt"), std::string::npos);
	EXPECT_EQ(Body.find("查询"), std::string::npos);
	EXPECT_EQ(Body.find("邀请"), std::string::npos);
	EXPECT_EQ(Body.find("悄悄话"), std::string::npos);
	EXPECT_EQ(Body.find("用法"), std::string::npos);
}

TEST(QmChatInteractions, SlashCommandSuggestionsFilterCommonCommands)
{
	const auto vSuggestions = CChat::BuildSlashCommandSuggestions("/", 8);
	ASSERT_GE(vSuggestions.size(), 3u);
	EXPECT_STREQ(vSuggestions[0].m_pCommand, "/pause");
	EXPECT_STREQ(vSuggestions[1].m_pCommand, "/spec");
	EXPECT_STREQ(vSuggestions[2].m_pCommand, "/team");

	const auto vFiltered = CChat::BuildSlashCommandSuggestions("/to", 8);
	ASSERT_EQ(vFiltered.size(), 2u);
	EXPECT_STREQ(vFiltered[0].m_pCommand, "/top5");
	EXPECT_STREQ(vFiltered[1].m_pCommand, "/top");

	EXPECT_TRUE(CChat::BuildSlashCommandSuggestions("/top", 8).empty());
	EXPECT_TRUE(CChat::BuildSlashCommandSuggestions("/pause", 8).empty());
	EXPECT_TRUE(CChat::BuildSlashCommandSuggestions("hello /p", 8).empty());
	EXPECT_TRUE(CChat::BuildSlashCommandSuggestions("/unknown", 8).empty());
}

TEST(QmChatInteractions, SlashCommandSuggestionAppliesWithTrailingSpace)
{
	char aBuf[256];
	EXPECT_TRUE(CChat::ApplySlashCommandSuggestion(aBuf, sizeof(aBuf), "/p", "/pause"));
	EXPECT_STREQ(aBuf, "/pause ");

	EXPECT_TRUE(CChat::ApplySlashCommandSuggestion(aBuf, sizeof(aBuf), "/", "/w"));
	EXPECT_STREQ(aBuf, "/w ");

	EXPECT_FALSE(CChat::ApplySlashCommandSuggestion(aBuf, sizeof(aBuf), "hello", "/pause"));
}

TEST(QmChatInteractions, SlashCommandSuggestionDismissSticksUntilInputChanges)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/chat.h");
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string RefreshBody = SourceFunctionBody(Source, "void CChat::RefreshSlashCommandSuggestions()");
	const std::string InputBody = SourceFunctionBody(Source, "bool CChat::OnInput(const IInput::CEvent &Event)");
	ASSERT_FALSE(RefreshBody.empty());
	ASSERT_FALSE(InputBody.empty());

	EXPECT_NE(Header.find("m_SlashCommandSuggestionsDismissed"), std::string::npos);
	EXPECT_NE(Header.find("m_aSlashCommandSuggestionsDismissedInput"), std::string::npos);
	EXPECT_NE(RefreshBody.find("str_comp(pInput, m_aSlashCommandSuggestionsDismissedInput) == 0"), std::string::npos);
	EXPECT_NE(RefreshBody.find("m_vSlashCommandSuggestions.clear();"), std::string::npos);
	EXPECT_NE(RefreshBody.find("m_SlashCommandSuggestionsDismissed = false;"), std::string::npos);
	EXPECT_NE(InputBody.find("Event.m_Key == KEY_ESCAPE"), std::string::npos);
	EXPECT_NE(InputBody.find("m_SlashCommandSuggestionsDismissed = true;"), std::string::npos);
	EXPECT_NE(InputBody.find("str_copy(m_aSlashCommandSuggestionsDismissedInput, m_Input.GetString(), sizeof(m_aSlashCommandSuggestionsDismissedInput));"), std::string::npos);
}

TEST(QmChatInteractions, MotdPopupRespectsJoinServerInfoToggle)
{
	EXPECT_TRUE(CMotd::ShouldActivateMotdPopup("Welcome", 10, false));
	EXPECT_FALSE(CMotd::ShouldActivateMotdPopup("Welcome", 10, true));
	EXPECT_FALSE(CMotd::ShouldActivateMotdPopup("", 10, false));
	EXPECT_FALSE(CMotd::ShouldActivateMotdPopup("Welcome", 0, false));
}

TEST(QmChatInteractions, ScoreboardOnDeathRespectsUserMode)
{
	EXPECT_TRUE(CScoreboard::ShouldAutoShowOnDeath(true, true, false, false, false));
	EXPECT_FALSE(CScoreboard::ShouldAutoShowOnDeath(false, true, false, false, true));
	EXPECT_FALSE(CScoreboard::ShouldAutoShowOnDeath(true, false, false, false, true));
	EXPECT_FALSE(CScoreboard::ShouldAutoShowOnDeath(true, true, true, false, true));
	EXPECT_FALSE(CScoreboard::ShouldAutoShowOnDeath(true, true, false, true, true));
	EXPECT_FALSE(CScoreboard::ShouldAutoShowOnDeath(true, true, false, false, true));
}

TEST(QmChatInteractions, ReusesKnownServerMessageClassWithoutReanalysis)
{
	const auto Class = CChat::ResolveLineServerMessageClass(-1, "DDraceNetwork Version: 18.9", QmHudNotifications::EServerMessageClass::Prompt);
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::Prompt);
}

TEST(QmChatInteractions, FallsBackToLegacyServerMessageClassificationWhenUnknown)
{
	const auto Class = CChat::ResolveLineServerMessageClass(-1, "DDraceNetwork Version: 18.9");
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::BasicInfo);
}

TEST(QmChatInteractions, IgnoresKnownServerClassForNonServerMessages)
{
	const auto Class = CChat::ResolveLineServerMessageClass(3, "DDraceNetwork Version: 18.9", QmHudNotifications::EServerMessageClass::Prompt);
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::None);
}

TEST(QmChatInteractions, ManualVisibleTranslationCandidatesAreOnlyUntranslatedRemotePlayerLines)
{
	int aLocalIds[] = {2, 7};
	EXPECT_TRUE(CChat::IsManualVisibleTranslateCandidate(3, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(-1, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(-2, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(2, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(3, false, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(3, true, true, aLocalIds, std::size(aLocalIds)));
}

TEST(QmChatInteractions, VisibleTranslationCollectsCandidatesBeforeStartingJobs)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CChat::TranslateVisibleChatLines()");
	const size_t ScanLoop = Body.find("for(int i = m_BacklogCurLine; i < MAX_LINES; i++)");
	ASSERT_NE(ScanLoop, std::string::npos);
	const size_t CollectIndex = Body.find("aLineIndices[NumLineIndices++] = LineIndex;", ScanLoop);
	ASSERT_NE(CollectIndex, std::string::npos);
	const size_t TranslateLoop = Body.find("for(int i = 0; i < NumLineIndices; i++)", CollectIndex);
	ASSERT_NE(TranslateLoop, std::string::npos);

	EXPECT_EQ(Body.find("GameClient()->m_Translate.Translate", ScanLoop), TranslateLoop + Body.substr(TranslateLoop).find("GameClient()->m_Translate.Translate"));
}
