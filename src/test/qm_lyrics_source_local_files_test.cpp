// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/storage.h>

#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_match.h>
#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_local_files.h>

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using namespace QmLyrics;

namespace
{

	void WriteStorageText(IStorage *pStorage, const char *pPath, const char *pText)
	{
		IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		EXPECT_EQ(io_write(File, pText, str_length(pText)), (unsigned)str_length(pText));
		io_close(File);
	}

} // namespace

TEST(QmLyricsSourceLocalFiles, BuildsSiblingLyricPathFromLinkedMedia)
{
	SSourceQuery Query;
	Query.m_LinkedFileName = "C:/Music/Queen - Bohemian Rhapsody.mp3";
	const std::vector<std::string> vPaths = BuildLocalLyricsCandidatePaths(Query, ELocalLyricsFileKind::LRC);
	ASSERT_GE(vPaths.size(), 2u);
	EXPECT_EQ(vPaths[0], "C:/Music/Queen - Bohemian Rhapsody.lrc");
	EXPECT_EQ(vPaths[1], "Queen - Bohemian Rhapsody.lrc");
}

TEST(QmLyricsSourceLocalFiles, KeepsMatchingLyricPathAsFirstCandidate)
{
	SSourceQuery Query;
	Query.m_LinkedFileName = "C:/Music/Nocturne.ttml";
	const std::vector<std::string> vPaths = BuildLocalLyricsCandidatePaths(Query, ELocalLyricsFileKind::TTML);
	ASSERT_FALSE(vPaths.empty());
	EXPECT_EQ(vPaths[0], "C:/Music/Nocturne.ttml");
}

TEST(QmLyricsSourceLocalFiles, ReadsRelativeLrcFromStorageOnTick)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteStorageText(pStorage.get(), "Queen - Bohemian Rhapsody.lrc", "[00:00.00]Is this the real life?\n");

	SSourceQuery Query;
	Query.m_Title = "Bohemian Rhapsody";
	Query.m_Artist = "Queen";
	Query.m_LinkedFileName = "Queen - Bohemian Rhapsody.mp3";

	CLyricsSourceLocalLyricsFile Source(pStorage.get(), ELocalLyricsFileKind::LRC);
	std::vector<SSourceCandidate> vCandidates;
	bool DoneCalled = false;
	Source.QueryAsync(
		Query,
		[&](std::vector<SSourceCandidate> vResult) {
			DoneCalled = true;
			vCandidates = std::move(vResult);
		},
		[](const char *) {
			FAIL() << "local source should return empty result instead of error";
		});

	EXPECT_TRUE(Source.BusyForTests());
	EXPECT_FALSE(DoneCalled);
	Source.Tick();
	EXPECT_TRUE(DoneCalled);
	ASSERT_EQ(vCandidates.size(), 1u);
	EXPECT_EQ(vCandidates[0].m_SourceId, "local-lrc");
	EXPECT_EQ(vCandidates[0].m_FormatHint, EFormat::LRC_ENHANCED);
	EXPECT_NE(vCandidates[0].m_RawText.find("real life"), std::string::npos);
	EXPECT_NEAR(Score(Query, vCandidates[0].m_Metadata), 100.0f, 0.001f);
}

TEST(QmLyricsSourceLocalFiles, ReadsTtmlMetadataWhenAvailable)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteStorageText(pStorage.get(), "Nocturne.ttml",
		"<tt xmlns=\"http://www.w3.org/ns/ttml\"><head><metadata>"
		"<song xmlns=\"http://music.apple.com/lyric-ttml-internal\" title=\"Nocturne\" artist=\"Jay Chou\" album=\"November Chopin\"/>"
		"</metadata></head><body><div><p begin=\"00:00.000\" end=\"00:02.000\">hello</p></div></body></tt>");

	SSourceCandidate Candidate;
	ASSERT_TRUE(LoadLocalLyricsCandidate(pStorage.get(), "Nocturne.ttml", ELocalLyricsFileKind::TTML, &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "local-ttml");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::TTML);
	EXPECT_EQ(Candidate.m_Metadata.m_FileName, "Nocturne.ttml");
}

TEST(QmLyricsSourceLocalFiles, SplitsLocalMediaFolders)
{
	const std::vector<std::string> vFolders = SplitLocalMediaFolders("D:/Music| E:/Songs \nF:/Library");
	ASSERT_EQ(vFolders.size(), 3u);
	EXPECT_EQ(vFolders[0], "D:/Music");
	EXPECT_EQ(vFolders[1], "E:/Songs");
	EXPECT_EQ(vFolders[2], "F:/Library");
}

