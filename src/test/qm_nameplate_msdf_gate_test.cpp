// 名牌 MSDF 门控与回退诊断测试：字体族匹配、缺失码点定位、回退日志去重。
// 生产逻辑在 qm_nameplate_msdf_gate.h（inline），渲染器与 nameplates 共用同一实现。
#include <game/client/components/qmclient/nameplate_msdf/qm_nameplate_msdf_gate.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cstdint>

TEST(QmNameplateMsdfGate, FontMatchesAtlasFamilies)
{
	EXPECT_STREQ(QmNameplateMsdfFontProfile("DejaVu Sans"), "dejavu");
	// MTSDF 名牌只做英文与图标：CJK 字体不映射任何 profile，整条名牌走 FreeType。
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Noto Sans SC"));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Glow Sans J Compressed Book"));
	EXPECT_EQ(QmNameplateMsdfFontProfile("NotoSansCJKsc-Thin"), nullptr);
}

TEST(QmNameplateMsdfGate, FontMatchIgnoresCaseAndWeightSuffix)
{
	// 对齐字体下拉框的 str_find_nocase 子串匹配：大小写与权重/变体后缀不影响命中
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("noto sans cjk sc heavy"));
}

TEST(QmNameplateMsdfGate, OtherFontsDoNotMatchAtlas)
{
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Arial"));
	// 未随包字体仍必须完整回退 FreeType，不能拿相近字体图形冒充
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("Source Han Serif SC"));
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("Nunito Black"));
	EXPECT_TRUE(QmNameplateMsdfFontMatchesAtlas("Nunito"));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas("PingFang SC"));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas(""));
	EXPECT_FALSE(QmNameplateMsdfFontMatchesAtlas(nullptr));
	EXPECT_EQ(QmNameplateMsdfFontProfile("Source Han Serif SC"), nullptr);
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
	// 换行、制表符与空格按空白处理，不需要字形
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("a\nb\tc", AsciiOnly), 0u);
	// 空格必须被跳过：图集里没有 U+0020 的 quad，但渲染器会为它推进笔位。
	// 这里一旦判成缺字，「John Doe」这类最常见的昵称会整条回退 FreeType。
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("John Doe", AsciiOnly), 0u);
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("a b c", AllSupported), 0u);

	// 零宽/不可见码点同样不需要字形。昵称里最常见的 emoji 形态是
	// 「基础字符 + 变体选择符」：基础字符在图集里，U+FE0F 永远不在。
	const auto StarsOnly = [](uint32_t Codepoint) { return Codepoint == 0x2B50; };
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("\u2B50\uFE0F", StarsOnly), 0u);
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("\u2B50\u200D\u2B50", StarsOnly), 0u);
	// 不可见码点不能掩盖真缺失：后面的 U+2605 仍必须被报出来
	EXPECT_EQ(QmNameplateMsdfFirstMissingCodepoint("\u2B50\uFE0F\u2605", StarsOnly), 0x2605u);
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

TEST(QmNameplateMsdfGate, InvisibleCodepointsNeedNoGlyph)
{
	// 空白：渲染器用固定 em 比例单独推进笔位，图集里没有它们的 quad
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(' '));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph('\t'));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph('\n'));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph('\r'));
	// 变体选择符、零宽连接/分隔符、软连字符：昵称里最常见的不可见修饰
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0xFE0E));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0xFE0F));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0x200B));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0x200C));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0x200D));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0x00AD));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0x202E));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0xFEFF));
	EXPECT_FALSE(QmNameplateMsdfCodepointNeedsGlyph(0xE0100));
	// 有实际宽度的填充符与普通字符必须仍然需要字形，否则会被静默画丢
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph(0x3164));
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph(0xFFA0));
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph('A'));
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph(0x2B50));
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph(0x4E2D));
	EXPECT_TRUE(QmNameplateMsdfCodepointNeedsGlyph(0x1F600));
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
