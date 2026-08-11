// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_AFK_PRESENTATION_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_AFK_PRESENTATION_H

constexpr float QM_AFK_PRESENTATION_ALPHA = 0.4f;

constexpr bool IsQmAfkForPresentation(bool ServerAfk, bool Online, bool IngameMenuActive, int ClientId, int LocalClientId)
{
	return ServerAfk || (Online && IngameMenuActive && ClientId >= 0 && ClientId == LocalClientId);
}

constexpr float ApplyQmAfkPresentationAlpha(float BaseAlpha, bool Afk)
{
	return Afk && BaseAlpha > QM_AFK_PRESENTATION_ALPHA ? QM_AFK_PRESENTATION_ALPHA : BaseAlpha;
}

#endif
