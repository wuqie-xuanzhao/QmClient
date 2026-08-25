#include <gtest/gtest.h>
#include <test/test.h>

#include <string>

TEST(MetalBackendContract, RuntimeGpuFailureIsPublishedAsAStickyRenderError)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("m_GpuFailureFrameId"), std::string::npos);
	EXPECT_NE(Source.find("m_GpuFailureCommandId"), std::string::npos);
	EXPECT_NE(Source.find("m_GpuFailureStatus"), std::string::npos);
	EXPECT_NE(Source.find("m_GpuFailureMutex"), std::string::npos);
	EXPECT_NE(Source.find("pError.domain.UTF8String"), std::string::npos);
	EXPECT_NE(Source.find("pError.code"), std::string::npos);
	EXPECT_NE(Source.find("GFX_ERROR_TYPE_RENDER_SUBMIT_FAILED"), std::string::npos);
	EXPECT_NE(Source.find("stage=completion"), std::string::npos);
	EXPECT_NE(Source.find("command_id="), std::string::npos);
	EXPECT_NE(Source.find("RUN_COMMAND_COMMAND_ERROR"), std::string::npos);
	EXPECT_NE(Source.find("if(!Success)\n\t\t\t\tRecordGpuFailure"), std::string::npos);
}

TEST(MetalBackendContract, RuntimeFailureAllowsOnlyLifecycleCommandsThrough)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	const size_t Guard = Source.find("const bool IsLifecycleCommand");
	const size_t FailureCheck = Source.find("if(!IsLifecycleCommand && SetGpuFailureError())", Guard);
	ASSERT_NE(Guard, std::string::npos);
	ASSERT_NE(FailureCheck, std::string::npos);
	EXPECT_LT(Guard, FailureCheck);
	EXPECT_NE(Source.find("CMD_SHUTDOWN", Guard), std::string::npos);
	EXPECT_NE(Source.find("CMD_POST_SHUTDOWN", Guard), std::string::npos);
}

TEST(MetalBackendContract, BufferContainerCommandsHaveExplicitResourceHandling)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("CMD_CREATE_BUFFER_CONTAINER"), std::string::npos);
	EXPECT_NE(Source.find("CMD_UPDATE_BUFFER_CONTAINER"), std::string::npos);
	EXPECT_NE(Source.find("CMD_DELETE_BUFFER_CONTAINER"), std::string::npos);
	EXPECT_NE(Source.find("CMD_INDICES_REQUIRED_NUM_NOTIFY"), std::string::npos);
	EXPECT_NE(Source.find("m_RequiredIndicesNum = std::max"), std::string::npos);
	EXPECT_NE(Source.find("MetalValidateVertexAttribute"), std::string::npos);
	EXPECT_NE(Source.find("if(Stride == 0)"), std::string::npos);
	EXPECT_NE(Source.find("TightStride"), std::string::npos);
}

TEST(MetalBackendContract, TileAndQuadCommandsUseDedicatedMetalPipelines)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("CMD_RENDER_TILE_LAYER"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_BORDER_TILE"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_QUAD_LAYER_GROUPED"), std::string::npos);
	EXPECT_NE(Source.find("TilePipelineIndex"), std::string::npos);
	EXPECT_NE(Source.find("QuadPipelineIndex"), std::string::npos);
	EXPECT_NE(Source.find("MatchesContainerLayout"), std::string::npos);
	EXPECT_NE(Source.find("MTLIndexTypeUInt16"), std::string::npos);

	const std::string Shader = ReadTestSourceFile("data/shader/metal/qmclient.metal");
	EXPECT_NE(Shader.find("qmclient_tile_vertex"), std::string::npos);
	EXPECT_NE(Shader.find("TexScale.yx"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_quad_vertex_grouped"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_quad_vertex_ungrouped"), std::string::npos);
}

TEST(MetalBackendContract, BufferedTextAndQuadContainerCommandsUseValidatedResources)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("CMD_RENDER_TEXT"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_QUAD_CONTAINER"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_QUAD_CONTAINER_EX"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE"), std::string::npos);
	EXPECT_NE(Source.find("GetStandardQuadDrawResources"), std::string::npos);
	EXPECT_NE(Source.find("MatchesStandardVertexLayout"), std::string::npos);
	EXPECT_NE(Source.find("QuadContainerExPipelineIndex"), std::string::npos);
	EXPECT_NE(Source.find("SpriteMultiplePipelineIndex"), std::string::npos);
	EXPECT_NE(Source.find("instanceCount:RenderCount"), std::string::npos);
	EXPECT_NE(Source.find("METAL_MAX_SPRITES"), std::string::npos);
	EXPECT_NE(Source.find("QuadEnd > std::numeric_limits<size_t>::max() / 6"), std::string::npos);

	const std::string Shader = ReadTestSourceFile("data/shader/metal/qmclient.metal");
	EXPECT_NE(Shader.find("qmclient_quad_container_ex_vertex"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_quad_container_ex_textured_fragment"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_sprite_multiple_vertex"), std::string::npos);
	EXPECT_NE(Shader.find("Uniforms.m_aRenderInfo[InstanceId]"), std::string::npos);
}

