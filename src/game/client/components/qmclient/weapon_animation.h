#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_WEAPON_ANIMATION_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_WEAPON_ANIMATION_H

#include <base/math.h>

#include <game/gamecore.h>

struct SQmWeaponReloadRotation
{
	bool m_Active = false;
	float m_Angle = 0.0f;
};

struct SQmWeaponReloadAnimationState
{
	int m_ObservedAttackTick = -1;
	int m_AttackWeapon = -1;
	int m_AttackTuneZone = 0;
	bool m_PredictedHammerHit = false;

	void ObserveAttack(int AttackTick, int Weapon, int TuneZone, bool PredictedHammerHit)
	{
		if(AttackTick == m_ObservedAttackTick)
			return;
		m_ObservedAttackTick = AttackTick;
		m_AttackWeapon = AttackTick > 0 ? Weapon : -1;
		m_AttackTuneZone = TuneZone;
		m_PredictedHammerHit = AttackTick > 0 && PredictedHammerHit;
	}

	bool MatchesAttack(int AttackTick, int Weapon) const
	{
		return AttackTick > 0 && AttackTick == m_ObservedAttackTick && Weapon == m_AttackWeapon;
	}
};

inline int QmWeaponReloadTicks(float ReloadSeconds, int TickSpeed)
{
	return ReloadSeconds > 0.0f && TickSpeed > 0 ? (int)(ReloadSeconds * TickSpeed) : 0;
}

inline bool QmWeaponReloadAnimationEligible(float ReloadSeconds, float DefaultReloadSeconds, int TickSpeed)
{
	return ReloadSeconds > 0.0f && DefaultReloadSeconds > 0.0f && ReloadSeconds < DefaultReloadSeconds * 1.5f && QmWeaponReloadTicks(ReloadSeconds, TickSpeed) > 0;
}

inline float QmWeaponReloadDelaySeconds(const CTuningParams &Tuning, int Weapon, bool HammerHit)
{
	if(Weapon == WEAPON_HAMMER)
		return (float)(HammerHit ? Tuning.m_HammerHitFireDelay : Tuning.m_HammerFireDelay) / 1000.0f;
	if(Weapon < WEAPON_GUN || Weapon > WEAPON_NINJA)
		return 0.0f;
	return Tuning.GetWeaponFireDelay(Weapon);
}

inline SQmWeaponReloadRotation QmWeaponReloadRotation(float TimeSinceAttack, float ReloadSeconds, float DefaultReloadSeconds, int TickSpeed)
{
	if(TimeSinceAttack < 0.0f || !QmWeaponReloadAnimationEligible(ReloadSeconds, DefaultReloadSeconds, TickSpeed))
		return {};

	const int ReloadTicks = QmWeaponReloadTicks(ReloadSeconds, TickSpeed);
	const float ReloadDuration = ReloadTicks / (float)TickSpeed;
	if(TimeSinceAttack >= ReloadDuration)
		return {};

	return {true, TimeSinceAttack / ReloadDuration * 2.0f * pi};
}

inline float QmResolveWeaponAnimationRotation(float WeaponSwitchRotation, const SQmWeaponReloadRotation &ReloadRotation, bool FireOverridesSwitchRotation)
{
	if(ReloadRotation.m_Active)
		return ReloadRotation.m_Angle;
	return FireOverridesSwitchRotation ? 0.0f : WeaponSwitchRotation;
}

#endif
