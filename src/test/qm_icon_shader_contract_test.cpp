// QmClient 图标/MSDF 着色器跨后端合同。
// 着色器只在真机上由 GPU 编译执行，单元测试无法在不启动图形后端的情况下观察其行为；
// 因此这里只固定“三个后端必须实现同一套 MSDF 抗锯齿语义”这一构建/资源边界事实。
#include "qmclient_source_contract_test.h"

#include <gtest/gtest.h>

TEST(QmIconShaderContract, UsesDerivativeAntialiasingOnBothBackends)
{
	for(const char *pPath : {"data/shader/textured_msdf.frag", "data/shader/vulkan/textured_msdf.frag"})
	{
		const std::string Source = ReadRepoFile(pPath);
		EXPECT_NE(Source.find("Median"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("fwidth(TexCoord)"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("ScreenPxRange"), std::string::npos) << pPath;
		// w 分量同时编码描边宽度和运行时字形的 Alpha 真 SDF 选择。
		EXPECT_NE(Source.find("RequestedOutline > 0.0"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("UseTrueSdf"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("FillCoverage = clamp(SignedDistance * ScreenPxRange + 0.5"), std::string::npos) << pPath;
		// MTSDF：RGB median 负责填充，Alpha 真 SDF 负责描边外缘。
		EXPECT_NE(Source.find("vec4 Sample = texture(gTextureSampler, TexCoord)"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("TrueSignedDistance = Sample.a - 0.5"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("OuterCoverage = clamp(TrueSignedDistance * ScreenPxRange + OutlineWidth + 0.5"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("OutlineCoverage = max(OuterCoverage - FillCoverage"), std::string::npos) << pPath;
		EXPECT_NE(Source.find("+ 0.5, 0.0, 1.0);"), std::string::npos) << pPath;
		EXPECT_EQ(Source.find("aDirs[8]"), std::string::npos) << pPath;
		EXPECT_EQ(Source.find("TexCoord + aDirs"), std::string::npos) << pPath;
	}
}

// Metal 是 Apple 平台默认启用的后端（见 graphics_backend_contract.h）。
// 图标/图集字形在 Metal 上必须与 OpenGL、Vulkan 使用同一套 MSDF 求值与屏幕像素范围推导，
// 否则同一字形会在默认后端上出现描边粗细不一致。
TEST(QmIconShaderContract, MetalMsdfMatchesOpenGlAndVulkanSemantics)
{
	const std::string Metal = ReadRepoFile("data/shader/metal/qmclient.metal");
	ASSERT_FALSE(Metal.empty());

	// 中值求值：GLSL 的 max(min(r, g), min(max(r, g), b)) 在 Metal 中是同一公式。
	EXPECT_NE(Metal.find("float QmClientMedian(float3 Value)"), std::string::npos);
	EXPECT_NE(Metal.find("return max(min(Value.r, Value.g), min(max(Value.r, Value.g), Value.b));"), std::string::npos);

	// 屏幕像素范围推导必须保留 fwidth 导数，否则小字号图标会退化成硬边或糊边。
	EXPECT_NE(Metal.find("const float4 Sample = Texture.sample(Sampler, Input.m_TexCoord);"), std::string::npos);
		EXPECT_NE(Metal.find("const float SignedDistance = UseTrueSdf ? TrueSignedDistance : QmClientMedian(Sample.rgb) - 0.5;"), std::string::npos);
	EXPECT_NE(Metal.find("const float TrueSignedDistance = Sample.a - 0.5;"), std::string::npos);
	EXPECT_NE(Metal.find("const float2 ScreenTexSize = 1.0 / fwidth(Input.m_TexCoord);"), std::string::npos);
	EXPECT_NE(Metal.find("const float ScreenPxRange = max(0.5 * dot(UnitRange, ScreenTexSize), 1.0);"), std::string::npos);
		EXPECT_NE(Metal.find("if(RequestedOutline > 0.0)"), std::string::npos);
	EXPECT_NE(Metal.find("const float FillCoverage = clamp(SignedDistance * ScreenPxRange + 0.5, 0.0, 1.0);"), std::string::npos);
	EXPECT_NE(Metal.find("const float OuterCoverage = clamp(TrueSignedDistance * ScreenPxRange + OutlineWidth + 0.5, 0.0, 1.0);"), std::string::npos);
	EXPECT_NE(Metal.find("const float OutlineCoverage = max(OuterCoverage - FillCoverage, 0.0);"), std::string::npos);
	EXPECT_EQ(Metal.find("aDirs[8]"), std::string::npos);

	// 入口必须绑定专用 MSDF 管线，并在片段阶段接收 MSDF 参数缓冲。
	EXPECT_NE(Metal.find("fragment float4 qmclient_textured_msdf_fragment("), std::string::npos);
	EXPECT_NE(Metal.find("constant float4 &MsdfParams [[buffer(1)]]"), std::string::npos);
}
