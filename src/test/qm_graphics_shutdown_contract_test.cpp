#include "qmclient_source_contract_test.h"

#include <gtest/gtest.h>

TEST(QmGraphicsShutdownContract, ClearsStickyFatalBeforeDeletingBackend)
{
	const std::string Source = ReadRepoFile("src/engine/client/backend_sdl.cpp");
	const size_t ShutdownStart = Source.find("int CGraphicsBackend_SDL_GL::Shutdown()");
	ASSERT_NE(ShutdownStart, std::string::npos);
	const size_t ShutdownEnd = Source.find("uint64_t CGraphicsBackend_SDL_GL::TextureMemoryUsage", ShutdownStart);
	ASSERT_NE(ShutdownEnd, std::string::npos);
	const std::string Shutdown = Source.substr(ShutdownStart, ShutdownEnd - ShutdownStart);
	const std::string Graphics = ReadRepoFile("src/engine/client/graphics_threaded.cpp");
	const std::string ThreadedShutdown = ExtractSourceFunctionBody(Graphics, "void CGraphics_Threaded::Shutdown()");
	EXPECT_NE(ThreadedShutdown.find("if(m_pBackend == nullptr)"), std::string::npos);
	EXPECT_NE(ThreadedShutdown.find("m_pBackend->Shutdown();"), std::string::npos);
	EXPECT_NE(ThreadedShutdown.find("delete m_pBackend;"), std::string::npos);
	EXPECT_LT(ThreadedShutdown.find("m_pBackend->Shutdown();"), ThreadedShutdown.find("delete m_pBackend;"));
}

TEST(QmGraphicsShutdownContract, FestiveCrashDialogOffersNonDestructiveFireworks)
{
	const std::string FestiveDialog = ReadRepoFile("src/engine/client/qm_festive_message_box.cpp");
	EXPECT_NE(FestiveDialog.find("m_FireworksFrame = 0"), std::string::npos);
	EXPECT_NE(FestiveDialog.find("SetTimer(State.m_Window, gs_FireworksTimerId"), std::string::npos);
	EXPECT_NE(FestiveDialog.find("if(WParam == gs_FireworksTimerId && pState->m_FireworksFrame >= 0)"), std::string::npos);
	EXPECT_NE(FestiveDialog.find("InvalidateFireworksRegion(Window, *pState"), std::string::npos);
	EXPECT_NE(FestiveDialog.find("KillTimer(Window, gs_FireworksTimerId)"), std::string::npos);
}

TEST(QmGraphicsShutdownContract, GraphicsFatalMessageBoxAvoidsWindowDestruction)
{
	const std::string ThreadedBackend = ReadRepoFile("src/engine/client/backend_threaded.cpp");
	const std::string ProcessError = ExtractSourceFunctionBody(ThreadedBackend, "void CGraphicsBackend_Threaded::ProcessError(const SGfxErrorContainer &Error)");
	ASSERT_NE(ProcessError.find("m_FatalErrorPending.store(true"), std::string::npos);
	EXPECT_LT(ProcessError.find("m_FatalErrorPending.store(true"), ProcessError.find("dbg_assert_failed"));

	const std::string SdlBackend = ReadRepoFile("src/engine/client/backend_sdl.cpp");
	const std::string ShowMessageBox = ExtractSourceFunctionBody(SdlBackend, "std::optional<int> CGraphicsBackend_SDL_GL::ShowMessageBox(const IGraphics::CMessageBox &MessageBox)");
	ASSERT_NE(ShowMessageBox.find("if(HasFatalError())"), std::string::npos);
	const size_t SafeMessageBoxCall = ShowMessageBox.find("ShowMessageBoxWithoutGraphics(MessageBox)");
	const size_t WindowDestructionCall = ShowMessageBox.find("SDL_DestroyWindow(m_pWindow)");
	ASSERT_NE(SafeMessageBoxCall, std::string::npos);
	ASSERT_NE(WindowDestructionCall, std::string::npos);
	EXPECT_LT(SafeMessageBoxCall, WindowDestructionCall);
}
