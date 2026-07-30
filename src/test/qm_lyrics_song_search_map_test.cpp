// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_song_search_map.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace QmLyrics;

TEST(QmLyricsSongSearchMap, CanonicalizesBetterLyricsProviderNames)
{
	EXPECT_EQ(CanonicalLyricsProviderId("QQ"), "qq");
	EXPECT_EQ(CanonicalLyricsProviderId("Netease"), "netease");
	EXPECT_EQ(CanonicalLyricsProviderId("LrcLib"), "lrclib");
	EXPECT_EQ(CanonicalLyricsProviderId("AmllTtmlDb"), "amll-ttml-db");
	EXPECT_EQ(CanonicalLyricsProviderId("LocalMusicFile"), "local-music-file");
	EXPECT_EQ(CanonicalLyricsProviderId("LocalLrcFile"), "local-lrc");
	EXPECT_EQ(CanonicalLyricsProviderId("LocalEslrcFile"), "local-eslrc");
	EXPECT_EQ(CanonicalLyricsProviderId("LocalTtmlFile"), "local-ttml");
	EXPECT_EQ(CanonicalLyricsProviderId("AppleMusic"), "apple-music");
}

TEST(QmLyricsSongSearchMap, CanonicalizesQmClientSourceIds)
{
	EXPECT_EQ(CanonicalLyricsProviderId("amll-ttml-db"), "amll-ttml-db");
	EXPECT_EQ(CanonicalLyricsProviderId("local-music-file"), "local-music-file");
	EXPECT_EQ(CanonicalLyricsProviderId("local-lrc"), "local-lrc");
	EXPECT_EQ(CanonicalLyricsProviderId("local-eslrc"), "local-eslrc");
	EXPECT_EQ(CanonicalLyricsProviderId("local-ttml"), "local-ttml");
	EXPECT_EQ(CanonicalLyricsProviderId("apple-music"), "apple-music");
	EXPECT_EQ(CanonicalLyricsProviderId("unknown"), "");
}

TEST(QmLyricsSongSearchMap, RoundTripsBetterLyricsFieldNames)
{
	std::vector<SSongSearchMapEntry> vEntries;
	SSongSearchMapEntry Entry;
	Entry.m_OriginalTitle = "夜曲";
	Entry.m_OriginalArtist = "周杰伦";
	Entry.m_OriginalAlbum = "11月的萧邦";
	Entry.m_MappedTitle = "Nocturne";
	Entry.m_MappedArtist = "Jay Chou";
	Entry.m_MappedAlbum = "November Chopin";
	Entry.m_IsMarkedAsPureMusic = true;
	Entry.m_LyricsSearchProvider = "Netease";
	vEntries.push_back(Entry);

	const std::string Json = SongSearchMapToJson(vEntries);
	std::vector<SSongSearchMapEntry> vParsed;
	ASSERT_TRUE(SongSearchMapFromJson(Json, &vParsed));
	ASSERT_EQ(vParsed.size(), 1u);
	EXPECT_EQ(vParsed[0].m_OriginalTitle, Entry.m_OriginalTitle);
	EXPECT_EQ(vParsed[0].m_MappedArtist, Entry.m_MappedArtist);
	EXPECT_TRUE(vParsed[0].m_IsMarkedAsPureMusic);
	EXPECT_EQ(vParsed[0].m_LyricsSearchProvider, "Netease");
}

TEST(QmLyricsSongSearchMap, AcceptsLowerCamelJsonFields)
{
	const char *pJson =
		"{\"entries\":[{"
		"\"originalTitle\":\"Song\","
		"\"originalArtist\":\"Artist\","
		"\"originalAlbum\":\"Album\","
		"\"mappedTitle\":\"Mapped Song\","
		"\"mappedArtist\":\"Mapped Artist\","
		"\"mappedAlbum\":\"Mapped Album\","
		"\"isMarkedAsPureMusic\":\"true\","
		"\"lyricsSearchProvider\":\"LocalMusicFile\""
		"}]}";
	std::vector<SSongSearchMapEntry> vParsed;
	ASSERT_TRUE(SongSearchMapFromJson(pJson, &vParsed));
	ASSERT_EQ(vParsed.size(), 1u);
	EXPECT_EQ(vParsed[0].m_OriginalTitle, "Song");
	EXPECT_EQ(vParsed[0].m_MappedTitle, "Mapped Song");
	EXPECT_TRUE(vParsed[0].m_IsMarkedAsPureMusic);
	EXPECT_EQ(CanonicalLyricsProviderId(vParsed[0].m_LyricsSearchProvider), "local-music-file");
}

TEST(QmLyricsSongSearchMap, FindsExactOriginalSongAndAppliesMappedQuery)
{
	std::vector<SSongSearchMapEntry> vEntries;
	SSongSearchMapEntry Entry;
	Entry.m_OriginalTitle = "Original";
	Entry.m_OriginalArtist = "Artist";
	Entry.m_OriginalAlbum = "Album";
	Entry.m_MappedTitle = "Mapped";
	Entry.m_MappedArtist = "Mapped Artist";
	Entry.m_MappedAlbum = "Mapped Album";
	vEntries.push_back(Entry);

	SMatchQuery Query;
	Query.m_Title = "Original";
	Query.m_Artist = "Artist";
	Query.m_Album = "Album";
	Query.m_PlayerId = "cloudmusic.exe";
	Query.m_NeteaseSongId = "123";
	Query.m_DurationSec = 180;

	const SSongSearchMapEntry *pFound = FindSongSearchMapping(Query, vEntries);
	ASSERT_NE(pFound, nullptr);
	const SMatchQuery Mapped = ApplySongSearchMapping(Query, *pFound);
	EXPECT_EQ(Mapped.m_Title, "Mapped");
	EXPECT_EQ(Mapped.m_Artist, "Mapped Artist");
	EXPECT_EQ(Mapped.m_Album, "Mapped Album");
	EXPECT_EQ(Mapped.m_PlayerId, Query.m_PlayerId);
	EXPECT_EQ(Mapped.m_NeteaseSongId, Query.m_NeteaseSongId);
	EXPECT_EQ(Mapped.m_DurationSec, Query.m_DurationSec);
}

TEST(QmLyricsSongSearchMap, BuildsBetterLyricsPureMusicCandidate)
{
	SMatchQuery Query;
	Query.m_Title = "Instrumental";
	Query.m_Artist = "Artist";
	Query.m_Album = "Album";
	Query.m_DurationSec = 99;

	const SSourceCandidate Candidate = BuildPureMusicCandidate(Query);
	EXPECT_EQ(Candidate.m_RawText, "[00:00.000]🎶🎶🎶\n[99:00.000]");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::LRC_STANDARD);
	EXPECT_EQ(Candidate.m_Metadata.m_Title, Query.m_Title);
	EXPECT_EQ(Candidate.m_Metadata.m_Artist, Query.m_Artist);
	EXPECT_EQ(Candidate.m_Metadata.m_DurationSec, Query.m_DurationSec);
	EXPECT_EQ(Candidate.m_SourceId, "song-search-map");
}
