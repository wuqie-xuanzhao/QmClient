// 名牌 MSDF 门控与回退诊断测试：字体族匹配、缺失码点定位、回退日志去重。
// 生产逻辑在 qm_nameplate_msdf_gate.h（inline），渲染器与 nameplates 共用同一实现。
#include <game/client/components/qmclient/nameplate_msdf/qm_nameplate_msdf_gate.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cstdint>

TEST(QmNameplateMsdfGate, FontMatchesAtlasFamilies)
{
	// 图集固定字体族：base 页烤 DejaVu Sans，CJK 页烤 Source Han Sans SC
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("DejaVu Sans"));
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("Source Han Sans SC"));
}

TEST(QmNameplateMsdfGate, FontMatchIgnoresCaseAndWeightSuffix)
{
	// 对齐字体下拉框的 str_find_nocase 子串匹配：大小写与权重/变体后缀不影响命中
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("dejavu sans"));
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("DEJAVU SANS BOLD"));
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("Source Han Sans SC Heavy"));
}

TEST(QmNameplateMsdfGate, OtherFontsDoNotMatchAtlas)
{
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Arial"));
	// 繁体族（TC）与宋体族（Serif）都不是图集预烤的族
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Source Han Sans TC"));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Source Han Serif SC"));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas(""));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas(nullptr));
}

TEST(QmNameplateMsdfGate, FirstMissingCodepointFindsFirstUnsupported)
{
	const auto AllSupported = [](uint32_t) { return true; };
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("player", AllSupported), 0u);

	const auto AsciiOnly = [](uint32_t Codepoint) { return Codepoint < 0x80; };
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("plain", AsciiOnly), 0u);
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("ni\u2605ck", AsciiOnly), 0x2605u);
	// 多个缺失码点时返回文本中第一个出现的
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("\u2605\u2665", AsciiOnly), 0x2605u);
	// 换行与制表符按空白处理，不需要字形
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("a\nb\tc", AsciiOnly), 0u);
	// 空文本与空指针没有缺失
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("", AsciiOnly), 0u);
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint(nullptr, AsciiOnly), 0u);
}

TEST(QmNameplateMsdfGate, FirstMissingCodepointReportsInvalidUtf8AsReplacement)
{
	const auto AsciiOnly = [](uint32_t Codepoint) { return Codepoint < 0x80; };
	// 非法 UTF-8 序列解码为 U+FFFD（替换字符），同样参与覆盖判定
	const char aInvalid[] = {'a', (char)0xFF, 'b', '\0'};
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint(aInvalid, AsciiOnly), 0xFFFDu);
}

TEST(QmNameplateMsdfGate, FallbackReporterDedupsByCodepoint)
{
	CQmNameplateMsdfFallbackReporter Reporter;
	EXPECT_TRUE(Reporter.ShouldReport(0x1F600));
	EXPECT_FALSE(Reporter.ShouldReport(0x1F600));
	EXPECT_EQ(Reporter.ReportedCount(), 1u);

	EXPECT_TRUE(Reporter.ShouldReport(0x2605));
	EXPECT_FALSE(Reporter.ShouldReport(0x2605));
	EXPECT_EQ(Reporter.ReportedCount(), 2u);

	// 去重键是码点而非「名字×码点」：不同名字里的同一码点只报一次
	EXPECT_FALSE(Reporter.ShouldReport(0x1F600));
}
