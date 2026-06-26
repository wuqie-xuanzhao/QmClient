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

TEST(QmLyricsCache, ValidatesPayloadFileName)
{
	EXPECT_TRUE(IsValidCachePayloadFileName("0123456789abcdef.json"));
	EXPECT_TRUE(IsValidCachePayloadFileName("0123456789ABCDEF.json"));
	EXPECT_FALSE(IsValidCachePayloadFileName("../outside.json"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdeg.json"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdef.txt"));
	EXPECT_FALSE(IsValidCachePayloadFileName("0123456789abcdef0.json"));
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
