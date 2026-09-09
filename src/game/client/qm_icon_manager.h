// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_ICON_MANAGER_H
#define GAME_CLIENT_QM_ICON_MANAGER_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/QmUi/QmTheme.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

class IConsole;
class IStorage;

enum class EQmIcon
{
	STAR = 0,
	BOOKMARK,
	SEARCH,
	CLOSE,
	EYE,
	EYE_OFF,
	CHEVRON_DOWN,
	PLUS,
	TRASH,
	SATELLITE_SWAP_INCOMING,
	SATELLITE_SWAP_OUTGOING,
	SATELLITE_SWITCH,
	SATELLITE_MUTE,
	SATELLITE_CHECK,
	SATELLITE_SPECTATOR_EYE,
	SATELLITE_SPECTATOR_EYE_CLOSED,
	// 与 FontIcons::FONT_ICON_* 对应的图集图标，名字见 CQmIconManager::IconName。
	MINUS,
	LOCK,
	HEART,
	HEART_CRACK,
	CIRCLE,
	ARROW_ROTATE_LEFT,
	ARROW_ROTATE_RIGHT,
	FLAG_CHECKERED,
	BAN,
	CIRCLE_CHEVRON_DOWN,
	KEY,
	LANGUAGE,
	SQUARE_MINUS,
	SQUARE_PLUS,
	SORT_UP,
	SORT_DOWN,
	TRIANGLE_EXCLAMATION,
	HOUSE,
	NEWSPAPER,
	POWER_OFF,
	GEAR,
	PEN_TO_SQUARE,
	CLAPPERBOARD,
	EARTH_AMERICAS,
	NETWORK_WIRED,
	LIST_UL,
	INFO,
	TERMINAL,
	USER,
	SLASH,
	PLAY,
	PAUSE,
	STOP,
	CHEVRON_LEFT,
	CHEVRON_RIGHT,
	CHEVRON_UP,
	BACKWARD,
	FORWARD,
	RIGHT_FROM_BRACKET,
	RIGHT_TO_BRACKET,
	ARROW_UP_RIGHT_FROM_SQUARE,
	BACKWARD_STEP,
	FORWARD_STEP,
	BACKWARD_FAST,
	FORWARD_FAST,
	KEYBOARD,
	ELLIPSIS,
	FOLDER,
	FOLDER_OPEN,
	FOLDER_TREE,
	FILM,
	VIDEO,
	MAP,
	IMAGE,
	MUSIC,
	FILE,
	PENCIL,
	COPY,
	ARROWS_LEFT_RIGHT,
	ARROWS_UP_DOWN,
	CIRCLE_PLAY,
	BORDER_ALL,
	EYE_DROPPER,
	COMMENT,
	COMMENT_SLASH,
	DICE_ONE,
	DICE_TWO,
	DICE_THREE,
	DICE_FOUR,
	DICE_FIVE,
	DICE_SIX,
	LAYER_GROUP,
	UNDO,
	REDO,
	ARROWS_ROTATE,
	QUESTION,
	CAMERA,
	USERS,
	COUNT,
};

enum class EQmIconState
{
	NORMAL = 0,
	HOVER,
	ACTIVE,
	DISABLED,
};

enum class EQmIconAtlasType
{
	ALPHA,
	MSDF,
};

enum class EQmIconRefreshAction
{
	NONE,
	RELOAD,
	RETRY_MSDF,
};

struct SQmIconRefreshState
{
	bool m_NeedsReload = false;
	bool m_ReloadCooldownActive = false;
	bool m_NeedsMsdfProbe = false;
	bool m_MsdfProbeCooldownActive = false;
};

inline EQmIconAtlasType SelectQmIconAtlasType(const bool MsdfSupported, const bool MsdfAvailable)
{
	return MsdfSupported && MsdfAvailable ? EQmIconAtlasType::MSDF : EQmIconAtlasType::ALPHA;
}

inline bool QmIconAtlasNeedsReload(const bool IsReady, const EQmIconAtlasType LoadedType, const EQmIconAtlasType DesiredType, const int LoadedWeight, const int DesiredWeight, const int LoadedScale, const int DesiredScale)
{
	return !IsReady || LoadedType != DesiredType || LoadedWeight != DesiredWeight || (DesiredType == EQmIconAtlasType::ALPHA && LoadedScale != DesiredScale);
}

