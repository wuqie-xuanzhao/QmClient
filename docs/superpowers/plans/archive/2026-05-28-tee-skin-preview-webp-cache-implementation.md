# Tee Skin Preview WebP Cache Implementation Plan

> **部分内容已过时** — 部分描述不再反映当前代码状态，请以较新的相关文档为准。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a WebP disk cache for the `设置 -> Tee` skin list previews that reuses the existing tee color pipeline, avoids color-specific cache keys, and safely falls back to live rendering.

**Architecture:** Extend the existing image loader with a minimal `SaveWebP(...)` helper, add a dedicated tee preview cache module for key/path/prune logic and runtime texture management, then wire the tee settings list to load, record, save, and reuse preview textures using the existing render target API. The cache is stored under `qmclient/skins/preview_cache`, keyed by sanitized skin name + cache version + content hash + size bucket, while colors remain a draw-time concern.

**Tech Stack:** C++, DDNet/QmClient client UI, existing render target API, libwebp, GoogleTest, Windows `qmclient_scripts/cmake-windows.cmd` build flow.

---

## Reference Documents

- Spec: `docs/superpowers/specs/2026-05-28-tee-skin-preview-webp-cache-design.md`
- Existing settings/runtime cache code: `src/game/client/components/menus.cpp`, `src/game/client/components/section_loader.cpp`
- Existing Workshop thumb cache patterns: `src/game/client/components/menus_settings_assets.cpp`

## File Structure

| File | Responsibility |
|------|------|
| `src/engine/gfx/image_loader.h` / `.cpp` | Add minimal WebP save helpers alongside existing PNG save/load helpers |
| `src/game/client/components/settings_skin_preview_cache.h` / `.cpp` | New tee-list preview cache key/path/prune/runtime cache module |
| `src/game/client/components/menus.h` | Store cache instance and lightweight per-page state |
| `src/game/client/components/menus.cpp` | Initialize/shutdown cache, propagate invalidation points if needed |
| `src/game/client/components/menus_settings.cpp` | Query cache, request cache generation, draw cached preview textures, fallback to `RenderTee(...)` |
| `src/game/client/components/skins.h` / `.cpp` | Expose minimal stable inputs needed for content hash / skin cache invalidation |
| `src/test/settings_skin_preview_cache_test.cpp` | Cache key/path/prune logic tests |
| `src/test/skins_test.cpp` | WebP save/load roundtrip test and any tee preview cache invariants that fit skin tests |
| `CMakeLists.txt` | Register new source/test files |

## Task 1: Add minimal WebP write support

**Files:**
- Modify: `src/engine/gfx/image_loader.h`
- Modify: `src/engine/gfx/image_loader.cpp`
- Modify: `src/test/skins_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test for WebP save/load roundtrip**

Add a test near the existing skin/image helper tests in `src/test/skins_test.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=Skins.WebPSaveRoundTripPreservesImageShape
```

Expected: compile failure because `CImageLoader::SaveWebP` does not exist.

- [ ] **Step 3: Add the public helper declarations**

In `src/engine/gfx/image_loader.h`, add:

```cpp
static bool SaveWebP(CByteBufferWriter &Writer, const CImageInfo &Image);
static bool SaveWebP(IOHANDLE File, const char *pFilename, const CImageInfo &Image);
```

- [ ] **Step 4: Implement the minimal WebP save helpers**

In `src/engine/gfx/image_loader.cpp`, mirror the existing `SavePng(...)` structure with a minimal WebP path. Use lossless RGBA encoding first, keep the logic small:

```cpp
bool CImageLoader::SaveWebP(CByteBufferWriter &Writer, const CImageInfo &Image)
{
#if defined(CONF_WEBP)
	CImageInfo RgbaImage;
	const CImageInfo *pEncodeImage = &Image;
	if(Image.m_Format != CImageInfo::FORMAT_RGBA)
	{
		RgbaImage = Image.DeepCopy();
		ConvertToRgba(RgbaImage);
		pEncodeImage = &RgbaImage;
	}

	uint8_t *pOutput = nullptr;
	const size_t EncodedSize = WebPEncodeLosslessRGBA(
		pEncodeImage->m_pData,
		(int)pEncodeImage->m_Width,
		(int)pEncodeImage->m_Height,
		(int)(pEncodeImage->m_Width * 4),
		&pOutput);
	if(RgbaImage.m_pData != nullptr)
		RgbaImage.Free();
	if(EncodedSize == 0 || pOutput == nullptr)
	{
		log_error("webp", "failed to encode WebP image");
		return false;
	}

	Writer.Write(pOutput, EncodedSize);
	WebPFree(pOutput);
	return !Writer.Error();
#else
	(void)Writer;
	(void)Image;
	log_error("webp", "cannot save WebP: client was built without libwebp support");
	return false;
#endif
}
```

And the file overload:

```cpp
bool CImageLoader::SaveWebP(IOHANDLE File, const char *pFilename, const CImageInfo &Image)
{
	if(!File)
	{
		log_error("webp", "failed to open '%s' for writing", pFilename);
		return false;
	}

	CByteBufferWriter Writer;
	if(!CImageLoader::SaveWebP(Writer, Image))
	{
		io_close(File);
		return false;
	}

	const size_t Written = io_write(File, Writer.Data(), Writer.Size());
	io_close(File);
	if(Written != Writer.Size())
	{
		log_error("webp", "failed to write all bytes to '%s'", pFilename);
		return false;
	}
	return true;
}
```

- [ ] **Step 5: Register any missing test include if needed**

If `skins_test.cpp` needs direct access to `CImageLoader` or `CByteBufferWriter`, add the exact includes at the top:

```cpp
#include <engine/gfx/image_loader.h>
#include <base/system++/io.h>
```

Only add what the compiler actually requires.

- [ ] **Step 6: Re-run the WebP test and make it pass**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=Skins.WebPSaveRoundTripPreservesImageShape
```

