#ifndef GAME_CLIENT_COMPONENTS_SECTION_LOADER_H
#define GAME_CLIENT_COMPONENTS_SECTION_LOADER_H

#include <game/client/components/settings_runtime_cache.h>
#include <game/client/ui_rect.h>

#include <cstdint>
#include <functional>
#include <vector>

enum class ESettingsSectionState : uint8_t
{
	UNINITIALIZED,
	MEASURING,
	COMPACT,
	FULL,
};

/**
 * A single section of a settings page.
 *
 * Each section provides three render callbacks:
 *   - MeasureFn:  calculate height without any rendering
 *   - RenderCompactFn: optional warmup/full-equivalent fallback; never render visible summary text
 *   - RenderFullFn:    render the full interactive section
 *
 * All callbacks receive a CUIRect for the available column space and return
 * the consumed height. The caller is responsible for advancing the column rect.
 *
 * Dependencies track which g_Config values affect this section's output.
 * Dirty sections refresh their config hash, but FULL sections still render
 * every frame because DDNet menus are immediate-mode UI.
 */
struct SSettingsSection
{
	const char *m_pName;
	ESettingsSectionState m_State = ESettingsSectionState::UNINITIALIZED;
	float m_CachedHeight = 0.0f;
	bool m_HasCachedHeight = false;

	std::function<float(CUIRect &)> m_MeasureFn;
	std::function<float(CUIRect &)> m_RenderCompactFn;
	std::function<float(CUIRect &)> m_RenderFullFn;

	std::vector<const int *> m_DependencyConfigInts;
	std::vector<const unsigned *> m_DependencyConfigCols;
	uint64_t m_LastConfigHash = 0;
	bool m_Dirty = true; // force render on first frame
};

/**
 * Session UI cache saved to disk across sessions.
 *
 * Stores the last active tab and scroll position so the next launch can
 * pre-warm the relevant sections during the loading screen.
 */
struct SSessionUiCache
{
	int m_LastSettingsPage = -1;
	int m_LastTClientTab = -1;
	int m_LastQmTab = -1;
	float m_LastScrollY = 0.0f;
	SSettingsSectionCacheRuntimeKey m_RuntimeKey;
	bool m_Valid = false;
};

struct SSectionLoaderFrameStats
{
	int m_SectionsTotal = 0;
	int m_SectionsVisible = 0;
	int m_SectionsSkipped = 0;
	int m_LayoutDirtySections = 0;
	ESettingsCacheDirtyReason m_DirtyReason = ESettingsCacheDirtyReason::NONE;
};

/**
 * Drives progressive rendering of settings-page sections.
 *
 * Usage:
 *   1. Register() all sections before Begin(); frame-local callbacks must be
 *      registered each frame with the same section names to preserve state.
 *   2. Call Begin() for the current frame.
 *   3. Call Process() every frame; returns true while there is still work.
 *   4. Call Reset() on tab switch.
 *   5. Optionally call Warmup() during the loading screen.
 *
 * The loader advances sections through a four-state machine with a
 * per-frame time budget:
 *
 *   UNINITIALIZED  →  measure height (negligible cost)
 *   MEASURING      →  optional progressive warmup path when explicitly enabled
 *   COMPACT        →  render full interactive section (1–2 per frame max)
 *   FULL           →  render full UI every frame; dirty refreshes config hash
 *
 * Viewport priority ensures that sections near the current scroll position
 * are promoted before off-screen sections.
 *
 * Process() clears callbacks before returning, so persistent loaders do not
 * retain references to frame-local UI layout objects.
 */
class CSectionLoader
{
public:
	CSectionLoader();
	~CSectionLoader();

	/** Register the full set of sections, preserving state for matching names. */
	void Register(std::vector<SSettingsSection> vSections);

	/**
	 * Start progressive rendering for the given viewport rect.
	 * @param MainView     Full available area for the sections.
	 * @param TimeBudgetMs Per-frame CPU budget in milliseconds (default 5.0).
	 */
	void Begin(CUIRect MainView, float TimeBudgetMs = 5.0f);

	/** Advance one frame. Returns true when there is still work left. */
	bool Process();

	/** All visible sections have reached FULL state. */
	bool IsComplete() const;

	/** Reset the state machine (e.g. when switching tabs). */
	void Reset();

	// -- Pre-warming (loading screen) --

	/**
	 * Pre-warm real section content for sections that were visible in the last
	 * session, so glyph atlases and section caches are ready before settings open.
	 * Call once per frame during the loading screen.
	 * @param pCache       Session cache from last run (null = skip).
	 * @param TimeBudgetMs Per-frame CPU budget in milliseconds (default 3.0).
	 * @returns true when warmup is finished.
	 */
	bool Warmup(const SSessionUiCache *pCache, float TimeBudgetMs = 3.0f);
	bool IsWarmupComplete() const;

	// -- Cache invalidation --

	/** Invalidate all section caches (e.g. after language change or resize). */
	void InvalidateCache(ESettingsCacheDirtyReason Reason = ESettingsCacheDirtyReason::CONFIG);

	/** Mark sections dirty that depend on the given config pointer. */
	void SetDirtyByConfig(const void *pConfigVar);

	// -- Session cache I/O --

	static bool LoadSessionCache(SSessionUiCache &Cache, const char *pFilename, class IStorage *pStorage);
	static void SaveSessionCache(const SSessionUiCache &Cache, const char *pFilename, class IStorage *pStorage);
	static bool IsVisibleSummarySectionName(const char *pName);
	void SetRuntimeKey(const SSettingsSectionCacheRuntimeKey &RuntimeKey);
	void SetProgressiveEnabled(bool Enabled);
	void SetDeferredFarMeasurementEnabled(bool Enabled);
	void SetMaxSectionsPerFrame(int MaxSectionsPerFrame);

	// -- State exposed for the rendering loop (updated externally) --

	int m_ActiveTab = -1;
	float m_ScrollY = 0.0f;
	CUIRect GetRunningColumn() const { return m_RunningColumn; }

	// -- Profiling --

	const char *GetPerfReport() const;
	const SSectionLoaderFrameStats &LastFrameStats() const { return m_LastFrameStats; }

private:
	std::vector<SSettingsSection> m_vSections;
	CUIRect m_MainView;
	CUIRect m_RunningColumn;
	double m_BudgetPerFrameMs = 5.0;

	int m_CurrentIndex = 0;
	bool m_Initialized = false;
	bool m_Complete = false;
	bool m_ProgressiveEnabled = false;
	bool m_DeferredFarMeasurementEnabled = false;
	int m_MaxSectionsPerFrame = 2;
	SSettingsSectionCacheRuntimeKey m_RuntimeKey;

	// Warmup state
	bool m_WarmupActive = false;
	int m_WarmupIndex = 0;
	float m_WarmupBudgetMs = 0.0f;
	const SSessionUiCache *m_pWarmupCache = nullptr;

	// Profiling
	double m_TotalFrameTimeMs = 0.0;
	SSectionLoaderFrameStats m_LastFrameStats;
	ESettingsCacheDirtyReason m_LastDirtyReason = ESettingsCacheDirtyReason::NONE;

	/** 0 = in viewport, 1 = near, 2 = far. */
	int ComputeViewportPriority(const CUIRect &SectionRect) const;
	static uint64_t ComputeConfigHash(const SSettingsSection &Section);
};

#endif // GAME_CLIENT_COMPONENTS_SECTION_LOADER_H
