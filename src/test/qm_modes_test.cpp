#include <base/color.h>

#include <engine/shared/config.h>

#include <game/client/components/qmclient/modes.h>

#include <gtest/gtest.h>

static void ExpectColorNear(const ColorRGBA &Color, const ColorRGBA &Expected)
{
	EXPECT_NEAR(Color.r, Expected.r, 0.02f);
	EXPECT_NEAR(Color.g, Expected.g, 0.02f);
	EXPECT_NEAR(Color.b, Expected.b, 0.02f);
	EXPECT_NEAR(Color.a, Expected.a, 0.02f);
}

TEST(QmGoresMode, ManualGuideRevealOverridesAutomaticGuideHiding)
{
	EXPECT_TRUE(ShouldHideGoresGuide(true, true, false));
	EXPECT_FALSE(ShouldHideGoresGuide(true, true, true));
	EXPECT_FALSE(ShouldHideGoresGuide(true, false, false));
	EXPECT_FALSE(ShouldHideGoresGuide(false, true, false));
}

TEST(QmGoresMode, DebugRouteDoesNotUseHideGuidesGate)
{
	EXPECT_TRUE(ShouldRenderGoresDebugRoute(true, true, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(false, true, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(true, false, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(true, true, false));
}

TEST(QmGoresMode, LinkedFastInputDirectlyFollowsGoresMode)
{
	bool Changed = false;
	EXPECT_TRUE(ApplyQmGoresLinkedConfig(true, true, false, Changed));
	EXPECT_TRUE(Changed);

	EXPECT_FALSE(ApplyQmGoresLinkedConfig(false, true, true, Changed));
	EXPECT_TRUE(Changed);

	EXPECT_TRUE(ApplyQmGoresLinkedConfig(true, true, true, Changed));
	EXPECT_FALSE(Changed);
}

TEST(QmGoresMode, UnlinkedFastInputConfigIsNotChanged)
{
	bool Changed = false;
	EXPECT_TRUE(ApplyQmGoresLinkedConfig(false, false, true, Changed));
	EXPECT_FALSE(Changed);

	EXPECT_FALSE(ApplyQmGoresLinkedConfig(true, false, false, Changed));
	EXPECT_FALSE(Changed);
}

TEST(QmGoresMode, ActiveGoresClearsDummyHammerState)
{
	bool Changed = false;
	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(true, 1, Changed), 0);
	EXPECT_TRUE(Changed);

	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(true, 0, Changed), 0);
	EXPECT_FALSE(Changed);

	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(false, 1, Changed), 1);
	EXPECT_FALSE(Changed);
}

TEST(QmFocusMode, ConfigOverrideRestoresOnlyAutoHiddenValues)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	int Value = ApplyQmFocusConfigOverride(State, true, 1, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 0);
	EXPECT_TRUE(State.m_WasActive);
	EXPECT_EQ(State.m_SavedValue, 1);

	Value = ApplyQmFocusConfigOverride(State, false, 0, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 1);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, ConfigOverrideKeepsUserChangesMadeWhileActive)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	EXPECT_EQ(ApplyQmFocusConfigOverride(State, true, 1, 0, Changed), 0);
	EXPECT_TRUE(Changed);

	const int UserChangedValue = 2;
	EXPECT_EQ(ApplyQmFocusConfigOverride(State, false, UserChangedValue, 0, Changed), UserChangedValue);
	EXPECT_FALSE(Changed);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, HudScoreboardNamesAndNameplatesRequireFocusModeAndTheirOwnToggle)
{
	EXPECT_TRUE(ShouldHideFocusHud(true, true));
	EXPECT_FALSE(ShouldHideFocusHud(true, false));
	EXPECT_FALSE(ShouldHideFocusHud(false, true));

	EXPECT_TRUE(ShouldHideFocusScoreboard(true, true));
	EXPECT_FALSE(ShouldHideFocusScoreboard(true, false));
	EXPECT_FALSE(ShouldHideFocusScoreboard(false, true));

	EXPECT_TRUE(ShouldHideFocusNames(true, true));
	EXPECT_FALSE(ShouldHideFocusNames(true, false));
	EXPECT_FALSE(ShouldHideFocusNames(false, true));

	EXPECT_TRUE(ShouldHideFocusNameplates(true, true));
	EXPECT_FALSE(ShouldHideFocusNameplates(true, false));
	EXPECT_FALSE(ShouldHideFocusNameplates(false, true));
}

