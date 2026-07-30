// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/shared/json.h>

#include <game/client/qm_icon_manager.h>

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace
{
	std::string ReadTextFile(const char *pPath)
	{
		return ReadTestSourceFile(pPath);
	}

	int JsonInt(const json_value *pObject, const char *pName)
	{
		const json_value *pValue = json_object_get(pObject, pName);
		EXPECT_NE(pValue, &json_value_none);
		EXPECT_EQ(pValue->type, json_integer);
		return pValue->type == json_integer ? static_cast<int>(pValue->u.integer) : 0;
	}

	const json_value *JsonObject(const json_value *pObject, const char *pName)
	{
		const json_value *pValue = json_object_get(pObject, pName);
		EXPECT_NE(pValue, &json_value_none);
		EXPECT_EQ(pValue->type, json_object);
		return pValue;
	}
}

TEST(QmIconAtlas, RuntimeIconNamesAreStable)
{
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::STAR), "star");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SEARCH), "search");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::CLOSE), "close");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::EYE), "eye");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::EYE_OFF), "eye-off");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::CHEVRON_DOWN), "chevron-down");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::PLUS), "plus");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::TRASH), "trash");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_SWAP_INCOMING), "satellite-swap-incoming");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_SWAP_OUTGOING), "satellite-swap-outgoing");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_SWITCH), "satellite-switch");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_MUTE), "satellite-mute");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_CHECK), "satellite-check");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_SPECTATOR_EYE), "satellite-spectator-eye");
	EXPECT_STREQ(CQmIconManager::IconName(EQmIcon::SATELLITE_SPECTATOR_EYE_CLOSED), "satellite-spectator-eye-closed");
}

TEST(QmIconAtlas, SelectedPhosphorFillSourcesKeepTheirApprovedGeometry)
{
	struct SSelectedIcon
	{
		const char *m_pPath;
		const char *m_pIconName;
		const char *m_pGeometryFingerprint;
	};
	const std::array<SSelectedIcon, 4> aSelectedIcons = {{
		{"datasrc/qm_icons/tabler/icon-eye.svg", "eyes-fill", "M176,32c-20.61"},
		{"datasrc/qm_icons/tabler/icon-satellite-swap-incoming.svg", "arrow-circle-left-fill", "H107.31l18.35,18.34"},
		{"datasrc/qm_icons/tabler/icon-satellite-swap-outgoing.svg", "arrow-circle-right-fill", "L148.69,136H88"},
		{"datasrc/qm_icons/tabler/icon-satellite-switch.svg", "clock-countdown-fill", "M208,96a12,12,0,1,1,12,12"},
	}};

	for(const SSelectedIcon &Icon : aSelectedIcons)
	{
		const std::string Source = ReadTextFile(Icon.m_pPath);
		EXPECT_NE(Source.find(Icon.m_pIconName), std::string::npos) << Icon.m_pPath;
		EXPECT_NE(Source.find(Icon.m_pGeometryFingerprint), std::string::npos) << Icon.m_pPath;
		EXPECT_NE(Source.find("fill=\"currentColor\""), std::string::npos) << Icon.m_pPath;
		EXPECT_NE(Source.find("LICENSE_PHOSPHOR.txt"), std::string::npos) << Icon.m_pPath;
		EXPECT_EQ(Source.find("stroke="), std::string::npos) << Icon.m_pPath;
	}

	const std::string License = ReadTextFile("datasrc/qm_icons/LICENSE_PHOSPHOR.txt");
	EXPECT_NE(License.find("MIT License"), std::string::npos);
	EXPECT_NE(License.find("Copyright (c) 2020 Phosphor Icons"), std::string::npos);
}

