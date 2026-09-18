// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MODES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MODES_H

#include <cstdint>
#include <limits>

struct CConfig;

struct SQmStatisticsModeDisplay
{
	int m_Maps = 0;
	int64_t m_PlaytimeSeconds = 0;
};

struct SQmAirJumpEffectDecision
{
	bool m_SpawnParticles = false;
	bool m_PlaySound = false;
};

struct SQmFocusModeConfig
{
	bool m_FocusActive = false;
	// 录制视频期间为 true：禅模式的一切效果都不进视频，决策按总开关关闭处理。
	bool m_VideoRecording = false;
	bool m_HideJumpEffects = false;
	bool m_HideKillEffects = false;
	bool m_HideExplosionEffects = false;
	bool m_HideFreezeEffects = false;
	bool m_HideHammerEffects = false;
	bool m_HideMuzzleEffects = false;
	bool m_MuteJumpSounds = false;
	bool m_MuteDeathSounds = false;
	bool m_MuteHammerSounds = false;
	bool m_SoundEnabled = true;
	bool m_HideMapProgress = false;
	bool m_HideHud = false;
	bool m_HideScoreboard = false;
	bool m_HideNames = false;
	bool m_HideNameplates = false;
	bool m_HideInfoMessages = false;
	bool m_HideDirectionIndicators = false;
	bool m_HideGuideLines = false;
	bool m_HidePlayerMessages = false;
	bool m_HideSystemInfoMessages = false;
	bool m_HideSystemPromptMessages = false;
	bool m_HideEchoMessages = false;
};

// 禅模式的最终判定。总开关与子开关已经在这里合并，调用方只消费结果，
// 不再自行拼 `g_Config.m_QmFocusMode != 0 && g_Config.m_QmFocusModeXxx`。
struct SQmFocusModeDecisions
{
	bool m_FocusActive = false;
	SQmAirJumpEffectDecision m_AirJump;
	bool m_PlayDeathOrSpawnSound = false;
	bool m_HideHud = false;
	bool m_HideMapProgress = false;
	bool m_HideScoreboard = false;
	bool m_HideNames = false;
	bool m_HideNameplates = false;
	bool m_HideInfoMessages = false;
	bool m_HideDirectionIndicators = false;
	bool m_HideGuideLines = false;
	bool m_HideKillEffects = false;
	bool m_HideExplosionEffects = false;
	bool m_HideFreezeEffects = false;
	bool m_HideHammerEffects = false;
	bool m_HideMuzzleEffects = false;
	bool m_MuteDeathSounds = false;
	bool m_MuteHammerSounds = false;
	bool m_HidePlayerMessages = false;
	bool m_HideSystemInfoMessages = false;
	bool m_HideSystemPromptMessages = false;
	bool m_HideEchoMessages = false;
};

struct SQmFocusConfigOverrideState
{
	bool m_WasActive = false;
	int m_SavedValue = 0;
	bool m_AutoChangedValue = false;
	int m_LastValue = 0;
};

enum EQmHookStrongWeakScope
{
	QM_HOOK_STRONG_WEAK_SCOPE_SELF = 0,
	QM_HOOK_STRONG_WEAK_SCOPE_OTHERS = 1,
	QM_HOOK_STRONG_WEAK_SCOPE_STRONG = 2,
	QM_HOOK_STRONG_WEAK_SCOPE_WEAK = 3,
	QM_HOOK_STRONG_WEAK_SCOPE_ALL = 4,
};

enum EQmNameplateTextPlayingScope
{
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF = 0,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF = 1,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS = 2,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS = 3,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS = 4,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL = 5,
};

enum EQmNameplateTextSpectateScope
{
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF = 0,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET = 1,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS = 2,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_FRIENDS = 3,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS = 4,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL = 5,
};

enum EQmNameplateTextDemoMode
{
	QM_NAMEPLATE_TEXT_DEMO_MODE_OFF = 0,
	QM_NAMEPLATE_TEXT_DEMO_MODE_SMART = 1,
	QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET = 2,
	QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE = 3,
};

int ApplyQmFocusConfigOverride(SQmFocusConfigOverrideState &State, bool HideActive, int CurrentValue, int HiddenValue, bool &Changed);
// 断线/退出时清理一次性接管：只恢复仍保持自动值的配置，并始终清空接管状态。
int ResetQmConfigOverride(SQmFocusConfigOverrideState &State, int CurrentValue, int OverrideValue, bool &Changed);
int ApplyQmGoresAutoEnableConfig(SQmFocusConfigOverrideState &State, bool GameModeEntered, bool GameModeLeft, bool AutoEnable, int CurrentValue, bool &Changed);
int ApplyQmGoresLinkedConfig(SQmFocusConfigOverrideState &State, bool GoresActive, bool AutoToggle, int CurrentValue, bool &Changed);
// Gores 模式开启的那一帧把分身锤关掉一次（由 qm_gores_disable_dummy_hammer 控制），
// 之后不再干预用户自己的开关；持续接管会让开关看起来被锁住，也无法手动重新打开。
int ApplyQmGoresDummyHammerConfig(SQmFocusConfigOverrideState &State, bool ModeActivated, bool ModeDeactivated, bool DisableRequested, int CurrentValue, bool &Changed);
bool ShouldKeepQmGoresHammerInFreeze(bool GoresCycleActive, bool InFreeze, bool HammerRequested);
bool ShouldTriggerQmGoresHammerWakeup(bool GoresCycleActive, bool HammerRequested, bool ExternalHammerWakeup);
int QmGoresHammerWakeupFireState(int CurrentFire);
bool ShouldReleaseQmGoresHammerWakeupFire(bool PendingRelease, int CurrentFire);
int QmGoresHammerWakeupReleaseFireState(int CurrentFire);
int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon);
bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive);
bool ShouldRestoreGoresWeaponAfterHammer(bool CurrentWeaponIsHammer, bool HasPreHammerWeapon);
bool ShouldShowQmHookStrongWeakScope(int Scope, bool Self, bool Strong, bool Weak);
bool ShouldUseQmNameplateTextEffects(int PlayingScope, int SpectateScope, int DemoMode, int DemoTarget, bool DemoPlayback, bool Spectating, bool Self, bool Friend, bool SpectateTarget, int ClientId);

