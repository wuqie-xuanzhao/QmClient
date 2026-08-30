// OBS Input Overlay v5/v5.1 runtime for QmClient.
#include "input_overlay.h"

#include <base/fs.h>
#include <base/log.h>
#include <base/str.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/input.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>

namespace
{
	static constexpr int OBS_INNER_BORDER = 3;
	static constexpr float WHEEL_HOLD_TIME = 0.15f;

	bool DemoInputKeyIsPressed(const CGameClient::SDemoInputPlaybackState *pState, int Key)
	{
		if(pState == nullptr || Key < KEY_FIRST || Key >= KEY_LAST)
			return false;
		return (pState->m_aKeyStates[Key >> 3] & (1U << (Key & 7))) != 0;
	}

	bool ReadJsonNumber(const json_value &Value, float &Out)
	{
		if(Value.type == json_integer)
			Out = static_cast<float>(Value.u.integer);
		else if(Value.type == json_double)
			Out = static_cast<float>(Value.u.dbl);
		else
			return false;
		return std::isfinite(Out);
	}

	bool ReadJsonOffset(const json_value &Object, float &OutX, float &OutY)
	{
		const json_value &Offset = Object["offset"];
		float Values[2];
		if(Offset.type == json_array && Offset.u.array.length == 2 && ReadJsonNumber(Offset[0], Values[0]) && ReadJsonNumber(Offset[1], Values[1]))
		{
			OutX = Values[0];
			OutY = Values[1];
			return true;
		}
		if(Offset.type == json_object && ReadJsonNumber(Offset["x"], OutX) && ReadJsonNumber(Offset["y"], OutY))
			return true;
		const bool HasX = ReadJsonNumber(Object["offset_x"], OutX);
		const bool HasY = ReadJsonNumber(Object["offset_y"], OutY);
		return HasX || HasY;
	}

	std::string ReplaceExtension(const std::string &Path, const char *pExtension)
	{
		const size_t Slash = Path.find_last_of('/');
		const size_t Dot = Path.find_last_of('.');
		if(Dot == std::string::npos || (Slash != std::string::npos && Dot < Slash))
			return Path + pExtension;
		return Path.substr(0, Dot) + pExtension;
	}

	std::string JoinPath(const char *pBasePath, const char *pRelativePath)
	{
		if(pBasePath == nullptr || pRelativePath == nullptr || pBasePath[0] == '\0' || pRelativePath[0] == '\0')
			return {};
		const std::string BasePath = pBasePath;
		const size_t Slash = BasePath.find_last_of('/');
		if(Slash == std::string::npos)
			return pRelativePath;
		return BasePath.substr(0, Slash + 1) + pRelativePath;
	}

	void AddPathCandidate(std::vector<std::string> &vCandidates, const std::string &Path)
	{
		if(Path.empty() || !QmInputOverlay::IsSafeRelativePath(Path.c_str()))
			return;
		if(std::find(vCandidates.begin(), vCandidates.end(), Path) == vCandidates.end())
			vCandidates.push_back(Path);
	}

	int MouseButtonFromCode(int Code)
	{
		// OBS uses left, right, middle, X1, X2 as 1, 2, 3, 4, 5.
		switch(Code)
		{
		case 1: return 1;
		case 2: return 3;
		case 3: return 2;
		case 4: return 4;
		case 5: return 5;
		case 6: return 6;
		case 7: return 7;
		case 8: return 8;
		case 9: return 9;
		default: return 0;
		}
	}

	int MouseButtonFromId(const std::string &Id)
	{
		if(str_comp_nocase(Id.c_str(), "lmb") == 0 || str_comp_nocase(Id.c_str(), "mouse1") == 0 || str_comp_nocase(Id.c_str(), "mouse_left") == 0)
			return 1;
		if(str_comp_nocase(Id.c_str(), "rmb") == 0 || str_comp_nocase(Id.c_str(), "mouse2") == 0 || str_comp_nocase(Id.c_str(), "mouse_right") == 0)
			return 3;
		if(str_comp_nocase(Id.c_str(), "mmb") == 0 || str_comp_nocase(Id.c_str(), "mouse3") == 0 || str_comp_nocase(Id.c_str(), "mouse_middle") == 0)
			return 2;
		if(str_comp_nocase(Id.c_str(), "smb1") == 0 || str_comp_nocase(Id.c_str(), "mb4") == 0 || str_comp_nocase(Id.c_str(), "mouse4") == 0)
			return 4;
		if(str_comp_nocase(Id.c_str(), "smb2") == 0 || str_comp_nocase(Id.c_str(), "mb5") == 0 || str_comp_nocase(Id.c_str(), "mouse5") == 0)
			return 5;
		return 0;
	}

	int WheelDirectionFromId(const std::string &Id)
	{
		if(str_comp_nocase(Id.c_str(), "wheel_up") == 0 || str_comp_nocase(Id.c_str(), "wheelup") == 0 || str_comp_nocase(Id.c_str(), "mousewheelup") == 0 || str_comp_nocase(Id.c_str(), "mwheelup") == 0)
			return 1;
		if(str_comp_nocase(Id.c_str(), "wheel_down") == 0 || str_comp_nocase(Id.c_str(), "wheeldown") == 0 || str_comp_nocase(Id.c_str(), "mousewheeldown") == 0 || str_comp_nocase(Id.c_str(), "mwheeldown") == 0)
			return 2;
		if(str_comp_nocase(Id.c_str(), "wheel_left") == 0 || str_comp_nocase(Id.c_str(), "wheelleft") == 0 || str_comp_nocase(Id.c_str(), "mousewheelleft") == 0 || str_comp_nocase(Id.c_str(), "mwheelleft") == 0)
			return 3;
		if(str_comp_nocase(Id.c_str(), "wheel_right") == 0 || str_comp_nocase(Id.c_str(), "wheelright") == 0 || str_comp_nocase(Id.c_str(), "mousewheelright") == 0 || str_comp_nocase(Id.c_str(), "mwheelright") == 0)
			return 4;
		return 0;
	}

