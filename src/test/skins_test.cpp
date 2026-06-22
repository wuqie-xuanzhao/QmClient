#include <engine/gfx/image_loader.h>

#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/components/skins.h>
#include <game/client/render.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cstdlib>
#include <fstream>
#include <list>
#include <sstream>

extern CDataContainer *g_pData;

static vec2 ComputeRenderedTeeMid(const CTeeRenderInfo &Info)
{
	const CAnimState *pIdle = CAnimState::GetIdle();
	float AnimScale, BaseSize;
	CRenderTools::GetRenderTeeAnimScaleAndBaseSize(&Info, AnimScale, BaseSize);
	const vec2 BodyPos = vec2(pIdle->GetBody()->m_X, pIdle->GetBody()->m_Y) * AnimScale;
	const float AssumedScale = BaseSize / 64.0f;
	vec2 BodyOffset;
	float BodyWidth, BodyHeight;
	CRenderTools::GetRenderTeeBodySize(pIdle, &Info, BodyOffset, BodyWidth, BodyHeight);
	vec2 FeetOffset;
	float FeetWidth, FeetHeight;
	CRenderTools::GetRenderTeeFeetSize(pIdle, &Info, FeetOffset, FeetWidth, FeetHeight);
	const vec2 FeetPos[2] = {
		vec2(pIdle->GetFrontFoot()->m_X, pIdle->GetFrontFoot()->m_Y) * AnimScale,
		vec2(pIdle->GetBackFoot()->m_X, pIdle->GetBackFoot()->m_Y) * AnimScale,
	};
	float MinX = -32.0f * AssumedScale + BodyPos.x + BodyOffset.x;
	float MaxX = MinX + BodyWidth;
	for(const vec2 &FootPos : FeetPos)
	{
		const float FootMinX = -32.0f * AssumedScale + FootPos.x + FeetOffset.x;
		MinX = minimum(MinX, FootMinX);
		MaxX = maximum(MaxX, FootMinX + FeetWidth);
	}
	float MinY = -32.0f * AssumedScale + BodyPos.y + BodyOffset.y;
	float MaxY = MinY + BodyHeight;
	for(const vec2 &FootPos : FeetPos)
	{
		MaxY = maximum(MaxY, -16.0f * AssumedScale + FootPos.y + FeetOffset.y + FeetHeight);
	}
	return vec2(MinX + (MaxX - MinX) / 2.0f, MinY + (MaxY - MinY) / 2.0f);
}

static void SetBeastLikeMetrics(CTeeRenderInfo &Info)
{
	Info.m_Size = 64.0f;
	Info.m_SkinMetrics.m_Body.m_Width = 80;
	Info.m_SkinMetrics.m_Body.m_Height = 82;
	Info.m_SkinMetrics.m_Body.m_OffsetX = 16;
	Info.m_SkinMetrics.m_Body.m_OffsetY = 14;
	Info.m_SkinMetrics.m_Body.m_MaxWidth = 96;
	Info.m_SkinMetrics.m_Body.m_MaxHeight = 96;
	Info.m_SkinMetrics.m_Feet.m_Width = 44;
	Info.m_SkinMetrics.m_Feet.m_Height = 23;
	Info.m_SkinMetrics.m_Feet.m_OffsetX = 20;
	Info.m_SkinMetrics.m_Feet.m_OffsetY = 9;
	Info.m_SkinMetrics.m_Feet.m_MaxWidth = 64;
	Info.m_SkinMetrics.m_Feet.m_MaxHeight = 32;
}

static void SetTestPixel(CImageInfo &Image, size_t x, size_t y, uint8_t Red, uint8_t Green, uint8_t Blue, uint8_t Alpha)
{
	const size_t Offset = (y * Image.m_Width + x) * Image.PixelSize();
	Image.m_pData[Offset] = Red;
	Image.m_pData[Offset + 1] = Green;
	Image.m_pData[Offset + 2] = Blue;
	Image.m_pData[Offset + 3] = Alpha;
}

static CImageInfo MakeTestSkinImage(size_t Width, size_t Height, CImageInfo::EImageFormat Format = CImageInfo::FORMAT_RGBA)
{
	CImageInfo Image;
	Image.m_Width = Width;
	Image.m_Height = Height;
	Image.m_Format = Format;
	Image.m_pData = static_cast<uint8_t *>(calloc(Image.DataSize(), 1));
	return Image;
}

TEST(Skins, UsageTrackingSkipsAlwaysLoadedStates)
{
	using EState = CSkins::CSkinContainer::EState;

	EXPECT_FALSE(CSkins::CSkinContainer::TracksUsage(EState::PENDING, true));
	EXPECT_FALSE(CSkins::CSkinContainer::TracksUsage(EState::LOADING, true));
	EXPECT_FALSE(CSkins::CSkinContainer::TracksUsage(EState::LOADED, true));

	EXPECT_TRUE(CSkins::CSkinContainer::TracksUsage(EState::PENDING, false));
	EXPECT_TRUE(CSkins::CSkinContainer::TracksUsage(EState::LOADING, false));
	EXPECT_TRUE(CSkins::CSkinContainer::TracksUsage(EState::LOADED, false));
	EXPECT_FALSE(CSkins::CSkinContainer::TracksUsage(EState::UNLOADED, false));
}

TEST(Skins, AlwaysLoadedStateTransitionsNeverTouchUsageList)
{
	using EState = CSkins::CSkinContainer::EState;

	for(const EState State : {EState::PENDING, EState::LOADING, EState::LOADED})
	{
		const auto Clean = CSkins::CSkinContainer::UsageTrackingUpdate(State, true, false);
		EXPECT_FALSE(Clean.m_ShouldTouch);
		EXPECT_FALSE(Clean.m_ShouldErase);

		const auto Polluted = CSkins::CSkinContainer::UsageTrackingUpdate(State, true, true);
		EXPECT_FALSE(Polluted.m_ShouldTouch);
		EXPECT_TRUE(Polluted.m_ShouldErase);
	}
}

TEST(Skins, UsageListEntriesThatCannotBeUnloadedAreDiscarded)
{
	using EState = CSkins::CSkinContainer::EState;

	EXPECT_TRUE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(false, EState::LOADED, false));
	EXPECT_TRUE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(true, EState::LOADED, true));
	EXPECT_TRUE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(true, EState::NOT_FOUND, false));
	EXPECT_FALSE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(true, EState::PENDING, false));
	EXPECT_FALSE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(true, EState::LOADING, false));
	EXPECT_FALSE(CSkins::CSkinContainer::ShouldDiscardUsageEntryBeforeUnload(true, EState::LOADED, false));
}

TEST(Skins, OnlyNotFoundTransitionsRequireSkinListRefresh)
{
	using EState = CSkins::CSkinContainer::EState;

	EXPECT_FALSE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::UNLOADED, EState::PENDING));
	EXPECT_FALSE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::PENDING, EState::LOADING));
	EXPECT_FALSE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::LOADING, EState::LOADED));
	EXPECT_FALSE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::ERROR, EState::UNLOADED));

	EXPECT_TRUE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::LOADING, EState::NOT_FOUND));
	EXPECT_TRUE(CSkins::CSkinContainer::StateChangeRequiresListRefresh(EState::NOT_FOUND, EState::PENDING));
}

TEST(Skins, RegularStateTransitionsEnterAndLeaveUsageList)
{
	using EState = CSkins::CSkinContainer::EState;

	for(const EState State : {EState::PENDING, EState::LOADING, EState::LOADED})
	{
		const auto MissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(State, false, false);
		EXPECT_TRUE(MissingEntry.m_ShouldTouch);
		EXPECT_FALSE(MissingEntry.m_ShouldErase);

		const auto ExistingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(State, false, true);
		EXPECT_FALSE(ExistingEntry.m_ShouldTouch);
		EXPECT_FALSE(ExistingEntry.m_ShouldErase);
	}

	for(const EState State : {EState::UNLOADED, EState::ERROR, EState::NOT_FOUND})
	{
		const auto ExistingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(State, false, true);
		EXPECT_FALSE(ExistingEntry.m_ShouldTouch);
		EXPECT_TRUE(ExistingEntry.m_ShouldErase);
	}
}

TEST(Skins, ImmediateRequestLoadShouldTouchUsageTrackingData)
{
	using EState = CSkins::CSkinContainer::EState;
	const auto MissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::PENDING, false, false);
	EXPECT_TRUE(MissingEntry.m_ShouldTouch);
	EXPECT_FALSE(MissingEntry.m_ShouldErase);
}

TEST(Skins, BackgroundRequestDoesNotTouchPriorityUsageTrackingData)
{
	using EState = CSkins::CSkinContainer::EState;
	const auto MissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::PENDING, false, false, ESettingsResourcePriority::BACKGROUND);
	EXPECT_FALSE(MissingEntry.m_ShouldTouch);
	EXPECT_FALSE(MissingEntry.m_ShouldErase);

	const auto ExistingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::LOADED, false, true, ESettingsResourcePriority::BACKGROUND);
	EXPECT_FALSE(ExistingEntry.m_ShouldTouch);
	EXPECT_TRUE(ExistingEntry.m_ShouldErase);
}

TEST(Skins, HighPriorityBackgroundRequestedTouchesUsageTrackingData)
{
	using EState = CSkins::CSkinContainer::EState;

	const auto VisibleMissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::BACKGROUND_REQUESTED, false, false, ESettingsResourcePriority::VISIBLE);
	EXPECT_TRUE(VisibleMissingEntry.m_ShouldTouch);
	EXPECT_FALSE(VisibleMissingEntry.m_ShouldErase);

	const auto PrefetchMissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::BACKGROUND_REQUESTED, false, false, ESettingsResourcePriority::PREFETCH);
	EXPECT_TRUE(PrefetchMissingEntry.m_ShouldTouch);
	EXPECT_FALSE(PrefetchMissingEntry.m_ShouldErase);

	const auto BackgroundMissingEntry = CSkins::CSkinContainer::UsageTrackingUpdate(EState::BACKGROUND_REQUESTED, false, false, ESettingsResourcePriority::BACKGROUND);
	EXPECT_FALSE(BackgroundMissingEntry.m_ShouldTouch);
	EXPECT_FALSE(BackgroundMissingEntry.m_ShouldErase);
}

