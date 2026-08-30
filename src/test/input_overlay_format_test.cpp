#include "test.h"

#include <engine/keys.h>

#include <game/client/components/qmclient/input_overlay_format.h>

#include <gtest/gtest.h>

#include <string>

TEST(QmInputOverlayFormat, ParsesOfficialV5LayoutAndEditorExtension)
{
	const std::string Layout = R"json({
 "version":507,
 "default_width":128,
 "default_height":96,
 "space_h":-2,
 "space_v":3,
 "flags":6,
 "overlay_width":640,
 "overlay_height":360,
 "image":"controller.png",
 "elements":[
  {"id":"texture","type":0,"pos":[0,0],"mapping":[1,1,10,10],"z_level":0},
  {"id":"key","type":1,"code":16,"pos":[12,0],"mapping":[12,1,20,20],"mapping_press":[12,24,20,20],"z_level":"1"},
  {"id":"pad","type":2,"code":0,"pos":[36,0],"mapping":[35,1,20,20],"z_level":1},
  {"id":"mouse","type":3,"code":1,"pos":[60,0],"mapping":[58,1,20,20],"z_level":1},
  {"id":"wheel","type":4,"direction":1,"pos":[84,0],"mapping":[81,1,20,20],"z_level":1},
  {"id":"stick","type":5,"side":0,"stick_radius":30,"pos":[108,0],"mapping":[104,1,30,30],"z_level":1},
  {"id":"trigger","type":6,"side":1,"direction":4,"trigger_mode":false,"pos":[142,0],"mapping":[135,1,30,30],"z_level":1},
  {"id":"gamepad-id","type":7,"pos":[176,0],"mapping":[169,1,20,20],"z_level":1},
  {"id":"dpad","type":8,"pos":[200,0],"mapping":[192,1,20,20],"z_level":1},
  {"id":"movement","type":9,"mouse_type":1,"mouse_radius":20,"direction":0,"pos":[224,0],"mapping":[215,1,20,20],"z_level":1}
 ],
 "_qm_editor":{"version":1,"state":{"name":"Controller"}}
})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	ASSERT_TRUE(QmInputOverlay::ParseLayout(Layout.data(), static_cast<unsigned>(Layout.size()), Parsed, Error)) << Error;
	ASSERT_EQ(Parsed.m_vElements.size(), 10U);
	EXPECT_EQ(Parsed.m_Version, 507);
	EXPECT_EQ(Parsed.m_ImagePath, "controller.png");
	EXPECT_TRUE(Parsed.m_HasQmEditorExtension);
	EXPECT_EQ(Parsed.m_vElements[0].m_Type, QmInputOverlay::ET_TEXTURE);
	EXPECT_EQ(Parsed.m_vElements[1].m_Type, QmInputOverlay::ET_KEYBOARD_KEY);
	EXPECT_TRUE(Parsed.m_vElements[1].m_HasMappingPress);
	EXPECT_EQ(Parsed.m_vElements[4].m_Direction, 1);
	EXPECT_EQ(Parsed.m_vElements[5].m_Side, 0);
	EXPECT_EQ(Parsed.m_vElements[6].m_TriggerMode, false);
	EXPECT_EQ(Parsed.m_vElements[9].m_MouseType, 1);
}

TEST(QmInputOverlayFormat, AcceptsOfficialV50LayoutWithoutVersion)
{
	const std::string Layout = R"json({
 "default_width":0,"default_height":0,"overlay_width":64,"overlay_height":64,
 "elements":[{"id":"q","type":1,"code":81,"pos":[0,0],"mapping":[0,0,32,32]}]
})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	ASSERT_TRUE(QmInputOverlay::ParseLayout(Layout.data(), static_cast<unsigned>(Layout.size()), Parsed, Error)) << Error;
	EXPECT_EQ(Parsed.m_Version, 506);
	EXPECT_FALSE(Parsed.m_HasVersion);
}

