// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AFK_PRESENTATION_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AFK_PRESENTATION_H

constexpr bool IsQmAfkForPresentation(bool ServerAfk, bool Online, bool IngameMenuActive, int ClientId, int LocalClientId)
{
	return ServerAfk || (Online && IngameMenuActive && ClientId >= 0 && ClientId == LocalClientId);
}

#endif
