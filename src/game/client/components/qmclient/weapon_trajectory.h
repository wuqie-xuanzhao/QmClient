// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_WEAPON_TRAJECTORY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_WEAPON_TRAJECTORY_H

#include <generated/protocol.h>

#include <game/client/component.h>

class CQmWeaponTrajectory : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void Render(const CNetObj_Character *pPrevChar, const CNetObj_Character *pPlayerChar, int ClientId);
};

#endif
