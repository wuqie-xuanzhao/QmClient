#include <engine/client/backend/graphics_backend_contract.h>
#include <engine/client/backend_sdl.h>

#include <gtest/gtest.h>

TEST(GraphicsBackendContract, NamesAreStable)
{
	EXPECT_STREQ(graphics_backend::BackendName(BACKEND_TYPE_OPENGL), "OpenGL");
	EXPECT_STREQ(graphics_backend::BackendName(BACKEND_TYPE_OPENGL_ES), "GLES");
	EXPECT_STREQ(graphics_backend::BackendName(BACKEND_TYPE_VULKAN), "Vulkan");
	EXPECT_STREQ(graphics_backend::BackendName(BACKEND_TYPE_METAL), "Metal");
}

TEST(GraphicsBackendContract, ParsesKnownNamesAndRejectsUnknownNames)
{
	EXPECT_EQ(graphics_backend::ParseBackendName("opengl", BACKEND_TYPE_VULKAN), BACKEND_TYPE_OPENGL);
	const EBackendType ExpectedOpenGLES = graphics_backend::IsBackendSelectable(BACKEND_TYPE_OPENGL_ES) ? BACKEND_TYPE_OPENGL_ES : BACKEND_TYPE_VULKAN;
	EXPECT_EQ(graphics_backend::ParseBackendName("OpenGL ES", ExpectedOpenGLES == BACKEND_TYPE_VULKAN ? BACKEND_TYPE_VULKAN : BACKEND_TYPE_OPENGL), ExpectedOpenGLES);
	const EBackendType ExpectedVulkan = graphics_backend::IsBackendSelectable(BACKEND_TYPE_VULKAN) ? BACKEND_TYPE_VULKAN : BACKEND_TYPE_OPENGL;
	EXPECT_EQ(graphics_backend::ParseBackendName("vUlKaN", BACKEND_TYPE_OPENGL), ExpectedVulkan);
	EXPECT_EQ(graphics_backend::ParseBackendName("unknown", BACKEND_TYPE_OPENGL), BACKEND_TYPE_OPENGL);
	EXPECT_EQ(graphics_backend::ParseBackendName(nullptr, BACKEND_TYPE_VULKAN), BACKEND_TYPE_VULKAN);
}

TEST(GraphicsBackendContract, MetalIsSelectableOnlyWhenCompiledForMacos)
{
#if defined(CONF_PLATFORM_MACOS) && defined(CONF_BACKEND_METAL) && defined(CONF_BACKEND_METAL_READY)
	EXPECT_TRUE(graphics_backend::IsMetalCompiled());
	EXPECT_EQ(graphics_backend::ParseBackendName("Metal", BACKEND_TYPE_OPENGL), BACKEND_TYPE_METAL);
#else
	EXPECT_FALSE(graphics_backend::IsMetalCompiled());
	EXPECT_EQ(graphics_backend::ParseBackendName("Metal", BACKEND_TYPE_OPENGL), BACKEND_TYPE_OPENGL);
#endif
	EXPECT_TRUE(graphics_backend::IsKnownBackendName("Metal"));
	EXPECT_TRUE(graphics_backend::IsKnownBackendName("OpenGL ES"));
	EXPECT_FALSE(graphics_backend::IsKnownBackendName("custom"));
	EXPECT_TRUE(graphics_backend::IsKnownUnavailableBackendName("Metal"));
	EXPECT_FALSE(graphics_backend::IsKnownUnavailableBackendName("OpenGL"));
	EXPECT_FALSE(graphics_backend::IsKnownUnavailableBackendName("custom"));
	EXPECT_FALSE(graphics_backend::IsBackendSelectable(BACKEND_TYPE_AUTO));
}

TEST(GraphicsBackendContract, VersionTupleIdentityIsApiNeutral)
{
	EXPECT_TRUE(graphics_backend::UsesOpenGLVersionTuple(BACKEND_TYPE_OPENGL));
	EXPECT_TRUE(graphics_backend::UsesOpenGLVersionTuple(BACKEND_TYPE_OPENGL_ES));
	EXPECT_FALSE(graphics_backend::UsesOpenGLVersionTuple(BACKEND_TYPE_VULKAN));
	EXPECT_FALSE(graphics_backend::UsesOpenGLVersionTuple(BACKEND_TYPE_METAL));
	EXPECT_TRUE(graphics_backend::PreservesOpenGLVersionTuple(BACKEND_TYPE_METAL));
	EXPECT_FALSE(graphics_backend::PreservesOpenGLVersionTuple(BACKEND_TYPE_VULKAN));
}

