// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "qm_icon_manager.h"

#include <base/system.h>

#include <engine/console.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char *QM_ICON_MANIFEST_PATTERN = "qmclient/icons/qm_icons_%dx.json";

	EQmIcon IconFromName(const char *pName)
	{
		if(str_comp(pName, "star") == 0)
			return EQmIcon::STAR;
		if(str_comp(pName, "search") == 0)
			return EQmIcon::SEARCH;
		if(str_comp(pName, "close") == 0)
			return EQmIcon::CLOSE;
		if(str_comp(pName, "eye") == 0)
			return EQmIcon::EYE;
		if(str_comp(pName, "eye-off") == 0)
			return EQmIcon::EYE_OFF;
		if(str_comp(pName, "chevron-down") == 0)
			return EQmIcon::CHEVRON_DOWN;
		if(str_comp(pName, "plus") == 0)
			return EQmIcon::PLUS;
		if(str_comp(pName, "trash") == 0)
			return EQmIcon::TRASH;
		if(str_comp(pName, "satellite-swap-incoming") == 0)
			return EQmIcon::SATELLITE_SWAP_INCOMING;
		if(str_comp(pName, "satellite-swap-outgoing") == 0)
			return EQmIcon::SATELLITE_SWAP_OUTGOING;
		if(str_comp(pName, "satellite-switch") == 0)
			return EQmIcon::SATELLITE_SWITCH;
		if(str_comp(pName, "satellite-mute") == 0)
			return EQmIcon::SATELLITE_MUTE;
		if(str_comp(pName, "satellite-check") == 0)
			return EQmIcon::SATELLITE_CHECK;
		if(str_comp(pName, "satellite-spectator-eye") == 0)
			return EQmIcon::SATELLITE_SPECTATOR_EYE;
		if(str_comp(pName, "satellite-spectator-eye-closed") == 0)
			return EQmIcon::SATELLITE_SPECTATOR_EYE_CLOSED;
		if(str_comp(pName, "tune-gravity") == 0)
			return EQmIcon::TUNE_GRAVITY;
		if(str_comp(pName, "tune-movement") == 0)
			return EQmIcon::TUNE_MOVEMENT;
		if(str_comp(pName, "tune-jump") == 0)
			return EQmIcon::TUNE_JUMP;
		if(str_comp(pName, "tune-hook") == 0)
			return EQmIcon::TUNE_HOOK;
		if(str_comp(pName, "tune-collision") == 0)
			return EQmIcon::TUNE_COLLISION;
		if(str_comp(pName, "tune-gun-jetpack") == 0)
			return EQmIcon::TUNE_GUN_JETPACK;
		if(str_comp(pName, "tune-shotgun") == 0)
			return EQmIcon::TUNE_SHOTGUN;
		if(str_comp(pName, "tune-grenade-explosion") == 0)
			return EQmIcon::TUNE_GRENADE_EXPLOSION;
		if(str_comp(pName, "tune-laser") == 0)
			return EQmIcon::TUNE_LASER;
		if(str_comp(pName, "tune-hammer") == 0)
			return EQmIcon::TUNE_HAMMER;
		if(str_comp(pName, "tune-weapon-fire-rate") == 0)
			return EQmIcon::TUNE_WEAPON_FIRE_RATE;
		if(str_comp(pName, "tune-velramp") == 0)
			return EQmIcon::TUNE_VELRAMP;
		if(str_comp(pName, "tune-elasticity") == 0)
			return EQmIcon::TUNE_ELASTICITY;
		return EQmIcon::COUNT;
	}

	bool JsonIntField(const json_value *pObject, const char *pName, int &Out)
	{
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == &json_value_none || pValue->type != json_integer)
			return false;
		Out = static_cast<int>(pValue->u.integer);
		return true;
	}

	const char *JsonStringField(const json_value *pObject, const char *pName)
	{
		const json_value *pValue = json_object_get(pObject, pName);
		if(pValue == &json_value_none || pValue->type != json_string)
			return "";
		return pValue->u.string.ptr;
	}

	void LogIconAtlas(IConsole *pConsole, const char *pText)
	{
		if(pConsole != nullptr)
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "qm_icons", pText);
	}
}

ColorRGBA SQmIconStyle::Color(EQmIconState State) const
{
	switch(State)
	{
	case EQmIconState::NORMAL: return m_Normal;
	case EQmIconState::HOVER: return m_Hover;
	case EQmIconState::ACTIVE: return m_Active;
	case EQmIconState::DISABLED: return m_Disabled;
	}
	return m_Normal;
}

void CQmIconAtlas::Clear(IGraphics *pGraphics)
{
	if(m_Texture.IsValid() && pGraphics != nullptr)
		pGraphics->UnloadTexture(&m_Texture);
	m_Texture = IGraphics::CTextureHandle();
	for(SEntry &Entry : m_aEntries)
		Entry = {};
	m_LoadedIconCount = 0;
	m_AtlasScale = 0;
	m_Width = 0;
	m_Height = 0;
	m_Padding = 0;
}

