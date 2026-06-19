#include <game/client/components/assets_author_persistence.h>
#include <game/client/components/assets_preview_scale.h>
#include <game/client/components/assets_resource_registry.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <fstream>
#include <sstream>

TEST(AssetsPreviewScale, LocalPreviewCapKeeps4kImagesAtNativeResolution)
{
	const auto Scaled = ComputePreviewTargetSize(4096, 2048, LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE);
	EXPECT_EQ(Scaled.m_Width, 4096);
	EXPECT_EQ(Scaled.m_Height, 2048);
	EXPECT_FALSE(Scaled.m_Resized);
}

TEST(AssetsPreviewScale, WorkshopPreviewCapKeeps2kImagesAtNativeResolution)
{
	const auto Scaled = ComputePreviewTargetSize(2048, 1024, WORKSHOP_ASSET_PREVIEW_MAX_TEXTURE_SIZE);
	EXPECT_EQ(Scaled.m_Width, 2048);
	EXPECT_EQ(Scaled.m_Height, 1024);
	EXPECT_FALSE(Scaled.m_Resized);
}

TEST(AssetsPreviewScale, LocalPreviewStillClampsTo4096)
{
	const auto Scaled = ComputePreviewTargetSize(8192, 4096, LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE);
	EXPECT_EQ(Scaled.m_Width, 4096);
	EXPECT_EQ(Scaled.m_Height, 2048);
	EXPECT_TRUE(Scaled.m_Resized);
}

TEST(AssetsPreviewScale, WorkshopPreviewStillClampsTo2048)
{
	const auto Scaled = ComputePreviewTargetSize(4096, 2048, WORKSHOP_ASSET_PREVIEW_MAX_TEXTURE_SIZE);
	EXPECT_EQ(Scaled.m_Width, 2048);
	EXPECT_EQ(Scaled.m_Height, 1024);
	EXPECT_TRUE(Scaled.m_Resized);
}

TEST(AssetsPreviewScale, DoesNotUpscaleSmallImages)
{
	const auto Scaled = ComputePreviewTargetSize(320, 160, LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE);
	EXPECT_EQ(Scaled.m_Width, 320);
	EXPECT_EQ(Scaled.m_Height, 160);
}

TEST(AssetsPreviewScale, PreviewBudgetUsesOverrideBeforeDevicePercent)
{
	EXPECT_EQ(PreviewBudgetBytes(192, 8, 4096.0f), 192ull * 1024ull * 1024ull);
}

TEST(AssetsPreviewScale, PreviewBudgetUsesDeviceBudgetPercentWhenAvailable)
{
	EXPECT_EQ(PreviewBudgetBytes(0, 8, 4096.0f), 342884352u);
	EXPECT_EQ(PreviewBudgetBytes(0, 0, 4096.0f), 0u);
	EXPECT_EQ(PreviewBudgetBytes(0, 8, -1.0f), 0u);
}

TEST(AssetsPreviewScale, LocalPreviewBudgetDropsFrom4kTo2kWhenTextureBudgetWouldBeExceeded)
{
	const size_t BudgetBytes = 32ull * 1024ull * 1024ull;
	const size_t CurrentTextureMemoryBytes = 0;
	const size_t ResidentPreviewBytes = 8ull * 1024ull * 1024ull;
	EXPECT_EQ(ComputePreviewBudgetedTextureSize(LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, BudgetBytes, CurrentTextureMemoryBytes, ResidentPreviewBytes), 2048);
}

TEST(AssetsPreviewScale, LocalPreviewBudgetDoesNotDoubleCountCurrentResidentTextureWhenReplacingSameItem)
{
	const size_t BudgetBytes = 80ull * 1024ull * 1024ull;
	const size_t CurrentTextureMemoryBytes = 64ull * 1024ull * 1024ull;
	const size_t CurrentItemResidentBytes = 64ull * 1024ull * 1024ull;
	EXPECT_EQ(ComputePreviewBudgetedTextureSize(LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, BudgetBytes, CurrentTextureMemoryBytes, CurrentItemResidentBytes), 4096);
}