	bool MakeAbsolutePath(const char *pPath, char *pBuffer, int BufferSize)
	{
		if(pPath == nullptr || pPath[0] == '\0' || pBuffer == nullptr || BufferSize <= 0)
			return false;
		if(!fs_is_relative_path(pPath))
		{
			str_copy(pBuffer, pPath, BufferSize);
			return pBuffer[0] != '\0';
		}
		char aWorkingDirectory[IO_MAX_PATH_LENGTH];
		if(!fs_getcwd(aWorkingDirectory, sizeof(aWorkingDirectory)))
			return false;
		str_format(pBuffer, BufferSize, "%s/%s", aWorkingDirectory, pPath);
		return pBuffer[0] != '\0';
	}
}

void CInputOverlay::OnInit()
{
	m_Time = 0.0f;
	m_ConfigCheckTimer = 0.0f;
	m_ConfigLoaded = false;
	m_ConfigValid = false;
	m_UsingDemoInputState = false;
	m_HasMousePositionSample = false;
	m_LastDemoWheelSequence = 0;
	LoadConfiguration(IStorage::TYPE_ALL);
	const vec2 MousePos = Input()->GlobalMousePos();
	m_LastMouseX = MousePos.x;
	m_LastMouseY = MousePos.y;
	m_HasMousePositionSample = true;
	time_t Modified;
	if(GetConfigModifiedTime(Modified))
	{
		m_ConfigModifiedTime = Modified;
		m_HasConfigModifiedTime = true;
	}
}

void CInputOverlay::OnShutdown()
{
	ClearObsLayouts();
	m_ConfigLoaded = false;
	m_ConfigValid = false;
}

bool CInputOverlay::LoadConfiguration(int StorageType)
{
	void *pFileData = nullptr;
	unsigned FileLength = 0;
	if(!Storage()->ReadFile(QmInputOverlay::CONFIGURATION_PATH, StorageType, &pFileData, &FileLength))
	{
		log_error("input_overlay", "Failed to read configuration from '%s'", QmInputOverlay::CONFIGURATION_PATH);
		ClearObsLayouts();
		m_ConfigLoaded = true;
		m_ConfigValid = false;
		return false;
	}
	const bool Result = ParseConfiguration(pFileData, FileLength);
	free(pFileData);
	m_ConfigLoaded = true;
	m_ConfigValid = Result;
	return Result;
}

