// QmClient: 铭牌 MSDF manifest 的极简 JSON 取值工具。
//
// manifest 体积很大（CJK 页 1.3MB、数千字形），运行时不为它引入完整 JSON DOM，
// 而是按 key 直接定位后读值。本文件把这套扫描逻辑抽成纯函数，让生产解析器与
// 单元测试共用同一份实现。
//
// 关键不变量：manifest 是标准 JSON（"key": value）。定位到 key 文本之后，
// 必须再跳过 key 的结束引号与冒号才能取到 value——漏掉这一步会让所有取值失败，
// 表现为「资产正确、运行时整包解析失败」，MSDF 从不生效。契约测试
// （QmNameplateMsdfAtlas.RuntimeScanner*）用真实产物锁住这条不变量。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_MSDF_QM_NAMEPLATE_MSDF_MANIFEST_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_MSDF_QM_NAMEPLATE_MSDF_MANIFEST_H

#include <base/system.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// 有界子串查找：manifest 很大，不能用依赖 NUL 终止的 str_find。
inline const char *QmNameplateMsdfFindSpan(const char *pBegin, const char *pEnd, const char *pNeedle)
{
	const size_t NeedleLen = str_length(pNeedle);
	if(NeedleLen == 0 || pBegin == nullptr || pEnd == nullptr || (size_t)(pEnd - pBegin) < NeedleLen)
		return nullptr;
	const char *pLast = pEnd - NeedleLen;
	for(const char *p = pBegin; p <= pLast; ++p)
	{
		if(p[0] == pNeedle[0] && mem_comp(p, pNeedle, NeedleLen) == 0)
			return p;
	}
	return nullptr;
}

// 在 [pBegin,pEnd) 中定位 key（带引号或不带均可），返回 key 文本之后的位置。
inline const char *QmNameplateMsdfFindKey(const char *pBegin, const char *pEnd, const char *pKey)
{
	const char *p = QmNameplateMsdfFindSpan(pBegin, pEnd, pKey);
	if(p == nullptr)
		return nullptr;
	return p + str_length(pKey);
}

inline bool QmNameplateMsdfIsJsonBlank(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// 从 key 之后推进到 value 的首字符：跳过 key 的结束引号、冒号与空白。
// 注意 key 可能带引号（指针落在冒号上）也可能不带（指针落在结束引号上），两种都要能吃下。
inline const char *QmNameplateMsdfSkipToValue(const char *p, const char *pEnd)
{
	while(p < pEnd && QmNameplateMsdfIsJsonBlank(*p))
		++p;
	if(p < pEnd && *p == '"')
		++p; // key 的结束引号
	while(p < pEnd && (QmNameplateMsdfIsJsonBlank(*p) || *p == ':'))
		++p;
	return p;
}

// 读取数字；读不到返回 false。
inline bool QmNameplateMsdfReadDouble(const char *p, const char *pEnd, double &Out)
{
	char aBuffer[64];
	size_t i = 0;
	p = QmNameplateMsdfSkipToValue(p, pEnd);
	while(p < pEnd && i + 1 < sizeof(aBuffer) && (*p == '-' || *p == '+' || *p == '.' || (*p >= '0' && *p <= '9') || *p == 'e' || *p == 'E'))
	{
		aBuffer[i++] = *p++;
	}
	aBuffer[i] = '\0';
	if(i == 0)
		return false;
	Out = strtod(aBuffer, nullptr);
	return true;
}

// 读取双引号字符串（不含引号）；读不到返回 false。
inline bool QmNameplateMsdfReadString(const char *p, const char *pEnd, std::string &Out)
{
	p = QmNameplateMsdfSkipToValue(p, pEnd);
	if(p >= pEnd || *p != '"')
		return false;
	++p;
	const char *pStringEnd = p;
	while(pStringEnd < pEnd && *pStringEnd != '"')
		++pStringEnd;
	if(pStringEnd >= pEnd)
		return false;
	Out.assign(p, pStringEnd);
	return true;
}

// 读取 true/false；读不到返回 false。
inline bool QmNameplateMsdfReadBool(const char *p, const char *pEnd, bool &Out)
{
	p = QmNameplateMsdfSkipToValue(p, pEnd);
	if(p + 4 <= pEnd && strncmp(p, "true", 4) == 0)
	{
		Out = true;
		return true;
	}
	if(p + 5 <= pEnd && strncmp(p, "false", 5) == 0)
	{
		Out = false;
		return true;
	}
	return false;
}

// 遍历 glyphs 块里的 "<codepoint>":{...} 条目，对每条调用 Callback(Codepoint, pObjectBegin, pObjectEnd)。
// 返回成功处理的条目数；Callback 返回 false 时立即停止。
// 分隔符必须同时吃下 key 之后的冒号与条目之间的逗号/花括号——漏掉冒号会让整个块解析为空
// （历史缺陷：资产正确、运行时整包解析失败，MSDF 从不生效）。
template<typename Fn>
int QmNameplateMsdfForEachGlyphObject(const char *pGlyphs, const char *pEnd, Fn &&Callback)
{
	const char *p = pGlyphs;
	int Parsed = 0;
	while(p < pEnd)
	{
		while(p < pEnd && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',' || *p == '{' || *p == '}' || *p == ':'))
			++p;
		if(p >= pEnd || *p != '"')
			break;
		++p;
		char *pCodeEnd = nullptr;
		const long Codepoint = strtol(p, &pCodeEnd, 10);
		if(pCodeEnd == nullptr || pCodeEnd == p || Codepoint <= 0 || Codepoint > 0x10FFFF)
			break;
		p = pCodeEnd;
		const char *pObjectBegin = QmNameplateMsdfFindSpan(p, pEnd, "{");
		if(pObjectBegin == nullptr)
			break;
		const char *pObjectEnd = QmNameplateMsdfFindSpan(pObjectBegin, pEnd, "}");
		if(pObjectEnd == nullptr)
			break;
		if(!Callback(static_cast<uint32_t>(Codepoint), pObjectBegin, pObjectEnd))
			break;
		++Parsed;
		p = pObjectEnd + 1;
	}
	return Parsed;
}

#endif