TEST(AssetsPreviewScale, PreviewBudgetTreatsOnlyCurrentItemAsReplaceableResidentBytes)
{
	const size_t BudgetBytes = 64ull * 1024ull * 1024ull;
	const size_t CurrentTextureMemoryBytes = 24ull * 1024ull * 1024ull;
	const size_t CurrentItemResidentBytes = 16ull * 1024ull * 1024ull;
	const size_t AllResidentPreviewBytes = 24ull * 1024ull * 1024ull;
	EXPECT_EQ(ComputePreviewBudgetedTextureSize(LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, BudgetBytes, CurrentTextureMemoryBytes, CurrentItemResidentBytes), 2048);
	EXPECT_EQ(ComputePreviewBudgetedTextureSize(LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, BudgetBytes, CurrentTextureMemoryBytes, AllResidentPreviewBytes), 4096);
}

TEST(AssetsPreviewScale, WorkshopPreviewBudgetNeverDropsBelow512Floor)
{
	const size_t BudgetBytes = 1ull * 1024ull * 1024ull;
	EXPECT_EQ(ComputePreviewBudgetedTextureSize(WORKSHOP_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, BudgetBytes, 0, 0), 512);
}

TEST(AssetsPreviewScale, UploadBudgetCapsPreviewTextureSize)
{
	const int TextureSize = PreviewUploadBudgetTextureSize(LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE, ASSET_PREVIEW_MIN_TEXTURE_SIZE, ASSET_PREVIEW_UPLOAD_MAX_BYTES_PER_FRAME);
	EXPECT_EQ(TextureSize, ASSET_PREVIEW_MIN_TEXTURE_SIZE);
	EXPECT_LE(PreviewTextureSizeBytesEstimate(TextureSize), ASSET_PREVIEW_UPLOAD_MAX_BYTES_PER_FRAME);
}

TEST(AssetsPreviewScale, EntityBgWorkshopRequiresSeparateDownloadUrl)
{
	EXPECT_FALSE(WorkshopEntityBgAllowsImageUrlFallback("entity_bg"));
	EXPECT_TRUE(WorkshopEntityBgAllowsImageUrlFallback("game"));
	EXPECT_FALSE(WorkshopAssetHasRequiredDownloadUrl("entity_bg", false));
	EXPECT_TRUE(WorkshopAssetHasRequiredDownloadUrl("entity_bg", true));
	EXPECT_TRUE(WorkshopAssetHasRequiredDownloadUrl("game", false));
	EXPECT_FALSE(WorkshopInstalledAssetUsesInstallPreviewSource("entity_bg"));
	EXPECT_TRUE(WorkshopInstalledAssetUsesInstallPreviewSource("game"));
	EXPECT_FALSE(WorkshopAssetPreviewDecodeUsesInstallSource("entity_bg", true));
	EXPECT_TRUE(WorkshopAssetPreviewDecodeUsesInstallSource("game", true));
	EXPECT_FALSE(WorkshopAssetPreviewDecodeUsesInstallSource("game", false));
	EXPECT_FALSE(WorkshopAssetCanDecodeInstalledPreview("entity_bg", true, false));
	EXPECT_TRUE(WorkshopAssetCanDecodeInstalledPreview("game", true, false));
	EXPECT_FALSE(WorkshopAssetCanDecodeInstalledPreview("game", false, false));
	EXPECT_FALSE(WorkshopAssetCanDecodeInstalledPreview("game", true, true));
	EXPECT_TRUE(WorkshopAssetCanDecodeAnyLocalPreview("entity_bg", true, false, true));
	EXPECT_FALSE(WorkshopAssetCanDecodeAnyLocalPreview("entity_bg", true, false, false));
}

TEST(AssetsPreviewScale, InstalledWorkshopEntityBgReusedThumbStateOnlyDecodesThumbCache)
{
	const auto EntityBgPlan = BuildWorkshopPreviewDecodeSourcePlan("entity_bg", true, true);
	EXPECT_FALSE(EntityBgPlan.m_UseInstallSource);
	EXPECT_TRUE(EntityBgPlan.m_UseThumbCache);

	const auto EntityBgWithoutThumbPlan = BuildWorkshopPreviewDecodeSourcePlan("entity_bg", true, false);
	EXPECT_FALSE(EntityBgWithoutThumbPlan.m_UseInstallSource);
	EXPECT_FALSE(EntityBgWithoutThumbPlan.m_UseThumbCache);

	const auto GamePlan = BuildWorkshopPreviewDecodeSourcePlan("game", true, true);
	EXPECT_TRUE(GamePlan.m_UseInstallSource);
	EXPECT_TRUE(GamePlan.m_UseThumbCache);
}