bool CInputOverlay::ParseConfiguration(const void *pFileData, unsigned FileLength)
{
	QmInputOverlay::SLayout DirectLayout;
	std::string Error;
	if(QmInputOverlay::ParseLayout(pFileData, FileLength, DirectLayout, Error))
	{
		SObsLayout Parsed;
		const char *pImagePath = DirectLayout.m_ImagePath.empty() ? QmInputOverlay::IMAGE_PATH : DirectLayout.m_ImagePath.c_str();
		if(!LoadParsedLayout(DirectLayout, QmInputOverlay::CONFIGURATION_PATH, pImagePath, 0.0f, 0.0f, Parsed))
		{
			ClearObsLayouts();
			return false;
		}
		ClearObsLayouts();
		m_vObsLayouts.emplace_back(std::move(Parsed));
		m_CanvasWidth = DirectLayout.m_OverlayWidth;
		m_CanvasHeight = DirectLayout.m_OverlayHeight;
		m_HasConfigModifiedTime = false;
		return true;
	}

	// OBS 5.0.x also shipped a small wrapper that composes several layouts,
	// and forked presets use a root-level "layout" string pointing at one
	// layout file. Both are accepted only for official layout objects, never
	// for Qm v3 profiles.
	json_settings Settings{};
	Settings.settings = json_enable_comments;
	char aJsonError[256];
	json_value *pRoot = JsonParseEx(&Settings, static_cast<const json_char *>(pFileData), FileLength, aJsonError);
	if(pRoot == nullptr || pRoot->type != json_object)
	{
		if(pRoot != nullptr)
			json_value_free(pRoot);
		log_error("input_overlay", "Failed to parse OBS configuration: %s", Error.c_str());
		ClearObsLayouts();
		return false;
	}
	const json_value &Root = *pRoot;
	if(Root["format"].type == json_string && (str_comp_nocase(Root["format"].u.string.ptr, "qm_input_overlay") == 0 || str_comp_nocase(Root["format"].u.string.ptr, "input_overlay_v3") == 0))
	{
		json_value_free(pRoot);
		ClearObsLayouts();
		log_error("input_overlay", "Legacy QmClient input overlay profiles are not supported");
		return false;
	}
	const bool HasLayouts = Root["layouts"].type == json_array && Root["layouts"].u.array.length > 0;
	const bool HasRootLayout = Root["layout"].type == json_string;
	if(!HasLayouts && !HasRootLayout)
	{
		json_value_free(pRoot);
		ClearObsLayouts();
		log_error("input_overlay", "Failed to parse OBS configuration: no layouts array or root layout string found");
		return false;
	}

	float RootOffsetX = 0.0f;
	float RootOffsetY = 0.0f;
	ReadJsonOffset(Root, RootOffsetX, RootOffsetY);
	float RootPressedOffset = 0.0f;
	const bool HasRootPressedOffset = ReadJsonNumber(Root["pressed_offset_y"], RootPressedOffset);

	// 加载一个 wrapper 条目（layouts[] 条目或根级 "layout" wrapper 本身）。
	const auto LoadEntry = [&](const json_value &Entry, float OffsetX, float OffsetY, SObsLayout &Out, std::string &EntryError) -> bool {
		if(Entry["layout"].type != json_string || !QmInputOverlay::IsSafeRelativePath(Entry["layout"].u.string.ptr))
		{
			EntryError = "layout entry must be a safe relative string path";
			return false;
		}
		const std::string LayoutPath = QmInputOverlay::NormalizePathSlashes(Entry["layout"].u.string.ptr);
		std::vector<std::string> LayoutCandidates;
		AddPathCandidate(LayoutCandidates, JoinPath(QmInputOverlay::CONFIGURATION_PATH, LayoutPath.c_str()));
		AddPathCandidate(LayoutCandidates, LayoutPath);
		void *pLayoutData = nullptr;
		unsigned LayoutLength = 0;
		std::string ResolvedLayoutPath;
		for(const std::string &Candidate : LayoutCandidates)
		{
			if(Storage()->ReadFile(Candidate.c_str(), IStorage::TYPE_ALL, &pLayoutData, &LayoutLength))
			{
				ResolvedLayoutPath = Candidate;
				break;
			}
		}
		if(ResolvedLayoutPath.empty())
		{
			EntryError = "layout file was not found";
			return false;
		}
		QmInputOverlay::SLayout Layout;
		std::string LayoutError;
		const bool LayoutValid = QmInputOverlay::ParseLayout(pLayoutData, LayoutLength, Layout, LayoutError);
		free(pLayoutData);
		if(!LayoutValid)
		{
			EntryError = LayoutError;
			return false;
		}
		ReadJsonOffset(Entry, OffsetX, OffsetY);
		std::string ImagePath;
		if(Entry["image"].type == json_string)
			ImagePath = QmInputOverlay::NormalizePathSlashes(Entry["image"].u.string.ptr);
		else if(!Layout.m_ImagePath.empty())
			ImagePath = Layout.m_ImagePath;
		else
			ImagePath = ReplaceExtension(LayoutPath, ".png");
		if(!QmInputOverlay::IsSafeRelativePath(ImagePath.c_str()) || str_endswith_nocase(ImagePath.c_str(), ".png") == nullptr)
		{
			EntryError = "image path is invalid";
			return false;
		}
		if(!LoadParsedLayout(Layout, ResolvedLayoutPath.c_str(), ImagePath.c_str(), OffsetX, OffsetY, Out))
		{
			EntryError = "failed to load layout image or texture";
			return false;
		}
		float PressedOffset = 0.0f;
		if(ReadJsonNumber(Entry["pressed_offset_y"], PressedOffset))
		{
			Out.m_PressedOffsetY = std::max(0, static_cast<int>(std::round(PressedOffset)));
			Out.m_HasPressedOffset = Out.m_PressedOffsetY > 0;
		}
		else if(HasRootPressedOffset)
		{
			Out.m_PressedOffsetY = std::max(0, static_cast<int>(std::round(RootPressedOffset)));
			Out.m_HasPressedOffset = Out.m_PressedOffsetY > 0;
		}
		return true;
	};

	std::vector<SObsLayout> ParsedLayouts;
	const auto FailWrapper = [&](const char *pContext, const std::string &EntryError) {
		json_value_free(pRoot);
		ClearObsLayouts();
		log_error("input_overlay", "%s: %s", pContext, EntryError.c_str());
		return false;
	};
	if(HasLayouts)
	{
		ParsedLayouts.reserve(Root["layouts"].u.array.length);
		for(unsigned i = 0; i < Root["layouts"].u.array.length; ++i)
		{
			const json_value &Entry = Root["layouts"][i];
			if(Entry.type != json_object)
				return FailWrapper("Invalid OBS layout wrapper entry", "entry " + std::to_string(i) + " is not an object");
			SObsLayout Parsed;
			std::string EntryError;
			if(!LoadEntry(Entry, RootOffsetX, RootOffsetY, Parsed, EntryError))
				return FailWrapper("Failed to load OBS layout", "entry " + std::to_string(i) + ": " + EntryError);
			ParsedLayouts.emplace_back(std::move(Parsed));
		}
	}
	else
	{
		// 根级 "layout" 只支持字符串路径，内嵌对象形式不支持并给出明确错误。
		SObsLayout Parsed;
		std::string EntryError;
		if(!LoadEntry(Root, 0.0f, 0.0f, Parsed, EntryError))
			return FailWrapper("Failed to load root OBS layout", EntryError);
		ParsedLayouts.emplace_back(std::move(Parsed));
	}
	json_value_free(pRoot);
	if(ParsedLayouts.empty())
	{
		ClearObsLayouts();
		return false;
	}
	ClearObsLayouts();
	m_vObsLayouts = std::move(ParsedLayouts);
	m_CanvasWidth = 0.0f;
	m_CanvasHeight = 0.0f;
	for(const SObsLayout &Layout : m_vObsLayouts)
	{
		m_CanvasWidth = std::max(m_CanvasWidth, Layout.m_OffsetX + Layout.m_OverlayWidth);
		m_CanvasHeight = std::max(m_CanvasHeight, Layout.m_OffsetY + Layout.m_OverlayHeight);
	}
	m_HasConfigModifiedTime = false;
	return true;
}

