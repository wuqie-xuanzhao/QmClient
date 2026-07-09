#include <base/system.h>

#include <game/client/live/live_director.h>
#include <game/client/live/live_finish_ranking.h>
#include <game/client/live/live_replay_buffer.h>
#include <game/client/live/live_replay_sidecar.h>
#include <game/client/live/live_team_render_filter.h>
#include <game/teamscore.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
	std::array<int, MAX_CLIENTS> DefaultTeams()
	{
		std::array<int, MAX_CLIENTS> aTeams{};
		aTeams.fill(TEAM_FLOCK);
		return aTeams;
	}

	std::array<bool, MAX_CLIENTS> DefaultActivePlayers()
	{
		std::array<bool, MAX_CLIENTS> aActivePlayers{};
		aActivePlayers.fill(false);
		return aActivePlayers;
	}
} // namespace

TEST(QmLiveDirector, BuildsRowsForActiveDDRaceTeams)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[1] = TEAM_FLOCK;
	aTeams[2] = 4;
	aTeams[3] = 4;
	aTeams[4] = 7;
	aTeams[5] = TEAM_SUPER;
	for(int ClientId = 1; ClientId <= 5; ++ClientId)
		aActivePlayers[ClientId] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 2u);
	EXPECT_TRUE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[0].m_Team, 4);
	EXPECT_EQ(Director.Entries()[0].m_NumPlayers, 2);
	EXPECT_EQ(Director.Entries()[1].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[1].m_Team, 7);
	EXPECT_EQ(Director.Entries()[1].m_NumPlayers, 1);
	EXPECT_EQ(Director.FallbackPlayer(), 1);
}

TEST(QmLiveDirector, IgnoresInactivePlayers)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[2] = 3;
	aTeams[6] = 6;
	aActivePlayers[6] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 1u);
	EXPECT_TRUE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::DDRACE_TEAM);
	EXPECT_EQ(Director.Entries()[0].m_Team, 6);
	EXPECT_EQ(Director.Entries()[0].m_NumPlayers, 1);
	EXPECT_EQ(Director.FallbackPlayer(), 6);
}

TEST(QmLiveDirector, FallsBackToPlayerRowsWithoutDDRaceTeams)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[2] = TEAM_FLOCK;
	aTeams[7] = TEAM_SUPER;
	aActivePlayers[2] = true;
	aActivePlayers[7] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	ASSERT_EQ(Director.Entries().size(), 2u);
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.Entries()[0].m_Type, CLiveDirector::EEntryType::PLAYER);
	EXPECT_EQ(Director.Entries()[0].m_ClientId, 2);
	EXPECT_EQ(Director.Entries()[1].m_Type, CLiveDirector::EEntryType::PLAYER);
	EXPECT_EQ(Director.Entries()[1].m_ClientId, 7);
	EXPECT_EQ(Director.FallbackPlayer(), 2);
}

TEST(QmLiveDirector, SelectRandomTeamUsesStableModulo)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[0] = 2;
	aTeams[1] = 5;
	aActivePlayers[0] = true;
	aActivePlayers[1] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_EQ(Director.SelectRandomTeam(0), 2);
	EXPECT_EQ(Director.SelectRandomTeam(1), 5);
	EXPECT_EQ(Director.SelectRandomTeam(2), 2);
}

TEST(QmLivePresentationMode, LiveObserverKeepsCompatDirectorPresentation)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t ModeStart = Source.find("CGameClient::EQmLivePresentationMode CGameClient::LivePresentationMode() const");
	ASSERT_NE(ModeStart, std::string::npos);
	const size_t ModeEnd = Source.find("void CGameClient::OnConsoleInit()", ModeStart);
	ASSERT_NE(ModeEnd, std::string::npos);
	const std::string Body = Source.substr(ModeStart, ModeEnd - ModeStart);

	EXPECT_NE(Body.find("Client()->QmLiveDirectorActive()"), std::string::npos);
}

TEST(QmClientStartup, OnConsoleInitAvoidsAccessorsForCoreInterfaceSetup)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t InitStart = Source.find("void CGameClient::OnConsoleInit()");
	ASSERT_NE(InitStart, std::string::npos);
	const size_t InitEnd = Source.find("// One-shot migration", InitStart);
	ASSERT_NE(InitEnd, std::string::npos);
	const std::string Body = Source.substr(InitStart, InitEnd - InitStart);

	EXPECT_EQ(Body.find("Client()->Foes()"), std::string::npos);
	EXPECT_EQ(Body.find("Console()->Register("), std::string::npos);
	EXPECT_EQ(Body.find("Console()->Chain("), std::string::npos);
	EXPECT_NE(Body.find("m_pClient->Foes()"), std::string::npos);
	EXPECT_NE(Body.find("IConsole *pConsole = m_pConsole;"), std::string::npos);
}