void CQmIconManager::Init(IGraphics *pGraphics, IStorage *pStorage, IConsole *pConsole)
{
	m_pGraphics = pGraphics;
	m_pStorage = pStorage;
	m_pConsole = pConsole;
	Reload();
}

void CQmIconManager::Clear()
{
	m_Atlas.Clear(m_pGraphics);
}

bool CQmIconManager::Reload()
{
	Clear();
	if(m_pGraphics == nullptr || m_pStorage == nullptr)
		return false;

	const int PreferredScale = PreferredAtlasScale();
	m_PreferredScale = PreferredScale;
	const int aScales[3] = {
		PreferredScale,
		PreferredScale == 4 ? 2 : (PreferredScale == 2 ? 4 : 2),
		PreferredScale == 1 ? 4 : 1,
	};

	for(const int Scale : aScales)
	{
		if(LoadManifestForScale(Scale))
			return true;
	}
	return false;
}

void CQmIconManager::RefreshForCurrentDpi()
{
	const int PreferredScale = PreferredAtlasScale();
	if(!IsReady() || PreferredScale != m_PreferredScale)
		Reload();
}

int CQmIconManager::PreferredAtlasScale() const
{
	if(m_pGraphics == nullptr)
		return 1;

	const float HiDpi = std::max(1.0f, m_pGraphics->ScreenHiDPIScale());
	return HiDpi >= 3.0f ? 4 : (HiDpi >= 1.5f ? 2 : 1);
}

bool CQmIconManager::LoadManifestForScale(int Scale)
{
	char aManifestPath[IO_MAX_PATH_LENGTH];
	str_format(aManifestPath, sizeof(aManifestPath), QM_ICON_MANIFEST_PATTERN, Scale);
	if(!m_pStorage->FileExists(aManifestPath, IStorage::TYPE_ALL))
		return false;

	void *pFileData = nullptr;
	unsigned FileSize = 0;
	if(!m_pStorage->ReadFile(aManifestPath, IStorage::TYPE_ALL, &pFileData, &FileSize))
		return false;

	char aError[256] = "";
	json_settings Settings{};
	json_value *pRoot = JsonParseEx(&Settings, static_cast<json_char *>(pFileData), FileSize, aError);
	free(pFileData);
	if(pRoot == nullptr)
	{
		char aBuf[320];
		str_format(aBuf, sizeof(aBuf), "Failed to parse %s: %s", aManifestPath, aError);
		LogIconAtlas(m_pConsole, aBuf);
		return false;
	}

	bool Success = false;
	do
	{
		const json_value *pAtlas = json_object_get(pRoot, "atlas");
		const json_value *pIcons = json_object_get(pRoot, "icons");
		if(pAtlas == &json_value_none || pAtlas->type != json_object || pIcons == &json_value_none || pIcons->type != json_object)
			break;

		int AtlasWidth = 0;
		int AtlasHeight = 0;
		if(!JsonIntField(pAtlas, "width", AtlasWidth) || !JsonIntField(pAtlas, "height", AtlasHeight) || AtlasWidth <= 0 || AtlasHeight <= 0)
			break;
		int AtlasPadding = 0;
		JsonIntField(pAtlas, "padding", AtlasPadding);

		const char *pImagePath = JsonStringField(pAtlas, "image");
		if(pImagePath[0] == '\0')
			break;

		std::array<CQmIconAtlas::SEntry, static_cast<size_t>(EQmIcon::COUNT)> aEntries{};
		int LoadedIconCount = 0;
		for(unsigned int IconIndex = 0; IconIndex < pIcons->u.object.length; ++IconIndex)
		{
			const auto &JsonIcon = pIcons->u.object.values[IconIndex];
			const EQmIcon Icon = IconFromName(JsonIcon.name);
			if(Icon == EQmIcon::COUNT || JsonIcon.value == nullptr || JsonIcon.value->type != json_object)
				continue;

			int X = 0;
			int Y = 0;
			int W = 0;
			int H = 0;
			if(!JsonIntField(JsonIcon.value, "x", X) || !JsonIntField(JsonIcon.value, "y", Y) ||
				!JsonIntField(JsonIcon.value, "w", W) || !JsonIntField(JsonIcon.value, "h", H) ||
				X < 0 || Y < 0 || W <= 1 || H <= 1 || X + W > AtlasWidth || Y + H > AtlasHeight)
				continue;

			CQmIconAtlas::SEntry &Entry = aEntries[static_cast<size_t>(Icon)];
			Entry.m_Valid = true;
			Entry.m_U0 = (X + 0.5f) / static_cast<float>(AtlasWidth);
			Entry.m_V0 = (Y + 0.5f) / static_cast<float>(AtlasHeight);
			Entry.m_U1 = (X + W - 0.5f) / static_cast<float>(AtlasWidth);
			Entry.m_V1 = (Y + H - 0.5f) / static_cast<float>(AtlasHeight);
			++LoadedIconCount;
		}

		if(LoadedIconCount == 0)
			break;

		IGraphics::CTextureHandle Texture = m_pGraphics->LoadTexture(pImagePath, IStorage::TYPE_ALL, IGraphics::TEXLOAD_NO_MIPMAPS);
		if(!Texture.IsValid())
			break;

		m_Atlas.m_Texture = Texture;
		m_Atlas.m_aEntries = aEntries;
		m_Atlas.m_LoadedIconCount = LoadedIconCount;
		m_Atlas.m_AtlasScale = Scale;
		m_Atlas.m_Width = AtlasWidth;
		m_Atlas.m_Height = AtlasHeight;
		m_Atlas.m_Padding = AtlasPadding;
		Success = true;
	} while(false);

	json_value_free(pRoot);
	return Success;
}

