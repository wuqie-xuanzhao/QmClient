// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_lyrics_parser_ttml.h"

#include <base/system.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace QmLyrics
{

	namespace
	{

		constexpr int64_t LAST_LINE_TAIL_MS = 5000;

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage, ErrSize);
		}

		std::string_view TrimView(std::string_view View)
		{
			while(!View.empty() && std::isspace((unsigned char)View.front()))
				View.remove_prefix(1);
			while(!View.empty() && std::isspace((unsigned char)View.back()))
				View.remove_suffix(1);
			return View;
		}

		// 实体引用解码（最常见的 5 种 + 数字引用）
		std::string DecodeEntities(std::string_view Text)
		{
			std::string Out;
			Out.reserve(Text.size());
			for(size_t i = 0; i < Text.size();)
			{
				if(Text[i] == '&')
				{
					const size_t Semi = Text.find(';', i);
					if(Semi != std::string_view::npos && Semi - i <= 8)
					{
						std::string_view Entity = Text.substr(i + 1, Semi - i - 1);
						if(Entity == "lt")
						{
							Out.push_back('<');
							i = Semi + 1;
							continue;
						}
						if(Entity == "gt")
						{
							Out.push_back('>');
							i = Semi + 1;
							continue;
						}
						if(Entity == "amp")
						{
							Out.push_back('&');
							i = Semi + 1;
							continue;
						}
						if(Entity == "quot")
						{
							Out.push_back('"');
							i = Semi + 1;
							continue;
						}
						if(Entity == "apos")
						{
							Out.push_back('\'');
							i = Semi + 1;
							continue;
						}
						if(!Entity.empty() && Entity.front() == '#')
						{
							int Base = 10;
							size_t Offset = 1;
							if(Entity.size() > 1 && (Entity[1] == 'x' || Entity[1] == 'X'))
							{
								Base = 16;
								Offset = 2;
							}
							std::string NumStr(Entity.substr(Offset));
							char *pStop = nullptr;
							const long Code = std::strtol(NumStr.c_str(), &pStop, Base);
							if(pStop != NumStr.c_str() && Code > 0 && Code < 0x80)
							{
								Out.push_back((char)Code);
								i = Semi + 1;
								continue;
							}
						}
					}
				}
				Out.push_back(Text[i]);
				++i;
			}
			return Out;
		}

	} // anonymous namespace

	bool ParseTtmlTime(const char *pStr, int64_t *pOutMs)
	{
		if(pStr == nullptr || pOutMs == nullptr || pStr[0] == '\0')
			return false;

		const size_t Len = std::strlen(pStr);

		// Sms 形式（如 "83456ms"）
		if(Len >= 3 && pStr[Len - 2] == 'm' && pStr[Len - 1] == 's')
		{
			char *pStop = nullptr;
			const double Val = std::strtod(pStr, &pStop);
			if(pStop == pStr + Len - 2)
			{
				*pOutMs = (int64_t)(Val + 0.5);
				return true;
			}
			return false;
		}

		// Ss 形式（如 "83.456s"）
		if(Len >= 2 && pStr[Len - 1] == 's')
		{
			char *pStop = nullptr;
			const double Val = std::strtod(pStr, &pStop);
			if(pStop == pStr + Len - 1)
			{
				*pOutMs = (int64_t)(Val * 1000.0 + 0.5);
				return true;
			}
			return false;
		}

		// h:mm:ss.fff / mm:ss.fff / mm:ss
		int Colons = 0;
		bool HasInvalid = false;
		for(size_t i = 0; i < Len; ++i)
		{
			if(pStr[i] == ':')
				++Colons;
			else if(pStr[i] != '.' && (pStr[i] < '0' || pStr[i] > '9'))
			{
				HasInvalid = true;
				break;
			}
		}
		if(HasInvalid || Colons == 0)
			return false;

		int64_t Hours = 0, Mins = 0, Secs = 0, Frac = 0, FracDigits = 0;
		const char *p = pStr;
		const char *pEnd = pStr + Len;
		const char *pDot = nullptr;
		for(const char *q = pStr; q < pEnd; ++q)
		{
			if(*q == '.')
			{
				pDot = q;
				break;
			}
		}
		const char *pIntEnd = pDot != nullptr ? pDot : pEnd;

		auto ReadField = [](const char *pS, const char *pE, int64_t *pOut) -> bool {
			if(pS >= pE)
				return false;
			int64_t Val = 0;
			for(const char *q = pS; q < pE; ++q)
			{
				if(*q < '0' || *q > '9')
					return false;
				Val = Val * 10 + (*q - '0');
			}
			*pOut = Val;
			return true;
		};

		if(Colons == 2)
		{
			const char *pC1 = nullptr;
			const char *pC2 = nullptr;
			for(const char *q = p; q < pIntEnd; ++q)
			{
				if(*q == ':')
				{
					if(pC1 == nullptr)
						pC1 = q;
					else
					{
						pC2 = q;
						break;
					}
				}
			}
			if(!ReadField(p, pC1, &Hours) || !ReadField(pC1 + 1, pC2, &Mins) || !ReadField(pC2 + 1, pIntEnd, &Secs))
				return false;
		}
		else if(Colons == 1)
		{
			const char *pC1 = nullptr;
			for(const char *q = p; q < pIntEnd; ++q)
			{
				if(*q == ':')
				{
					pC1 = q;
					break;
				}
			}
			if(!ReadField(p, pC1, &Mins) || !ReadField(pC1 + 1, pIntEnd, &Secs))
				return false;
		}
		else
		{
			return false;
		}

		if(pDot != nullptr)
		{
			for(const char *q = pDot + 1; q < pEnd && FracDigits < 3; ++q)
			{
				if(*q < '0' || *q > '9')
					return false;
				Frac = Frac * 10 + (*q - '0');
				++FracDigits;
			}
			while(FracDigits < 3)
			{
				Frac *= 10;
				++FracDigits;
			}
		}

		*pOutMs = ((Hours * 60 + Mins) * 60 + Secs) * 1000 + Frac;
		return true;
	}

	namespace
	{

		// 简易 XML 标签扫描器：在 pText 范围内找下一个 `<` 开始的标签。
		// 跳过注释 <!-- --> 和 CDATA。
		// 返回标签起点（指向 '<'），找不到返回 nullptr。
		const char *FindNextTag(const char *p, const char *pEnd)
		{
			while(p < pEnd)
			{
				if(*p == '<')
				{
					if(p + 4 <= pEnd && std::memcmp(p, "<!--", 4) == 0)
					{
						const char *pStop = nullptr;
						for(const char *q = p + 4; q + 3 <= pEnd; ++q)
						{
							if(std::memcmp(q, "-->", 3) == 0)
							{
								pStop = q + 3;
								break;
							}
						}
						p = pStop != nullptr ? pStop : pEnd;
						continue;
					}
					if(p + 9 <= pEnd && std::memcmp(p, "<![CDATA[", 9) == 0)
					{
						const char *pStop = nullptr;
						for(const char *q = p + 9; q + 3 <= pEnd; ++q)
						{
							if(std::memcmp(q, "]]>", 3) == 0)
							{
								pStop = q + 3;
								break;
							}
						}
						p = pStop != nullptr ? pStop : pEnd;
						continue;
					}
					return p;
				}
				++p;
			}
			return nullptr;
		}

		// 从已定位到 '<' 的位置读完整标签（直到 '>'）。
		// 返回标签结束位置（指向 '>'）的下一个字符；失败返回 nullptr。
		const char *FindTagEnd(const char *pStart, const char *pEnd)
		{
			bool InQuote = false;
			char QuoteCh = 0;
			for(const char *p = pStart + 1; p < pEnd; ++p)
			{
				if(InQuote)
				{
					if(*p == QuoteCh)
						InQuote = false;
				}
				else
				{
					if(*p == '"' || *p == '\'')
					{
						InQuote = true;
						QuoteCh = *p;
					}
					else if(*p == '>')
						return p + 1;
				}
			}
			return nullptr;
		}

		// 从标签内容里取标签名（剥离命名空间前缀）。
		std::string TagLocalName(std::string_view TagBody)
		{
			// 去掉前置 '/' 和空白
			size_t Cursor = 0;
			if(Cursor < TagBody.size() && TagBody[Cursor] == '/')
				++Cursor;
			while(Cursor < TagBody.size() && std::isspace((unsigned char)TagBody[Cursor]))
				++Cursor;
			size_t NameEnd = Cursor;
			while(NameEnd < TagBody.size())
			{
				const char C = TagBody[NameEnd];
				if(std::isspace((unsigned char)C) || C == '/' || C == '>')
					break;
				++NameEnd;
			}
			std::string_view Name = TagBody.substr(Cursor, NameEnd - Cursor);
			const size_t Colon = Name.find(':');
			if(Colon != std::string_view::npos)
				Name.remove_prefix(Colon + 1);
			std::string Out(Name);
			for(char &C : Out)
				C = (char)std::tolower((unsigned char)C);
			return Out;
		}

		// 从标签 body 提取属性值（local name 匹配，命名空间前缀忽略）。
		bool GetAttribute(std::string_view TagBody, std::string_view AttrName, std::string *pOut)
		{
			size_t i = 0;
			while(i < TagBody.size())
			{
				while(i < TagBody.size() && !std::isspace((unsigned char)TagBody[i]))
					++i;
				while(i < TagBody.size() && std::isspace((unsigned char)TagBody[i]))
					++i;
				if(i >= TagBody.size())
					break;
				const size_t NameStart = i;
				while(i < TagBody.size() && TagBody[i] != '=' && !std::isspace((unsigned char)TagBody[i]) && TagBody[i] != '/' && TagBody[i] != '>')
					++i;
				if(i >= TagBody.size() || TagBody[i] != '=')
					continue;
				std::string_view Name = TagBody.substr(NameStart, i - NameStart);
				const size_t Colon = Name.find(':');
				std::string_view Local = Colon == std::string_view::npos ? Name : Name.substr(Colon + 1);
				++i; // '='
				if(i >= TagBody.size())
					return false;
				const char Quote = TagBody[i];
				if(Quote != '"' && Quote != '\'')
					return false;
				++i;
				const size_t ValueStart = i;
				while(i < TagBody.size() && TagBody[i] != Quote)
					++i;
				if(i >= TagBody.size())
					return false;
				std::string_view Value = TagBody.substr(ValueStart, i - ValueStart);
				++i;
				if(Local == AttrName)
				{
					*pOut = DecodeEntities(Value);
					return true;
				}
			}
			return false;
		}

		bool IsSelfClosing(std::string_view TagBody)
		{
			size_t i = TagBody.size();
			while(i > 0 && std::isspace((unsigned char)TagBody[i - 1]))
				--i;
			return i > 0 && TagBody[i - 1] == '/';
		}

		bool IsCloseTag(std::string_view TagBody)
		{
			return !TagBody.empty() && TagBody.front() == '/';
		}

		struct SSpanCollected
		{
			int64_t m_StartMs = 0;
			int64_t m_EndMs = 0;
			std::string m_Text;
			std::string m_Role; // ttm:role 局部名 "role"
		};

		// 提取 <p>…</p> 的全部子节点为 SSpanCollected 列表。
		// 不识别复杂嵌套（如 span in span）；只支持平铺 span 和裸文本。
		void CollectSpans(const char *pContent, const char *pContentEnd, std::vector<SSpanCollected> *pOut, std::string *pPlainOut)
		{
			const char *p = pContent;
			while(p < pContentEnd)
			{
				const char *pTag = FindNextTag(p, pContentEnd);
				if(pTag == nullptr)
				{
					// 余下纯文本
					pPlainOut->append(DecodeEntities(std::string_view(p, (size_t)(pContentEnd - p))));
					break;
				}
				if(pTag > p)
					pPlainOut->append(DecodeEntities(std::string_view(p, (size_t)(pTag - p))));

				const char *pTagEnd = FindTagEnd(pTag, pContentEnd);
				if(pTagEnd == nullptr)
					break;

				std::string_view TagBody(pTag + 1, (size_t)(pTagEnd - pTag - 2));
				const std::string Name = TagLocalName(TagBody);

				if(Name != "span" || IsCloseTag(TagBody))
				{
					p = pTagEnd;
					continue;
				}

				SSpanCollected Span;
				std::string BeginAttr, EndAttr, RoleAttr;
				if(GetAttribute(TagBody, "begin", &BeginAttr))
					ParseTtmlTime(BeginAttr.c_str(), &Span.m_StartMs);
				if(GetAttribute(TagBody, "end", &EndAttr))
					ParseTtmlTime(EndAttr.c_str(), &Span.m_EndMs);
				if(GetAttribute(TagBody, "role", &RoleAttr))
					Span.m_Role = RoleAttr;

				if(IsSelfClosing(TagBody))
				{
					pOut->push_back(std::move(Span));
					p = pTagEnd;
					continue;
				}

				// 找匹配 </span>
				const char *pInner = pTagEnd;
				int Depth = 1;
				const char *pInnerEnd = nullptr;
				const char *q = pTagEnd;
				while(q < pContentEnd)
				{
					const char *pSubTag = FindNextTag(q, pContentEnd);
					if(pSubTag == nullptr)
						break;
					const char *pSubTagEnd = FindTagEnd(pSubTag, pContentEnd);
					if(pSubTagEnd == nullptr)
						break;
					std::string_view SubBody(pSubTag + 1, (size_t)(pSubTagEnd - pSubTag - 2));
					const std::string SubName = TagLocalName(SubBody);
					if(SubName == "span")
					{
						if(IsCloseTag(SubBody))
						{
							--Depth;
							if(Depth == 0)
							{
								pInnerEnd = pSubTag;
								q = pSubTagEnd;
								break;
							}
						}
						else if(!IsSelfClosing(SubBody))
							++Depth;
					}
					q = pSubTagEnd;
				}
				if(pInnerEnd == nullptr)
				{
					pOut->push_back(std::move(Span));
					break;
				}

				// 提取 span 内文本（含子 span 文本递归）
				std::vector<SSpanCollected> NestedDummy;
				std::string Inner;
				CollectSpans(pInner, pInnerEnd, &NestedDummy, &Inner);
				Span.m_Text = Inner;

				if(Span.m_Role == "x-translation" || Span.m_Role == "x-transliteration")
				{
					// 角色 span 不进主文本流
				}
				else
				{
					pPlainOut->append(Inner);
				}

				pOut->push_back(std::move(Span));
				p = q;
			}
		}

	} // anonymous namespace

	bool ParseTtml(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pText == nullptr || TextSize == 0)
		{
			SetError(pErr, ErrSize, "empty input");
			return false;
		}

		pOut->m_vLines.clear();
		pOut->m_Title.reset();
		pOut->m_Artist.reset();
		pOut->m_Album.reset();
		pOut->m_OffsetMs = 0;
		pOut->m_Format = EFormat::TTML;

		const char *p = pText;
		const char *pEnd = pText + TextSize;

		while(p < pEnd)
		{
			const char *pTag = FindNextTag(p, pEnd);
			if(pTag == nullptr)
				break;
			const char *pTagEnd = FindTagEnd(pTag, pEnd);
			if(pTagEnd == nullptr)
				break;

			std::string_view TagBody(pTag + 1, (size_t)(pTagEnd - pTag - 2));
			const std::string Name = TagLocalName(TagBody);

			if(Name != "p" || IsCloseTag(TagBody))
			{
				p = pTagEnd;
				continue;
			}

			SLyricsLine Line;
			std::string BeginAttr, EndAttr;
			if(GetAttribute(TagBody, "begin", &BeginAttr))
				ParseTtmlTime(BeginAttr.c_str(), &Line.m_StartMs);
			if(GetAttribute(TagBody, "end", &EndAttr))
				ParseTtmlTime(EndAttr.c_str(), &Line.m_EndMs);

			if(IsSelfClosing(TagBody))
			{
				p = pTagEnd;
				pOut->m_vLines.push_back(std::move(Line));
				continue;
			}

			// 找 </p>
			const char *pInner = pTagEnd;
			const char *pInnerEnd = nullptr;
			int Depth = 1;
			const char *q = pTagEnd;
			while(q < pEnd)
			{
				const char *pSubTag = FindNextTag(q, pEnd);
				if(pSubTag == nullptr)
					break;
				const char *pSubTagEnd = FindTagEnd(pSubTag, pEnd);
				if(pSubTagEnd == nullptr)
					break;
				std::string_view SubBody(pSubTag + 1, (size_t)(pSubTagEnd - pSubTag - 2));
				const std::string SubName = TagLocalName(SubBody);
				if(SubName == "p")
				{
					if(IsCloseTag(SubBody))
					{
						--Depth;
						if(Depth == 0)
						{
							pInnerEnd = pSubTag;
							q = pSubTagEnd;
							break;
						}
					}
					else if(!IsSelfClosing(SubBody))
						++Depth;
				}
				q = pSubTagEnd;
			}
			if(pInnerEnd == nullptr)
				break;

			std::vector<SSpanCollected> vSpans;
			std::string PlainText;
			CollectSpans(pInner, pInnerEnd, &vSpans, &PlainText);

			Line.m_RawText = TrimView(PlainText);
			for(const SSpanCollected &S : vSpans)
			{
				if(S.m_Role == "x-translation")
				{
					if(!Line.m_Translation.has_value())
						Line.m_Translation = S.m_Text;
					else
						*Line.m_Translation += S.m_Text;
				}
				else if(S.m_Role == "x-transliteration")
				{
					if(!Line.m_Transliteration.has_value())
						Line.m_Transliteration = S.m_Text;
					else
						*Line.m_Transliteration += S.m_Text;
				}
				else if(S.m_StartMs > 0 || S.m_EndMs > 0)
				{
					SLyricsWord W;
					W.m_StartMs = S.m_StartMs;
					W.m_EndMs = S.m_EndMs > 0 ? S.m_EndMs : S.m_StartMs;
					W.m_Text = S.m_Text;
					Line.m_vWords.push_back(std::move(W));
				}
			}

			pOut->m_vLines.push_back(std::move(Line));
			p = q;
		}

		if(pOut->m_vLines.empty())
		{
			SetError(pErr, ErrSize, "no <p> elements with content");
			return false;
		}

		std::sort(pOut->m_vLines.begin(), pOut->m_vLines.end(), [](const SLyricsLine &A, const SLyricsLine &B) {
			return A.m_StartMs < B.m_StartMs;
		});

		// EndMs 修正：若 0，取下一行 StartMs 或 +LAST_LINE_TAIL_MS
		for(size_t i = 0; i < pOut->m_vLines.size(); ++i)
		{
			if(pOut->m_vLines[i].m_EndMs > 0)
				continue;
			if(i + 1 < pOut->m_vLines.size())
				pOut->m_vLines[i].m_EndMs = pOut->m_vLines[i + 1].m_StartMs;
			else
				pOut->m_vLines[i].m_EndMs = pOut->m_vLines[i].m_StartMs + LAST_LINE_TAIL_MS;
		}

		return true;
	}

} // namespace QmLyrics
