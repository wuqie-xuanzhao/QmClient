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
	EXPECT_NE(Source.find("WaitForGpuIdle();\n\t\tReleaseGpuObjects();", ErroneousCleanup), std::string::npos);
	EXPECT_NE(Source.find("WaitForGpuIdle();\n\t\tReleaseGpuObjects();", Destructor), std::string::npos);
	EXPECT_LT(Release, Wait);
	EXPECT_LT(Wait, ErroneousCleanup);
}
