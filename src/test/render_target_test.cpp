// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <engine/client/backend_sdl.h>
#include <engine/client/graphics_threaded.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <type_traits>

namespace
{
	using TBeginRenderTargetReadback = IGraphics::CRenderTargetReadbackHandle (IGraphics::*)(IGraphics::CRenderTargetHandle);
	using TPollRenderTargetReadback = IGraphics::ERenderTargetReadbackState (IGraphics::*)(IGraphics::CRenderTargetReadbackHandle);
	using TResolveRenderTargetReadback = bool (IGraphics::*)(IGraphics::CRenderTargetReadbackHandle *, CImageInfo &);
	using TCancelRenderTargetReadback = void (IGraphics::*)(IGraphics::CRenderTargetReadbackHandle *);

	std::string ReadFile(const char *pPath)
	{
		return ReadTestSourceFile(pPath);
	}

	std::string ExtractFunctionBody(const std::string &Source, const char *pSignature)
	{
		const size_t SignaturePos = Source.find(pSignature);
		EXPECT_NE(SignaturePos, std::string::npos) << pSignature;
		if(SignaturePos == std::string::npos)
			return {};

		const size_t BodyStart = Source.find('{', SignaturePos);
		EXPECT_NE(BodyStart, std::string::npos) << pSignature;
		if(BodyStart == std::string::npos)
			return {};

		int Depth = 1;
		size_t Pos = BodyStart + 1;
		for(; Pos < Source.size() && Depth > 0; ++Pos)
		{
			if(Source[Pos] == '{')
				++Depth;
			else if(Source[Pos] == '}')
				--Depth;
		}

		EXPECT_EQ(Depth, 0) << pSignature;
		if(Depth != 0 || Pos <= BodyStart + 1)
			return {};
		return Source.substr(BodyStart + 1, Pos - BodyStart - 2);
	}
} // namespace

static_assert(std::is_same_v<decltype(&IGraphics::BeginRenderTargetReadback), TBeginRenderTargetReadback>);
static_assert(std::is_same_v<decltype(&IGraphics::PollRenderTargetReadback), TPollRenderTargetReadback>);
static_assert(std::is_same_v<decltype(&IGraphics::ResolveRenderTargetReadback), TResolveRenderTargetReadback>);
static_assert(std::is_same_v<decltype(&IGraphics::CancelRenderTargetReadback), TCancelRenderTargetReadback>);

TEST(GraphicsRenderTarget, CommandStructsExposeExpectedFields)
{
	CCommandBuffer::SCommand_RenderTarget_Create Create;
	Create.m_TargetId = 7;
	Create.m_Width = 320;
	Create.m_Height = 180;
	EXPECT_EQ(Create.m_Cmd, CCommandBuffer::CMD_RENDER_TARGET_CREATE);
	EXPECT_EQ(Create.m_TargetId, 7);
	EXPECT_EQ(Create.m_Width, 320);
	EXPECT_EQ(Create.m_Height, 180);

	CCommandBuffer::SCommand_RenderTarget_Draw Draw;
	Draw.m_TargetId = 4;
	Draw.m_X = 1.0f;
	Draw.m_Y = 2.0f;
	Draw.m_W = 3.0f;
	Draw.m_H = 4.0f;
	EXPECT_EQ(Draw.m_Cmd, CCommandBuffer::CMD_RENDER_TARGET_DRAW);
	EXPECT_EQ(Draw.m_TargetId, 4);
	EXPECT_FLOAT_EQ(Draw.m_X, 1.0f);
	EXPECT_FLOAT_EQ(Draw.m_Y, 2.0f);
	EXPECT_FLOAT_EQ(Draw.m_W, 3.0f);
	EXPECT_FLOAT_EQ(Draw.m_H, 4.0f);
}

TEST(GraphicsRenderTarget, ReadbackCommandStructExposesExpectedFields)
{
	CCommandBuffer::SCommand_RenderTarget_Readback Readback;
	Readback.m_TargetId = 3;
	Readback.m_pImage = nullptr;
	EXPECT_EQ(Readback.m_Cmd, CCommandBuffer::CMD_RENDER_TARGET_READBACK);
	EXPECT_EQ(Readback.m_TargetId, 3);
	EXPECT_EQ(Readback.m_pImage, nullptr);
}