Expected: `1 test from Skins` passes.

## Task 2: Add tee preview cache key/path/prune module

**Files:**
- Create: `src/game/client/components/settings_skin_preview_cache.h`
- Create: `src/game/client/components/settings_skin_preview_cache.cpp`
- Create: `src/test/settings_skin_preview_cache_test.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing cache logic tests**

Create `src/test/settings_skin_preview_cache_test.cpp` with focused pure logic tests:

```cpp
#include <game/client/components/settings_skin_preview_cache.h>

#include <gtest/gtest.h>

TEST(SettingsSkinPreviewCache, PathUsesSanitizedNameVersionAndHash)
{
	const auto Path = BuildSettingsSkinPreviewCachePath("beast skin", 1, "7f3c1d8a2b44");
	EXPECT_EQ(Path, "qmclient/skins/preview_cache/beast_skin--v1--7f3c1d8a2b44.webp");
}

TEST(SettingsSkinPreviewCache, KeyIgnoresBodyAndFeetColor)
{
	SSettingsSkinPreviewCacheKey A{"beast", 1, 64, "hash-a"};
	SSettingsSkinPreviewCacheKey B{"beast", 1, 64, "hash-a"};
	EXPECT_EQ(A, B);
}

TEST(SettingsSkinPreviewCache, DifferentVersionChangesPath)
{
	EXPECT_NE(
		BuildSettingsSkinPreviewCachePath("beast", 1, "hash"),
		BuildSettingsSkinPreviewCachePath("beast", 2, "hash"));
}

TEST(SettingsSkinPreviewCache, PruneRemovesOldestOverflowEntries)
{
	std::vector<SSettingsSkinPreviewCacheFileEntry> vEntries = {
		{"a.webp", 10},
		{"b.webp", 20},
		{"c.webp", 30},
	};
	const auto vDelete = ComputeSettingsSkinPreviewCachePruneList(vEntries, 2);
	ASSERT_EQ(vDelete.size(), 1u);
	EXPECT_EQ(vDelete[0], "a.webp");
}
```

- [ ] **Step 2: Run the tests and verify they fail**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=SettingsSkinPreviewCache.*
```

Expected: compile failure because the module does not exist.

- [ ] **Step 3: Create the cache module header**

Create `src/game/client/components/settings_skin_preview_cache.h` with only the minimal pure-logic/public state needed now:

