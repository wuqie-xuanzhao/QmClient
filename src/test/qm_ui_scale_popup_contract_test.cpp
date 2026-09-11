#include <gtest/gtest.h>
#include <test/test.h>

#include <string>

TEST(QmUiScaleContract, BlockingPopupsAndDemoRowsFitScaledScreen)
{
	const std::string Menus = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const std::string DemoMenus = ReadTestSourceFile("src/game/client/components/menus_demo.cpp");
	EXPECT_NE(Menus.find("QmUiCenteredMargin(Box, 150.0f, 300.0f, 300.0f)"), std::string::npos);
	EXPECT_NE(Menus.find("QmUiCenteredMargin(Screen, 150.0f, 300.0f, 300.0f)"), std::string::npos);
	EXPECT_NE(DemoMenus.find("QmUiVisibleRows(SegmentsArea.h"), std::string::npos);
	EXPECT_NE(DemoMenus.find("VerticalExpansion = std::min(60.0f, PopupMargin)"), std::string::npos);
}
