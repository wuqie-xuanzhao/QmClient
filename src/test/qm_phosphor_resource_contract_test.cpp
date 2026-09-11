#include <gtest/gtest.h>
#include <test/test.h>

#include <array>
#include <string>

TEST(QmPhosphorResourceContract, WeightSourcesRemainSeparated)
{
	struct SSelectedIcon
	{
		const char *m_pPath;
		const char *m_pVariant;
	};
	const std::array<SSelectedIcon, 8> aSelectedIcons = {{
		{"datasrc/qm_icons/phosphor_bold/icon-eye.svg", "bold"},
		{"datasrc/qm_icons/phosphor_bold/icon-satellite-swap-incoming.svg", "bold"},
		{"datasrc/qm_icons/phosphor_fill/icon-eye.svg", "fill"},
		{"datasrc/qm_icons/phosphor_fill/icon-satellite-swap-incoming.svg", "fill"},
		{"datasrc/qm_icons/phosphor_regular/icon-eye.svg", "regular"},
		{"datasrc/qm_icons/phosphor_regular/icon-satellite-swap-incoming.svg", "regular"},
		{"datasrc/qm_icons/phosphor_thin/icon-eye.svg", "thin"},
		{"datasrc/qm_icons/phosphor_thin/icon-satellite-swap-incoming.svg", "thin"},
	}};
	for(const SSelectedIcon &Icon : aSelectedIcons)
	{
		const std::string Source = ReadTestSourceFile(Icon.m_pPath);
		const std::string SourcePath = Icon.m_pPath;
		EXPECT_NE(Source.find("<svg"), std::string::npos) << Icon.m_pPath;
		EXPECT_NE(Source.find("viewBox=\"0 0 256 256\""), std::string::npos) << Icon.m_pPath;
		EXPECT_NE(SourcePath.find("phosphor_" + std::string(Icon.m_pVariant)), std::string::npos) << Icon.m_pPath;
	}
	const std::string License = ReadTestSourceFile("datasrc/qm_icons/LICENSE_PHOSPHOR.txt");
	EXPECT_NE(License.find("MIT License"), std::string::npos);
	EXPECT_NE(License.find("Copyright (c) 2020 Phosphor Icons"), std::string::npos);
}
