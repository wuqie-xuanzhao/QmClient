#include "qm_lyrics_render.h"

#include <base/math.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string_view>

namespace QmLyrics
{

	int SnapManualOffsetMsToStep(int OffsetMs)
	{
		const int AbsOffset = std::abs(OffsetMs);
		const int SnappedAbs = ((AbsOffset + LYRICS_OFFSET_STEP_MS / 2) / LYRICS_OFFSET_STEP_MS) * LYRICS_OFFSET_STEP_MS;
		return OffsetMs < 0 ? -SnappedAbs : SnappedAbs;
	}

	int64_t EffectivePlaybackOffsetMs(int ManualOffsetMs, int64_t TrackOffsetMs)
	{
		return (int64_t)SnapManualOffsetMsToStep(ManualOffsetMs) - TrackOffsetMs;
	}

	float LineTransitionDistance(float ActiveFontSize, float OtherFontSize, float LineSpacing, float SubtitleFontSize, int ActiveSubtitleCount)
	{
		return (LineHeight(ActiveFontSize, LineSpacing, SubtitleFontSize, ActiveSubtitleCount) + LineHeight(OtherFontSize, LineSpacing, SubtitleFontSize, 0)) * 0.5f;
	}

	float EaseOutCubic(float T)
	{
		T = std::clamp(T, 0.0f, 1.0f);
		const float Inv = 1.0f - T;
		return 1.0f - Inv * Inv * Inv;
	}

	float MotionScaleForLevel(int MotionLevel)
	{
		if(MotionLevel <= 0)
			return 0.0f;
		return MotionLevel == 1 ? 0.6f : 1.0f;
	}

	int FindActiveLineIndex(const SLyricsTrack &Track, int64_t NowMs)
	{
		int Lo = 0;
		int Hi = (int)Track.m_vLines.size() - 1;
		while(Lo <= Hi)
		{
			const int Mid = (Lo + Hi) / 2;
			const SLyricsLine &Line = Track.m_vLines[Mid];
			if(NowMs < Line.m_StartMs)
				Hi = Mid - 1;
			else if(NowMs >= Line.m_EndMs)
				Lo = Mid + 1;
			else
				return Mid;
		}
		return -1;
	}

	int ResolveDisplayLineIndex(const SLyricsTrack &Track, int ActiveLineIndex, int64_t NowMs)
	{
		if(Track.m_vLines.empty())
			return -1;
		if(ActiveLineIndex >= 0)
			return ActiveLineIndex;
		if(NowMs < Track.m_vLines.front().m_StartMs)
			return 0;
		int Lo = 0;
		int Hi = (int)Track.m_vLines.size() - 1;
		int Best = 0;
		while(Lo <= Hi)
		{
			const int Mid = (Lo + Hi) / 2;
			if(Track.m_vLines[Mid].m_StartMs <= NowMs)
			{
				Best = Mid;
				Lo = Mid + 1;
			}
			else
			{
				Hi = Mid - 1;
			}
		}
		return Best;
	}

	size_t FindNextWordTextOffset(const std::string &RawText, const std::string &WordText, size_t SearchOffset)
	{
		if(RawText.empty() || WordText.empty() || SearchOffset >= RawText.size())
			return std::string::npos;
		return RawText.find(WordText, SearchOffset);
	}

	bool HasOnlyAsciiSpacingBetweenWords(const std::string &RawText, size_t SearchOffset, size_t WordOffset)
	{
		if(WordOffset == std::string::npos || WordOffset <= SearchOffset || SearchOffset >= RawText.size())
			return false;
		const size_t MissingEnd = minimum(WordOffset, RawText.size());
		for(size_t i = SearchOffset; i < MissingEnd; ++i)
		{
			if(RawText[i] != ' ' && RawText[i] != '\t')
				return false;
		}
		return true;
	}

	float LinePlayProgress(const SLyricsLine &Line, int64_t NowMs)
	{
		const int64_t Duration = std::max<int64_t>(1, Line.m_EndMs - Line.m_StartMs);
		return std::clamp((float)(NowMs - Line.m_StartMs) / (float)Duration, 0.0f, 1.0f);
	}

	float WordPlayProgress(const SLyricsWord &Word, int64_t NowMs)
	{
		const int64_t Duration = std::max<int64_t>(1, Word.m_EndMs - Word.m_StartMs);
		return std::clamp((float)(NowMs - Word.m_StartMs) / (float)Duration, 0.0f, 1.0f);
	}