TEST(QmLyricsSourceLocalFiles, RecognizesBetterLyricsMusicExtensions)
{
	EXPECT_TRUE(IsLocalMusicFileExtension("song.mp3"));
	EXPECT_TRUE(IsLocalMusicFileExtension("song.FLAC"));
	EXPECT_TRUE(IsLocalMusicFileExtension("song.opus"));
	EXPECT_TRUE(IsLocalMusicFileExtension("song.dsf"));
	EXPECT_TRUE(IsLocalMusicFileExtension("song.s3m"));
	EXPECT_FALSE(IsLocalMusicFileExtension("song.lrc"));
	EXPECT_FALSE(IsLocalMusicFileExtension("song.txt"));
}

TEST(QmLyricsSourceLocalFiles, LocalMediaIndexRoundTripsJson)
{
	std::vector<SLocalMediaFileEntry> vEntries;
	SLocalMediaFileEntry Entry;
	Entry.m_Path = "D:/Music/Jay Chou - Nocturne.flac";
	Entry.m_FileName = "Jay Chou - Nocturne.flac";
	Entry.m_Title = "Nocturne";
	Entry.m_Artist = "Jay Chou";
	Entry.m_Album = "November Chopin";
	Entry.m_EmbeddedLyrics = "[00:00.00]hello\n[00:01.00]world";
	Entry.m_DurationSec = 260;
	Entry.m_FileSize = 12345678901LL;
	Entry.m_Modified = 1782000000;
	vEntries.push_back(Entry);

	const std::string Json = LocalMediaIndexToJson(vEntries);
	std::vector<SLocalMediaFileEntry> vParsed;
	ASSERT_TRUE(LocalMediaIndexFromJson(Json, &vParsed));
	ASSERT_EQ(vParsed.size(), 1u);
	EXPECT_EQ(vParsed[0].m_Path, Entry.m_Path);
	EXPECT_EQ(vParsed[0].m_Title, Entry.m_Title);
	EXPECT_EQ(vParsed[0].m_EmbeddedLyrics, Entry.m_EmbeddedLyrics);
	EXPECT_EQ(vParsed[0].m_FileSize, Entry.m_FileSize);
	EXPECT_EQ(vParsed[0].m_Modified, Entry.m_Modified);
}

TEST(QmLyricsSourceLocalFiles, LocalMusicFileSelectsBestEmbeddedLyricsEntry)
{
	std::vector<SLocalMediaFileEntry> vEntries;
	SLocalMediaFileEntry Wrong;
	Wrong.m_Path = "D:/Music/Other.mp3";
	Wrong.m_FileName = "Other.mp3";
	Wrong.m_Title = "Other";
	Wrong.m_Artist = "Someone";
	Wrong.m_EmbeddedLyrics = "[00:00.00]wrong";
	Wrong.m_DurationSec = 180;
	vEntries.push_back(Wrong);

	SLocalMediaFileEntry NoLyrics;
	NoLyrics.m_Path = "D:/Music/Nocturne instrumental.flac";
	NoLyrics.m_FileName = "Nocturne instrumental.flac";
	NoLyrics.m_Title = "Nocturne";
	NoLyrics.m_Artist = "Jay Chou";
	NoLyrics.m_Album = "November Chopin";
	NoLyrics.m_DurationSec = 260;
	vEntries.push_back(NoLyrics);

	SLocalMediaFileEntry Best;
	Best.m_Path = "D:/Music/Jay Chou - Nocturne.flac";
	Best.m_FileName = "Jay Chou - Nocturne.flac";
	Best.m_Title = "Nocturne";
	Best.m_Artist = "Jay Chou";
	Best.m_Album = "November Chopin";
	Best.m_EmbeddedLyrics = "[00:00.00]hello";
	Best.m_DurationSec = 260;
	vEntries.push_back(Best);

	SSourceQuery Query;
	Query.m_Title = "Nocturne";
	Query.m_Artist = "Jay Chou";
	Query.m_Album = "November Chopin";
	Query.m_DurationSec = 260;
	const std::vector<SSourceCandidate> vCandidates = BuildLocalMusicCandidatesFromIndex(Query, vEntries);
	ASSERT_EQ(vCandidates.size(), 1u);
	EXPECT_EQ(vCandidates[0].m_SourceId, "local-music-file");
	EXPECT_EQ(vCandidates[0].m_Metadata.m_FileName, "Jay Chou - Nocturne.flac");
	EXPECT_EQ(vCandidates[0].m_RawText, Best.m_EmbeddedLyrics);
	EXPECT_NEAR(Score(Query, vCandidates[0].m_Metadata), 100.0f, 0.001f);
}
