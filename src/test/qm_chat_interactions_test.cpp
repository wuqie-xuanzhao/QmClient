#include <game/client/components/chat.h>
#include <game/client/components/tclient/fast_practice.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <iterator>
#include <string>

namespace
{
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

TEST(QmChatInteractions, ClampBacklogLine)
{
	EXPECT_EQ(CChat::ClampBacklogLine(-3, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(0, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(6, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(7, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(20, 10, 4), 6);
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

	EXPECT_NE(RenderBody.find("CUIRect TranslateButtonRect = {x"), std::string::npos);
	EXPECT_NE(RenderBody.find("InputCursor.SetPosition(vec2(x + TranslateButtonSize + TranslateButtonGap, y));"), std::string::npos);
	EXPECT_NE(PopupBody.find("FONT_ICON_XMARK"), std::string::npos);
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