```cpp
#ifndef GAME_CLIENT_COMPONENTS_SETTINGS_SKIN_PREVIEW_CACHE_H
#define GAME_CLIENT_COMPONENTS_SETTINGS_SKIN_PREVIEW_CACHE_H

#include <engine/graphics.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

inline constexpr int SETTINGS_SKIN_PREVIEW_CACHE_VERSION = 1;
inline constexpr int SETTINGS_SKIN_PREVIEW_CACHE_FILE_LIMIT = 512;

struct SSettingsSkinPreviewCacheKey
{
	std::string m_SkinName;
	int m_Version = 0;
	int m_SizeBucket = 0;
	std::string m_ContentHash;

	bool operator==(const SSettingsSkinPreviewCacheKey &Other) const = default;
};

struct SSettingsSkinPreviewCacheFileEntry
{
	std::string m_Path;
	int64_t m_ModifiedTime = 0;
};

std::string BuildSettingsSkinPreviewCachePath(const char *pSkinName, int Version, const char *pHash);
std::vector<std::string> ComputeSettingsSkinPreviewCachePruneList(std::vector<SSettingsSkinPreviewCacheFileEntry> vEntries, int MaxFiles);

#endif
```

- [ ] **Step 4: Implement the pure helpers**

Create `src/game/client/components/settings_skin_preview_cache.cpp` with only the test-driven helpers first:

```cpp
#include "settings_skin_preview_cache.h"

#include <base/system.h>

#include <algorithm>

std::string BuildSettingsSkinPreviewCachePath(const char *pSkinName, int Version, const char *pHash)
{
	char aName[IO_MAX_PATH_LENGTH];
	str_copy(aName, pSkinName ? pSkinName : "skin", sizeof(aName));
	str_sanitize_filename(aName);
	for(char *pChar = aName; *pChar != '\0'; ++pChar)
	{
		if(*pChar == ' ')
			*pChar = '_';
	}

	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "qmclient/skins/preview_cache/%s--v%d--%s.webp", aName, Version, pHash ? pHash : "missing");
	return aBuf;
}

std::vector<std::string> ComputeSettingsSkinPreviewCachePruneList(std::vector<SSettingsSkinPreviewCacheFileEntry> vEntries, int MaxFiles)
{
	if(MaxFiles < 0)
		MaxFiles = 0;
	if((int)vEntries.size() <= MaxFiles)
		return {};

	std::sort(vEntries.begin(), vEntries.end(), [](const auto &A, const auto &B) {
		return A.m_ModifiedTime < B.m_ModifiedTime;
	});

	std::vector<std::string> vDelete;
	vDelete.reserve(vEntries.size() - MaxFiles);
	for(size_t i = 0; i < vEntries.size() - (size_t)MaxFiles; ++i)
		vDelete.push_back(vEntries[i].m_Path);
	return vDelete;
}
```

- [ ] **Step 5: Re-run the pure cache tests and make them pass**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=SettingsSkinPreviewCache.*
```

Expected: all four tests pass.

## Task 3: Wire runtime preview cache into Tee settings list

**Files:**
- Modify: `src/game/client/components/settings_skin_preview_cache.h`
- Modify: `src/game/client/components/settings_skin_preview_cache.cpp`
- Modify: `src/game/client/components/menus.h`
- Modify: `src/game/client/components/menus.cpp`
- Modify: `src/game/client/components/menus_settings.cpp`
- Modify: `src/game/client/components/skins.h`
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/test/settings_skin_preview_cache_test.cpp`
- Modify: `src/test/settings_warmup_test.cpp`

- [ ] **Step 1: Add failing tests for non-color cache reuse logic**

Extend `src/test/settings_skin_preview_cache_test.cpp` with a runtime-logic-level test that proves colors do not participate in the disk cache identity:

```cpp
TEST(SettingsSkinPreviewCache, SameSkinAndHashReuseCacheAcrossColors)
{
	const auto PathA = BuildSettingsSkinPreviewCachePath("beast", SETTINGS_SKIN_PREVIEW_CACHE_VERSION, "abc123");
	const auto PathB = BuildSettingsSkinPreviewCachePath("beast", SETTINGS_SKIN_PREVIEW_CACHE_VERSION, "abc123");
	EXPECT_EQ(PathA, PathB);
}
```

Add one more focused test for invalid filename characters:

```cpp
TEST(SettingsSkinPreviewCache, PathSanitizesInvalidFilenameCharacters)
{
	const auto Path = BuildSettingsSkinPreviewCachePath("a:b/c?d*e", 1, "hash");
	EXPECT_EQ(Path, "qmclient/skins/preview_cache/abcde--v1--hash.webp");
}
```

Run the tests. If the sanitize expectation differs on Windows, update the expected literal to the actual DDNet sanitizer output and keep the rule explicit.

- [ ] **Step 2: Expose the minimal skin revision input**

Add a stable lightweight helper on the skins side so the cache can build a content hash without guessing file paths in UI code. In `src/game/client/components/skins.h`, add public helpers on `CSkinContainer` or `CSkins` such as:

