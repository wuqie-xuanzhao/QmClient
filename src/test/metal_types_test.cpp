#include <engine/client/backend/metal/metal_types.h>

#include <gtest/gtest.h>

#include <cstddef>

TEST(MetalTypes, UniformsMatchTheMSLBufferLayout)
{
	EXPECT_EQ(alignof(SMetalUniforms), 16U);
	EXPECT_EQ(sizeof(SMetalFloat4x4), 64U);
	EXPECT_EQ(sizeof(SMetalFloat4), 16U);
	EXPECT_EQ(sizeof(SMetalUniforms), 80U);
	EXPECT_EQ(offsetof(SMetalUniforms, m_MVP), 0U);
	EXPECT_EQ(offsetof(SMetalUniforms, m_Color), 64U);
}