TEST(QmClientStartup, LanguageConchainDefersClientTimeUntilAfterConfigCallback)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t ChainStart = Source.find("void CGameClient::ConchainLanguageUpdate(");
	ASSERT_NE(ChainStart, std::string::npos);
	const size_t ChainEnd = Source.find("void CGameClient::ConchainSpecialInfoupdate(", ChainStart);
	ASSERT_NE(ChainEnd, std::string::npos);
	const std::string Body = Source.substr(ChainStart, ChainEnd - ChainStart);

	const size_t CallbackPos = Body.find("pfnCallback(pResult, pCallbackUserData);");
	const size_t RuntimeGuardPos = Body.find("CanRunRuntimeConfigConchainEffects()");
	ASSERT_NE(CallbackPos, std::string::npos);
	ASSERT_NE(RuntimeGuardPos, std::string::npos);
	EXPECT_LT(CallbackPos, RuntimeGuardPos);
	EXPECT_EQ(Body.find("pThis->Client()->GlobalTime()"), std::string::npos);
	EXPECT_NE(Source.find("bool CGameClient::CanRunRuntimeConfigConchainEffects() const"), std::string::npos);
}

TEST(QmClientStartup, InfoConchainsSkipNetworkSideEffectsBeforeClientRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t ChainStart = Source.find("void CGameClient::ConchainSpecialInfoupdate(");
	ASSERT_NE(ChainStart, std::string::npos);
	const size_t ChainEnd = Source.find("IGameClient *CreateGameClient()", ChainStart);
	ASSERT_NE(ChainEnd, std::string::npos);
	const std::string Body = Source.substr(ChainStart, ChainEnd - ChainStart);

	EXPECT_NE(Source.find("bool CGameClient::CanRunRuntimeConfigConchainEffects() const"), std::string::npos);
	EXPECT_NE(Body.find("CanRunRuntimeConfigConchainEffects()"), std::string::npos);
	EXPECT_EQ(Body.find(")->Client()->DummyConnected()"), std::string::npos);
	EXPECT_EQ(Body.find(")->SendInfo(false);"), std::string::npos);
	EXPECT_EQ(Body.find(")->SendDummyInfo(false);"), std::string::npos);
}

TEST(QmClientStartup, OnInitUsesCachedClientForEarlyStartupSetup)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t InitStart = Source.find("void CGameClient::OnInit()");
	ASSERT_NE(InitStart, std::string::npos);
	const size_t InitEnd = Source.find("void CGameClient::OnWindowResize()", InitStart);
	ASSERT_NE(InitEnd, std::string::npos);
	const std::string Body = Source.substr(InitStart, InitEnd - InitStart);

	EXPECT_NE(Body.find("IClient *pClient = m_pClient;"), std::string::npos);
	EXPECT_NE(Body.find("pClient->SetLoadingCallback("), std::string::npos);
	EXPECT_EQ(Body.find("Client()->SetLoadingCallback("), std::string::npos);
	EXPECT_EQ(Body.find("Client()->SnapSetStaticsize("), std::string::npos);
	EXPECT_EQ(Body.find("Client()->UpdateAndSwap("), std::string::npos);
}

TEST(QmClientStartup, MenuLoadingBackgroundDoesNotRequireComponentClientAccessor)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const size_t LoadingStart = Source.find("void CMenus::RenderLoading(");
	ASSERT_NE(LoadingStart, std::string::npos);
	const size_t LoadingEnd = Source.find("void CMenus::FinishLoading()", LoadingStart);
	ASSERT_NE(LoadingEnd, std::string::npos);
	const std::string LoadingBody = Source.substr(LoadingStart, LoadingEnd - LoadingStart);
	const size_t RenderStart = Source.find("void CMenus::RenderBackground()");
	ASSERT_NE(RenderStart, std::string::npos);
	const size_t RenderEnd = Source.find("int CMenus::DoButton_CheckBox_Tristate(", RenderStart);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string Body = Source.substr(RenderStart, RenderEnd - RenderStart);

	EXPECT_EQ(LoadingBody.find("Client()->UpdateAndSwap()"), std::string::npos);
	EXPECT_NE(LoadingBody.find("GameClient()->UpdateAndSwapClient()"), std::string::npos);
	EXPECT_EQ(Body.find("Client()->GlobalTime()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->GlobalTimeOrZero()"), std::string::npos);
}

TEST(QmClientStartup, SkinInitializationDoesNotRenderLoadingInsideStorageScanCallbacks)
{
	const std::string SkinsSource = ReadTestSourceFile("src/game/client/components/skins.cpp");
	const size_t SkinsInitStart = SkinsSource.find("void CSkins::OnInit()");
	ASSERT_NE(SkinsInitStart, std::string::npos);
	const size_t SkinsInitEnd = SkinsSource.find("void CSkins::OnShutdown()", SkinsInitStart);
	ASSERT_NE(SkinsInitEnd, std::string::npos);
	const std::string SkinsInitBody = SkinsSource.substr(SkinsInitStart, SkinsInitEnd - SkinsInitStart);

	const std::string Skins7Source = ReadTestSourceFile("src/game/client/components/skins7.cpp");
	const size_t Skins7InitStart = Skins7Source.find("void CSkins7::OnInit()");
	ASSERT_NE(Skins7InitStart, std::string::npos);
	const size_t Skins7InitEnd = Skins7Source.find("void CSkins7::OnReset()", Skins7InitStart);
	ASSERT_NE(Skins7InitEnd, std::string::npos);
	const std::string Skins7InitBody = Skins7Source.substr(Skins7InitStart, Skins7InitEnd - Skins7InitStart);

	EXPECT_EQ(SkinsInitBody.find("RenderLoading("), std::string::npos);
	EXPECT_EQ(Skins7InitBody.find("RenderLoading("), std::string::npos);
	EXPECT_NE(SkinsInitBody.find("Refresh([]() {});"), std::string::npos);
	EXPECT_NE(Skins7InitBody.find("Refresh([]() {});"), std::string::npos);
}