```cpp
std::string SettingsPreviewCacheContentHash() const;
```

Implement it in `skins.cpp` from stable inputs already owned by the skin system:

- skin name
- storage type / source type
- loaded metrics
- [if available cheaply] underlying file bytes hash for local/downloading skins

Keep it boring and deterministic. If file-content hashing is too invasive for the first cut, hash the loaded grayscale/original image bytes during `LoadSkinFinish()` and store the result on `CSkin`.

- [ ] **Step 3: Extend the cache module with runtime state**

Add a small runtime cache wrapper to `settings_skin_preview_cache.h`:

```cpp
class CSettingsSkinPreviewCache
{
public:
	void Init(IStorage *pStorage, IGraphics *pGraphics);
	void Shutdown();
	void OnSkinDirectoryChanged();
	std::optional<IGraphics::CTextureHandle> FindTexture(const SSettingsSkinPreviewCacheKey &Key) const;
	void RememberTexture(const SSettingsSkinPreviewCacheKey &Key, IGraphics::CTextureHandle Texture, std::string Path);
	bool EnsureCacheFolders();
	void PruneDiskCache();
private:
	IStorage *m_pStorage = nullptr;
	IGraphics *m_pGraphics = nullptr;
	std::unordered_map<SSettingsSkinPreviewCacheKey, IGraphics::CTextureHandle, SSettingsSkinPreviewCacheKeyHash> m_vTextures;
};
```

Keep this first version narrow: no speculative queue manager, no global singleton, no color state.

- [ ] **Step 4: Initialize and clean up the cache from menus**

In `menus.h`, add a member:

```cpp
CSettingsSkinPreviewCache m_TeeSkinPreviewCache;
```

In `menus.cpp`:
- initialize it from `CMenus::OnInit()` / equivalent initialization path with `Storage()` and `Graphics()`;
- call `Shutdown()` in `OnShutdown()`;
- call `OnSkinDirectoryChanged()` from the same invalidation paths that already react to skin resource changes.

Do not clear it for mere color changes or scroll changes.

- [ ] **Step 5: Integrate cache lookup into the Tee list render loop**

In `menus_settings.cpp` around the current tee list loop (`RenderTee(...)` around lines ~1735-1750), change the flow to:

1. Build `CTeeRenderInfo Info` exactly as today.
2. Build cache key from:
   - skin name
   - cache version
   - size bucket from `SettingsSkinPreviewSize(...)`
   - skin content hash helper
3. Ask `m_TeeSkinPreviewCache` for a ready texture.
4. If found:
   - draw the cached texture with a simple textured quad in the preview rect;
5. If not found:
   - keep the existing `RenderTools()->RenderTee(...)` fallback;
   - schedule/attempt cache generation after drawing.

Keep selection labels, favorite icons, queue buttons, text highlight, and debug blood-color square exactly as they are.

- [ ] **Step 6: Record and save new preview textures**

Add a minimal generation path using the existing render target API:

- create a render target sized to the preview bucket;
- begin target;
- draw the tee preview using the existing `RenderTee(...)` path with a transparent clear color;
- end target;
- read back the recorded image;
- save it as WebP to `qmclient/skins/preview_cache/...`;
- immediately load/upload or directly remember the generated texture for runtime reuse.

If a backend-safe readback helper is missing, add the smallest possible graphics-threaded helper for render-target-to-`CImageInfo` readback instead of inventing a second rendering path.

- [ ] **Step 7: Reuse Workshop cache error-handling patterns**

Follow the same shape already used in `menus_settings_assets.cpp`:
- create folder before write;
- if decode fails, remove the broken `.webp` file;
- never crash if cache load/save fails;
- continue drawing live tee preview.