inline bool QmIconAtlasRetryCooldownActive(const int64_t Now, const int64_t RetryDeadline)
{
	return Now < RetryDeadline;
}

inline bool QmIconReloadCooldownActive(const int64_t Now, const int64_t RetryDeadline, const bool HasFailedTarget, const int FailedWeight, const int FailedScale, const bool FailedMsdfSupported, const int DesiredWeight, const int DesiredScale, const bool MsdfSupported)
{
	return HasFailedTarget && Now < RetryDeadline && FailedWeight == DesiredWeight && FailedScale == DesiredScale && FailedMsdfSupported == MsdfSupported;
}

inline EQmIconRefreshAction QmIconRefreshAction(const SQmIconRefreshState &State)
{
	if(State.m_NeedsReload)
		return State.m_ReloadCooldownActive ? EQmIconRefreshAction::NONE : EQmIconRefreshAction::RELOAD;
	if(State.m_NeedsMsdfProbe && !State.m_MsdfProbeCooldownActive)
		return EQmIconRefreshAction::RETRY_MSDF;
	return EQmIconRefreshAction::NONE;
}

inline EQmIconRefreshAction QmIconRefreshAction(const bool NeedsReload, const bool ReloadCooldownActive, const bool NeedsMsdfProbe, const bool MsdfProbeCooldownActive)
{
	return QmIconRefreshAction({NeedsReload, ReloadCooldownActive, NeedsMsdfProbe, MsdfProbeCooldownActive});
}

inline size_t QmIconMsdfRunBucket(const uint64_t RunLength)
{
	if(RunLength <= 1)
		return 0;
	if(RunLength <= 2)
		return 1;
	if(RunLength <= 4)
		return 2;
	if(RunLength <= 8)
		return 3;
	if(RunLength <= 16)
		return 4;
	if(RunLength <= 32)
		return 5;
	if(RunLength <= 64)
		return 6;
	return 7;
}

struct SQmIconDiagnostics
{
	static constexpr size_t MSDF_RUN_BUCKET_COUNT = 8;
	uint64_t m_AlphaIconDraws = 0;
	uint64_t m_MsdfIconDraws = 0;
	uint64_t m_MaxMsdfManagerCallRun = 0;
	std::array<uint64_t, MSDF_RUN_BUCKET_COUNT> m_MsdfManagerCallRunBuckets{};
	uint64_t m_ReloadAttempts = 0;
	uint64_t m_ReloadSuccesses = 0;
	uint64_t m_AtlasSwaps = 0;
	uint64_t m_MsdfProbes = 0;
	uint64_t m_MsdfProbeSuccesses = 0;
	uint64_t m_TextureLoads = 0;
	uint64_t m_TextureLoadFailures = 0;
	uint64_t m_TextureUnloads = 0;
};

inline bool QmIconTextureCanCommit(const bool IsValid, const bool IsNullTexture)
{
	return IsValid && !IsNullTexture;
}

inline bool QmIconAtlasCanRetainOnReloadFailure(const bool IsReady, const EQmIconAtlasType LoadedType, const bool MsdfSupported)
{
	return IsReady && (LoadedType != EQmIconAtlasType::MSDF || MsdfSupported);
}

inline bool QmIconAtlasMustDropMsdf(const bool MsdfSupported, const EQmIconAtlasType LoadedType)
{
	return !MsdfSupported && LoadedType == EQmIconAtlasType::MSDF;
}

inline bool QmIconAtlasNeedsMsdfProbe(const bool MsdfSupported, const bool MsdfManifestAvailable)
{
	return MsdfSupported && !MsdfManifestAvailable;
}

inline int QmIconPreferredAtlasScale(const float HiDpiScale)
{
	const float HiDpi = std::max(1.0f, HiDpiScale);
	return HiDpi >= 3.0f ? 4 : (HiDpi >= 1.5f ? 2 : 1);
}

inline std::array<int, 3> QmIconAtlasScaleFallbackOrder(const int PreferredScale)
{
	return {
		PreferredScale,
		PreferredScale == 4 ? 2 : (PreferredScale == 2 ? 4 : 2),
		PreferredScale == 1 ? 4 : 1,
	};
}

