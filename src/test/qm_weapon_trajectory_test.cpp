// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/components/qmclient/weapon_animation.h>
#include <game/client/components/qmclient/weapon_trajectory.h>

#include <gtest/gtest.h>

TEST(QmWeaponReloadAnimation, UsesTheActualHammerReloadVariant)
{
	EXPECT_FLOAT_EQ(QmWeaponReloadDelaySeconds(CTuningParams::DEFAULT, WEAPON_HAMMER, false), 0.125f);
	EXPECT_FLOAT_EQ(QmWeaponReloadDelaySeconds(CTuningParams::DEFAULT, WEAPON_HAMMER, true), 0.32f);
	EXPECT_FLOAT_EQ(QmWeaponReloadDelaySeconds(CTuningParams::DEFAULT, WEAPON_NINJA, false), 0.8f);
}

TEST(QmWeaponReloadAnimation, RequiresReloadBelowOneAndAHalfTimesDefault)
{
	EXPECT_TRUE(QmWeaponReloadRotation(0.02f, 0.149f, 0.1f, 50).m_Active);
	EXPECT_FALSE(QmWeaponReloadRotation(0.02f, 0.15f, 0.1f, 50).m_Active);
	EXPECT_FALSE(QmWeaponReloadRotation(0.02f, 0.151f, 0.1f, 50).m_Active);
}

TEST(QmWeaponReloadAnimation, RotatesUniformlyUntilTheTickQuantizedReloadEnds)
{
	const SQmWeaponReloadRotation Start = QmWeaponReloadRotation(0.0f, 0.125f, 0.125f, 50);
	const SQmWeaponReloadRotation Quarter = QmWeaponReloadRotation(0.03f, 0.125f, 0.125f, 50);
	const SQmWeaponReloadRotation End = QmWeaponReloadRotation(0.12f, 0.125f, 0.125f, 50);

	ASSERT_TRUE(Start.m_Active);
	EXPECT_FLOAT_EQ(Start.m_Angle, 0.0f);
	ASSERT_TRUE(Quarter.m_Active);
	EXPECT_FLOAT_EQ(Quarter.m_Angle, pi / 2.0f);
	EXPECT_FALSE(End.m_Active);
	EXPECT_FLOAT_EQ(End.m_Angle, 0.0f);
}

TEST(QmWeaponReloadAnimation, ReloadRotationOverridesWeaponSwitchRotation)
{
	const SQmWeaponReloadRotation ReloadRotation = QmWeaponReloadRotation(0.03f, 0.125f, 0.125f, 50);
	EXPECT_FLOAT_EQ(QmResolveWeaponAnimationRotation(0.75f, ReloadRotation, true), pi / 2.0f);
	EXPECT_FLOAT_EQ(QmResolveWeaponAnimationRotation(0.75f, SQmWeaponReloadRotation(), true), 0.0f);
	EXPECT_FLOAT_EQ(QmResolveWeaponAnimationRotation(0.75f, SQmWeaponReloadRotation(), false), 0.75f);
}

TEST(QmWeaponReloadAnimation, KeepsAnAttackBoundToItsWeaponAndTuneZone)
{
	SQmWeaponReloadAnimationState State;
	State.ObserveAttack(100, WEAPON_GUN, 2, false);
	EXPECT_TRUE(State.MatchesAttack(100, WEAPON_GUN));
	EXPECT_EQ(State.m_AttackTuneZone, 2);

	State.ObserveAttack(100, WEAPON_LASER, 3, false);
	EXPECT_FALSE(State.MatchesAttack(100, WEAPON_LASER));
	EXPECT_EQ(State.m_AttackTuneZone, 2);

	State.ObserveAttack(101, WEAPON_LASER, 3, false);
	EXPECT_TRUE(State.MatchesAttack(101, WEAPON_LASER));
	EXPECT_EQ(State.m_AttackTuneZone, 3);

	State.ObserveAttack(102, WEAPON_HAMMER, 4, true);
	EXPECT_TRUE(State.m_PredictedHammerHit);
}

