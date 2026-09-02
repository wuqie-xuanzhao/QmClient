#include "input_overlay_format.h"

#include <base/str.h>

#include <engine/keys.h>
#include <engine/shared/json.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace QmInputOverlay
{
	namespace
	{
		bool ReadNumber(const json_value &Value, float &Out)
		{
			if(Value.type == json_integer)
				Out = static_cast<float>(Value.u.integer);
			else if(Value.type == json_double)
				Out = static_cast<float>(Value.u.dbl);
			else
				return false;
			return std::isfinite(Out);
		}

		bool ReadInt(const json_value &Value, int &Out)
		{
			if(Value.type == json_integer)
			{
				if(Value.u.integer < std::numeric_limits<int>::min() || Value.u.integer > std::numeric_limits<int>::max())
					return false;
				Out = static_cast<int>(Value.u.integer);
				return true;
			}
			// A number of official presets serialize z_level as a JSON string.
			if(Value.type == json_string && Value.u.string.ptr != nullptr && Value.u.string.length > 0 && Value.u.string.length < 16)
			{
				std::string Text(Value.u.string.ptr, Value.u.string.length);
				char *pEnd = nullptr;
				const long Parsed = std::strtol(Text.c_str(), &pEnd, 10);
				if(pEnd != Text.c_str() && *pEnd == '\0' && Parsed >= std::numeric_limits<int>::min() && Parsed <= std::numeric_limits<int>::max())
				{
					Out = static_cast<int>(Parsed);
					return true;
				}
			}
			return false;
		}

		bool ReadString(const json_value &Value, std::string &Out, size_t MaxLength)
		{
			if(Value.type != json_string || Value.u.string.ptr == nullptr || Value.u.string.length == 0 || Value.u.string.length > MaxLength)
				return false;
			Out.assign(Value.u.string.ptr, Value.u.string.length);
			return str_utf8_check(Out.c_str());
		}

		bool ReadArray(const json_value &Value, float *pOut, unsigned Count)
		{
			if(Value.type != json_array || Value.u.array.length != Count)
				return false;
			for(unsigned i = 0; i < Count; ++i)
				if(!ReadNumber(Value[i], pOut[i]))
					return false;
			return true;
		}

		bool ReadRect(const json_value &Value, SRect &Out, bool PositiveSize)
		{
			float aValues[4];
			if(!ReadArray(Value, aValues, 4))
				return false;
			Out = {aValues[0], aValues[1], aValues[2], aValues[3]};
			if(std::fabs(Out.m_X) > MAX_IMAGE_DIMENSION * 8.0f || std::fabs(Out.m_Y) > MAX_IMAGE_DIMENSION * 8.0f || Out.m_W > MAX_IMAGE_DIMENSION || Out.m_H > MAX_IMAGE_DIMENSION)
				return false;
			return !PositiveSize || (Out.m_W > 0.0f && Out.m_H > 0.0f && Out.m_X >= 0.0f && Out.m_Y >= 0.0f);
		}

		bool ReadOptionalInt(const json_value &Object, const char *pName, int &Out, int Default)
		{
			const json_value &Value = Object[pName];
			if(Value.type == json_none)
			{
				Out = Default;
				return true;
			}
			return ReadInt(Value, Out);
		}

		bool IsValidType(int Type)
		{
			return Type >= ET_TEXTURE && Type <= ET_MOUSE_MOVEMENT;
		}

		// v5.0.x used uiohook virtual-key values. v5.1 uses the USB set-1
		// values below. This table is the official converter's vcto507 map.
		const std::unordered_map<int, int> &LegacyKeyMap()
		{
			static const std::unordered_map<int, int> s_Map = {
				{0x0041, 0x001e},
				{0x0042, 0x0030},
				{0x0043, 0x002e},
				{0x0044, 0x0020},
				{0x0045, 0x0012},
				{0x0046, 0x0021},
				{0x0047, 0x0022},
				{0x0048, 0x0023},
				{0x0049, 0x0017},
				{0x004a, 0x0024},
				{0x004b, 0x0025},
				{0x004c, 0x0026},
				{0x004d, 0x0032},
				{0x004e, 0x0031},
				{0x004f, 0x0018},
				{0x0050, 0x0019},
				{0x0051, 0x0010},
				{0x0052, 0x0013},
				{0x0053, 0x001f},
				{0x0054, 0x0014},
				{0x0055, 0x0016},
				{0x0056, 0x002f},
				{0x0057, 0x0011},
				{0x0058, 0x002d},
				{0x0059, 0x0015},
				{0x005a, 0x002c},
				{0x0030, 0x0002},
				{0x0031, 0x0003},
				{0x0032, 0x0004},
				{0x0033, 0x0005},
				{0x0034, 0x0006},
				{0x0035, 0x0007},
				{0x0036, 0x0008},
				{0x0037, 0x0009},
				{0x0038, 0x000a},
				{0x0039, 0x000b},
				{0x00c0, 0x0029},
				{0x0070, 0x003b},
				{0x0071, 0x003c},
				{0x0072, 0x003d},
				{0x0073, 0x003e},
				{0x0074, 0x003f},
				{0x0075, 0x0040},
				{0x0076, 0x0041},
				{0x0077, 0x0042},
				{0x0078, 0x0043},
				{0x0079, 0x0044},
				{0x007a, 0x0057},
				{0x007b, 0x0058},
				{0xf000, 0x005b},
				{0xf001, 0x005c},
				{0xf002, 0x005d},
				{0xf003, 0x0063},
				{0xf004, 0x0064},
				{0xf005, 0x0065},
				{0xf006, 0x0066},
				{0xf007, 0x0067},
				{0xf008, 0x0068},
				{0xf009, 0x0069},
				{0xf00a, 0x006a},
				{0xf00b, 0x006b},
				{0x001b, 0x0001},
				{0x002d, 0x000c},
				{0x003d, 0x000d},
				{0x0008, 0x000e},
				{0x0009, 0x000f},
				{0x0014, 0x003a},
				{0x005c, 0x001b},
				{0x005b, 0x001a},
				{0x005d, 0x002b},
				{0x003b, 0x0027},
				{0x00de, 0x0028},
				{0x000a, 0x001c},
				{0x002c, 0x0033},
				{0x002e, 0x0034},
				{0x002f, 0x0035},
				{0x0020, 0x0039},
				{0x009a, 0x0e37},
				{0x0091, 0x0046},
				{0x0013, 0x0e45},
				{0x0099, 0x0e46},
				{0x009b, 0x0e52},
				{0x007f, 0x0e53},
				{0x0024, 0x0e47},
				{0x0023, 0x0e4f},
				{0x0021, 0x0e49},
				{0x0022, 0x0e51},
				{0x0026, 0xe048},
				{0x0025, 0xe04b},
				{0x0027, 0xe04d},
				{0x0028, 0xe050},
				{0x007c, 0x0e0d},
				{0x0090, 0x0045},
				{0x006f, 0x0e35},
				{0x006a, 0x0037},
				{0x006d, 0x004a},
				{0x006b, 0x004e},
				{0x007d, 0x0e1c},
				{0x006e, 0x0053},
				{0x0060, 0x004f},
				{0x0061, 0x0050},
				{0x0062, 0x0051},
				{0x0063, 0x004b},
				{0x0064, 0x004c},
				{0x0065, 0x004d},
				{0x0066, 0x0047},
				{0x0067, 0x0048},
				{0x0068, 0x0049},
				{0x0069, 0x0052},
				{0xa010, 0x002a},
				{0xb010, 0x0036},
				{0xa011, 0x001d},
				{0xb011, 0x0e1d},
				{0xa012, 0x0038},
				{0xb012, 0x0e38},
				{0xa09d, 0x0e5b},
				{0xb09d, 0x0e5c},
				{0x020d, 0x0e5d},
			};
			return s_Map;
		}

		int NewCodeToEngine(int Code)
		{
			static const std::unordered_map<int, int> s_Map = {
				{0x01, KEY_ESCAPE},
				{0x02, KEY_1},
				{0x03, KEY_2},
				{0x04, KEY_3},
				{0x05, KEY_4},
				{0x06, KEY_5},
				{0x07, KEY_6},
				{0x08, KEY_7},
				{0x09, KEY_8},
				{0x0a, KEY_9},
				{0x0b, KEY_0},
				{0x0c, KEY_MINUS},
				{0x0d, KEY_EQUALS},
				{0x0e, KEY_BACKSPACE},
				{0x0f, KEY_TAB},
				{0x10, KEY_Q},
				{0x11, KEY_W},
				{0x12, KEY_E},
				{0x13, KEY_R},
				{0x14, KEY_T},
				{0x15, KEY_Y},
				{0x16, KEY_U},
				{0x17, KEY_I},
				{0x18, KEY_O},
				{0x19, KEY_P},
				{0x1a, KEY_LEFTBRACKET},
				{0x1b, KEY_RIGHTBRACKET},
				{0x1c, KEY_RETURN},
				{0x1d, KEY_LCTRL},
				{0x1e, KEY_A},
				{0x1f, KEY_S},
				{0x20, KEY_D},
				{0x21, KEY_F},
				{0x22, KEY_G},
				{0x23, KEY_H},
				{0x24, KEY_J},
				{0x25, KEY_K},
				{0x26, KEY_L},
				{0x27, KEY_SEMICOLON},
				{0x28, KEY_APOSTROPHE},
				{0x29, KEY_GRAVE},
				{0x2a, KEY_LSHIFT},
				{0x2b, KEY_BACKSLASH},
				{0x2c, KEY_Z},
				{0x2d, KEY_X},
				{0x2e, KEY_C},
				{0x2f, KEY_V},
				{0x30, KEY_B},
				{0x31, KEY_N},
				{0x32, KEY_M},
				{0x33, KEY_COMMA},
				{0x34, KEY_PERIOD},
				{0x35, KEY_SLASH},
				{0x36, KEY_RSHIFT},
				{0x37, KEY_KP_MULTIPLY},
				{0x38, KEY_LALT},
				{0x39, KEY_SPACE},
				{0x3a, KEY_CAPSLOCK},
				{0x3b, KEY_F1},
				{0x3c, KEY_F2},
				{0x3d, KEY_F3},
				{0x3e, KEY_F4},
				{0x3f, KEY_F5},
				{0x40, KEY_F6},
				{0x41, KEY_F7},
				{0x42, KEY_F8},
				{0x43, KEY_F9},
				{0x44, KEY_F10},
				{0x45, KEY_NUMLOCKCLEAR},
				{0x46, KEY_SCROLLLOCK},
				{0x47, KEY_KP_7},
				{0x48, KEY_KP_8},
				{0x49, KEY_KP_9},
				{0x4a, KEY_KP_MINUS},
				{0x4b, KEY_KP_4},
				{0x4c, KEY_KP_5},
				{0x4d, KEY_KP_6},
				{0x4e, KEY_KP_PLUS},
				{0x4f, KEY_KP_1},
				{0x50, KEY_KP_2},
				{0x51, KEY_KP_3},
				{0x52, KEY_KP_0},
				{0x53, KEY_KP_PERIOD},
				{0x57, KEY_F11},
				{0x58, KEY_F12},
				{0x5b, KEY_F13},
				{0x5c, KEY_F14},
				{0x5d, KEY_F15},
				{0x63, KEY_F16},
				{0x64, KEY_F17},
				{0x65, KEY_F18},
				{0x66, KEY_F19},
				{0x67, KEY_F20},
				{0x68, KEY_F21},
				{0x69, KEY_F22},
				{0x6a, KEY_F23},
				{0x6b, KEY_F24},
				{0x0e37, KEY_PRINTSCREEN},
				{0x0e45, KEY_PAUSE},
				{0x0e52, KEY_INSERT},
				{0x0e53, KEY_DELETE},
				{0x0e47, KEY_HOME},
				{0x0e4f, KEY_END},
				{0x0e49, KEY_PAGEUP},
				{0x0e51, KEY_PAGEDOWN},
				{0xe048, KEY_UP},
				{0xe04b, KEY_LEFT},
				{0xe04d, KEY_RIGHT},
				{0xe050, KEY_DOWN},
				{0x0e0d, KEY_KP_EQUALS},
				{0x0e35, KEY_KP_DIVIDE},
				{0x0e1c, KEY_KP_ENTER},
				{0x0e5b, KEY_LGUI},
				{0x0e5c, KEY_RGUI},
				{0x0e5d, KEY_MENU},
			};
			const auto It = s_Map.find(Code);
			return It == s_Map.end() ? KEY_UNKNOWN : It->second;
		}

		bool ParseElement(const json_value &Value, int Version, SElement &Out, std::unordered_set<std::string> &Ids, std::string &Error)
		{
			if(Value.type != json_object || !ReadString(Value["id"], Out.m_Id, 256) || !Ids.insert(Out.m_Id).second)
			{
				Error = "element id is missing or duplicated";
				return false;
			}
			if(!ReadInt(Value["type"], Out.m_Type) || !IsValidType(Out.m_Type))
			{
				Error = "element type is outside 0..9";
				return false;
			}
			float Pos[2];
			if(!ReadArray(Value["pos"], Pos, 2))
			{
				Error = "element pos must contain two finite numbers";
				return false;
			}
			Out.m_Pos = {Pos[0], Pos[1], 0.0f, 0.0f};
			if(!ReadRect(Value["mapping"], Out.m_Mapping, true))
			{
				Error = "element mapping is invalid";
				return false;
			}
			if(Value["mapping_press"].type != json_none)
			{
				if(!ReadRect(Value["mapping_press"], Out.m_MappingPress, true))
				{
					Error = "element mapping_press is invalid";
					return false;
				}
				Out.m_HasMappingPress = true;
			}
			if(!ReadOptionalInt(Value, "z_level", Out.m_ZLevel, 0) || !ReadOptionalInt(Value, "code", Out.m_Code, 0))
			{
				Error = "element integer field is invalid";
				return false;
			}
			if(Value["side"].type != json_none && (!ReadInt(Value["side"], Out.m_Side) || Out.m_Side < 0 || Out.m_Side > 1))
			{
				Error = "element side is invalid";
				return false;
			}
			if(Value["stick_radius"].type != json_none && (!ReadInt(Value["stick_radius"], Out.m_StickRadius) || Out.m_StickRadius < 0 || Out.m_StickRadius > MAX_IMAGE_DIMENSION))
			{
				Error = "element stick_radius is invalid";
				return false;
			}
			if(Value["mouse_radius"].type != json_none && (!ReadInt(Value["mouse_radius"], Out.m_MouseRadius) || Out.m_MouseRadius < 0 || Out.m_MouseRadius > MAX_IMAGE_DIMENSION))
			{
				Error = "element mouse_radius is invalid";
				return false;
			}
			if(Value["mouse_type"].type != json_none && (!ReadInt(Value["mouse_type"], Out.m_MouseType) || Out.m_MouseType < 0 || Out.m_MouseType > 1))
			{
				Error = "element mouse_type is invalid";
				return false;
			}
			if(Value["direction"].type != json_none && (!ReadInt(Value["direction"], Out.m_Direction) || Out.m_Direction < 0 || Out.m_Direction > 5))
			{
				Error = "element direction is invalid";
				return false;
			}
			if(Value["trigger_mode"].type != json_none && Value["trigger_mode"].type != json_boolean)
			{
				Error = "element trigger_mode is invalid";
				return false;
			}
			if(Value["trigger_mode"].type == json_boolean)
				Out.m_TriggerMode = Value["trigger_mode"].u.boolean != 0;
			if(Value["active_only"].type != json_none && Value["active_only"].type != json_boolean)
			{
				Error = "element active_only is invalid";
				return false;
			}
			if(Value["active_only"].type == json_boolean)
				Out.m_ActiveOnly = Value["active_only"].u.boolean != 0;
			return true;
		}
	}

	std::string NormalizePathSlashes(const char *pPath)
	{
		if(pPath == nullptr)
			return {};
		std::string Out(pPath);
		for(char &c : Out)
			if(c == '\\')
				c = '/';
		return Out;
	}

	bool IsSafeRelativePath(const char *pPath)
	{
		if(pPath == nullptr || pPath[0] == '\0' || pPath[0] == '/' || pPath[0] == '\\' || str_find(pPath, ":") != nullptr)
			return false;
		const size_t Length = str_length(pPath);
		if(Length > 240 || pPath[Length - 1] == '/' || pPath[Length - 1] == '\\')
			return false;
		// 先安全规范化反斜杠，再逐段检查目录穿越，兼容官方 Windows 配置。
		const std::string Normalized = NormalizePathSlashes(pPath);
		size_t Start = 0;
		while(true)
		{
			const size_t Slash = Normalized.find('/', Start);
			const std::string Component = Slash == std::string::npos ? Normalized.substr(Start) : Normalized.substr(Start, Slash - Start);
			if(Component.empty() || Component == "." || Component == "..")
				return false;
			if(Slash == std::string::npos)
				break;
			Start = Slash + 1;
		}
		return str_utf8_check(pPath);
	}

	int KeyboardCodeToEngine(int Code, int Version)
	{
		if(Version < CURRENT_VERSION)
		{
			const auto It = LegacyKeyMap().find(Code);
			if(It != LegacyKeyMap().end())
				Code = It->second;
		}
		return NewCodeToEngine(Code);
	}

	int GamepadCodeToButton(int Code)
	{
		if(Code >= 0 && Code <= 16)
			return Code;
		if(Code >= 0xec00 && Code <= 0xec10)
		{
			static const int s_aLegacyToSdl[] = {0, 1, 2, 3, 9, 10, 4, 6, 5, 7, 8, 11, 12, 13, 14, 15, 16};
			const int Index = Code - 0xec00;
			return Index < static_cast<int>(sizeof(s_aLegacyToSdl) / sizeof(s_aLegacyToSdl[0])) ? s_aLegacyToSdl[Index] : -1;
		}
		return -1;
	}

	bool ParseLayout(const void *pData, unsigned DataSize, SLayout &Out, std::string &Error)
	{
		Out = {};
		if(pData == nullptr || DataSize == 0 || DataSize > 16U * 1024U * 1024U)
		{
			Error = "layout is empty or oversized";
			return false;
		}
		json_settings Settings{};
		Settings.settings = json_enable_comments;
		char aJsonError[256];
		json_value *pRoot = JsonParseEx(&Settings, static_cast<const json_char *>(pData), DataSize, aJsonError);
		if(pRoot == nullptr)
		{
			Error = std::string("invalid layout JSON: ") + aJsonError;
			return false;
		}
		auto Fail = [&](const char *pMessage) {
			Error = pMessage;
			json_value_free(pRoot);
			return false;
		};
		if(pRoot->type != json_object)
			return Fail("layout root must be an object");
		const json_value &Root = *pRoot;
		if(Root["format"].type == json_string &&
			(str_comp_nocase(Root["format"].u.string.ptr, "qm_input_overlay") == 0 || str_comp_nocase(Root["format"].u.string.ptr, "input_overlay_v3") == 0))
			return Fail("legacy QmClient input overlay profiles are not supported");
		if(Root["version"].type != json_none)
		{
			if(!ReadInt(Root["version"], Out.m_Version) || Out.m_Version < 500 || Out.m_Version > CURRENT_VERSION)
				return Fail("unsupported OBS layout version");
			Out.m_HasVersion = true;
		}
		if(!ReadOptionalInt(Root, "flags", Out.m_Flags, 0) || Out.m_Flags < 0 || Out.m_Flags > 255)
			return Fail("layout flags are invalid");
		if(!ReadNumber(Root["default_width"], Out.m_DefaultWidth) && Root["default_width"].type != json_none)
			return Fail("default_width is invalid");
		if(!ReadNumber(Root["default_height"], Out.m_DefaultHeight) && Root["default_height"].type != json_none)
			return Fail("default_height is invalid");
		if(!ReadNumber(Root["space_h"], Out.m_SpaceH) && Root["space_h"].type != json_none)
			return Fail("space_h is invalid");
		if(!ReadNumber(Root["space_v"], Out.m_SpaceV) && Root["space_v"].type != json_none)
			return Fail("space_v is invalid");
		if(!ReadNumber(Root["overlay_width"], Out.m_OverlayWidth) && Root["overlay_width"].type != json_none)
			return Fail("overlay_width is invalid");
		if(!ReadNumber(Root["overlay_height"], Out.m_OverlayHeight) && Root["overlay_height"].type != json_none)
			return Fail("overlay_height is invalid");
		if(Out.m_DefaultWidth < 0.0f || Out.m_DefaultHeight < 0.0f || Out.m_OverlayWidth < 0.0f || Out.m_OverlayHeight < 0.0f || Out.m_DefaultWidth > MAX_IMAGE_DIMENSION || Out.m_DefaultHeight > MAX_IMAGE_DIMENSION || Out.m_OverlayWidth > MAX_IMAGE_DIMENSION || Out.m_OverlayHeight > MAX_IMAGE_DIMENSION)
			return Fail("layout dimensions are invalid");
		if(Root["image"].type != json_none)
		{
			if(!ReadString(Root["image"], Out.m_ImagePath, 240) || !IsSafeRelativePath(Out.m_ImagePath.c_str()) || str_endswith_nocase(Out.m_ImagePath.c_str(), ".png") == nullptr)
				return Fail("layout image path is invalid");
			// 统一为正斜杠，便于后续按目录拼接与跨平台读取。
			Out.m_ImagePath = NormalizePathSlashes(Out.m_ImagePath.c_str());
		}
		const json_value &Elements = Root["elements"];
		if(Elements.type != json_array || Elements.u.array.length == 0 || Elements.u.array.length > MAX_ELEMENTS)
			return Fail("elements must contain 1 to 256 entries");
		std::unordered_set<std::string> Ids;
		for(unsigned i = 0; i < Elements.u.array.length; ++i)
		{
			SElement Element;
			if(!ParseElement(Elements[i], Out.m_Version, Element, Ids, Error))
			{
				json_value_free(pRoot);
				return false;
			}
			Out.m_vElements.push_back(std::move(Element));
		}
		if(Out.m_OverlayWidth <= 0.0f || Out.m_OverlayHeight <= 0.0f)
		{
			for(const SElement &Element : Out.m_vElements)
			{
				Out.m_OverlayWidth = std::max(Out.m_OverlayWidth, Element.m_Pos.m_X + Element.m_Mapping.m_W);
				Out.m_OverlayHeight = std::max(Out.m_OverlayHeight, Element.m_Pos.m_Y + Element.m_Mapping.m_H);
			}
		}
		if(Out.m_OverlayWidth <= 0.0f || Out.m_OverlayHeight <= 0.0f)
			return Fail("overlay dimensions cannot be inferred");
		Out.m_HasQmEditorExtension = Root["_qm_editor"].type != json_none;
		std::stable_sort(Out.m_vElements.begin(), Out.m_vElements.end(), [](const SElement &A, const SElement &B) { return A.m_ZLevel < B.m_ZLevel; });
		json_value_free(pRoot);
		return true;
	}
}