inline float QmIconPixelScale(const int DrawableExtent, const float LogicalExtent)
{
	return DrawableExtent > 0 && LogicalExtent > 0.0f ? DrawableExtent / LogicalExtent : 0.0f;
}

inline int NormalizeQmIconWeight(const int Weight)
{
	return Weight >= 0 && Weight <= 3 ? Weight : 1;
}

inline bool QmIconWeightUsesBoldFontFallback(const int Weight)
{
	return NormalizeQmIconWeight(Weight) == 1;
}

inline ColorRGBA QmUiIconColor(const ColorRGBA &Color, const int ConfiguredColor, const unsigned int CustomColor = 0xFFFFFFFF, const float RainbowTime = 0.0f)
{
	ColorRGBA Result;
	switch(ConfiguredColor)
	{
	case 2:
		Result = ColorRGBA(0.0f, 0.0f, 0.0f, Color.a);
		break;
	case 3:
		Result = color_cast<ColorRGBA>(ColorHSLA(CustomColor));
		Result.a = Color.a;
		break;
	case 4:
		Result = color_cast<ColorRGBA>(ColorHSLA(std::fmod(RainbowTime * 0.2f, 1.0f), 0.75f, 0.6f, Color.a));
		break;
	case 1:
	default:
		Result = ColorRGBA(1.0f, 1.0f, 1.0f, Color.a);
		break;
	}
	return Result;
}

ColorRGBA ConfiguredQmUiIconColor(const ColorRGBA &Color);

struct SQmIconStyle
{
	ColorRGBA m_Normal{qm_theme::ICON.m_Normal};
	ColorRGBA m_Hover{qm_theme::ICON.m_Hover};
	ColorRGBA m_Active{qm_theme::ICON.m_Active};
	ColorRGBA m_Disabled{qm_theme::ICON.m_Disabled};

	ColorRGBA Color(EQmIconState State) const;
};

class CQmIconAtlas
{
public:
	enum class EType
	{
		ALPHA,
		MSDF,
	};

	struct SEntry
	{
		bool m_Valid = false;
		float m_U0 = 0.0f;
		float m_V0 = 0.0f;
		float m_U1 = 1.0f;
		float m_V1 = 1.0f;
	};

	void Clear(IGraphics *pGraphics);
	void Swap(CQmIconAtlas &Other)
	{
		std::swap(m_Texture, Other.m_Texture);
		std::swap(m_aEntries, Other.m_aEntries);
		std::swap(m_LoadedIconCount, Other.m_LoadedIconCount);
		std::swap(m_AtlasScale, Other.m_AtlasScale);
		std::swap(m_Width, Other.m_Width);
		std::swap(m_Height, Other.m_Height);
		std::swap(m_Padding, Other.m_Padding);
		std::swap(m_PxRange, Other.m_PxRange);
		std::swap(m_Type, Other.m_Type);
	}
	bool IsReady() const { return m_Texture.IsValid() && m_LoadedIconCount == static_cast<int>(EQmIcon::COUNT); }
	int LoadedIconCount() const { return m_LoadedIconCount; }
	int AtlasScale() const { return m_AtlasScale; }
	int Width() const { return m_Width; }
	int Height() const { return m_Height; }
	int Padding() const { return m_Padding; }
	bool IsMsdf() const { return m_Type == EType::MSDF; }
	EQmIconAtlasType Type() const { return IsMsdf() ? EQmIconAtlasType::MSDF : EQmIconAtlasType::ALPHA; }

private:
	friend class CQmIconManager;

	IGraphics::CTextureHandle m_Texture;
	std::array<SEntry, static_cast<size_t>(EQmIcon::COUNT)> m_aEntries{};
	int m_LoadedIconCount = 0;
	int m_AtlasScale = 0;
	int m_Width = 0;
	int m_Height = 0;
	int m_Padding = 0;
	float m_PxRange = 0.0f;
	EType m_Type = EType::ALPHA;
};

class CQmIconManager
{
public:
	void Init(IGraphics *pGraphics, IStorage *pStorage, IConsole *pConsole);
	void Shutdown();
	bool Reload();
	void RefreshForCurrentDpi();
	bool IsReady() const { return m_Atlas.IsReady(); }
	int LoadedIconCount() const { return m_Atlas.LoadedIconCount(); }
	int AtlasScale() const { return m_Atlas.AtlasScale(); }
	SQmIconDiagnostics TakeDiagnostics() const;

