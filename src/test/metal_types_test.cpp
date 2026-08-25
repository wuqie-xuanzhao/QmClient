#include <engine/client/backend/metal/metal_types.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>

TEST(MetalTypes, UniformsMatchTheMSLBufferLayout)
{
	EXPECT_EQ(alignof(SMetalUniforms), 16U);
	EXPECT_EQ(sizeof(SMetalFloat4x4), 64U);
	EXPECT_EQ(sizeof(SMetalFloat4), 16U);
	EXPECT_EQ(sizeof(SMetalUniforms), 80U);
	EXPECT_EQ(offsetof(SMetalUniforms, m_MVP), 0U);
	EXPECT_EQ(offsetof(SMetalUniforms, m_Color), 64U);
}

TEST(MetalTypes, ComputesRgbaAndTextLayouts)
{
	SMetalTextureLayout Layout;
	ASSERT_TRUE(MetalTextureLayout(3, 5, EMetalTextureFormat::RGBA8, true, Layout));
	EXPECT_EQ(Layout.m_BytesPerPixel, 4U);
	EXPECT_EQ(Layout.m_RowBytes, 12U);
	EXPECT_EQ(Layout.m_DataBytes, 60U);
	EXPECT_EQ(Layout.m_MipLevels, 1U);

	ASSERT_TRUE(MetalTextureLayout(8, 4, EMetalTextureFormat::R8, false, Layout));
	EXPECT_EQ(Layout.m_RowBytes, 8U);
	EXPECT_EQ(Layout.m_DataBytes, 8U * 4U + 4U * 2U + 2U * 1U + 1U * 1U);
	EXPECT_EQ(Layout.m_MipLevels, 4U);
}

TEST(MetalTypes, RejectsTextureSizeOverflow)
{
	SMetalTextureLayout Layout;
	EXPECT_FALSE(MetalTextureLayout(std::numeric_limits<size_t>::max(), 2, EMetalTextureFormat::RGBA8, true, Layout));
	EXPECT_EQ(Layout.m_DataBytes, 0U);
}

TEST(MetalTypes, ValidatesSubregionsWithoutOverflow)
{
	EXPECT_TRUE(MetalValidateSubregion(16, 8, 0, 0, 16, 8));
	EXPECT_TRUE(MetalValidateSubregion(16, 8, 4, 2, 12, 6));
	EXPECT_FALSE(MetalValidateSubregion(16, 8, 5, 0, 12, 8));
	EXPECT_FALSE(MetalValidateSubregion(16, 8, 0, 0, 0, 1));
	EXPECT_FALSE(MetalValidateSubregion(16, 8, std::numeric_limits<size_t>::max(), 0, 1, 1));
}

TEST(MetalTypes, RejectsStaleTextureHandles)
{
	CMetalTextureRegistry Registry;
	const SMetalTextureHandle First = Registry.Allocate(7);
	ASSERT_TRUE(Registry.IsValid(First));
	EXPECT_FALSE(Registry.Allocate(7).IsValid());
	ASSERT_TRUE(Registry.Release(First));
	EXPECT_FALSE(Registry.IsValid(First));

	const SMetalTextureHandle Second = Registry.Allocate(7);
	ASSERT_TRUE(Registry.IsValid(Second));
	EXPECT_NE(First.m_Generation, Second.m_Generation);
	EXPECT_FALSE(Registry.Release(First));
	EXPECT_TRUE(Registry.IsValid(Second));
}
