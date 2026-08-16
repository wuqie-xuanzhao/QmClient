// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/storage.h>

#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_cache.h>

#include <gtest/gtest.h>

using namespace QmLyrics;

namespace
{

	SCacheEntry MakeEntry(const char *pKey, int64_t StoredAt, int64_t LastUsedAt = 0, const char *pSource = "lrclib")
	{
		SCacheEntry E;
		E.m_Key = pKey;
		E.m_FileName = FileNameForKey(pKey);
		E.m_Source = pSource;
		E.m_Score = 95.0f;
		E.m_StoredAt = StoredAt;
		E.m_LastUsedAt = LastUsedAt > 0 ? LastUsedAt : StoredAt;
		return E;
	}

	SCachePayload MakePayload(const char *pRawText, const char *pTitle = "Song", const char *pArtist = "Singer")
	{
		SCachePayload Payload;
		Payload.m_RawText = pRawText;
		Payload.m_Metadata.m_Title = pTitle;
		Payload.m_Metadata.m_Artist = pArtist;
		Payload.m_Source = "lrclib";
		return Payload;
	}

	void BlockIndexTempFile(IStorage *pStorage)
	{
		ASSERT_TRUE(pStorage->CreateFolder("qmclient/lyrics/index.json.tmp", IStorage::TYPE_SAVE));
	}

} // namespace

TEST(QmLyricsCache, BuildCacheKeyAndFileName)
{
	const std::string Key = BuildCacheKey("Bohemian Rhapsody", "Queen", "A Night at the Opera", 354);
	EXPECT_NE(Key.find("bohemian rhapsody"), std::string::npos);
	EXPECT_NE(Key.find("queen"), std::string::npos);
	EXPECT_NE(Key.find("354"), std::string::npos);

	const std::string File = FileNameForKey(Key);
	EXPECT_EQ(File.size(), 21u); // 16 hex + ".json"
	EXPECT_NE(File.find(".json"), std::string::npos);

	// 同样输入归一化后产生同样的 key
	const std::string Key2 = BuildCacheKey("BOHEMIAN RHAPSODY", "queen", "A Night at the Opera", 354);
	EXPECT_EQ(Key, Key2);
}

TEST(QmLyricsCache, BuildsReadableSafePayloadFileName)
{
	EXPECT_EQ(FileNameForTrack("Song Name", "Singer", "track-key"), "Song Name+Singer.lrc");
	EXPECT_EQ(FileNameForTrack("CON", "Singer", "track-key"), "_CON+Singer.lrc");

	const std::string Unsafe = FileNameForTrack("A/B:*?", "Artist\\Name", "track-key");
	EXPECT_TRUE(IsValidCachePayloadFileName(Unsafe));
	EXPECT_EQ(Unsafe.find('/'), std::string::npos);
	EXPECT_EQ(Unsafe.find('\\'), std::string::npos);

	const std::string Collision = FileNameForTrack("Song Name", "Singer", "track-key", true);
	EXPECT_NE(Collision, "Song Name+Singer.lrc");
	EXPECT_EQ(Collision.find("Song Name+Singer-"), 0u);
	EXPECT_TRUE(IsValidCachePayloadFileName(Collision));
}

TEST(QmLyricsCache, ChoosesCollisionSafeNameCaseInsensitively)
{
	CCacheIndex Index;
	SCacheEntry Existing = MakeEntry("first", 1000);
	Existing.m_FileName = "Song+Singer.lrc";
	Index.Upsert(Existing, 100);

	const std::string Chosen = ChooseCachePayloadFileName(Index, "song", "singer", "second");
	EXPECT_NE(Chosen, "song+singer.lrc");
	EXPECT_TRUE(IsValidCachePayloadFileName(Chosen));
}