bool CInputOverlay::LoadParsedLayout(const QmInputOverlay::SLayout &Layout, const char *pLayoutPath, const char *pImagePath, float OffsetX, float OffsetY, SObsLayout &Out)
{
	if(pImagePath == nullptr || !QmInputOverlay::IsSafeRelativePath(pImagePath) || str_endswith_nocase(pImagePath, ".png") == nullptr)
		return false;
	std::vector<std::string> ImageCandidates;
	// OBS stores image names relative to the selected layout in several
	// official and forked presets. Keep the original path as a fallback for
	// wrappers that already provide a workspace-relative path.
	AddPathCandidate(ImageCandidates, JoinPath(pLayoutPath, pImagePath));
	AddPathCandidate(ImageCandidates, pImagePath);
	std::string ResolvedImagePath;
	// CImageInfo 是 move-only 类型：Image = {} 触发移动赋值，会先 Free 旧数据
	// 再接管空对象，失败候选不会泄漏，成功候选随后由 LoadTextureRawMove 接管。
	CImageInfo Image;
	for(const std::string &Candidate : ImageCandidates)
	{
		Image = {};
		if(!Graphics()->LoadPng(Image, Candidate.c_str(), IStorage::TYPE_ALL))
			continue;
		if(Image.m_Width == 0 || Image.m_Height == 0 || Image.m_Width > QmInputOverlay::MAX_IMAGE_DIMENSION || Image.m_Height > QmInputOverlay::MAX_IMAGE_DIMENSION)
		{
			Image.Free();
			continue;
		}
		ResolvedImagePath = Candidate;
		break;
	}
	if(ResolvedImagePath.empty())
	{
		log_error("input_overlay", "Failed to load OBS overlay image '%s'", pImagePath);
		return false;
	}
	const int TextureWidth = Image.m_Width;
	const int TextureHeight = Image.m_Height;
	IGraphics::CTextureHandle Texture = Graphics()->LoadTextureRawMove(Image, IGraphics::TEXLOAD_NO_MIPMAPS, ResolvedImagePath.c_str());
	if(!Texture.IsValid())
	{
		log_error("input_overlay", "Failed to create texture for '%s'", ResolvedImagePath.c_str());
		return false;
	}

	Out = {};
	Out.m_Texture = Texture;
	Out.m_TextureWidth = TextureWidth;
	Out.m_TextureHeight = TextureHeight;
	Out.m_OverlayWidth = Layout.m_OverlayWidth;
	Out.m_OverlayHeight = Layout.m_OverlayHeight;
	Out.m_DefaultWidth = Layout.m_DefaultWidth;
	Out.m_DefaultHeight = Layout.m_DefaultHeight;
	Out.m_OffsetX = OffsetX;
	Out.m_OffsetY = OffsetY;
	Out.m_LayoutPath = pLayoutPath != nullptr ? pLayoutPath : QmInputOverlay::CONFIGURATION_PATH;
	Out.m_ImagePath = ResolvedImagePath;
	bool HasKeyboard = false;
	bool HasMouse = false;
	Out.m_vElements.reserve(Layout.m_vElements.size());
	for(const QmInputOverlay::SElement &Source : Layout.m_vElements)
	{
		SObsElement Element;
		Element.m_Id = Source.m_Id;
		Element.m_Code = Source.m_Code;
		Element.m_Type = Source.m_Type;
		Element.m_ZLevel = Source.m_ZLevel;
		Element.m_MouseType = Source.m_MouseType;
		Element.m_Side = Source.m_Side;
		Element.m_StickRadius = Source.m_StickRadius;
		Element.m_MouseRadius = Source.m_MouseRadius;
		Element.m_Direction = Source.m_Direction;
		Element.m_TriggerMode = Source.m_TriggerMode;
		Element.m_ActiveOnly = Source.m_ActiveOnly;
		Element.m_HasPressedMapping = Source.m_HasMappingPress;
		Element.m_MapX = Source.m_Mapping.m_X;
		Element.m_MapY = Source.m_Mapping.m_Y;
		Element.m_MapW = Source.m_Mapping.m_W;
		Element.m_MapH = Source.m_Mapping.m_H;
		Element.m_PressedMapX = Source.m_MappingPress.m_X;
		Element.m_PressedMapY = Source.m_MappingPress.m_Y;
		Element.m_PressedMapW = Source.m_MappingPress.m_W;
		Element.m_PressedMapH = Source.m_MappingPress.m_H;
		Element.m_PosX = Source.m_Pos.m_X;
		Element.m_PosY = Source.m_Pos.m_Y;
		if(Element.m_Type == QmInputOverlay::ET_KEYBOARD_KEY)
		{
			Element.m_Key = QmInputOverlay::KeyboardCodeToEngine(Element.m_Code, Layout.m_Version);
			HasKeyboard = true;
		}
		else if(Element.m_Type == QmInputOverlay::ET_GAMEPAD_BUTTON)
		{
			Element.m_GamepadButton = QmInputOverlay::GamepadCodeToButton(Element.m_Code);
		}
		else if(Element.m_Type == QmInputOverlay::ET_MOUSE_BUTTON)
		{
			Element.m_MouseButton = MouseButtonFromCode(Element.m_Code);
			if(Element.m_MouseButton == 0)
				Element.m_MouseButton = MouseButtonFromId(Element.m_Id);
			HasMouse = true;
		}
		else if(Element.m_Type == QmInputOverlay::ET_WHEEL)
		{
			Element.m_WheelDir = WheelDirectionFromId(Element.m_Id);
			HasMouse = true;
		}
		else if(Element.m_Type == QmInputOverlay::ET_MOUSE_MOVEMENT)
		{
			HasMouse = true;
		}
		if(Element.m_Type == QmInputOverlay::ET_GAMEPAD_BUTTON && Element.m_GamepadButton < 0)
			Element.m_ActiveOnly = true;
		if(Element.m_Type == QmInputOverlay::ET_ANALOG_STICK || Element.m_Type == QmInputOverlay::ET_TRIGGER || Element.m_Type == QmInputOverlay::ET_GAMEPAD_ID || Element.m_Type == QmInputOverlay::ET_DPAD_STICK)
			Element.m_ActiveOnly = false;
		Out.m_vElements.emplace_back(std::move(Element));
	}
	Out.m_IsMouseLayout = HasMouse && !HasKeyboard;
	return !Out.m_vElements.empty();
}

void CInputOverlay::ClearObsLayouts()
{
	for(SObsLayout &Layout : m_vObsLayouts)
	{
		if(Layout.m_Texture.IsValid())
			Graphics()->UnloadTexture(&Layout.m_Texture);
	}
	m_vObsLayouts.clear();
}

