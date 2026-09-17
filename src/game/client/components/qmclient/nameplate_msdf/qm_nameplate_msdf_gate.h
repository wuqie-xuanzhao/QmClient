// QmClient: 铭牌 MSDF 门控与回退诊断工具。
//
// 门控：只有存在正式预生成 profile 的客户端内置字体才允许走 MSDF；
// 语言缺口由渲染器加载随包的 fallback profile 补齐。
// 用户字体和缺少 profile 的字体统一走 FreeType。
//
// 回退诊断：SupportsText 判定失败时定位首个缺失码点，并按码点去重报告，
// 日志量级受字符集约束（几十条封顶），不会因每帧刷屏。
//
// 本头全部为 inline/模板实现，生产路径（nameplates / renderer）与单元测试共用同一份逻辑。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_MSDF_QM_NAMEPLATE_MSDF_GATE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_MSDF_QM_NAMEPLATE_MSDF_GATE_H

#include <base/system.h>

#include <cstddef>
#include <cstdint>
#include <unordered_set>

// UTF-8 解码：返回码点并推进指针；非法序列返回 0xFFFD 并前进一个字节。
inline uint32_t QmNameplateMsdfDecodeUtf8(const char *&p)
{
	const unsigned char C0 = (unsigned char)*p;
	if(C0 == 0)
		return 0;
	if(C0 < 0x80)
	{
		++p;
		return C0;
	}
	int Len = 1;
	uint32_t Cp = 0;
	if((C0 & 0xE0) == 0xC0)
	{
		Len = 2;
		Cp = C0 & 0x1Fu;
	}
	else if((C0 & 0xF0) == 0xE0)
	{
		Len = 3;
		Cp = C0 & 0x0Fu;
	}
	else if((C0 & 0xF8) == 0xF0)
	{
		Len = 4;
		Cp = C0 & 0x07u;
	}
	else
	{
		++p;
		return 0xFFFD;
	}
	for(int i = 1; i < Len; ++i)
	{
		const unsigned char C = (unsigned char)p[i];
		if((C & 0xC0) != 0x80)
		{
			++p;
			return 0xFFFD;
		}
		Cp = (Cp << 6) | (C & 0x3Fu);
	}
	p += Len;
	return Cp;
}

// 返回当前已验收字体对应的图集 profile。未知字体必须返回 nullptr。
// profile 名称同时作为 data/qmclient/nameplate_msdf/profiles/ 下的 manifest 文件名。
inline const char *QmNameplateMsdfFontProfile(const char *pConfiguredFont)
{
	if(pConfiguredFont == nullptr || pConfiguredFont[0] == '\0')
		return nullptr;
	if(str_comp_nocase(pConfiguredFont, "Noto Sans SC") == 0 || str_comp_nocase(pConfiguredFont, "NotoSansSC") == 0 ||
		str_comp_nocase(pConfiguredFont, "Glow Sans J Compressed Book") == 0 || str_comp_nocase(pConfiguredFont, "Glow Sans J") == 0 ||
		str_comp_nocase(pConfiguredFont, "GlowSansJ-Compressed-Book") == 0)
		return "noto_glow_cjk";
	struct SFontProfile
	{
		const char *m_pFamily;
		const char *m_pProfile;
	};
	static constexpr SFontProfile s_aProfiles[] = {
		{"DejaVu Sans", "dejavu"},
		{"Cabin", "cabin"},
		{"FreeSans", "freesans"},
		{"FreeSans Bold", "freesans"},
		{"Google Sans", "google_sans"},
		{"Inter", "inter_regular"},
		{"Inter SemiBold", "inter_semibold"},
		{"Maple Mono Normal", "maple_mono_regular"},
		{"Maple Mono Normal CN", "maple_mono_regular"},
		{"Maple Mono Normal Bold", "maple_mono_bold"},
		{"Minecraft", "minecraft"},
		{"Montserrat", "montserrat"},
		{"Nunito", "nunito"},
		{"Nunito Black", "nunito"},
		{"Poppins", "poppins_regular"},
		{"Poppins Medium", "poppins_medium"},
		{"Poppins Bold", "poppins_bold"},
		{"Rubik", "rubik"},
		{"Times New Roman", "times_new_roman"},
	};
	for(const SFontProfile &Profile : s_aProfiles)
		if(str_comp_nocase(pConfiguredFont, Profile.m_pFamily) == 0)
			return Profile.m_pProfile;
	return nullptr;
}

