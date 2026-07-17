// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/shared/json.h>

#include <game/client/qm_icon_manager.h>

#include <gtest/gtest.h>

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