TEST(MetalBackendContract, TextureArrayPathConvertsAtlasAndSamplesArrayLayers)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("m_TextureArray"), std::string::npos);
	EXPECT_NE(Source.find("MetalTextureArrayLayout"), std::string::npos);
	EXPECT_NE(Source.find("Texture2DTo3D"), std::string::npos);
	EXPECT_NE(Source.find("UploadTextureArray"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TEX3D"), std::string::npos);
	EXPECT_NE(Source.find("TextureArrayPipelineIndex"), std::string::npos);
	EXPECT_NE(Source.find("m_2DArrayTextures = true"), std::string::npos);
	EXPECT_EQ(Source.find("m_3DTextures = true"), std::string::npos);

	const std::string Shader = ReadTestSourceFile("data/shader/metal/qmclient.metal");
	EXPECT_NE(Shader.find("texture2d_array<float>"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_tex_array_vertex"), std::string::npos);
	EXPECT_NE(Shader.find("qmclient_tex_array_fragment"), std::string::npos);
}

TEST(MetalBackendContract, RenderTargetsOwnAttachmentsAndRestoreDrawableRendering)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("struct SRenderTarget"), std::string::npos);
	EXPECT_NE(Source.find("m_vRenderTargets"), std::string::npos);
	EXPECT_NE(Source.find("pDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm"), std::string::npos);
	EXPECT_EQ(Source.find("pDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm"), std::string::npos);
	EXPECT_NE(Source.find("MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead"), std::string::npos);
	EXPECT_NE(Source.find("pLayer.framebufferOnly = NO"), std::string::npos);
	EXPECT_NE(Source.find("void DestroyAllRenderTargets()"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_Create"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_Destroy"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_Begin"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_End"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_Draw"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_CaptureBackbuffer"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_CREATE"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_DESTROY"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_BEGIN"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_END"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_DRAW"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_CAPTURE_BACKBUFFER"), std::string::npos);
	EXPECT_NE(Source.find("m_RenderTargetState"), std::string::npos);
	EXPECT_NE(Source.find("BeginRenderEncoderForTexture"), std::string::npos);
	EXPECT_NE(Source.find("const size_t UniformOffset = (VertexOffset + VertexBytes + 255) & ~size_t(255)"), std::string::npos);
	EXPECT_NE(Source.find("m_BackbufferHasContents ? MTLLoadActionLoad : MTLLoadActionClear"), std::string::npos);
	EXPECT_NE(Source.find("[m_CurrentRenderEncoder setFragmentTexture:m_CurrentDrawable.texture atIndex:0]"), std::string::npos);
	EXPECT_NE(Source.find("EndActiveEncoders();\n\t\treturn true;\n\t}\n\n\tvoid EndActiveEncoders()"), std::string::npos);
	EXPECT_NE(Source.find("m_RenderTargets = false"), std::string::npos);
	EXPECT_EQ(Source.find("m_RenderTargets = true"), std::string::npos);
	EXPECT_NE(Source.find("m_pRenderTargetSupportReason = \"metal_render_target_readback_not_implemented\""), std::string::npos);
}

TEST(MetalBackendContract, MultisampleBackbufferUsesResolveAttachmentAndRealDeviceSupport)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("bool Cmd_MultiSampling"), std::string::npos);
	EXPECT_NE(Source.find("CMD_MULTISAMPLING"), std::string::npos);
	EXPECT_NE(Source.find("[m_Device supportsTextureSampleCount:2]"), std::string::npos);
	EXPECT_NE(Source.find("MTLTextureType2DMultisample"), std::string::npos);
	EXPECT_NE(Source.find("pDescriptor.sampleCount = m_MultiSamplingCount"), std::string::npos);
	EXPECT_NE(Source.find("MTLStoreActionStoreAndMultisampleResolve"), std::string::npos);
	EXPECT_NE(Source.find("pPass.colorAttachments[0].resolveTexture = ResolveTexture"), std::string::npos);
	EXPECT_NE(Source.find("CreatePipelineStates(m_MultiSamplingCount, m_aMultiSamplePipelineStates)"), std::string::npos);
	EXPECT_NE(Source.find("const size_t UniformOffset = (VertexOffset + Bytes + 255) & ~size_t(255)"), std::string::npos);
	EXPECT_NE(Source.find("m_CurrentDrawable != nil || m_RenderEncoderStarted || m_CurrentRenderEncoder != nil || m_CurrentBlitEncoder != nil || m_BackbufferHasContents"), std::string::npos);
	EXPECT_NE(Source.find("Requested %u FSAA samples, using %u."), std::string::npos);
}

