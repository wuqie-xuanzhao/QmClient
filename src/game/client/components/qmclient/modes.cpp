#include "modes.h"

#include <base/str.h>

#include <generated/protocol.h>

#include <algorithm>

static bool QmTextContainsNoCase(const char *pText, const char *pNeedle)
{
	return pText && pText[0] != '\0' && pNeedle && pNeedle[0] != '\0' && str_find_nocase(pText, pNeedle) != nullptr;
}

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

int ApplyQmGoresDummyHammerOverride(SQmFocusConfigOverrideState &State, bool GoresActive, bool Disable, int CurrentValue, bool &Changed)
{
	return ApplyQmFocusConfigOverride(State, GoresActive && Disable, CurrentValue, 0, Changed);
}

bool ShouldKeepQmGoresHammerInFreeze(bool GoresCycleActive, bool InFreeze, bool HammerRequested)
{
	return GoresCycleActive && InFreeze && HammerRequested;
}

bool ShouldTriggerQmGoresHammerWakeup(bool GoresCycleActive, bool HammerRequested, bool ExternalHammerWakeup)
{
	return GoresCycleActive && HammerRequested && ExternalHammerWakeup;
}

int QmGoresHammerWakeupFireState(int CurrentFire)
{
	return ((CurrentFire + 1) | 1) & INPUT_STATE_MASK;
}

bool ShouldReleaseQmGoresHammerWakeupFire(bool PendingRelease, int CurrentFire)
{
	return PendingRelease && (CurrentFire & 1) != 0;
}

int QmGoresHammerWakeupReleaseFireState(int CurrentFire)
{
	return ((CurrentFire + 1) & ~1) & INPUT_STATE_MASK;
}

int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon)
{
	return HasPreHammerWeapon ? PreHammerWeapon : WEAPON_GUN;
}

bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive)
{
	return GoresCycleActive && FireJustPressed && !CurrentWeaponIsHammer && !FreezeWakeupActive;
}

bool ShouldRestoreGoresWeaponAfterHammer(bool CurrentWeaponIsHammer, bool HasPreHammerWeapon)
{
	return CurrentWeaponIsHammer && HasPreHammerWeapon;
}

bool ShouldShowQmHookStrongWeakScope(int Scope, bool Self, bool Strong, bool Weak)
{
	switch(Scope)
	{
	case QM_HOOK_STRONG_WEAK_SCOPE_SELF:
		return Self;
	case QM_HOOK_STRONG_WEAK_SCOPE_OTHERS:
		return !Self;
	case QM_HOOK_STRONG_WEAK_SCOPE_STRONG:
		return Strong;
	case QM_HOOK_STRONG_WEAK_SCOPE_WEAK:
		return Weak;
	case QM_HOOK_STRONG_WEAK_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

static bool ShouldUseQmNameplateTextPlayingScope(int Scope, bool Self, bool Friend)
{
	switch(Scope)
	{
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF:
		return false;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF:
		return Self;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS:
		return !Self;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS:
		return Friend;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS:
		return Self || Friend;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

static bool ShouldUseQmNameplateTextSpectateScope(int Scope, bool Friend, bool SpectateTarget)
{
	switch(Scope)
	{
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF:
		return false;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET:
		return SpectateTarget;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS:
		return !SpectateTarget;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_FRIENDS:
		return Friend;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS:
		return SpectateTarget || Friend;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

bool ShouldUseQmNameplateTextEffects(int PlayingScope, int SpectateScope, int DemoMode, int DemoTarget, bool DemoPlayback, bool Spectating, bool Self, bool Friend, bool SpectateTarget, int ClientId)
{
	if(DemoPlayback)
	{
		switch(DemoMode)
		{
		case QM_NAMEPLATE_TEXT_DEMO_MODE_OFF:
			return false;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_SMART:
			return SpectateTarget;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET:
			return DemoTarget >= 0 && ClientId == DemoTarget;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE:
			return ShouldUseQmNameplateTextPlayingScope(PlayingScope, Self, Friend);
		default:
			return false;
		}
	}
	if(Spectating)
		return ShouldUseQmNameplateTextSpectateScope(SpectateScope, Friend, SpectateTarget);
	return ShouldUseQmNameplateTextPlayingScope(PlayingScope, Self, Friend);
}

bool ShouldHideGoresGuide(bool GoresEnabled, bool HideGuidesEnabled, bool ManualGuideVisible)
{
	return GoresEnabled && HideGuidesEnabled && !ManualGuideVisible;
}

bool ShouldRenderGoresDebugRoute(bool Online, bool DebugRouteEnabled, bool GoresMapProgressEnabled)
{
	return Online && DebugRouteEnabled && GoresMapProgressEnabled;
}

bool ShouldEnableQmMovingWaterTiles(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName)
{
	return QmTextContainsNoCase(pGameInfoGameType, "gores") ||
	       QmTextContainsNoCase(pServerInfoGameType, "gores") ||
	       QmTextContainsNoCase(pCommunityId, "axiom") ||
	       QmTextContainsNoCase(pCommunityName, "axiom");
}

bool ShouldUseServerControlledLocalSkin(const char *pGameType)
{
	return QmTextContainsNoCase(pGameType, "infclass") ||
	       QmTextContainsNoCase(pGameType, "infc") ||
	       QmTextContainsNoCase(pGameType, "infect");
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
	return SpectatorActive && SpectatorHudEnabled && ShouldHideFocusHud(FocusActive, HideHud);
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