	float LongWordPulseBrightness(const SLyricsWord &Word, int64_t NowMs)
	{
		const int64_t Duration = Word.m_EndMs - Word.m_StartMs;
		const float Progress = WordPlayProgress(Word, NowMs);
		if(Duration < 1500 || Progress <= 0.0f || Progress >= 1.0f)
			return 1.0f;
		return 1.0f + 0.14f * std::sin((float)(NowMs - Word.m_StartMs) * 0.010472f);
	}

	float LineHeight(float PrimaryFontSize, float LineSpacing, float SubtitleFontSize, int SubtitleCount)
	{
		float Height = PrimaryFontSize;
		for(int i = 0; i < SubtitleCount; ++i)
			Height += SubtitleFontSize + maximum(1.0f, LineSpacing * 0.35f);
		return Height + LineSpacing;
	}

	void UpdateTextWidthCache(STextWidthCache *pCache, const char *pText, float FontSize, size_t ContextHash, void *pUser, FTextMeasureCallback pfnMeasure)
	{
		if(pCache == nullptr || pfnMeasure == nullptr)
			return;

		const char *pMeasureText = pText != nullptr ? pText : "";
		const std::string_view MeasureText(pMeasureText);
		const size_t TextHash = std::hash<std::string_view>{}(MeasureText);
		if(pCache->m_Valid && pCache->m_FontSize == FontSize && pCache->m_ContextHash == ContextHash && pCache->m_TextHash == TextHash)
			return;

		pCache->m_FontSize = FontSize;
		pCache->m_ContextHash = ContextHash;
		pCache->m_TextHash = TextHash;
		pCache->m_TextWidth = pfnMeasure(pUser, FontSize, pMeasureText);
		pCache->m_Valid = true;
	}

	void UpdateLineTextWidthCache(SLineTextWidthCache *pCache, const SLyricsLine &Line, float FontSize, size_t ContextHash, void *pUser, FTextMeasureCallback pfnMeasure)
	{
		if(pCache == nullptr || pfnMeasure == nullptr)
			return;

		size_t TextHash = std::hash<std::string>{}(Line.m_RawText);
		for(const SLyricsWord &Word : Line.m_vWords)
		{
			TextHash ^= std::hash<std::string>{}(Word.m_Text) + 0x9e3779b9 + (TextHash << 6) + (TextHash >> 2);
		}
		if(pCache->m_Valid && pCache->m_FontSize == FontSize && pCache->m_ContextHash == ContextHash && pCache->m_TextHash == TextHash && pCache->m_vWordWidths.size() == Line.m_vWords.size())
			return;

		pCache->m_FontSize = FontSize;
		pCache->m_ContextHash = ContextHash;
		pCache->m_TextHash = TextHash;
		pCache->m_RawTextWidth = pfnMeasure(pUser, FontSize, Line.m_RawText.empty() ? "♪" : Line.m_RawText.c_str());
		pCache->m_SpaceWidth = pfnMeasure(pUser, FontSize, " ");
		pCache->m_vWordWidths.clear();
		pCache->m_vWordWidths.reserve(Line.m_vWords.size());
		for(const SLyricsWord &Word : Line.m_vWords)
			pCache->m_vWordWidths.push_back(pfnMeasure(pUser, FontSize, Word.m_Text.c_str()));
		pCache->m_Valid = true;
	}

	SLineVisual BuildLineVisual(int LineIndex, int ActiveLineIndex, float PrimaryAnchorY, float DistanceFromActive, float FontActive, float FontOther, float LineSpacing, float Opacity, float InactiveMinOpacity, float ScaleActive, float ScaleFalloff, float FadePerLine, int SubtitleCount, float SubtitleFontSize)
	{
		const int Distance = std::abs(LineIndex - ActiveLineIndex);
		SLineVisual Visual;
		Visual.m_Scale = LineIndex == ActiveLineIndex ? ScaleActive : std::max(0.5f, 1.0f - (float)Distance * ScaleFalloff);
		Visual.m_Alpha = LineIndex == ActiveLineIndex ? Opacity : std::max(InactiveMinOpacity, Opacity * (1.0f - (float)Distance * FadePerLine));
		Visual.m_FontSize = (LineIndex == ActiveLineIndex ? FontActive : FontOther) * Visual.m_Scale;
		Visual.m_Height = LineHeight(Visual.m_FontSize, LineSpacing, SubtitleFontSize, SubtitleCount);
		Visual.m_PrimaryY = PrimaryAnchorY + DistanceFromActive - Visual.m_FontSize * 0.5f;
		Visual.m_SubtitleY = Visual.m_PrimaryY + Visual.m_FontSize + maximum(1.0f, LineSpacing * 0.35f);
		return Visual;
	}

} // namespace QmLyrics