TEST(QmFocusMode, SpectatorHudStaysVisibleWhenFocusModeAutoHidesMainHud)
{
	EXPECT_TRUE(ShouldRenderFocusSpectatorHud(true, true, false, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(false, true, false, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, false, false, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, true, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, false, false, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, false, true, false));
}

TEST(QmFocusMode, VisualEffectChildrenDoNotInheritTheLegacyVisualParentToggle)
{
	EXPECT_FALSE(ShouldHideFocusJumpEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusJumpEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusKillEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusKillEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusExplosionEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusExplosionEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusFreezeEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusFreezeEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusFreezeEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusHammerEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusHammerEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusHammerEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusMuzzleEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusMuzzleEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusMuzzleEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusJumpEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusKillEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusExplosionEffects(false, true));
}

TEST(QmFocusMode, UncheckedJumpEffectsStayVisibleInFocusMode)
{
	EXPECT_FALSE(ShouldHideFocusJumpEffects(true, false));
}

TEST(QmFocusMode, MapProgressAndInfoMessagesUseTheirOwnChildToggles)
{
	EXPECT_FALSE(ShouldHideFocusMapProgress(true, false));
	EXPECT_TRUE(ShouldHideFocusMapProgress(true, true));
	EXPECT_FALSE(ShouldHideFocusInfoMessages(true, false));
	EXPECT_TRUE(ShouldHideFocusInfoMessages(true, true));
	EXPECT_FALSE(ShouldHideFocusMapProgress(false, true));
	EXPECT_FALSE(ShouldHideFocusInfoMessages(false, true));
}

TEST(QmFocusMode, IndependentMapProgressUsesItsOwnToggleAndBottomStyle)
{
	EXPECT_FALSE(ShouldRenderMapProgressBar(false, 0, false, true));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 1, false, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 1, true, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 0, false, false));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 0, false, true));
}

TEST(QmFocusMode, JumpSoundMuteIsIndependentFromJumpVisualEffects)
{
	EXPECT_TRUE(ShouldPlayFocusJumpSound(true, false, true));
	EXPECT_FALSE(ShouldPlayFocusJumpSound(true, true, true));
	EXPECT_TRUE(ShouldPlayFocusJumpSound(false, true, true));
	EXPECT_FALSE(ShouldPlayFocusJumpSound(true, false, false));
}

TEST(QmFocusMode, DeathOrSpawnSoundUsesDeathSoundMuteToggle)
{
	EXPECT_TRUE(ShouldPlayFocusDeathOrSpawnSound(true, false, true));
	EXPECT_FALSE(ShouldPlayFocusDeathOrSpawnSound(true, true, true));
	EXPECT_TRUE(ShouldPlayFocusDeathOrSpawnSound(false, true, true));
	EXPECT_FALSE(ShouldPlayFocusDeathOrSpawnSound(true, false, false));
}

TEST(QmFocusMode, HammerSoundMuteRequiresFocusModeAndHammerSoundToggle)
{
	EXPECT_FALSE(ShouldMuteFocusHammerSounds(true, false));
	EXPECT_TRUE(ShouldMuteFocusHammerSounds(true, true));
	EXPECT_FALSE(ShouldMuteFocusHammerSounds(false, true));
}

TEST(QmFocusMode, AirJumpDecisionSeparatesParticlesAndSound)
{
	SQmAirJumpEffectDecision Decision = GetQmAirJumpEffectDecision(true, false, true, true);
	EXPECT_TRUE(Decision.m_SpawnParticles);
	EXPECT_FALSE(Decision.m_PlaySound);

	Decision = GetQmAirJumpEffectDecision(true, true, false, true);
	EXPECT_FALSE(Decision.m_SpawnParticles);
	EXPECT_TRUE(Decision.m_PlaySound);

	Decision = GetQmAirJumpEffectDecision(true, false, false, false);
	EXPECT_TRUE(Decision.m_SpawnParticles);
	EXPECT_FALSE(Decision.m_PlaySound);
}