TEST(GraphicsRenderTarget, ReadbackContractExposesExpectedTypes)
{
	IGraphics::CRenderTargetReadbackHandle Handle;
	EXPECT_FALSE(Handle.IsValid());
	EXPECT_EQ(Handle.Id(), -1);
	EXPECT_EQ(Handle.Generation(), 0u);
	EXPECT_EQ(IGraphics::ERenderTargetReadbackState::INVALID, IGraphics::ERenderTargetReadbackState::INVALID);
	EXPECT_EQ(IGraphics::ERenderTargetReadbackState::PENDING, IGraphics::ERenderTargetReadbackState::PENDING);
	EXPECT_EQ(IGraphics::ERenderTargetReadbackState::READY, IGraphics::ERenderTargetReadbackState::READY);
	EXPECT_EQ(IGraphics::ERenderTargetReadbackState::FAILED, IGraphics::ERenderTargetReadbackState::FAILED);
}

TEST(GraphicsRenderTarget, BeginCommandDoesNotInheritCurrentClip)
{
	const std::string Source = ReadFile("src/engine/client/graphics_threaded.cpp");
	const std::string Body = ExtractFunctionBody(Source, "bool CGraphics_Threaded::BeginRenderTarget");
	ASSERT_FALSE(Body.empty());

	const size_t AddCmd = Body.find("AddCmd(Cmd);");
	const size_t DisableClip = Body.find("Cmd.m_State.m_ClipEnable = false;");
	ASSERT_NE(AddCmd, std::string::npos);
	ASSERT_NE(DisableClip, std::string::npos);
	EXPECT_LT(DisableClip, AddCmd);
}

TEST(GraphicsRenderTarget, AsyncBeginReadbackDoesNotWaitForIdle)
{
	const std::string Source = ReadFile("src/engine/client/graphics_threaded.cpp");
	const std::string Body = ExtractFunctionBody(Source, "IGraphics::CRenderTargetReadbackHandle CGraphics_Threaded::BeginRenderTargetReadback");
	ASSERT_FALSE(Body.empty());
	EXPECT_EQ(Body.find("WaitForIdle();"), std::string::npos);
	EXPECT_NE(Body.find("KickCommandBuffer();"), std::string::npos);
	EXPECT_NE(Body.find("CCommandBuffer::SCommand_Signal"), std::string::npos);
}

TEST(GraphicsRenderTarget, SyncReadRenderTargetUsesAsyncContract)
{
	const std::string Source = ReadFile("src/engine/client/graphics_threaded.cpp");
	const std::string Body = ExtractFunctionBody(Source, "bool CGraphics_Threaded::ReadRenderTarget");
	ASSERT_FALSE(Body.empty());
	EXPECT_NE(Body.find("BeginRenderTargetReadback"), std::string::npos);
	EXPECT_NE(Body.find("PollRenderTargetReadback"), std::string::npos);
	EXPECT_NE(Body.find("ResolveRenderTargetReadback"), std::string::npos);
	EXPECT_NE(Body.find("CancelRenderTargetReadback"), std::string::npos);
}

TEST(GraphicsRenderTarget, ResolveAndCancelDefendInvalidHandles)
{
	const std::string Source = ReadFile("src/engine/client/graphics_threaded.cpp");
	const std::string ResolveBody = ExtractFunctionBody(Source, "bool CGraphics_Threaded::ResolveRenderTargetReadback");
	const std::string CancelBody = ExtractFunctionBody(Source, "void CGraphics_Threaded::CancelRenderTargetReadback");
	ASSERT_FALSE(ResolveBody.empty());
	ASSERT_FALSE(CancelBody.empty());
	EXPECT_NE(ResolveBody.find("pHandle == nullptr"), std::string::npos);
	EXPECT_NE(ResolveBody.find("!pHandle->IsValid()"), std::string::npos);
	EXPECT_NE(CancelBody.find("pHandle == nullptr"), std::string::npos);
	EXPECT_NE(CancelBody.find("!pHandle->IsValid()"), std::string::npos);
}

TEST(GraphicsRenderTarget, BackendCapabilitiesDefaultToNoRenderTarget)
{
	SBackendCapabilities Capabilities{};
	EXPECT_FALSE(Capabilities.m_RenderTargets);
	EXPECT_STREQ(Capabilities.m_pRenderTargetSupportReason, "not_initialized");
}

