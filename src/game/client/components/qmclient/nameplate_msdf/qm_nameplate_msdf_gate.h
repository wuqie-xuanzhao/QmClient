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
	if(str_comp_nocase(pConfiguredFont, "DejaVu Sans") == 0 ||
		str_comp_nocase(pConfiguredFont, "Noto Sans SC") == 0 || str_comp_nocase(pConfiguredFont, "NotoSansSC") == 0 ||
		str_comp_nocase(pConfiguredFont, "Glow Sans J Compressed Book") == 0 || str_comp_nocase(pConfiguredFont, "Glow Sans J") == 0 ||
		str_comp_nocase(pConfiguredFont, "GlowSansJ-Compressed-Book") == 0)
		return "noto_glow_cjk";
	struct SFontProfile { const char *m_pFamily; const char *m_pProfile; };
	static constexpr SFontProfile s_aProfiles[] = {
		{"Cabin", "cabin"}, {"FreeSans", "freesans"}, {"FreeSans Bold", "freesans"}, {"Google Sans", "google_sans"},
		{"Inter", "inter_regular"}, {"Inter SemiBold", "inter_semibold"},
		{"Maple Mono Normal", "maple_mono_regular"}, {"Maple Mono Normal CN", "maple_mono_regular"},
		{"Maple Mono Normal Bold", "maple_mono_bold"}, {"Minecraft", "minecraft"}, {"Montserrat", "montserrat"},
		{"Nunito Black", "nunito"}, {"Poppins", "poppins_regular"}, {"Poppins Medium", "poppins_medium"},
		{"Poppins Bold", "poppins_bold"}, {"Rubik", "rubik"},
		{"Times New Roman", "times_new_roman"}, {"LXGW WenKai", "lxgw_wenkai_regular"},
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

// 遍历文本（换行/制表符按空白跳过），返回第一个 HasGlyph 判定为缺失的码点；
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
		// 换行与制表符按空白处理，不需要字形
		if(Codepoint == '\n' || Codepoint == '\r' || Codepoint == '\t')
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