TEST(QmLyricsCache, ValidatesPayloadFileName)
{
	EXPECT_TRUE(IsValidCachePayloadFileName("0123456789abcdef.json"));
	EXPECT_TRUE(IsValidCachePayloadFileName("0123456789ABCDEF.json"));
	EXPECT_TRUE(IsValidCachePayloadFileName("Song+Singer.lrc"));
	EXPECT_FALSE(IsValidCachePayloadFileName("../outside.json"));
	EXPECT_FALSE(IsValidCachePayloadFileName("../outside.lrc"));
	EXPECT_FALSE(IsValidCachePayloadFileName("folder/outside.lrc"));
	EXPECT_FALSE(IsValidCachePayloadFileName("CON.lrc"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdeg.json"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdef.txt"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdef0.json"));
}

TEST(QmLyricsCache, ReadablePayloadPairRoundTripKeepsRawLyricsInLrcFile)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	SCachePayload Payload;
	Payload.m_RawText = "[00:01.00]Hello\n[00:02.00]world\n";
	Payload.m_TranslationText = "[00:01.00]你好\n";
	Payload.m_TransliterationText = "[00:01.00]ni hao\n";
	Payload.m_Format = EFormat::LRC_STANDARD;
	Payload.m_Metadata.m_Title = "Song";
	Payload.m_Metadata.m_Artist = "Singer";
	Payload.m_Metadata.m_DurationSec = 180;
	Payload.m_Source = "lrclib";

	ASSERT_TRUE(SaveCachePayload(pStorage.get(), "Song+Singer.lrc", Payload));
	char *pRaw = pStorage->ReadFileStr("qmclient/lyrics/Song+Singer.lrc", IStorage::TYPE_SAVE);
	ASSERT_NE(pRaw, nullptr);
	EXPECT_STREQ(pRaw, Payload.m_RawText.c_str());
	free(pRaw);
	EXPECT_TRUE(pStorage->FileExists("qmclient/lyrics/Song+Singer.meta.json", IStorage::TYPE_SAVE));

	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), "Song+Singer.lrc", &Loaded));
	EXPECT_EQ(Loaded.m_RawText, Payload.m_RawText);
	EXPECT_EQ(Loaded.m_TranslationText, Payload.m_TranslationText);
	EXPECT_EQ(Loaded.m_TransliterationText, Payload.m_TransliterationText);
	EXPECT_EQ(Loaded.m_Format, Payload.m_Format);
	EXPECT_EQ(Loaded.m_Metadata.m_Title, "Song");
	EXPECT_EQ(Loaded.m_Source, "lrclib");

	RemoveCachePayload(pStorage.get(), "Song+Singer.lrc");
	EXPECT_FALSE(pStorage->FileExists("qmclient/lyrics/Song+Singer.lrc", IStorage::TYPE_SAVE));
	EXPECT_FALSE(pStorage->FileExists("qmclient/lyrics/Song+Singer.meta.json", IStorage::TYPE_SAVE));
}

TEST(QmLyricsCache, LegacyJsonPayloadStillRoundTrips)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	SCachePayload Payload;
	Payload.m_RawText = "[00:01.00]legacy";
	Payload.m_Source = "lrclib";
	const std::string FileName = FileNameForKey("legacy-key");
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), FileName.c_str(), Payload));
	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, Payload.m_RawText);
}

TEST(QmLyricsCache, MigratesLegacyProviderPayloadToReadableTrackEntry)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	const std::string TrackKey = BuildCacheKey("Song", "Singer", "Album", 180);
	const std::string LegacyKey = TrackKey + "|provider:lrclib";
	SCacheEntry Legacy = MakeEntry(LegacyKey.c_str(), 1000);
	Legacy.m_Key = LegacyKey;
	Legacy.m_FileName = FileNameForKey(LegacyKey);
	CCacheIndex Index;
	Index.Upsert(Legacy, 100);

	SCachePayload Payload;
	Payload.m_RawText = "[00:01.00]legacy";
	Payload.m_Metadata.m_Title = "Song";
	Payload.m_Metadata.m_Artist = "Singer";
	Payload.m_Source = "lrclib";
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), Legacy.m_FileName.c_str(), Payload));
	ASSERT_TRUE(SaveCacheIndex(pStorage.get(), Index));

	ASSERT_TRUE(MigrateLegacyCacheEntry(pStorage.get(), &Index, LegacyKey, TrackKey, "Song", "Singer", 100));
	EXPECT_EQ(Index.Find(LegacyKey), nullptr);
	const SCacheEntry *pMigrated = Index.Find(TrackKey);
	ASSERT_NE(pMigrated, nullptr);
	EXPECT_EQ(pMigrated->m_FileName, "Song+Singer.lrc");
	EXPECT_FALSE(pStorage->FileExists(("qmclient/lyrics/" + Legacy.m_FileName).c_str(), IStorage::TYPE_SAVE));
	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), pMigrated->m_FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, Payload.m_RawText);
}