TEST(QmFocusMode, DirectionIndicatorsAndGuideLinesAreControlledSeparately)
{
	EXPECT_TRUE(ShouldHideFocusDirectionIndicators(true, true));
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(true, false));
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(false, true));

	EXPECT_TRUE(ShouldHideFocusGuideLines(true, true));
	EXPECT_FALSE(ShouldHideFocusGuideLines(true, false));
	EXPECT_FALSE(ShouldHideFocusGuideLines(false, true));
}

TEST(QmFocusMode, UncheckedDirectionAndGuideIndicatorsStayVisibleInFocusMode)
{
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(true, false));
	EXPECT_FALSE(ShouldHideFocusGuideLines(true, false));
}

TEST(QmFocusMode, ForceVisibleClientLinesRemainVisibleWhenChatIsHidden)
{
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, true, true, -2, true, false));
	EXPECT_FALSE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, true));
}

TEST(QmFocusMode, ChatFiltersSeparatePlayerSystemAndEchoMessages)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(true, false, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -2, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -1, false, false));
}

TEST(QmFocusMode, UnknownChatLinesFollowSystemMessageVisibility)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, false, true, -3, false, false));
}

TEST(QmFocusMode, ChatAreaRendersWhenAnyMessageClassIsVisible)
{
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(false, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, false, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, false, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, false, false));
}

TEST(QmFocusMode, ConfigSnapshotKeepsExplicitVisualChildrenIndependent)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
	EXPECT_FALSE(Decisions.m_HideKillEffects);
	EXPECT_FALSE(Decisions.m_HideExplosionEffects);
	EXPECT_FALSE(Decisions.m_HideFreezeEffects);
	EXPECT_FALSE(Decisions.m_HideHammerEffects);
	EXPECT_FALSE(Decisions.m_HideMuzzleEffects);

	Config.m_HideMuzzleEffects = true;
	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_HideMuzzleEffects);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesNameTextFromWholeNameplate)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HideNames = true;

	SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_FALSE(Decisions.m_HideNameplates);

	Config.m_HideNames = false;
	Config.m_HideNameplates = true;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesChatMessageClasses)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HidePlayerMessages = true;
	Config.m_HideSystemInfoMessages = false;
	Config.m_HideSystemPromptMessages = true;
	Config.m_HideEchoMessages = true;
	Config.m_HideHud = true;
	Config.m_HideScoreboard = true;
	Config.m_HideNames = true;
	Config.m_HideNameplates = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideHud);
	EXPECT_TRUE(Decisions.m_HideScoreboard);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, 0, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, true));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, false));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -2, false, false));
}

TEST(QmFocusMode, ConfigSnapshotMapProgressRequiresStyleAndGoresProgressAndChildToggle)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_MapProgressEnabled = true;
	Config.m_MapProgressStyle = 0;
	Config.m_PlayerStatsHudEnabled = false;
	Config.m_GoresMapProgressEnabled = true;
	Config.m_HideMapProgress = false;

	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_MapProgressStyle = 1;
	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_PlayerStatsHudEnabled = true;
	EXPECT_FALSE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_MapProgressStyle = 0;
	Config.m_PlayerStatsHudEnabled = false;
	Config.m_HideMapProgress = true;
	EXPECT_FALSE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);
}

TEST(QmTranslateUiSettings, DefaultColorsMatchSettingsPreviewDefaults)
{
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(CConfig::ms_QmTranslateBtnColorDisabled, true)), ColorRGBA(0.16f, 0.16f, 0.16f, 0.82f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(CConfig::ms_QmTranslateBtnColorEnabled, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(CConfig::ms_QmTranslateMenuBgColor, true)), ColorRGBA(0.12f, 0.12f, 0.12f, 0.95f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(CConfig::ms_QmTranslateMenuOptionSelected, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(CConfig::ms_QmTranslateMenuOptionNormal, true)), ColorRGBA(0.20f, 0.20f, 0.20f, 0.90f));
}