TEST(Skins, TeeBackgroundDrainSeparatesRequestedBacklogFromAdmittedQueue)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.h"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("BACKGROUND_REQUESTED"), std::string::npos);
	EXPECT_NE(Source.find("size_t m_NumBackgroundRequested = 0;"), std::string::npos);
	EXPECT_NE(Source.find("return !ExistsInSkinMap || (!TracksUsage(State, AlwaysLoaded) && State != EState::BACKGROUND_REQUESTED);"), std::string::npos);
}

TEST(Skins, LoadingStatsRealInflightExcludesBackgroundRequested)
{
	CSkins::CSkinLoadingStats Stats;
	Stats.m_NumBackgroundRequested = 999;
	Stats.m_NumPending = 7;
	Stats.m_NumLoading = 11;

	EXPECT_EQ(Stats.RealInflight(), 18u);
	EXPECT_FALSE(Stats.AdmissionInvariantViolated(18));
	EXPECT_TRUE(Stats.AdmissionInvariantViolated(17));
}

TEST(Skins, TeeBackgroundRequestsWaitForAdmissionBeforePending)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("if(m_State == EState::UNLOADED)\n\t\t\tSetState(EState::BACKGROUND_REQUESTED, ESettingsResourcePriority::BACKGROUND);"), std::string::npos);
	EXPECT_NE(Source.find("if(m_State == EState::UNLOADED || m_State == EState::BACKGROUND_REQUESTED)"), std::string::npos);
	const size_t StartLoadingPos = Source.find("void CSkins::UpdateStartLoading(CSkinLoadingStats &Stats)");
	ASSERT_NE(StartLoadingPos, std::string::npos);
	const size_t StartLoadingEnd = Source.find("CSkins::ESkinProcessResult CSkins::ProcessSkinContainer", StartLoadingPos);
	ASSERT_NE(StartLoadingEnd, std::string::npos);
	const std::string StartLoading = Source.substr(StartLoadingPos, StartLoadingEnd - StartLoadingPos);

	EXPECT_NE(StartLoading.find("if(Stats.m_NumPending == 0 && pSkinContainer->m_State != CSkinContainer::EState::BACKGROUND_REQUESTED)"), std::string::npos);
	EXPECT_NE(StartLoading.find("if(pSkinContainer->m_State == CSkinContainer::EState::BACKGROUND_REQUESTED)"), std::string::npos);
	EXPECT_NE(StartLoading.find("pSkinContainer->SetState(CSkinContainer::EState::PENDING, Admission.m_PromotePriority);"), std::string::npos);
	EXPECT_NE(StartLoading.find("Stats.m_NumBackgroundRequested--;"), std::string::npos);
	EXPECT_NE(StartLoading.find("Stats.m_NumPending++;"), std::string::npos);
}

TEST(Skins, TeeSkinListVirtualizationKeepsTotalListLength)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	const size_t RenderTeePos = Source.find("void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_NE(RenderTeePos, std::string::npos);
	const size_t RenderTeeEnd = Source.find("void CMenus::RenderSettings", RenderTeePos + 1);
	const std::string RenderTeeBody = Source.substr(RenderTeePos, RenderTeeEnd - RenderTeePos);

	EXPECT_NE(RenderTeeBody.find("s_ListBox.DoStart(50.0f, vSkinList.size(), 3, 2, OldSelected, &MainView);"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("SettingsSkinListVisibleRangeForScroll("), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("s_ListBox.SkipItems("), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("int RowsRendered = 0;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("if(RowStart)\n\t\t\t++RowsRendered;"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("const int RowsRendered = RowsIterated;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("event=list_frame page=settings:tee"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("rows_total=%d rows_visible=%d rows_rendered=%d rows_iterated=%d rows_skipped=%d"), std::string::npos);
}

TEST(Skins, TeeSkinListSortsByModifiedTimeAfterFavorites)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/skins.h");
	const std::string Source = ReadTestSourceFile("src/game/client/components/skins.cpp");
	const size_t ComparePos = Source.find("bool CSkins::CSkinListEntry::operator<");
	const size_t ScanJobPos = Source.find("int CSkins::CSkinDirectoryScanJob::ScanCallback");
	ASSERT_NE(ComparePos, std::string::npos);
	ASSERT_NE(ScanJobPos, std::string::npos);
	const std::string CompareBody = Source.substr(ComparePos, 1200);
	const std::string ScanJobBody = Source.substr(ScanJobPos, 900);

	EXPECT_NE(Header.find("time_t m_LastModified"), std::string::npos);
	EXPECT_NE(Header.find("time_t LastModified() const"), std::string::npos);
	EXPECT_NE(Source.find("ListDirectoryInfo(IStorage::TYPE_ALL, \"skins\""), std::string::npos);
	EXPECT_NE(ScanJobBody.find("pInfo->m_TimeModified"), std::string::npos);
	EXPECT_NE(CompareBody.find("LastModified()"), std::string::npos);
	EXPECT_NE(CompareBody.find("LastModified() > Other.m_pSkinContainer->LastModified()"), std::string::npos);
	const size_t FavoritePos = CompareBody.find("m_Favorite");
	const size_t ModifiedPos = CompareBody.find("LastModified()");
	ASSERT_NE(FavoritePos, std::string::npos);
	ASSERT_NE(ModifiedPos, std::string::npos);
	EXPECT_LT(FavoritePos, ModifiedPos);
}

TEST(Skins, TeeSkinListStableIdleAvoidsFullBackgroundScan)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	const size_t RenderTeePos = Source.find("void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_NE(RenderTeePos, std::string::npos);
	const size_t RenderTeeEnd = Source.find("void CMenus::RenderSettingsAppearance", RenderTeePos);
	ASSERT_NE(RenderTeeEnd, std::string::npos);
	const std::string RenderTeeBody = Source.substr(RenderTeePos, RenderTeeEnd - RenderTeePos);

	EXPECT_NE(Source.find("bool m_BackgroundRequestScanComplete = false;"), std::string::npos);
	EXPECT_NE(Source.find("int m_BackgroundRequestScanListSize = -1;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("VisibleSourceSettled && BackgroundRequestBudget > 0"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("!gs_TeeSettingsPageState.m_BackgroundRequestScanComplete"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("SkinStatsBeforeBackgroundRequest.m_NumUnloaded > 0"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("gs_TeeSettingsPageState.m_BackgroundRequestScanComplete = true;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("event=tee_skin_background_scan"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("items_total=%d items_scanned=%d items_skipped_visible=%d requests_issued=%d complete=%d budget=%d dur_ms=%.3f block_reason=%s"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("event=tee_skin_list_prescan"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("visual_ready_count=%d"), std::string::npos);
}

TEST(Skins, TeeSkinListSeparatesVisualReadyFromSourceSettled)
{
	std::ifstream JobsFile(TestSourcePath("src/game/client/components/settings_resource_jobs.cpp"));
	ASSERT_TRUE(JobsFile.good());
	std::stringstream JobsBuffer;
	JobsBuffer << JobsFile.rdbuf();
	const std::string JobsSource = JobsBuffer.str();

	EXPECT_NE(JobsSource.find("bool SettingsSkinListEntryVisualReady(bool SourceReady, bool TerminalFailure, bool PreviewCacheReady)"), std::string::npos);
	EXPECT_NE(JobsSource.find("bool SettingsSkinListEntrySourceSettled(bool SourceReady, bool TerminalFailure)"), std::string::npos);
	EXPECT_NE(JobsSource.find("return SourceReady || TerminalFailure || PreviewCacheReady;"), std::string::npos);
	EXPECT_NE(JobsSource.find("return SourceReady || TerminalFailure;"), std::string::npos);

	std::ifstream MenusFile(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();
	const size_t RenderTeePos = MenusSource.find("void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_NE(RenderTeePos, std::string::npos);
	const size_t RenderTeeEnd = MenusSource.find("void CMenus::RenderSettingsAppearance", RenderTeePos);
	ASSERT_NE(RenderTeeEnd, std::string::npos);
	const std::string RenderTeeBody = MenusSource.substr(RenderTeePos, RenderTeeEnd - RenderTeePos);

	EXPECT_NE(RenderTeeBody.find("VisibleVisualReadyCount"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("VisibleSourceSettledCount"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const bool VisibleSourceSettled = VisibleSourceSettledCount == (int)vVisibleSkinIndices.size();"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("m_SettingsHighPrioritySettled = VisibleSourceSettled;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("FrameContext.m_HighPrioritySettled = VisibleSourceSettled;"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("m_SettingsHighPrioritySettled = VisibleSettled;"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("FrameContext.m_HighPrioritySettled = VisibleSettled;"), std::string::npos);
}

TEST(Skins, TeeStartLoadingFallbackSweepIsBoundedAndLogged)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/components/skins.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string HeaderSource = HeaderBuffer.str();
	EXPECT_NE(HeaderSource.find("m_FallbackSweepScanned"), std::string::npos);
	EXPECT_NE(HeaderSource.find("m_FallbackSweepStarted"), std::string::npos);
	EXPECT_EQ(HeaderSource.find("m_SettingsSourceFallbackSweepCursor"), std::string::npos);

	std::ifstream SourceFile(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();
	const size_t StartLoadingPos = Source.find("void CSkins::UpdateStartLoading(CSkinLoadingStats &Stats)");
	ASSERT_NE(StartLoadingPos, std::string::npos);
	const size_t StartLoadingEnd = Source.find("CSkins::ESkinProcessResult CSkins::ProcessSkinContainer", StartLoadingPos);
	ASSERT_NE(StartLoadingEnd, std::string::npos);
	const std::string StartLoading = Source.substr(StartLoadingPos, StartLoadingEnd - StartLoadingPos);

	EXPECT_NE(Source.find("event=skin_start_loading_fallback_sweep"), std::string::npos);
	EXPECT_NE(Source.find("items_total=%d items_scanned=%d items_started=%d items_skipped=%d invoked=%d dur_ms=%.3f reason=%s"), std::string::npos);
	EXPECT_NE(StartLoading.find("LogSettingsSkinStartLoadingFallbackSweepEvent("), std::string::npos);
	EXPECT_NE(StartLoading.find("disabled_explicit_queues"), std::string::npos);
	EXPECT_EQ(StartLoading.find("std::advance("), std::string::npos);
	EXPECT_EQ(StartLoading.find("for(auto &[_, pSkinContainer] : m_Skins)\n\t{"), std::string::npos);
}

TEST(Skins, SettingsAssetsListVirtualizationKeepsTotalListLength)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	const size_t LocalListPos = Source.find("if(!UsesCombinedAssetList(pCurrentCategory))");
	ASSERT_NE(LocalListPos, std::string::npos);
	const size_t WorkshopListPos = Source.find("if(const SAssetResourceCategory *pCategory = AssetResourceCategoryByTab(s_CurCustomTab); UsesCombinedAssetList(pCategory) && WorkshopHudView.h > 0.0f)", LocalListPos);
	ASSERT_NE(WorkshopListPos, std::string::npos);
	const std::string LocalListBody = Source.substr(LocalListPos, WorkshopListPos - LocalListPos);
	const size_t WorkshopListEnd = Source.find("if(Ui()->DoEditBox_Search", WorkshopListPos);
	ASSERT_NE(WorkshopListEnd, std::string::npos);
	const std::string WorkshopListBody = Source.substr(WorkshopListPos, WorkshopListEnd - WorkshopListPos);

	EXPECT_NE(LocalListBody.find("SettingsSkinListVisibleRangeForScroll("), std::string::npos);
	EXPECT_NE(LocalListBody.find("OldSelected = SelectedCustomAssetIndex(s_CurCustomTab, SearchListSize);"), std::string::npos);
	EXPECT_NE(LocalListBody.find("s_ListBox.SkipItems("), std::string::npos);
	EXPECT_NE(LocalListBody.find("assets_local_list_frame"), std::string::npos);
	EXPECT_EQ(LocalListBody.find("for(size_t i = 0; i < SearchListSize; ++i)"), std::string::npos);

	EXPECT_NE(WorkshopListBody.find("SettingsSkinListVisibleRangeForScroll("), std::string::npos);
	EXPECT_NE(WorkshopListBody.find("OldCombinedSelected = SelectedCombinedAssetIndex(s_CurCustomTab, vVisibleLocalAssetIndices);"), std::string::npos);
	EXPECT_NE(WorkshopListBody.find("s_WorkshopAssetsListBox.SkipItems("), std::string::npos);
	EXPECT_NE(WorkshopListBody.find("abs(WorkshopVisibleRange.m_FirstItem - PreviousFirstVisibleCombinedIndex)"), std::string::npos);
	EXPECT_NE(WorkshopListBody.find("assets_workshop_list_frame"), std::string::npos);
	EXPECT_EQ(WorkshopListBody.find("for(size_t ListIndex = 0; ListIndex < CombinedCount; ++ListIndex)"), std::string::npos);
}

TEST(Skins, TeePriorityRequestsReclaimBackgroundRequestedBeforeAdmittedBackgroundWork)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t ReclaimBackground = Source.find("bool CSkins::ReclaimBackgroundSkinForPriorityRequest");
	ASSERT_NE(ReclaimBackground, std::string::npos);
	const size_t ReclaimBackgroundEnd = Source.find("\n}", ReclaimBackground);
	ASSERT_NE(ReclaimBackgroundEnd, std::string::npos);
	const std::string ReclaimBody = Source.substr(ReclaimBackground, ReclaimBackgroundEnd - ReclaimBackground);

	const size_t BackgroundRequestedPos = ReclaimBody.find("if(pSkinContainer->m_State == CSkinContainer::EState::BACKGROUND_REQUESTED)");
	const size_t PendingBranchPos = ReclaimBody.find("if(pSkinContainer->m_State != CSkinContainer::EState::PENDING &&");
	ASSERT_NE(BackgroundRequestedPos, std::string::npos);
	ASSERT_NE(PendingBranchPos, std::string::npos);
	EXPECT_LT(BackgroundRequestedPos, PendingBranchPos);
}

TEST(Skins, SettingsResourcePriorityOnlyUpgradesTowardVisible)
{
	EXPECT_TRUE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::PREFETCH, ESettingsResourcePriority::BACKGROUND));
	EXPECT_TRUE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::VISIBLE, ESettingsResourcePriority::PREFETCH));
	EXPECT_TRUE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::VISIBLE, ESettingsResourcePriority::BACKGROUND));

	EXPECT_FALSE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::BACKGROUND, ESettingsResourcePriority::PREFETCH));
	EXPECT_FALSE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::PREFETCH, ESettingsResourcePriority::VISIBLE));
	EXPECT_FALSE(CSkins::CSkinContainer::SettingsResourcePriorityCanUpgrade(ESettingsResourcePriority::VISIBLE, ESettingsResourcePriority::VISIBLE));
}

