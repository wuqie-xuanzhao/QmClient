// HUD media island 静态源码合同。运行时逻辑保留在 qm_hud_media_island_logic_test.cpp.
// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/graphics.h>

#include <game/client/components/hud_frozen_tee_state.h>
#include <game/client/components/hud_media_island_logic.h>
#include <game/client/components/tclient/pet.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace
{
	std::string FunctionBody(const std::string &Source, const std::string &Signature)
	{
		const size_t FunctionStart = Source.find(Signature);
		if(FunctionStart == std::string::npos)
			return {};
		const size_t BodyStart = Source.find('{', FunctionStart);
		if(BodyStart == std::string::npos)
			return {};
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}' && --Depth == 0)
				return Source.substr(BodyStart, Index - BodyStart);
		}
		return {};
	}

	SHudMediaIslandTrackInput Track(const char *pTitle, const char *pArtist = "", const char *pAlbum = "")
	{
		SHudMediaIslandTrackInput Input;
		Input.m_pTitle = pTitle;
		Input.m_pArtist = pArtist;
		Input.m_pAlbum = pAlbum;
		return Input;
	}

	EHudMediaIslandTrackUpdate ApplyTrack(
		SHudMediaIslandTrackSnapshot &Current,
		SHudMediaIslandTrackSnapshot &Outgoing,
		bool &HasIdentity,
		bool &TransitionActive,
		bool &NeedsNodeReset,
		int64_t &StartTick,
		int64_t Now,
		const SHudMediaIslandTrackInput &Input)
	{
		return QmHudMediaIslandUpdateTrackSnapshots(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, Now, Input);
	}
}


TEST(QmHudFrozenTeeSource, TracksConfirmedKillsWithoutTreatingDeathEffectsAsKills)
{
	const std::string CMakeSource = ReadTestSourceFile("CMakeLists.txt");
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const std::string GameClientHeader = ReadTestSourceFile("src/game/client/gameclient.h");
	const std::string HudSource = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string OnMessageBody = FunctionBody(GameClientSource, "void CGameClient::OnMessage(");
	const std::string ProcessEventsBody = FunctionBody(GameClientSource, "void CGameClient::ProcessEvents()");
	const std::string ResetDemoPlaybackStateBody = FunctionBody(GameClientSource, "void CGameClient::ResetDemoPlaybackState()");
	const std::string OnNewSnapshotBody = FunctionBody(GameClientSource, "void CGameClient::OnNewSnapshot(bool DummySwapped)");
	const std::string FrozenTeamInfoBody = FunctionBody(HudSource, "SHudFrozenTeamInfo BuildHudFrozenTeamInfo(");
	const std::string RenderTextInfoBody = FunctionBody(HudSource, "void CHud::RenderTextInfo()");

	EXPECT_NE(CMakeSource.find("components/hud_frozen_tee_state.h"), std::string::npos);
	EXPECT_NE(GameClientHeader.find("SHudFrozenTeeState m_HudFrozenTeeState"), std::string::npos);
	EXPECT_NE(OnMessageBody.find("QmHudMarkTeeDead(m_aClients[pMsg->m_Victim].m_HudFrozenTeeState"), std::string::npos);
	EXPECT_NE(OnMessageBody.find("QmHudMarkTeeDead(m_aClients[i].m_HudFrozenTeeState"), std::string::npos);
	EXPECT_EQ(ProcessEventsBody.find("QmHudMarkTeeDead"), std::string::npos);
	EXPECT_NE(ResetDemoPlaybackStateBody.find("Client.m_HudFrozenTeeState = {}"), std::string::npos);
	EXPECT_NE(OnNewSnapshotBody.find("QmHudObserveTeeCharacterSnapshot"), std::string::npos);
	EXPECT_NE(FrozenTeamInfoBody.find("QmHudTeeIsFrozen"), std::string::npos);
	EXPECT_NE(RenderTextInfoBody.find("QmHudTeeIsFrozen"), std::string::npos);
}