TEST(GraphicsBackendContract, FrameSerializationWorkaroundIsVulkanOnly)
{
	EXPECT_FALSE(graphics_backend::RequiresFrameSerializationWorkaround(BACKEND_TYPE_OPENGL));
	EXPECT_FALSE(graphics_backend::RequiresFrameSerializationWorkaround(BACKEND_TYPE_OPENGL_ES));
	EXPECT_TRUE(graphics_backend::RequiresFrameSerializationWorkaround(BACKEND_TYPE_VULKAN));
	EXPECT_FALSE(graphics_backend::RequiresFrameSerializationWorkaround(BACKEND_TYPE_METAL));
}

TEST(GraphicsBackendContract, MetalInitializationIsNotAnOpenGLVersionFailure)
{
	EXPECT_TRUE(IsGraphicsBackendMetalInitError(GRAPHICS_BACKEND_ERROR_CODE_METAL_INIT_FAILED));
	EXPECT_FALSE(IsGraphicsBackendOpenGLRetryableError(GRAPHICS_BACKEND_ERROR_CODE_METAL_INIT_FAILED));
	EXPECT_TRUE(IsGraphicsBackendOpenGLRetryableError(GRAPHICS_BACKEND_ERROR_CODE_GL_CONTEXT_FAILED));
	EXPECT_TRUE(IsGraphicsBackendOpenGLRetryableError(GRAPHICS_BACKEND_ERROR_CODE_GL_VERSION_FAILED));
}

TEST(GraphicsBackendContract, SafeBackendConfigIsDeterministic)
{
	constexpr auto SafeConfig = graphics_backend::SafeBackendConfig();
	EXPECT_STREQ(SafeConfig.m_pBackend, "OpenGL");
	EXPECT_EQ(SafeConfig.m_GLMajor, 4);
	EXPECT_EQ(SafeConfig.m_GLMinor, 1);
	EXPECT_EQ(SafeConfig.m_GLPatch, 0);
	EXPECT_EQ(SafeConfig.m_FsaaSamples, 0);
	EXPECT_EQ(SafeConfig.m_Fullscreen, 0);
	EXPECT_EQ(SafeConfig.m_Borderless, 0);
}

TEST(GraphicsBackendContract, ConfiguredIdentityMatchesOnlyRelevantVersionFields)
{
	EXPECT_TRUE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_OPENGL, "OpenGL", 4, 1, 0, "opengl", 4, 1, 0));
	EXPECT_FALSE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_OPENGL, "OpenGL", 4, 1, 0, "opengl", 3, 3, 0));
#if defined(CONF_BACKEND_VULKAN)
	EXPECT_TRUE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_VULKAN, "Vulkan", 0, 0, 0, "vulkan", 4, 1, 0));
#else
	EXPECT_FALSE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_VULKAN, "Vulkan", 0, 0, 0, "vulkan", 4, 1, 0));
#endif
#if defined(CONF_PLATFORM_MACOS) && defined(CONF_BACKEND_METAL) && defined(CONF_BACKEND_METAL_READY)
	EXPECT_TRUE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_METAL, "Metal", 0, 0, 0, "metal", 4, 1, 0));
#else
	EXPECT_FALSE(graphics_backend::MatchesConfiguredBackend(BACKEND_TYPE_METAL, "Metal", 0, 0, 0, "metal", 4, 1, 0));
#endif
}