bool CInputOverlay::IsGamepadButtonPressed(int Button) const
{
	if(Button < 0 || Button >= NUM_JOYSTICK_BUTTONS)
		return false;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		// 回放期间只使用 Demo 扩展消息中的手柄状态；旧 Demo 没有该消息时
		// 一律视为释放，绝不回退读取当前实时手柄。
		const CGameClient::SDemoInputPlaybackState *pDemo = GameClient()->DemoInputPlaybackState();
		return pDemo != nullptr && pDemo->m_GamepadValid && (pDemo->m_GamepadButtons & (1U << Button)) != 0;
	}
	if(Input()->GetActiveJoystick() == nullptr)
		return false;
	return Input()->GamepadButtonIsPressed(Button);
}

float CInputOverlay::GamepadAxisValue(int Axis) const
{
	if(Axis < 0 || Axis >= 6)
		return 0.0f;
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		// 回放期间轴数据只来自 Demo 扩展消息；缺失时一律居中。
		const CGameClient::SDemoInputPlaybackState *pDemo = GameClient()->DemoInputPlaybackState();
		if(pDemo == nullptr || !pDemo->m_GamepadValid)
			return 0.0f;
		return std::clamp(pDemo->m_aGamepadAxes[Axis], -1.0f, 1.0f);
	}
	return std::clamp(Input()->GamepadAxisValue(Axis), -1.0f, 1.0f);
}

int CInputOverlay::GamepadPlayerIndex() const
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		// 回放期间玩家编号只来自 Demo 扩展消息；缺失时按 0 号玩家处理。
		const CGameClient::SDemoInputPlaybackState *pDemo = GameClient()->DemoInputPlaybackState();
		if(pDemo == nullptr || !pDemo->m_GamepadValid)
			return 0;
		return std::clamp(pDemo->m_GamepadPlayerIndex, 0, 2);
	}
	return std::clamp(Input()->GamepadPlayerIndex(), 0, 2);
}

void CInputOverlay::UpdateInputState()
{
	const bool DemoPlayback = Client()->State() == IClient::STATE_DEMOPLAYBACK;
	const CGameClient::SDemoInputPlaybackState *pDemo = DemoPlayback ? GameClient()->DemoInputPlaybackState() : nullptr;
	const bool UseDemo = DemoPlayback;
	const vec2 MousePos = UseDemo ? (pDemo != nullptr ? vec2((float)pDemo->m_TargetX, (float)pDemo->m_TargetY) : vec2(0.0f, 0.0f)) : Input()->GlobalMousePos();
	if(UseDemo != m_UsingDemoInputState || !m_HasMousePositionSample)
	{
		m_MouseDeltaX = 0.0f;
		m_MouseDeltaY = 0.0f;
	}
	else
	{
		m_MouseDeltaX = MousePos.x - m_LastMouseX;
		m_MouseDeltaY = MousePos.y - m_LastMouseY;
	}
	m_LastMouseX = MousePos.x;
	m_LastMouseY = MousePos.y;
	m_HasMousePositionSample = true;
	if(UseDemo != m_UsingDemoInputState)
	{
		std::fill(std::begin(m_aWheelLastTime), std::end(m_aWheelLastTime), -1.0f);
		std::fill(std::begin(m_aWheelAlpha), std::end(m_aWheelAlpha), 0.0f);
		m_LastDemoWheelSequence = 0;
		m_UsingDemoInputState = UseDemo;
	}
	if(UseDemo)
	{
		if(pDemo != nullptr && pDemo->m_WheelSequence != m_LastDemoWheelSequence)
		{
			for(int i = 0; i < 4; ++i)
				if((pDemo->m_WheelMask & (1U << i)) != 0)
					m_aWheelLastTime[i] = m_Time;
			m_LastDemoWheelSequence = pDemo->m_WheelSequence;
		}
	}
	else
	{
		m_LastDemoWheelSequence = 0;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_UP))
			m_aWheelLastTime[0] = m_Time;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN))
			m_aWheelLastTime[1] = m_Time;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_LEFT))
			m_aWheelLastTime[2] = m_Time;
		if(Input()->KeyPress(KEY_MOUSE_WHEEL_RIGHT))
			m_aWheelLastTime[3] = m_Time;
	}
	for(int i = 0; i < 4; ++i)
	{
		if(m_aWheelLastTime[i] < 0.0f)
		{
			m_aWheelAlpha[i] = 0.0f;
			continue;
		}
		const float Age = m_Time - m_aWheelLastTime[i];
		m_aWheelAlpha[i] = Age <= WHEEL_HOLD_TIME ? 1.0f : 0.0f;
		if(Age > WHEEL_HOLD_TIME)
			m_aWheelLastTime[i] = -1.0f;
	}
}