bool ShouldHideGoresGuide(bool GoresEnabled, bool HideGuidesEnabled, bool ManualGuideVisible);
bool ShouldRenderGoresDebugRoute(bool Online, bool DebugRouteEnabled, bool GoresMapProgressEnabled);
bool ShouldEnableQmMovingWaterTiles(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName);
bool ShouldUseServerControlledLocalSkin(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName);
bool ServerPrefersTeeMenuSkin(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName);

// 皮肤描述符应当使用的协议版本。判定必须同时考虑连接协议与服务器白名单：
// 0.6 连接只能使用六部位皮肤，0.7 连接才允许七部位皮肤。
enum class EServerSkinProtocol
{
	NONE = 0,
	SIX,
	SEVEN,
};

EServerSkinProtocol ResolveServerSkinProtocol(bool Sixup, bool UseServerControlledSkin, bool LocalClientHasServerSkin);
int ResolveLocalSkinConfigIndex(bool DemoPlayback, int ClientId, int MainClientId, int DummyClientId);
bool ConsumeQmBudgetedWork(int &Cursor, int Total, int Budget);
bool QmStatisticsShouldShowAxiomGores(bool HasLocalAxiomGores, bool IsCurrentAxiomCommunity, bool HasAxiomResult);
SQmStatisticsModeDisplay ResolveQmStatisticsModeDisplay(int LocalMaps, int64_t LocalPlaytimeSeconds, bool IsAxiomGores, bool HasAxiomStats, int64_t AxiomMaps, int64_t AxiomPlaytimeSeconds, bool IsDdnet, int DdnetFinishes, int64_t DdnetPlaytimeHours = -1);
int64_t QmStatisticsChartWeight(int Maps, int64_t PlaytimeSeconds, bool UseMaps);

template<typename T>
T QmSaturatingAddStats(T Left, T Right)
{
	if(Right > 0 && Left > std::numeric_limits<T>::max() - Right)
		return std::numeric_limits<T>::max();
	if(Right < 0 && Left < std::numeric_limits<T>::min() - Right)
		return std::numeric_limits<T>::min();
	return Left + Right;
}

// 把列表中所有被 IsMode 命中的条目折叠进第一条：m_Maps / m_Score /
// m_PlaytimeSeconds 饱和累加，其余字段保留第一条的值。返回是否发生折叠。
// 统计页同一模式会因服务器社区不同、DDStats 追加同名条目等原因出现多条
// 记录，展示前必须折叠成一条，否则图例和饼图会渲染出完全相同的重复项。
template<typename TList, typename TPred>
bool QmCollapseModeEntries(TList &vStats, TPred IsMode)
{
	bool Collapsed = false;
	auto First = vStats.end();
	auto It = vStats.begin();
	while(It != vStats.end())
	{
		if(!IsMode(*It))
		{
			++It;
			continue;
		}
		if(First == vStats.end())
		{
			First = It;
			++It;
			continue;
		}
		First->m_Maps = QmSaturatingAddStats(First->m_Maps, It->m_Maps);
		First->m_Score = QmSaturatingAddStats(First->m_Score, It->m_Score);
		First->m_PlaytimeSeconds = QmSaturatingAddStats(First->m_PlaytimeSeconds, It->m_PlaytimeSeconds);
		It = vStats.erase(It);
		Collapsed = true;
	}
	return Collapsed;
}

// 禅模式决策入口：QmReadFocusModeConfig 把 20 个 qm_focus_mode_* 配置项读成快照，
// GetQmFocusModeDecisions 把总开关与子开关合并成最终判定。无参重载读取当前全局配置，
// 供界面/玩家/视觉/音效/聊天各处消费，避免每个调用点重复拼总开关条件。
SQmFocusModeConfig QmReadFocusModeConfig(const CConfig &Config);
SQmFocusModeDecisions GetQmFocusModeDecisions(const SQmFocusModeConfig &Config);
SQmFocusModeDecisions GetQmFocusModeDecisions();

bool ShouldRenderFocusSpectatorHud(bool SpectatorActive, bool SpectatorHudEnabled, bool MainHudVisible, bool HideHud);
bool ShouldRenderMapProgressBar(bool MapProgressEnabled, int MapProgressStyle, bool PlayerStatsHudEnabled, bool GoresMapProgressEnabled);
bool ShouldRenderFocusFilteredChatLine(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, int ClientId, bool ForceVisible, bool ServerMessageIsBasicInfo);
bool ShouldRenderAnyFocusFilteredChat(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, bool HasForceVisibleLine);

#endif