TEST(QmClientStartup, InitialResetDoesNotCollectSnapshotEntitiesBeforeSnapshotRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t InvalidateStart = Source.find("void CGameClient::InvalidateSnapshot()");
	ASSERT_NE(InvalidateStart, std::string::npos);
	const size_t InvalidateEnd = Source.find("void CGameClient::OnNewSnapshot()", InvalidateStart);
	ASSERT_NE(InvalidateEnd, std::string::npos);
	const std::string Body = Source.substr(InvalidateStart, InvalidateEnd - InvalidateStart);

	EXPECT_NE(Body.find("m_vSnapEntities.clear();"), std::string::npos);
	EXPECT_NE(Body.find("m_pClient != nullptr"), std::string::npos);
	EXPECT_NE(Body.find("IClient::STATE_ONLINE"), std::string::npos);
	EXPECT_NE(Body.find("IClient::STATE_DEMOPLAYBACK"), std::string::npos);
	EXPECT_NE(Body.find("SnapCollectEntities();"), std::string::npos);
}

TEST(QmClientStartup, SoundResetDoesNotRequireComponentClientAccessorBeforeRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/sounds.cpp");
	const size_t ResetStart = Source.find("void CSounds::OnReset()");
	ASSERT_NE(ResetStart, std::string::npos);
	const size_t ResetEnd = Source.find("void CSounds::OnStateChange(", ResetStart);
	ASSERT_NE(ResetEnd, std::string::npos);
	const std::string Body = Source.substr(ResetStart, ResetEnd - ResetStart);

	EXPECT_EQ(Body.find("Client()->State()"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Client()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->ClientStateAtLeastOnline()"), std::string::npos);
	EXPECT_NE(Body.find("Sound()->StopAll();"), std::string::npos);
	EXPECT_NE(Body.find("ClearQueue();"), std::string::npos);

	const std::string GameClientHeader = ReadTestSourceFile("src/game/client/gameclient.h");
	EXPECT_NE(GameClientHeader.find("bool ClientStateAtLeastOnline() const;"), std::string::npos);
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	EXPECT_NE(GameClientSource.find("bool CGameClient::ClientStateAtLeastOnline() const"), std::string::npos);
}

TEST(QmClientStartup, VotingResetDoesNotRequireComponentClientAccessorBeforeRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/voting.cpp");
	const size_t ResetStart = Source.find("void CVoting::OnReset()");
	ASSERT_NE(ResetStart, std::string::npos);
	const size_t ResetEnd = Source.find("void CVoting::OnConsoleInit()", ResetStart);
	ASSERT_NE(ResetEnd, std::string::npos);
	const std::string Body = Source.substr(ResetStart, ResetEnd - ResetStart);

	EXPECT_EQ(Body.find("Client()->State()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->ClientStateOnline()"), std::string::npos);
	EXPECT_NE(Body.find("ClearUnfinishedMapVoteChain();"), std::string::npos);

	const std::string GameClientHeader = ReadTestSourceFile("src/game/client/gameclient.h");
	EXPECT_NE(GameClientHeader.find("bool ClientStateOnline() const;"), std::string::npos);
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	EXPECT_NE(GameClientSource.find("bool CGameClient::ClientStateOnline() const"), std::string::npos);
}

TEST(QmClientStartup, RaceDemoResetDoesNotRequireComponentClientAccessorBeforeRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/race_demo.cpp");
	const size_t ConstructorStart = Source.find("CRaceDemo::CRaceDemo() :");
	ASSERT_NE(ConstructorStart, std::string::npos);
	const size_t ConstructorEnd = Source.find("void CRaceDemo::GetPath(", ConstructorStart);
	ASSERT_NE(ConstructorEnd, std::string::npos);
	const std::string ConstructorBody = Source.substr(ConstructorStart, ConstructorEnd - ConstructorStart);
	EXPECT_NE(ConstructorBody.find("m_aTmpFilename[0] = '\\0';"), std::string::npos);

	const size_t StopStart = Source.find("void CRaceDemo::StopRecord(");
	ASSERT_NE(StopStart, std::string::npos);
	const size_t StopEnd = Source.find("struct SRaceDemoFetchUser", StopStart);
	ASSERT_NE(StopEnd, std::string::npos);
	const std::string Body = Source.substr(StopStart, StopEnd - StopStart);

	EXPECT_EQ(Body.find("Client()->RaceRecord_IsRecording()"), std::string::npos);
	EXPECT_EQ(Body.find("Client()->RaceRecord_Stop()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->StopRaceRecordIfRecording()"), std::string::npos);

	const std::string GameClientHeader = ReadTestSourceFile("src/game/client/gameclient.h");
	EXPECT_NE(GameClientHeader.find("void StopRaceRecordIfRecording() const;"), std::string::npos);
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	EXPECT_NE(GameClientSource.find("void CGameClient::StopRaceRecordIfRecording() const"), std::string::npos);
}

TEST(QmClientStartup, GhostResetDoesNotTouchMenusWhenNoRecordingExists)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/ghost.h");
	EXPECT_NE(Header.find("class IGhostLoader *m_pGhostLoader = nullptr;"), std::string::npos);
	EXPECT_NE(Header.find("class IGhostRecorder *m_pGhostRecorder = nullptr;"), std::string::npos);
	EXPECT_NE(Header.find("char m_aTmpFilename[IO_MAX_PATH_LENGTH] = \"\";"), std::string::npos);

	const std::string Source = ReadTestSourceFile("src/game/client/components/ghost.cpp");
	const size_t StopStart = Source.find("void CGhost::StopRecord(");
	ASSERT_NE(StopStart, std::string::npos);
	const size_t StopEnd = Source.find("void CGhost::StartRender(", StopStart);
	ASSERT_NE(StopEnd, std::string::npos);
	const std::string Body = Source.substr(StopStart, StopEnd - StopStart);

	const size_t EmptyGuardPos = Body.find("if(!WasRecording && !RecordingToFile && m_aTmpFilename[0] == '\\0')");
	const size_t OwnGhostPos = Body.find("GetOwnGhost()");
	ASSERT_NE(EmptyGuardPos, std::string::npos);
	ASSERT_NE(OwnGhostPos, std::string::npos);
	EXPECT_LT(EmptyGuardPos, OwnGhostPos);
	EXPECT_NE(Body.find("CMenus::CGhostItem *pOwnGhost = nullptr;"), std::string::npos);
	EXPECT_NE(Body.find("if(Time > 0)"), std::string::npos);
}

TEST(QmClientStartup, CoreInterfacePointersAreNullInitializedBeforeConsoleInit)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.h");
	const size_t FieldsStart = Source.find("class IEngine *m_pEngine");
	ASSERT_NE(FieldsStart, std::string::npos);
	const size_t FieldsEnd = Source.find("CLayers m_Layers;", FieldsStart);
	ASSERT_NE(FieldsEnd, std::string::npos);
	const std::string Fields = Source.substr(FieldsStart, FieldsEnd - FieldsStart);

	EXPECT_NE(Fields.find("class IClient *m_pClient = nullptr;"), std::string::npos);
	EXPECT_NE(Fields.find("class IConsole *m_pConsole = nullptr;"), std::string::npos);
	EXPECT_NE(Fields.find("class IStorage *m_pStorage = nullptr;"), std::string::npos);
	EXPECT_NE(Fields.find("class ISound *m_pSound = nullptr;"), std::string::npos);
	EXPECT_NE(Fields.find("class ITextRender *m_pTextRender = nullptr;"), std::string::npos);
}