TEST(QmHudDummyMiniViewSource, VulkanUsesOffscreenTargetForEveryVendor)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string Header = ReadTestSourceFile("src/game/client/components/hud.h");
	const std::string VulkanSource = ReadTestSourceFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string RectBody = FunctionBody(Source, "bool CHud::GetDummyMiniMapRect(");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderDummyMiniMap(");
	const std::string ReleaseBody = FunctionBody(Source, "void CHud::OnRelease(");

	ASSERT_FALSE(RectBody.empty());
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(ReleaseBody.empty());
	EXPECT_EQ(Source.find("IsVulkanAmdBackend"), std::string::npos);
	EXPECT_EQ(Source.find("known driver crash"), std::string::npos);
	EXPECT_NE(Source.find("bool IsVulkanBackend(IGraphics *pGraphics)"), std::string::npos);
	EXPECT_NE(Source.find("GetDetectedContextVersion"), std::string::npos);
	EXPECT_NE(Header.find("m_DummyMiniViewRenderTarget"), std::string::npos);
	EXPECT_NE(Header.find("DestroyDummyMiniViewRenderTarget"), std::string::npos);
	EXPECT_EQ(RectBody.find("Vulkan"), std::string::npos);
	EXPECT_NE(RenderBody.find("IsVulkanBackend(Graphics())"), std::string::npos);
	EXPECT_NE(RenderBody.find("Graphics()->BeginRenderTarget(m_DummyMiniViewRenderTarget"), std::string::npos);
	EXPECT_NE(RenderBody.find("Graphics()->EndRenderTarget()"), std::string::npos);
	EXPECT_NE(RenderBody.find("Graphics()->DrawRenderTarget(m_DummyMiniViewRenderTarget"), std::string::npos);
	EXPECT_NE(RenderBody.find("DrawParams.m_V0 = 0.0f;"), std::string::npos);
	EXPECT_NE(RenderBody.find("DrawParams.m_V1 = 1.0f;"), std::string::npos);
	EXPECT_NE(ReleaseBody.find("DestroyDummyMiniViewRenderTarget"), std::string::npos);
	EXPECT_EQ(VulkanSource.find("VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT"), std::string::npos);
	EXPECT_NE(VulkanSource.find("MultiSamplingColorAttachment.storeOp = HasMultiSamplingTargets ? VK_ATTACHMENT_STORE_OP_STORE"), std::string::npos);
	EXPECT_NE(VulkanSource.find("LoadAttachments ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : InitialLayout"), std::string::npos);
}


