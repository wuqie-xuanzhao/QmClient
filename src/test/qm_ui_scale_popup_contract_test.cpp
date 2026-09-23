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
	// 弹窗的纵向扩张必须被外边距夹住，缩放后才不会溢出屏幕。
	// 这里只锁定「用 std::min 对 PopupMargin 取夹」这一稳定事实，不锁定具体常量——
	// 弹窗增删一行内容时那个常量本来就会变（例如加入回放显示面板）。
	const size_t Expansion = DemoMenus.find("const float VerticalExpansion = std::min(");
	ASSERT_NE(Expansion, std::string::npos);
	EXPECT_NE(DemoMenus.find("PopupMargin)", Expansion), std::string::npos);
}