TEST(QmClientStartup, InitialResetDoesNotRequireEditorAccessorBeforeRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t ResetStart = Source.find("void CGameClient::OnReset()");
	ASSERT_NE(ResetStart, std::string::npos);
	const size_t ResetEnd = Source.find("bool CGameClient::LivePresentationUsesLiveObserverOverlay() const", ResetStart);
	ASSERT_NE(ResetEnd, std::string::npos);
	const std::string Body = Source.substr(ResetStart, ResetEnd - ResetStart);

	EXPECT_EQ(Body.find("Editor()->ResetMentions()"), std::string::npos);
	EXPECT_EQ(Body.find("Editor()->ResetIngameMoved()"), std::string::npos);
	EXPECT_NE(Body.find("if(m_pEditor != nullptr)"), std::string::npos);
	EXPECT_NE(Body.find("m_pEditor->ResetMentions();"), std::string::npos);
	EXPECT_NE(Body.find("m_pEditor->ResetIngameMoved();"), std::string::npos);
}

TEST(QmClientStartup, LoadingSettingsTextPrewarmDoesNotRequireComponentClientAccessor)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const size_t PrewarmStart = Source.find("int CMenus::PrebuildSettingsTextPoolForLoading(");
	ASSERT_NE(PrewarmStart, std::string::npos);
	const size_t PrewarmEnd = Source.find("void CMenus::LogSettingsAdaptiveBudget(", PrewarmStart);
	ASSERT_NE(PrewarmEnd, std::string::npos);
	const std::string Body = Source.substr(PrewarmStart, PrewarmEnd - PrewarmStart);

	EXPECT_EQ(Body.find("Client()->PerfFrame()"), std::string::npos);
	EXPECT_EQ(Body.find("CurrentQmUiPerfPage()"), std::string::npos);
	EXPECT_EQ(Body.find("SettingsPerfContextName()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->PerfFrameOrOne()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->ClientStateOnline() ? \"online\" : \"offline\""), std::string::npos);

	const std::string GameClientHeader = ReadTestSourceFile("src/game/client/gameclient.h");
	EXPECT_NE(GameClientHeader.find("uint64_t PerfFrameOrOne() const"), std::string::npos);
}

TEST(QmClientStartup, RenderOnlyMenuTabsDoNotResolveAnimationRuntime)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const size_t TabStart = Source.find("int CMenus::DoMenuTabV2(");
	ASSERT_NE(TabStart, std::string::npos);
	const size_t TabEnd = Source.find("void CMenus::RenderMenubar(", TabStart);
	ASSERT_NE(TabEnd, std::string::npos);
	const std::string Body = Source.substr(TabStart, TabEnd - TabStart);

	const size_t RenderOnlyGuard = Body.find("if(!Ui()->RenderOnly())");
	const size_t AnimationResolve = Body.find("ResolveUiAnimValueColor(");
	ASSERT_NE(RenderOnlyGuard, std::string::npos);
	ASSERT_NE(AnimationResolve, std::string::npos);
	EXPECT_LT(RenderOnlyGuard, AnimationResolve);

	const size_t MenubarStart = Source.find("void CMenus::RenderMenubar(");
	ASSERT_NE(MenubarStart, std::string::npos);
	const size_t MenubarEnd = Source.find("void CMenus::RenderNews(", MenubarStart);
	ASSERT_NE(MenubarEnd, std::string::npos);
	const std::string MenubarBody = Source.substr(MenubarStart, MenubarEnd - MenubarStart);
	const size_t IndicatorGuard = MenubarBody.find("if(UseNewUi && MenubarHaveActive && !Ui()->RenderOnly())");
	const size_t IndicatorResolve = MenubarBody.find("ResolveUiAnimValueRect(");
	ASSERT_NE(IndicatorGuard, std::string::npos);
	ASSERT_NE(IndicatorResolve, std::string::npos);
	EXPECT_LT(IndicatorGuard, IndicatorResolve);
}