	bool RenderIcon(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color) const;
	bool RenderIconRotated(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color, float Rotation) const;
	bool RenderIcon(EQmIcon Icon, const CUIRect &Rect, EQmIconState State, const SQmIconStyle &Style = SQmIconStyle()) const;

	static const char *IconName(EQmIcon Icon)
	{
		// 枚举 -> 图集格子名。新增图标时只在这张表里登记一次。
		struct SEntry
		{
			EQmIcon m_Icon;
			const char *m_pName;
		};
		static constexpr SEntry s_aEntries[] = {
			{EQmIcon::STAR, "star"},
			{EQmIcon::BOOKMARK, "bookmark"},
			{EQmIcon::SEARCH, "magnifying-glass"},
			{EQmIcon::CLOSE, "close"},
			{EQmIcon::EYE, "eye"},
			{EQmIcon::EYE_OFF, "eye-off"},
			{EQmIcon::CHEVRON_DOWN, "chevron-down"},
			{EQmIcon::PLUS, "plus"},
			{EQmIcon::TRASH, "trash"},
			{EQmIcon::SATELLITE_SWAP_INCOMING, "satellite-swap-incoming"},
			{EQmIcon::SATELLITE_SWAP_OUTGOING, "satellite-swap-outgoing"},
			{EQmIcon::SATELLITE_SWITCH, "satellite-switch"},
			{EQmIcon::SATELLITE_MUTE, "satellite-mute"},
			{EQmIcon::SATELLITE_CHECK, "satellite-check"},
			{EQmIcon::SATELLITE_SPECTATOR_EYE, "satellite-spectator-eye"},
			{EQmIcon::SATELLITE_SPECTATOR_EYE_CLOSED, "satellite-spectator-eye-closed"},
			{EQmIcon::MINUS, "minus"},
			{EQmIcon::LOCK, "lock"},
			{EQmIcon::HEART, "heart"},
			{EQmIcon::HEART_CRACK, "heart-break"},
			{EQmIcon::CIRCLE, "circle"},
			{EQmIcon::ARROW_ROTATE_LEFT, "arrow-counter-clockwise"},
			{EQmIcon::ARROW_ROTATE_RIGHT, "arrow-clockwise"},
			{EQmIcon::FLAG_CHECKERED, "flag-checkered"},
			{EQmIcon::BAN, "prohibit"},
			{EQmIcon::CIRCLE_CHEVRON_DOWN, "caret-circle-down"},
			{EQmIcon::KEY, "key"},
			{EQmIcon::LANGUAGE, "translate"},
			{EQmIcon::SQUARE_MINUS, "minus-square"},
			{EQmIcon::SQUARE_PLUS, "plus-square"},
			{EQmIcon::SORT_UP, "sort-ascending"},
			{EQmIcon::SORT_DOWN, "sort-descending"},
			{EQmIcon::TRIANGLE_EXCLAMATION, "warning"},
			{EQmIcon::HOUSE, "house"},
			{EQmIcon::NEWSPAPER, "newspaper"},
			{EQmIcon::POWER_OFF, "power"},
			{EQmIcon::GEAR, "gear"},
			{EQmIcon::PEN_TO_SQUARE, "pencil-simple"},
			{EQmIcon::CLAPPERBOARD, "film-slate"},
			{EQmIcon::EARTH_AMERICAS, "globe-hemisphere-west"},
			{EQmIcon::NETWORK_WIRED, "network"},
			{EQmIcon::LIST_UL, "list"},
			{EQmIcon::INFO, "info"},
			{EQmIcon::TERMINAL, "terminal"},
			{EQmIcon::USER, "user"},
			{EQmIcon::SLASH, "line-segment"},
			{EQmIcon::PLAY, "play"},
			{EQmIcon::PAUSE, "pause"},
			{EQmIcon::STOP, "stop"},
			{EQmIcon::CHEVRON_LEFT, "caret-left"},
			{EQmIcon::CHEVRON_RIGHT, "caret-right"},
			{EQmIcon::CHEVRON_UP, "caret-up"},
			{EQmIcon::BACKWARD, "skip-back"},
			{EQmIcon::FORWARD, "skip-forward"},
			{EQmIcon::RIGHT_FROM_BRACKET, "sign-out"},
			{EQmIcon::RIGHT_TO_BRACKET, "sign-in"},
			{EQmIcon::ARROW_UP_RIGHT_FROM_SQUARE, "arrow-square-out"},
			{EQmIcon::BACKWARD_STEP, "backward-step"},
			{EQmIcon::FORWARD_STEP, "forward-step"},
			{EQmIcon::BACKWARD_FAST, "rewind"},
			{EQmIcon::FORWARD_FAST, "fast-forward"},
			{EQmIcon::KEYBOARD, "keyboard"},
			{EQmIcon::ELLIPSIS, "dots-three"},
			{EQmIcon::FOLDER, "folder"},
			{EQmIcon::FOLDER_OPEN, "folder-open"},
			{EQmIcon::FOLDER_TREE, "tree-structure"},
			{EQmIcon::FILM, "film-strip"},
			{EQmIcon::VIDEO, "video"},
			{EQmIcon::MAP, "map-trifold"},
			{EQmIcon::IMAGE, "image"},
			{EQmIcon::MUSIC, "music-notes"},
			{EQmIcon::FILE, "file"},
			{EQmIcon::PENCIL, "pencil"},
			{EQmIcon::COPY, "copy"},
			{EQmIcon::ARROWS_LEFT_RIGHT, "arrows-left-right"},
			{EQmIcon::ARROWS_UP_DOWN, "arrows-vertical"},
			{EQmIcon::CIRCLE_PLAY, "play-circle"},
			{EQmIcon::BORDER_ALL, "grid-four"},
			{EQmIcon::EYE_DROPPER, "eyedropper"},
			{EQmIcon::COMMENT, "chat"},
			{EQmIcon::COMMENT_SLASH, "chat-slash"},
			{EQmIcon::DICE_ONE, "dice-one"},
			{EQmIcon::DICE_TWO, "dice-two"},
			{EQmIcon::DICE_THREE, "dice-three"},
			{EQmIcon::DICE_FOUR, "dice-four"},
			{EQmIcon::DICE_FIVE, "dice-five"},
			{EQmIcon::DICE_SIX, "dice-six"},
			{EQmIcon::LAYER_GROUP, "stack"},
			{EQmIcon::UNDO, "arrow-u-up-left"},
			{EQmIcon::REDO, "arrow-u-up-right"},
			{EQmIcon::ARROWS_ROTATE, "arrows-clockwise"},
			{EQmIcon::QUESTION, "question"},
			{EQmIcon::CAMERA, "camera"},
			{EQmIcon::USERS, "users"},
		};
		for(const SEntry &Entry : s_aEntries)
		{
			if(Entry.m_Icon == Icon)
				return Entry.m_pName;
		}
		return "";
	}

private:
	bool RetryMsdfAtlas();
	bool LoadManifest(CQmIconAtlas &Atlas, const char *pManifestPath, int Scale, bool Msdf);
	bool LoadMsdfManifest(CQmIconAtlas &Atlas);
	bool LoadManifestForScale(CQmIconAtlas &Atlas, int Scale);
	int PreferredAtlasScale() const;
	CUIRect PixelAlignedRect(const CUIRect &Rect) const;
	void ClearAtlas(CQmIconAtlas &Atlas);
	void FinishMsdfManagerCallRun() const;

	IGraphics *m_pGraphics = nullptr;
	IStorage *m_pStorage = nullptr;
	IConsole *m_pConsole = nullptr;
	CQmIconAtlas m_Atlas;
	int m_PreferredScale = 0;
	int m_AtlasWeight = -1;
	bool m_MsdfManifestAvailable = false;
	int64_t m_NextReloadAttemptTime = 0;
	int64_t m_NextMsdfProbeTime = 0;
	int m_FailedReloadWeight = -1;
	int m_FailedReloadScale = 0;
	bool m_FailedReloadMsdfSupported = false;
	bool m_HasFailedReloadTarget = false;
	mutable SQmIconDiagnostics m_Diagnostics;
	bool m_DiagnosticsEnabled = false;
	mutable uint64_t m_CurrentMsdfManagerCallRun = 0;
};

#endif
