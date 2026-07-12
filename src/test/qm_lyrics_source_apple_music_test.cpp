// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_apple_music.h>

#include <gtest/gtest.h>

#include <cstring>

using namespace QmLyrics;

TEST(QmLyricsSourceAppleMusic, BrowseAndIndexUrlsMatchBetterLyrics)
{
	EXPECT_EQ(BuildAppleMusicBrowseUrl(), "https://music.apple.com/us/browse");
	const std::string IndexUrl = BuildAppleMusicIndexJsUrl(R"(<script src="/assets/index-abc123.js"></script>)");
	EXPECT_EQ(IndexUrl, "https://music.apple.com/assets/index-abc123.js");
}

TEST(QmLyricsSourceAppleMusic, AccessTokenStartsAtAppleJwtPrefix)
{
	const std::string Token = ParseAppleMusicAccessToken(R"(const token="eyJh.TEST.TOKEN";)");
	EXPECT_EQ(Token, "eyJh.TEST.TOKEN");
}

TEST(QmLyricsSourceAppleMusic, StorefrontResponseReadsIdAndLanguage)
{
	const char *pBody = R"({
		"data": [{
			"id": "us",
			"attributes": {"defaultLanguageTag": "en-US"}
		}]
	})";
	std::string Storefront;
	std::string Language;
	char aErr[128];
	ASSERT_TRUE(ParseAppleMusicStorefrontResponse(pBody, std::strlen(pBody), &Storefront, &Language, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(Storefront, "us");
	EXPECT_EQ(Language, "en-US");
}

TEST(QmLyricsSourceAppleMusic, SearchUrlUsesArtistTitleAndCatalog)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay Chou";
	const std::string Url = BuildAppleMusicSearchUrl("us", "en-US", Q);
	EXPECT_EQ(Url, "https://amp-api.music.apple.com/v1/catalog/us/search?term=Jay%20Chou%20Sunny%20Day&types=songs&limit=1&l=en-US");
}

TEST(QmLyricsSourceAppleMusic, SearchResponseReadsFirstSongAndScore)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay Chou";
	Q.m_Album = "Album A";
	Q.m_DurationSec = 269;
	const char *pBody = R"({
		"results": {
			"songs": {
				"data": [{
					"id": "12345",
					"attributes": {
						"name": "Sunny Day",
						"artistName": "Jay Chou",
						"albumName": "Album A",
						"durationInMillis": 269000
					}
				}]
			}
		}
	})";
	SAppleMusicSearchHit Hit;
	char aErr[128];
	ASSERT_TRUE(ParseAppleMusicSearchResponse(pBody, std::strlen(pBody), Q, &Hit, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(Hit.m_Id, "12345");
	EXPECT_EQ(Hit.m_Reference, "https://music.apple.com/song/12345");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
	EXPECT_EQ(Hit.m_Metadata.m_Artist, "Jay Chou");
	EXPECT_EQ(Hit.m_Metadata.m_Album, "Album A");
	EXPECT_EQ(Hit.m_Metadata.m_DurationSec, 269);
	EXPECT_GT(Hit.m_Score, 95.0f);
}

TEST(QmLyricsSourceAppleMusic, LyricsUrlIncludesSyllableLyrics)
{
	const std::string Url = BuildAppleMusicLyricsUrl("us", "en-US", "12345");
	EXPECT_EQ(Url, "https://amp-api.music.apple.com/v1/catalog/us/songs/12345?include[songs]=lyrics,syllable-lyrics&l=en-US");
}

TEST(QmLyricsSourceAppleMusic, LyricsResponsePrefersTimedSyllableTtml)
{
	SAppleMusicSearchHit Hit;
	Hit.m_Metadata.m_Title = "Sunny Day";
	Hit.m_Metadata.m_Artist = "Jay Chou";
	Hit.m_Score = 100.0f;
	const char *pBody = R"({
		"data": [{
			"relationships": {
				"syllable-lyrics": {
					"data": [{
						"attributes": {
							"ttml": "<tt><body><div><p begin=\"00:01.000\" end=\"00:02.000\"><span begin=\"00:01.000\" end=\"00:01.500\">Sun</span></p></div></body></tt>"
						}
					}]
				}
			}
		}]
	})";
	SSourceCandidate Candidate;
	char aErr[128];
	ASSERT_TRUE(ParseAppleMusicLyricsResponse(pBody, std::strlen(pBody), Hit, &Candidate, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(Candidate.m_SourceId, "apple-music");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::TTML);
	EXPECT_EQ(Candidate.m_Metadata.m_Title, "Sunny Day");
	EXPECT_NE(Candidate.m_RawText.find("begin="), std::string::npos);
	EXPECT_FLOAT_EQ(Candidate.m_SourceScore, 1.0f);
}

TEST(QmLyricsSourceAppleMusic, LyricsResponseRejectsUntimedTtml)
{
	SAppleMusicSearchHit Hit;
	Hit.m_Score = 100.0f;
	const char *pBody = R"({
		"data": [{
			"relationships": {
				"syllable-lyrics": {
					"data": [{
						"attributes": {"ttml": "<tt><body><p>plain</p></body></tt>"}
					}]
				}
			}
		}]
	})";
	SSourceCandidate Candidate;
	EXPECT_FALSE(ParseAppleMusicLyricsResponse(pBody, std::strlen(pBody), Hit, &Candidate));
}