TEST(QmClientStartup, StartupSettingsPrewarmDoesNotCollectIngameEscPlan)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const size_t CollectionStart = Source.find("void CMenus::PrepareSettingsMenuTextPlanCollectionUnits(const char *pOperationOverride)");
	ASSERT_NE(CollectionStart, std::string::npos);
	const size_t CollectionEnd = Source.find("void CMenus::CollectSettingsMenuTextPlanUnit(", CollectionStart);
	ASSERT_NE(CollectionEnd, std::string::npos);
	const std::string Body = Source.substr(CollectionStart, CollectionEnd - CollectionStart);

	const size_t OperationFlag = Body.find("const bool IngameEscOperation = str_comp(pOperation, \"ingame_esc_open\") == 0;");
	const size_t IngameUnit = Body.find("m_vSettingsMenuTextPlanCollectionUnits.push_back({MENU_TEXT_PLAN_UNIT_INGAME_ESC, -1, -1});");
	ASSERT_NE(OperationFlag, std::string::npos);
	ASSERT_NE(IngameUnit, std::string::npos);
	EXPECT_LT(OperationFlag, IngameUnit);
	EXPECT_NE(Body.find("if(IngameEscOperation)"), std::string::npos);
}

TEST(QmClientStartup, SettingsTextPlanWrappersDoNotCopyInactivePendingItem)
{
	const std::string MenusSource = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const std::string TClientSource = ReadTestSourceFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string QmClientSource = ReadTestSourceFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	const std::string DirectCopy = "const SMenuTextPlanItem PreviousPendingItem = m_MenuTextPlanPendingItem;";
	EXPECT_EQ(MenusSource.find(DirectCopy), std::string::npos);
	EXPECT_EQ(TClientSource.find(DirectCopy), std::string::npos);
	EXPECT_EQ(QmClientSource.find(DirectCopy), std::string::npos);

	EXPECT_NE(MenusSource.find("if(PreviousPendingActive)\n\t\tPreviousPendingItem = m_MenuTextPlanPendingItem;"), std::string::npos);
	EXPECT_NE(TClientSource.find("if(PreviousPendingActive)\n\t\tPreviousPendingItem = m_MenuTextPlanPendingItem;"), std::string::npos);
	EXPECT_NE(QmClientSource.find("if(PreviousPendingActive)\n\t\tPreviousPendingItem = m_MenuTextPlanPendingItem;"), std::string::npos);
}

TEST(QmLiveMatchReplay, StartKeepsOrdinaryDDNetRecordingCompatible)
{
	const std::string Source = ReadTestSourceFile("src/game/client/live/live_match_replay.cpp");
	const size_t StartPos = Source.find("bool CLiveMatchReplay::Start(CGameClient *pGameClient)");
	ASSERT_NE(StartPos, std::string::npos);
	const size_t StopPos = Source.find("bool CLiveMatchReplay::Stop(CGameClient *pGameClient, bool WriteSidecarFile)", StartPos);
	ASSERT_NE(StopPos, std::string::npos);
	const std::string Body = Source.substr(StartPos, StopPos - StartPos);

	EXPECT_EQ(Body.find("QmLiveObserverActive"), std::string::npos);
	EXPECT_NE(Body.find("DemoRecorder_Start"), std::string::npos);
	EXPECT_NE(Source.find("demos/qm_live/matches"), std::string::npos);
}

TEST(QmLiveDirector, SelectRandomPlayerUsesStableModulo)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[3] = TEAM_FLOCK;
	aTeams[8] = TEAM_SUPER;
	aActivePlayers[3] = true;
	aActivePlayers[8] = true;

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_EQ(Director.SelectRandomPlayer(0), 3);
	EXPECT_EQ(Director.SelectRandomPlayer(1), 8);
	EXPECT_EQ(Director.SelectRandomPlayer(2), 3);
	EXPECT_EQ(Director.SelectRandomTeam(0), -1);
}

TEST(QmLiveDirector, EmptyDataHasNoEntries)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	Director.UpdateEntries(aTeams, aActivePlayers);

	EXPECT_TRUE(Director.Entries().empty());
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.SelectRandomTeam(0), -1);
	EXPECT_EQ(Director.SelectRandomPlayer(0), -1);
	EXPECT_EQ(Director.FallbackPlayer(), -1);
}

