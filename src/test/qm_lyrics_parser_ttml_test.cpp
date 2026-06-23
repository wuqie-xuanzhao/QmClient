#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_parser_ttml.h>

#include <gtest/gtest.h>

#include <cstring>

namespace
{

	bool ParseTtml(const char *pText, QmLyrics::SLyricsTrack *pOut)
	{
		char aErr[128];
		return QmLyrics::ParseTtml(pText, std::strlen(pText), pOut, aErr, sizeof(aErr));
	}

} // namespace

TEST(QmLyricsParserTtmlTime, ParsesColonFormat)
{
	int64_t Ms = 0;
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("01:23.456", &Ms));
	EXPECT_EQ(Ms, 83456);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("0:01:23.456", &Ms));
	EXPECT_EQ(Ms, 83456);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("1:02:03.500", &Ms));
	EXPECT_EQ(Ms, 3723500);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("00:00.000", &Ms));
	EXPECT_EQ(Ms, 0);
}

TEST(QmLyricsParserTtmlTime, ParsesSecondsFormat)
{
	int64_t Ms = 0;
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("83.456s", &Ms));
	EXPECT_EQ(Ms, 83456);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("12s", &Ms));
	EXPECT_EQ(Ms, 12000);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("0.500s", &Ms));
	EXPECT_EQ(Ms, 500);
}

TEST(QmLyricsParserTtmlTime, ParsesMillisecondsFormat)
{
	int64_t Ms = 0;
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("83456ms", &Ms));
	EXPECT_EQ(Ms, 83456);
	EXPECT_TRUE(QmLyrics::ParseTtmlTime("0ms", &Ms));
	EXPECT_EQ(Ms, 0);
}

TEST(QmLyricsParserTtmlTime, RejectsInvalidFormats)
{
	int64_t Ms = 0;
	EXPECT_FALSE(QmLyrics::ParseTtmlTime("", &Ms));
	EXPECT_FALSE(QmLyrics::ParseTtmlTime("abc", &Ms));
	EXPECT_FALSE(QmLyrics::ParseTtmlTime("12", &Ms)); // 没单位、没冒号
	EXPECT_FALSE(QmLyrics::ParseTtmlTime("01:23:45:67.000", &Ms)); // 太多冒号
}

TEST(QmLyricsParserTtml, RejectsEmpty)
{
	QmLyrics::SLyricsTrack Track;
	EXPECT_FALSE(ParseTtml("", &Track));
	EXPECT_FALSE(ParseTtml("<tt><body></body></tt>", &Track));
}

TEST(QmLyricsParserTtml, ParsesBasicLineLevelTimestamps)
{
	const char *pInput = R"(<?xml version="1.0" encoding="UTF-8"?>
<tt xmlns="http://www.w3.org/ns/ttml">
  <body>
    <div>
      <p begin="00:01.000" end="00:05.000">First line</p>
      <p begin="00:05.000" end="00:10.000">Second line</p>
    </div>
  </body>
</tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 2u);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[0].m_EndMs, 5000);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "First line");
	EXPECT_EQ(Track.m_vLines[1].m_StartMs, 5000);
	EXPECT_EQ(Track.m_vLines[1].m_EndMs, 10000);
	EXPECT_EQ(Track.m_vLines[1].m_RawText, "Second line");
	EXPECT_TRUE(Track.m_vLines[0].m_vWords.empty());
}

TEST(QmLyricsParserTtml, ParsesWordLevelSpans)
{
	const char *pInput = R"(<tt><body><div>
<p begin="00:10.000" end="00:13.000"><span begin="00:10.000" end="00:11.000">Hello </span><span begin="00:11.000" end="00:12.000">cruel </span><span begin="00:12.000" end="00:13.000">world</span></p>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	const QmLyrics::SLyricsLine &Line = Track.m_vLines[0];
	EXPECT_EQ(Line.m_StartMs, 10000);
	EXPECT_EQ(Line.m_EndMs, 13000);
	ASSERT_EQ(Line.m_vWords.size(), 3u);
	EXPECT_EQ(Line.m_vWords[0].m_StartMs, 10000);
	EXPECT_EQ(Line.m_vWords[0].m_EndMs, 11000);
	EXPECT_EQ(Line.m_vWords[0].m_Text, "Hello ");
	EXPECT_EQ(Line.m_vWords[1].m_Text, "cruel ");
	EXPECT_EQ(Line.m_vWords[2].m_Text, "world");
	EXPECT_EQ(Line.m_RawText, "Hello cruel world");
}

TEST(QmLyricsParserTtml, CapturesTranslationRole)
{
	const char *pInput = R"(<tt><body><div>
<p begin="00:01.000" end="00:05.000">Original text<span ttm:role="x-translation">翻译文本</span></p>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	const QmLyrics::SLyricsLine &Line = Track.m_vLines[0];
	EXPECT_EQ(Line.m_RawText, "Original text");
	ASSERT_TRUE(Line.m_Translation.has_value());
	EXPECT_EQ(*Line.m_Translation, "翻译文本");
	EXPECT_FALSE(Line.m_Transliteration.has_value());
}

TEST(QmLyricsParserTtml, CapturesTransliterationRole)
{
	const char *pInput = R"(<tt><body><div>
<p begin="00:01.000" end="00:05.000">Hello<span ttm:role="x-transliteration">heh-loh</span></p>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	const QmLyrics::SLyricsLine &Line = Track.m_vLines[0];
	EXPECT_EQ(Line.m_RawText, "Hello");
	ASSERT_TRUE(Line.m_Transliteration.has_value());
	EXPECT_EQ(*Line.m_Transliteration, "heh-loh");
}

TEST(QmLyricsParserTtml, HandlesEntitiesInText)
{
	const char *pInput = R"(<tt><body><div>
<p begin="00:01.000" end="00:05.000">5 &lt; 6 &amp; 7 &gt; 4</p>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "5 < 6 & 7 > 4");
}

TEST(QmLyricsParserTtml, MixedTimeFormatsInSameDocument)
{
	const char *pInput = R"(<tt><body><div>
<p begin="00:01.000" end="00:05.000">A</p>
<p begin="5s" end="10s">B</p>
<p begin="10000ms" end="15000ms">C</p>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 3u);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[1].m_StartMs, 5000);
	EXPECT_EQ(Track.m_vLines[1].m_EndMs, 10000);
	EXPECT_EQ(Track.m_vLines[2].m_StartMs, 10000);
	EXPECT_EQ(Track.m_vLines[2].m_EndMs, 15000);
}

TEST(QmLyricsParserTtml, SkipsCommentsAndCdata)
{
	const char *pInput = R"(<tt><body><div>
<!-- comment about lyrics -->
<p begin="00:01.000" end="00:05.000">Real line</p>
<![CDATA[ ignored cdata ]]>
</div></body></tt>)";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseTtml(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "Real line");
}