TEST(Skins, PriorityRequestsCanReclaimBackgroundLoadingSlots)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t ReclaimBackground = Source.find("bool CSkins::ReclaimBackgroundSkinForPriorityRequest");
	ASSERT_NE(ReclaimBackground, std::string::npos);
	const size_t ReclaimBackgroundEnd = Source.find("\n}", ReclaimBackground);
	ASSERT_NE(ReclaimBackgroundEnd, std::string::npos);
	const std::string ReclaimBody = Source.substr(ReclaimBackground, ReclaimBackgroundEnd - ReclaimBackground);
	EXPECT_NE(ReclaimBody.find("CSkinContainer::EState::LOADING"), std::string::npos);
	EXPECT_NE(ReclaimBody.find("m_pLoadJob->Abort()"), std::string::npos);

	const size_t StartLoadJob = Source.find("auto StartLoadJob = [&]");
	ASSERT_NE(StartLoadJob, std::string::npos);
	const size_t StartLoadJobEnd = Source.find("\n\t};", StartLoadJob);
	ASSERT_NE(StartLoadJobEnd, std::string::npos);
	const std::string StartLoadBody = Source.substr(StartLoadJob, StartLoadJobEnd - StartLoadJob);
	EXPECT_NE(StartLoadBody.find("ReclaimBackgroundSkinForPriorityRequest"), std::string::npos);
	EXPECT_NE(StartLoadBody.find("Stats = LoadingStats();"), std::string::npos);
	EXPECT_EQ(StartLoadBody.find("Priority != ESettingsResourcePriority::BACKGROUND"), std::string::npos);
}

TEST(Skins, PrioritizedLoadQueueKeepsOriginalRequestPriority)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t StartLoadingPos = Source.find("void CSkins::UpdateStartLoading(CSkinLoadingStats &Stats)");
	ASSERT_NE(StartLoadingPos, std::string::npos);
	const size_t StartLoadingEnd = Source.find("CSkins::ESkinProcessResult CSkins::ProcessSkinContainer", StartLoadingPos);
	ASSERT_NE(StartLoadingEnd, std::string::npos);
	const std::string StartLoading = Source.substr(StartLoadingPos, StartLoadingEnd - StartLoadingPos);

	EXPECT_NE(StartLoading.find("StartLoadJob(It->second.get(), It->second->m_LoadPriority)"), std::string::npos);
	EXPECT_EQ(StartLoading.find("StartLoadJob(It->second.get(), ESettingsResourcePriority::VISIBLE)"), std::string::npos);
	EXPECT_NE(StartLoading.find("Stats.m_NumPending + Stats.m_NumLoading"), std::string::npos);
	EXPECT_EQ(StartLoading.find("Stats.m_NumPending + Stats.m_NumLoaded + Stats.m_NumLoading"), std::string::npos);
}

TEST(Skins, TeeSettingsScrollBudgetFeedsFinalizeAndUploadLimits)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t FinalizeBudget = Source.find("return pGameClient->m_Skins.SettingsFinalizeBudgetForFrame();");
	EXPECT_NE(FinalizeBudget, std::string::npos);

	const size_t UploadBudget = Source.find("return pGameClient->m_Skins.SettingsGpuUploadFrameBudgetForFrame();");
	EXPECT_NE(UploadBudget, std::string::npos);

	const size_t ImmediateScrollContext = Source.find("return SettingsBuildFrameContext(PersistentContext.m_ScrollActive, ImmediateScrollInput, PersistentContext.m_PostScrollRecoveryFrames);");
	EXPECT_NE(ImmediateScrollContext, std::string::npos);

	const size_t ScrollRegionMouseDown = Source.find("(pGameClient->Input()->KeyPress(KEY_MOUSE_1) && pUi->HotScrollRegion() != nullptr)");
	EXPECT_NE(ScrollRegionMouseDown, std::string::npos);
}

