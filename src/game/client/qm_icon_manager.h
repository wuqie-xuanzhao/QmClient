// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_ICON_MANAGER_H
#define GAME_CLIENT_QM_ICON_MANAGER_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/QmUi/QmTheme.h>
#include <game/client/ui_rect.h>

#include <array>

class IConsole;
class IStorage;

enum class EQmIcon
{
	STAR = 0,
	SEARCH,
	CLOSE,
	EYE,
	EYE_OFF,
	CHEVRON_DOWN,
	PLUS,
	TRASH,
	COUNT,
};

enum class EQmIconState
{
	NORMAL = 0,
	HOVER,
	ACTIVE,
	DISABLED,
};

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
	struct SEntry
	{
		bool m_Valid = false;
		float m_U0 = 0.0f;
		float m_V0 = 0.0f;
		float m_U1 = 1.0f;
		float m_V1 = 1.0f;
	};

	void Clear(IGraphics *pGraphics);
	bool IsReady() const { return m_Texture.IsValid() && m_LoadedIconCount > 0; }
	int LoadedIconCount() const { return m_LoadedIconCount; }
	int AtlasScale() const { return m_AtlasScale; }
	int Width() const { return m_Width; }
	int Height() const { return m_Height; }
	int Padding() const { return m_Padding; }

private:
	friend class CQmIconManager;

	IGraphics::CTextureHandle m_Texture;
	std::array<SEntry, static_cast<size_t>(EQmIcon::COUNT)> m_aEntries{};
	int m_LoadedIconCount = 0;
	int m_AtlasScale = 0;
	int m_Width = 0;
	int m_Height = 0;
	int m_Padding = 0;
};

class CQmIconManager
{
public:
	void Init(IGraphics *pGraphics, IStorage *pStorage, IConsole *pConsole);
	bool Reload();
	void RefreshForCurrentDpi();
	bool IsReady() const { return m_Atlas.IsReady(); }
	int LoadedIconCount() const { return m_Atlas.LoadedIconCount(); }
	int AtlasScale() const { return m_Atlas.AtlasScale(); }

	bool RenderIcon(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color) const;
	bool RenderIconRotated(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color, float Rotation) const;
	bool RenderIcon(EQmIcon Icon, const CUIRect &Rect, EQmIconState State, const SQmIconStyle &Style = SQmIconStyle()) const;

	static const char *IconName(EQmIcon Icon)
	{
		switch(Icon)
		{
		case EQmIcon::STAR: return "star";
		case EQmIcon::SEARCH: return "search";
		case EQmIcon::CLOSE: return "close";
		case EQmIcon::EYE: return "eye";
		case EQmIcon::EYE_OFF: return "eye-off";
		case EQmIcon::CHEVRON_DOWN: return "chevron-down";
		case EQmIcon::PLUS: return "plus";
		case EQmIcon::TRASH: return "trash";
		case EQmIcon::COUNT: break;
		}
		return "";
	}

private:
	bool LoadManifestForScale(int Scale);
	int PreferredAtlasScale() const;
	CUIRect PixelAlignedRect(const CUIRect &Rect) const;
	void Clear();

	IGraphics *m_pGraphics = nullptr;
	IStorage *m_pStorage = nullptr;
	IConsole *m_pConsole = nullptr;
	CQmIconAtlas m_Atlas;
	int m_PreferredScale = 0;
};

#endif
