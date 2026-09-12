// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <base/system.h>

#include <game/client/components/chat.h>
#include <game/client/components/console.h>
#include <game/client/components/qmclient/axiom_auto_login.h>
#include <game/client/components/qmclient/red_packet_auto_claim.h>
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

TEST(QmChatSecurity, SensitiveLoginCommandsAreClassifiedCorrectly)
{
	EXPECT_TRUE(CChat::IsSensitiveChatCommand("/login secret"));
	EXPECT_TRUE(CChat::IsSensitiveChatCommand(" \t/LOGIN secret"));
	EXPECT_TRUE(CChat::IsSensitiveChatCommand("/login\tsecret"));
	EXPECT_FALSE(CChat::IsSensitiveChatCommand("/login"));
	EXPECT_FALSE(CChat::IsSensitiveChatCommand("/login "));
	EXPECT_FALSE(CChat::IsSensitiveChatCommand("/login-secret"));
	EXPECT_FALSE(CChat::IsSensitiveChatCommand("hello /login secret"));
}

TEST(QmChatMessageMerge, EligibilityUsesExactTextSlidingWindowAndMatchingChannelsOnly)
{
	const int64_t Start = TestTicks(10.0f);

	EXPECT_TRUE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, 0, "same", Start + TestTicks(2.0f)));
	EXPECT_TRUE(CChat::CanMergePlayerMessages(2, 1, "same", Start, 7, 1, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, 1, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 1, "same", Start, 7, 0, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, 0, "same", Start + TestTicks(2.01f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, 0, "Same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(-1, 0, "same", Start, 7, 0, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, -1, 0, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, TEAM_WHISPER_RECV, "same", Start, 7, 0, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, TEAM_WHISPER_SEND, "same", Start + TestTicks(0.1f)));
	EXPECT_FALSE(CChat::CanMergePlayerMessages(2, 0, "same", Start, 7, 0, "same", Start - 1));
}

TEST(QmAxiomAutoLogin, ClassifiesOnlyExplicitLoginSuccessReplies)
{
	EXPECT_EQ(QmClassifyAxiomLoginReply("Login successful."), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("You are logged in."), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("登录成功"), EQmAxiomLoginReply::SUCCESS);
	EXPECT_EQ(QmClassifyAxiomLoginReply("Welcome, please login with /login."), EQmAxiomLoginReply::IGNORE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("Authentication is required before login."), EQmAxiomLoginReply::IGNORE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("You must be logged in to use this command."), EQmAxiomLoginReply::IGNORE);
	EXPECT_EQ(QmClassifyAxiomLoginReply("Login successful, but an error occurred."), EQmAxiomLoginReply::RETRYABLE_FAILURE);
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

TEST(QmChatInteractions, ClampBacklogLine)
{
	EXPECT_EQ(CChat::ClampBacklogLine(-3, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(0, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(6, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(7, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(20, 10, 4), 6);
}

TEST(QmChatInteractions, HudTransformInversePreservesChatOrigin)
{
	const CUIRect DefaultRect = {0.0f, 50.0f, 400.0f, 250.0f};
	const CUIRect TargetRect = {100.0f, 200.0f, 800.0f, 500.0f};
	const vec2 LogicalPoint = {73.0f, 91.0f};
	const float Scale = TargetRect.w / DefaultRect.w;
	const vec2 TransformedPoint = {
		TargetRect.x + (LogicalPoint.x - DefaultRect.x) * Scale,
		TargetRect.y + (LogicalPoint.y - DefaultRect.y) * Scale};

	const vec2 Result = CChat::InverseHudTransformPoint(TransformedPoint, DefaultRect, TargetRect);
	EXPECT_NEAR(Result.x, LogicalPoint.x, 0.001f);
	EXPECT_NEAR(Result.y, LogicalPoint.y, 0.001f);
}

TEST(QmChatInteractions, ChatLineHitStopsAtContentWidth)
{
	const CUIRect ContentRect = {5.0f, 90.0f, 120.0f, 15.0f};

	EXPECT_TRUE(CChat::IsChatLineHit(ContentRect, vec2(100.0f, 95.0f)));
	EXPECT_FALSE(CChat::IsChatLineHit(ContentRect, vec2(130.0f, 95.0f)));
	EXPECT_FALSE(CChat::IsChatLineHit(ContentRect, vec2(100.0f, 106.0f)));
}

TEST(QmChatInteractions, ChatLineMenuUsesContentBoundsAndKeepsTargetHighlighted)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/chat.h");
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string OnRender = SourceFunctionBody(Source, "void CChat::OnRender()");
	const std::string OpenMenu = SourceFunctionBody(Source, "void CChat::OpenChatLineMenu(");

	EXPECT_NE(Header.find("float m_ContentWidth"), std::string::npos);
	EXPECT_NE(Header.find("int m_LineIndex = -1"), std::string::npos);
	EXPECT_NE(OnRender.find("Line.m_ContentWidth"), std::string::npos);
	EXPECT_NE(OnRender.find("const float RenderedContentWidth = Line.m_ContentWidth * RenderScale;"), std::string::npos);
	EXPECT_NE(OnRender.find("const bool MouseInsideLine = IsChatLineHit(RenderedTextRect, MousePos);"), std::string::npos);
	EXPECT_NE(OnRender.find("ChatLineMenuOpen && m_ChatLinePopupContext.m_LineIndex == LineIndex"), std::string::npos);
	EXPECT_NE(OnRender.find("const ColorRGBA SelectionColor"), std::string::npos);
	EXPECT_NE(OnRender.find("Graphics()->DrawRect(RenderedTextRect.x"), std::string::npos);
	EXPECT_NE(OpenMenu.find("m_ChatLinePopupContext.m_LineIndex = GetLineIndex(&Line);"), std::string::npos);
	EXPECT_NE(OpenMenu.find("UiMousePos.x, UiMousePos.y"), std::string::npos);
	EXPECT_EQ(OpenMenu.find("ChatToUiScale"), std::string::npos);
	EXPECT_NE(OnRender.find("OpenChatLineMenu(*pMenuLine, GetUiMousePos());"), std::string::npos);
}

TEST(QmChatInteractions, ChatLineMenuReopensOnOtherLinesAndClosesOnEmptySpaceOrOutsideLeftClick)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string OnRender = SourceFunctionBody(Source, "void CChat::OnRender()");
	const std::string UiHeader = ReadTestSourceFile("src/game/client/ui.h");
	const std::string UiSource = ReadTestSourceFile("src/game/client/ui.cpp");

	// 菜单打开时右键不再被整体屏蔽：允许在其它消息行重新定位菜单
	EXPECT_NE(OnRender.find("const bool ChatLineMenuRequested = m_Mode != MODE_NONE && !LanguageMenuOpen && !InsideInputBlock && !InsideTranslateButton && !InsideScrollbar && !m_ScrollbarDragging && !InsideChatLineMenu && Input()->KeyPress(KEY_MOUSE_2);"), std::string::npos);
	EXPECT_EQ(OnRender.find("const bool ChatLineMenuRequested = ChatCopyActive && Input()->KeyPress(KEY_MOUSE_2);"), std::string::npos);
	// 左键拖拽复制仍只在菜单关闭时生效，避免关闭菜单时误复制消息
	EXPECT_NE(OnRender.find("const bool ChatCopyActive = m_Mode != MODE_NONE && !LanguageMenuOpen && !ChatLineMenuOpen && !InsideInputBlock && !InsideTranslateButton && !InsideScrollbar && !m_ScrollbarDragging;"), std::string::npos);
	// 空白处右键关闭已打开的菜单
	EXPECT_NE(OnRender.find("else if(ChatLineMenuOpen)"), std::string::npos);
	// 左键按下非菜单区域立即关闭菜单
	EXPECT_NE(OnRender.find("Ui()->GetPopupMenuRect(&m_ChatLinePopupContext)"), std::string::npos);
	EXPECT_NE(OnRender.find("Input()->KeyPress(KEY_MOUSE_1)"), std::string::npos);
	// CUi 提供弹窗矩形访问器
	EXPECT_NE(UiHeader.find("const CUIRect *GetPopupMenuRect(const SPopupMenuId *pId) const;"), std::string::npos);
	EXPECT_NE(UiSource.find("const CUIRect *CUi::GetPopupMenuRect("), std::string::npos);
}

TEST(QmChatCommandCompletion, KeepsDdnetTabCompletionWithoutQmExtensions)
{
	const std::string ChatHeader = ReadTestSourceFile("src/game/client/components/chat.h");
	const std::string Chat = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string BindChatHeader = ReadTestSourceFile("src/game/client/components/tclient/bindchat.h");
	const std::string BindChat = ReadTestSourceFile("src/game/client/components/tclient/bindchat.cpp");
	const std::string CMake = ReadTestSourceFile("CMakeLists.txt");
	const std::string OnInput = SourceFunctionBody(Chat, "bool CChat::OnInput(");
	const std::string RegisterCommand = SourceFunctionBody(Chat, "void CChat::RegisterCommand(");
	const size_t CompletionBufferGuard = OnInput.find("if(!m_CompletionUsed)");
	const size_t PlayerCompletionGuard = OnInput.find("if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')");

	EXPECT_EQ(ChatHeader.find("SSlashCommandSuggestion"), std::string::npos);
	EXPECT_EQ(ChatHeader.find("BuildCommandUsagePreview"), std::string::npos);
	EXPECT_NE(ChatHeader.find("m_ServerCommandsNeedSorting"), std::string::npos);
	EXPECT_EQ(Chat.find("RenderSlashCommandSuggestions"), std::string::npos);
	EXPECT_EQ(Chat.find("BuildCommandUsagePreview"), std::string::npos);
	EXPECT_EQ(Chat.find("Autocompletion hint"), std::string::npos);
	EXPECT_EQ(OnInput.find("ApplySelectedSlashCommandSuggestion"), std::string::npos);
	EXPECT_EQ(OnInput.find("Event.m_Key == KEY_TAB && m_Input.GetString()[0] == '/'"), std::string::npos);
	ASSERT_NE(CompletionBufferGuard, std::string::npos);
	ASSERT_NE(PlayerCompletionGuard, std::string::npos);
	EXPECT_LT(CompletionBufferGuard, PlayerCompletionGuard);
	EXPECT_NE(OnInput.find("const bool ShiftPressed = Input()->ShiftIsPressed();"), std::string::npos);
	EXPECT_NE(OnInput.find("if(m_aCompletionBuffer[0] == '/' && !m_vServerCommands.empty())"), std::string::npos);
	EXPECT_NE(OnInput.find("str_startswith_nocase(Command.m_aName, pCommandStart)"), std::string::npos);
	EXPECT_NE(OnInput.find("if(m_Input.GetString()[0] == '/' && (str_find(pCompletionString, \" \") || str_find(pCompletionString, \"\\\"\")))"), std::string::npos);
	EXPECT_NE(OnInput.find("m_aPlayerCompletionList"), std::string::npos);
	EXPECT_NE(RegisterCommand.find("m_ServerCommandsNeedSorting = true;"), std::string::npos);
	EXPECT_NE(OnInput.find("std::sort(m_vServerCommands.begin(), m_vServerCommands.end());"), std::string::npos);
	EXPECT_NE(ChatHeader.find("std::vector<CCommand> m_vServerCommands"), std::string::npos);
	EXPECT_EQ(BindChatHeader.find("ChatDoAutocomplete"), std::string::npos);
	EXPECT_EQ(BindChat.find("CBindChat::ChatDoAutocomplete"), std::string::npos);
	EXPECT_EQ(CMake.find("components/chat_completion"), std::string::npos);
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

TEST(QmChatInteractions, ClickDragThreshold)
{
	EXPECT_TRUE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(12.0f, 12.0f)));
	EXPECT_FALSE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(30.0f, 10.0f)));
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