TEST(AssetsPreviewScale, EntityBgCorruptInstallDetectionRecognizesPreviewHeaders)
{
	static const unsigned char s_aPng[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	static const unsigned char s_aWebp[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
	static const unsigned char s_aMap[] = {'D', 'A', 'T', 'A', 0, 0, 0, 0};

	EXPECT_TRUE(DetectCorruptEntityBgInstallHeader(s_aPng, sizeof(s_aPng)));
	EXPECT_TRUE(DetectCorruptEntityBgInstallHeader(s_aWebp, sizeof(s_aWebp)));
	EXPECT_FALSE(DetectCorruptEntityBgInstallHeader(s_aMap, sizeof(s_aMap)));
	EXPECT_FALSE(DetectCorruptEntityBgInstallHeader(nullptr, 0));
}

TEST(AssetsPreviewScale, LocalAuthorPersistenceTabRoundTripsCategoryIds)
{
	EXPECT_TRUE(SupportsPersistedLocalAssetAuthorCategory("entities"));
	EXPECT_TRUE(SupportsPersistedLocalAssetAuthorCategory("game"));
	EXPECT_TRUE(SupportsPersistedLocalAssetAuthorCategory("hud"));
	EXPECT_TRUE(SupportsPersistedLocalAssetAuthorCategory("entity_bg"));
	EXPECT_TRUE(SupportsPersistedLocalAssetAuthorCategory("extras"));
	EXPECT_FALSE(SupportsPersistedLocalAssetAuthorCategory("missing"));
}

TEST(AssetsPreviewScale, LocalAuthorPersistenceBuildsStablePerTabKeys)
{
	EXPECT_EQ(BuildPersistedLocalAssetAuthorKey("entities", "default"), "entities:default");
	EXPECT_EQ(BuildPersistedLocalAssetAuthorKey("game", "default"), "game:default");
	EXPECT_NE(BuildPersistedLocalAssetAuthorKey("entities", "same"), BuildPersistedLocalAssetAuthorKey("game", "same"));
	EXPECT_TRUE(BuildPersistedLocalAssetAuthorKey(nullptr, "same").empty());
	EXPECT_TRUE(BuildPersistedLocalAssetAuthorKey("entities", "").empty());
}

TEST(AssetsPreviewScale, LocalAuthorPersistencePrimarySourcePathsFollowActualAssetLoadRules)
{
	const SAssetResourceCategory *pGame = FindAssetResourceCategory("game");
	const SAssetResourceCategory *pCursor = FindAssetResourceCategory("gui_cursor");
	const SAssetResourceCategory *pEntityBg = FindAssetResourceCategory("entity_bg");
	const SAssetResourceCategory *pExtras = FindAssetResourceCategory("extras");
	ASSERT_NE(pGame, nullptr);
	ASSERT_NE(pCursor, nullptr);
	ASSERT_NE(pEntityBg, nullptr);
	ASSERT_NE(pExtras, nullptr);

	const auto GamePaths = LocalAssetPrimarySourcePaths(*pGame, "foo");
	EXPECT_EQ(GamePaths.m_PrimaryPath, "assets/game/foo.png");
	EXPECT_EQ(GamePaths.m_FallbackPath, "assets/game/foo/game.png");

	const auto CursorPaths = LocalAssetPrimarySourcePaths(*pCursor, "foo");
	EXPECT_EQ(CursorPaths.m_PrimaryPath, "assets/gui_cursor/foo.png");
	EXPECT_TRUE(CursorPaths.m_FallbackPath.empty());

	const auto EntityBgPaths = LocalAssetPrimarySourcePaths(*pEntityBg, "foo");
	EXPECT_EQ(EntityBgPaths.m_PrimaryPath, "maps/foo.map");
	EXPECT_TRUE(EntityBgPaths.m_FallbackPath.empty());

	const auto ManagedEntityBgPaths = LocalAssetPrimarySourcePaths(*pEntityBg, "entity_bg/foo");
	EXPECT_EQ(ManagedEntityBgPaths.m_PrimaryPath, "assets/entity_bg/foo.map");
	EXPECT_EQ(ManagedEntityBgPaths.m_FallbackPath, "maps/entity_bg/foo.map");

	const auto ExtrasPaths = LocalAssetPrimarySourcePaths(*pExtras, "foo");
	EXPECT_EQ(ExtrasPaths.m_PrimaryPath, "assets/extras/foo.png");
	EXPECT_EQ(ExtrasPaths.m_FallbackPath, "assets/extras/foo/extras.png");
}

TEST(AssetsPreviewScale, LocalAuthorPersistenceResolvesPrimarySourcePathUsingRuntimeSelectionRules)
{
	const SAssetResourceCategory *pGame = FindAssetResourceCategory("game");
	const SAssetResourceCategory *pEntityBg = FindAssetResourceCategory("entity_bg");
	ASSERT_NE(pGame, nullptr);
	ASSERT_NE(pEntityBg, nullptr);

	EXPECT_EQ(ResolveLocalAssetPrimarySourcePath(*pGame, "foo", true, true), "assets/game/foo.png");
	EXPECT_EQ(ResolveLocalAssetPrimarySourcePath(*pGame, "foo", false, true), "assets/game/foo/game.png");

	EXPECT_EQ(ResolveLocalAssetPrimarySourcePath(*pEntityBg, "foo", true, false), "maps/foo.map");
	EXPECT_EQ(ResolveLocalAssetPrimarySourcePath(*pEntityBg, "entity_bg/foo", true, false), "assets/entity_bg/foo.map");
	EXPECT_EQ(ResolveLocalAssetPrimarySourcePath(*pEntityBg, "entity_bg/foo", false, true), "maps/entity_bg/foo.map");
}

TEST(AssetsPreviewScale, WorkshopThumbDecodeResizesOffMainThread)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("Asset.m_pDecodeJob = std::make_shared<CFullAsyncImageLoadJob>(std::move(vPossiblePaths), pStorage, Asset.m_Name.c_str(), IStorage::TYPE_SAVE, MaxTextureSize);"), std::string::npos);
	EXPECT_NE(Source.find("SettingsAssetPreviewBudgetedTextureSize("), std::string::npos);
	EXPECT_NE(Source.find("const SWorkshopPreviewDecodeSourcePlan SourcePlan = BuildWorkshopPreviewDecodeSourcePlan(pCategoryId, Asset.m_Installed, !Asset.m_ThumbCachePath.empty());"), std::string::npos);
	EXPECT_NE(Source.find("if(SourcePlan.m_UseInstallSource && !Asset.m_InstallPath.empty())"), std::string::npos);
	EXPECT_NE(Source.find("if(SourcePlan.m_UseThumbCache)"), std::string::npos);
	EXPECT_EQ(Source.find("const SPreviewTargetSize TargetSize = ComputePreviewTargetSize(Result.m_Image.m_Width, Result.m_Image.m_Height, WORKSHOP_ASSET_PREVIEW_MAX_TEXTURE_SIZE);"), std::string::npos);
}

