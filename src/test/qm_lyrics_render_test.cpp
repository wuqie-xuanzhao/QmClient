#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_render.h>

#include <gtest/gtest.h>

#include <cstring>
#include <utility>

using namespace QmLyrics;

namespace
{

	SLyricsTrack BuildThreeLineTrack()
	{
		SLyricsTrack Track;
		for(int i = 0; i < 3; ++i)
		{
			SLyricsLine Line;
			Line.m_StartMs = i * 1000;
			Line.m_EndMs = (i + 1) * 1000;
			Line.m_RawText = "line";
			Track.m_vLines.push_back(std::move(Line));
		}
		return Track;
	}

} // namespace

TEST(QmLyricsRender, SnapsManualOffsetToHalfSecondStep)
{
	EXPECT_EQ(SnapManualOffsetMsToStep(0), 0);
	EXPECT_EQ(SnapManualOffsetMsToStep(249), 0);
	EXPECT_EQ(SnapManualOffsetMsToStep(250), 500);
	EXPECT_EQ(SnapManualOffsetMsToStep(749), 500);
	EXPECT_EQ(SnapManualOffsetMsToStep(750), 1000);
	EXPECT_EQ(SnapManualOffsetMsToStep(-249), 0);
	EXPECT_EQ(SnapManualOffsetMsToStep(-250), -500);
	EXPECT_EQ(SnapManualOffsetMsToStep(-750), -1000);
}

TEST(QmLyricsRender, EffectivePlaybackOffsetCombinesManualAndTrackOffset)
{
	EXPECT_EQ(EffectivePlaybackOffsetMs(499, 200), 300);
	EXPECT_EQ(EffectivePlaybackOffsetMs(-501, 200), -700);
	EXPECT_EQ(EffectivePlaybackOffsetMs(0, 500), -500);
}

TEST(QmLyricsRender, FindsActiveLineBySortedTimeRange)
{
	const SLyricsTrack Track = BuildThreeLineTrack();
	EXPECT_EQ(FindActiveLineIndex(Track, -1), -1);
	EXPECT_EQ(FindActiveLineIndex(Track, 0), 0);
	EXPECT_EQ(FindActiveLineIndex(Track, 999), 0);
	EXPECT_EQ(FindActiveLineIndex(Track, 1000), 1);
	EXPECT_EQ(FindActiveLineIndex(Track, 2500), 2);
	EXPECT_EQ(FindActiveLineIndex(Track, 3000), -1);
}

TEST(QmLyricsRender, ResolvesDisplayLineAtTrackEdges)
{
	const SLyricsTrack Track = BuildThreeLineTrack();
	EXPECT_EQ(ResolveDisplayLineIndex(Track, -1, -50), 0);
	EXPECT_EQ(ResolveDisplayLineIndex(Track, -1, 3500), 2);
	EXPECT_EQ(ResolveDisplayLineIndex(Track, 1, 1500), 1);
}

TEST(QmLyricsRender, ResolveDisplayLineKeepsPreviousLineInTimedGaps)
{
	SLyricsTrack Track;
	SLyricsLine First;
	First.m_StartMs = 0;
	First.m_EndMs = 1000;
	Track.m_vLines.push_back(std::move(First));

	SLyricsLine Second;
	Second.m_StartMs = 2000;
	Second.m_EndMs = 3000;
	Track.m_vLines.push_back(std::move(Second));

	EXPECT_EQ(FindActiveLineIndex(Track, 1500), -1);
	EXPECT_EQ(ResolveDisplayLineIndex(Track, -1, 1500), 0);
	EXPECT_EQ(ResolveDisplayLineIndex(Track, -1, 2500), 1);
}

TEST(QmLyricsRender, WordAndLineProgressClamp)
{
	SLyricsWord Word;
	Word.m_StartMs = 1000;
	Word.m_EndMs = 2000;
	EXPECT_EQ(WordPlayProgress(Word, 500), 0.0f);
	EXPECT_EQ(WordPlayProgress(Word, 1500), 0.5f);
	EXPECT_EQ(WordPlayProgress(Word, 2500), 1.0f);

	SLyricsLine Line;
	Line.m_StartMs = 3000;
	Line.m_EndMs = 5000;
	EXPECT_EQ(LinePlayProgress(Line, 2000), 0.0f);
	EXPECT_EQ(LinePlayProgress(Line, 4000), 0.5f);
	EXPECT_EQ(LinePlayProgress(Line, 6000), 1.0f);
}

