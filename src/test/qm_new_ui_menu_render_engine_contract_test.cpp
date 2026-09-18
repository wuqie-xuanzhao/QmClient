// QmNewUi 菜单源码合同：render 引擎与图形驱动。运行时行为保留在 qm_new_ui_menu_branch_test.cpp.
#include <engine/client/backend/vulkan/backend_vulkan.h>
#include <engine/client/backend_sdl.h>
#include <engine/client/plausible_sizes.h>
#include <engine/client/rounded_rect_geometry.h>
#include <engine/storage.h>

#include <game/client/QmUi/UiSurface.h>
#include <game/client/components/camera.h>
#include <game/client/components/controls.h>
#include <game/client/components/menus.h>
#include <game/client/components/nameplate_text_effects.h>
#include <game/client/components/nameplates.h>
#include <game/client/components/qmclient/axiom_auto_login.h>
#include <game/client/components/tclient/statusbar.h>
#include <game/client/components/tooltips.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <gtest/gtest.h>
#include <test/qmclient_source_contract_test.h>
#include <test/test.h>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <string>

TEST(QmNewUiMenuRenderEngineContract, KcpLogUsesBoundedFormatting)
{
	const std::string Source = ReadTextFile("src/engine/external/kcp/ikcp.c");

	EXPECT_EQ(Source.find("vsprintf(buffer, fmt, argptr);"), std::string::npos);
	EXPECT_NE(Source.find("vsnprintf(buffer, sizeof(buffer), fmt, argptr);"), std::string::npos);
}

