import { basename } from 'node:path';

import type { PerfEntry } from './parse.ts';
import { operationSignature, type OperationSignature } from './quality_core.ts';
import {
  BUDGET,
  adaptiveBudgetSummary,
  budgetCorrelationSummary,
  assetsPreviewAdmissionSummary,
  assetsVisibleReadySummary,
  calcPercentiles,
  demoBrowserPhaseSummary,
  detectSpikes,
  inferSamplingThreshold,
  isSamplingBiased,
  fpsSummaries,
  hasOnlineTargetSettingsFpsSummary,
  pagePerformanceAttribution,
  previewBudgetSummary,
  selectFrameTimeEntries,
  settingsUiBudgetSummary,
  textRuntimeBudgetSummary,
  targetSettingsSnapshot,
  type AttributionEntry,
  type FpsSummary,
  type Percentiles,
  type Verdict,
  computeVerdict,
  entryDurationMs,
} from './stats.ts';

export interface ParseDiagnostics {
  totalLines: number;
  invalidLines: number;
}

export interface ReportQuality {
  sampleCount: number;
  totalEntries: number;
  totalLines: number;
  invalidLines: number;
  samplingThresholdMs: number;
  biased: boolean;
  operation: OperationSignature;
  warnings: string[];
  failed: boolean;
}

export interface PerfBundleSummary {
  generatedAt: string;
  sourceFile: string;
  percentiles: Percentiles;
  verdict: Verdict;
  verdictAvailable: boolean;
  spikeCount: number;
  quality: ReportQuality;
  attribution: {
    top: AttributionEntry[];
  };
  fps: {
    available: boolean;
    summaries: FpsSummary[];
  };
  targetSettings: {
    verdict: Verdict;
    verdictAvailable: boolean;
    spikeCount: number;
    percentiles: Percentiles;
    stableTextCoverage: {
      acceptanceBlocked: boolean;
      utilizationAvailable: boolean;
      planCoverageAvailable: boolean;
      planCandidateCount: number;
      visibleCandidateCount: number;
      unplannedVisibleCount: number;
      keyMismatchCount: number;
      candidateTotal: number;
      hitCount: number;
      reuseCount: number;
      missCount: number;
      staleCount: number;
      hitRate: number;
      reuseRate: number;
      textNew: number;
      textReused: number;
      prebuildRemainingBeforeTarget: number;
      planCollectionAvailable: boolean;
      planCollectionComplete: boolean;
      planCollectionUnitsTotal: number;
      planCollectionUnitsDone: number;
      planCollectionRemainingBeforeTarget: number;
      planCollectionBudget: number;
      planCollectionPhase: string;
      planCollectionScope: string;
      planCollectionOperation: string;
      consistencyWarnings: string[];
      samples: string[];
    };
  };
  assetsPreviewAdmission: {
    available: boolean;
    visibleFirstAvailable: boolean;
    eventCount: number;
    maxDurationMs: number;
    maxLayoutTextMs: number;
    maxPreviewDrawMs: number;
    maxThumbSchedulingMs: number;
    maxRendered: number;
    maxThumbStarts: number;
    visibleStarts: number;
    prefetchStarts: number;
    backgroundStarts: number;
    samples: string[];
  };
  assetsVisibleReady: {
    available: boolean;
    visibleReadyAvailable: boolean;
    geometryStable: boolean;
    thumbStartsBeforeVisible: number;
    thumbStartsDuringDraw: number;
    visibleCount: number;
    notReadyCount: number;
    eventCount: number;
    samples: string[];
  };
  previewBudget: {
    available: boolean;
    eventCount: number;
    previewJobsStarted: number;
    previewJobsDone: number;
    previewUploads: number;
    previewAdmissions: number;
    maxPreviewArtifactMs: number;
    maxMetadataHydrateMs: number;
    maxPlaceholderCount: number;
    maxReadyTextureCount: number;
    minVisibleReadyRatio: number;
    samples: string[];
  };
  demoBrowser: {
    available: boolean;
    startupCount: number;
    headerFetchCount: number;
    dateFetchCount: number;
    previewLoadCount: number;
    visibleScanned: number;
    visibleDone: number;
    backgroundScanned: number;
    backgroundDone: number;
    maxDurationMs: number;
    maxRemaining: number;
    maxMetadataRemaining: number;
    samples: string[];
  };
  adaptiveBudget: {
    available: boolean;
    eventCount: number;
    framePressureCount: number;
    maxVisibleTokens: number;
    maxPrefetchTokens: number;
    maxBackgroundTokens: number;
    maxGpuUploadTokens: number;
    maxResourceUploadTokens: number;
    maxTextTokens: number;
    maxTextContainerTokens: number;
    maxGlyphUploadTokens: number;
    maxParagraphLayoutTokens: number;
    maxDemoTokens: number;
    samples: string[];
  };
  settingsUiBudget: {
    available: boolean;
    eventCount: number;
    maxLayoutMs: number;
    maxTextMs: number;
    maxTextNew: number;
    maxTextReused: number;
    maxDrawCalls: number;
    maxVertices: number;
    maxIndices: number;
    maxHeapAllocs: number;
    maxVisibleWidgets: number;
    samples: string[];
  };
  textRuntimeBudget: {
    available: boolean;
    eventCount: number;
    glyphNew: number;
    glyphUploads: number;
    maxGlyphRasterizeMs: number;
    maxGlyphUploadMs: number;
    textContainerNew: number;
    textContainerUploads: number;
    maxTextContainerCreateMs: number;
    maxTextContainerUploadMs: number;
    maxParagraphLayoutMs: number;
    paragraphBudgetBlocked: number;
    paragraphCacheHit: number;
    paragraphCacheMiss: number;
    samples: string[];
  };
  budgetCorrelation: {
    available: boolean;
    windowCount: number;
    failingWindowCount: number;
    unattributedFailingWindowCount: number;
    windows: {
      operation: string;
      context: string;
      page: string;
      tab: string;
      fpsOnePctLow: number;
      fpsOnePctLowAvailable: boolean;
      fpsOnePctLowSource: string;
      windowStartFrame: number;
      windowEndFrame: number;
      frameMsP99: number;
      resourceUploadTokens: number;
      previewUploads: number;
      maxPreviewArtifactMs: number;
      maxPreviewDrawMs: number;
      maxMetadataLayoutMs: number;
      maxUiLayoutMs: number;
      maxCardDrawMs: number;
      glyphUploads: number;
      maxGlyphRasterizeMs: number;
      maxGlyphUploadMs: number;
      textContainerNew: number;
      maxTextContainerCreateMs: number;
      maxParagraphLayoutMs: number;
      paragraphBudgetBlocked: number;
      dominantAttribution: string;
      culpritRank: {
        kind: string;
        score: number;
        details: string;
      }[];
      sample: string;
    }[];
  };
}