TEST(QmLyricsRender, WordTextLookupHandlesUtf8WithoutBytePrefixSlicing)
{
	const std::string RawText = "你好 世界";
	EXPECT_EQ(FindNextWordTextOffset(RawText, "你好", 0), 0);
	const size_t SecondWord = FindNextWordTextOffset(RawText, "世界", 1);
	EXPECT_NE(SecondWord, std::string::npos);
	EXPECT_TRUE(HasOnlyAsciiSpacingBetweenWords(RawText, std::string("你好").size(), SecondWord));
	EXPECT_FALSE(HasOnlyAsciiSpacingBetweenWords(RawText, 0, SecondWord));
}

TEST(QmLyricsRender, BuildLineVisualFadesAndScalesByDistance)
{
	const SLineVisual Active = BuildLineVisual(3, 3, 100.0f, 0.0f, 20.0f, 14.0f, 6.0f, 0.9f, 0.25f, 1.1f, 0.05f, 0.2f, 1, 10.0f);
	const SLineVisual Far = BuildLineVisual(0, 3, 100.0f, -72.0f, 20.0f, 14.0f, 6.0f, 0.9f, 0.25f, 1.1f, 0.05f, 0.2f, 0, 10.0f);

	EXPECT_FLOAT_EQ(Active.m_Alpha, 0.9f);
	EXPECT_FLOAT_EQ(Active.m_FontSize, 22.0f);
	EXPECT_LT(Far.m_Alpha, Active.m_Alpha);
	EXPECT_LT(Far.m_FontSize, Active.m_FontSize);
	EXPECT_LT(Far.m_PrimaryY, Active.m_PrimaryY);
}

TEST(QmLyricsRender, LineTransitionDistanceUsesActualBlockHeights)
{
	const float Distance = LineTransitionDistance(22.0f, 14.0f, 6.0f, 10.0f, 1);
	EXPECT_NEAR(Distance, 30.05f, 0.001f);
}

TEST(QmLyricsRender, LineTextWidthCacheAvoidsRepeatedTextMeasurements)
{
	SLyricsLine Line;
	Line.m_RawText = "hello world";
	Line.m_vWords.push_back({0, 500, "hello"});
	Line.m_vWords.push_back({500, 1000, "world"});

	struct SMeasureState
	{
		int m_Calls = 0;
	};
	SMeasureState State;
	auto Measure = [](void *pUser, float FontSize, const char *pText) {
		SMeasureState *pState = static_cast<SMeasureState *>(pUser);
		++pState->m_Calls;
		return FontSize + (float)std::strlen(pText);
	};

	SLineTextWidthCache Cache;
	UpdateLineTextWidthCache(&Cache, Line, 18.0f, 1, &State, Measure);
	EXPECT_EQ(State.m_Calls, 4);
	EXPECT_FLOAT_EQ(Cache.m_RawTextWidth, 29.0f);
	EXPECT_EQ(Cache.m_vWordWidths.size(), 2u);

	UpdateLineTextWidthCache(&Cache, Line, 18.0f, 1, &State, Measure);
	EXPECT_EQ(State.m_Calls, 4);

	UpdateLineTextWidthCache(&Cache, Line, 18.0f, 2, &State, Measure);
	EXPECT_EQ(State.m_Calls, 8);

	UpdateLineTextWidthCache(&Cache, Line, 20.0f, 2, &State, Measure);
	EXPECT_EQ(State.m_Calls, 12);
}

TEST(QmLyricsRender, TextWidthCacheAvoidsRepeatedTextMeasurements)
{
	struct SMeasureState
	{
		int m_Calls = 0;
	};
	SMeasureState State;
	auto Measure = [](void *pUser, float FontSize, const char *pText) {
		SMeasureState *pState = static_cast<SMeasureState *>(pUser);
		++pState->m_Calls;
		return FontSize + (float)std::strlen(pText);
	};

	STextWidthCache Cache;
	UpdateTextWidthCache(&Cache, "translated line", 12.0f, 1, &State, Measure);
	EXPECT_EQ(State.m_Calls, 1);
	EXPECT_FLOAT_EQ(Cache.m_TextWidth, 27.0f);

	UpdateTextWidthCache(&Cache, "translated line", 12.0f, 1, &State, Measure);
	EXPECT_EQ(State.m_Calls, 1);

	UpdateTextWidthCache(&Cache, "translated line", 12.0f, 2, &State, Measure);
	EXPECT_EQ(State.m_Calls, 2);

	UpdateTextWidthCache(&Cache, "other line", 12.0f, 2, &State, Measure);
	EXPECT_EQ(State.m_Calls, 3);
}