TEST(AssetsPreviewScale, EntitiesPreviewTileSizeIsClampedByBothAxes)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("float TileSize = minimum(PreviewRect.w / (float)COLS, PreviewRect.h / (float)ROWS);"), std::string::npos);
	EXPECT_EQ(Source.find("float TileSize = PreviewRect.w / (float)COLS;"), std::string::npos);
}

TEST(AssetsPreviewScale, CursorAndArrowPreviewCardsUseSmallerContentBounds)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("else if(s_CurCustomTab == ASSETS_TAB_GUI_CURSOR)\n\t{\n\t\tSearchListSize = gs_vpSearchGuiCursorList.size();\n\t\tTextureWidth = 96;\n\t\tTextureHeight = 96;"), std::string::npos);
	EXPECT_NE(Source.find("else if(s_CurCustomTab == ASSETS_TAB_ARROW)\n\t{\n\t\tSearchListSize = gs_vpSearchArrowList.size();\n\t\tTextureWidth = 96;\n\t\tTextureHeight = 96;"), std::string::npos);
}

TEST(AssetsPreviewScale, WorkshopAndLocalCardsUseSharedPreviewContentSizing)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("auto ComputeAssetPreviewContentSize = [&](bool WorkshopCard)"), std::string::npos);
	EXPECT_NE(Source.find("const float TileContentSize = WorkshopCard ? 112.0f : 104.0f;"), std::string::npos);
	EXPECT_NE(Source.find("ContentWidth = 76.0f;"), std::string::npos);
	EXPECT_NE(Source.find("ResolveLocalAssetStatusLabel(pItem, ShowLocalOnlyBadge)"), std::string::npos);
	EXPECT_NE(Source.find("LayoutAssetsCardShell(CardRect, HasDeleteButton, pLocalStatusLabel, ShowLocalOnlyBadge, ShowAuthorRow)"), std::string::npos);
	EXPECT_NE(Source.find("TitleProps.m_StopAtEnd = true;"), std::string::npos);
	EXPECT_NE(Source.find("TitleProps.m_EllipsisAtEnd = true;"), std::string::npos);
	EXPECT_NE(Source.find("AuthorProps.m_StopAtEnd = true;"), std::string::npos);
	EXPECT_NE(Source.find("AuthorProps.m_EllipsisAtEnd = true;"), std::string::npos);
}

