// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_HUD_FROZEN_TEE_STATE_H
#define GAME_CLIENT_COMPONENTS_HUD_FROZEN_TEE_STATE_H

#include <algorithm>

struct SHudFrozenTeeState
{
	bool m_DeathOverride = false;
	int m_DeathBarrierTick = -1;
};

inline void QmHudMarkTeeDead(SHudFrozenTeeState &State, int GameTick, int PredictedGameTick)
{
	State.m_DeathOverride = true;
	State.m_DeathBarrierTick = std::max(GameTick, PredictedGameTick);
}

inline void QmHudObserveTeeCharacterSnapshot(SHudFrozenTeeState &State, bool HasCharacter, int SnapshotTick)
{
	if(State.m_DeathOverride && HasCharacter && SnapshotTick > State.m_DeathBarrierTick)
		State = {};
}

inline bool QmHudTeeIsFrozen(const SHudFrozenTeeState &State, int FreezeEnd, bool DeepFrozen)
{
	return !State.m_DeathOverride && (FreezeEnd > 0 || DeepFrozen);
}

#endif