TEST(QmLyricsCache, MigratesReadableProviderPayloadWithoutOverwritingLegacyFile)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	const std::string TrackKey = BuildCacheKey("Song", "Singer", "Album", 180);
	const std::string LegacyKey = TrackKey + "|provider:lrclib";
	SCacheEntry Legacy = MakeEntry(LegacyKey.c_str(), 1000);
	Legacy.m_Key = LegacyKey;
	Legacy.m_FileName = "Song+Singer.lrc";
	CCacheIndex Index;
	Index.Upsert(Legacy, 100);
	const SCachePayload Payload = MakePayload("[00:01.00]legacy");
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), Legacy.m_FileName.c_str(), Payload));
	ASSERT_TRUE(SaveCacheIndex(pStorage.get(), Index));

	ASSERT_TRUE(MigrateLegacyCacheEntry(pStorage.get(), &Index, LegacyKey, TrackKey, "Song", "Singer", 100));
	const SCacheEntry *pMigrated = Index.Find(TrackKey);
	ASSERT_NE(pMigrated, nullptr);
	EXPECT_NE(pMigrated->m_FileName, Legacy.m_FileName);
	EXPECT_FALSE(pStorage->FileExists("qmclient/lyrics/Song+Singer.lrc", IStorage::TYPE_SAVE));
	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), pMigrated->m_FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, Payload.m_RawText);
}

TEST(QmLyricsCache, FailedReadableProviderMigrationPreservesLegacyIndexAndPayload)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	const std::string TrackKey = BuildCacheKey("Song", "Singer", "Album", 180);
	const std::string LegacyKey = TrackKey + "|provider:lrclib";
	SCacheEntry Legacy = MakeEntry(LegacyKey.c_str(), 1000);
	Legacy.m_Key = LegacyKey;
	Legacy.m_FileName = "Song+Singer.lrc";
	CCacheIndex Index;
	Index.Upsert(Legacy, 100);
	const SCachePayload Payload = MakePayload("[00:01.00]legacy");
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), Legacy.m_FileName.c_str(), Payload));
	ASSERT_TRUE(SaveCacheIndex(pStorage.get(), Index));
	BlockIndexTempFile(pStorage.get());

	EXPECT_FALSE(MigrateLegacyCacheEntry(pStorage.get(), &Index, LegacyKey, TrackKey, "Song", "Singer", 100));
	ASSERT_NE(Index.Find(LegacyKey), nullptr);
	EXPECT_EQ(Index.Find(TrackKey), nullptr);
	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), Legacy.m_FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, Payload.m_RawText);
	const std::string Replacement = FileNameForTrack("Song", "Singer", TrackKey, true);
	EXPECT_FALSE(pStorage->FileExists(("qmclient/lyrics/" + Replacement).c_str(), IStorage::TYPE_SAVE));

	CCacheIndex DiskIndex;
	ASSERT_TRUE(LoadCacheIndex(pStorage.get(), &DiskIndex));
	EXPECT_NE(DiskIndex.Find(LegacyKey), nullptr);
	EXPECT_EQ(DiskIndex.Find(TrackKey), nullptr);
}