CUIRect CQmIconManager::PixelAlignedRect(const CUIRect &Rect) const
{
	if(m_pGraphics == nullptr)
		return Rect;

	float ScreenX0 = 0.0f;
	float ScreenY0 = 0.0f;
	float ScreenX1 = 0.0f;
	float ScreenY1 = 0.0f;
	m_pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float ScaleX = m_pGraphics->ScreenWidth() / std::max(1.0f, ScreenX1 - ScreenX0);
	const float ScaleY = m_pGraphics->ScreenHeight() / std::max(1.0f, ScreenY1 - ScreenY0);

	CUIRect Out = Rect;
	const float X0 = std::round(Rect.x * ScaleX) / ScaleX;
	const float Y0 = std::round(Rect.y * ScaleY) / ScaleY;
	const float X1 = std::round((Rect.x + Rect.w) * ScaleX) / ScaleX;
	const float Y1 = std::round((Rect.y + Rect.h) * ScaleY) / ScaleY;
	Out.x = X0;
	Out.y = Y0;
	Out.w = std::max(1.0f / ScaleX, X1 - X0);
	Out.h = std::max(1.0f / ScaleY, Y1 - Y0);
	return Out;
}

bool CQmIconManager::RenderIcon(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color) const
{
	const size_t IconIndex = static_cast<size_t>(Icon);
	if(!IsReady() || IconIndex >= m_Atlas.m_aEntries.size() || !m_Atlas.m_aEntries[IconIndex].m_Valid || Color.a <= 0.0f)
		return false;

	const CQmIconAtlas::SEntry &Entry = m_Atlas.m_aEntries[IconIndex];
	const CUIRect Aligned = PixelAlignedRect(Rect);

	m_pGraphics->WrapClamp();
	m_pGraphics->TextureSet(m_Atlas.m_Texture);
	m_pGraphics->QuadsBegin();
	m_pGraphics->SetColor(Color.r, Color.g, Color.b, Color.a);
	m_pGraphics->QuadsSetSubset(Entry.m_U0, Entry.m_V0, Entry.m_U1, Entry.m_V1);
	IGraphics::CQuadItem Quad(Aligned.x, Aligned.y, Aligned.w, Aligned.h);
	m_pGraphics->QuadsDrawTL(&Quad, 1);
	m_pGraphics->QuadsEnd();
	m_pGraphics->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	m_pGraphics->WrapNormal();
	return true;
}

bool CQmIconManager::RenderIconRotated(EQmIcon Icon, const CUIRect &Rect, const ColorRGBA &Color, float Rotation) const
{
	const size_t IconIndex = static_cast<size_t>(Icon);
	if(!IsReady() || IconIndex >= m_Atlas.m_aEntries.size() || !m_Atlas.m_aEntries[IconIndex].m_Valid || Color.a <= 0.0f)
		return false;

	const CQmIconAtlas::SEntry &Entry = m_Atlas.m_aEntries[IconIndex];
	const CUIRect Aligned = PixelAlignedRect(Rect);

	m_pGraphics->WrapClamp();
	m_pGraphics->TextureSet(m_Atlas.m_Texture);
	m_pGraphics->QuadsBegin();
	m_pGraphics->SetColor(Color.r, Color.g, Color.b, Color.a);
	m_pGraphics->QuadsSetSubset(Entry.m_U0, Entry.m_V0, Entry.m_U1, Entry.m_V1);
	m_pGraphics->QuadsSetRotation(Rotation);
	IGraphics::CQuadItem Quad(Aligned.x, Aligned.y, Aligned.w, Aligned.h);
	m_pGraphics->QuadsDrawTL(&Quad, 1);
	m_pGraphics->QuadsSetRotation(0.0f);
	m_pGraphics->QuadsEnd();
	m_pGraphics->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	m_pGraphics->WrapNormal();
	return true;
}

bool CQmIconManager::RenderIcon(EQmIcon Icon, const CUIRect &Rect, EQmIconState State, const SQmIconStyle &Style) const
{
	return RenderIcon(Icon, Rect, Style.Color(State));
}
