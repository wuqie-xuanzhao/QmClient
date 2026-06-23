#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_lrclib.h>

#include <gtest/gtest.h>

#include <cstring>

using namespace QmLyrics;

namespace
{

	std::vector<SSourceCandidate> ParseGet(const char *pBody)
	{
		char aErr[128];
		return ParseLrclibGetResponse(pBody, std::strlen(pBody), aErr, sizeof(aErr));
	}

	std::vector<SSourceCandidate> ParseSearch(const char *pBody)
	{
		char aErr[128];
		return ParseLrclibSearchResponse(pBody, std::strlen(pBody), aErr, sizeof(aErr));
	}

} // namespace

TEST(QmLyricsSourceLrclibUrl, GetBasicQuery)
{
	SSourceQuery Q;
	Q.m_Title = "Bohemian Rhapsody";
	Q.m_Artist = "Queen";
	Q.m_DurationSec = 354;
	const std::string Url = BuildLrclibGetUrl(Q);
	EXPECT_NE(Url.find("https://lrclib.net/api/get"), std::string::npos);
	EXPECT_NE(Url.find("track_name=Bohemian%20Rhapsody"), std::string::npos);
	EXPECT_NE(Url.find("artist_name=Queen"), std::string::npos);
	EXPECT_NE(Url.find("duration=354"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, GetOmitsEmptyAndZeroDuration)
{
	SSourceQuery Q;
	Q.m_Title = "Hello";
	const std::string Url = BuildLrclibGetUrl(Q);
	EXPECT_NE(Url.find("track_name=Hello"), std::string::npos);
	EXPECT_EQ(Url.find("artist_name"), std::string::npos);
	EXPECT_EQ(Url.find("duration"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, GetEncodesSpecialChars)
{
	SSourceQuery Q;
	Q.m_Title = "AC/DC & friends";
	const std::string Url = BuildLrclibGetUrl(Q);
	// '/' '&' ' ' 都被编码
	EXPECT_NE(Url.find("AC%2FDC%20%26%20friends"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, SearchCombinesTitleAndArtist)
{
	SSourceQuery Q;
	Q.m_Title = "Hello";
	Q.m_Artist = "Adele";
	Q.m_Album = "25";
	Q.m_DurationSec = 295;
	const std::string Url = BuildLrclibSearchUrl(Q);
	EXPECT_NE(Url.find("/api/search"), std::string::npos);
	EXPECT_NE(Url.find("track_name=Hello"), std::string::npos);
	EXPECT_NE(Url.find("artist_name=Adele"), std::string::npos);
	EXPECT_NE(Url.find("album_name=25"), std::string::npos);
	EXPECT_NE(Url.find("durationMs=295000"), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetSyncedLyricsBecomesEnhanced)
{
	const char *pBody = R"({
		"id": 123,
		"trackName": "Hello",
		"artistName": "Adele",
		"albumName": "25",
		"duration": 295,
		"syncedLyrics": "[00:01.00]<00:01.00>Hello <00:01.50>world",
		"plainLyrics": "Hello world"
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_Metadata.m_Title, "Hello");
	EXPECT_EQ(vC[0].m_Metadata.m_Artist, "Adele");
	EXPECT_EQ(vC[0].m_Metadata.m_DurationSec, 295);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::LRC_ENHANCED);
	EXPECT_NE(vC[0].m_RawText.find("[00:01.00]"), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetSyncedWithoutInlineTagsIsStandard)
{
	const char *pBody = R"({
		"trackName": "Plain Song",
		"artistName": "X",
		"duration": 100,
		"syncedLyrics": "[00:01.00]Plain"
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::LRC_STANDARD);
}

TEST(QmLyricsSourceLrclibParse, GetFallsBackToPlainWhenSyncedEmpty)
{
	const char *pBody = R"({
		"trackName": "X",
		"plainLyrics": "Line 1\nLine 2",
		"syncedLyrics": ""
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::PLAIN);
	EXPECT_NE(vC[0].m_RawText.find("Line 1"), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetEmptyLyricsReturnsNoCandidates)
{
	const char *pBody = R"({"trackName":"X","syncedLyrics":"","plainLyrics":""})";
	const auto vC = ParseGet(pBody);
	EXPECT_EQ(vC.size(), 0u);
}

TEST(QmLyricsSourceLrclibParse, GetMalformedJsonReturnsEmpty)
{
	char aErr[128];
	const auto vC = ParseLrclibGetResponse("not json", 8, aErr, sizeof(aErr));
	EXPECT_EQ(vC.size(), 0u);
	EXPECT_GT(std::strlen(aErr), 0u);
}

TEST(QmLyricsSourceLrclibParse, SearchReturnsMultipleCandidates)
{
	const char *pBody = R"([
		{"trackName":"A","artistName":"X","duration":100,"syncedLyrics":"[00:01.00]A1"},
		{"trackName":"B","artistName":"Y","duration":200,"syncedLyrics":"[00:02.00]B1"},
		{"trackName":"C","syncedLyrics":""}
	])";
	const auto vC = ParseSearch(pBody);
	ASSERT_EQ(vC.size(), 2u);
	EXPECT_EQ(vC[0].m_Metadata.m_Title, "A");
	EXPECT_EQ(vC[1].m_Metadata.m_Title, "B");
}

TEST(QmLyricsSourceLrclibParse, SearchEmptyArray)
{
	const auto vC = ParseSearch("[]");
	EXPECT_EQ(vC.size(), 0u);
}

TEST(QmLyricsSourceLrclibParse, SearchObjectInsteadOfArrayReturnsEmpty)
{
	// 防御：服务端返回非数组也别崩
	const auto vC = ParseSearch(R"({"error":"not found"})");
	EXPECT_EQ(vC.size(), 0u);
}