TEST(QmLiveDirector, ResetClearsState)
{
	CLiveDirector Director;
	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	std::array<bool, MAX_CLIENTS> aActivePlayers = DefaultActivePlayers();

	aTeams[0] = 2;
	aActivePlayers[0] = true;
	Director.UpdateEntries(aTeams, aActivePlayers);
	Director.SetMode(CLiveObserverSession::EDirectorMode::FOLLOW_TEAM);

	Director.Reset();

	EXPECT_TRUE(Director.Entries().empty());
	EXPECT_FALSE(Director.HasDDRaceTeams());
	EXPECT_EQ(Director.FallbackPlayer(), -1);
	EXPECT_EQ(Director.Mode(), CLiveObserverSession::EDirectorMode::FREEVIEW);
}

TEST(QmLiveReplayBuffer, KeepsLatestFramesAndCopiesData)
{
	CLiveReplayBuffer Buffer;
	Buffer.SetMaxFrames(2);

	uint8_t aFrame[] = {1, 2, 3};
	Buffer.PushSnapshot(10, aFrame, sizeof(aFrame));
	aFrame[0] = 9;
	Buffer.PushSnapshot(11, aFrame, sizeof(aFrame));
	Buffer.PushSnapshot(12, aFrame, sizeof(aFrame));

	ASSERT_EQ(Buffer.Frames().size(), 2u);
	EXPECT_EQ(Buffer.Frames()[0].m_Tick, 11);
	EXPECT_EQ(Buffer.Frames()[1].m_Tick, 12);
	ASSERT_EQ(Buffer.Frames()[0].m_vData.size(), 3u);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[0], 9);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[1], 2);
	EXPECT_EQ(Buffer.Frames()[0].m_vData[2], 3);
}

TEST(QmLiveReplayBuffer, RejectsEmptyFramesAndHonorsDisabledBuffer)
{
	CLiveReplayBuffer Buffer;
	const uint8_t aFrame[] = {1};

	Buffer.PushSnapshot(1, aFrame, sizeof(aFrame));
	EXPECT_TRUE(Buffer.Frames().empty());

	Buffer.SetMaxFrames(2);
	Buffer.PushSnapshot(2, nullptr, sizeof(aFrame));
	Buffer.PushSnapshot(3, aFrame, 0);
	EXPECT_TRUE(Buffer.Frames().empty());
}

TEST(QmLiveTeamRenderFilter, AcceptsOnlyRegularDDRaceTeams)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();

	EXPECT_FALSE(Filter.SetTeam(TEAM_FLOCK));
	EXPECT_FALSE(Filter.SetTeam(TEAM_SUPER));
	EXPECT_FALSE(Filter.SetTeam(-1));
	ASSERT_TRUE(Filter.SetTeam(4));

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	aTeams[2] = 4;
	aTeams[3] = 5;
	Filter.UpdateTeams(aTeams);

	EXPECT_TRUE(Filter.AllowsClient(2));
	EXPECT_FALSE(Filter.AllowsClient(3));
	EXPECT_FALSE(Filter.AllowsTeam(TEAM_FLOCK));
	EXPECT_FALSE(Filter.AllowsTeam(TEAM_SUPER));
}

TEST(QmLiveTeamRenderFilter, StrictUnknownEventsDefaultRejects)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();
	ASSERT_TRUE(Filter.SetTeam(2));

	EXPECT_FALSE(Filter.AllowsUnknownPlayerEvent());
	Filter.SetStrictUnknownEvents(false);
	EXPECT_TRUE(Filter.AllowsUnknownPlayerEvent());
}

TEST(QmLiveTeamRenderFilter, ObservesPlaybackSeekAndRestart)
{
	CLiveTeamRenderFilter Filter;
	Filter.Reset();
	ASSERT_TRUE(Filter.SetTeam(3));
	const int InitialSerial = Filter.ResetSerial();

	EXPECT_FALSE(Filter.ObservePlaybackTick(100));
	EXPECT_FALSE(Filter.ObservePlaybackTick(101));
	EXPECT_TRUE(Filter.ObservePlaybackTick(50));
	EXPECT_GT(Filter.ResetSerial(), InitialSerial);
}

TEST(QmLiveReplaySidecar, BuildsParsesAndDeduplicatesEvents)
{
	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	Sidecar.SetEndTick(90);

	EXPECT_TRUE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	EXPECT_FALSE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	EXPECT_TRUE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));
	EXPECT_FALSE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));

	const std::string Json = Sidecar.BuildJson();
	EXPECT_NE(Json.find("\"time_ms\""), std::string::npos);
	SLiveReplaySidecarData Parsed;
	char aError[128];
	ASSERT_TRUE(CLiveReplaySidecar::LoadFromString(Json.c_str(), Parsed, aError, sizeof(aError))) << aError;

	EXPECT_EQ(Parsed.m_FormatVersion, SLiveReplaySidecarData::FORMAT_VERSION);
	EXPECT_STREQ(Parsed.m_aDemoFilename, "demos/live/Map_match.demo");
	EXPECT_STREQ(Parsed.m_aMapName, "Map");
	EXPECT_EQ(Parsed.m_MapCrc, 1234u);
	EXPECT_EQ(Parsed.m_StartTick, 10);
	EXPECT_EQ(Parsed.m_EndTick, 90);
	ASSERT_EQ(Parsed.m_vFinishEvents.size(), 1u);
	EXPECT_EQ(Parsed.m_vFinishEvents[0].m_Team, 4);
	ASSERT_EQ(Parsed.m_vTeamEvents.size(), 1u);
	EXPECT_EQ(Parsed.m_vTeamEvents[0].m_NewTeam, 4);
}

