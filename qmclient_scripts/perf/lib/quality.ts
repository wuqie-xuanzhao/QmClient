import { basename } from 'node:path';

import type { PerfEntry, ParseDiagnostics } from './parse.ts';
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
  type TargetSettingsSnapshot,
  type Verdict,
  computeVerdict,
  entryDurationMs,
} from './stats.ts';

export type { ParseDiagnostics } from './parse.ts';

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

export const FPS_BASELINES = {
  ingame_esc_open: { p99: 16.7, max: 33.4, menuMax: 12.0, onePctLow: 60 },
  settings_assets_tab_switch: { p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
  settings_tab_switch: { p99: 16.7, max: 33.4, menuMax: 8.0, onePctLow: 60 },
} as const;

export type FpsBaselineOperation = keyof typeof FPS_BASELINES;

export function fpsBaselineVerdict(summary: FpsSummary): { operation: FpsBaselineOperation; passed: boolean; reason: string } | null {
  if (!(summary.operation in FPS_BASELINES)) return null;
  const operation = summary.operation as FpsBaselineOperation;
  const baseline = FPS_BASELINES[operation];
  const failures: string[] = [];
  if (!summary.fpsOnePctLowAvailable) {
    failures.push(`1pct_source=${summary.fpsOnePctLowSource}`);
  } else if (summary.fpsOnePctLow < baseline.onePctLow) {
    failures.push(`1pct_low=${summary.fpsOnePctLow.toFixed(1)}<${baseline.onePctLow}`);
  }
  if (summary.frameMsP99 > baseline.p99) {
    failures.push(`p99=${summary.frameMsP99.toFixed(3)}>${baseline.p99}`);
  }
  if (summary.frameMsMax > baseline.max) {
    failures.push(`max=${summary.frameMsMax.toFixed(3)}>${baseline.max}`);
  }
  if (summary.menuMsMax > baseline.menuMax) {
    failures.push(`menu_max=${summary.menuMsMax.toFixed(3)}>${baseline.menuMax}`);
  }
  return {
    operation,
    passed: failures.length === 0,
    reason: failures.join(' '),
  };
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
  targetSettings: TargetSettingsSnapshot;
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
      maxTextureUploadMs: number;
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
      topUiSectionStage: string;
      topUiSectionPage: string;
      topUiSectionFrame: number;
      topUiSectionMs: number;
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
  const FpsBaselineFailures = fps
    .map(summary => ({ summary, verdict: fpsBaselineVerdict(summary) }))
    .filter((row): row is { summary: FpsSummary; verdict: NonNullable<ReturnType<typeof fpsBaselineVerdict>> } => row.verdict !== null && !row.verdict.passed);
  for (const row of FpsBaselineFailures) {
    warnings.push(`fps_baseline_failed: ${row.summary.operation} context=${row.summary.context || 'unknown'} page=${row.summary.page || 'unknown'} tab=${row.summary.tab || 'none'} ${row.verdict.reason}`);
  }
  if (!hasOnlineTargetSettingsFpsSummary(entries)) {
    warnings.push('missing ingame/online operation window; settings acceptance is incomplete');
  }
  const targetSettings = targetSettingsSnapshot(entries);
  if (targetSettings.stableTextCoverage.acceptanceBlocked) {
    warnings.push('stable text coverage blocked settings acceptance');
  }
  if (targetSettings.stableTextCoverage.buildQueued > 0) {
    warnings.push('stable text queued visible builds during target window');
  }
  if (targetSettings.stableTextCoverage.fallbackImmediate > 0) {
    warnings.push('stable text used immediate fallback during target window');
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
  const AggregateOnlyTargetFpsFailures = budgetCorrelation.windows.filter(window =>
    (!window.fpsOnePctLowAvailable || window.fpsOnePctLow < 240) &&
    window.culpritRank.length > 0 &&
    !window.culpritRank.some(culprit => culprit.kind.startsWith('ui_section:')) &&
    window.culpritRank.some(culprit => culprit.kind === 'ui_layout_or_render_total' && culprit.score > 0));
  if (AggregateOnlyTargetFpsFailures.length > 0) {
    warnings.push('target fps failure has only aggregate ui attribution');
  }
  const FpsWindowMissingRealOnePctLow = budgetCorrelation.windows.some(window => !window.fpsOnePctLowAvailable || window.fpsOnePctLowSource !== 'real_sampled');
  if (FpsWindowMissingRealOnePctLow) {
    warnings.push('fps_1pct_low_missing_real_sampled: fps_summary windows without fps_1pct_source=real_sampled cannot pass the 240Hz gate');
  }
  const Failed = budgetCorrelation.failingWindowCount > 0 || FpsWindowMissingRealOnePctLow || AggregateOnlyTargetFpsFailures.length > 0 || FpsBaselineFailures.length > 0;

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