TEST(QmLyricsCache, FailedCacheCommitPreservesIndexAndEvictedPayloads)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	SCacheEntry First = MakeEntry("first", 1000, 1000);
	First.m_FileName = "First+Singer.lrc";
	SCacheEntry Second = MakeEntry("second", 1001, 1001);
	Second.m_FileName = "Second+Singer.lrc";
	CCacheIndex Index;
	Index.Upsert(First, 2);
	Index.Upsert(Second, 2);
	const SCachePayload FirstPayload = MakePayload("[00:01.00]first", "First");
	const SCachePayload SecondPayload = MakePayload("[00:01.00]second", "Second");
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), First.m_FileName.c_str(), FirstPayload));
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), Second.m_FileName.c_str(), SecondPayload));
	ASSERT_TRUE(SaveCacheIndex(pStorage.get(), Index));
	BlockIndexTempFile(pStorage.get());

	SCacheEntry Incoming = MakeEntry("incoming", 2000, 2000);
	Incoming.m_FileName = "Incoming+Singer.lrc";
	const SCachePayload IncomingPayload = MakePayload("[00:01.00]incoming", "Incoming");
	EXPECT_FALSE(CommitCacheEntry(pStorage.get(), &Index, Incoming, IncomingPayload, 2));
	EXPECT_EQ(Index.Size(), 2u);
	EXPECT_NE(Index.Find(First.m_Key), nullptr);
	EXPECT_NE(Index.Find(Second.m_Key), nullptr);
	EXPECT_EQ(Index.Find(Incoming.m_Key), nullptr);
	EXPECT_FALSE(pStorage->FileExists("qmclient/lyrics/Incoming+Singer.lrc", IStorage::TYPE_SAVE));

	SCachePayload Loaded;
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), First.m_FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, FirstPayload.m_RawText);
	ASSERT_TRUE(LoadCachePayload(pStorage.get(), Second.m_FileName.c_str(), &Loaded));
	EXPECT_EQ(Loaded.m_RawText, SecondPayload.m_RawText);
	CCacheIndex DiskIndex;
	ASSERT_TRUE(LoadCacheIndex(pStorage.get(), &DiskIndex));
	EXPECT_EQ(DiskIndex.Size(), 2u);
	EXPECT_NE(DiskIndex.Find(First.m_Key), nullptr);
	EXPECT_NE(DiskIndex.Find(Second.m_Key), nullptr);
	EXPECT_EQ(DiskIndex.Find(Incoming.m_Key), nullptr);
}

TEST(QmLyricsCache, SuccessfulCacheCommitPublishesIndexBeforeRemovingEvictedPayload)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	SCacheEntry First = MakeEntry("first", 1000, 1000);
	First.m_FileName = "First+Singer.lrc";
	SCacheEntry Second = MakeEntry("second", 1001, 1001);
	Second.m_FileName = "Second+Singer.lrc";
	CCacheIndex Index;
	Index.Upsert(First, 2);
	Index.Upsert(Second, 2);
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), First.m_FileName.c_str(), MakePayload("[00:01.00]first", "First")));
	ASSERT_TRUE(SaveCachePayload(pStorage.get(), Second.m_FileName.c_str(), MakePayload("[00:01.00]second", "Second")));
	ASSERT_TRUE(SaveCacheIndex(pStorage.get(), Index));

	SCacheEntry Incoming = MakeEntry("incoming", 2000, 2000);
	Incoming.m_FileName = "Incoming+Singer.lrc";
	ASSERT_TRUE(CommitCacheEntry(pStorage.get(), &Index, Incoming, MakePayload("[00:01.00]incoming", "Incoming"), 2));
	EXPECT_EQ(Index.Find(First.m_Key), nullptr);
	EXPECT_NE(Index.Find(Second.m_Key), nullptr);
	EXPECT_NE(Index.Find(Incoming.m_Key), nullptr);
	EXPECT_FALSE(pStorage->FileExists("qmclient/lyrics/First+Singer.lrc", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->FileExists("qmclient/lyrics/Second+Singer.lrc", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->FileExists("qmclient/lyrics/Incoming+Singer.lrc", IStorage::TYPE_SAVE));

	CCacheIndex DiskIndex;
	ASSERT_TRUE(LoadCacheIndex(pStorage.get(), &DiskIndex));
	EXPECT_EQ(DiskIndex.Find(First.m_Key), nullptr);
	EXPECT_NE(DiskIndex.Find(Second.m_Key), nullptr);
	EXPECT_NE(DiskIndex.Find(Incoming.m_Key), nullptr);
}

TEST(QmLyricsCache, ComponentChecksCacheBeforeDispatchAndIndexesOnlySavedPayloads)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp");
	const size_t CacheLookup = Source.find("else if(TryLoadCache(");
	const size_t AutoFetch = Source.find("else if(g_Config.m_QmLyricsAutoFetch)");
	ASSERT_NE(CacheLookup, std::string::npos);
	ASSERT_NE(AutoFetch, std::string::npos);
	EXPECT_LT(CacheLookup, AutoFetch);

	EXPECT_NE(Source.find("if(!QmLyrics::CommitCacheEntry("), std::string::npos);
}