TEST(QmLiveReplaySidecar, RejectsDamagedAndMismatchedSidecars)
{
	SLiveReplaySidecarData Parsed;
	char aError[128];
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString("{\"format_version\":1,\"finish_events\":[", Parsed, aError, sizeof(aError)));

	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	const std::string Json = Sidecar.BuildJson();
	ASSERT_TRUE(CLiveReplaySidecar::LoadFromString(Json.c_str(), Parsed, aError, sizeof(aError)));

	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_TRUE(CLiveReplaySidecar::MatchesDemo(Parsed, "archive/Map_match.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Other.demo", "Map", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Other", SHA256_DIGEST{}, 1234));
	EXPECT_FALSE(CLiveReplaySidecar::MatchesDemo(Parsed, "demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 4321));
}

TEST(QmLiveReplaySidecar, RejectsInvalidTimelineFields)
{
	CLiveReplaySidecar Sidecar;
	Sidecar.Start("demos/live/Map_match.demo", "Map", SHA256_DIGEST{}, 1234, 10);
	Sidecar.SetEndTick(90);
	ASSERT_TRUE(Sidecar.AddFinishEvent(80, 4, 151420, 7));
	ASSERT_TRUE(Sidecar.AddTeamEvent(20, 7, TEAM_FLOCK, 4));
	const std::string Json = Sidecar.BuildJson();

	SLiveReplaySidecarData Parsed;
	char aError[128];

	std::string InvalidRecording = Json;
	size_t Pos = InvalidRecording.find("\"end_tick\": 90");
	ASSERT_NE(Pos, std::string::npos);
	InvalidRecording.replace(Pos, std::string("\"end_tick\": 90").size(), "\"end_tick\": 9");
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidRecording.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid recording tick range");

	std::string InvalidFinishTeam = Json;
	Pos = InvalidFinishTeam.find("\"team\": 4");
	ASSERT_NE(Pos, std::string::npos);
	InvalidFinishTeam.replace(Pos, std::string("\"team\": 4").size(), "\"team\": 0");
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidFinishTeam.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid finish event");

	std::string InvalidTeamClient = Json;
	const size_t TeamEventsPos = InvalidTeamClient.find("\"team_events\"");
	ASSERT_NE(TeamEventsPos, std::string::npos);
	Pos = InvalidTeamClient.find("\"client_id\": 7", TeamEventsPos);
	ASSERT_NE(Pos, std::string::npos);
	const std::string InvalidClient = "\"client_id\": " + std::to_string(MAX_CLIENTS);
	InvalidTeamClient.replace(Pos, std::string("\"client_id\": 7").size(), InvalidClient);
	EXPECT_FALSE(CLiveReplaySidecar::LoadFromString(InvalidTeamClient.c_str(), Parsed, aError, sizeof(aError)));
	EXPECT_STREQ(aError, "invalid team event");
}

TEST(QmLiveFinishRanking, ResolvesPendingFinishTeamAttribution)
{
	CLiveFinishRanking Ranking;
	CLiveFinishRanking::CResult Pending = Ranking.OnFinishMessage(5, 151420, 100, false, -1);
	EXPECT_EQ(Pending.m_Status, CLiveFinishRanking::EFinishStatus::PENDING);
	EXPECT_TRUE(Ranking.Events().empty());

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	aTeams[5] = 4;
	const CLiveFinishRanking::CResolveResult Resolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 101);

	ASSERT_EQ(Resolved.m_vAccepted.size(), 1u);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_Team, 4);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_ClientId, 5);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_TimeMs, 151420);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Rank, 1);
}

TEST(QmLiveFinishRanking, KeepsPendingFinishUntilTeamBecomesReliable)
{
	CLiveFinishRanking Ranking;
	Ranking.OnFinishMessage(5, 151420, 100, false, -1);

	std::array<int, MAX_CLIENTS> aTeams = DefaultTeams();
	CLiveFinishRanking::CResolveResult Unresolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 101);
	EXPECT_TRUE(Unresolved.m_vAccepted.empty());
	EXPECT_EQ(Unresolved.m_DroppedPending, 0);

	aTeams[5] = 4;
	CLiveFinishRanking::CResolveResult Resolved = Ranking.ResolvePending(aTeams.data(), aTeams.size(), 102);
	ASSERT_EQ(Resolved.m_vAccepted.size(), 1u);
	EXPECT_EQ(Resolved.m_vAccepted[0].m_Event.m_Team, 4);
}