TEST(GraphicsBackendContract, BackendCapabilitiesDefaultToUnsupported)
{
	SBackendCapabilities Capabilities;
	EXPECT_FALSE(Capabilities.m_TileBuffering);
	EXPECT_FALSE(Capabilities.m_QuadBuffering);
	EXPECT_FALSE(Capabilities.m_TextBuffering);
	EXPECT_FALSE(Capabilities.m_QuadContainerBuffering);
	EXPECT_FALSE(Capabilities.m_MipMapping);
	EXPECT_FALSE(Capabilities.m_NPOTTextures);
	EXPECT_FALSE(Capabilities.m_3DTextures);
	EXPECT_FALSE(Capabilities.m_2DArrayTextures);
	EXPECT_FALSE(Capabilities.m_2DArrayTexturesAsExtension);
	EXPECT_FALSE(Capabilities.m_ShaderSupport);
	EXPECT_FALSE(Capabilities.m_TexturedMsdf.load(std::memory_order_relaxed));
	EXPECT_FALSE(Capabilities.m_RenderTargets);
	EXPECT_FALSE(Capabilities.m_RenderTargetGaussianBlur);
	EXPECT_FALSE(Capabilities.m_BackbufferCapture);
	EXPECT_FALSE(Capabilities.m_RenderTargetExternalPassRequiresSingleSample);
	EXPECT_STREQ(Capabilities.m_pRenderTargetSupportReason, "not_initialized");
	EXPECT_FALSE(Capabilities.m_TrianglesAsQuads);
	EXPECT_EQ(Capabilities.m_ContextMajor, 0);
	EXPECT_EQ(Capabilities.m_ContextMinor, 0);
	EXPECT_EQ(Capabilities.m_ContextPatch, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextMajor, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextMinor, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextPatch, 0);
}

TEST(GraphicsBackendContract, BackendCapabilitiesResetClearsPreviousBackendState)
{
	SBackendCapabilities Capabilities;
	Capabilities.m_TileBuffering = true;
	Capabilities.m_QuadBuffering = true;
	Capabilities.m_TextBuffering = true;
	Capabilities.m_QuadContainerBuffering = true;
	Capabilities.m_MipMapping = true;
	Capabilities.m_NPOTTextures = true;
	Capabilities.m_3DTextures = true;
	Capabilities.m_2DArrayTextures = true;
	Capabilities.m_2DArrayTexturesAsExtension = true;
	Capabilities.m_ShaderSupport = true;
	Capabilities.m_MediaIslandSdf = true;
	Capabilities.m_RoundedRectSdf = true;
	Capabilities.m_TexturedMsdf.store(true, std::memory_order_relaxed);
	Capabilities.m_RenderTargets = true;
	Capabilities.m_RenderTargetGaussianBlur = true;
	Capabilities.m_BackbufferCapture = true;
	Capabilities.m_RenderTargetExternalPassRequiresSingleSample = true;
	Capabilities.m_pRenderTargetSupportReason = "supported";
	Capabilities.m_TrianglesAsQuads = true;
	Capabilities.m_ContextMajor = 4;
	Capabilities.m_ContextMinor = 1;
	Capabilities.m_ContextPatch = 2;
	Capabilities.m_DetectedContextMajor = 4;
	Capabilities.m_DetectedContextMinor = 1;
	Capabilities.m_DetectedContextPatch = 2;

	Capabilities.Reset();

	EXPECT_FALSE(Capabilities.m_TileBuffering);
	EXPECT_FALSE(Capabilities.m_QuadBuffering);
	EXPECT_FALSE(Capabilities.m_TextBuffering);
	EXPECT_FALSE(Capabilities.m_QuadContainerBuffering);
	EXPECT_FALSE(Capabilities.m_MipMapping);
	EXPECT_FALSE(Capabilities.m_NPOTTextures);
	EXPECT_FALSE(Capabilities.m_3DTextures);
	EXPECT_FALSE(Capabilities.m_2DArrayTextures);
	EXPECT_FALSE(Capabilities.m_2DArrayTexturesAsExtension);
	EXPECT_FALSE(Capabilities.m_ShaderSupport);
	EXPECT_FALSE(Capabilities.m_MediaIslandSdf);
	EXPECT_FALSE(Capabilities.m_RoundedRectSdf);
	EXPECT_FALSE(Capabilities.m_TexturedMsdf.load(std::memory_order_relaxed));
	EXPECT_FALSE(Capabilities.m_RenderTargets);
	EXPECT_FALSE(Capabilities.m_RenderTargetGaussianBlur);
	EXPECT_FALSE(Capabilities.m_BackbufferCapture);
	EXPECT_FALSE(Capabilities.m_RenderTargetExternalPassRequiresSingleSample);
	EXPECT_STREQ(Capabilities.m_pRenderTargetSupportReason, "not_initialized");
	EXPECT_FALSE(Capabilities.m_TrianglesAsQuads);
	EXPECT_EQ(Capabilities.m_ContextMajor, 0);
	EXPECT_EQ(Capabilities.m_ContextMinor, 0);
	EXPECT_EQ(Capabilities.m_ContextPatch, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextMajor, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextMinor, 0);
	EXPECT_EQ(Capabilities.m_DetectedContextPatch, 0);
}