TEST(Skins, GpuUploadLimiterResetsBeforeSkinUpdateConsumesBudget)
{
	std::ifstream File(TestSourcePath("src/game/client/gameclient.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t OnUpdatePos = Source.find("void CGameClient::OnUpdate()");
	ASSERT_NE(OnUpdatePos, std::string::npos);
	const size_t OnRenderPos = Source.find("void CGameClient::OnRender()");
	ASSERT_NE(OnRenderPos, std::string::npos);
	const std::string OnUpdateBody = Source.substr(OnUpdatePos, OnRenderPos - OnUpdatePos);
	EXPECT_NE(OnUpdateBody.find("m_Skins.PrepareSettingsThroughputForFrame();"), std::string::npos);
	EXPECT_NE(OnUpdateBody.find("m_GpuUploadLimiter.OnFrameStart(FrameGpuUploadLimit);"), std::string::npos);
	EXPECT_NE(OnUpdateBody.find("const int FrameGpuUploadLimit = m_Menus.SettingsGpuUploadLimitForFrame(TeeSettingsActive, AssetsSettingsActive, m_Skins.SettingsGpuUploadLimiterUnitsForFrame());"), std::string::npos);
	EXPECT_NE(OnUpdateBody.find("m_Menus.ResetSettingsFrameBudgetForFrame(TeeSettingsActive, AssetsSettingsActive, FrameSkinUploadBudget);"), std::string::npos);

	const size_t OnRenderEnd = Source.find("const ColorRGBA ClearColor", OnRenderPos);
	ASSERT_NE(OnRenderEnd, std::string::npos);
	const std::string OnRenderPreamble = Source.substr(OnRenderPos, OnRenderEnd - OnRenderPos);
	EXPECT_EQ(OnRenderPreamble.find("m_GpuUploadLimiter.OnFrameStart();"), std::string::npos);
}

TEST(Skins, SettingsWarmupBypassesPeriodicSkinUpdateThrottle)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t WarmupPos = Source.find("void CSkins::UpdateForSettingsWarmup()");
	ASSERT_NE(WarmupPos, std::string::npos);
	const size_t PreparePos = Source.find("void CSkins::PrepareSettingsThroughputForFrame()", WarmupPos);
	ASSERT_NE(PreparePos, std::string::npos);
	const std::string WarmupBody = Source.substr(WarmupPos, PreparePos - WarmupPos);

	EXPECT_NE(WarmupBody.find("m_ContainerUpdateTime.reset();"), std::string::npos);
	EXPECT_NE(WarmupBody.find("OnUpdate();"), std::string::npos);
}

TEST(Skins, ManagedTeeRenderInfoSkipsInvalidSixupSkinNames)
{
	std::ifstream File(TestSourcePath("src/game/client/gameclient.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("NormalizeSixupSkinName(SkinDescriptor.m_aSkinName"), std::string::npos);
	EXPECT_NE(Source.find("CSkin::IsValidName(pManagedTeeRenderInfo->m_SkinDescriptor.m_aSkinName)"), std::string::npos);
	EXPECT_NE(Source.find("m_Skins.Find(CSkin::IsValidName(SkinDescriptor.m_aSkinName) ? SkinDescriptor.m_aSkinName : \"default\")"), std::string::npos);
}

TEST(Skins, BackgroundRequestedStatusUsesLoadingIndicator)
{
	using EIndicator = CSkins::CSkinContainer::EStatusIndicator;
	using EState = CSkins::CSkinContainer::EState;

	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::UNLOADED), EIndicator::LOADING);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::BACKGROUND_REQUESTED), EIndicator::LOADING);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::PENDING), EIndicator::LOADING);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::LOADING), EIndicator::LOADING);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::NOT_FOUND), EIndicator::NOT_FOUND);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::ERROR), EIndicator::ERROR);
	EXPECT_EQ(CSkins::CSkinContainer::StatusIndicator(EState::LOADED), EIndicator::NONE);
}

TEST(Skins, TeeSkinUploadRequiresWholeSourceTextureBudgetBeforeUpload)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t ProcessSkinPos = Source.find("CSkins::ESkinProcessResult CSkins::ProcessSkinContainer");
	ASSERT_NE(ProcessSkinPos, std::string::npos);
	const size_t LoadingStatsPos = Source.find("CSkins::CSkinLoadingStats CSkins::LoadingStats() const", ProcessSkinPos);
	ASSERT_NE(LoadingStatsPos, std::string::npos);
	const std::string ProcessSkinBody = Source.substr(ProcessSkinPos, LoadingStatsPos - ProcessSkinPos);

	EXPECT_NE(ProcessSkinBody.find("CanUpload(SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)"), std::string::npos);
	EXPECT_NE(ProcessSkinBody.find("UploadBudget.m_MaxGpuUploads = 1;"), std::string::npos);
	EXPECT_NE(ProcessSkinBody.find("SettingsResourceConsumeGpuUpload(UploadBudget, SettingsFrameBudgetOrNull(GameClient()))"), std::string::npos);
	EXPECT_NE(ProcessSkinBody.find("LogSettingsSkinSourceWaitEvent(pSkinContainer->Name(), \"gpu_upload_budget\""), std::string::npos);
	EXPECT_NE(ProcessSkinBody.find("LogSettingsSkinSourceWaitEvent(pSkinContainer->Name(), \"max_per_frame\""), std::string::npos);
	EXPECT_NE(Source.find("static constexpr int SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS = 24;"), std::string::npos);
	EXPECT_NE(Source.find("event=%s skin=%s artifact=source width=%d height=%d bytes=%d dur_ms=%.3f uploads=%d"), std::string::npos);
	EXPECT_NE(Source.find("LogSettingsSkinSourceStageEvent(\"upload_done\""), std::string::npos);
}