TEST(MetalBackendContract, GaussianBlurUsesSingleSamplePingPongPasses)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	EXPECT_NE(Source.find("bool CreateGaussianBlurPipeline()"), std::string::npos);
	EXPECT_NE(Source.find("qmclient_gaussian_blur_fragment"), std::string::npos);
	EXPECT_NE(Source.find("pPipelineDescriptor.rasterSampleCount = 1"), std::string::npos);
	EXPECT_NE(Source.find("bool Cmd_RenderTarget_GaussianBlurPass"), std::string::npos);
	EXPECT_NE(Source.find("pCommand->m_SourceTargetId == DestinationTargetId"), std::string::npos);
	EXPECT_NE(Source.find("Source.m_Width != Destination.m_Width"), std::string::npos);
	EXPECT_NE(Source.find("EndActiveEncoders();\n\t\tif(!BeginRenderEncoder"), std::string::npos);
	EXPECT_NE(Source.find("setFragmentBuffer:Frame.m_VertexBuffer offset:FragmentUniformOffset atIndex:1"), std::string::npos);
	EXPECT_NE(Source.find("CMD_RENDER_TARGET_GAUSSIAN_BLUR_PASS"), std::string::npos);
	EXPECT_NE(Source.find("m_RenderTargetGaussianBlur = false"), std::string::npos);

	const std::string Shader = ReadTestSourceFile("data/shader/metal/qmclient.metal");
	EXPECT_NE(Shader.find("float GaussianBlurWeight"), std::string::npos);
	EXPECT_NE(Shader.find("fragment float4 qmclient_gaussian_blur_fragment"), std::string::npos);
	EXPECT_NE(Shader.find("if(Offset > Radius)"), std::string::npos);
	EXPECT_NE(Shader.find("Texture.sample(Sampler, Input.m_TexCoord + SampleOffset)"), std::string::npos);
}

TEST(MetalBackendContract, CleanupWaitsBeforeReleasingCommandBuffers)
{
	const std::string Source = ReadTestSourceFile("src/engine/client/backend/metal/backend_metal.mm");
	const size_t Wait = Source.find("void WaitForGpuIdle()");
	const size_t Release = Source.find("void ReleaseGpuObjects()");
	const size_t ErroneousCleanup = Source.find("void ErroneousCleanup() override");
	const size_t Destructor = Source.find("~CCommandProcessorFragment_Metal() override");
	ASSERT_NE(Wait, std::string::npos);
	ASSERT_NE(Release, std::string::npos);
	ASSERT_NE(ErroneousCleanup, std::string::npos);
	ASSERT_NE(Destructor, std::string::npos);
	const size_t ErroneousWait = Source.find("WaitForGpuIdle();", ErroneousCleanup);
	const size_t ErroneousContainerRelease = Source.find("DestroyAllBufferContainers();", ErroneousWait);
	const size_t ErroneousBufferRelease = Source.find("DestroyAllBuffers();", ErroneousContainerRelease);
	const size_t ErroneousGpuRelease = Source.find("ReleaseGpuObjects();", ErroneousBufferRelease);
	const size_t DestructorWait = Source.find("WaitForGpuIdle();", Destructor);
	const size_t DestructorContainerRelease = Source.find("DestroyAllBufferContainers();", DestructorWait);
	const size_t DestructorBufferRelease = Source.find("DestroyAllBuffers();", DestructorContainerRelease);
	const size_t DestructorGpuRelease = Source.find("ReleaseGpuObjects();", DestructorBufferRelease);
	EXPECT_NE(ErroneousWait, std::string::npos);
	EXPECT_NE(ErroneousContainerRelease, std::string::npos);
	EXPECT_NE(ErroneousBufferRelease, std::string::npos);
	EXPECT_NE(ErroneousGpuRelease, std::string::npos);
	EXPECT_NE(DestructorWait, std::string::npos);
	EXPECT_NE(DestructorContainerRelease, std::string::npos);
	EXPECT_NE(DestructorBufferRelease, std::string::npos);
	EXPECT_NE(DestructorGpuRelease, std::string::npos);
	EXPECT_LT(ErroneousWait, ErroneousContainerRelease);
	EXPECT_LT(ErroneousContainerRelease, ErroneousBufferRelease);
	EXPECT_LT(ErroneousBufferRelease, ErroneousGpuRelease);
	EXPECT_LT(DestructorWait, DestructorContainerRelease);
	EXPECT_LT(DestructorContainerRelease, DestructorBufferRelease);
	EXPECT_LT(DestructorBufferRelease, DestructorGpuRelease);
	EXPECT_LT(Release, Wait);
	EXPECT_LT(Wait, ErroneousCleanup);
}
