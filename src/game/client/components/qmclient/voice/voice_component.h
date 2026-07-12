// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_VOICE_VOICE_COMPONENT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_VOICE_VOICE_COMPONENT_H

#include "voice_core.h"

#include <engine/shared/config.h>

#include <game/client/component.h>

class CVoiceComponent : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnRender() override;
	void OnConsoleInit() override;

	void RenderOverlay();
	const CRClientVoice &Voice() const { return m_Voice; }

private:
	static void ConVoicePtt(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceListDevices(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceSetInputDevice(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceSetOutputDevice(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceClearInputDevice(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceClearOutputDevice(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceToggleMicMute(IConsole::IResult *pResult, void *pUserData);
	static void ConVoiceSetMicMute(IConsole::IResult *pResult, void *pUserData);

	CRClientVoice m_Voice;
	CVoiceOverlayState m_OverlayState;
};

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_VOICE_VOICE_COMPONENT_H
