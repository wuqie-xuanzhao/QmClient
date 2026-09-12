// QmClient: 铭牌 MSDF 门控与回退诊断工具。
//
// 门控：图集按固定字体族离线预烤（base 页 DejaVu Sans，CJK 页 Source Han Sans SC），
// 自定义字体与图集族不匹配时整条铭牌保持 FreeType 路径，避免与用户所选字体不一致。
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

// 图集固定字体族：base 页烤 DejaVu Sans，CJK 页烤 Source Han Sans SC。
// 配置字体串包含任一族名（忽略大小写，对齐字体下拉框的 str_find_nocase 匹配）才算命中。
inline bool QmNameplateMsdfFontMatchesAtlas(const char *pConfiguredFont)
{
	static const char *const s_apAtlasFamilies[] = {"DejaVu Sans", "Source Han Sans SC"};
	if(pConfiguredFont == nullptr || pConfiguredFont[0] == '\0')
		return false;
	for(const char *pFamily : s_apAtlasFamilies)
	{
		if(str_find_nocase(pConfiguredFont, pFamily) != nullptr)
			return true;
	}
	return false;
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
