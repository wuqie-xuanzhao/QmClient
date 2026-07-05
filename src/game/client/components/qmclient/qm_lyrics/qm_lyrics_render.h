#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_RENDER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_RENDER_H

#include "qm_lyrics_model.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace QmLyrics
{

	constexpr int LYRICS_OFFSET_STEP_MS = 500;

	struct SLineVisual
	{
		float m_Alpha = 1.0f;
		float m_FontSize = 12.0f;
		float m_PrimaryY = 0.0f;
		float m_SubtitleY = 0.0f;
		float m_Height = 0.0f;
		float m_Scale = 1.0f;
	};

	int SnapManualOffsetMsToStep(int OffsetMs);
	int64_t EffectivePlaybackOffsetMs(int ManualOffsetMs, int64_t TrackOffsetMs);
	float LineTransitionDistance(float ActiveFontSize, float OtherFontSize, float LineSpacing, float SubtitleFontSize, int ActiveSubtitleCount);
	float EaseOutCubic(float T);
	float MotionScaleForLevel(int MotionLevel);
	int FindActiveLineIndex(const SLyricsTrack &Track, int64_t NowMs);
	int ResolveDisplayLineIndex(const SLyricsTrack &Track, int ActiveLineIndex, int64_t NowMs);
	size_t FindNextWordTextOffset(const std::string &RawText, const std::string &WordText, size_t SearchOffset);
	bool HasOnlyAsciiSpacingBetweenWords(const std::string &RawText, size_t SearchOffset, size_t WordOffset);
	float LinePlayProgress(const SLyricsLine &Line, int64_t NowMs);
	float WordPlayProgress(const SLyricsWord &Word, int64_t NowMs);
	float LongWordPulseBrightness(const SLyricsWord &Word, int64_t NowMs);
	float LineHeight(float PrimaryFontSize, float LineSpacing, float SubtitleFontSize, int SubtitleCount);
	SLineVisual BuildLineVisual(int LineIndex, int ActiveLineIndex, float PrimaryAnchorY, float DistanceFromActive, float FontActive, float FontOther, float LineSpacing, float Opacity, float InactiveMinOpacity, float ScaleActive, float ScaleFalloff, float FadePerLine, int SubtitleCount, float SubtitleFontSize);
	using FTextMeasureCallback = float (*)(void *pUser, float FontSize, const char *pText);
	void UpdateTextWidthCache(STextWidthCache *pCache, const char *pText, float FontSize, size_t ContextHash, void *pUser, FTextMeasureCallback pfnMeasure);
	void UpdateLineTextWidthCache(SLineTextWidthCache *pCache, const SLyricsLine &Line, float FontSize, size_t ContextHash, void *pUser, FTextMeasureCallback pfnMeasure);

} // namespace QmLyrics

#endif
