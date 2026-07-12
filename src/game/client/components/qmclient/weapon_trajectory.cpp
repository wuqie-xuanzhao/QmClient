// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "weapon_trajectory.h"

#include <base/color.h>

#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/client_data.h>

#include <game/client/components/controls.h>
#include <game/client/components/players.h>
#include <game/client/components/qmclient/modes.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <vector>

void CQmWeaponTrajectory::Render(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	int ClientId)
{
	if(ShouldHideFocusGuideLines(g_Config.m_QmFocusMode != 0, g_Config.m_QmFocusModeHideGuideLines != 0))
		return;

	const int TrajectoryMode = std::clamp(g_Config.m_QmWeaponTrajectory, 0, 2);
	const bool ManualTrajectoryVisible = GameClient()->m_Controls.m_aShowWeaponTrajectory[g_Config.m_ClDummy] != 0;
	const bool TrajectoryVisible = TrajectoryMode == 2 || (TrajectoryMode == 1 && ManualTrajectoryVisible);
	if(GameClient()->m_TClient.ShouldHideGoresGuides(TrajectoryVisible))
		return;

	if(ClientId < 0 || !TrajectoryVisible)
		return;

	const int Weapon = pPlayerChar->m_Weapon;
	if(Weapon != WEAPON_GRENADE && Weapon != WEAPON_SHOTGUN && Weapon != WEAPON_LASER)
		return;

	const float TrajectoryAlpha = std::clamp(g_Config.m_QmWeaponTrajectoryAlpha / 100.0f, 0.0f, 1.0f);
	if(TrajectoryAlpha <= 0.0f)
		return;
	const float TrajectoryHalfWidth = 0.5f + (float)(std::clamp(g_Config.m_QmWeaponTrajectoryWidth, 1, 10) - 1) * 0.3f;
	ColorRGBA TrajectoryColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmWeaponTrajectoryColor));
	TrajectoryColor.a = TrajectoryAlpha;

	float Intra = GameClient()->m_aClients[ClientId].m_IsPredicted ? Client()->PredIntraGameTick(g_Config.m_ClDummy) : Client()->IntraGameTick(g_Config.m_ClDummy);
	const float Angle = GameClient()->m_Players.GetPlayerTargetAngle(pPrevChar, pPlayerChar, ClientId, Intra);
	const vec2 Direction = direction(Angle);
	if(length(Direction) < 0.0001f)
		return;

	const vec2 Position = GameClient()->m_aClients[ClientId].m_RenderPos;
	int TuneZone = 0;
	if(Client()->State() == IClient::STATE_ONLINE && GameClient()->m_GameWorld.m_WorldConfig.m_UseTuneZones)
		TuneZone = Collision()->IsTune(Collision()->GetMapIndex(Position));
	const CTuningParams *pTuning = GameClient()->GetTuning(TuneZone);

	auto FindBlockingTee = [&](const vec2 &From, const vec2 &To, bool IgnoreShooterAtStart, vec2 &OutPos) -> bool {
		const float SelfIgnoreDistance = CCharacterCore::PhysicalSize() * 0.5f;
		float ClosestDistance = distance(From, To) + 1.0f;
		bool Found = false;
		const CGameClient::CClientData &ShooterData = GameClient()->m_aClients[ClientId];
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			const CGameClient::CClientData &ClientData = GameClient()->m_aClients[i];
			if(!ClientData.m_Active || ClientData.m_Team == TEAM_SPECTATORS)
				continue;
			if(!GameClient()->m_Snap.m_aCharacters[i].m_Active)
				continue;
			const bool IsOneSuper = ClientData.m_Super || ShooterData.m_Super;
			const bool IsOneSolo = ClientData.m_Solo || ShooterData.m_Solo;
			if(!IsOneSuper && (!GameClient()->m_Teams.SameTeam(i, ClientId) || IsOneSolo))
				continue;

			vec2 ClosestPoint;
			const vec2 TeePos = ClientData.m_RenderPos;
			if(closest_point_on_line(From, To, TeePos, ClosestPoint))
			{
				if(distance(TeePos, ClosestPoint) < CCharacterCore::PhysicalSize())
				{
					const float Dist = distance(From, ClosestPoint);
					if(i == ClientId && IgnoreShooterAtStart && Dist <= SelfIgnoreDistance)
						continue;
					if(Dist < ClosestDistance)
					{
						ClosestDistance = Dist;
						OutPos = ClosestPoint;
						Found = true;
					}
				}
			}
		}
		return Found;
	};

	if(Weapon == WEAPON_GRENADE)
	{
		const vec2 StartPos = Position + Direction * (CCharacterCore::PhysicalSize() * 0.75f);
		float Curvature = pTuning->m_GrenadeCurvature;
		float Speed = pTuning->m_GrenadeSpeed;
		float Lifetime = pTuning->m_GrenadeLifetime * 10.0f;

		constexpr int PointCount = 180;
		std::vector<vec2> vPoints;
		vPoints.reserve(PointCount);
		vec2 LandingPos = StartPos;

		vec2 PrevPos = StartPos;
		for(int i = 0; i < PointCount; ++i)
		{
			const float U = PointCount > 1 ? (float)i / (float)(PointCount - 1) : 0.0f;
			const float T = std::pow(U, 2.0f);
			vec2 Pos = CalcPos(StartPos, Direction, Curvature, Speed, Lifetime * T);
			if(i > 0)
			{
				vec2 ColPos, BeforePos;
				const bool CollidesWithWorld = Collision()->IntersectLine(PrevPos, Pos, &ColPos, &BeforePos);
				const vec2 SegmentEnd = CollidesWithWorld ? ColPos : Pos;
				vec2 TeeHitPos;
				if(FindBlockingTee(PrevPos, SegmentEnd, true, TeeHitPos))
				{
					vPoints.push_back(TeeHitPos);
					LandingPos = TeeHitPos;
					break;
				}
				if(CollidesWithWorld)
				{
					vPoints.push_back(ColPos);
					LandingPos = ColPos;
					break;
				}
			}
			vPoints.push_back(Pos);
			LandingPos = Pos;
			PrevPos = Pos;
		}

		if(vPoints.empty())
			return;

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		const ColorRGBA BaseColor = TrajectoryColor;
		const float StartSize = 2.5f + TrajectoryHalfWidth * 1.5f;
		for(size_t i = 0; i < vPoints.size(); ++i)
		{
			const float T = vPoints.size() > 1 ? (float)i / (float)(vPoints.size() - 1) : 0.0f;
			const float Fade = 1.0f - T;
			if(Fade <= 0.0f)
				continue;
			float Size = StartSize * Fade;
			if(Size < TrajectoryHalfWidth)
				Size = TrajectoryHalfWidth;

			ColorRGBA Color = BaseColor;
			Color.a = TrajectoryAlpha * Fade;
			Graphics()->SetColor(Color);
			Graphics()->DrawCircle(vPoints[i].x, vPoints[i].y, Size, 12);
		}
		Graphics()->QuadsEnd();

		const IGraphics::CTextureHandle &GrenadeCursor = GameClient()->m_GameSkin.m_SpriteWeaponGrenadeCursor;
		if(GrenadeCursor.IsValid())
		{
			float CursorSpriteScaleX, CursorSpriteScaleY;
			Graphics()->GetSpriteScale(g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_pSpriteCursor, CursorSpriteScaleX, CursorSpriteScaleY);

			float CursorScale = (float)g_Config.m_TcCursorScale / 100.0f;
			CursorScale = std::clamp(CursorScale, 0.3f, 3.0f);
			const float CursorSize = 64.0f * CursorScale * 0.8f;
			IGraphics::CQuadItem CursorQuad(
				LandingPos.x,
				LandingPos.y,
				CursorSize * CursorSpriteScaleX,
				CursorSize * CursorSpriteScaleY);

			Graphics()->TextureSet(GrenadeCursor);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.9f);
			Graphics()->QuadsDraw(&CursorQuad, 1);
			Graphics()->QuadsEnd();
		}
		return;
	}

	float Energy = pTuning->m_LaserReach;
	if(GameClient()->m_GameWorld.m_WorldConfig.m_IsFNG && Energy < 10.0f)
		Energy = 800.0f;

	std::vector<IGraphics::CLineItem> vLineSegments;
	vLineSegments.reserve(pTuning->m_LaserBounceNum + 2);

	vec2 From = Position;
	vec2 Dir = Direction;
	bool ZeroEnergyBounceInLastTick = false;
	int Bounces = 0;

	while(Energy > 0.0f)
	{
		vec2 To = From + Dir * Energy;
		vec2 ColTile;
		vec2 HitPos;
		int Res = Collision()->IntersectLineTeleWeapon(From, To, &ColTile, &HitPos);
		vec2 SegmentEnd = Res ? HitPos : To;
		vec2 TeeHitPos;
		const bool IgnoreShooterAtStart =
			Bounces == 0 || g_Config.m_SvOldLaser || !GameClient()->m_GameWorld.m_WorldConfig.m_IsDDRace;
		if(FindBlockingTee(From, SegmentEnd, IgnoreShooterAtStart, TeeHitPos))
		{
			vLineSegments.emplace_back(From, TeeHitPos);
			break;
		}
		if(!Res)
		{
			vLineSegments.emplace_back(From, To);
			break;
		}

		vLineSegments.emplace_back(From, SegmentEnd);

		vec2 TempPos = SegmentEnd;
		vec2 TempDir = Dir * 4.0f;
		int SavedTile = 0;
		if(Res == -1)
		{
			SavedTile = Collision()->GetTile(round_to_int(ColTile.x), round_to_int(ColTile.y));
			Collision()->SetCollisionAt(round_to_int(ColTile.x), round_to_int(ColTile.y), TILE_SOLID);
		}
		Collision()->MovePoint(&TempPos, &TempDir, 1.0f, nullptr);
		if(Res == -1)
		{
			Collision()->SetCollisionAt(round_to_int(ColTile.x), round_to_int(ColTile.y), SavedTile);
		}

		const float Distance = distance(From, TempPos);
		if(Distance == 0.0f && ZeroEnergyBounceInLastTick)
			break;

		Energy -= Distance + pTuning->m_LaserBounceCost;
		ZeroEnergyBounceInLastTick = Distance == 0.0f;
		if(Energy <= 0.0f)
			break;

		Bounces++;
		if(Bounces > pTuning->m_LaserBounceNum)
			break;

		if(length(TempDir) < 0.0001f)
			break;

		Dir = normalize(TempDir);
		From = TempPos;
	}

	if(vLineSegments.empty())
		return;

	Graphics()->TextureClear();
	if(g_Config.m_QmWeaponTrajectoryWidth > 1)
	{
		std::vector<IGraphics::CFreeformItem> vLineQuadSegments;
		vLineQuadSegments.reserve(vLineSegments.size());
		for(const IGraphics::CLineItem &LineSegment : vLineSegments)
		{
			const vec2 FromPos(LineSegment.m_X0, LineSegment.m_Y0);
			const vec2 ToPos(LineSegment.m_X1, LineSegment.m_Y1);
			const vec2 Delta = ToPos - FromPos;
			if(length(Delta) < 0.0001f)
				continue;
			const vec2 Perp = normalize(vec2(-Delta.y, Delta.x));
			vLineQuadSegments.emplace_back(
				ToPos + Perp * -TrajectoryHalfWidth,
				ToPos + Perp * TrajectoryHalfWidth,
				FromPos + Perp * -TrajectoryHalfWidth,
				FromPos + Perp * TrajectoryHalfWidth);
		}

		if(vLineQuadSegments.empty())
			return;

		Graphics()->QuadsBegin();
		Graphics()->SetColor(TrajectoryColor);
		Graphics()->QuadsDrawFreeform(vLineQuadSegments.data(), vLineQuadSegments.size());
		Graphics()->QuadsEnd();
	}
	else
	{
		Graphics()->LinesBegin();
		Graphics()->SetColor(TrajectoryColor);
		Graphics()->LinesDraw(vLineSegments.data(), vLineSegments.size());
		Graphics()->LinesEnd();
	}
}
