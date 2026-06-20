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
