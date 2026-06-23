#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_amll_ttml_db.h>

#include <gtest/gtest.h>

#include <cstring>

using namespace QmLyrics;

TEST(QmLyricsSourceAmllTtmlDb, BuildsIndexAndLyricUrls)
{
	EXPECT_EQ(BuildAmllTtmlDbIndexUrl("https://example.test/base/"), "https://example.test/base/metadata/raw-lyrics-index.jsonl");
	EXPECT_EQ(BuildAmllTtmlDbLyricUrl("artist/song file.ttml", "https://example.test/base/"), "https://example.test/base/raw-lyrics/artist/song%20file.ttml");
}

TEST(QmLyricsSourceAmllTtmlDb, ParsesIndexLineMetadata)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	Q.m_DurationSec = 269;
	const char *pLine = R"({
		"rawLyricFile": "jay/sunny-day.ttml",
		"metadata": [
			["musicName", ["Sunny Day"]],
			["artists", ["Jay"]],
			["album", ["Album A"]],
			["duration", [269]]
		]
	})";
	SAmllTtmlDbIndexHit Hit;
	ASSERT_TRUE(ParseAmllTtmlDbIndexLine(pLine, std::strlen(pLine), Q, &Hit));
	EXPECT_EQ(Hit.m_RawLyricFile, "jay/sunny-day.ttml");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
	EXPECT_EQ(Hit.m_Metadata.m_Artist, "Jay");
	EXPECT_EQ(Hit.m_Metadata.m_Album, "Album A");
	EXPECT_EQ(Hit.m_Metadata.m_DurationSec, 269);
	EXPECT_GT(Hit.m_Score, 90.0f);
}

TEST(QmLyricsSourceAmllTtmlDb, FindsBestIndexMatch)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	const char *pIndex = R"({"rawLyricFile":"other.ttml","metadata":[["musicName",["Other"]],["artists",["Nobody"]],["album",["Other"]]]}
{"rawLyricFile":"jay/sunny-day.ttml","metadata":[["musicName",["Sunny Day"]],["artists",["Jay"]],["album",["Album A"]]]}
)";
	SAmllTtmlDbIndexHit Hit;
	ASSERT_TRUE(FindAmllTtmlDbBestMatch(pIndex, Q, &Hit));
	EXPECT_EQ(Hit.m_RawLyricFile, "jay/sunny-day.ttml");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
}

TEST(QmLyricsSourceAmllTtmlDb, LyricResponseBecomesTtmlCandidate)
{
	SAmllTtmlDbIndexHit Hit;
	Hit.m_RawLyricFile = "jay/sunny-day.ttml";
	Hit.m_Metadata.m_Title = "Sunny Day";
	Hit.m_Metadata.m_Artist = "Jay";
	Hit.m_Score = 98.0f;
	const char *pBody = R"(<tt xmlns="http://www.w3.org/ns/ttml"><body><div><p begin="00:01.000" end="00:03.000">Hello</p></div></body></tt>)";
	SSourceCandidate Candidate;
	ASSERT_TRUE(ParseAmllTtmlDbLyricResponse(pBody, std::strlen(pBody), Hit, &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "amll-ttml-db");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::TTML);
	EXPECT_EQ(Candidate.m_Metadata.m_Title, "Sunny Day");
	EXPECT_NE(Candidate.m_RawText.find("<tt"), std::string::npos);
}
