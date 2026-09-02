// Input Overlay component. The on-disk layout is the official OBS v5 format.
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_INPUT_OVERLAY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_INPUT_OVERLAY_H

#include "input_overlay_format.h"

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/keys.h>

#include <game/client/component.h>

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

class CInputOverlay : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnConsoleInit() override;
	void OnRender() override;
	bool OpenEditor() const;

private:
	struct SObsElement
	{
		std::string m_Id;
		int m_Code = 0;
		int m_Type = QmInputOverlay::ET_TEXTURE;
		int m_ZLevel = 0;
		int m_MouseType = 0;
		int m_WheelDir = 0;
		int m_Side = -1;
		int m_StickRadius = 0;
		int m_MouseRadius = 0;
		int m_Direction = 0;
		bool m_TriggerMode = false;
		bool m_ActiveOnly = false;
		bool m_HasPressedMapping = false;
		int m_Key = KEY_UNKNOWN;
		int m_GamepadButton = -1;
		int m_MouseButton = 0;
		float m_MapX = 0.0f;
		float m_MapY = 0.0f;
		float m_MapW = 0.0f;
		float m_MapH = 0.0f;
		float m_PressedMapX = 0.0f;
		float m_PressedMapY = 0.0f;
		float m_PressedMapW = 0.0f;
		float m_PressedMapH = 0.0f;
		float m_PosX = 0.0f;
		float m_PosY = 0.0f;
	};

	struct SObsLayout
	{
		std::vector<SObsElement> m_vElements;
		IGraphics::CTextureHandle m_Texture;
		int m_TextureWidth = 0;
		int m_TextureHeight = 0;
		float m_OverlayWidth = 0.0f;
		float m_OverlayHeight = 0.0f;
		float m_DefaultWidth = 0.0f;
		float m_DefaultHeight = 0.0f;
		float m_OffsetX = 0.0f;
		float m_OffsetY = 0.0f;
		bool m_IsMouseLayout = false;
		int m_PressedOffsetY = 0;
		bool m_HasPressedOffset = false;
		std::string m_LayoutPath;
		std::string m_ImagePath;
	};

	bool LoadConfiguration(int StorageType);
	bool ParseConfiguration(const void *pFileData, unsigned FileLength);
	bool LoadParsedLayout(const QmInputOverlay::SLayout &Layout, const char *pLayoutPath, const char *pImagePath, float OffsetX, float OffsetY, SObsLayout &Out);
	bool IsObsActive(const SObsElement &Element) const;
	bool IsGamepadButtonPressed(int Button) const;
	float GamepadAxisValue(int Axis) const;
	int GamepadPlayerIndex() const;
	void UpdateInputState();
	float MouseMoveAngle() const;
	void DrawObsElement(const SObsLayout &Layout, const SObsElement &Element, float OriginX, float OriginY, float LayoutScale, float Opacity);
	bool GetConfigModifiedTime(time_t &OutModified) const;
	void ClearObsLayouts();
	static void ConOpenEditor(IConsole::IResult *pResult, void *pUserData);

	std::vector<SObsLayout> m_vObsLayouts;
	float m_CanvasWidth = 320.0f;
	float m_CanvasHeight = 120.0f;
	bool m_ConfigLoaded = false;
	bool m_ConfigValid = false;
	float m_Time = 0.0f;
	float m_ConfigCheckTimer = 0.0f;
	time_t m_ConfigModifiedTime = 0;
	bool m_HasConfigModifiedTime = false;
	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
	float m_MouseDeltaX = 0.0f;
	float m_MouseDeltaY = 0.0f;
	float m_aWheelLastTime[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
	float m_aWheelAlpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	bool m_UsingDemoInputState = false;
	bool m_HasMousePositionSample = false;
	uint64_t m_LastDemoWheelSequence = 0;
};

#endif
