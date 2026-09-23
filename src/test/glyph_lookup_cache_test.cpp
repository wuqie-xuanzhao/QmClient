#include <engine/client/glyph_lookup_cache.h>

#include <gtest/gtest.h>

TEST(GlyphLookupCache, ReturnsOnlyExactFaceCharacterAndSizeMatches)
{
	CQmGlyphLookupCache<int> Cache;
	int Glyph = 42;
	const int FaceA = 1;
	const int FaceB = 2;

	Cache.Store(&FaceA, 'A', 12, &Glyph);
	EXPECT_EQ(Cache.Find(&FaceA, 'A', 12), &Glyph);
	// 任一键不同都不得命中，否则会返回别的字面/字号的字形。
	EXPECT_EQ(Cache.Find(&FaceB, 'A', 12), nullptr);
	EXPECT_EQ(Cache.Find(&FaceA, 'B', 12), nullptr);
	EXPECT_EQ(Cache.Find(&FaceA, 'A', 13), nullptr);
	// 未写入过的槽位同样不命中。
	EXPECT_EQ(Cache.Find(&FaceA, 'Z', 99), nullptr);
}

TEST(GlyphLookupCache, ResetDropsEveryEntry)
{
	CQmGlyphLookupCache<int> Cache;
	int Glyph = 7;
	Cache.Store(nullptr, 'x', 10, &Glyph);
	ASSERT_EQ(Cache.Find(nullptr, 'x', 10), &Glyph);

	// 字形表清空或字体回退链变化后必须失效，否则会返回悬空指针。
	Cache.Reset();
	EXPECT_EQ(Cache.Find(nullptr, 'x', 10), nullptr);
}

TEST(GlyphLookupCache, OverwritesSameSlotWithoutGrowing)
{
	CQmGlyphLookupCache<int> Cache;
	int A = 1;
	int B = 2;
	// 同一 (face, chr, size) 反复写入只更新内容。
	for(int i = 0; i < 1000; ++i)
		Cache.Store(nullptr, 'a', 5, i % 2 == 0 ? &A : &B);
	EXPECT_EQ(Cache.Find(nullptr, 'a', 5), &B);
}
