#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_lyricify_cn.h>

#include <gtest/gtest.h>

#include <cstring>

using namespace QmLyrics;

TEST(QmLyricsSourceLyricifyCn, QqSearchJsonContainsQuery)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	const std::string Json = BuildQqMusicSearchJson(Q);
	EXPECT_NE(Json.find("\"query\":\"Sunny Day Jay\""), std::string::npos);
	EXPECT_NE(Json.find("\"search_type\":0"), std::string::npos);
}

TEST(QmLyricsSourceLyricifyCn, QqQrcPostBodyMatchesLyricify)
{
	const std::string Body = BuildQqMusicQrcPostBody("12345");
	EXPECT_NE(Body.find("version=15"), std::string::npos);
	EXPECT_NE(Body.find("miniversion=82"), std::string::npos);
	EXPECT_NE(Body.find("lrctype=4"), std::string::npos);
	EXPECT_NE(Body.find("musicid=12345"), std::string::npos);
}

TEST(QmLyricsSourceLyricifyCn, QqSearchSelectsBestSong)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	Q.m_DurationSec = 269;
	const char *pBody = R"({
		"req_1": {"data": {"body": {"song": {"list": [
			{"id":1,"mid":"bad","title":"Other","interval":200,"album":{"title":"Other"},"singer":[{"name":"Nobody"}]},
			{"id":2,"mid":"good","title":"Sunny Day","interval":269,"album":{"title":"Album A"},"singer":[{"name":"Jay"}]}
		]}}}}
	})";
	SQqMusicSearchHit Hit;
	ASSERT_TRUE(ParseQqMusicSearchResponse(pBody, std::strlen(pBody), Q, &Hit));
	EXPECT_EQ(Hit.m_Id, "2");
	EXPECT_EQ(Hit.m_Mid, "good");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
	EXPECT_EQ(Hit.m_Metadata.m_Artist, "Jay");
	EXPECT_EQ(Hit.m_Metadata.m_DurationSec, 269);
}