TEST(QmLyricsCache, UpsertAndLookup)
{
	CCacheIndex Idx;
	Idx.Upsert(MakeEntry("k1", 1000), 100);
	Idx.Upsert(MakeEntry("k2", 1001), 100);
	EXPECT_EQ(Idx.Size(), 2u);

	const SCacheEntry *pHit = Idx.Lookup("k1", 2000);
	ASSERT_NE(pHit, nullptr);
	EXPECT_EQ(pHit->m_Key, "k1");
	EXPECT_EQ(pHit->m_LastUsedAt, 2000); // Lookup 更新 LastUsedAt

	EXPECT_EQ(Idx.Lookup("k_missing", 2000), nullptr);
}

TEST(QmLyricsCache, UpsertReplacesAndReturnsEvictedFile)
{
	CCacheIndex Idx;
	SCacheEntry First = MakeEntry("k1", 1000);
	First.m_FileName = "old.json";
	Idx.Upsert(First, 100);

	SCacheEntry Replacement = MakeEntry("k1", 2000);
	Replacement.m_FileName = "new.json";
	const auto vEvicted = Idx.Upsert(Replacement, 100);

	ASSERT_EQ(vEvicted.size(), 1u);
	EXPECT_EQ(vEvicted[0], "old.json");
	const SCacheEntry *pHit = Idx.Lookup("k1", 3000);
	ASSERT_NE(pHit, nullptr);
	EXPECT_EQ(pHit->m_FileName, "new.json");
}

TEST(QmLyricsCache, RemoveReturnsPayloadFileName)
{
	CCacheIndex Idx;
	SCacheEntry Entry = MakeEntry("k1", 1000);
	Entry.m_FileName = "0123456789abcdef.json";
	Idx.Upsert(Entry, 100);

	std::string FileName;
	EXPECT_TRUE(Idx.Remove("k1", &FileName));
	EXPECT_EQ(FileName, "0123456789abcdef.json");
	EXPECT_EQ(Idx.Lookup("k1", 2000), nullptr);
	EXPECT_EQ(Idx.Size(), 0u);
	EXPECT_FALSE(Idx.Remove("k1", &FileName));
}

TEST(QmLyricsCache, LruEvictionWhenOverCapacity)
{
	CCacheIndex Idx;
	// 容量 3：插 4 个，最旧的应该被淘汰
	Idx.Upsert(MakeEntry("k1", 1000), 3);
	Idx.Upsert(MakeEntry("k2", 1001), 3);
	Idx.Upsert(MakeEntry("k3", 1002), 3);
	EXPECT_EQ(Idx.Size(), 3u);

	const auto vEvicted = Idx.Upsert(MakeEntry("k4", 1003), 3);
	EXPECT_EQ(Idx.Size(), 3u);
	ASSERT_EQ(vEvicted.size(), 1u);
	// 最旧 k1 (LastUsedAt=1000) 被淘汰
	EXPECT_EQ(Idx.Lookup("k1", 9999), nullptr);
	EXPECT_NE(Idx.Lookup("k2", 9999), nullptr);
	EXPECT_NE(Idx.Lookup("k4", 9999), nullptr);
}