TEST(GraphicsRenderTarget, VulkanBackendDeclaresRenderTargetSupport)
{
	const std::string Source = ReadFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const size_t MultiSamplingInit = Source.find("m_MultiSamplingCount = (g_Config.m_GfxFsaaSamples & 0xFFFFFFFE)");
	const size_t InitVulkan = Source.find("InitVulkan<true>()");
	const size_t RenderTargetsCapability = Source.find("m_RenderTargets = SupportsRenderTargetReadback()");
	ASSERT_NE(MultiSamplingInit, std::string::npos);
	ASSERT_NE(InitVulkan, std::string::npos);
	ASSERT_NE(RenderTargetsCapability, std::string::npos);
	EXPECT_LT(MultiSamplingInit, RenderTargetsCapability);
	EXPECT_LT(InitVulkan, RenderTargetsCapability);
	EXPECT_NE(Source.find("m_VKRenderTargetRenderPass != VK_NULL_HANDLE"), std::string::npos);
	EXPECT_NE(Source.find("RenderTargetReadbackSupportReason()"), std::string::npos);
	EXPECT_NE(Source.find("RenderTargetReadbackFormat()"), std::string::npos);
	EXPECT_NE(Source.find("VK_FORMAT_R8G8B8A8_UNORM"), std::string::npos);
	EXPECT_NE(Source.find("SubmitCurrentCommandsAndRestartSwapPass()"), std::string::npos);
	EXPECT_NE(Source.find("m_OptimalSwapChainImageBlitting && m_OptimalRGBAImageBlitting && m_LinearRGBAImageBlitting"), std::string::npos);
}

TEST(GraphicsRenderTarget, VulkanSwapRenderPassUsesInlineAfterForcedSingleThreadedRecording)
{
	const std::string Source = ReadFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string Body = ExtractFunctionBody(Source, "void BeginSwapRenderPass");
	ASSERT_FALSE(Body.empty());

	const size_t SubpassContents = Body.find("SubpassContents");
	const size_t ForceSingleThreaded = Body.find("m_ForceSingleThreadedRender");
	const size_t BeginRenderPass = Body.find("vkCmdBeginRenderPass");
	ASSERT_NE(SubpassContents, std::string::npos);
	ASSERT_NE(ForceSingleThreaded, std::string::npos);
	ASSERT_NE(BeginRenderPass, std::string::npos);
	EXPECT_LT(SubpassContents, BeginRenderPass);
	EXPECT_LT(ForceSingleThreaded, BeginRenderPass);
	EXPECT_NE(Body.find("VK_SUBPASS_CONTENTS_INLINE"), std::string::npos);
	EXPECT_NE(Body.find("VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS"), std::string::npos);
}

TEST(GraphicsRenderTarget, VulkanIntermediateSwapPassSubmitUsesFenceInsteadOfQueueIdle)
{
	const std::string Source = ReadFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string Body = ExtractFunctionBody(Source, "[[nodiscard]] bool SubmitCurrentCommandsAndRestartSwapPass()");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("vkResetFences"), std::string::npos);
	EXPECT_NE(Body.find("QueueSubmit("), std::string::npos);
	EXPECT_NE(Body.find("WaitForFences("), std::string::npos);
	EXPECT_EQ(Body.find("vkQueueSubmit("), std::string::npos);
	EXPECT_EQ(Body.find("vkQueueWaitIdle("), std::string::npos);
}

TEST(GraphicsRenderTarget, VulkanRenderTargetReadbackUsesFenceInsteadOfQueueIdle)
{
	const std::string Source = ReadFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string Body = ExtractFunctionBody(Source, "[[nodiscard]] bool Cmd_RenderTarget_Readback");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("vkResetFences"), std::string::npos);
	EXPECT_NE(Body.find("QueueSubmit("), std::string::npos);
	EXPECT_NE(Body.find("WaitForFences("), std::string::npos);
	EXPECT_NE(Body.find("InvalidateMappedMemoryRanges("), std::string::npos);
	EXPECT_EQ(Body.find("vkQueueSubmit("), std::string::npos);
	EXPECT_EQ(Body.find("vkQueueWaitIdle("), std::string::npos);
	EXPECT_EQ(Body.find("vkInvalidateMappedMemoryRanges("), std::string::npos);
}

TEST(GraphicsRenderTarget, VulkanPreviewReadbackDoesNotDependOnSwapchainMsaa)
{
	const std::string Source = ReadFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string SupportBody = ExtractFunctionBody(Source, "[[nodiscard]] bool SupportsRenderTargetReadback() const");
	const std::string CreateBody = ExtractFunctionBody(Source, "[[nodiscard]] bool Cmd_RenderTarget_Create");
	ASSERT_FALSE(SupportBody.empty());
	ASSERT_FALSE(CreateBody.empty());
	EXPECT_EQ(SupportBody.find("!HasMultiSampling()"), std::string::npos);
	EXPECT_EQ(CreateBody.find("HasMultiSampling() ||"), std::string::npos);
}