TEST(QmLyricsSourceLyricifyCn, QqQrcDecryptsLyricifyPayload)
{
	const char *pEncrypted = "6368816BE2985DC6D89840186DB9743E5BA12463873C221E5243FF174DAF468EC53CCB8F6987F7D8";
	std::string Decrypted;
	char aErr[128];
	ASSERT_TRUE(DecryptQqMusicQrcHex(pEncrypted, &Decrypted, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(Decrypted, "He(1000,200)llo(1200,300)\n");
}

TEST(QmLyricsSourceLyricifyCn, QqQrcParsesSyllableLines)
{
	SLyricsTrack Track;
	const char *pText = "[0]He(1000,200)llo(1200,300)\n";
	ASSERT_TRUE(ParseQqMusicQrcText(pText, std::strlen(pText), &Track));
	ASSERT_EQ(Track.m_vLines.size(), 1u);
	EXPECT_EQ(Track.m_Format, EFormat::QRC);
	EXPECT_EQ(Track.m_vLines[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[0].m_EndMs, 1500);
	EXPECT_EQ(Track.m_vLines[0].m_RawText, "Hello");
	ASSERT_EQ(Track.m_vLines[0].m_vWords.size(), 2u);
	EXPECT_EQ(Track.m_vLines[0].m_vWords[0].m_Text, "He");
	EXPECT_EQ(Track.m_vLines[0].m_vWords[0].m_StartMs, 1000);
	EXPECT_EQ(Track.m_vLines[0].m_vWords[0].m_EndMs, 1200);
	EXPECT_EQ(Track.m_vLines[0].m_vWords[1].m_Text, "llo");
	EXPECT_EQ(Track.m_vLines[0].m_vWords[1].m_StartMs, 1200);
	EXPECT_EQ(Track.m_vLines[0].m_vWords[1].m_EndMs, 1500);
}

TEST(QmLyricsSourceLyricifyCn, QqQrcDownloadReadsOriginalTranslationAndRoma)
{
	const char *pEncrypted = "6368816BE2985DC6D89840186DB9743E5BA12463873C221E5243FF174DAF468EC53CCB8F6987F7D8";
	std::string Body = "<!--<QrcInfos><content>";
	Body.append(pEncrypted);
	Body.append("</content><contentts>[00:01.00]World\n</contentts><contentroma>Ro(1000,200)ma(1200,300)\n</contentroma></QrcInfos>-->");
	SSourceCandidate Candidate;
	ASSERT_TRUE(ParseQqMusicQrcDownloadResponse(Body.c_str(), Body.size(), &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "qq");
	EXPECT_EQ(Candidate.m_RawText, "He(1000,200)llo(1200,300)\n");
	EXPECT_EQ(Candidate.m_TranslationText, "[00:01.00]World\n");
	EXPECT_EQ(Candidate.m_TransliterationText, "Ro(1000,200)ma(1200,300)\n");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::QRC);
}

TEST(QmLyricsSourceLyricifyCn, QqLyricDecodesJsonpBase64)
{
	const char *pBody = R"(MusicJsonCallback_lrc({"code":0,"lyric":"WzAwOjAxLjAwXUhlbGxvCg==","trans":"WzAwOjAxLjAwXVdvcmxkCg==" }))";
	SSourceCandidate Candidate;
	ASSERT_TRUE(ParseQqMusicLyricResponse(pBody, std::strlen(pBody), &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "qq");
	EXPECT_EQ(Candidate.m_RawText, "[00:01.00]Hello\n");
	EXPECT_EQ(Candidate.m_TranslationText, "[00:01.00]World\n");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::LRC_STANDARD);
}

TEST(QmLyricsSourceLyricifyCn, QqDirectSongIdRequiresQqPlayerFamily)
{
	SSourceQuery Q;
	Q.m_PlayerId = "QQMusic.exe";
	Q.m_QqMusicSongId = "998877";
	EXPECT_TRUE(IsQqMusicFamilyPlayerId(Q.m_PlayerId));
	EXPECT_TRUE(ShouldUseQqMusicDirectSongId(Q));

	Q.m_PlayerId = "cloudmusic.exe";
	EXPECT_FALSE(ShouldUseQqMusicDirectSongId(Q));
}

TEST(QmLyricsSourceLyricifyCn, NeteaseUrlEncodesQueryAndSongId)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay Chou";
	const std::string SearchUrl = BuildNeteaseSearchUrl(Q);
	EXPECT_NE(SearchUrl.find("Sunny%20Day%20Jay%20Chou"), std::string::npos);
	EXPECT_NE(SearchUrl.find("type=1"), std::string::npos);
	EXPECT_EQ(BuildNeteaseLyricUrl("12345"), "https://music.163.com/weapi/song/lyric?csrf_token=");
}

TEST(QmLyricsSourceLyricifyCn, NeteaseLyricPostBodyMatchesLyricifyWeapi)
{
	const std::string Body = BuildNeteaseLyricPostBody("12345");
	EXPECT_EQ(Body,
		"params=h%2FDRvNtIGkkSszFqW%2BooeT7E9JLjlA37QFU3g3WMKRYdgrPEmCORsjFlbdNN%2FKCmHimjS4teLEiCw9U9rjl0F%2BVod6k6dHpul2mGqZNR%2Fxg7%2B798EREHwFDL3rdtxaFYUbEAk5b6YPjleLWvup3i%2Bi5tjX%2FosnWAi1NuemxVD9GoMb4AckpdeQsjijF7H4GU"
		"&encSecKey=c76aa624ba1d2a7676339d94fa890b7510d33bf21d270f2e21d81bcb5a8a299fe8cf7303c98128fc"
		"9de8a87742f186db1b02be275feea7dddd4a71e5ac0965ad3ffd776e8b6537adcf67db8a2f8566"
		"346519806ecd0aadea28247b6d891af3791ac466ee7ba88e6520006ded154cde1787e644269"
		"6a819d1924af6d0fa402e30");
}

TEST(QmLyricsSourceLyricifyCn, NeteaseDirectSongIdMatchesBetterLyricsFamilies)
{
	EXPECT_TRUE(IsNeteaseFamilyPlayerId("cloudmusic.exe"));
	EXPECT_TRUE(IsNeteaseFamilyPlayerId("17588BrandonWong.LyricEase_abc"));
	EXPECT_TRUE(IsNeteaseFamilyPlayerId("48848aaaaaaccd.HyPlayer_xyz"));
	EXPECT_FALSE(IsNeteaseFamilyPlayerId("QQMusic.exe"));

	SSourceQuery Q;
	Q.m_PlayerId = "cloudmusic.exe";
	Q.m_NeteaseSongId = "186016";
	EXPECT_TRUE(ShouldUseNeteaseDirectSongId(Q));

	Q.m_PlayerId = "OtherPlayer.exe";
	EXPECT_FALSE(ShouldUseNeteaseDirectSongId(Q));
}

TEST(QmLyricsSourceLyricifyCn, NeteaseSearchSelectsBestSong)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	Q.m_DurationSec = 269;
	const char *pBody = R"({
		"result": {"songs": [
			{"id": 1, "name":"Other", "duration":200000, "album":{"name":"Other"}, "artists":[{"name":"Nobody"}]},
			{"id": 2, "name":"Sunny Day", "duration":269000, "album":{"name":"Album A"}, "artists":[{"name":"Jay"}]}
		]},
		"code": 200
	})";
	SNeteaseSearchHit Hit;
	ASSERT_TRUE(ParseNeteaseSearchResponse(pBody, std::strlen(pBody), Q, &Hit));
	EXPECT_EQ(Hit.m_Id, "2");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
	EXPECT_EQ(Hit.m_Metadata.m_Artist, "Jay");
	EXPECT_EQ(Hit.m_Metadata.m_DurationSec, 269);
}

TEST(QmLyricsSourceLyricifyCn, NeteaseLyricReadsOriginalTranslationAndRoma)
{
	const char *pBody = R"({
		"lrc": {"lyric":"[00:01.00]Hello\n"},
		"tlyric": {"lyric":"[00:01.00]World\n"},
		"romalrc": {"lyric":"[00:01.00]Roma\n"},
		"code": 200
	})";
	SSourceCandidate Candidate;
	ASSERT_TRUE(ParseNeteaseLyricResponse(pBody, std::strlen(pBody), &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "netease");
	EXPECT_EQ(Candidate.m_RawText, "[00:01.00]Hello\n");
	EXPECT_EQ(Candidate.m_TranslationText, "[00:01.00]World\n");
	EXPECT_EQ(Candidate.m_TransliterationText, "[00:01.00]Roma\n");
}