inline bool QmNameplateMsdfFontMatchesAtlas(const char *pConfiguredFont)
{
	return QmNameplateMsdfFontProfile(pConfiguredFont) != nullptr;
}

// 该码点是否需要图集字形。返回 false 的码点在渲染器里既不出 quad 也不推进笔位。
//
// 两类：
//  1) 空白（空格/换行/制表符）——渲染器用固定 em 比例单独推进笔位；
//  2) Unicode Default_Ignorable_Code_Point——零宽、不可见，图集里永远没有 quad。
//
// 第 2 类必须一并跳过：昵称里最常见的 emoji 形态是「基础字符 + 变体选择符」
// （⭐️ = U+2B50 U+FE0F，❤️、☺️ 同理），基础字符已经在图集里，但 U+FE0F 不在。
// 若把它判成缺字，整条铭牌会因为一个看不见的字符回退 FreeType。
inline bool QmNameplateMsdfCodepointNeedsGlyph(uint32_t Codepoint)
{
	if(Codepoint == ' ' || Codepoint == '\n' || Codepoint == '\r' || Codepoint == '\t')
		return false;
	// Default_Ignorable_Code_Point 的实用子集（只列昵称/文本里实际可能出现的区段；
	// U+3164 / U+FFA0 这两个「填充符」在 CJK 字体里有实际宽度，故意不算不可见）。
	static constexpr uint32_t s_aInvisibleRanges[][2] = {
		{0x00AD, 0x00AD}, // SOFT HYPHEN
		{0x034F, 0x034F}, // COMBINING GRAPHEME JOINER
		{0x061C, 0x061C}, // ARABIC LETTER MARK
		{0x115F, 0x1160}, // HANGUL CHOSEONG / JUNGSEONG FILLER
		{0x17B4, 0x17B5}, // KHMER VOWEL INHERENT AQ / AA
		{0x180B, 0x180E}, // MONGOLIAN FREE VARIATION SELECTOR ONE..FOUR / VOWEL SEPARATOR
		{0x200B, 0x200F}, // ZWSP / ZWNJ / ZWJ / LRM / RLM
		{0x202A, 0x202E}, // LRE / RLE / PDF / LRO / RLO
		{0x2060, 0x206F}, // WORD JOINER..INVISIBLE PLUS 与已废弃格式符
		{0xFE00, 0xFE0F}, // VARIATION SELECTOR ONE..SIXTEEN
		{0xFEFF, 0xFEFF}, // ZERO WIDTH NO-BREAK SPACE
		{0xFFF0, 0xFFF8}, // 保留区 / 行间注记控制符
		{0x1BCA0, 0x1BCA3}, // SHORTHAND FORMAT LETTER OVERLAP..UP STEP
		{0x1D173, 0x1D17A}, // MUSICAL SYMBOL BEGIN BEAM..END PHRASE
		{0xE0000, 0xE0FFF}, // TAG 字符与 VARIATION SELECTOR 17..256
	};
	for(const auto &Range : s_aInvisibleRanges)
	{
		if(Codepoint >= Range[0] && Codepoint <= Range[1])
			return false;
	}
	return true;
}

// 遍历文本，返回第一个需要字形但 HasGlyph 判定为缺失的码点；
// 返回 0 表示全部可渲染。HasGlyph 由调用方提供（生产环境查图集字形表；测试可注入）。
template<typename HasGlyphFn>
uint32_t QmNameplateMsdfFirstMissingCodepoint(const char *pText, HasGlyphFn &&HasGlyph)
{
	if(pText == nullptr)
		return 0;
	const char *p = pText;
	while(*p != '\0')
	{
		const uint32_t Codepoint = QmNameplateMsdfDecodeUtf8(p);
		if(Codepoint == 0)
			break;
		if(!QmNameplateMsdfCodepointNeedsGlyph(Codepoint))
			continue;
		if(!HasGlyph(Codepoint))
			return Codepoint;
	}
	return 0;
}

// 整名回退日志去重：同一缺失码点在整个会话内只报告一次。
class CQmNameplateMsdfFallbackReporter
{
	std::unordered_set<uint32_t> m_Reported;

public:
	// 返回 true 表示该码点是首次出现（调用方应输出一行日志）。
	bool ShouldReport(uint32_t Codepoint) { return m_Reported.insert(Codepoint).second; }
	size_t ReportedCount() const { return m_Reported.size(); }
};

#endif