TEST(Skins, TeeSettingsListUsesIdleBackgroundRequestsAfterVisibleSettle)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t RenderTeePos = Source.find("void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_NE(RenderTeePos, std::string::npos);
	const size_t RenderTeeEnd = Source.find("void CMenus::RenderSettingsAppearance", RenderTeePos);
	ASSERT_NE(RenderTeeEnd, std::string::npos);
	const std::string RenderTeeBody = Source.substr(RenderTeePos, RenderTeeEnd - RenderTeePos);

	EXPECT_NE(RenderTeeBody.find("RequestLoad(ESettingsResourcePriority::VISIBLE)"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("RequestLoad(ESettingsResourcePriority::PREFETCH)"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("static std::vector<size_t> s_vVisibleSkinIndices;"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("vVisibleSkinIndices.reserve(vSkinList.size())"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("vVisibleSkinIndices.push_back(i);"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("for(auto It = vVisibleSkinIndices.rbegin(); It != vVisibleSkinIndices.rend(); ++It)"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("std::binary_search(vVisibleSkinIndices.begin(), vVisibleSkinIndices.end(), BackgroundIndex)"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("std::find(vVisibleSkinIndices.begin(), vVisibleSkinIndices.end(), BackgroundIndex)"), std::string::npos);
	EXPECT_EQ(RenderTeeBody.find("std::find(vVisibleSkinIndices.begin(), vVisibleSkinIndices.end(), i)"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const bool RequestWindowScrollBlocked = SkinListScrollInteraction || s_SkinListScrollCooldownFrames > 0;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const bool VisibleSourceSettled = VisibleSourceSettledCount == (int)vVisibleSkinIndices.size();"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const int DefaultBackgroundRequestBudget = Throughput.m_BackgroundRequestBudget;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const auto BackgroundBudgetDecision = SettingsSkinBackgroundRequestBudgetDecision({"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("const int BackgroundRequestBudget = BackgroundBudgetDecision.m_RequestBudget;"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("vSkinList[BackgroundIndex].RequestLoad(ESettingsResourcePriority::BACKGROUND);"), std::string::npos);
	EXPECT_NE(RenderTeeBody.find("GameClient()->m_Skins.SetSettingsTeeVisibleSnapshot(VisibleSnapshot);"), std::string::npos);
}

TEST(Skins, TeeSourcePathEmitsRequestAndFrameCapPerfLogs)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("event=source_request skin=%s priority=%s state=%s"), std::string::npos);
	EXPECT_NE(Source.find("event=source_wait skin=%s artifact=source reason=%s remaining_uploads=%d max_uploads=%d"), std::string::npos);
	EXPECT_NE(Source.find("event=frame_cap gpu_cap=%d finalize_cap=%d loading_visible_cap=%d loading_other_cap=%d"), std::string::npos);
	EXPECT_NE(Source.find("LogSettingsSkinSourceRequestEvent(pSkinContainer->Name(), Priority, pSkinContainer->m_State);"), std::string::npos);
	EXPECT_NE(Source.find("LogSettingsSkinFrameCapEvent(GameClient());"), std::string::npos);
}

TEST(Skins, TeeSourcePathCapsActiveLoadingBeforeQueueFuse)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t StartLoadingPos = Source.find("void CSkins::UpdateStartLoading(CSkinLoadingStats &Stats)");
	ASSERT_NE(StartLoadingPos, std::string::npos);
	const size_t StartLoadingEnd = Source.find("CSkins::ESkinProcessResult CSkins::ProcessSkinContainer", StartLoadingPos);
	ASSERT_NE(StartLoadingEnd, std::string::npos);
	const std::string StartLoading = Source.substr(StartLoadingPos, StartLoadingEnd - StartLoadingPos);

	EXPECT_NE(StartLoading.find("const bool BackgroundDrainActive = TeeSettingsActive ? m_SettingsThroughputControllerOutput.m_BackgroundDrainActive"), std::string::npos);
	EXPECT_NE(StartLoading.find("const int CountFuseLimit = TeeSettingsActive ? m_SettingsThroughputControllerOutput.m_CountFuseLimit"), std::string::npos);
	EXPECT_NE(StartLoading.find("const int NormalLoadingWindow = TeeSettingsActive ? m_SettingsThroughputControllerOutput.m_NormalLoadingWindow"), std::string::npos);
	EXPECT_NE(StartLoading.find("const int VisibleLoadingWindow = TeeSettingsActive ? m_SettingsThroughputControllerOutput.m_VisibleLoadingWindow"), std::string::npos);
	EXPECT_NE(StartLoading.find("m_SettingsSourceAdmissionTelemetry.m_VisibleReserve = TeeSettingsActive ? m_SettingsThroughputControllerOutput.m_VisibleReserve : 8;"), std::string::npos);
	EXPECT_NE(StartLoading.find("const auto Admission = DetermineAdmission(pSkinContainer, Priority);"), std::string::npos);
	EXPECT_NE(StartLoading.find("const auto SourceAdmission = SettingsSkinSourceAdmissionDecision({"), std::string::npos);
	EXPECT_NE(StartLoading.find("Admission.m_pBlockReason = SettingsSkinSourceAdmissionBlockReasonName(SourceAdmission.m_BlockReason);"), std::string::npos);
	EXPECT_NE(StartLoading.find("const bool CountFuseApplies = Admission.m_CountFuseApplies;"), std::string::npos);
	EXPECT_NE(StartLoading.find("LogSettingsSkinSourceWaitEvent(pSkinContainer->Name(), Admission.m_pBlockReason"), std::string::npos);
}

TEST(Skins, TeeBackgroundWindowUsesRealDecodeJobSaturationSignal)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PreparePos = Source.find("void CSkins::PrepareSettingsThroughputForFrame()");
	ASSERT_NE(PreparePos, std::string::npos);
	const size_t NextFunctionPos = Source.find("void CSkins::ClampSkinQueueIndex(int Dummy)", PreparePos);
	ASSERT_NE(NextFunctionPos, std::string::npos);
	const std::string PrepareBody = Source.substr(PreparePos, NextFunctionPos - PreparePos);

	EXPECT_NE(PrepareBody.find("int LoadingJobsAwaitingResult = 0;"), std::string::npos);
	EXPECT_NE(PrepareBody.find("int LoadingJobsReadyForMainThread = 0;"), std::string::npos);
	EXPECT_NE(PrepareBody.find("if(!pSkinContainer->m_pLoadJob->Done())"), std::string::npos);
	EXPECT_NE(PrepareBody.find("const bool DecodeJobsSaturated ="), std::string::npos);
	EXPECT_NE(PrepareBody.find("LoadingJobsReadyForMainThread == 0"), std::string::npos);
	EXPECT_NE(PrepareBody.find("m_SettingsThroughputControllerOutput = SettingsSkinThroughputControllerStep({"), std::string::npos);
}

TEST(Skins, TeeFinishLoadingKeepsPriorityBeforeBackgroundSweep)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t FinishLoadingPos = Source.find("void CSkins::UpdateFinishLoading(");
	ASSERT_NE(FinishLoadingPos, std::string::npos);
	const size_t FinishLoadingEnd = Source.find("void CSkins::RefreshEventSkins()", FinishLoadingPos);
	ASSERT_NE(FinishLoadingEnd, std::string::npos);
	const std::string FinishLoading = Source.substr(FinishLoadingPos, FinishLoadingEnd - FinishLoadingPos);

	const size_t UsageListPos = FinishLoading.find("for(const std::string &SkinName : vUsageSnapshot)");
	const size_t DeferBackgroundPos = FinishLoading.find("if(SettingsSkinFinalizeShouldDeferBackgroundSweep(ProcessedHighPrioritySkin, SkinsProcessedThisFrame, MaxSkinsPerFrame))");
	const size_t BackgroundListPos = FinishLoading.find("for(const std::string &SkinName : vBackgroundSnapshot)");
	const size_t FallbackSweepPos = FinishLoading.find("for(auto &[_, pSkinContainer] : m_Skins)");

	ASSERT_NE(UsageListPos, std::string::npos);
	ASSERT_NE(DeferBackgroundPos, std::string::npos);
	ASSERT_NE(BackgroundListPos, std::string::npos);
	ASSERT_NE(FallbackSweepPos, std::string::npos);
	EXPECT_LT(UsageListPos, DeferBackgroundPos);
	EXPECT_LT(DeferBackgroundPos, BackgroundListPos);
	EXPECT_LT(BackgroundListPos, FallbackSweepPos);
}

TEST(Skins, TeeSettingsListEmitsRequestWindowPerfLogs)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("controller_reason=%s"), std::string::npos);
	EXPECT_NE(Source.find("frame_time_avg_ms=%.3f render_frame_time_ms=%.3f admission_underfed=%d underfed_streak=%d"), std::string::npos);
	EXPECT_NE(Source.find("visible_reserve_effective=%d"), std::string::npos);
	EXPECT_NE(Source.find("GameClient()->m_Skins.SetSettingsTeeVisibleSnapshot(VisibleSnapshot);"), std::string::npos);
	EXPECT_NE(Source.find("event=work_drain page=settings:tee kind=merge count=%llu bytes=%d dur_ms=%.3f stop=%s source=list_drain_summary scope=session"), std::string::npos);
	EXPECT_NE(Source.find("uploads_done_total=%llu loaded_total=%llu uploads_per_sec=%.3f loaded_per_sec=%.3f"), std::string::npos);
	EXPECT_NE(Source.find("max_requested=%d max_pending=%d max_loading=%d max_real_inflight=%d count_fuse_limit=%d"), std::string::npos);
	EXPECT_NE(Source.find("total_requested=%llu total_admitted=%llu total_started=%llu"), std::string::npos);
	EXPECT_NE(Source.find("num_loading_window_waits=%d num_gpu_budget_waits=%d num_queue_fuse_waits=%d full_list_ready=%d final_real_inflight=%d"), std::string::npos);
	EXPECT_NE(Source.find("last_wait_reason=%s last_dynamic_decision=%s last_request_budget_block_reason=%s"), std::string::npos);
	EXPECT_NE(Source.find("event=admission_invariant_violation pending=%d loading=%d real_inflight=%d count_fuse_limit=%d"), std::string::npos);
	EXPECT_NE(Source.find("if(gs_TeeListDrainPerfSession.m_Active)\n\t\t\tLogTeeListDrainSummary(Client(), GameClient()->m_Skins, GameClient()->m_Skins.LoadingStats(), false, RefreshNowNs);"), std::string::npos);
	EXPECT_NE(Source.find("BeginTeeListDrainPerfSession(GameClient()->m_Skins, RefreshNowNs);"), std::string::npos);
	EXPECT_NE(Source.find("m_SettingsHighPrioritySettled = VisibleSourceSettled;"), std::string::npos);
	EXPECT_NE(Source.find("if(PerfDebugEnabled() &&"), std::string::npos);
	EXPECT_NE(Source.find("if(m_SettingsRuntimeMetadata.m_LastPage != SETTINGS_TEE)"), std::string::npos);
}

TEST(Skins, PrewarmPlayerPreviewReadyRequiresSelectedAndVisibleSourcesLoaded)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PrewarmPos = Source.find("bool CSkins::PrewarmPlayerPreviewReady(int Dummy, int MaxEntries, bool ProgressiveListReady)");
	ASSERT_NE(PrewarmPos, std::string::npos);
	const size_t PrewarmEnd = Source.find("void CSkins::QueueSkinListPlanJob(int Dummy)", PrewarmPos);
	ASSERT_NE(PrewarmEnd, std::string::npos);
	const std::string PrewarmBody = Source.substr(PrewarmPos, PrewarmEnd - PrewarmPos);

	EXPECT_NE(PrewarmBody.find("pSelectedContainer"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("SelectedReady"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("VisibleReadyCount"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("SettingsPreviewCacheContentHash()"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("DiskCacheArtifactsValid"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("FindTextures(CacheKey).has_value()"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("State == CSkinContainer::EState::LOADED"), std::string::npos);
}

TEST(Skins, PrewarmPlayerPreviewReadyNoLongerBuildsPreviewCacheKeys)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PrewarmPos = Source.find("bool CSkins::PrewarmPlayerPreviewReady(int Dummy, int MaxEntries, bool ProgressiveListReady)");
	ASSERT_NE(PrewarmPos, std::string::npos);
	const size_t PrewarmEnd = Source.find("void CSkins::QueueSkinListPlanJob(int Dummy)", PrewarmPos);
	ASSERT_NE(PrewarmEnd, std::string::npos);
	const std::string PrewarmBody = Source.substr(PrewarmPos, PrewarmEnd - PrewarmPos);

	EXPECT_EQ(PrewarmBody.find("SSettingsSkinPreviewCacheKey CacheKey"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("ColorBody"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("ColorFeet"), std::string::npos);
}

TEST(Skins, TeeSettingsRequestsNoLongerPromoteToPendingAtRequestSite)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t RequestLoadPos = Source.find("void CSkins::CSkinContainer::RequestLoad(ESettingsResourcePriority Priority)");
	ASSERT_NE(RequestLoadPos, std::string::npos);
	const size_t RequestLoadEnd = Source.find("CSkins::CSkinContainer::EState CSkins::CSkinContainer::DetermineInitialState() const", RequestLoadPos);
	ASSERT_NE(RequestLoadEnd, std::string::npos);
	const std::string RequestLoadBody = Source.substr(RequestLoadPos, RequestLoadEnd - RequestLoadPos);

	EXPECT_NE(RequestLoadBody.find("const bool TeeSettingsActive = ActiveSettingsTeePage(m_pSkins->GameClient());"), std::string::npos);
	EXPECT_NE(RequestLoadBody.find("SetState(EState::BACKGROUND_REQUESTED, Priority);"), std::string::npos);
	EXPECT_EQ(RequestLoadBody.find("SetState(EState::PENDING, Priority);"), std::string::npos);
}

TEST(Skins, TeePrewarmNoLongerUsesImmediateBoolPath)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PrewarmByNamesPos = Source.find("void CSkins::PrewarmByNames(const std::vector<std::string> &vNames, bool Immediate)");
	ASSERT_NE(PrewarmByNamesPos, std::string::npos);
	const size_t PrewarmReadyPos = Source.find("bool CSkins::PrewarmPlayerPreviewReady(int Dummy, int MaxEntries, bool ProgressiveListReady)", PrewarmByNamesPos);
	ASSERT_NE(PrewarmReadyPos, std::string::npos);
	const std::string PrewarmByNamesBody = Source.substr(PrewarmByNamesPos, PrewarmReadyPos - PrewarmByNamesPos);

	EXPECT_EQ(PrewarmByNamesBody.find("RequestLoad(Immediate)"), std::string::npos);
	EXPECT_NE(PrewarmByNamesBody.find("Immediate ? ESettingsResourcePriority::VISIBLE : ESettingsResourcePriority::PREFETCH"), std::string::npos);

	const size_t FindImplPos = Source.find("const CSkins::CSkinContainer *CSkins::FindContainerImpl(const char *pName)");
	ASSERT_NE(FindImplPos, std::string::npos);
	const size_t FindOrNullptrPos = Source.find("const CSkin *CSkins::FindOrNullptr(const char *pName)", FindImplPos);
	ASSERT_NE(FindOrNullptrPos, std::string::npos);
	const std::string FindImplBody = Source.substr(FindImplPos, FindOrNullptrPos - FindImplPos);
	EXPECT_NE(FindImplBody.find("ExistingSkin->second->RequestLoad(true);"), std::string::npos);
}

TEST(Skins, SourceResidencyNoLongerDependsOnPreviewCachePins)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t UpdateUnload = Source.find("void CSkins::UpdateUnloadSkins");
	ASSERT_NE(UpdateUnload, std::string::npos);
	EXPECT_EQ(Source.find("IsSettingsPreviewCachePinned()", UpdateUnload), std::string::npos);

	const size_t ReclaimBackground = Source.find("bool CSkins::ReclaimBackgroundSkinForPriorityRequest");
	ASSERT_NE(ReclaimBackground, std::string::npos);
	EXPECT_EQ(Source.find("IsSettingsPreviewCachePinned()", ReclaimBackground), std::string::npos);
	EXPECT_NE(Source.find("if(pSkinContainer->m_State == CSkinContainer::EState::LOADED)"), ReclaimBackground);
	EXPECT_NE(Source.find("continue;"), ReclaimBackground);
	EXPECT_EQ(Source.find("NumPendingLoadingLoaded"), std::string::npos);
}

TEST(Skins, SkinListWaitsForCompletePlanInsteadOfSeedingPlaceholderEntry)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_EQ(Source.find("SeedVisibleSkinListIfEmpty"), std::string::npos);
	EXPECT_NE(Source.find("m_SkinList.m_vSkins = std::move(m_vPendingSkinListEntries);"), std::string::npos);
}

TEST(Skins, AsyncSkinListKeepsQueuedColorVariantsSelectable)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/components/skins.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string Header = HeaderBuffer.str();

	std::ifstream SourceFile(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();

	std::ifstream MenuFile(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(MenuFile.good());
	std::stringstream MenuBuffer;
	MenuBuffer << MenuFile.rdbuf();
	const std::string MenuSource = MenuBuffer.str();

	EXPECT_NE(Header.find("struct SColorKey"), std::string::npos);
	EXPECT_NE(Header.find("const std::optional<SColorKey> &ColorKey() const"), std::string::npos);
	EXPECT_NE(Header.find("CSkinList &SkinList(int Dummy);"), std::string::npos);

	EXPECT_NE(Source.find("MakeSkinListColorKey(QueueEntry.m_UseCustomColor"), std::string::npos);
	EXPECT_NE(Source.find("m_vPendingSkinListMergeEntries = std::move(Result.m_Plan.m_vEntries);"), std::string::npos);
	EXPECT_NE(Source.find("Entry.m_ColorKey.has_value() ? std::make_optional(MakeSkinListColorKey(Entry.m_ColorKey.value())) : std::nullopt"), std::string::npos);
	EXPECT_NE(Source.find("MakeSkinListEntry(SkinIt->second.get(), ColorKey)"), std::string::npos);

	EXPECT_NE(MenuSource.find("SelectedSkinEntry.ColorKey().has_value()"), std::string::npos);
	EXPECT_NE(MenuSource.find("*pUseCustomColor = SelectedColorKey.m_UseCustomColor ? 1 : 0;"), std::string::npos);
	EXPECT_NE(MenuSource.find("*pColorBody = SelectedColorKey.m_ColorBody;"), std::string::npos);
	EXPECT_NE(MenuSource.find("*pColorFeet = SelectedColorKey.m_ColorFeet;"), std::string::npos);
}

TEST(Skins, DirectoryScanPromotesDownloadContainersToLocal)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t ProcessDirectoryPos = Source.find("void CSkins::ProcessSkinDirectoryScanJob()");
	ASSERT_NE(ProcessDirectoryPos, std::string::npos);
	const size_t ProcessListPlanPos = Source.find("void CSkins::ProcessSkinListPlanJob()", ProcessDirectoryPos);
	ASSERT_NE(ProcessListPlanPos, std::string::npos);
	const std::string ProcessDirectoryBody = Source.substr(ProcessDirectoryPos, ProcessListPlanPos - ProcessDirectoryPos);

	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->Type() == CSkinContainer::EType::DOWNLOAD"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->m_Type = CSkinContainer::EType::LOCAL;"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->m_StorageType = Entry.m_StorageType;"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->SetLastModified(Entry.m_LastModified);"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->m_pLoadJob->Abort();"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("if(OldState == CSkinContainer::EState::LOADED && pSkinContainer->m_pSkin)"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->m_pSkin->m_OriginalSkin.Unload(Graphics());"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->m_pSkin.reset();"), std::string::npos);
	EXPECT_NE(ProcessDirectoryBody.find("pSkinContainer->SetState(CSkinContainer::EState::PENDING, OldPriority);"), std::string::npos);
}

TEST(Skins, SkinDataPreparationBuildsMergedMetricsWithoutGraphics)
{
	CImageInfo Image = MakeTestSkinImage(64, 32);
	SetTestPixel(Image, 1, 2, 12, 24, 48, 255);
	SetTestPixel(Image, 26, 4, 255, 0, 0, 255);
	SetTestPixel(Image, 28, 6, 255, 0, 0, 255);
	SetTestPixel(Image, 49, 10, 0, 255, 0, 255);
	SetTestPixel(Image, 52, 19, 0, 255, 0, 255);

	const CSkins::SSkinSpriteSpec Body{8, 4, 0, 0, 3, 3};
	const CSkins::SSkinSpriteSpec BodyOutline{8, 4, 3, 0, 3, 3};
	const CSkins::SSkinSpriteSpec Feet{8, 4, 6, 1, 2, 1};
	const CSkins::SSkinSpriteSpec FeetOutline{8, 4, 6, 2, 2, 1};
	CSkins::SSkinDataPlan Plan;

	EXPECT_TRUE(CSkins::BuildSkinDataPlan(Image, Body, BodyOutline, Feet, FeetOutline, Plan));

	EXPECT_EQ(Plan.m_Body.m_Width, 4);
	EXPECT_EQ(Plan.m_Body.m_Height, 5);
	EXPECT_EQ(Plan.m_Body.m_OffsetX, 1);
	EXPECT_EQ(Plan.m_Body.m_OffsetY, 2);
	EXPECT_EQ(Plan.m_Body.m_MaxWidth, 24);
	EXPECT_EQ(Plan.m_Body.m_MaxHeight, 24);
	EXPECT_EQ(Plan.m_Feet.m_Width, 4);
	EXPECT_EQ(Plan.m_Feet.m_Height, 2);
	EXPECT_EQ(Plan.m_Feet.m_OffsetX, 1);
	EXPECT_EQ(Plan.m_Feet.m_OffsetY, 2);
	EXPECT_EQ(Plan.m_Feet.m_MaxWidth, 16);
	EXPECT_EQ(Plan.m_Feet.m_MaxHeight, 8);

	Image.Free();
}

TEST(Skins, SkinDataPreparationUsesOutlineMetricsWhenFillIsEmpty)
{
	CImageInfo Image = MakeTestSkinImage(64, 32);
	SetTestPixel(Image, 27, 5, 255, 0, 0, 255);
	SetTestPixel(Image, 29, 7, 255, 0, 0, 255);
	SetTestPixel(Image, 50, 18, 0, 255, 0, 255);
	SetTestPixel(Image, 53, 21, 0, 255, 0, 255);

	const CSkins::SSkinSpriteSpec Body{8, 4, 0, 0, 3, 3};
	const CSkins::SSkinSpriteSpec BodyOutline{8, 4, 3, 0, 3, 3};
	const CSkins::SSkinSpriteSpec Feet{8, 4, 6, 1, 2, 1};
	const CSkins::SSkinSpriteSpec FeetOutline{8, 4, 6, 2, 2, 1};
	CSkins::SSkinDataPlan Plan;

	EXPECT_TRUE(CSkins::BuildSkinDataPlan(Image, Body, BodyOutline, Feet, FeetOutline, Plan));

	EXPECT_EQ(Plan.m_Body.m_Width, 3);
	EXPECT_EQ(Plan.m_Body.m_Height, 3);
	EXPECT_EQ(Plan.m_Body.m_OffsetX, 3);
	EXPECT_EQ(Plan.m_Body.m_OffsetY, 5);
	EXPECT_EQ(Plan.m_Feet.m_Width, 4);
	EXPECT_EQ(Plan.m_Feet.m_Height, 4);
	EXPECT_EQ(Plan.m_Feet.m_OffsetX, 2);
	EXPECT_EQ(Plan.m_Feet.m_OffsetY, 2);

	Image.Free();
}

TEST(Skins, RenderedTeeOffsetKeepsGlobalHorizontalOffsetStable)
{
	CTeeRenderInfo Info;
	SetBeastLikeMetrics(Info);

	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(CAnimState::GetIdle(), &Info, OffsetToMid);
	const vec2 Mid = ComputeRenderedTeeMid(Info);

	EXPECT_FLOAT_EQ(OffsetToMid.x, 0.0f);
	EXPECT_NEAR(OffsetToMid.y + Mid.y, 0.0f, 0.0001f);
}

TEST(Skins, SkinQueueEntrySixupDataParticipatesInEquality)
{
	CSkins::CSkinQueueEntry Base;
	Base.m_SkinName = "cammostripes";
	Base.m_UseCustomColor = true;
	Base.m_ColorBody = 123;
	Base.m_ColorFeet = 456;
	Base.m_HasSixup = true;
	for(int Part = 0; Part < protocol7::NUM_SKINPARTS; ++Part)
	{
		str_copy(Base.m_aaSixupSkinPartNames[Part], "standard", sizeof(Base.m_aaSixupSkinPartNames[Part]));
		Base.m_aSixupUseCustomColors[Part] = 0;
		Base.m_aSixupSkinPartColors[Part] = Part;
	}

	CSkins::CSkinQueueEntry Same = Base;
	EXPECT_TRUE(Base == Same);

	CSkins::CSkinQueueEntry DifferentPartName = Base;
	str_copy(DifferentPartName.m_aaSixupSkinPartNames[0], "kitty", sizeof(DifferentPartName.m_aaSixupSkinPartNames[0]));
	EXPECT_FALSE(Base == DifferentPartName);

	CSkins::CSkinQueueEntry DifferentUseCustomColor = Base;
	DifferentUseCustomColor.m_aSixupUseCustomColors[1] = 1;
	EXPECT_FALSE(Base == DifferentUseCustomColor);

	CSkins::CSkinQueueEntry DifferentPartColor = Base;
	DifferentPartColor.m_aSixupSkinPartColors[2] = 999;
	EXPECT_FALSE(Base == DifferentPartColor);
}

TEST(Skins, MapPlayerSkinQueueSyncUpdatesExistingQueueInPlace)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t SyncPos = Source.find("void CSkins::SyncSkinQueueFromMapPlayers(int Dummy)");
	ASSERT_NE(SyncPos, std::string::npos);
	const size_t SyncEnd = Source.find("bool CSkins::IsInSkinQueue", SyncPos);
	ASSERT_NE(SyncEnd, std::string::npos);
	const std::string SyncBody = Source.substr(SyncPos, SyncEnd - SyncPos);
	const size_t UpdatePos = Source.find("void CSkins::UpdateSkinQueue(std::chrono::nanoseconds Now, int Dummy)");
	ASSERT_NE(UpdatePos, std::string::npos);
	const size_t UpdateEnd = Source.find("void CSkins::SyncSkinQueueFromMapPlayers(int Dummy)", UpdatePos);
	ASSERT_NE(UpdateEnd, std::string::npos);
	const std::string UpdateBody = Source.substr(UpdatePos, UpdateEnd - UpdatePos);
	const size_t ApplyPresetPos = Source.find("bool CSkins::ApplySkinQueuePreset(size_t PresetIndex, int Dummy)");
	ASSERT_NE(ApplyPresetPos, std::string::npos);
	const size_t ApplyPresetEnd = Source.find("bool CSkins::RemoveSkinQueuePreset", ApplyPresetPos);
	ASSERT_NE(ApplyPresetEnd, std::string::npos);
	const std::string ApplyPresetBody = Source.substr(ApplyPresetPos, ApplyPresetEnd - ApplyPresetPos);

	EXPECT_EQ(SyncBody.find("std::vector<CSkinQueueEntry> vMapSkins"), std::string::npos);
	EXPECT_EQ(SyncBody.find("Queue = std::move("), std::string::npos);
	EXPECT_EQ(SyncBody.find("m_aSkinQueue[Dummy] ="), std::string::npos);
	EXPECT_EQ(ApplyPresetBody.find("m_aSkinQueue[Dummy] ="), std::string::npos);
	EXPECT_EQ(UpdateBody.find("TrimSkinQueueToLimit(Dummy);"), std::string::npos);
	EXPECT_EQ(ApplyPresetBody.find("TrimSkinQueueToLimit(Dummy);"), std::string::npos);
	EXPECT_EQ(UpdateBody.find("SkinQueueLengthVar(Dummy)"), std::string::npos);
	EXPECT_EQ(SyncBody.find("SyncSkinQueueEntriesInPlace(Queue, aMapSkins.data(), DesiredCount)"), std::string::npos);
	EXPECT_EQ(SyncBody.find("Queue.erase(Queue.begin() + DesiredCount, Queue.end());"), std::string::npos);
	EXPECT_NE(Source.find("#include <array>"), std::string::npos);
	EXPECT_NE(SyncBody.find("std::array<CSkinQueueEntry, MAX_CLIENTS> aMapSkins"), std::string::npos);
	EXPECT_NE(SyncBody.find("SyncSkinQueueEntriesInPlace(Queue, aMapSkins.data(), DesiredCount, false)"), std::string::npos);
	EXPECT_NE(ApplyPresetBody.find("SyncSkinQueueEntriesInPlace(m_aSkinQueue[Dummy], Presets[PresetIndex].m_Queue.data(), Presets[PresetIndex].m_Queue.size())"), std::string::npos);
	EXPECT_NE(ApplyPresetBody.find("SkinQueueIndexVar(Dummy) = 0;"), std::string::npos);
	EXPECT_NE(ApplyPresetBody.find("m_aSkinQueueElapsed[Dummy] = 0ns;"), std::string::npos);
	EXPECT_NE(ApplyPresetBody.find("m_aSkinQueueLastUpdate[Dummy].reset();"), std::string::npos);
	EXPECT_NE(ApplyPresetBody.find("ApplySkinQueueCurrent(Dummy);"), std::string::npos);
	EXPECT_NE(Source.find("Queue.erase(Queue.begin() + DesiredCount, Queue.end());"), std::string::npos);
	EXPECT_NE(Source.find("Queue.insert(Queue.begin() + DesiredIndex, pDesiredEntries[DesiredIndex]);"), std::string::npos);
	EXPECT_NE(Source.find("std::rotate(Queue.begin() + DesiredIndex, It, It + 1);"), std::string::npos);
}

TEST(Skins, SkinQueuePresetsAreSelectableEditableQueues)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/components/skins.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string Header = HeaderBuffer.str();

	std::ifstream SourceFile(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();
	const size_t SelectPresetPos = Source.find("bool CSkins::SelectSkinQueuePreset(size_t PresetIndex, int Dummy)");
	ASSERT_NE(SelectPresetPos, std::string::npos);
	const size_t SelectPresetEnd = Source.find("void CSkins::ClearSkinQueuePresetSelection(int Dummy)", SelectPresetPos);
	ASSERT_NE(SelectPresetEnd, std::string::npos);
	const std::string SelectPresetBody = Source.substr(SelectPresetPos, SelectPresetEnd - SelectPresetPos);

	std::ifstream MenusFile(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string Menus = MenusBuffer.str();
	const size_t PresetsUiPos = Menus.find("if(QueuePresets.h > 0.0f)");
	ASSERT_NE(PresetsUiPos, std::string::npos);
	const size_t PresetsUiEnd = Menus.find("MainView.HSplitTop(5.0f, nullptr, &MainView);", PresetsUiPos);
	ASSERT_NE(PresetsUiEnd, std::string::npos);
	const std::string PresetsUi = Menus.substr(PresetsUiPos, PresetsUiEnd - PresetsUiPos);

	EXPECT_NE(Header.find("int ActiveSkinQueuePresetIndex(int Dummy) const"), std::string::npos);
	EXPECT_NE(Header.find("int AppliedSkinQueuePresetIndex(int Dummy) const"), std::string::npos);
	EXPECT_NE(Header.find("bool SelectSkinQueuePreset(size_t PresetIndex, int Dummy)"), std::string::npos);
	EXPECT_NE(Header.find("void ClearSkinQueuePresetSelection(int Dummy)"), std::string::npos);
	EXPECT_NE(Header.find("const std::vector<CSkinQueueEntry> &ActiveSkinQueue(int Dummy) const"), std::string::npos);
	EXPECT_NE(Header.find("bool AddActiveSkinQueue("), std::string::npos);
	EXPECT_NE(Header.find("bool RemoveActiveSkinQueue("), std::string::npos);
	EXPECT_NE(Header.find("void MoveActiveSkinQueueItem("), std::string::npos);
	EXPECT_NE(Header.find("bool ApplySkinQueueIndex(size_t QueueIndex, int Dummy)"), std::string::npos);
	EXPECT_NE(Header.find("void TrimActiveSkinQueueToLimit("), std::string::npos);
	EXPECT_NE(Header.find("std::array<int, NUM_DUMMIES> m_aActiveSkinQueuePresetIndex"), std::string::npos);
	EXPECT_NE(Header.find("std::array<int, NUM_DUMMIES> m_aAppliedSkinQueuePresetIndex"), std::string::npos);
	EXPECT_NE(Header.find("std::vector<CSkinQueuePreset> m_vSkinQueuePresets"), std::string::npos);
	EXPECT_EQ(Header.find("std::array<std::vector<CSkinQueuePreset>, NUM_DUMMIES> m_aSkinQueuePresets"), std::string::npos);

	EXPECT_NE(Source.find("std::fill(m_aActiveSkinQueuePresetIndex.begin(), m_aActiveSkinQueuePresetIndex.end(), -1);"), std::string::npos);
	EXPECT_NE(Source.find("std::fill(m_aAppliedSkinQueuePresetIndex.begin(), m_aAppliedSkinQueuePresetIndex.end(), -1);"), std::string::npos);
	EXPECT_NE(Source.find("std::vector<CSkins::CSkinQueueEntry> &CSkins::ActiveSkinQueueMutable(int Dummy)"), std::string::npos);
	EXPECT_NE(Source.find("return ActivePresetIndex >= 0 ? m_vSkinQueuePresets[ActivePresetIndex].m_Queue : m_aSkinQueue[Dummy];"), std::string::npos);
	EXPECT_EQ(Source.find("m_aSkinQueuePresets[Dummy]"), std::string::npos);
	EXPECT_NE(Source.find("Localize(\"Preset %d\")"), std::string::npos);
	EXPECT_EQ(Source.find("\"Preset %d\""), Source.find("Localize(\"Preset %d\")") + strlen("Localize("));
	EXPECT_EQ(SelectPresetBody.find("SyncSkinQueueEntriesInPlace("), std::string::npos);

	EXPECT_NE(Menus.find("const int ActivePresetIndex = GameClient()->m_Skins.ActiveSkinQueuePresetIndex(QueueDummy);"), std::string::npos);
	EXPECT_NE(Menus.find("const int AppliedPresetIndex = GameClient()->m_Skins.AppliedSkinQueuePresetIndex(QueueDummy);"), std::string::npos);
	EXPECT_NE(Menus.find("const auto &SkinQueue = GameClient()->m_Skins.ActiveSkinQueue(QueueDummy);"), std::string::npos);
	EXPECT_EQ(Menus.find("GameClient()->m_Skins.TrimActiveSkinQueueToLimit(QueueDummy);"), std::string::npos);
	EXPECT_EQ(Menus.find("Localize(\"Queue capacity\")"), std::string::npos);
	EXPECT_EQ(Menus.find("Localize(\"Editing: %s\")"), std::string::npos);
	EXPECT_EQ(Menus.find("Localize(\"Editing: current queue\")"), std::string::npos);
	EXPECT_NE(Menus.find("Localize(\"Current queue: %s\")"), std::string::npos);
	EXPECT_NE(Menus.find("Localize(\"Custom\")"), std::string::npos);
	EXPECT_NE(Menus.find("Localize(\"Rotate all server player skins\")"), std::string::npos);
	EXPECT_NE(Menus.find("CLineInputNumber &QueueIntervalInput = s_aQueueIntervalInputs[QueueDummy];"), std::string::npos);
	EXPECT_NE(Menus.find("Ui()->DoEditBox(&QueueIntervalInput, &IntervalInput"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.MoveActiveSkinQueueItem("), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.RemoveActiveSkinQueue("), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.AddActiveSkinQueue("), std::string::npos);
	EXPECT_NE(Menus.find("ApplyQueueIndex = s_QueueDragIndex;"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.ApplySkinQueueIndex((size_t)ApplyQueueIndex, QueueDummy);"), std::string::npos);
	EXPECT_NE(PresetsUi.find("const int PresetSelectedOld = ActivePresetIndex >= 0 ? ActivePresetIndex : -1;"), std::string::npos);
	EXPECT_NE(PresetsUi.find("s_PresetListBox.DoStart(20.0f, (int)vQueuePresets.size(), 1, 1, PresetSelectedOld, &PresetList, true, IGraphics::CORNER_ALL);"), std::string::npos);
	EXPECT_NE(PresetsUi.find("if(s_PresetListBox.WasItemSelected())"), std::string::npos);
	EXPECT_EQ(PresetsUi.find("Ui()->DoButtonLogic(&s_vPresetItemIds[i], 0, &SelectRect, BUTTONFLAG_LEFT)"), std::string::npos);
	EXPECT_NE(PresetsUi.find("PresetControlsTop.VSplitLeft(ActionButtonWidth"), std::string::npos);
	EXPECT_NE(PresetsUi.find("PresetControlsTop.VSplitLeft(ActionGapWidth"), std::string::npos);
	EXPECT_EQ(PresetsUi.find("Localize(\"Edit\")"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.SelectSkinQueuePreset((size_t)SelectPresetIndex, QueueDummy);"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.ClearSkinQueuePresetSelection(QueueDummy);"), std::string::npos);
	EXPECT_NE(Menus.find("Localize(\"Apply\")"), std::string::npos);
	EXPECT_NE(Menus.find("Localize(\"Apply this preset to the current queue\")"), std::string::npos);
	EXPECT_NE(Menus.find("GameClient()->m_Skins.ApplySkinQueuePreset((size_t)ActivePresetIndex, QueueDummy);"), std::string::npos);
	EXPECT_NE(Source.find("SyncSkinQueueEntriesInPlace(m_aSkinQueue[Dummy], Presets[PresetIndex].m_Queue.data(), Presets[PresetIndex].m_Queue.size())"), std::string::npos);
}

TEST(Skins, SkinQueuePresetCompatibilityKeepsLimitAndIgnoresLegacyDummyPresetCommands)
{
	std::ifstream File(TestSourcePath("src/game/client/components/skins.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t AddQueuePos = Source.find("bool CSkins::AddSkinQueue(const char *pName, bool UseCustomColor, int ColorBody, int ColorFeet, int Dummy)");
	ASSERT_NE(AddQueuePos, std::string::npos);
	const size_t AddQueueEnd = Source.find("bool CSkins::AddActiveSkinQueue", AddQueuePos);
	ASSERT_NE(AddQueueEnd, std::string::npos);
	const std::string AddQueueBody = Source.substr(AddQueuePos, AddQueueEnd - AddQueuePos);
	const size_t AddActivePos = AddQueueEnd;
	const size_t AddActiveEnd = Source.find("bool CSkins::RemoveSkinQueue", AddActivePos);
	ASSERT_NE(AddActiveEnd, std::string::npos);
	const std::string AddActiveBody = Source.substr(AddActivePos, AddActiveEnd - AddActivePos);
	const size_t AddPresetItemPos = Source.find("bool CSkins::AddSkinQueuePresetItem(int PresetIndex, const char *pSkinName, bool UseCustomColor, int ColorBody, int ColorFeet, int Dummy)");
	ASSERT_NE(AddPresetItemPos, std::string::npos);
	const size_t AddPresetItemEnd = Source.find("bool CSkins::AddSkinQueuePresetFromCurrent", AddPresetItemPos);
	ASSERT_NE(AddPresetItemEnd, std::string::npos);
	const std::string AddPresetItemBody = Source.substr(AddPresetItemPos, AddPresetItemEnd - AddPresetItemPos);
	const size_t DummyPresetPos = Source.find("void CSkins::ConAddDummySkinQueuePreset(IConsole::IResult *pResult, void *pUserData)");
	ASSERT_NE(DummyPresetPos, std::string::npos);
	const size_t DummyPresetEnd = Source.find("void CSkins::ConAddSkinQueuePresetItem", DummyPresetPos);
	ASSERT_NE(DummyPresetEnd, std::string::npos);
	const std::string DummyPresetBody = Source.substr(DummyPresetPos, DummyPresetEnd - DummyPresetPos);
	const size_t DummyPresetItemPos = Source.find("void CSkins::ConAddDummySkinQueuePresetItem(IConsole::IResult *pResult, void *pUserData)");
	ASSERT_NE(DummyPresetItemPos, std::string::npos);
	const size_t DummyPresetItemEnd = Source.find("void CSkins::ConAddSkinQueuePresetItemEx", DummyPresetItemPos);
	ASSERT_NE(DummyPresetItemEnd, std::string::npos);
	const std::string DummyPresetItemBody = Source.substr(DummyPresetItemPos, DummyPresetItemEnd - DummyPresetItemPos);
	const size_t DummyPresetItemExPos = Source.find("void CSkins::ConAddDummySkinQueuePresetItemEx(IConsole::IResult *pResult, void *pUserData)");
	ASSERT_NE(DummyPresetItemExPos, std::string::npos);
	const size_t DummyPresetItemExEnd = Source.find("void CSkins::ConfigSaveCallback", DummyPresetItemExPos);
	ASSERT_NE(DummyPresetItemExEnd, std::string::npos);
	const std::string DummyPresetItemExBody = Source.substr(DummyPresetItemExPos, DummyPresetItemExEnd - DummyPresetItemExPos);
	const size_t SavePos = Source.find("void CSkins::OnQueueConfigSave(IConfigManager *pConfigManager)");
	ASSERT_NE(SavePos, std::string::npos);
	const std::string SaveBody = Source.substr(SavePos);

	EXPECT_NE(AddQueueBody.find("const int Limit = minimum(SKIN_QUEUE_HARD_LIMIT, maximum(0, SkinQueueLengthVar(Dummy)));"), std::string::npos);
	EXPECT_NE(AddQueueBody.find("if((int)Queue.size() >= Limit)"), std::string::npos);
	EXPECT_NE(AddActiveBody.find("const int Limit = minimum(SKIN_QUEUE_HARD_LIMIT, maximum(0, SkinQueueLengthVar(Dummy)));"), std::string::npos);
	EXPECT_NE(AddActiveBody.find("if((int)Queue.size() >= Limit)"), std::string::npos);
	EXPECT_NE(AddPresetItemBody.find("const int Limit = minimum(SKIN_QUEUE_HARD_LIMIT, maximum(0, SkinQueueLengthVar(Dummy)));"), std::string::npos);
	EXPECT_NE(AddPresetItemBody.find("if((int)Queue.size() >= Limit)"), std::string::npos);

	EXPECT_EQ(DummyPresetBody.find("AddSkinQueuePreset("), std::string::npos);
	EXPECT_EQ(DummyPresetItemBody.find("AddSkinQueuePresetItem("), std::string::npos);
	EXPECT_EQ(DummyPresetItemExBody.find("AddSkinQueuePresetItem("), std::string::npos);
	EXPECT_NE(DummyPresetBody.find("log_info(\"skins\""), std::string::npos);
	EXPECT_NE(DummyPresetItemBody.find("log_info(\"skins\""), std::string::npos);
	EXPECT_NE(DummyPresetItemExBody.find("log_info(\"skins\""), std::string::npos);
	EXPECT_EQ(SaveBody.find("add_dummy_skin_queue_preset"), std::string::npos);
}

TEST(Skins, WebPSaveRoundTripPreservesImageShape)
{
	CImageInfo Image = MakeTestSkinImage(4, 4);
	SetTestPixel(Image, 0, 0, 255, 0, 0, 255);
	SetTestPixel(Image, 3, 0, 0, 255, 0, 255);
	SetTestPixel(Image, 0, 3, 0, 0, 255, 255);
	SetTestPixel(Image, 3, 3, 255, 255, 255, 255);

	CByteBufferWriter Writer;
	EXPECT_TRUE(CImageLoader::SaveWebP(Writer, Image));

	CImageInfo Reloaded;
	EXPECT_TRUE(CImageLoader::LoadWebP(Writer.Data(), Writer.Size(), "skins-test-webp", Reloaded));
	EXPECT_EQ(Reloaded.m_Width, 4u);
	EXPECT_EQ(Reloaded.m_Height, 4u);
	EXPECT_EQ(Reloaded.m_Format, CImageInfo::FORMAT_RGBA);

	Reloaded.Free();
	Image.Free();
}

TEST(Skins, TeePreviewLayerFlagsAreDistinctBits)
{
	EXPECT_NE(TEE_PREVIEW_LAYER_BODY_OUTLINE, TEE_PREVIEW_LAYER_BODY);
	EXPECT_NE(TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE, TEE_PREVIEW_LAYER_BACK_FEET);
	EXPECT_NE(TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE, TEE_PREVIEW_LAYER_FRONT_FEET);
	EXPECT_NE(TEE_PREVIEW_LAYER_OUTLINE, TEE_PREVIEW_LAYER_BODY);
	EXPECT_NE(TEE_PREVIEW_LAYER_BODY, TEE_PREVIEW_LAYER_FEET);
	EXPECT_NE(TEE_PREVIEW_LAYER_FEET, TEE_PREVIEW_LAYER_EYES);
}

TEST(Skins, TeePreviewLayerFlagsDefaultToAllLayers)
{
	EXPECT_EQ(ResolveTeePreviewLayers(0), TEE_PREVIEW_LAYER_ALL);
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_OUTLINE));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_BODY_OUTLINE));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_BODY));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_FEET));
	EXPECT_TRUE(HasTeePreviewLayer(0, TEE_PREVIEW_LAYER_EYES));
}

TEST(Skins, TeePreviewLayerFlagsRespectExplicitMask)
{
	const int Mask = TEE_PREVIEW_LAYER_BODY | TEE_PREVIEW_LAYER_FEET;
	EXPECT_TRUE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_BODY));
	EXPECT_TRUE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_FEET));
	EXPECT_FALSE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_OUTLINE));
	EXPECT_FALSE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_EYES));
}

TEST(Skins, TeePreviewLayerOutlineMasksDoNotSelectFillLayers)
{
	const int Mask = TEE_PREVIEW_LAYER_BODY_OUTLINE | TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE;
	EXPECT_TRUE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_BODY_OUTLINE));
	EXPECT_TRUE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_BACK_FEET_OUTLINE));
	EXPECT_FALSE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_BODY));
	EXPECT_FALSE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_BACK_FEET));
	EXPECT_FALSE(HasTeePreviewLayer(Mask, TEE_PREVIEW_LAYER_FRONT_FEET_OUTLINE));
}