TEST(QmHudSwitchCountdownSource, FollowRingsReuseMediaIslandSatelliteStyleWithoutIconsOrText)
{
	const std::string HudSource = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string PetSource = ReadTestSourceFile("src/game/client/components/tclient/pet.cpp");
	const std::string FollowBody = FunctionBody(HudSource, "void CHud::RenderFollowSwitchCountdowns()");
	const std::string SatelliteBody = FunctionBody(HudSource, "void DrawMediaIslandCountdownSatellite(");

	EXPECT_NE(PetSource.find("QmTClientPetAdvanceSpring"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmTClientPetAdvanceSpring"), std::string::npos);
	EXPECT_NE(FollowBody.find("GameClient()->m_Pet.IsVisibleForClient"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmHudSwitchCountdownFollowSide"), std::string::npos);
	EXPECT_NE(FollowBody.find("DrawMediaIslandCountdownSatellite"), std::string::npos);
	EXPECT_NE(FollowBody.find("constexpr float SatelliteRadius = 9.0f + 2.5f * 0.5f"), std::string::npos);
	EXPECT_NE(FollowBody.find("RingRadius = SatelliteRadius * MEDIA_ISLAND_SATELLITE_RING_RADIUS_SCALE"), std::string::npos);
	EXPECT_NE(FollowBody.find("SatelliteRadius * MEDIA_ISLAND_SATELLITE_RING_THICKNESS_SCALE"), std::string::npos);
	EXPECT_NE(FollowBody.find("g_Config.m_QmHudIslandBgColor"), std::string::npos);
	EXPECT_NE(FollowBody.find("g_Config.m_QmHudIslandBgOpacity"), std::string::npos);
	EXPECT_EQ(FollowBody.find("DrawMediaIslandArcGeometry"), std::string::npos);
	EXPECT_EQ(FollowBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_EQ(FollowBody.find("TextRender()"), std::string::npos);

	EXPECT_NE(SatelliteBody.find("SHudMediaIslandSdfRenderState"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("m_Radii = vec2()"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("MEDIA_ISLAND_OUTER_SHADOW_PIXELS"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("MEDIA_ISLAND_OUTER_SHADOW_OPACITY"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("QmHudMediaIslandBuildGpuSdfParams"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("RenderMediaIslandSdf"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("DrawMediaIslandGeometryFallback"), std::string::npos);
	EXPECT_EQ(SatelliteBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_EQ(SatelliteBody.find("TextRender()"), std::string::npos);
}


TEST(QmHudSwitchCountdownSource, ModesShareTrackingAndCanFeedBothSurfaces)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string UpdateBody = FunctionBody(Source, "void CHud::UpdateSwitchCountdownTracker()");
	const std::string HasIslandBody = FunctionBody(Source, "bool CHud::HasActiveSwitchCountdown() const");
	const std::string FollowBody = FunctionBody(Source, "void CHud::RenderFollowSwitchCountdowns()");
	const std::string OnRenderBody = FunctionBody(Source, "void CHud::OnRender()");
	const size_t IslandBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(IslandBegin, std::string::npos);
	const size_t IslandEnd = Source.find("void CHud::RenderPlayerState", IslandBegin);
	ASSERT_NE(IslandEnd, std::string::npos);
	const std::string IslandBody = Source.substr(IslandBegin, IslandEnd - IslandBegin);

	EXPECT_NE(UpdateBody.find("m_aaClientId[Team][SwitchNumber] = ClientId"), std::string::npos);
	EXPECT_NE(UpdateBody.find("m_aaConnection[Team][SwitchNumber] = Connection"), std::string::npos);
	EXPECT_NE(UpdateBody.find("QmHudSwitchCountdownShowsFollowTee"), std::string::npos);
	EXPECT_NE(HasIslandBody.find("QmHudSwitchCountdownShowsMediaIsland"), std::string::npos);
	EXPECT_NE(IslandBody.find("g_Config.m_QmSwitchCountdown"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudSwitchCountdownShowsMediaIsland"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("RenderFollowSwitchCountdowns();"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmHudSwitchCountdownShowsFollowTee"), std::string::npos);
}


TEST(QmHudMediaIslandSpectatorEye, OpenGlUsesTextIconWithoutChangingTheAtlasPath)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	ASSERT_FALSE(RenderBody.empty());

	EXPECT_NE(Source.find("bool IsOpenGlBackend()"), std::string::npos);
	EXPECT_NE(Source.find("str_comp_nocase(g_Config.m_GfxBackend, \"OpenGL\") == 0"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(IsOpenGlBackend())"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE_SLASH"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE"), std::string::npos);
	EXPECT_NE(RenderBody.find("else if(CQmIconManager *pIconManager = GameClient()->QmIconManager())"), std::string::npos);
}


TEST(QmHudMediaIslandSatellite, RenderPathUsesBlobSatellitesInsteadOfCountdownText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const std::string TClientSource = ReadTestSourceFile("src/game/client/components/tclient/tclient.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("void CHud::RenderPlayerState", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("RenderMediaIslandSdf"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudAdvanceMediaIslandLiquidProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobPose"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobBlend"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobConnectionStrength(BlobPose.m_Travel)"), std::string::npos);
	EXPECT_NE(RenderBody.find("mix(SpawnCenterX, FinalCenterX, BlobPose.m_Travel)"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSdfOuterRect(CurrentSdfState)"), std::string::npos);
	EXPECT_NE(RenderBody.find("TransformedScreenX1 - TransformedScreenX0"), std::string::npos);
	EXPECT_NE(RenderBody.find("TransformedScreenY1 - TransformedScreenY0"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandRipple"), std::string::npos);
	EXPECT_EQ(Source.find("m_MediaIslandRippleTexture"), std::string::npos);
	EXPECT_NE(RenderBody.find("const float SatelliteRadius = Radius;"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr float SatelliteItemGap = QmHudMediaIslandScaled(2.0f);"), std::string::npos);
	EXPECT_NE(RenderBody.find("aActiveSatelliteTargetCenters[i] = SatelliteCursorX + aActiveSatelliteTargetWidths[i] * 0.5f"), std::string::npos);
	EXPECT_NE(RenderBody.find("const CUIRect MainIslandSdfRect = EntrancePose.m_Rect;"), std::string::npos);
	EXPECT_EQ(RenderBody.find("QmHudMediaIslandMainCapRect"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandRightBlobCapsule"), std::string::npos);
	EXPECT_NE(RenderBody.find("EQmIcon::SATELLITE_SPECTATOR_EYE"), std::string::npos);
	EXPECT_NE(RenderBody.find("EQmIcon::SATELLITE_SPECTATOR_EYE_CLOSED"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSpectatorCountAlpha(ShowSpectator, SpectatorIconPose)"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSpectatorIconProgressDuringExit(AnimState.m_SpectatorExitIconStart, AnimState.m_SpectatorExitLiquidStart, AnimState.m_SpectatorLiquidProgress)"), std::string::npos);
	EXPECT_NE(Source.find("EQmIcon::SATELLITE_CHECK"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(IsOpenGlBackend())"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE"), std::string::npos);
	EXPECT_EQ(RenderBody.find("if(SatelliteRenderItemCount > 0)"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandLiquidBridge"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandProgressRing"), std::string::npos);
	EXPECT_EQ(RenderBody.find("Graphics()->DrawRect(IslandX, IslandY"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_NE(RenderBody.find("const ColorRGBA IconColor = Item.m_Completed ? ColorRGBA(0.20f, 1.0f, 0.42f"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon(Item.m_Type, Item.m_Completed, Item.m_SwapOutgoing)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandSwapVisibleForConnection(Dummy, g_Config.m_ClDummy)"), std::string::npos);
	EXPECT_NE(Source.find("Out.m_Outgoing = State.m_Outgoing"), std::string::npos);
	EXPECT_NE(Source.find("SwapOutgoing ? EQmIcon::SATELLITE_SWAP_OUTGOING : EQmIcon::SATELLITE_SWAP_INCOMING"), std::string::npos);
	EXPECT_NE(RenderBody.find("SdfItem.m_RingColor = MediaIslandCountdownColor(Item.m_Type);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("MediaIslandCountdownColor(Item.m_Type, Item.m_Completed)"), std::string::npos);
	EXPECT_NE(RenderBody.find("SatelliteVisibleLeft"), std::string::npos);
	EXPECT_EQ(RenderBody.find("BuildSwitchCountdownSummary"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_Hud.HandleSpamProtectionMessage(pMsg->m_pMessage);"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_TClient.HandleSwapCountdownMessage(pMsg->m_pMessage, Conn);"), std::string::npos);
	EXPECT_NE(TClientSource.find("m_aSwapCountdownTrackers[Dummy].Cancel"), std::string::npos);
}


TEST(QmHudMediaIslandSwapText, OnlyIncomingRequestsReplaceCheckpointAndExpandBeforeLyrics)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	const std::string BuildInfoBody = FunctionBody(Source, "bool BuildSwapCountdownInfo(");

	EXPECT_NE(BuildInfoBody.find("if(!Out.m_Outgoing)"), std::string::npos);
	EXPECT_NE(BuildInfoBody.find("Localize(\"%s has requested to swap with %s\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSwapRows(IncomingSwapCount, TimerCapsule.m_Visible, ShowLyricsIslandLine)"), std::string::npos);
	EXPECT_NE(RenderBody.find("TimerCapsule.m_BoxW = std::min(MaxTimerWidth"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(SwapRows.m_InlineSwapCount > 0)"), std::string::npos);
	EXPECT_NE(RenderBody.find("else if(Checkpoint > 0)"), std::string::npos);
	EXPECT_NE(RenderBody.find("LyricsTextY = BottomTextY + BottomRowLineHeight * SwapRows.m_LyricsLineIndex"), std::string::npos);
}


TEST(QmHudMediaIslandSatellite, CompletedSwapUsesCheckIconWithoutBottomReadyText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("void CHud::RenderPlayerState", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon(Item.m_Type, Item.m_Completed, Item.m_SwapOutgoing)"), std::string::npos);
	EXPECT_EQ(RenderBody.find("ShowSwapReady"), std::string::npos);
	EXPECT_EQ(RenderBody.find("SwapReadyCount"), std::string::npos);
	EXPECT_EQ(RenderBody.find("SwapBottomContentWidth"), std::string::npos);
	EXPECT_EQ(RenderBody.find("RenderBottomTextCentered(BottomTextY, SwapList"), std::string::npos);
}


TEST(QmHudMediaIslandTimerLayout, CheckpointOrSwapRaceTextDoesNotIntrudeIntoTopMargin)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");

	EXPECT_NE(RenderBody.find("const bool ShowTimerSecondaryLine = SwapRows.m_InlineSwapCount > 0 || Checkpoint > 0;"), std::string::npos);
	EXPECT_NE(RenderBody.find("ShowTimerSecondaryLine ? TimerRows.m_RaceY + (TimerRows.m_RaceH - TimerRaceFontSize) * 0.5f : TimerCapsule.m_TextY"), std::string::npos);
}