TEST(AssetsPreviewScale, PreviewFrameAppliesInnerContentInset)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("PreviewFrame.Margin(3.0f, &PreviewFrame);"), std::string::npos);
	EXPECT_NE(Source.find("if(s_CurCustomTab != ASSETS_TAB_GAME && s_CurCustomTab != ASSETS_TAB_STRONG_WEAK)\n\t\t\tPreviewFrame.Margin(8.0f, &PreviewFrame);"), std::string::npos);
}

TEST(AssetsPreviewScale, WorkshopRootFolderUsesPinkAccent)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("const bool IsWorkshopRootFolder = IsEntityBgDirectory &&"), std::string::npos);
	EXPECT_NE(Source.find("TextRender()->TextColor(ColorRGBA(1.0f, 0.78f, 0.78f, 1.0f));"), std::string::npos);
	EXPECT_NE(Source.find("IsEntityBgWorkshopFolderPath(pItem->m_aName);"), std::string::npos);
}

TEST(AssetsPreviewScale, StatusTagTextUsesSingleLineShrinkBeforeWrapping)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("StatusLabelProps.m_StopAtEnd = true;"), std::string::npos);
	EXPECT_NE(Source.find("StatusLabelProps.m_EllipsisAtEnd = true;"), std::string::npos);
}

TEST(AssetsPreviewScale, AssetCardHeaderPrioritizesRightSideControls)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("constexpr float AssetCardHeaderMargin = 3.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetCardHeaderControlMargin = 1.0f;"), std::string::npos);
	EXPECT_NE(Source.find("TitleRect.VSplitRight(16.0f, &TitleRect, &Shell.m_ActionButtonRect);"), std::string::npos);
	EXPECT_NE(Source.find("const float BadgeGap = 2.0f;"), std::string::npos);
	EXPECT_NE(Source.find("const float TitleMinWidth = 12.0f;"), std::string::npos);
	EXPECT_NE(Source.find("return pWorkshopAsset != nullptr ? Localize(\"Downloaded\") : Localize(\"Local\");"), std::string::npos);
}