TEST(QmWeaponTrajectory, SelectsWeaponSpecificBaseColors)
{
	const ColorRGBA GrenadeColor(0.1f, 0.2f, 0.3f, 0.4f);
	const ColorRGBA RifleInnerColor(0.2f, 0.3f, 0.4f, 0.5f);
	const ColorRGBA ShotgunInnerColor(0.3f, 0.4f, 0.5f, 0.6f);

	EXPECT_EQ(QmWeaponTrajectoryBaseColor(WEAPON_GRENADE, GrenadeColor, RifleInnerColor, ShotgunInnerColor), GrenadeColor);
	EXPECT_EQ(QmWeaponTrajectoryBaseColor(WEAPON_LASER, GrenadeColor, RifleInnerColor, ShotgunInnerColor), RifleInnerColor);
	EXPECT_EQ(QmWeaponTrajectoryBaseColor(WEAPON_SHOTGUN, GrenadeColor, RifleInnerColor, ShotgunInnerColor), ShotgunInnerColor);
	EXPECT_EQ(QmWeaponTrajectoryBaseColor(WEAPON_GUN, GrenadeColor, RifleInnerColor, ShotgunInnerColor), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}

TEST(QmWeaponTrajectory, InvertsRgbAndPreservesAlpha)
{
	const ColorRGBA Inverted = QmInvertWeaponTrajectoryColor(ColorRGBA(1.0f, 0.25f, 0.0f, 0.35f));
	EXPECT_FLOAT_EQ(Inverted.r, 0.0f);
	EXPECT_FLOAT_EQ(Inverted.g, 0.75f);
	EXPECT_FLOAT_EQ(Inverted.b, 1.0f);
	EXPECT_FLOAT_EQ(Inverted.a, 0.35f);
}

TEST(QmWeaponTrajectory, UsesLaserLineStyleForGun)
{
	EXPECT_TRUE(QmWeaponTrajectoryUsesLineStyle(WEAPON_GUN));
	EXPECT_TRUE(QmWeaponTrajectoryUsesLineStyle(WEAPON_SHOTGUN));
	EXPECT_TRUE(QmWeaponTrajectoryUsesLineStyle(WEAPON_LASER));
	EXPECT_FALSE(QmWeaponTrajectoryUsesLineStyle(WEAPON_GRENADE));
}

TEST(QmWeaponTrajectory, RespectsWeaponSpecificPlayerHitDisables)
{
	EXPECT_FALSE(QmWeaponTrajectoryCanHitOtherPlayers(WEAPON_GUN, true, false, false));
	EXPECT_FALSE(QmWeaponTrajectoryCanHitOtherPlayers(WEAPON_GRENADE, true, false, false));
	EXPECT_FALSE(QmWeaponTrajectoryCanHitOtherPlayers(WEAPON_LASER, false, true, false));
	EXPECT_FALSE(QmWeaponTrajectoryCanHitOtherPlayers(WEAPON_SHOTGUN, false, false, true));
	EXPECT_TRUE(QmWeaponTrajectoryCanHitOtherPlayers(WEAPON_GUN, false, true, true));
}

TEST(QmWeaponTrajectory, AcceptsFrontLayerTeleGunWallsForSupportedWeapons)
{
	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GUN, TILE_ALLOW_TELE_GUN, 0, 0));
	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GRENADE, TILE_ALLOW_BLUE_TELE_GUN, 0, 0));
	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_LASER, TILE_ALLOW_TELE_GUN, 0, 0));
	EXPECT_FALSE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_SHOTGUN, TILE_ALLOW_TELE_GUN, 0, 0));
}

TEST(QmWeaponTrajectory, RequiresTheCurrentWeaponTeleGunCapability)
{
	EXPECT_TRUE(QmWeaponTrajectoryHasTeleGun(WEAPON_GUN, true, false, false));
	EXPECT_TRUE(QmWeaponTrajectoryHasTeleGun(WEAPON_GRENADE, false, true, false));
	EXPECT_TRUE(QmWeaponTrajectoryHasTeleGun(WEAPON_LASER, false, false, true));
	EXPECT_FALSE(QmWeaponTrajectoryHasTeleGun(WEAPON_SHOTGUN, true, true, true));
	EXPECT_FALSE(QmWeaponTrajectoryHasTeleGun(WEAPON_GUN, false, true, true));
}

TEST(QmWeaponTrajectory, AppliesSwitchLayerWeaponDelay)
{
	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GUN, 0, TILE_ALLOW_TELE_GUN, 0));
	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GUN, 0, TILE_ALLOW_TELE_GUN, 1));
	EXPECT_FALSE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GUN, 0, TILE_ALLOW_TELE_GUN, 2));

	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GRENADE, 0, TILE_ALLOW_BLUE_TELE_GUN, 2));
	EXPECT_FALSE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_GRENADE, 0, TILE_ALLOW_BLUE_TELE_GUN, 3));

	EXPECT_TRUE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_LASER, 0, TILE_ALLOW_TELE_GUN, 3));
	EXPECT_FALSE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_LASER, 0, TILE_ALLOW_TELE_GUN, 1));
	EXPECT_FALSE(QmWeaponTrajectoryIsTeleGunWall(WEAPON_SHOTGUN, 0, TILE_ALLOW_TELE_GUN, 0));
}
