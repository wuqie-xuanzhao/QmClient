// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_TUNE_ZONE_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_TUNE_ZONE_EFFECTS_H

#include <game/gamecore.h>

#include <algorithm>
#include <array>

enum class EQmTuneZoneEffectCategory
{
	UNUSED = -1,
	GRAVITY = 0,
	MOVEMENT,
	JUMP,
	HOOK,
	COLLISION,
	GUN_JETPACK,
	SHOTGUN,
	GRENADE_EXPLOSION,
	LASER,
	HAMMER,
	WEAPON_FIRE_RATE,
	VELRAMP,
	ELASTICITY,
	COUNT,
};

struct SQmTuneZoneEffectSummary
{
	static constexpr int MAX_VISIBLE_CATEGORIES = 7;
	static constexpr int CATEGORY_COUNT = static_cast<int>(EQmTuneZoneEffectCategory::COUNT);

	std::array<EQmTuneZoneEffectCategory, CATEGORY_COUNT> m_aCategories{};
	int m_Count = 0;

	bool HasEffects() const { return m_Count > 0; }
	int VisibleCategoryCount() const { return std::clamp(m_Count, 0, MAX_VISIBLE_CATEGORIES); }
	int HiddenCategoryCount() const { return std::max(0, m_Count - MAX_VISIBLE_CATEGORIES); }
	int DisplaySlotCount() const { return VisibleCategoryCount() + (HiddenCategoryCount() > 0 ? 1 : 0); }
};

namespace qm_tune_zone_effects_detail
{
	inline bool Changed(const CTuneParam &ZoneZero, const CTuneParam &Zone)
	{
		return ZoneZero.Get() != Zone.Get();
	}
}