TEST(AssetsPreviewScale, LocalAssetCardsUsePersistedAuthorMetadata)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	std::ifstream HelperFile(TestSourcePath("src/game/client/components/assets_author_persistence.h"));
	ASSERT_TRUE(File.good());
	ASSERT_TRUE(HelperFile.good());
	std::stringstream Buffer;
	std::stringstream HelperBuffer;
	Buffer << File.rdbuf();
	HelperBuffer << HelperFile.rdbuf();
	const std::string Source = Buffer.str();
	const std::string HelperSource = HelperBuffer.str();

	EXPECT_NE(HelperSource.find("qmclient/workshop/local_asset_authors.json"), std::string::npos);
	EXPECT_NE(Source.find("FindPersistedLocalAssetAuthor("), std::string::npos);
	EXPECT_NE(Source.find("pItem->m_aAuthor[0] != '\\0' ? pItem->m_aAuthor : \"--\""), std::string::npos);
	EXPECT_NE(Source.find("\\\"content_hash\\\":\\\""), std::string::npos);
	EXPECT_NE(Source.find("TryGetLocalAssetContentHash("), std::string::npos);
	EXPECT_EQ(Source.find("else if(TryGetLocalAssetContentHash(pStorage, Tab, pLocalName, CurrentContentHash))"), std::string::npos);
	EXPECT_NE(Source.find("const char *pAuthor = pAsset->m_aAuthor;"), std::string::npos);
	EXPECT_NE(Source.find("PopulateLocalAssetAuthor(Item, ASSETS_TAB_ENTITY_BG, Storage());"), std::string::npos);
	EXPECT_NE(Source.find("Asset.m_Installed = true;\n\t\t\t\t\tPersistLocalAssetAuthorForWorkshopAsset(s_CurCustomTab, Asset, Storage());"), std::string::npos);
	EXPECT_NE(Source.find("FlushPersistedLocalAssetAuthorsIfDirty(Storage(), s_CurCustomTab);"), std::string::npos);
	EXPECT_NE(Source.find("\\\"tab\\\":\\\""), std::string::npos);
	EXPECT_NE(Source.find("SupportsPersistedLocalAssetAuthor(Tab)"), std::string::npos);
	EXPECT_NE(Source.find("m_vContentHashByKey"), std::string::npos);
	EXPECT_EQ(Source.find("PersistLocalAssetAuthor(Tab, Item.m_aName, pWorkshopAuthor, pStorage);"), std::string::npos);
	EXPECT_EQ(Source.find("FindWorkshopAuthorByLocalName(pWorkshopState, Item.m_aName)"), std::string::npos);
	EXPECT_EQ(Source.find("\"modified\":"), std::string::npos);
}

TEST(AssetsPreviewScale, WorkshopMergeDoesNotReappendExistingUninstalledAssets)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("ExistingAsset = std::move(NewAsset);"), std::string::npos);
	EXPECT_NE(Source.find("continue;"), std::string::npos);
	EXPECT_NE(Source.find("WorkshopState.m_vAssets.push_back(std::move(NewAsset));"), std::string::npos);
}

TEST(AssetsPreviewScale, InstalledWorkshopEntityBgThumbTakeoverSkipsLocalDecodeFallback)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("QueueWorkshopReadyThumb(State, *pAsset, CurTab);\n\t\t\treturn true;"), std::string::npos);
	EXPECT_NE(Source.find("QueueWorkshopDecodeThumb(State, *pAsset, CurTab);\n\t\t\treturn true;"), std::string::npos);
	EXPECT_NE(Source.find("if(pAsset->m_pThumbTask)\n\t\t\treturn true;"), std::string::npos);
	EXPECT_NE(Source.find("++ThumbStartsThisFrame;\n\t\tQueueWorkshopDecodeThumb(State, *pAsset, CurTab);\n\t\treturn true;"), std::string::npos);
}

TEST(AssetsPreviewScale, InstalledWorkshopEntityBgWithoutThumbCacheFallsBackToRemotePreview)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings_assets.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("const bool HasUsableInstalledThumb = WorkshopAssetCanDecodePreviewFromInstall(*pAsset, CurTab);"), std::string::npos);
	EXPECT_NE(Source.find("return StartWorkshopRemoteThumbRequest(*pAsset, CurTab, PreviewEpoch, TargetTextureSize, ThumbStartsThisFrame, pStorage, pHttp);"), std::string::npos);
	EXPECT_NE(Source.find("const bool HasUsableInstalledThumb = WorkshopAssetCanDecodePreviewFromInstall(Asset, s_CurCustomTab);"), std::string::npos);
	EXPECT_NE(Source.find("if(!StartWorkshopRemoteThumbRequest(Asset, s_CurCustomTab, PreviewEpoch, TargetTextureSize, WorkshopThumbStartsThisFrame, Storage(), Http()))"), std::string::npos);
}