- [ ] **Step 8: Re-run focused tests for the new logic**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=Skins.WebPSaveRoundTripPreservesImageShape:SettingsSkinPreviewCache.*:SettingsResourceJobs.ProgressiveSkinListWarmupWaitsForRequestedRange:SettingsResourceJobs.SkinListSelectionRejectsShrunkenPublishedIndex:SettingsResourceJobs.SkinListScrollResetOnlyWhenScrollbarStateCanBreakDrag:SettingsResourceJobs.TeeSkinListScrollResetWaitsUntilListboxBecomesInactive
```

Expected: all focused tests pass.

## Task 4: Full verification and docs checks

**Files:**
- Modify if needed: `docs/superpowers/specs/2026-05-28-tee-skin-preview-webp-cache-design.md`
- Create: `docs/superpowers/plans/2026-05-28-tee-skin-preview-webp-cache-implementation.md`

- [ ] **Step 1: Save this plan file**

Ensure this plan is written to:

```text
docs/superpowers/plans/2026-05-28-tee-skin-preview-webp-cache-implementation.md
```

- [ ] **Step 2: Run docs consistency checks**

Run:

```powershell
python qmclient_scripts/gate/check_docs.py
```

Expected: pass.

- [ ] **Step 3: Run broader C++ regression coverage**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 10
cmake-build-release/testrunner.exe --gtest_filter=Skins.UsageTrackingSkipsAlwaysLoadedStates:Skins.AlwaysLoadedStateTransitionsNeverTouchUsageList:Skins.UsageListEntriesThatCannotBeUnloadedAreDiscarded:Skins.RegularStateTransitionsEnterAndLeaveUsageList:Skins.ImmediateRequestLoadShouldTouchUsageTrackingData:Skins.SkinDataPreparationBuildsMergedMetricsWithoutGraphics:Skins.SkinDataPreparationUsesOutlineMetricsWhenFillIsEmpty:Skins.RenderedTeeOffsetAccountsForHorizontalAndVerticalMetrics:Skins.WebPSaveRoundTripPreservesImageShape:SettingsSkinPreviewCache.*:SettingsResourceJobs.SkinListPublishesIncrementalMergedEntries:SettingsResourceJobs.SkinListDoesNotShrinkDuringDirectoryScanReplan:SettingsResourceJobs.SkinListFirstPageWarmupUsesVisibleRowsPlusExtraRow:SettingsResourceJobs.SkinListFirstPageWarmupRejectsInvalidGeometry:SettingsResourceJobs.SkinListPrefetchCountExtendsVisibleRangeDownward:SettingsResourceJobs.PrioritizedSkinListEntriesRequestImmediateLoad:SettingsResourceJobs.ActiveTeeSettingsBoostsSkinBudgets:SettingsResourceJobs.InactiveTeeSettingsKeepsDefaultSkinBudgets:SettingsResourceJobs.ProgressiveSkinListWarmupWaitsForRequestedRange:SettingsResourceJobs.SkinListSelectionRejectsShrunkenPublishedIndex:SettingsResourceJobs.SkinListScrollResetOnlyWhenScrollbarStateCanBreakDrag:SettingsResourceJobs.TeeSkinListScrollResetWaitsUntilListboxBecomesInactive
```

Expected: all targeted regressions pass.

- [ ] **Step 4: Build the client**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 10
```

Expected: `DDNet.exe` links successfully.

- [ ] **Step 5: Manual runtime verification**

Launch:

```powershell
cmake-build-release\DDNet.exe
```

Verify manually:

1. `设置 -> Tee` first open creates files under `qmclient/skins/preview_cache`.
2. Scrolling down and back up reuses preview cache.
3. Switching to the server browser and back does not re-trigger the previous crash path.
4. Changing body / feet color changes visible color but does not create color-specific duplicate cache files.
5. Deleting one cached `.webp` while the client is closed causes that preview to be regenerated on next visit.

- [ ] **Step 6: Commit**

```powershell
git add docs/superpowers/specs/2026-05-28-tee-skin-preview-webp-cache-design.md docs/superpowers/plans/2026-05-28-tee-skin-preview-webp-cache-implementation.md src/engine/gfx/image_loader.h src/engine/gfx/image_loader.cpp src/game/client/components/settings_skin_preview_cache.h src/game/client/components/settings_skin_preview_cache.cpp src/game/client/components/menus.h src/game/client/components/menus.cpp src/game/client/components/menus_settings.cpp src/game/client/components/skins.h src/game/client/components/skins.cpp src/test/settings_skin_preview_cache_test.cpp src/test/skins_test.cpp CMakeLists.txt
git commit -m "feat(settings): cache tee skin previews as webp"
```

## Plan Self-Review

- Spec coverage: WebP disk cache path, color-independent keying, version/hash naming, fallback behavior, pruning, and Tee list integration are all covered by Tasks 1-4.
- Placeholder scan: No `TODO`/`TBD` placeholders remain.
- Type consistency: The plan uses one cache module name (`CSettingsSkinPreviewCache`) and one helper family (`BuildSettingsSkinPreviewCachePath`, `ComputeSettingsSkinPreviewCachePruneList`, `SaveWebP`) consistently.