TEST(QmInputOverlayFormat, RejectsLegacyQmProfileAndUnsafeImage)
{
	const std::string Legacy = R"json({"format":"qm_input_overlay","version":1,"elements":[]})json";
	const std::string Unsafe = R"json({"version":507,"image":"../overlay.png","elements":[{"id":"x","type":0,"pos":[0,0],"mapping":[0,0,1,1]}]})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	EXPECT_FALSE(QmInputOverlay::ParseLayout(Legacy.data(), static_cast<unsigned>(Legacy.size()), Parsed, Error));
	EXPECT_FALSE(QmInputOverlay::ParseLayout(Unsafe.data(), static_cast<unsigned>(Unsafe.size()), Parsed, Error));
	EXPECT_TRUE(QmInputOverlay::IsSafeRelativePath("InputOverlay/controller.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath("../controller.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath("C:/controller.png"));
}

TEST(QmInputOverlayFormat, ConvertsOfficialKeyboardAndGamepadCodes)
{
	EXPECT_EQ(QmInputOverlay::KeyboardCodeToEngine(0x10, 507), KEY_Q);
	EXPECT_EQ(QmInputOverlay::KeyboardCodeToEngine(0x0051, 506), KEY_Q);
	EXPECT_EQ(QmInputOverlay::KeyboardCodeToEngine(0xe048, 507), KEY_UP);
	EXPECT_EQ(QmInputOverlay::GamepadCodeToButton(0), 0);
	EXPECT_EQ(QmInputOverlay::GamepadCodeToButton(0xec04), 9);
	EXPECT_EQ(QmInputOverlay::GamepadCodeToButton(0xffff), -1);
}

TEST(QmInputOverlayFormat, ParsesExtendedElementOptions)
{
	// 覆盖 z_level 数字字符串、side、stick_radius、mouse_radius、direction、
	// trigger_mode、active_only 与 mapping_press。
	const std::string Layout = R"json({
	 "version":507,"overlay_width":128,"overlay_height":128,
	 "elements":[
	  {"id":"stick","type":5,"side":1,"stick_radius":42,"active_only":true,"pos":[0,0],"mapping":[0,0,20,20],"mapping_press":[0,24,20,20],"z_level":"3"},
	  {"id":"trigger","type":6,"direction":2,"trigger_mode":true,"pos":[24,0],"mapping":[24,0,20,20]},
	  {"id":"move","type":9,"mouse_radius":15,"mouse_type":0,"pos":[48,0],"mapping":[48,0,20,20]}
	 ]
	})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	ASSERT_TRUE(QmInputOverlay::ParseLayout(Layout.data(), static_cast<unsigned>(Layout.size()), Parsed, Error)) << Error;
	ASSERT_EQ(Parsed.m_vElements.size(), 3U);
	// ParseLayout 会按 z_level 升序稳定排序：stick(z=3) 排最后，trigger/move(z=0) 在前。
	const QmInputOverlay::SElement *pStick = nullptr;
	const QmInputOverlay::SElement *pTrigger = nullptr;
	const QmInputOverlay::SElement *pMove = nullptr;
	for(const QmInputOverlay::SElement &Element : Parsed.m_vElements)
	{
		if(Element.m_Id == "stick")
			pStick = &Element;
		else if(Element.m_Id == "trigger")
			pTrigger = &Element;
		else if(Element.m_Id == "move")
			pMove = &Element;
	}
	ASSERT_NE(pStick, nullptr);
	ASSERT_NE(pTrigger, nullptr);
	ASSERT_NE(pMove, nullptr);
	EXPECT_EQ(pStick->m_Side, 1);
	EXPECT_EQ(pStick->m_StickRadius, 42);
	EXPECT_TRUE(pStick->m_ActiveOnly);
	EXPECT_TRUE(pStick->m_HasMappingPress);
	EXPECT_EQ(pStick->m_ZLevel, 3);
	EXPECT_EQ(pTrigger->m_Direction, 2);
	EXPECT_TRUE(pTrigger->m_TriggerMode);
	EXPECT_EQ(pMove->m_MouseRadius, 15);
	EXPECT_EQ(pMove->m_MouseType, 0);
}

TEST(QmInputOverlayFormat, RejectsInvalidElementsAndDimensions)
{
	const std::string BadType = R"json({"version":507,"elements":[{"id":"x","type":10,"pos":[0,0],"mapping":[0,0,1,1]}]})json";
	const std::string DupId = R"json({"version":507,"elements":[{"id":"x","type":0,"pos":[0,0],"mapping":[0,0,1,1]},{"id":"x","type":1,"pos":[2,0],"mapping":[2,0,1,1]}]})json";
	const std::string BadSize = R"json({"version":507,"elements":[{"id":"x","type":0,"pos":[0,0],"mapping":[0,0,-1,1]}]})json";
	const std::string HugeOverlay = R"json({"version":507,"overlay_width":999999,"overlay_height":64,"elements":[{"id":"x","type":0,"pos":[0,0],"mapping":[0,0,1,1]}]})json";
	const std::string LegacyV3 = R"json({"format":"input_overlay_v3","version":1,"elements":[]})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	EXPECT_FALSE(QmInputOverlay::ParseLayout(BadType.data(), static_cast<unsigned>(BadType.size()), Parsed, Error));
	EXPECT_FALSE(QmInputOverlay::ParseLayout(DupId.data(), static_cast<unsigned>(DupId.size()), Parsed, Error));
	EXPECT_FALSE(QmInputOverlay::ParseLayout(BadSize.data(), static_cast<unsigned>(BadSize.size()), Parsed, Error));
	EXPECT_FALSE(QmInputOverlay::ParseLayout(HugeOverlay.data(), static_cast<unsigned>(HugeOverlay.size()), Parsed, Error));
	EXPECT_FALSE(QmInputOverlay::ParseLayout(LegacyV3.data(), static_cast<unsigned>(LegacyV3.size()), Parsed, Error));
}

TEST(QmInputOverlayFormat, AcceptsBackslashRelativeImagePaths)
{
	// 官方 Windows 配置可能使用反斜杠路径：先安全规范化再检查目录穿越。
	const std::string Layout = R"json({
	 "version":507,"overlay_width":64,"overlay_height":64,
	 "image":"images\\controller.png",
	 "elements":[{"id":"q","type":1,"code":16,"pos":[0,0],"mapping":[0,0,32,32]}]
	})json";
	QmInputOverlay::SLayout Parsed;
	std::string Error;
	ASSERT_TRUE(QmInputOverlay::ParseLayout(Layout.data(), static_cast<unsigned>(Layout.size()), Parsed, Error)) << Error;
	EXPECT_EQ(Parsed.m_ImagePath, "images/controller.png");
	EXPECT_TRUE(QmInputOverlay::IsSafeRelativePath("InputOverlay\\controller.png"));
	EXPECT_TRUE(QmInputOverlay::IsSafeRelativePath("a/b.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath("..\\controller.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath("a/../b.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath(".\\controller.png"));
	EXPECT_FALSE(QmInputOverlay::IsSafeRelativePath("C:\\controller.png"));
	EXPECT_EQ(QmInputOverlay::NormalizePathSlashes("a\\b/c.png"), "a/b/c.png");
}