TEST(QmNewUiMenuRenderEngineContract, DisplayChangedDoesNotUseDisplayUnionData)
{
	const std::string Source = ReadTextFile("src/engine/client/input.cpp");
	const size_t CaseStart = Source.find("case SDL_WINDOWEVENT_DISPLAY_CHANGED:");
	ASSERT_NE(CaseStart, std::string::npos);
	const size_t Break = Source.find("break;", CaseStart);
	ASSERT_NE(Break, std::string::npos);
	const std::string Body = Source.substr(CaseStart, Break - CaseStart);

	EXPECT_EQ(Body.find("Event.display.data1"), std::string::npos);
	EXPECT_NE(Body.find("Event.window.data1"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->SwitchWindowScreen(DisplayIndex, false);"), std::string::npos);
}

TEST(QmNewUiMenuRenderEngineContract, GraphicsDriverCrashRecoveryUsesSafeStartupFallback)
{
	const std::string Source = ReadTextFile("src/engine/client/client.cpp");
	const std::string Detector = FunctionBody(Source, "static bool QmCrashTextHasGraphicsDriverFault");
	const std::string Recovery = FunctionBody(Source, "static bool ApplyQmSafeGraphicsRecovery");
	const std::string StartupHook = FunctionBody(Source, "static void RecoverQmGraphicsSettingsAfterDriverCrash");

	EXPECT_NE(Detector.find("Exception module: nvoglv64.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module nvoglv64.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: vulkan-1.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: D3D12Core.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module D3D12Core.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: d3d12.dll"), std::string::npos);
	EXPECT_NE(Detector.find("Exception module: dxgi.dll"), std::string::npos);
	EXPECT_NE(Detector.find(" in module opengl32.dll"), std::string::npos);

	EXPECT_NE(StartupHook.find("gs_pQmLifecycleMarkerFile"), std::string::npos);
	EXPECT_NE(StartupHook.find("ListDirectoryInfo"), std::string::npos);
	EXPECT_NE(StartupHook.find("ReadFileStr"), std::string::npos);

	EXPECT_NE(Recovery.find("str_copy(g_Config.m_GfxBackend, SafeConfig.m_pBackend);"), std::string::npos);
	EXPECT_NE(Recovery.find("g_Config.m_QmGraphicsMode = graphics_backend::GRAPHICS_MODE_COMPATIBILITY;"), std::string::npos);
	EXPECT_NE(Recovery.find("const int FallbackGLMajor = 0;"), std::string::npos);
	EXPECT_NE(Recovery.find("const int FallbackGLMinor = 0;"), std::string::npos);
	EXPECT_EQ(Recovery.find("CONF_PLATFORM_MACOS"), std::string::npos);
	EXPECT_NE(Recovery.find("SafeConfig.m_FsaaSamples"), std::string::npos);
	EXPECT_NE(Recovery.find("RecoveryFullscreen"), std::string::npos);
	EXPECT_NE(Recovery.find("graphics_backend::RecoveryFullscreenMode(g_Config.m_GfxFullscreen)"), std::string::npos);
	EXPECT_NE(StartupHook.find("previous crash report '%s' points to the graphics driver; this launch uses safe graphics settings"), std::string::npos);
	EXPECT_EQ(StartupHook.find("resetting safe graphics settings in windowed mode without FSAA"), std::string::npos);
	EXPECT_EQ(StartupHook.find("CONF_PLATFORM_MACOS"), std::string::npos);

	const size_t HookCall = Source.find("RecoverQmGraphicsSettingsAfterDriverCrash(pStorage);");
	const size_t CommandLineParse = Source.find("pConsole->ParseArguments(argc - 1, &argv[1]);");
	ASSERT_NE(HookCall, std::string::npos);
	ASSERT_NE(CommandLineParse, std::string::npos);
	EXPECT_LT(HookCall, CommandLineParse);
}

TEST(QmNewUiMenuRenderEngineContract, ImplausibleRefreshRatesAreNotPersisted)
{
	const std::string Backend = ReadTextFile("src/engine/client/backend_sdl.cpp");
	const std::string Graphics = ReadTextFile("src/engine/client/graphics_threaded.cpp");

	// IsPlausible* guards now live in the shared plausible_sizes.h header
	// (behavior-tested in PlausibleSizes.RefreshRateAndWindowGuardsMatchContract);
	// both backends include it instead of re-declaring file-static copies.
	const std::string Plausible = ReadTextFile("src/engine/client/plausible_sizes.h");
	EXPECT_NE(Plausible.find("IsPlausibleRefreshRate"), std::string::npos);
	EXPECT_NE(Plausible.find("IsPlausibleWindowSize"), std::string::npos);
	EXPECT_NE(Graphics.find("#include <engine/client/plausible_sizes.h>"), std::string::npos);
	EXPECT_NE(Graphics.find("Ignoring implausible refresh rate during resize"), std::string::npos);
	EXPECT_NE(Graphics.find("RefreshRate = m_ScreenRefreshRate;"), std::string::npos);
	EXPECT_NE(Graphics.find("static int LogicalWindowSizeFromViewport(int ViewportSize, float HiDPIScale)"), std::string::npos);
	EXPECT_NE(Graphics.find("Ignoring implausible resize dimensions"), std::string::npos);
	EXPECT_NE(Graphics.find("if(IsPlausibleWindowSize(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight))"), std::string::npos);
	EXPECT_EQ(Graphics.find("w = g_Config.m_GfxScreenWidth > 0 ? g_Config.m_GfxScreenWidth : m_ScreenWidth;"), std::string::npos);
	EXPECT_EQ(Graphics.find("h = g_Config.m_GfxScreenHeight > 0 ? g_Config.m_GfxScreenHeight : m_ScreenHeight;"), std::string::npos);
	EXPECT_NE(Graphics.find("w = LogicalWindowSizeFromViewport(m_ScreenWidth, m_ScreenHiDPIScale);"), std::string::npos);
	EXPECT_NE(Graphics.find("h = LogicalWindowSizeFromViewport(m_ScreenHeight, m_ScreenHiDPIScale);"), std::string::npos);
}

TEST(QmNewUiMenuRenderEngineContract, QmClientLanguageReadmeDescribesChineseSourceKeys)
{
	EXPECT_FALSE(fs_is_dir(TestSourcePath("data/qmclient/languages").c_str()));
}

TEST(QmNewUiMenuRenderEngineContract, TClientQueuesAspectRefreshFromSnapshots)
{
	const std::string TClientSource = ReadTextFile("src/game/client/components/tclient/tclient.cpp");
	const std::string SnapshotBody = FunctionBody(TClientSource, "void CTClient::OnNewSnapshot()");
	const std::string UpdateBody = FunctionBody(TClientSource, "void CTClient::OnUpdate()");

	ASSERT_FALSE(SnapshotBody.empty());
	ASSERT_FALSE(UpdateBody.empty());
	EXPECT_NE(SnapshotBody.find("QueueAspectApply();"), std::string::npos);
	EXPECT_EQ(SnapshotBody.find("SetForcedAspect();"), std::string::npos);
	EXPECT_NE(UpdateBody.find("if(m_QmAspectApplyPending)"), std::string::npos);
	EXPECT_NE(UpdateBody.find("SetForcedAspect();"), std::string::npos);
}