bool CInputOverlay::IsObsActive(const SObsElement &Element) const
{
	const CGameClient::SDemoInputPlaybackState *pDemo = Client()->State() == IClient::STATE_DEMOPLAYBACK ? GameClient()->DemoInputPlaybackState() : nullptr;
	switch(Element.m_Type)
	{
	case QmInputOverlay::ET_TEXTURE:
		return true;
	case QmInputOverlay::ET_KEYBOARD_KEY:
		return Element.m_Key != KEY_UNKNOWN && (pDemo != nullptr ? DemoInputKeyIsPressed(pDemo, Element.m_Key) : Input()->GlobalKeyIsPressed(Element.m_Key));
	case QmInputOverlay::ET_GAMEPAD_BUTTON:
		return IsGamepadButtonPressed(Element.m_GamepadButton);
	case QmInputOverlay::ET_MOUSE_BUTTON:
		return Element.m_MouseButton > 0 && (pDemo != nullptr ? DemoInputKeyIsPressed(pDemo, KEY_MOUSE_1 + Element.m_MouseButton - 1) : Input()->GlobalMousePressed(Element.m_MouseButton));
	case QmInputOverlay::ET_WHEEL:
		if(Element.m_WheelDir >= 1 && Element.m_WheelDir <= 4)
			return m_aWheelAlpha[Element.m_WheelDir - 1] > 0.0f;
		return std::max(std::max(m_aWheelAlpha[0], m_aWheelAlpha[1]), std::max(m_aWheelAlpha[2], m_aWheelAlpha[3])) > 0.0f;
	case QmInputOverlay::ET_ANALOG_STICK:
	case QmInputOverlay::ET_TRIGGER:
	case QmInputOverlay::ET_GAMEPAD_ID:
	case QmInputOverlay::ET_DPAD_STICK:
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		{
			// 旧 Demo 没有手柄扩展消息时，不把实时手柄连接状态带进回放。
			const CGameClient::SDemoInputPlaybackState *pDemo = GameClient()->DemoInputPlaybackState();
			return pDemo != nullptr && pDemo->m_GamepadValid;
		}
		return Input()->GetActiveJoystick() != nullptr;
	case QmInputOverlay::ET_MOUSE_MOVEMENT:
		return true;
	default:
		return false;
	}
}

float CInputOverlay::MouseMoveAngle() const
{
	return std::atan2(m_MouseDeltaY, m_MouseDeltaX) + static_cast<float>(pi / 2.0);
}