TEST(QmLiveFinishRanking, IgnoresInvalidTeamsAfterTeamStateIsKnown)
{
	CLiveFinishRanking Ranking;

	EXPECT_EQ(Ranking.OnFinishMessage(1, 1000, 10, true, TEAM_FLOCK).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_EQ(Ranking.OnFinishMessage(2, 1000, 11, true, TEAM_SUPER).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_EQ(Ranking.OnFinishMessage(MAX_CLIENTS, 1000, 12, true, 3).m_Status, CLiveFinishRanking::EFinishStatus::IGNORED);
	EXPECT_TRUE(Ranking.Events().empty());
}

TEST(QmLiveFinishRanking, DeduplicatesSameTeamFinishMessages)
{
	CLiveFinishRanking Ranking;

	EXPECT_EQ(Ranking.OnFinishMessage(1, 90000, 200, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::ACCEPTED);
	EXPECT_EQ(Ranking.OnFinishMessage(2, 90000, 201, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::DUPLICATE);
	EXPECT_EQ(Ranking.OnFinishMessage(3, 91000, 260, true, 7).m_Status, CLiveFinishRanking::EFinishStatus::DUPLICATE);

	ASSERT_EQ(Ranking.Events().size(), 1u);
	EXPECT_EQ(Ranking.Events()[0].m_Team, 7);
	EXPECT_EQ(Ranking.Events()[0].m_ClientId, 1);
}

TEST(QmLiveFinishRanking, SortsFinishedTeamsByOfficialTime)
{
	CLiveFinishRanking Ranking;

	Ranking.OnFinishMessage(1, 200000, 50, true, 2);
	Ranking.OnFinishMessage(2, 100000, 55, true, 3);

	EXPECT_EQ(Ranking.RankForTeam(3, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
}

TEST(QmLiveFinishRanking, UsesStableTieBreakersForEqualFinishTimes)
{
	CLiveFinishRanking Ranking;

	Ranking.OnFinishMessage(2, 100000, 10, true, 2);
	Ranking.OnFinishMessage(1, 100000, 10, true, 1);
	Ranking.OnFinishMessage(3, 100000, 20, true, 3);

	EXPECT_EQ(Ranking.RankForTeam(1, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
	EXPECT_EQ(Ranking.RankForTeam(3, false), 3);
}

TEST(QmLiveFinishRanking, FiltersConfiguredTeamRangeWithoutChangingInternalLog)
{
	CLiveFinishRanking Ranking;
	Ranking.SetTeamRange(1, 2);

	Ranking.OnFinishMessage(5, 80000, 10, true, 5);
	Ranking.OnFinishMessage(2, 120000, 20, true, 2);

	ASSERT_EQ(Ranking.Events().size(), 2u);
	EXPECT_FALSE(Ranking.IsTeamInConfiguredRange(5));
	EXPECT_EQ(Ranking.RankForTeam(5, false), 0);
	EXPECT_EQ(Ranking.RankForTeam(2, false), 2);
	EXPECT_EQ(Ranking.RankForTeam(5, true), 1);
}

TEST(QmLiveFinishRanking, ResetClearsEventsPendingAndCards)
{
	CLiveFinishRanking Ranking;
	Ranking.OnFinishMessage(1, 1000, 10, false, -1);
	const CLiveFinishRanking::CResult Accepted = Ranking.OnFinishMessage(2, 2000, 11, true, 2);
	Ranking.EnqueueCard(Accepted.m_Event, 1, 11);
	ASSERT_NE(Ranking.VisibleCard(11, 50), nullptr);

	Ranking.Reset();

	EXPECT_TRUE(Ranking.Events().empty());
	EXPECT_EQ(Ranking.VisibleCard(12, 50), nullptr);
}

TEST(QmLiveFinishRanking, SidecarMissingAndDamagedInputFailSafely)
{
	std::vector<CLiveFinishEvent> vEvents;
	EXPECT_FALSE(CLiveFinishRanking::ParseSidecarJson(nullptr, 0, vEvents));
	EXPECT_FALSE(CLiveFinishRanking::ParseSidecarJson("{\"finish_events\":[", 18, vEvents));
	EXPECT_TRUE(vEvents.empty());
}

TEST(QmLiveFinishRanking, SidecarJsonUsesLiveReplayTimeFieldAndAcceptsLegacyTimeMs)
{
	std::vector<CLiveFinishEvent> vSource;
	vSource.push_back({2, 1, 12345, 67});
	const std::string Json = CLiveFinishRanking::EventsToSidecarJson(vSource);
	EXPECT_NE(Json.find("\"time\""), std::string::npos);
	EXPECT_EQ(Json.find("time_ms"), std::string::npos);

	std::vector<CLiveFinishEvent> vEvents;
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(Json.c_str(), Json.size(), vEvents));
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_TimeMs, 12345);

	const char *pLegacyJson = "{\"finish_events\":[{\"team\":2,\"client_id\":1,\"time_ms\":12345,\"tick\":67}]}";
	vEvents.clear();
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(pLegacyJson, str_length(pLegacyJson), vEvents));
	ASSERT_EQ(vEvents.size(), 1u);
	EXPECT_EQ(vEvents[0].m_TimeMs, 12345);
}

TEST(QmLiveFinishRanking, SidecarRebuildUsesOnlyEventsAtOrBeforeCurrentTick)
{
	const char *pJson = "{\"version\":1,\"finish_events\":[{\"team\":3,\"client_id\":3,\"time\":80000,\"tick\":90},{\"team\":4,\"client_id\":4,\"time\":70000,\"tick\":150}]}";
	std::vector<CLiveFinishEvent> vEvents;
	ASSERT_TRUE(CLiveFinishRanking::ParseSidecarJson(pJson, str_length(pJson), vEvents));

	CLiveFinishRanking Ranking;
	EXPECT_TRUE(Ranking.RebuildFromEvents(vEvents, 100));

	ASSERT_EQ(Ranking.Events().size(), 1u);
	EXPECT_EQ(Ranking.Events()[0].m_Team, 3);
	EXPECT_EQ(Ranking.RankForTeam(3, false), 1);
	EXPECT_EQ(Ranking.RankForTeam(4, false), 0);
}
