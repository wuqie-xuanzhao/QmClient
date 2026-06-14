import { basename } from 'node:path';

import type { PerfEntry } from './parse.ts';
import { operationSignature, type OperationSignature } from './quality_core.ts';
import {
  BUDGET,
  adaptiveBudgetSummary,
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
  selectFrameTimeEntries,
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
  demoBrowser: {
    available: boolean;
    startupCount: number;
    headerFetchCount: number;
    dateFetchCount: number;
    previewLoadCount: number;
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
    maxTextTokens: number;
    maxDemoTokens: number;
    samples: string[];
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

  return {
    sampleCount: frameEntries.length,
    totalEntries: entries.length,
    totalLines: diagnostics.totalLines,
    invalidLines: diagnostics.invalidLines,
    samplingThresholdMs,
    biased,
    operation: operationSignature(entries),
    warnings,
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
  const demoBrowser = demoBrowserPhaseSummary(entries);
  const adaptiveBudget = adaptiveBudgetSummary(entries);
  return {
    generatedAt: new Date().toISOString(),
    sourceFile: basename(sourceFile),
    percentiles,
    verdict: frameEntries.length === 0 ? 'WARN' : computeVerdict(percentiles, spikes.length),
    verdictAvailable: frameEntries.length > 0,
    spikeCount: spikes.length,
    quality: reportQuality(entries, diagnostics),
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
    demoBrowser,
    adaptiveBudget,
  };
}
