// OBS Input Overlay v5/v5.1 layout format helpers.
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_INPUT_OVERLAY_FORMAT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_INPUT_OVERLAY_FORMAT_H

#include <string>
#include <vector>

namespace QmInputOverlay
{
	inline constexpr int CURRENT_VERSION = 507;
	inline constexpr int MAX_ELEMENTS = 256;
	inline constexpr int MAX_IMAGE_DIMENSION = 16384;
	inline constexpr const char *CONFIGURATION_PATH = "qmclient/InputOverlay/input_overlay.json";
	// 直接布局缺省 image 时使用的默认图片（官方 Zac 键盘预设，相对布局文件目录解析）。
	inline constexpr const char *IMAGE_PATH = "wasd.png";

	enum EElementType
	{
		ET_TEXTURE = 0,
		ET_KEYBOARD_KEY = 1,
		ET_GAMEPAD_BUTTON = 2,
		ET_MOUSE_BUTTON = 3,
		ET_WHEEL = 4,
		ET_ANALOG_STICK = 5,
		ET_TRIGGER = 6,
		ET_GAMEPAD_ID = 7,
		ET_DPAD_STICK = 8,
		ET_MOUSE_MOVEMENT = 9,
	};

	struct SRect
	{
		float m_X = 0.0f;
		float m_Y = 0.0f;
		float m_W = 0.0f;
		float m_H = 0.0f;
	};

	struct SElement
	{
		std::string m_Id;
		int m_Type = ET_TEXTURE;
		int m_Code = 0;
		int m_ZLevel = 0;
		SRect m_Pos;
		SRect m_Mapping;
		SRect m_MappingPress;
		bool m_HasMappingPress = false;
		int m_Side = -1;
		int m_StickRadius = 0;
		int m_MouseRadius = 0;
		int m_MouseType = 0;
		int m_Direction = 0;
		bool m_TriggerMode = false;
		bool m_ActiveOnly = false;
	};

	struct SLayout
	{
		int m_Version = 506;
		bool m_HasVersion = false;
		int m_Flags = 0;
		float m_DefaultWidth = 0.0f;
		float m_DefaultHeight = 0.0f;
		float m_SpaceH = 0.0f;
		float m_SpaceV = 0.0f;
		float m_OverlayWidth = 0.0f;
		float m_OverlayHeight = 0.0f;
		std::string m_ImagePath;
		bool m_HasQmEditorExtension = false;
		std::vector<SElement> m_vElements;
	};

	// Parse one official OBS layout object. Unknown fields are ignored so the
	// OBS reader remains compatible with Qm editor extensions and future OBS fields.
	bool ParseLayout(const void *pData, unsigned DataSize, SLayout &Out, std::string &Error);

	// Convert a keyboard code from the version declared by an OBS layout to the
	// engine's SDL-scancode based key enum. Layouts without version are v5.0.x.
	int KeyboardCodeToEngine(int Code, int Version);

	// Convert an OBS gamepad button code, including v5.0.x ECxx codes, to the
	// SDL2 joystick button index used by QmClient.
	int GamepadCodeToButton(int Code);

	// 将路径中的反斜杠规范化为正斜杠，供跨平台解析 OBS 的 Windows 配置。
	std::string NormalizePathSlashes(const char *pPath);

	// 判断是否为安全的相对路径：拒绝绝对路径、盘符、目录穿越等。
	bool IsSafeRelativePath(const char *pPath);
}

#endif