TEST(QmIconAtlas, DynamicIslandMuteAndCompletionIconsRemainLucideOutlineIcons)
{
	const std::array<const char *, 2> apUnchangedIcons = {
		"datasrc/qm_icons/tabler/icon-satellite-mute.svg",
		"datasrc/qm_icons/tabler/icon-satellite-check.svg",
	};
	for(const char *pPath : apUnchangedIcons)
	{
		const std::string Source = ReadTextFile(pPath);
		EXPECT_NE(Source.find("LICENSE_LUCIDE.txt"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("fill=\"none\""), std::string::npos) << pPath;
		EXPECT_NE(Source.find("stroke=\"currentColor\""), std::string::npos) << pPath;
	}
}

TEST(QmIconAtlas, DynamicIslandSpectatorEyesUseTheApprovedHandDrawnPair)
{
	const std::string OpenEye = ReadTextFile("datasrc/qm_icons/tabler/icon-satellite-spectator-eye.svg");
	const std::string ClosedEye = ReadTextFile("datasrc/qm_icons/tabler/icon-satellite-spectator-eye-closed.svg");

	EXPECT_NE(OpenEye.find("M28 128C53 88 88 68 128 68"), std::string::npos);
	EXPECT_NE(OpenEye.find("<circle cx=\"128\" cy=\"128\" r=\"32\""), std::string::npos);
	EXPECT_NE(ClosedEye.find("M28 104C53 144 88 164 128 164"), std::string::npos);
	EXPECT_NE(ClosedEye.find("M57 137L41 165M94 158L87 190"), std::string::npos);
	for(const std::string *pSource : {&OpenEye, &ClosedEye})
	{
		EXPECT_NE(pSource->find("viewBox=\"0 0 256 256\""), std::string::npos);
		EXPECT_NE(pSource->find("stroke=\"currentColor\""), std::string::npos);
		EXPECT_NE(pSource->find("stroke-width=\"20\""), std::string::npos);
		EXPECT_EQ(pSource->find("LICENSE_PHOSPHOR.txt"), std::string::npos);
	}
}

TEST(QmIconAtlas, GeneratedManifestsContainEveryRuntimeIcon)
{
	constexpr int aScales[] = {1, 2, 4};
	for(const int Scale : aScales)
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "data/qmclient/icons/qm_icons_%dx.json", Scale);
		const std::string Json = ReadTextFile(aPath);
		ASSERT_FALSE(Json.empty()) << aPath;

		json_value *pRoot = JsonParse(Json.c_str(), Json.size());
		ASSERT_NE(pRoot, nullptr) << aPath;

		const json_value *pAtlas = JsonObject(pRoot, "atlas");
		const json_value *pIcons = JsonObject(pRoot, "icons");
		const int AtlasWidth = JsonInt(pAtlas, "width");
		const int AtlasHeight = JsonInt(pAtlas, "height");
		EXPECT_EQ(JsonInt(pRoot, "scale"), Scale);
		EXPECT_EQ(JsonInt(pAtlas, "padding"), 4 * Scale);

		for(int IconIndex = 0; IconIndex < static_cast<int>(EQmIcon::COUNT); ++IconIndex)
		{
			const EQmIcon Icon = static_cast<EQmIcon>(IconIndex);
			const char *pIconName = CQmIconManager::IconName(Icon);
			ASSERT_NE(pIconName[0], '\0');

			const json_value *pEntry = JsonObject(pIcons, pIconName);
			const int X = JsonInt(pEntry, "x");
			const int Y = JsonInt(pEntry, "y");
			const int W = JsonInt(pEntry, "w");
			const int H = JsonInt(pEntry, "h");

			EXPECT_EQ(W, 24 * Scale) << pIconName;
			EXPECT_EQ(H, 24 * Scale) << pIconName;
			EXPECT_GE(X, 0) << pIconName;
			EXPECT_GE(Y, 0) << pIconName;
			EXPECT_LE(X + W, AtlasWidth) << pIconName;
			EXPECT_LE(Y + H, AtlasHeight) << pIconName;
		}

		json_value_free(pRoot);
	}
}