void CInputOverlay::DrawObsElement(const SObsLayout &Layout, const SObsElement &Element, float OriginX, float OriginY, float LayoutScale, float Opacity)
{
	if(Element.m_MapW <= 0.0f || Element.m_MapH <= 0.0f || !Layout.m_Texture.IsValid())
		return;
	const bool Active = IsObsActive(Element);
	if(Element.m_ActiveOnly && !Active)
		return;
	const float X = OriginX + Element.m_PosX * LayoutScale;
	const float Y = OriginY + Element.m_PosY * LayoutScale;
	const float W = Element.m_MapW * LayoutScale;
	const float H = Element.m_MapH * LayoutScale;
	const float Alpha = std::clamp(Opacity, 0.0f, 1.0f);
	if(W <= 0.0f || H <= 0.0f || Alpha <= 0.0f)
		return;

	auto DrawRegion = [&](float MapX, float MapY, float MapW, float MapH, float PosX, float PosY, float DrawW, float DrawH, float Rotation = 0.0f) {
		if(MapW <= 0.0f || MapH <= 0.0f || MapX < 0.0f || MapY < 0.0f || MapX + MapW > Layout.m_TextureWidth || MapY + MapH > Layout.m_TextureHeight)
			return;
		Graphics()->QuadsSetSubset(MapX / Layout.m_TextureWidth, MapY / Layout.m_TextureHeight, (MapX + MapW) / Layout.m_TextureWidth, (MapY + MapH) / Layout.m_TextureHeight);
		Graphics()->QuadsSetRotation(Rotation);
		Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
		IGraphics::CQuadItem Quad(PosX + DrawW * 0.5f, PosY + DrawH * 0.5f, DrawW, DrawH);
		Graphics()->QuadsDraw(&Quad, 1);
	};
	auto PressedRect = [&]() {
		QmInputOverlay::SRect Rect{Element.m_MapX, Element.m_MapY + Element.m_MapH + OBS_INNER_BORDER, Element.m_MapW, Element.m_MapH};
		if(Layout.m_HasPressedOffset)
			Rect.m_Y = Element.m_MapY + Layout.m_PressedOffsetY;
		if(Element.m_HasPressedMapping)
			Rect = {Element.m_PressedMapX, Element.m_PressedMapY, Element.m_PressedMapW, Element.m_PressedMapH};
		return Rect;
	};
	auto DrawButton = [&]() {
		const QmInputOverlay::SRect Rect = Active ? PressedRect() : QmInputOverlay::SRect{Element.m_MapX, Element.m_MapY, Element.m_MapW, Element.m_MapH};
		DrawRegion(Rect.m_X, Rect.m_Y, Rect.m_W, Rect.m_H, X, Y, W, H);
	};

	switch(Element.m_Type)
	{
	case QmInputOverlay::ET_TEXTURE:
		DrawRegion(Element.m_MapX, Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		break;
	case QmInputOverlay::ET_KEYBOARD_KEY:
	case QmInputOverlay::ET_GAMEPAD_BUTTON:
	case QmInputOverlay::ET_MOUSE_BUTTON:
		DrawButton();
		break;
	case QmInputOverlay::ET_WHEEL:
	{
		const CGameClient::SDemoInputPlaybackState *pDemo = Client()->State() == IClient::STATE_DEMOPLAYBACK ? GameClient()->DemoInputPlaybackState() : nullptr;
		const bool MiddlePressed = pDemo != nullptr ? DemoInputKeyIsPressed(pDemo, KEY_MOUSE_3) : Input()->GlobalMousePressed(2);
		const int BaseIndex = MiddlePressed ? 1 : 0;
		DrawRegion(Element.m_MapX + BaseIndex * (Element.m_MapW + OBS_INNER_BORDER), Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		int WheelIndex = -1;
		if(m_aWheelAlpha[0] > 0.0f)
			WheelIndex = 2;
		if(m_aWheelAlpha[1] > 0.0f)
			WheelIndex = 3;
		if(m_aWheelAlpha[2] > 0.0f)
			WheelIndex = 4;
		if(m_aWheelAlpha[3] > 0.0f)
			WheelIndex = 5;
		if(WheelIndex >= 0)
			DrawRegion(Element.m_MapX + WheelIndex * (Element.m_MapW + OBS_INNER_BORDER), Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		break;
	}
	case QmInputOverlay::ET_ANALOG_STICK:
	{
		const int Axis = Element.m_Side == 1 ? 2 : 0;
		const float AxisX = GamepadAxisValue(Axis);
		const float AxisY = GamepadAxisValue(Axis + 1);
		const float StickX = X + AxisX * Element.m_StickRadius * LayoutScale;
		const float StickY = Y + AxisY * Element.m_StickRadius * LayoutScale;
		const bool StickPressed = IsGamepadButtonPressed(Element.m_Side == 1 ? 8 : 7);
		const QmInputOverlay::SRect Rect = StickPressed ? PressedRect() : QmInputOverlay::SRect{Element.m_MapX, Element.m_MapY, Element.m_MapW, Element.m_MapH};
		DrawRegion(Rect.m_X, Rect.m_Y, Rect.m_W, Rect.m_H, StickX, StickY, W, H);
		break;
	}
	case QmInputOverlay::ET_TRIGGER:
	{
		const float Progress = std::clamp(GamepadAxisValue(Element.m_Side == 1 ? 5 : 4), 0.0f, 1.0f);
		const QmInputOverlay::SRect Base{Element.m_MapX, Element.m_MapY, Element.m_MapW, Element.m_MapH};
		const QmInputOverlay::SRect Pressed = PressedRect();
		if(Element.m_TriggerMode)
		{
			const QmInputOverlay::SRect &Rect = Progress >= 0.1f ? Pressed : Base;
			DrawRegion(Rect.m_X, Rect.m_Y, Rect.m_W, Rect.m_H, X, Y, W, H);
		}
		else
		{
			DrawRegion(Base.m_X, Base.m_Y, Base.m_W, Base.m_H, X, Y, W, H);
			QmInputOverlay::SRect Crop = Pressed;
			float CropX = X;
			float CropY = Y;
			float CropW = W;
			float CropH = H;
			switch(Element.m_Direction)
			{
			case 1:
				Crop.m_H *= Progress;
				Crop.m_Y = Pressed.m_Y + Pressed.m_H - Crop.m_H;
				CropY += H - CropH;
				break;
			case 2:
				Crop.m_H *= Progress;
				CropH = H * Progress;
				break;
			case 3:
				Crop.m_W *= Progress;
				Crop.m_X = Pressed.m_X + Pressed.m_W - Crop.m_W;
				CropX += W - CropW;
				break;
			case 4:
				Crop.m_W *= Progress;
				CropW = W * Progress;
				break;
			default:
				Crop.m_W = 0.0f;
				Crop.m_H = 0.0f;
				break;
			}
			if(Crop.m_W > 0.0f && Crop.m_H > 0.0f)
				DrawRegion(Crop.m_X, Crop.m_Y, Crop.m_W, Crop.m_H, CropX, CropY, CropW, CropH);
		}
		break;
	}
	case QmInputOverlay::ET_GAMEPAD_ID:
	{
		if(IsGamepadButtonPressed(5))
			DrawRegion(Element.m_MapX + 3 * (Element.m_MapW + OBS_INNER_BORDER), Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		const int Player = std::clamp(GamepadPlayerIndex(), 0, 2);
		DrawRegion(Element.m_MapX + Player * (Element.m_MapW + OBS_INNER_BORDER), Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		break;
	}
	case QmInputOverlay::ET_DPAD_STICK:
	{
		const bool Up = IsGamepadButtonPressed(11);
		const bool Down = IsGamepadButtonPressed(12);
		const bool Left = IsGamepadButtonPressed(13);
		const bool Right = IsGamepadButtonPressed(14);
		int Index = 0;
		if(Up && Left)
			Index = 5;
		else if(Up && Right)
			Index = 6;
		else if(Down && Left)
			Index = 7;
		else if(Down && Right)
			Index = 8;
		else if(Left)
			Index = 1;
		else if(Right)
			Index = 2;
		else if(Up)
			Index = 3;
		else if(Down)
			Index = 4;
		DrawRegion(Element.m_MapX + Index * (Element.m_MapW + OBS_INNER_BORDER), Element.m_MapY, Element.m_MapW, Element.m_MapH, X, Y, W, H);
		break;
	}
	case QmInputOverlay::ET_MOUSE_MOVEMENT:
	{
		float MoveX = X;
		float MoveY = Y;
		if(Element.m_MouseType == 0)
		{
			const float Radius = Element.m_MouseRadius * LayoutScale;
			const float FactorX = std::clamp(m_MouseDeltaX / 32.0f, -1.0f, 1.0f);
			const float FactorY = std::clamp(m_MouseDeltaY / 32.0f, -1.0f, 1.0f);
			MoveX += Radius * FactorX;
			MoveY += Radius * FactorY;
		}
		DrawRegion(Element.m_MapX, Element.m_MapY, Element.m_MapW, Element.m_MapH, MoveX, MoveY, W, H, Element.m_MouseType == 1 ? MouseMoveAngle() : 0.0f);
		break;
	}
	default:
		break;
	}
}

bool CInputOverlay::GetConfigModifiedTime(time_t &OutModified) const
{
	bool Found = false;
	time_t Latest = 0;
	const auto CheckPath = [&](const std::string &Path) {
		if(Path.empty())
			return;
		for(int StorageType = IStorage::TYPE_SAVE; StorageType < Storage()->NumPaths(); ++StorageType)
		{
			if(!Storage()->FileExists(Path.c_str(), StorageType))
				continue;
			time_t Created = 0;
			time_t Modified = 0;
			if(Storage()->RetrieveTimes(Path.c_str(), StorageType, &Created, &Modified))
			{
				Latest = std::max(Latest, Modified);
				Found = true;
			}
			break;
		}
	};
	CheckPath(QmInputOverlay::CONFIGURATION_PATH);
	for(const SObsLayout &Layout : m_vObsLayouts)
	{
		for(const std::string &Path : {Layout.m_LayoutPath, Layout.m_ImagePath})
			CheckPath(Path);
	}
	if(Found)
		OutModified = Latest;
	return Found;
}

bool CInputOverlay::OpenEditor() const
{
#if defined(CONF_PLATFORM_ANDROID)
	return false;
#else
	char aEditorPath[IO_MAX_PATH_LENGTH];
	for(int StorageType = IStorage::TYPE_SAVE + 1; StorageType < Storage()->NumPaths(); ++StorageType)
	{
		if(!Storage()->FindFile("input_overlay_editor.html", "qmclient", StorageType, aEditorPath, sizeof(aEditorPath)))
			continue;
		char aCompletePath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(StorageType, aEditorPath, aCompletePath, sizeof(aCompletePath));
		if(MakeAbsolutePath(aCompletePath, aEditorPath, sizeof(aEditorPath)))
			return Client()->ViewFile(aEditorPath);
	}
	Storage()->GetBinaryPathAbsolute("data/qmclient/input_overlay_editor.html", aEditorPath, sizeof(aEditorPath));
	return fs_is_file(aEditorPath) && Client()->ViewFile(aEditorPath);
#endif
}

void CInputOverlay::ConOpenEditor(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	static_cast<CInputOverlay *>(pUserData)->OpenEditor();
}

void CInputOverlay::OnConsoleInit()
{
	Console()->Register("qm_input_overlay_editor", "", CFGFLAG_CLIENT, ConOpenEditor, this, "Open the input overlay editor");
}

void CInputOverlay::OnRender()
{
	if(!g_Config.m_QmInputOverlay || (Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK))
		return;
	// 计分板或 ESC 菜单打开时隐藏，仅游戏内显示。
	if(GameClient()->m_Scoreboard.IsActive() || GameClient()->m_Menus.IsActive())
		return;
	if(!m_ConfigLoaded)
		LoadConfiguration(IStorage::TYPE_ALL);
	m_ConfigCheckTimer += Client()->RenderFrameTime();
	if(m_ConfigCheckTimer >= 0.5f)
	{
		m_ConfigCheckTimer = 0.0f;
		time_t Modified = 0;
		if(GetConfigModifiedTime(Modified) && (!m_HasConfigModifiedTime || Modified != m_ConfigModifiedTime))
		{
			LoadConfiguration(IStorage::TYPE_ALL);
			m_ConfigModifiedTime = Modified;
			m_HasConfigModifiedTime = true;
		}
	}
	if(!m_ConfigValid || m_vObsLayouts.empty())
		return;
	m_Time += Client()->RenderFrameTime();
	UpdateInputState();
	const CUIRect *pScreen = Ui()->Screen();
	if(pScreen == nullptr || pScreen->w <= 0.0f || pScreen->h <= 0.0f)
		return;
	Graphics()->MapScreen(pScreen->x, pScreen->y, pScreen->x + pScreen->w, pScreen->y + pScreen->h);
	const float KeyboardScale = g_Config.m_QmInputOverlayScale / 100.0f;
	const float MouseScale = g_Config.m_QmInputOverlayMouseScale / 100.0f;
	const float Opacity = g_Config.m_QmInputOverlayOpacity / 100.0f;
	float MinX = 0.0f;
	float MinY = 0.0f;
	float MaxX = 1.0f;
	float MaxY = 1.0f;
	for(const SObsLayout &Layout : m_vObsLayouts)
	{
		const float Scale = Layout.m_IsMouseLayout ? MouseScale : KeyboardScale;
		MinX = std::min(MinX, Layout.m_OffsetX * KeyboardScale);
		MinY = std::min(MinY, Layout.m_OffsetY * KeyboardScale);
		MaxX = std::max(MaxX, Layout.m_OffsetX * KeyboardScale + Layout.m_OverlayWidth * Scale);
		MaxY = std::max(MaxY, Layout.m_OffsetY * KeyboardScale + Layout.m_OverlayHeight * Scale);
	}
	const float CanvasW = std::max(MaxX - MinX, 1.0f);
	const float CanvasH = std::max(MaxY - MinY, 1.0f);
	// 默认屏幕居中；拖拽位置由 HUD 编辑器保存并接管（原水平/垂直位置配置已删除）。
	const float OriginX = pScreen->x + (pScreen->w - CanvasW) * 0.5f;
	const float OriginY = pScreen->y + (pScreen->h - CanvasH) * 0.5f;
	const CUIRect OverlayRect = {OriginX, OriginY, CanvasW, CanvasH};
	const auto HudEditorScope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::InputOverlay, OverlayRect);
	const CUIRect CanvasClip = HudEditorScope.m_TargetRect.Intersection(*pScreen);
	if(CanvasClip.w > 0.0f && CanvasClip.h > 0.0f)
		Ui()->ClipEnable(&CanvasClip);
	for(const SObsLayout &Layout : m_vObsLayouts)
	{
		if(!Layout.m_Texture.IsValid())
			continue;
		const float LayoutScale = Layout.m_IsMouseLayout ? MouseScale : KeyboardScale;
		const float LayoutOriginX = OriginX + Layout.m_OffsetX * KeyboardScale - MinX;
		const float LayoutOriginY = OriginY + Layout.m_OffsetY * KeyboardScale - MinY;
		Graphics()->TextureSet(Layout.m_Texture);
		Graphics()->QuadsBegin();
		for(const SObsElement &Element : Layout.m_vElements)
			DrawObsElement(Layout, Element, LayoutOriginX, LayoutOriginY, LayoutScale, Opacity);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();
	}
	if(CanvasClip.w > 0.0f && CanvasClip.h > 0.0f)
		Ui()->ClipDisable();
	Graphics()->TextureClear();
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	GameClient()->m_HudEditor.UpdateVisibleRect(EHudEditorElement::InputOverlay, OverlayRect);
	GameClient()->m_HudEditor.EndTransform(HudEditorScope);
}