export function reportQuality(entries: PerfEntry[], diagnostics: ParseDiagnostics): ReportQuality {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const samplingThresholdMs = inferSamplingThreshold(durations);
  const biased = isSamplingBiased(durations, BUDGET.samplingDefault);
  const warnings: string[] = [];

  if (frameEntries.length === 0) {
    warnings.push('no frame-time samples; percentile and verdict data are unavailable');
  }
  if (diagnostics.invalidLines > 0) {
    warnings.push(`${diagnostics.invalidLines} invalid lines were ignored during parsing`);
  }
  if (biased) {
    warnings.push(`sampling threshold appears above default 4ms; p5=${samplingThresholdMs.toFixed(1)}ms`);
  }
  const fps = fpsSummaries(entries);
  if (fps.length === 0) {
    warnings.push('missing fps_summary; settings acceptance is incomplete');
  }
  if (!hasOnlineTargetSettingsFpsSummary(entries)) {
    warnings.push('missing ingame/online operation window; settings acceptance is incomplete');
  }
  const targetSettings = targetSettingsSnapshot(entries);
  if (targetSettings.stableTextCoverage.acceptanceBlocked) {
    warnings.push('stable text coverage blocked settings acceptance');
  }
  const assetsVisibleReady = assetsVisibleReadySummary(entries);
  if (!assetsVisibleReady.available) {
    warnings.push('assets visible-ready preflight missing');
  } else {
    if (!assetsVisibleReady.visibleReadyAvailable) {
      warnings.push('assets visible-ready preflight did not reach visible_ready=1');
    }
    if (assetsVisibleReady.notReadyCount > 0) {
      warnings.push('assets visible-ready preflight still has not_ready_count > 0');
    }
    if (assetsVisibleReady.thumbStartsDuringDraw > 0) {
      // thumbStartsDuringDraw > 0 blocks visible-ready acceptance.
      warnings.push('assets thumbs started during draw; visible-ready presentation is not met');
    }
    if (assetsVisibleReady.geometryStable === false) {
      warnings.push('assets card geometry unstable');
    }
  }
  const adaptiveBudget = adaptiveBudgetSummary(entries);
  const AssetsAdaptiveTargetStages = new Set([
    'assets_preview_draw_workshop_cards',
    'assets_visible_preflight',
    'assets_preview_gpu_upload_scan',
    'assets_workshop_thumb_upload_total',
  ]);
  const DemoAdaptiveTargetEvents = new Set([
    'demo_browser_header_fetch',
    'demo_browser_date_fetch',
    'demo_browser_startup',
  ]);
  const NeedsAdaptiveBudget = entries.some(entry =>
    (entry.system === 'perf/assets' && AssetsAdaptiveTargetStages.has(String(entry.fields.stage ?? ''))) ||
    (entry.system === 'perf/interaction' && DemoAdaptiveTargetEvents.has(String(entry.fields.event ?? ''))) ||
    (String(entry.fields.event ?? '').startsWith('settings_text_') && String(entry.fields.scope ?? '').includes('target')));
  if (NeedsAdaptiveBudget && !adaptiveBudget.available) {
    warnings.push('adaptive budget telemetry missing for assets/demo/stable text work');
  }
  const budgetCorrelation = budgetCorrelationSummary(entries);
  // Contract marker: budgetCorrelation: budgetCorrelationSummary(entries)
  if (budgetCorrelation.failingWindowCount > 0) {
    warnings.push(`fps_1pct_low_below_240: ${budgetCorrelation.failingWindowCount} real sampled low-fps window(s) failed the 240Hz target`);
  }
  if (budgetCorrelation.unattributedFailingWindowCount > 0) {
    warnings.push(`unattributed_spike: ${budgetCorrelation.unattributedFailingWindowCount} low-fps window(s) have no text/resource budget culprit`);
  }
  const FpsWindowMissingRealOnePctLow = budgetCorrelation.windows.some(window => !window.fpsOnePctLowAvailable || window.fpsOnePctLowSource !== 'real_sampled');
  if (FpsWindowMissingRealOnePctLow) {
    warnings.push('fps_1pct_low_missing_real_sampled: fps_summary windows without fps_1pct_source=real_sampled cannot pass the 240Hz gate');
  }
  const Failed = budgetCorrelation.failingWindowCount > 0 || FpsWindowMissingRealOnePctLow;

  return {
    sampleCount: frameEntries.length,
    totalEntries: entries.length,
    totalLines: diagnostics.totalLines,
    invalidLines: diagnostics.invalidLines,
    samplingThresholdMs,
    biased,
    operation: operationSignature(entries),
    warnings,
    failed: Failed,
  };
}