inline SQmTuneZoneEffectSummary BuildQmTuneZoneEffectSummary(const CTuningParams &ZoneZero, const CTuningParams &Zone)
{
	using qm_tune_zone_effects_detail::Changed;
	static_assert(sizeof(CTuningParams) == sizeof(int) * 47, "Update Tune Zone effect categories when tuning parameters change");

	SQmTuneZoneEffectSummary Summary;
	const auto Add = [&](EQmTuneZoneEffectCategory Category, bool HasChange) {
		if(HasChange)
			Summary.m_aCategories[Summary.m_Count++] = Category;
	};

	Add(EQmTuneZoneEffectCategory::GRAVITY,
		Changed(ZoneZero.m_Gravity, Zone.m_Gravity));
	Add(EQmTuneZoneEffectCategory::MOVEMENT,
		Changed(ZoneZero.m_GroundControlSpeed, Zone.m_GroundControlSpeed) ||
			Changed(ZoneZero.m_GroundControlAccel, Zone.m_GroundControlAccel) ||
			Changed(ZoneZero.m_GroundFriction, Zone.m_GroundFriction) ||
			Changed(ZoneZero.m_AirControlSpeed, Zone.m_AirControlSpeed) ||
			Changed(ZoneZero.m_AirControlAccel, Zone.m_AirControlAccel) ||
			Changed(ZoneZero.m_AirFriction, Zone.m_AirFriction));
	Add(EQmTuneZoneEffectCategory::JUMP,
		Changed(ZoneZero.m_GroundJumpImpulse, Zone.m_GroundJumpImpulse) ||
			Changed(ZoneZero.m_AirJumpImpulse, Zone.m_AirJumpImpulse));
	Add(EQmTuneZoneEffectCategory::HOOK,
		Changed(ZoneZero.m_HookLength, Zone.m_HookLength) ||
			Changed(ZoneZero.m_HookFireSpeed, Zone.m_HookFireSpeed) ||
			Changed(ZoneZero.m_HookDragAccel, Zone.m_HookDragAccel) ||
			Changed(ZoneZero.m_HookDragSpeed, Zone.m_HookDragSpeed) ||
			Changed(ZoneZero.m_HookDuration, Zone.m_HookDuration));
	Add(EQmTuneZoneEffectCategory::COLLISION,
		Changed(ZoneZero.m_PlayerCollision, Zone.m_PlayerCollision) ||
			Changed(ZoneZero.m_PlayerHooking, Zone.m_PlayerHooking));
	Add(EQmTuneZoneEffectCategory::GUN_JETPACK,
		Changed(ZoneZero.m_GunCurvature, Zone.m_GunCurvature) ||
			Changed(ZoneZero.m_GunSpeed, Zone.m_GunSpeed) ||
			Changed(ZoneZero.m_GunLifetime, Zone.m_GunLifetime) ||
			Changed(ZoneZero.m_JetpackStrength, Zone.m_JetpackStrength));
	Add(EQmTuneZoneEffectCategory::SHOTGUN,
		Changed(ZoneZero.m_ShotgunCurvature, Zone.m_ShotgunCurvature) ||
			Changed(ZoneZero.m_ShotgunSpeed, Zone.m_ShotgunSpeed) ||
			Changed(ZoneZero.m_ShotgunStrength, Zone.m_ShotgunStrength));
	Add(EQmTuneZoneEffectCategory::GRENADE_EXPLOSION,
		Changed(ZoneZero.m_GrenadeCurvature, Zone.m_GrenadeCurvature) ||
			Changed(ZoneZero.m_GrenadeSpeed, Zone.m_GrenadeSpeed) ||
			Changed(ZoneZero.m_GrenadeLifetime, Zone.m_GrenadeLifetime) ||
			Changed(ZoneZero.m_ExplosionStrength, Zone.m_ExplosionStrength));
	Add(EQmTuneZoneEffectCategory::LASER,
		Changed(ZoneZero.m_LaserReach, Zone.m_LaserReach) ||
			Changed(ZoneZero.m_LaserBounceDelay, Zone.m_LaserBounceDelay) ||
			Changed(ZoneZero.m_LaserBounceNum, Zone.m_LaserBounceNum) ||
			Changed(ZoneZero.m_LaserBounceCost, Zone.m_LaserBounceCost));
	Add(EQmTuneZoneEffectCategory::HAMMER,
		Changed(ZoneZero.m_HammerStrength, Zone.m_HammerStrength));
	Add(EQmTuneZoneEffectCategory::WEAPON_FIRE_RATE,
		Changed(ZoneZero.m_HammerFireDelay, Zone.m_HammerFireDelay) ||
			Changed(ZoneZero.m_GunFireDelay, Zone.m_GunFireDelay) ||
			Changed(ZoneZero.m_ShotgunFireDelay, Zone.m_ShotgunFireDelay) ||
			Changed(ZoneZero.m_GrenadeFireDelay, Zone.m_GrenadeFireDelay) ||
			Changed(ZoneZero.m_LaserFireDelay, Zone.m_LaserFireDelay) ||
			Changed(ZoneZero.m_NinjaFireDelay, Zone.m_NinjaFireDelay) ||
			Changed(ZoneZero.m_HammerHitFireDelay, Zone.m_HammerHitFireDelay));
	Add(EQmTuneZoneEffectCategory::VELRAMP,
		Changed(ZoneZero.m_VelrampStart, Zone.m_VelrampStart) ||
			Changed(ZoneZero.m_VelrampRange, Zone.m_VelrampRange) ||
			Changed(ZoneZero.m_VelrampCurvature, Zone.m_VelrampCurvature));
	Add(EQmTuneZoneEffectCategory::ELASTICITY,
		Changed(ZoneZero.m_GroundElasticityX, Zone.m_GroundElasticityX) ||
			Changed(ZoneZero.m_GroundElasticityY, Zone.m_GroundElasticityY));

	return Summary;
}

inline float QmTuneZoneEffectSatelliteWidth(const SQmTuneZoneEffectSummary &Summary, float MinimumDiameter, float SlotWidth, float SlotGap, float HorizontalPadding)
{
	const int SlotCount = Summary.DisplaySlotCount();
	if(SlotCount <= 0)
		return 0.0f;
	const float ContentWidth = std::max(0.0f, SlotWidth) * SlotCount + std::max(0.0f, SlotGap) * (SlotCount - 1) + std::max(0.0f, HorizontalPadding) * 2.0f;
	return std::max(std::max(0.0f, MinimumDiameter), ContentWidth);
}

#endif
