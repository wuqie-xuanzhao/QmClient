#include "qm_lyrics_parser_lrc.h"

#include <base/system.h>

#include <algorithm>
#include <charconv>
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
			while(!View.empty() && (View.front() == ' ' || View.front() == '\t' || View.front() == '\r'))
				View.remove_prefix(1);
			while(!View.empty() && (View.back() == ' ' || View.back() == '\t' || View.back() == '\r'))
				View.remove_suffix(1);
			return View;
		}

		// 解析 [mm:ss.fff] / [mm:ss.ff] / [mm:ss] / [h:mm:ss.fff]
		// 返回字符数（含括号），失败返回 0；成功填 *pOutMs。
		size_t ParseBracketTime(const char *pStr, size_t Avail, int64_t *pOutMs)
		{
			if(Avail < 5 || pStr[0] != '[')
				return 0;
			const char *pEnd = nullptr;
			for(size_t i = 1; i < Avail; ++i)
			{
				if(pStr[i] == ']')
				{
					pEnd = pStr + i;
					break;
				}
			}
			if(pEnd == nullptr)
				return 0;

			const char *p = pStr + 1;
			const size_t InnerLen = (size_t)(pEnd - p);

			// 元数据 tag（如 [ar:xxx]）首字符是字母——拒绝。
			if(InnerLen > 0 && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')))
				return 0;

			int64_t Hours = 0, Mins = 0, Secs = 0, Frac = 0, FracDigits = 0;
			const char *pColon1 = nullptr;
			const char *pColon2 = nullptr;
			const char *pDot = nullptr;
			for(size_t i = 0; i < InnerLen; ++i)
			{
				if(p[i] == ':')
				{
					if(pColon1 == nullptr)
						pColon1 = p + i;
					else if(pColon2 == nullptr)
						pColon2 = p + i;
					else
						return 0;
				}
				else if(p[i] == '.')
				{
					if(pDot != nullptr)
						return 0;
					pDot = p + i;
				}
				else if(p[i] < '0' || p[i] > '9')
					return 0;
			}
			if(pColon1 == nullptr)
				return 0;

			auto ParseInt = [](const char *pStart, const char *pStop, int64_t *pOut) -> bool {
				int64_t Val = 0;
				for(const char *q = pStart; q < pStop; ++q)
				{
					if(*q < '0' || *q > '9')
						return false;
					Val = Val * 10 + (*q - '0');
				}
				*pOut = Val;
				return true;
			};

			if(pColon2 != nullptr)
			{
				if(!ParseInt(p, pColon1, &Hours))
					return 0;
				if(!ParseInt(pColon1 + 1, pColon2, &Mins))
					return 0;
				const char *pSecEnd = pDot != nullptr ? pDot : pEnd;
				if(!ParseInt(pColon2 + 1, pSecEnd, &Secs))
					return 0;
			}
			else
			{
				if(!ParseInt(p, pColon1, &Mins))
					return 0;
				const char *pSecEnd = pDot != nullptr ? pDot : pEnd;
				if(!ParseInt(pColon1 + 1, pSecEnd, &Secs))
					return 0;
			}

			if(pDot != nullptr)
			{
				const char *q = pDot + 1;
				while(q < pEnd && FracDigits < 3)
				{
					if(*q < '0' || *q > '9')
						return 0;
					Frac = Frac * 10 + (*q - '0');
					++q;
					++FracDigits;
				}
				while(FracDigits < 3)
				{
					Frac *= 10;
					++FracDigits;
				}
			}

			*pOutMs = ((Hours * 60 + Mins) * 60 + Secs) * 1000 + Frac;
			return (size_t)(pEnd - pStr + 1);
		}

		// 解析 <mm:ss.fff> 内联词时间戳
		size_t ParseAngleTime(const char *pStr, size_t Avail, int64_t *pOutMs)
		{
			if(Avail < 5 || pStr[0] != '<')
				return 0;
			const char *pEnd = nullptr;
			for(size_t i = 1; i < Avail; ++i)
			{
				if(pStr[i] == '>')
				{
					pEnd = pStr + i;
					break;
				}
			}
			if(pEnd == nullptr)
				return 0;

			// 借用 ParseBracketTime 的逻辑：把 [ 临时换 ]，复用解析。
			char aBuf[32];
			const size_t InnerLen = (size_t)(pEnd - pStr - 1);
			if(InnerLen + 2 >= sizeof(aBuf))
				return 0;
			aBuf[0] = '[';
			memcpy(aBuf + 1, pStr + 1, InnerLen);
			aBuf[InnerLen + 1] = ']';
			int64_t Ms = 0;
			if(ParseBracketTime(aBuf, InnerLen + 2, &Ms) == 0)
				return 0;
			*pOutMs = Ms;
			return (size_t)(pEnd - pStr + 1);
		}

		// 元数据 tag [key:value]，如 [ti:Title]
		bool ParseMetaTag(std::string_view Line, std::string_view *pOutKey, std::string_view *pOutValue)
		{
			if(Line.size() < 4 || Line.front() != '[' || Line.back() != ']')
				return false;
			std::string_view Inner = Line.substr(1, Line.size() - 2);
			const size_t Colon = Inner.find(':');
			if(Colon == std::string_view::npos || Colon == 0)
				return false;
			const std::string_view Key = Inner.substr(0, Colon);
			// 拒绝纯数字 key（那是时间戳）
			for(char C : Key)
			{
				if(C < 'a' || C > 'z')
				{
					if(C < 'A' || C > 'Z')
						return false;
				}
			}
			*pOutKey = Key;
			*pOutValue = TrimView(Inner.substr(Colon + 1));
			return true;
		}

		struct SParsedLine
		{
			int64_t m_StartMs = 0;
			std::string m_RawText;
			std::vector<SLyricsWord> m_vWords;
			bool m_HasWords = false;
		};

		// 解析单个 [time]text 块（已剥离前置时间戳），可能含 <time>word<time>word 序列。
		bool ParseLineBody(std::string_view Body, int64_t LineStartMs, SParsedLine *pOut)
		{
			pOut->m_StartMs = LineStartMs;

			std::string PlainText;
			PlainText.reserve(Body.size());
			std::vector<int64_t> vWordStarts;
			std::vector<std::string> vWordTexts;

			const char *p = Body.data();
			const char *pEnd = p + Body.size();
			std::string CurrentWord;
			int64_t CurrentWordStart = -1;
			bool HasInlineTimestamp = false;

			while(p < pEnd)
			{
				if(*p == '<')
				{
					int64_t Ms = 0;
					const size_t Consumed = ParseAngleTime(p, (size_t)(pEnd - p), &Ms);
					if(Consumed > 0)
					{
						HasInlineTimestamp = true;
						if(CurrentWordStart >= 0)
						{
							vWordStarts.push_back(CurrentWordStart);
							vWordTexts.push_back(std::move(CurrentWord));
							CurrentWord.clear();
						}
						CurrentWordStart = Ms;
						p += Consumed;
						continue;
					}
				}
				CurrentWord.push_back(*p);
				PlainText.push_back(*p);
				++p;
			}
			if(CurrentWordStart >= 0)
			{
				vWordStarts.push_back(CurrentWordStart);
				vWordTexts.push_back(std::move(CurrentWord));
			}

			pOut->m_RawText = std::move(PlainText);

			if(HasInlineTimestamp)
			{
				pOut->m_HasWords = true;
				for(size_t i = 0; i < vWordStarts.size(); ++i)
				{
					SLyricsWord W;
					W.m_StartMs = vWordStarts[i];
					W.m_EndMs = (i + 1 < vWordStarts.size()) ? vWordStarts[i + 1] : vWordStarts[i];
					W.m_Text = std::move(vWordTexts[i]);
					pOut->m_vWords.push_back(std::move(W));
				}
			}

			return true;
		}

	} // anonymous namespace

	bool ParseLrc(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr, size_t ErrSize)
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
		pOut->m_Format = EFormat::LRC_STANDARD;

		bool HasAnyWords = false;
		std::vector<SParsedLine> vParsed;
		vParsed.reserve(64);

		const char *p = pText;
		const char *pEnd = pText + TextSize;
		while(p < pEnd)
		{
			const char *pLineStart = p;
			while(p < pEnd && *p != '\n')
				++p;
			std::string_view Line(pLineStart, (size_t)(p - pLineStart));
			if(p < pEnd)
				++p;
			Line = TrimView(Line);
			if(Line.empty())
				continue;

			std::string_view MetaKey, MetaValue;
			if(ParseMetaTag(Line, &MetaKey, &MetaValue))
			{
				std::string Key(MetaKey);
				std::string Value(MetaValue);
				if(Key == "ti")
					pOut->m_Title = Value;
				else if(Key == "ar")
					pOut->m_Artist = Value;
				else if(Key == "al")
					pOut->m_Album = Value;
				else if(Key == "offset")
				{
					char *pStop = nullptr;
					const long Offset = std::strtol(Value.c_str(), &pStop, 10);
					if(pStop != Value.c_str())
						pOut->m_OffsetMs = (int64_t)Offset;
				}
				continue;
			}

			// 收集所有前置 [time]
			std::vector<int64_t> vStartTimes;
			size_t Cursor = 0;
			while(Cursor < Line.size())
			{
				int64_t Ms = 0;
				const size_t Consumed = ParseBracketTime(Line.data() + Cursor, Line.size() - Cursor, &Ms);
				if(Consumed == 0)
					break;
				vStartTimes.push_back(Ms);
				Cursor += Consumed;
			}
			if(vStartTimes.empty())
				continue;

			std::string_view Body = TrimView(Line.substr(Cursor));
			for(int64_t StartMs : vStartTimes)
			{
				SParsedLine Parsed;
				if(!ParseLineBody(Body, StartMs, &Parsed))
					continue;
				if(Parsed.m_HasWords)
					HasAnyWords = true;
				vParsed.push_back(std::move(Parsed));
			}
		}

		if(vParsed.empty())
		{
			SetError(pErr, ErrSize, "no parseable lyric lines");
			return false;
		}

		std::sort(vParsed.begin(), vParsed.end(), [](const SParsedLine &A, const SParsedLine &B) {
			return A.m_StartMs < B.m_StartMs;
		});

		pOut->m_Format = HasAnyWords ? EFormat::LRC_ENHANCED : EFormat::LRC_STANDARD;
		pOut->m_vLines.reserve(vParsed.size());
		for(size_t i = 0; i < vParsed.size(); ++i)
		{
			SLyricsLine L;
			L.m_StartMs = vParsed[i].m_StartMs;
			L.m_RawText = std::move(vParsed[i].m_RawText);
			L.m_vWords = std::move(vParsed[i].m_vWords);
			if(i + 1 < vParsed.size())
				L.m_EndMs = vParsed[i + 1].m_StartMs;
			else
			{
				int64_t MaxWordEnd = L.m_StartMs;
				for(const SLyricsWord &W : L.m_vWords)
					MaxWordEnd = std::max(MaxWordEnd, W.m_EndMs);
				L.m_EndMs = std::max(MaxWordEnd, L.m_StartMs + LAST_LINE_TAIL_MS);
			}
			// 词级时间轴最后一个词的 EndMs 兜底
			if(!L.m_vWords.empty() && L.m_vWords.back().m_EndMs <= L.m_vWords.back().m_StartMs)
				L.m_vWords.back().m_EndMs = L.m_EndMs;
			pOut->m_vLines.push_back(std::move(L));
		}

		return true;
	}

} // namespace QmLyrics