TEST(QmLyricsCache, EvictExpiredHonorsTtl)
{
	CCacheIndex Idx;
	const int64_t Day = 86400;
	Idx.Upsert(MakeEntry("old", 0), 100); // 已过期（StoredAt=0）

	const int64_t Now = 30 * Day;
	Idx.Upsert(MakeEntry("fresh", Now - 2 * Day), 100);
	const auto vEvicted = Idx.EvictExpired(7, Now); // TTL 7 天
	EXPECT_EQ(vEvicted.size(), 1u);
	EXPECT_EQ(Idx.Size(), 1u);
	EXPECT_NE(Idx.Lookup("fresh", Now), nullptr);
	EXPECT_EQ(Idx.Lookup("old", Now), nullptr);
}

TEST(QmLyricsCache, EvictExpiredZeroTtlMeansNever)
{
	CCacheIndex Idx;
	Idx.Upsert(MakeEntry("old", 0), 100);
	const auto vEvicted = Idx.EvictExpired(0, 1000000000);
	EXPECT_EQ(vEvicted.size(), 0u);
	EXPECT_EQ(Idx.Size(), 1u);
}

TEST(QmLyricsCache, RoundTripJson)
{
	CCacheIndex Original;
	SCacheEntry E1 = MakeEntry("title1|artist1||180", 100, 200, "lrclib");
	E1.m_Score = 95.5f;
	Original.Upsert(E1, 100);
	SCacheEntry E2 = MakeEntry("titleB|artistB|album|200", 300, 400, "qq");
	E2.m_Score = 88.0f;
	Original.Upsert(E2, 100);

	const std::string Json = Original.ToJson();
	CCacheIndex Loaded;
	char aErr[128];
	ASSERT_TRUE(Loaded.FromJson(Json, aErr, sizeof(aErr))) << "FromJson failed: " << aErr;
	EXPECT_EQ(Loaded.Size(), 2u);

	const SCacheEntry *pE1 = Loaded.Lookup("title1|artist1||180", 500);
	ASSERT_NE(pE1, nullptr);
	EXPECT_EQ(pE1->m_Source, "lrclib");
	EXPECT_NEAR(pE1->m_Score, 95.5f, 0.01f);
	EXPECT_EQ(pE1->m_StoredAt, 100);

	const SCacheEntry *pE2 = Loaded.Lookup("titleB|artistB|album|200", 500);
	ASSERT_NE(pE2, nullptr);
	EXPECT_EQ(pE2->m_Source, "qq");
	EXPECT_NEAR(pE2->m_Score, 88.0f, 0.01f);
}

TEST(QmLyricsCache, FromJsonRejectsMalformed)
{
	CCacheIndex Idx;
	char aErr[128];
	EXPECT_FALSE(Idx.FromJson("", aErr, sizeof(aErr)));
	EXPECT_FALSE(Idx.FromJson("not json", aErr, sizeof(aErr)));
	EXPECT_FALSE(Idx.FromJson("[1,2,3]", aErr, sizeof(aErr))); // root not object
	EXPECT_FALSE(Idx.FromJson("{\"entries\":\"x\"}", aErr, sizeof(aErr))); // entries not array
}

TEST(QmLyricsCache, FromJsonEmptyEntriesArray)
{
	CCacheIndex Idx;
	char aErr[128];
	ASSERT_TRUE(Idx.FromJson("{\"entries\":[]}", aErr, sizeof(aErr)));
	EXPECT_EQ(Idx.Size(), 0u);
}

TEST(QmLyricsCache, FromJsonSkipsUnsafePayloadFileNames)
{
	const char *pJson = R"({
		"entries": [
			{"key":"ok","file":"0123456789abcdef.json","source":"lrclib","score":95,"used":20,"stored":10},
			{"key":"bad","file":"../outside.json","source":"lrclib","score":95,"used":20,"stored":10}
		]
	})";
	CCacheIndex Idx;
	char aErr[128];
	ASSERT_TRUE(Idx.FromJson(pJson, aErr, sizeof(aErr)));
	EXPECT_NE(Idx.Lookup("ok", 30), nullptr);
	EXPECT_EQ(Idx.Lookup("bad", 30), nullptr);
	EXPECT_EQ(Idx.Size(), 1u);
}
