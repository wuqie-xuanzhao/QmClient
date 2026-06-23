#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_match.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace QmLyrics;

TEST(QmLyricsNormalize, EmptyInput)
{
	EXPECT_EQ(NormalizeForMatch(""), "");
	EXPECT_EQ(NormalizeForMatch("   "), "");
}

TEST(QmLyricsNormalize, LowercaseAscii)
{
	EXPECT_EQ(NormalizeForMatch("Hello World"), "hello world");
	EXPECT_EQ(NormalizeForMatch("ABC"), "abc");
}

TEST(QmLyricsNormalize, StripsPunctuation)
{
	EXPECT_EQ(NormalizeForMatch("Hello, World!"), "hello world");
	EXPECT_EQ(NormalizeForMatch("A.B.C."), "abc");
}

TEST(QmLyricsNormalize, StripsFeatModifier)
{
	EXPECT_EQ(NormalizeForMatch("Song Title (feat. Someone)"), "song title");
	EXPECT_EQ(NormalizeForMatch("Track (Feat. Guest Artist)"), "track");
}

TEST(QmLyricsNormalize, StripsLiveAndRemaster)
{
	EXPECT_EQ(NormalizeForMatch("Song [Live]"), "song");
	EXPECT_EQ(NormalizeForMatch("Bohemian Rhapsody - Remastered 2011"), "bohemian rhapsody");
	EXPECT_EQ(NormalizeForMatch("Imagine (Acoustic Version)"), "imagine");
}

TEST(QmLyricsNormalize, StripsAsciiAccents)
{
	EXPECT_EQ(NormalizeForMatch("Café Müller"), "cafe muller");
	EXPECT_EQ(NormalizeForMatch("Beyoncé"), "beyonce");
}

TEST(QmLyricsNormalize, PreservesCjk)
{
	const std::string Out = NormalizeForMatch("夜曲 Nocturne");
	EXPECT_NE(Out.find("夜曲"), std::string::npos);
	EXPECT_NE(Out.find("nocturne"), std::string::npos);
}

TEST(QmLyricsLevenshtein, ZeroForIdenticalAndEmpty)
{
	EXPECT_EQ(LevenshteinDistance("hello", "hello"), 0);
	EXPECT_EQ(LevenshteinDistance("", ""), 0);
}

TEST(QmLyricsLevenshtein, SimpleEdits)
{
	EXPECT_EQ(LevenshteinDistance("kitten", "sitting"), 3);
	EXPECT_EQ(LevenshteinDistance("abc", ""), 3);
	EXPECT_EQ(LevenshteinDistance("", "abc"), 3);
}

TEST(QmLyricsTokenSet, OrderInsensitive)
{
	EXPECT_NEAR(TokenSetRatio("Pink Floyd", "Floyd Pink"), 1.0f, 0.001f);
	EXPECT_NEAR(TokenSetRatio("a b c", "c b a"), 1.0f, 0.001f);
}

TEST(QmLyricsTokenSet, PartialOverlap)
{
	// {one, two, three} vs {two, three, four} → 交集 2，|A|=|B|=3 → 4/6
	EXPECT_NEAR(TokenSetRatio("one two three", "two three four"), 4.0f / 6.0f, 0.001f);
}

TEST(QmLyricsDuration, ExactMatch)
{
	EXPECT_NEAR(DurationScore(180, 180, 5000), 1.0f, 0.001f);
}

TEST(QmLyricsDuration, WithinTolerance)
{
	// BetterLyrics: <=1s 满分，>=5s 为 0；2s 差 → 1 - (2 - 1) / (5 - 1)
	EXPECT_NEAR(DurationScore(180, 182, 5000), 0.75f, 0.001f);
}

TEST(QmLyricsDuration, BeyondTolerance)
{
	EXPECT_NEAR(DurationScore(180, 200, 5000), 0.0f, 0.001f);
}

TEST(QmLyricsDuration, UnknownCandidateReturnsZero)
{
	EXPECT_NEAR(DurationScore(0, 180, 5000), 0.0f, 0.001f);
	EXPECT_NEAR(DurationScore(180, 0, 5000), 0.0f, 0.001f);
}

TEST(QmLyricsScore, PerfectMatch)
{
	SMatchQuery Q;
	Q.m_Title = "Bohemian Rhapsody";
	Q.m_Artist = "Queen";
	Q.m_DurationSec = 354;
	SMatchCandidate C;
	C.m_Title = "Bohemian Rhapsody";
	C.m_Artist = "Queen";
	C.m_DurationSec = 354;
	EXPECT_NEAR(Score(Q, C), 100.0f, 0.5f);
}

TEST(QmLyricsScore, FeatModifierDoesNotBreakMatch)
{
	SMatchQuery Q;
	Q.m_Title = "Imagine (feat. Yoko)";
	Q.m_Artist = "John Lennon";
	Q.m_DurationSec = 183;
	SMatchCandidate C;
	C.m_Title = "Imagine";
	C.m_Artist = "John Lennon";
	C.m_DurationSec = 183;
	EXPECT_GE(Score(Q, C), 95.0f);
}

TEST(QmLyricsScore, DifferentDurationLowersScore)
{
	SMatchQuery Q;
	Q.m_Title = "Bohemian Rhapsody";
	Q.m_Artist = "Queen";
	Q.m_DurationSec = 354;
	SMatchCandidate C;
	C.m_Title = "Bohemian Rhapsody";
	C.m_Artist = "Queen";
	C.m_DurationSec = 400; // 远超 5s tol
	const float S = Score(Q, C);
	EXPECT_NEAR(S, 70.0f, 0.001f); // title+artist+empty album 满分，duration 为 0
}

TEST(QmLyricsScore, WrongTitleLowersScore)
{
	SMatchQuery Q;
	Q.m_Title = "Bohemian Rhapsody";
	Q.m_Artist = "Queen";
	Q.m_DurationSec = 354;
	SMatchCandidate C;
	C.m_Title = "Killer Queen";
	C.m_Artist = "Queen";
	C.m_DurationSec = 354;
	EXPECT_NEAR(Score(Q, C), 83.0f, 0.001f);
}

TEST(QmLyricsScore, AlbumContributesTenPercent)
{
	SMatchQuery Q;
	Q.m_Title = "Nocturne";
	Q.m_Artist = "Jay Chou";
	Q.m_Album = "November Chopin";
	Q.m_DurationSec = 230;
	SMatchCandidate C;
	C.m_Title = "Nocturne";
	C.m_Artist = "Jay Chou";
	C.m_Album = "Initial J";
	C.m_DurationSec = 230;
	EXPECT_NEAR(Score(Q, C), 95.0f, 0.001f);
}

TEST(QmLyricsScore, FallsBackToLinkedFileNameFingerprint)
{
	SMatchQuery Q;
	Q.m_LinkedFileName = "C:/Music/Queen - Bohemian Rhapsody.flac";
	SMatchCandidate C;
	C.m_Title = "Bohemian Rhapsody";
	C.m_Artist = "Queen";
	EXPECT_NEAR(Score(Q, C), 100.0f, 0.001f);
}
