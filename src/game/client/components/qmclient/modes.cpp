#include "modes.h"

#include <algorithm>

int ApplyQmFocusConfigOverride(SQmFocusConfigOverrideState &State, bool HideActive, int CurrentValue, int HiddenValue, bool &Changed)
{
	Changed = false;
	if(HideActive)
	{
		if(!State.m_WasActive)
		{
			State.m_WasActive = true;
			State.m_SavedValue = CurrentValue;
			State.m_AutoChangedValue = false;
			if(CurrentValue != HiddenValue)
			{
				Changed = true;
				State.m_AutoChangedValue = true;
				return HiddenValue;
			}
		}
		return CurrentValue;
	}
	if(State.m_WasActive)
	{
		State.m_WasActive = false;
		if(State.m_AutoChangedValue && CurrentValue == HiddenValue)
		{
			Changed = true;
			State.m_AutoChangedValue = false;
			return State.m_SavedValue;
		}
		State.m_AutoChangedValue = false;
	}
	else
	{
		State.m_SavedValue = CurrentValue;
	}
	return CurrentValue;
}

bool ApplyQmGoresLinkedConfig(bool GoresActive, bool AutoToggle, bool CurrentValue, bool &Changed)
{
	Changed = false;
	if(!AutoToggle)
		return CurrentValue;
	Changed = CurrentValue != GoresActive;
	return GoresActive;
}

int ApplyQmGoresDummyHammerConfig(bool GoresActive, int CurrentValue, bool &Changed)
{
	Changed = false;
	if(!GoresActive || CurrentValue == 0)
		return CurrentValue;
	Changed = true;
	return 0;
}

bool ShouldHideGoresGuide(bool GoresEnabled, bool HideGuidesEnabled, bool ManualGuideVisible)
{
	return GoresEnabled && HideGuidesEnabled && !ManualGuideVisible;
}

bool ShouldRenderGoresDebugRoute(bool Online, bool DebugRouteEnabled, bool GoresMapProgressEnabled)
{
	return Online && DebugRouteEnabled && GoresMapProgressEnabled;
}

bool ConsumeQmBudgetedWork(int &Cursor, int Total, int Budget)
{
	if(Cursor >= Total)
		return false;
	if(Budget <= 0)
		return true;
	Cursor = std::min(Total, Cursor + Budget);
	return Cursor < Total;
}

bool ShouldHideFocusHud(bool FocusActive, bool HideHud)
{
	return FocusActive && HideHud;
}

bool ShouldRenderFocusSpectatorHud(bool SpectatorActive, bool SpectatorHudEnabled, bool MainHudVisible, bool FocusActive, bool HideHud)
{
	return SpectatorActive && SpectatorHudEnabled && !MainHudVisible && ShouldHideFocusHud(FocusActive, HideHud);
}

bool ShouldHideFocusScoreboard(bool FocusActive, bool HideScoreboard)
{
	return FocusActive && HideScoreboard;
}

bool ShouldHideFocusNames(bool FocusActive, bool HideNames)
{
	return FocusActive && HideNames;
}

bool ShouldHideFocusNameplates(bool FocusActive, bool HideNameplates)
{
	return FocusActive && HideNameplates;
}

bool ShouldHideFocusJumpEffects(bool FocusActive, bool HideJumpEffects)
{
	return FocusActive && HideJumpEffects;
}

bool ShouldHideFocusKillEffects(bool FocusActive, bool HideKillEffects)
{
	return FocusActive && HideKillEffects;
}

bool ShouldHideFocusExplosionEffects(bool FocusActive, bool HideExplosionEffects)
{
	return FocusActive && HideExplosionEffects;
}

bool ShouldHideFocusFreezeEffects(bool FocusActive, bool HideFreezeEffects)
{
	return FocusActive && HideFreezeEffects;
}

bool ShouldHideFocusHammerEffects(bool FocusActive, bool HideHammerEffects)
{
	return FocusActive && HideHammerEffects;
}

bool ShouldHideFocusMuzzleEffects(bool FocusActive, bool HideMuzzleEffects)
{
	return FocusActive && HideMuzzleEffects;
}

bool ShouldMuteFocusJumpSounds(bool FocusActive, bool MuteJumpSounds)
{
	return FocusActive && MuteJumpSounds;
}

bool ShouldMuteFocusDeathSounds(bool FocusActive, bool MuteDeathSounds)
{
	return FocusActive && MuteDeathSounds;
}

bool ShouldMuteFocusHammerSounds(bool FocusActive, bool MuteHammerSounds)
{
	return FocusActive && MuteHammerSounds;
}

bool ShouldPlayFocusJumpSound(bool FocusActive, bool MuteJumpSounds, bool SoundEnabled)
{
	return SoundEnabled && !ShouldMuteFocusJumpSounds(FocusActive, MuteJumpSounds);
}

bool ShouldPlayFocusDeathOrSpawnSound(bool FocusActive, bool MuteDeathSounds, bool SoundEnabled)
{
	return SoundEnabled && !ShouldMuteFocusDeathSounds(FocusActive, MuteDeathSounds);
}