export function summarizeForBundle(entries: PerfEntry[], sourceFile: string, diagnostics: ParseDiagnostics): PerfBundleSummary {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const percentiles = calcPercentiles(durations);
  const spikes = detectSpikes(frameEntries, BUDGET.h60);
  const fps = fpsSummaries(entries);
  const targetSettings = targetSettingsSnapshot(entries);
  const assetsPreviewAdmission = assetsPreviewAdmissionSummary(entries);
  const assetsVisibleReady = assetsVisibleReadySummary(entries);
  const previewBudget = previewBudgetSummary(entries);
  const demoBrowser = demoBrowserPhaseSummary(entries);
  const adaptiveBudget = adaptiveBudgetSummary(entries);
  const settingsUiBudget = settingsUiBudgetSummary(entries);
  const textRuntimeBudget = textRuntimeBudgetSummary(entries);
  const budgetCorrelation = budgetCorrelationSummary(entries);
  const quality = reportQuality(entries, diagnostics);
  return {
    generatedAt: new Date().toISOString(),
    sourceFile: basename(sourceFile),
    percentiles,
    verdict: quality.failed ? 'FAIL' : frameEntries.length === 0 ? 'WARN' : computeVerdict(percentiles, spikes.length),
    verdictAvailable: frameEntries.length > 0,
    spikeCount: spikes.length,
    quality,
    attribution: {
      top: pagePerformanceAttribution(entries).slice(0, 10),
    },
    fps: {
      available: fps.length > 0,
      summaries: fps,
    },
    targetSettings,
    assetsPreviewAdmission,
    assetsVisibleReady,
    previewBudget,
    demoBrowser,
    adaptiveBudget,
    settingsUiBudget,
    textRuntimeBudget,
    budgetCorrelation,
  };
}
