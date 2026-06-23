#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_parser_lrc.h>

#include <gtest/gtest.h>

#include <cstring>

namespace
{

	bool ParseLrc(const char *pText, QmLyrics::SLyricsTrack *pOut)
	{
		char aErr[128];
		return QmLyrics::ParseLrc(pText, std::strlen(pText), pOut, aErr, sizeof(aErr));
	}

} // namespace

TEST(QmLyricsParserLrc, RejectsEmpty)
{
	QmLyrics::SLyricsTrack Track;
	EXPECT_FALSE(ParseLrc("", &Track));
	EXPECT_FALSE(ParseLrc("   \n\n   ", &Track));
}

TEST(QmLyricsParserLrc, ParsesStandardThreeLines)
{
	const char *pInput =
		"[00:01.00]First\n"
		"[00:05.50]Second\n"
		"[00:10.250]Third\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 3u);
	EXPECT_EQ(Track.m_Format, QmLyrics::EFormat::LRC_STANDARD);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "First");
	EXPECT_EQ(Track.m_vLines[0].m_EndMs, 5500);
	EXPECT_EQ(Track.m_vLines[1].m_StartMs, 5500);
	EXPECT_EQ(Track.m_vLines[1].m_RawText, "Second");
	EXPECT_EQ(Track.m_vLines[1].m_EndMs, 10250);
	EXPECT_EQ(Track.m_vLines[2].m_StartMs, 10250);
	EXPECT_EQ(Track.m_vLines[2].m_RawText, "Third");
	// 最后一行 EndMs 用 +5000ms 兜底
	EXPECT_EQ(Track.m_vLines[2].m_EndMs, 15250);
	// 标准 LRC 行级时间轴：words 为空
	EXPECT_TRUE(Track.m_vLines[0].m_vWords.empty());
}

TEST(QmLyricsParserLrc, ExtractsMetadataTags)
{
	const char *pInput =
		"[ti: Test Song ]\n"
		"[ar:Artist Name]\n"
		"[al:Album Title]\n"
		"[offset:+200]\n"
		"[00:01.00]First line\n"
		"[00:05.00]Second line\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_TRUE(Track.m_Title.has_value());
	EXPECT_EQ(*Track.m_Title, "Test Song");
	ASSERT_TRUE(Track.m_Artist.has_value());
	EXPECT_EQ(*Track.m_Artist, "Artist Name");
	ASSERT_TRUE(Track.m_Album.has_value());
	EXPECT_EQ(*Track.m_Album, "Album Title");
	EXPECT_EQ(Track.m_OffsetMs, 200);
	EXPECT_EQ(Track.m_vLines.size(), 2u);
}

TEST(QmLyricsParserLrc, ExpandsMultipleTimestampsPerLine)
{
	const char *pInput =
		"[00:01.000][00:30.500]Refrain\n"
		"[00:10.000]Verse\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 3u);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "Refrain");
	EXPECT_EQ(Track.m_vLines[1].m_StartMs, 10000);
	EXPECT_EQ(Track.m_vLines[1].m_RawText, "Verse");
	EXPECT_EQ(Track.m_vLines[2].m_StartMs, 30500);
	EXPECT_EQ(Track.m_vLines[2].m_RawText, "Refrain");
}

TEST(QmLyricsParserLrc, SkipsBlankAndCommentLines)
{
	const char *pInput =
		"\n"
		"   \n"
		"# this is a comment\n"
		"[00:01.000]Hello\n"
		"\n"
		"[00:05.000]World\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 2u);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "Hello");
	EXPECT_EQ(Track.m_vLines[1].m_RawText, "World");
}

TEST(QmLyricsParserLrc, ParsesEnhancedWithWordTimestamps)
{
	const char *pInput =
		"[00:10.000]<00:10.000>Hello <00:10.500>cruel <00:11.000>world\n"
		"[00:15.000]<00:15.000>Goodbye\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	EXPECT_EQ(Track.m_Format, QmLyrics::EFormat::LRC_ENHANCED);
	ASSERT_EQ(Track.m_vLines.size(), 2u);
	const QmLyrics::SLyricsLine &First = Track.m_vLines[0];
	EXPECT_EQ(First.m_StartMs, 10000);
	EXPECT_EQ(First.m_EndMs, 15000);
	ASSERT_EQ(First.m_vWords.size(), 3u);
	EXPECT_EQ(First.m_vWords[0].m_StartMs, 10000);
	EXPECT_EQ(First.m_vWords[0].m_EndMs, 10500);
	EXPECT_EQ(First.m_vWords[0].m_Text, "Hello ");
	EXPECT_EQ(First.m_vWords[1].m_StartMs, 10500);
	EXPECT_EQ(First.m_vWords[1].m_EndMs, 11000);
	EXPECT_EQ(First.m_vWords[1].m_Text, "cruel ");
	EXPECT_EQ(First.m_vWords[2].m_StartMs, 11000);
	// 最后一词 EndMs 兜底为行 EndMs（15000）
	EXPECT_EQ(First.m_vWords[2].m_EndMs, 15000);
	EXPECT_EQ(First.m_vWords[2].m_Text, "world");
	// 整行 raw text = 词文本拼接
	EXPECT_EQ(First.m_RawText, "Hello cruel world");
}

TEST(QmLyricsParserLrc, MixedStandardAndEnhancedLinesPromoteToEnhanced)
{
	const char *pInput =
		"[00:01.000]Plain line\n"
		"[00:05.000]<00:05.000>Timed <00:05.500>line\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	// 任何一行含词时间戳，整条 track 标 ENHANCED
	EXPECT_EQ(Track.m_Format, QmLyrics::EFormat::LRC_ENHANCED);
	EXPECT_EQ(Track.m_vLines.size(), 2u);
	EXPECT_TRUE(Track.m_vLines[0].m_vWords.empty());
	EXPECT_EQ(Track.m_vLines[1].m_vWords.size(), 2u);
}

TEST(QmLyricsParserLrc, AcceptsTwoDigitFractionTimestamp)
{
	const char *pInput = "[01:23.45]Two-digit frac\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 83450);
}

TEST(QmLyricsParserLrc, AcceptsHoursTimestamp)
{
	const char *pInput = "[1:02:03.500]Hour-format\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 3723500);
}

TEST(QmLyricsParserLrc, IgnoresInvalidTimestampOnlyLines)
{
	const char *pInput =
		"[bad]Not a time\n"
		"[00:01.000]Valid\n";
	QmLyrics::SLyricsTrack Track;
	ASSERT_TRUE(ParseLrc(pInput, &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "Valid");
}