SQmAirJumpEffectDecision GetQmAirJumpEffectDecision(bool FocusActive, bool HideJumpEffects, bool MuteJumpSounds, bool SoundEnabled)
{
	return {!ShouldHideFocusJumpEffects(FocusActive, HideJumpEffects), ShouldPlayFocusJumpSound(FocusActive, MuteJumpSounds, SoundEnabled)};
}

bool ShouldHideFocusMapProgress(bool FocusActive, bool HideMapProgress)
{
	return FocusActive && HideMapProgress;
}

bool ShouldRenderMapProgressBar(bool MapProgressEnabled, int MapProgressStyle, bool PlayerStatsHudEnabled, bool GoresMapProgressEnabled)
{
	return MapProgressEnabled && !(MapProgressStyle != 0 && PlayerStatsHudEnabled) && GoresMapProgressEnabled;
}

bool ShouldHideFocusInfoMessages(bool FocusActive, bool HideInfoMessages)
{
	return FocusActive && HideInfoMessages;
}

bool ShouldHideFocusDirectionIndicators(bool FocusActive, bool HideDirectionIndicators)
{
	return FocusActive && HideDirectionIndicators;
}

bool ShouldHideFocusGuideLines(bool FocusActive, bool HideGuideLines)
{
	return FocusActive && HideGuideLines;
}

bool ShouldRenderFocusFilteredChatLine(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, int ClientId, bool ForceVisible, bool ServerMessageIsBasicInfo)
{
	if(ForceVisible)
		return true;
	if(ClientId == -2)
		return !FocusHideEcho;
	if(ClientId == -1)
		return ServerMessageIsBasicInfo ? !FocusHideSystemInfoMessages : !FocusHideSystemPromptMessages;
	if(ClientId >= 0)
		return !FocusHidePlayerMessages;
	if(FocusHideSystemPromptMessages)
		return false;
	return true;
}

bool ShouldRenderAnyFocusFilteredChat(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, bool HasForceVisibleLine)
{
	return !(FocusHidePlayerMessages && FocusHideSystemInfoMessages && FocusHideSystemPromptMessages && FocusHideEcho) || HasForceVisibleLine;
}

SQmFocusModeDecisions GetQmFocusModeDecisions(const SQmFocusModeConfig &Config)
{
	SQmFocusModeDecisions Decisions;
	Decisions.m_AirJump = GetQmAirJumpEffectDecision(Config.m_FocusActive, Config.m_HideJumpEffects, Config.m_MuteJumpSounds, Config.m_SoundEnabled);
	Decisions.m_HideKillEffects = ShouldHideFocusKillEffects(Config.m_FocusActive, Config.m_HideKillEffects);
	Decisions.m_HideExplosionEffects = ShouldHideFocusExplosionEffects(Config.m_FocusActive, Config.m_HideExplosionEffects);
	Decisions.m_HideFreezeEffects = ShouldHideFocusFreezeEffects(Config.m_FocusActive, Config.m_HideFreezeEffects);
	Decisions.m_HideHammerEffects = ShouldHideFocusHammerEffects(Config.m_FocusActive, Config.m_HideHammerEffects);
	Decisions.m_HideMuzzleEffects = ShouldHideFocusMuzzleEffects(Config.m_FocusActive, Config.m_HideMuzzleEffects);
	Decisions.m_MuteDeathSounds = ShouldMuteFocusDeathSounds(Config.m_FocusActive, Config.m_MuteDeathSounds);
	Decisions.m_MuteHammerSounds = ShouldMuteFocusHammerSounds(Config.m_FocusActive, Config.m_MuteHammerSounds);
	Decisions.m_RenderMapProgressBar = ShouldRenderMapProgressBar(Config.m_MapProgressEnabled, Config.m_MapProgressStyle, Config.m_PlayerStatsHudEnabled, Config.m_GoresMapProgressEnabled) && !ShouldHideFocusMapProgress(Config.m_FocusActive, Config.m_HideMapProgress);
	Decisions.m_HideHud = ShouldHideFocusHud(Config.m_FocusActive, Config.m_HideHud);
	Decisions.m_HideScoreboard = ShouldHideFocusScoreboard(Config.m_FocusActive, Config.m_HideScoreboard);
	Decisions.m_HideNames = ShouldHideFocusNames(Config.m_FocusActive, Config.m_HideNames);
	Decisions.m_HideNameplates = ShouldHideFocusNameplates(Config.m_FocusActive, Config.m_HideNameplates);
	Decisions.m_HideInfoMessages = ShouldHideFocusInfoMessages(Config.m_FocusActive, Config.m_HideInfoMessages);
	Decisions.m_HideDirectionIndicators = ShouldHideFocusDirectionIndicators(Config.m_FocusActive, Config.m_HideDirectionIndicators);
	Decisions.m_HideGuideLines = ShouldHideFocusGuideLines(Config.m_FocusActive, Config.m_HideGuideLines);
	Decisions.m_HidePlayerMessages = Config.m_FocusActive && Config.m_HidePlayerMessages;
	Decisions.m_HideSystemInfoMessages = Config.m_FocusActive && Config.m_HideSystemInfoMessages;
	Decisions.m_HideSystemPromptMessages = Config.m_FocusActive && Config.m_HideSystemPromptMessages;
	Decisions.m_HideEchoMessages = Config.m_FocusActive && Config.m_HideEchoMessages;
	return Decisions;
}
