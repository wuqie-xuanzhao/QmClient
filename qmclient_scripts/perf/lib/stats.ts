// stats.ts — 统计算子（R-style 完整版）

import type { PerfEntry } from './parse.ts';
import { compareOperationSignatures, operationSignature, type OperationComparison, type OperationSignature } from './quality_core.ts';

export interface Percentiles {
  p10: number;
  p25: number;
  p50: number;
  p75: number;
  p90: number;
  p95: number;
  p99: number;
  max: number;
  min: number;
  avg: number;
  count: number;
  /** 标准差 */
  std: number;
  /** 四分位距 IQR = Q3 - Q1 */
  iqr: number;
}

/** 计算百分位（nearest-rank method） */
export function calcPercentiles(values: number[]): Percentiles {
  const sorted = [...values].sort((a, b) => a - b);
  const n = sorted.length;
  if (n === 0) return { p10:0, p25:0, p50:0, p75:0, p90:0, p95:0, p99:0, max:0, min:0, avg:0, count:0, std:0, iqr:0 };

  const p = (k: number) => sorted[Math.min(n - 1, Math.ceil((k / 100) * n) - 1)];
  const avg = sorted.reduce((a, b) => a + b, 0) / n;
  const variance = sorted.reduce((sum, v) => sum + (v - avg) ** 2, 0) / n;
  const std = Math.sqrt(variance);
  const q1 = p(25);
  const q3 = p(75);

  return {
    p10: p(10),
    p25: q1,
    p50: p(50),
    p75: q3,
    p90: p(90),
    p95: p(95),
    p99: p(99),
    max: sorted[n - 1],
    min: sorted[0],
    avg,
    count: n,
    std,
    iqr: q3 - q1,
  };
}

/** 计算帧预算合规率（百分比） */
export function complianceRate(values: number[], budgetMs: number): number {
  if (values.length === 0) return 100;
  const within = values.filter(v => v <= budgetMs).length;
  return (within / values.length) * 100;
}

/** 帧预算常量 */
export const BUDGET = {
  /** 240Hz → 4.17ms */
  h240: 4.17,
  /** 120Hz → 8.33ms */
  h120: 8.33,
  /** 60Hz → 16.67ms */
  h60: 16.67,
  /** 默认性能日志采样阈值 qm_perf_debug_threshold_ms */
  samplingDefault: 4,
  /** 2x 60Hz 帧预算，作为严重尖峰阈值 */
  h60Double: 33,
} as const;

/** 从日志数据推断采样阈值（取 p5，避开强制记录的极小样本） */
export function inferSamplingThreshold(durations: number[]): number {
  if (durations.length === 0) return 0;
  const sorted = [...durations].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * 0.05))];
}

/** 判断数据是否受采样偏差影响 */
export function isSamplingBiased(durations: number[], thresholdMs: number = BUDGET.samplingDefault): boolean {
  if (durations.length === 0) return false;
  // 如果 p5 高于日志采样阈值，说明大量正常帧可能被过滤了。
  return inferSamplingThreshold(durations) > thresholdMs;
}

export interface FrameTimeSeries {
  times: string[];
  durations: number[];
}

export const PERF_SYSTEM = {
  MENU: 'perf/menu',
  FPS: 'perf/fps',
  GAMECLIENT: 'perf/gameclient',
  INTERACTION: 'perf/interaction',
  DEVICE: 'perf/device',
  SKIN_UX: 'perf/skin-ux',
  SECTION: 'perf/section',
  UI_RUNTIME: 'perf/ui_runtime',
} as const;

export const PERF_EVENT = {
  PAGE_SWITCH: 'page_switch',
  LIST_FRAME: 'list_frame',
  WORK_DRAIN: 'work_drain',
  LIST_DRAIN_SUMMARY: 'list_drain_summary',
} as const;

const FRAME_TIME_SYSTEMS: ReadonlySet<string> = new Set([PERF_SYSTEM.MENU, PERF_SYSTEM.GAMECLIENT]);

function rawField(e: PerfEntry, name: string): unknown {
  return (e.fields as Record<string, unknown>)[name];
}

function hasField(e: PerfEntry, name: string): boolean {
  const Value = rawField(e, name);
  return Value !== undefined && Value !== null && Value !== '';
}

export function entryDurationMs(e: PerfEntry): number | null {
  const Raw = rawField(e, 'duration_ms') ?? rawField(e, 'dur_ms') ?? rawField(e, 'dur');
  if (Raw === undefined || Raw === null || Raw === '') {
    return null;
  }
  const Parsed = Number(Raw);
  return Number.isFinite(Parsed) ? Parsed : null;
}

export function isFrameTimeEntry(e: PerfEntry): boolean {
  return FRAME_TIME_SYSTEMS.has(e.system) && entryDurationMs(e) !== null;
}

export function selectFrameTimeEntries(entries: PerfEntry[]): PerfEntry[] {
  return entries.filter(isFrameTimeEntry);
}

export function toTimeSeries(entries: PerfEntry[]): FrameTimeSeries {
  const sorted = [...entries].sort((a, b) => a.timestamp.localeCompare(b.timestamp));
  return {
    times: sorted.map(e => e.timestamp),
    durations: sorted.map(e => entryDurationMs(e) ?? e.durationMs),
  };
}

export interface SpikeInfo {
  index: number;
  timestamp: string;
  durationMs: number;
  stage: string;
  page: string;
  threshold: number;
}

export interface FpsSummary {
  timestamp: string;
  operation: string;
  context: string;
  page: string;
  tab: string;
  sampleFrames: number;
  sampleSeconds: number;
  fpsAvg: number;
  fpsMin: number;
  fpsMax: number;
  frameMsAvg: number;
  frameMsP95: number;
  frameMsP99: number;
  frameMsMax: number;
  menuMsMax: number;
  capLimited: boolean;
}

function numberField(e: PerfEntry, name: string, fallback = 0): number {
  const Value = rawField(e, name);
  const Parsed = Number(Value);
  return Number.isFinite(Parsed) ? Parsed : fallback;
}

export function fpsSummaries(entries: PerfEntry[]): FpsSummary[] {
  return entries
    .filter(e => e.system === PERF_SYSTEM.FPS && field(e, 'event') === 'fps_summary')
    .map(e => ({
      timestamp: e.timestamp,
      operation: field(e, 'operation'),
      context: field(e, 'context'),
      page: field(e, 'page'),
      tab: field(e, 'tab', 'none'),
      sampleFrames: numberField(e, 'sample_frames'),
      sampleSeconds: numberField(e, 'sample_seconds'),
      fpsAvg: numberField(e, 'fps_avg'),
      fpsMin: numberField(e, 'fps_min'),
      fpsMax: numberField(e, 'fps_max'),
      frameMsAvg: numberField(e, 'frame_ms_avg'),
      frameMsP95: numberField(e, 'frame_ms_p95'),
      frameMsP99: numberField(e, 'frame_ms_p99'),
      frameMsMax: numberField(e, 'frame_ms_max'),
      menuMsMax: numberField(e, 'menu_ms_max'),
      capLimited: numberField(e, 'cap_limited') !== 0,
    }));
}

const TARGET_SETTINGS_FPS_OPERATIONS: ReadonlySet<string> = new Set([
  'settings_open',
  'settings_tab_switch',
  'settings_subtab_switch',
  'settings_tee_scroll',
  'ingame_esc_open',
]);

export function isTargetSettingsFpsSummary(summary: FpsSummary): boolean {
  return TARGET_SETTINGS_FPS_OPERATIONS.has(summary.operation);
}

export function hasOnlineTargetSettingsFpsSummary(entries: PerfEntry[]): boolean {
  return fpsSummaries(entries).some(s => isTargetSettingsFpsSummary(s) && s.context === 'online');
}

function normalizedPage(e: PerfEntry): string {
  return field(e, 'page', field(e, 'page_name')).toLowerCase();
}

function isSettingsTargetPage(Page: string): boolean {
  if(Page.length === 0) {
    return false;
  }
  if(Page.includes('internet') || Page.includes('server_browser') || Page === 'network') {
    return false;
  }
  return Page.startsWith('settings') ||
    ['general', 'tee', 'appearance', 'controls', 'graphics', 'sound', 'ddnet', 'assets', 'tclient', 'qmclient'].includes(Page);
}

export function selectTargetSettingsFrameEntries(entries: PerfEntry[]): PerfEntry[] {
  return selectFrameTimeEntries(entries).filter(e => {
    const Stage = e.stage.toLowerCase();
    const Page = normalizedPage(e);
    if(!isSettingsTargetPage(Page)) {
      return false;
    }
    return Stage.startsWith('settings_') ||
      (Stage === 'ingame_page_content' && (Page === 'settings' || Page.startsWith('settings')));
  });
}

export interface TargetSettingsSnapshot {
  percentiles: Percentiles;
  spikeCount: number;
  verdict: Verdict;
  verdictAvailable: boolean;
  stableTextCoverage: StableTextCoverageSummary;
}

export interface AssetsPreviewAdmissionSummary {
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
}

export interface AssetsVisibleReadySummary {
  available: boolean;
  visibleReadyAvailable: boolean;
  geometryStable: boolean;
  thumbStartsBeforeVisible: number;
  thumbStartsDuringDraw: number;
  visibleCount: number;
  notReadyCount: number;
  eventCount: number;
  samples: string[];
}

export interface DemoBrowserPhaseSummary {
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
}

export interface AdaptiveBudgetSummary {
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
}

export interface StableTextCoverageSummary {
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
}

export interface SettingsTextAnalysisLocationStats {
  location: string;
  page: string;
  tab: string;
  subtab: string;
  missCount: number;
  staleCount: number;
}

export interface SettingsTextAnalysisReasonStats {
  reason: string;
  missCount: number;
  staleCount: number;
}

export interface SettingsTextAnalysisOperationStats {
  operation: string;
  missCount: number;
  staleCount: number;
}

export interface SettingsTextAnalysisPrebuildSample {
  timestamp: string;
  scope: string;
  operation: string;
  built: number;
  reused: number;
  remaining: number;
  budget: number;
}

export interface SettingsTextAnalysisEventSample {
  timestamp: string;
  event: 'settings_text_prebuild' | 'settings_text_miss' | 'settings_text_stale' | 'settings_text_usage';
  page: string;
  tab: string;
  subtab: string;
  reason: string;
  planStatus: string;
  operation: string;
  built: number;
  reused: number;
  remaining: number;
  key: string;
}

export interface SettingsTextAnalysisSummary {
  missCount: number;
  staleCount: number;
  targetMissCount: number;
  targetStaleCount: number;
  prebuildCount: number;
  remainingPositiveCount: number;
}

export interface SettingsTextAnalysisResult {
  summary: SettingsTextAnalysisSummary;
  topByLocation: SettingsTextAnalysisLocationStats[];
  topByReason: SettingsTextAnalysisReasonStats[];
  topByOperation: SettingsTextAnalysisOperationStats[];
  prebuildSeries: SettingsTextAnalysisPrebuildSample[];
  eventTimeline: SettingsTextAnalysisEventSample[];
}

interface StableTextEvent {
  timestamp: string;
  event: 'settings_text_prebuild' | 'settings_text_miss' | 'settings_text_stale' | 'settings_text_usage' | 'settings_text_plan_collection';
  scope: string;
  page: string;
  tab: string;
  subtab: string;
  key: string;
  reason: string;
  operation: string;
  phase: string;
  remaining: number;
  candidates: number;
  hits: number;
  reused: number;
  miss: number;
  stale: number;
  textNew: number;
  textReused: number;
  planned: number;
  unplanned: number;
  unitsDone: number;
  unitsTotal: number;
  budget: number;
  complete: boolean;
  dirty: boolean;
  sample: string;
}

interface AdaptiveBudgetEvent {
  timestamp: string;
  mode: string;
  reason: string;
  frameMsAvg: number;
  frameMsP95: number;
  targetMs: number;
  visibleTokens: number;
  prefetchTokens: number;
  backgroundTokens: number;
  gpuUploadTokens: number;
  textTokens: number;
  demoTokens: number;
  backlog: number;
  scroll: number;
  jumpScroll: number;
  sample: string;
}

function isStableTextEvent(e: PerfEntry): boolean {
  const Event = field(e, 'event');
  return Event === 'settings_text_prebuild' || Event === 'settings_text_miss' || Event === 'settings_text_stale' || Event === 'settings_text_usage' || Event === 'settings_text_plan_collection';
}

function isAdaptiveBudgetEvent(e: PerfEntry): boolean {
  return e.system === 'perf/settings-resource' && field(e, 'event') === 'settings_adaptive_budget';
}

function stableTextEvents(entries: PerfEntry[]): StableTextEvent[] {
  return entries
    .filter(isStableTextEvent)
    .map(e => {
      const event = field(e, 'event') as StableTextEvent['event'];
      const scope = field(e, 'scope');
      const page = field(e, 'page');
      const tab = field(e, 'tab');
      const subtab = field(e, 'subtab');
      const key = field(e, 'key');
      const reason = field(e, 'reason');
      const planStatus = field(e, 'plan_status');
      const operation = field(e, 'operation');
      const phase = field(e, 'phase');
      const remaining = numberField(e, 'remaining');
      const candidates = numberField(e, 'candidates');
      const hits = numberField(e, 'hits');
      const reused = numberField(e, 'reused');
      const miss = numberField(e, 'miss');
      const stale = numberField(e, 'stale');
      const textNew = numberField(e, 'text_new');
      const textReused = numberField(e, 'text_reused');
      const planned = numberField(e, 'planned');
      const unplanned = numberField(e, 'unplanned');
      const unitsDone = numberField(e, 'units_done');
      const unitsTotal = numberField(e, 'units_total');
      const budget = numberField(e, 'budget');
      const complete = numberField(e, 'complete') > 0;
      const dirty = numberField(e, 'dirty') > 0;
      const parts = [
        `event=${event}`,
        scope.length > 0 ? `scope=${scope}` : '',
        page.length > 0 ? `page=${page}` : '',
        tab.length > 0 ? `tab=${tab}` : '',
        subtab.length > 0 ? `subtab=${subtab}` : '',
        key.length > 0 ? `key=${key}` : '',
        reason.length > 0 ? `reason=${reason}` : '',
        planStatus.length > 0 ? `plan_status=${planStatus}` : '',
        operation.length > 0 ? `operation=${operation}` : '',
        phase.length > 0 ? `phase=${phase}` : '',
        event === 'settings_text_prebuild' ? `remaining=${remaining}` : '',
        event === 'settings_text_usage' ? `candidates=${candidates}` : '',
        event === 'settings_text_usage' ? `hits=${hits}` : '',
        event === 'settings_text_usage' ? `reused=${reused}` : '',
        event === 'settings_text_usage' ? `miss=${miss}` : '',
        event === 'settings_text_usage' ? `stale=${stale}` : '',
        event === 'settings_text_usage' ? `planned=${planned}` : '',
        event === 'settings_text_usage' ? `unplanned=${unplanned}` : '',
        event === 'settings_text_plan_collection' ? `units_done=${unitsDone}` : '',
        event === 'settings_text_plan_collection' ? `units_total=${unitsTotal}` : '',
        event === 'settings_text_plan_collection' ? `remaining=${remaining}` : '',
        event === 'settings_text_plan_collection' ? `budget=${budget}` : '',
        event === 'settings_text_plan_collection' ? `complete=${complete ? 1 : 0}` : '',
        event === 'settings_text_plan_collection' ? `dirty=${dirty ? 1 : 0}` : '',
      ].filter(Boolean);
      return {
        timestamp: e.timestamp,
        event,
        scope,
        page,
        tab,
        subtab,
        key,
        reason,
        planStatus,
        operation,
        phase,
        remaining,
        candidates,
        hits,
        reused,
        miss,
        stale,
        textNew,
        textReused,
        planned,
        unplanned,
        unitsDone,
        unitsTotal,
        budget,
        complete,
        dirty,
        sample: parts.join(' '),
      };
    });
}

export function adaptiveBudgetSummary(entries: PerfEntry[]): AdaptiveBudgetSummary {
  const events: AdaptiveBudgetEvent[] = entries.filter(isAdaptiveBudgetEvent).map(e => {
    const mode = field(e, 'mode');
    const reason = field(e, 'reason');
    const frameMsAvg = numberField(e, 'frame_ms_avg');
    const frameMsP95 = numberField(e, 'frame_ms_p95');
    const targetMs = numberField(e, 'target_ms');
    const visibleTokens = numberField(e, 'visible_tokens');
    const prefetchTokens = numberField(e, 'prefetch_tokens');
    const backgroundTokens = numberField(e, 'background_tokens');
    const gpuUploadTokens = numberField(e, 'gpu_upload_tokens');
    const textTokens = numberField(e, 'text_tokens');
    const demoTokens = numberField(e, 'demo_tokens');
    const backlog = numberField(e, 'backlog');
    const scroll = numberField(e, 'scroll');
    const jumpScroll = numberField(e, 'jump_scroll');
    const sample = summaryKv(
      ['event', 'settings_adaptive_budget'],
      ['mode', mode],
      ['reason', reason],
      ['frame_ms_avg', frameMsAvg.toFixed(3)],
      ['frame_ms_p95', frameMsP95.toFixed(3)],
      ['target_ms', targetMs.toFixed(3)],
      ['visible_tokens', String(visibleTokens)],
      ['prefetch_tokens', String(prefetchTokens)],
      ['background_tokens', String(backgroundTokens)],
      ['gpu_upload_tokens', String(gpuUploadTokens)],
      ['text_tokens', String(textTokens)],
      ['demo_tokens', String(demoTokens)],
      ['backlog', String(backlog)],
      ['scroll', String(scroll)],
      ['jump_scroll', String(jumpScroll)],
    );
    return { timestamp: e.timestamp, mode, reason, frameMsAvg, frameMsP95, targetMs, visibleTokens, prefetchTokens, backgroundTokens, gpuUploadTokens, textTokens, demoTokens, backlog, scroll, jumpScroll, sample };
  });
  return {
    available: events.length > 0,
    eventCount: events.length,
    framePressureCount: events.filter(event => event.reason === 'frame_pressure').length,
    maxVisibleTokens: events.reduce((max, event) => Math.max(max, event.visibleTokens), 0),
    maxPrefetchTokens: events.reduce((max, event) => Math.max(max, event.prefetchTokens), 0),
    maxBackgroundTokens: events.reduce((max, event) => Math.max(max, event.backgroundTokens), 0),
    maxGpuUploadTokens: events.reduce((max, event) => Math.max(max, event.gpuUploadTokens), 0),
    maxTextTokens: events.reduce((max, event) => Math.max(max, event.textTokens), 0),
    maxDemoTokens: events.reduce((max, event) => Math.max(max, event.demoTokens), 0),
    samples: events.sort((a, b) => b.frameMsP95 - a.frameMsP95 || b.frameMsAvg - a.frameMsAvg).map(event => event.sample).slice(0, 8),
  };
}

function isTargetStableTextEvent(event: StableTextEvent): boolean {
  const scope = event.scope.toLowerCase();
  return scope.includes('target');
}

function stableTextCoverage(entries: PerfEntry[], targetFps: FpsSummary[]): StableTextCoverageSummary {
  const events = stableTextEvents(entries).sort((a, b) => a.timestamp.localeCompare(b.timestamp));
  const targetSummaryTimes = targetFps
    .map(summary => summary.timestamp)
    .filter(timestamp => timestamp.length > 0)
    .sort((a, b) => a.localeCompare(b));
  const firstTargetSummaryTime = targetSummaryTimes.length > 0
    ? targetSummaryTimes[0]
    : selectTargetSettingsFrameEntries(entries)
        .map(entry => entry.timestamp)
        .sort((a, b) => a.localeCompare(b))[0] ?? '';

  const relevantMisses = events.filter(event =>
    event.event === 'settings_text_miss' &&
    isTargetStableTextEvent(event));
  const relevantStales = events.filter(event =>
    event.event === 'settings_text_stale' &&
    isTargetStableTextEvent(event));
  const relevantUsage = events.filter(event =>
    event.event === 'settings_text_usage' &&
    isTargetStableTextEvent(event));
  const targetPrebuilds = events
    .filter(event =>
      event.event === 'settings_text_prebuild' &&
      isTargetStableTextEvent(event));
  const targetCollections = events
    .filter(event =>
      event.event === 'settings_text_plan_collection' &&
      isTargetStableTextEvent(event));
  const prebuildSnapshots = targetSummaryTimes.length > 0
    ? targetSummaryTimes
        .map(summaryTime => targetPrebuilds.filter(event => event.timestamp.localeCompare(summaryTime) <= 0).at(-1))
        .filter((event): event is StableTextEvent => event !== undefined)
    : firstTargetSummaryTime.length > 0
      ? targetPrebuilds.filter(event => event.timestamp.localeCompare(firstTargetSummaryTime) <= 0).slice(-1)
      : [];
  const prebuildBeforeTarget = prebuildSnapshots
    .sort((a, b) => b.remaining - a.remaining || a.timestamp.localeCompare(b.timestamp))
    .at(0);
  const prebuildRemainingBeforeTarget = prebuildBeforeTarget?.remaining ?? 0;
  const collectionSnapshots = targetSummaryTimes.length > 0
    ? targetSummaryTimes
        .map(summaryTime => targetCollections.filter(event => event.timestamp.localeCompare(summaryTime) <= 0).at(-1))
        .filter((event): event is StableTextEvent => event !== undefined)
    : firstTargetSummaryTime.length > 0
      ? targetCollections.filter(event => event.timestamp.localeCompare(firstTargetSummaryTime) <= 0).slice(-1)
      : [];
  const collectionBeforeTarget = collectionSnapshots
    .sort((a, b) => b.remaining - a.remaining || a.timestamp.localeCompare(b.timestamp))
    .at(0);
  const planCollectionAvailable = targetFps.length === 0 || targetCollections.length > 0;
  const planCollectionComplete = targetFps.length === 0 || (collectionBeforeTarget?.complete ?? false);
  const planCollectionUnitsTotal = collectionBeforeTarget?.unitsTotal ?? 0;
  const planCollectionUnitsDone = collectionBeforeTarget?.unitsDone ?? 0;
  const planCollectionRemainingBeforeTarget = collectionBeforeTarget?.remaining ?? 0;
  const planCollectionBudget = collectionBeforeTarget?.budget ?? 0;
  const planCollectionPhase = collectionBeforeTarget?.phase ?? '';
  const planCollectionScope = collectionBeforeTarget?.scope ?? '';
  const planCollectionOperation = collectionBeforeTarget?.operation ?? '';
  const candidateTotal = relevantUsage.reduce((sum, event) => sum + event.candidates, 0);
  const hitCount = relevantUsage.reduce((sum, event) => sum + event.hits, 0);
  const reuseCount = relevantUsage.reduce((sum, event) => sum + event.reused, 0);
  const usageMissCount = relevantUsage.reduce((sum, event) => sum + event.miss, 0);
  const usageStaleCount = relevantUsage.reduce((sum, event) => sum + event.stale, 0);
  const textNew = relevantUsage.reduce((sum, event) => sum + event.textNew, 0);
  const textReused = relevantUsage.reduce((sum, event) => sum + event.textReused, 0);
  const planCandidateCount = relevantUsage.reduce((sum, event) => sum + event.planned, 0);
  const unplannedVisibleCount = relevantUsage.reduce((sum, event) => sum + event.unplanned, 0);
  const visibleCandidateCount = candidateTotal;
  const hasPlanCounters = relevantUsage.some(event => event.planned > 0 || event.unplanned > 0);
  const planCoverageAvailable = targetFps.length === 0 || hasPlanCounters;
  const keyMismatchCount = [...relevantMisses, ...relevantStales].filter(event => event.planStatus === 'key_mismatch').length;
  const missCount = Math.max(relevantMisses.length, usageMissCount);
  const staleCount = Math.max(relevantStales.length, usageStaleCount);
  const utilizationAvailable = targetFps.length === 0 || relevantUsage.length > 0;
  const consistencyWarnings: string[] = [];
  if(targetFps.length > 0 && relevantUsage.length === 0) {
    consistencyWarnings.push('missing settings_text_usage for target settings window');
  }
  if(targetFps.length > 0 && relevantUsage.length > 0 && !planCoverageAvailable) {
    consistencyWarnings.push('missing stable text plan coverage counters for target settings window');
  }
  if(targetFps.length > 0 && !planCollectionAvailable) {
    consistencyWarnings.push('missing stable text plan collection counters for target settings window');
  }
  if(planCollectionAvailable && !planCollectionComplete) {
    consistencyWarnings.push(`stable text plan collection incomplete: ${planCollectionRemainingBeforeTarget}`);
  }
  if(unplannedVisibleCount > 0) {
    consistencyWarnings.push(`unplanned visible stable text candidates: ${unplannedVisibleCount}`);
  }
  if(keyMismatchCount > 0) {
    consistencyWarnings.push(`stable text key mismatches: ${keyMismatchCount}`);
  }
  if(candidateTotal > 0 && hitCount + missCount + staleCount < candidateTotal) {
    consistencyWarnings.push('stable text usage counters do not cover all candidates');
  }
  if(textNew > 0) {
    consistencyWarnings.push(`target stable text created ${textNew} text containers during visible usage`);
  }
  const samples = [...relevantMisses, ...relevantStales, ...(prebuildRemainingBeforeTarget > 0 && prebuildBeforeTarget ? [prebuildBeforeTarget] : []), ...(planCollectionRemainingBeforeTarget > 0 && collectionBeforeTarget ? [collectionBeforeTarget] : []), ...consistencyWarnings.map(warning => ({
    timestamp: firstTargetSummaryTime || '',
    event: 'settings_text_usage' as const,
    scope: 'target_settings',
    page: '',
    tab: '',
    subtab: '',
    key: '',
    reason: warning,
    planStatus: '',
    operation: '',
    phase: '',
    remaining: 0,
    candidates: 0,
    hits: 0,
    reused: 0,
    miss: 0,
    stale: 0,
    textNew: 0,
    textReused: 0,
    planned: 0,
    unplanned: 0,
    unitsDone: 0,
    unitsTotal: 0,
    budget: 0,
    complete: false,
    dirty: false,
    sample: `event=settings_text_usage scope=target_settings reason=${warning.replaceAll(' ', '_')}`,
  }))]
    .sort((a, b) => a.timestamp.localeCompare(b.timestamp))
    .map(event => event.sample)
    .slice(0, 8);

  return {
    acceptanceBlocked: missCount > 0 || staleCount > 0 || prebuildRemainingBeforeTarget > 0 || !planCollectionAvailable || !planCollectionComplete || !utilizationAvailable || !planCoverageAvailable || unplannedVisibleCount > 0 || keyMismatchCount > 0 || textNew > 0,
    utilizationAvailable,
    planCoverageAvailable,
    planCandidateCount,
    visibleCandidateCount,
    unplannedVisibleCount,
    keyMismatchCount,
    candidateTotal,
    hitCount,
    reuseCount,
    missCount,
    staleCount,
    hitRate: candidateTotal > 0 ? (hitCount / candidateTotal) * 100 : 0,
    reuseRate: candidateTotal > 0 ? (reuseCount / candidateTotal) * 100 : 0,
    textNew,
    textReused,
    prebuildRemainingBeforeTarget,
    planCollectionAvailable,
    planCollectionComplete,
    planCollectionUnitsTotal,
    planCollectionUnitsDone,
    planCollectionRemainingBeforeTarget,
    planCollectionBudget,
    planCollectionPhase,
    planCollectionScope,
    planCollectionOperation,
    consistencyWarnings,
    samples,
  };
}

function stableTextEventsDetailed(entries: PerfEntry[]) {
  return entries
    .filter(isStableTextEvent)
    .map(e => ({
      timestamp: e.timestamp,
      event: field(e, 'event') as 'settings_text_prebuild' | 'settings_text_miss' | 'settings_text_stale' | 'settings_text_usage',
      scope: field(e, 'scope'),
      page: field(e, 'page'),
      tab: field(e, 'tab'),
      subtab: field(e, 'subtab'),
      reason: field(e, 'reason'),
      planStatus: field(e, 'plan_status'),
      operation: field(e, 'operation'),
      built: numberField(e, 'built'),
      reused: numberField(e, 'reused'),
      remaining: numberField(e, 'remaining'),
      budget: numberField(e, 'budget'),
      key: field(e, 'key'),
    }));
}

function stableTextLocation(page: string, tab: string, subtab: string): string {
  return `${page || '-'} / ${tab || '-'} / ${subtab || '-'}`;
}

export function settingsTextAnalysis(entries: PerfEntry[]): SettingsTextAnalysisResult {
  const events = stableTextEventsDetailed(entries).sort((a, b) => a.timestamp.localeCompare(b.timestamp));
  const missCount = events.filter(event => event.event === 'settings_text_miss').length;
  const staleCount = events.filter(event => event.event === 'settings_text_stale').length;
  const targetMissCount = events.filter(event => event.event === 'settings_text_miss' && String(event.scope).includes('target')).length;
  const targetStaleCount = events.filter(event => event.event === 'settings_text_stale' && String(event.scope).includes('target')).length;
  const prebuildCount = events.filter(event => event.event === 'settings_text_prebuild').length;
  const remainingPositiveCount = events.filter(event => event.event === 'settings_text_prebuild' && event.remaining > 0).length;

  const byLocation = new Map<string, SettingsTextAnalysisLocationStats>();
  const byReason = new Map<string, SettingsTextAnalysisReasonStats>();
  const byOperation = new Map<string, SettingsTextAnalysisOperationStats>();
  const prebuildSeries: SettingsTextAnalysisPrebuildSample[] = [];

  for (const event of events) {
    const location = stableTextLocation(event.page, event.tab, event.subtab);
    const locationStats = byLocation.get(location) ?? {
      location,
      page: event.page || '-',
      tab: event.tab || '-',
      subtab: event.subtab || '-',
      missCount: 0,
      staleCount: 0,
    };
    const reasonStats = byReason.get(event.reason || '-') ?? {
      reason: event.reason || '-',
      missCount: 0,
      staleCount: 0,
    };
    const operationStats = byOperation.get(event.operation || '-') ?? {
      operation: event.operation || '-',
      missCount: 0,
      staleCount: 0,
    };

    if (event.event === 'settings_text_miss') {
      locationStats.missCount += 1;
      reasonStats.missCount += 1;
      operationStats.missCount += 1;
    } else if (event.event === 'settings_text_stale') {
      locationStats.staleCount += 1;
      reasonStats.staleCount += 1;
      operationStats.staleCount += 1;
    } else if (event.event === 'settings_text_prebuild') {
      prebuildSeries.push({
        timestamp: event.timestamp,
        scope: event.scope || '-',
        operation: event.operation || '-',
        built: event.built,
        reused: event.reused,
        remaining: event.remaining,
        budget: event.budget,
      });
    }

    byLocation.set(location, locationStats);
    byReason.set(reasonStats.reason, reasonStats);
    byOperation.set(operationStats.operation, operationStats);
  }

  const topByLocation = [...byLocation.values()].sort((a, b) => b.missCount - a.missCount || b.staleCount - a.staleCount || a.location.localeCompare(b.location));
  const topByReason = [...byReason.values()].sort((a, b) => b.missCount - a.missCount || b.staleCount - a.staleCount || a.reason.localeCompare(b.reason));
  const topByOperation = [...byOperation.values()].sort((a, b) => b.missCount - a.missCount || b.staleCount - a.staleCount || a.operation.localeCompare(b.operation));

  return {
    summary: {
      missCount,
      staleCount,
      targetMissCount,
      targetStaleCount,
      prebuildCount,
      remainingPositiveCount,
    },
    topByLocation,
    topByReason,
    topByOperation,
    prebuildSeries,
    eventTimeline: events.map(event => ({
      timestamp: event.timestamp,
      event: event.event,
      page: event.page || '-',
      tab: event.tab || '-',
      subtab: event.subtab || '-',
      reason: event.reason || '-',
      planStatus: event.planStatus || '-',
      operation: event.operation || '-',
      built: event.built,
      reused: event.reused,
      remaining: event.remaining,
      key: event.key || '-',
    })),
  };
}

export function targetSettingsSnapshot(entries: PerfEntry[]): TargetSettingsSnapshot {
  const targetFps = fpsSummaries(entries).filter(isTargetSettingsFpsSummary);
  const stableText = stableTextCoverage(entries, targetFps);
  if(targetFps.length > 0) {
    const durations = targetFps.map(s => s.frameMsP99);
    const percentiles = calcPercentiles(durations);
    const spikeCount = targetFps.filter(s => s.frameMsMax > BUDGET.h60).length;
    return {
      percentiles,
      spikeCount,
      verdict: computeVerdict(percentiles, spikeCount),
      verdictAvailable: true,
      stableTextCoverage: stableText,
    };
  }
  const targetEntries = selectTargetSettingsFrameEntries(entries);
  const durations = targetEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const percentiles = calcPercentiles(durations);
  const spikes = detectSpikes(targetEntries, BUDGET.h60);
  return {
    percentiles,
    spikeCount: spikes.length,
    verdict: 'WARN',
    verdictAvailable: false,
    stableTextCoverage: stableText,
  };
}

export function assetsPreviewAdmissionSummary(entries: PerfEntry[]): AssetsPreviewAdmissionSummary {
  const events = entries
    .filter(entry => entry.system === 'perf/assets' && field(entry, 'stage') === 'assets_preview_draw_workshop_cards')
    .map(entry => {
      const visibleFirst = numberField(entry, 'visible_first');
      const rendered = numberField(entry, 'rendered');
      const thumbStarts = numberField(entry, 'thumb_starts');
      const visibleStarts = numberField(entry, 'visible_starts');
      const prefetchStarts = numberField(entry, 'prefetch_starts');
      const backgroundStarts = numberField(entry, 'background_starts');
      const durationMs = entryDurationMs(entry) ?? 0;
      const sample = [
        'stage=assets_preview_draw_workshop_cards',
        `tab=${field(entry, 'tab')}`,
        `combined=${field(entry, 'combined')}`,
        `rendered=${rendered}`,
        `thumb_starts=${thumbStarts}`,
        `visible_first=${visibleFirst}`,
        `visible_starts=${visibleStarts}`,
        `prefetch_starts=${prefetchStarts}`,
        `background_starts=${backgroundStarts}`,
        `duration_ms=${durationMs.toFixed(3)}`,
      ].join(' ');
      return { visibleFirst, rendered, thumbStarts, visibleStarts, prefetchStarts, backgroundStarts, durationMs, sample };
    });
  return {
    available: events.length > 0,
    visibleFirstAvailable: events.some(event => event.visibleFirst === 1),
    eventCount: events.length,
    maxDurationMs: events.reduce((max, event) => Math.max(max, event.durationMs), 0),
    maxRendered: events.reduce((max, event) => Math.max(max, event.rendered), 0),
    maxThumbStarts: events.reduce((max, event) => Math.max(max, event.thumbStarts), 0),
    visibleStarts: events.reduce((sum, event) => sum + event.visibleStarts, 0),
    prefetchStarts: events.reduce((sum, event) => sum + event.prefetchStarts, 0),
    backgroundStarts: events.reduce((sum, event) => sum + event.backgroundStarts, 0),
    samples: events
      .sort((a, b) => b.durationMs - a.durationMs)
      .map(event => event.sample)
      .slice(0, 8),
  };
}

export function assetsVisibleReadySummary(entries: PerfEntry[]): AssetsVisibleReadySummary {
  const preflightEvents = entries
    .filter(entry => entry.system === 'perf/assets' && field(entry, 'stage') === 'assets_visible_preflight')
    .map(entry => {
      const visibleReady = numberField(entry, 'visible_ready');
      const geometryStable = numberField(entry, 'geometry_stable');
      const thumbStartsBeforeVisible = numberField(entry, 'thumb_starts_before_visible');
      const thumbStartsDuringDraw = numberField(entry, 'thumb_starts_during_draw');
      const visibleCount = numberField(entry, 'visible_count');
      const notReadyCount = numberField(entry, 'not_ready_count');
      const sample = [
        'stage=assets_visible_preflight',
        `tab=${field(entry, 'tab')}`,
        `visible_ready=${visibleReady}`,
        `geometry_stable=${geometryStable}`,
        `visible_count=${visibleCount}`,
        `not_ready_count=${notReadyCount}`,
        `thumb_starts_before_visible=${thumbStartsBeforeVisible}`,
        `thumb_starts_during_draw=${thumbStartsDuringDraw}`,
      ].join(' ');
      return { visibleReady, geometryStable, hasGeometryStable: hasField(entry, 'geometry_stable'), thumbStartsBeforeVisible, thumbStartsDuringDraw, visibleCount, notReadyCount, sample };
    });
  const drawEvents = entries
    .filter(entry => entry.system === 'perf/assets' && field(entry, 'stage') === 'assets_preview_draw_workshop_cards')
    .map(entry => ({
      thumbStartsDuringDraw: numberField(entry, 'thumb_starts_during_draw'),
      geometryStable: numberField(entry, 'geometry_stable'),
      hasGeometryStable: hasField(entry, 'geometry_stable'),
      rendered: numberField(entry, 'rendered'),
    }));
  const thumbStartsDuringDraw = preflightEvents.reduce((sum, event) => sum + event.thumbStartsDuringDraw, 0) +
    drawEvents.reduce((sum, event) => sum + event.thumbStartsDuringDraw, 0);
  if (thumbStartsDuringDraw > 0) {
    // thumbStartsDuringDraw > 0 blocks visible-ready acceptance.
  }
  const relevantPreflightEvents = preflightEvents.filter(event => event.visibleCount > 0 && event.hasGeometryStable);
  const relevantDrawEvents = drawEvents.filter(event => event.rendered > 0 && event.hasGeometryStable);
  const geometryStableIsFalse = relevantPreflightEvents.some(event => event.geometryStable === 0) ||
    relevantDrawEvents.some(event => event.geometryStable === 0);
  const geometryStable = (relevantPreflightEvents.length > 0 || relevantDrawEvents.length > 0) && !geometryStableIsFalse;
  if (geometryStable === false) {
    // Keep this condition explicit because geometry instability blocks visible-ready acceptance.
  }
  const lastPreflight = preflightEvents.at(-1);
  const visibleReadyAvailable = preflightEvents.length > 0 &&
    lastPreflight !== undefined &&
    lastPreflight.visibleReady === 1 &&
    lastPreflight.notReadyCount === 0;
  const notReadyCount = lastPreflight !== undefined ? lastPreflight.notReadyCount : 0;
  return {
    available: preflightEvents.length > 0,
    visibleReadyAvailable,
    geometryStable,
    thumbStartsBeforeVisible: preflightEvents.reduce((sum, event) => sum + event.thumbStartsBeforeVisible, 0),
    thumbStartsDuringDraw,
    visibleCount: preflightEvents.reduce((sum, event) => Math.max(sum, event.visibleCount), 0),
    notReadyCount,
    eventCount: preflightEvents.length,
    samples: preflightEvents.map(event => event.sample).slice(0, 8),
  };
}

function isDemoBrowserPhaseEvent(e: PerfEntry): boolean {
  const event = eventName(e);
  return e.system === PERF_SYSTEM.INTERACTION && (
    event === 'demo_browser_startup' ||
    event === 'demo_browser_header_fetch' ||
    event === 'demo_browser_date_fetch' ||
    event === 'demo_browser_preview_load'
  );
}

export function demoBrowserPhaseSummary(entries: PerfEntry[]): DemoBrowserPhaseSummary {
  const events = entries.filter(isDemoBrowserPhaseEvent);
  const samples = events.slice(0, 12).map(e => summaryKv(
    ['event', eventName(e)],
    ['items_total', field(e, 'items_total')],
    ['items_scanned', field(e, 'items_scanned')],
    ['items_done', field(e, 'items_done')],
    ['visible_first', field(e, 'visible_first')],
    ['visible_end', field(e, 'visible_end')],
    ['visible_scanned', field(e, 'visible_scanned')],
    ['visible_done', field(e, 'visible_done')],
    ['background_scanned', field(e, 'background_scanned')],
    ['background_done', field(e, 'background_done')],
    ['remaining', field(e, 'remaining')],
    ['metadata_remaining', field(e, 'metadata_remaining')],
    ['budget', field(e, 'budget')],
    ['dur_ms', String(fieldDuration(e))],
    ['trigger', field(e, 'trigger')],
    ['source', field(e, 'source')],
    ['sort', field(e, 'sort')],
    ['fetch_info', field(e, 'fetch_info')],
  ));
  return {
    available: events.length > 0,
    startupCount: events.filter(e => eventName(e) === 'demo_browser_startup').length,
    headerFetchCount: events.filter(e => eventName(e) === 'demo_browser_header_fetch').length,
    dateFetchCount: events.filter(e => eventName(e) === 'demo_browser_date_fetch').length,
    previewLoadCount: events.filter(e => eventName(e) === 'demo_browser_preview_load').length,
    visibleScanned: events.reduce((sum, e) => sum + numberField(e, 'visible_scanned'), 0),
    visibleDone: events.reduce((sum, e) => sum + numberField(e, 'visible_done'), 0),
    backgroundScanned: events.reduce((sum, e) => sum + numberField(e, 'background_scanned'), 0),
    backgroundDone: events.reduce((sum, e) => sum + numberField(e, 'background_done'), 0),
    maxDurationMs: events.reduce((max, e) => Math.max(max, fieldDuration(e)), 0),
    maxRemaining: events.reduce((max, e) => Math.max(max, numberField(e, 'remaining')), 0),
    maxMetadataRemaining: events.reduce((max, e) => Math.max(max, numberField(e, 'metadata_remaining')), 0),
    samples,
  };
}

export function detectSpikes(entries: PerfEntry[], thresholdMs: number = BUDGET.h60): SpikeInfo[] {
  return entries
    .map((e, i) => ({
      index: i,
      timestamp: e.timestamp,
      durationMs: entryDurationMs(e) ?? e.durationMs,
      stage: e.stage,
      page: e.fields.page ?? '',
      threshold: thresholdMs,
    }))
    .filter(s => s.durationMs > thresholdMs)
    .sort((a, b) => b.durationMs - a.durationMs);
}

export function histogram(values: number[], bucketEdges: number[]): { label: string; count: number }[] {
  const buckets = bucketEdges.map((edge, i) => ({
    label: i < bucketEdges.length - 1 ? `${edge}-${bucketEdges[i + 1]}ms` : `${edge}+ms`,
    min: edge,
    max: bucketEdges[i + 1] ?? Infinity,
    count: 0,
  }));
  for (const v of values) {
    for (const b of buckets) {
      if (v >= b.min && v < b.max) { b.count++; break; }
    }
  }
  return buckets;
}

/** 页面级统计 */
export interface PageStats {
  page: string;
  count: number;
  avg: number;
  min: number;
  max: number;
  p95: number;
  spikes: number;
  /** 箱线图五数: [min, Q1, median, Q3, max] */
  boxPlot: [number, number, number, number, number];
  /** 离群点 */
  outliers: number[];
}

export type AttributionKind = 'List Interaction' | 'UI Rebuild' | 'Work Drain' | 'UI Runtime' | 'Session Summary';

export interface AttributionEntry {
  kind: AttributionKind;
  timestamp: string;
  page: string;
  durationMs: number;
  summary: string;
  details: string;
}

export interface SectionPerfStats {
  page: string;
  section: string;
  count: number;
  avg: number;
  p95: number;
  max: number;
}

function fieldDuration(e: PerfEntry): number {
  const Raw = e.fields.dur_ms ?? e.fields.duration_ms ?? e.fields.dur ?? String(e.durationMs);
  const Parsed = Number(Raw);
  return Number.isFinite(Parsed) ? Parsed : 0;
}

function field(e: PerfEntry, name: string, fallback = ''): string {
  const Value = e.fields[name];
  return Value === undefined || Value === null ? fallback : String(Value);
}

function eventName(e: PerfEntry): string {
  return field(e, 'event');
}

export function isPageSwitchEvent(e: PerfEntry): boolean {
  const event = eventName(e);
  return event === PERF_EVENT.PAGE_SWITCH || event.endsWith('_page_switch');
}

export function isListFrameEvent(e: PerfEntry): boolean {
  return eventName(e) === PERF_EVENT.LIST_FRAME;
}

export function isUiRebuildEvent(e: PerfEntry): boolean {
  return e.system === PERF_SYSTEM.SECTION;
}

export function isWorkDrainEvent(e: PerfEntry): boolean {
  const event = eventName(e);
  return event === PERF_EVENT.WORK_DRAIN || event === PERF_EVENT.LIST_DRAIN_SUMMARY;
}

function summaryKv(...pairs: [string, string][]): string {
  return pairs
    .filter(([, value]) => value.length > 0)
    .map(([key, value]) => `${key}=${value}`)
    .join(' ');
}

function makeEntry(
  e: PerfEntry,
  kind: AttributionKind,
  summary: string,
  details = '',
): AttributionEntry {
  return {
    kind,
    timestamp: e.timestamp,
    page: field(e, 'page', 'unknown'),
    durationMs: fieldDuration(e),
    summary,
    details,
  };
}

export function pagePerformanceAttribution(entries: PerfEntry[]): AttributionEntry[] {
  const attribution: AttributionEntry[] = [];
  for (const e of entries) {
    const event = eventName(e);
    if (isPageSwitchEvent(e)) {
      continue;
    } else if (e.system === PERF_SYSTEM.UI_RUNTIME) {
      attribution.push({
        kind: 'UI Runtime',
        timestamp: e.timestamp,
        page: field(e, 'page', 'unknown'),
        durationMs: entryDurationMs(e) ?? e.durationMs,
        summary: summaryKv(
          ['operation', field(e, 'operation')],
          ['nodes', field(e, 'nodes')],
          ['active_anims', field(e, 'active_anims')],
        ),
        details: summaryKv(
          ['anim_ms', field(e, 'anim_ms')],
          ['render_bridge_ms', field(e, 'render_bridge_ms')],
          ['queued_anims', field(e, 'queued_anims')],
        ),
      });
    } else if (isListFrameEvent(e)) {
      attribution.push(makeEntry(
        e,
        'List Interaction',
        summaryKv(
          ['items_total', field(e, 'items_total')],
          ['rows_visible', field(e, 'rows_visible')],
        ),
        summaryKv(
          ['rows_rendered', field(e, 'rows_rendered')],
          ['rows_iterated', field(e, 'rows_iterated')],
          ['rows_processed', field(e, 'rows_processed')],
          ['rows_skipped', field(e, 'rows_skipped')],
        ),
      ));
    } else if (isDemoBrowserPhaseEvent(e)) {
      attribution.push(makeEntry(
        e,
        'List Interaction',
        summaryKv(
          ['event', event],
          ['items_total', field(e, 'items_total')],
          ['remaining', field(e, 'remaining')],
        ),
        summaryKv(
          ['items_scanned', field(e, 'items_scanned')],
          ['items_done', field(e, 'items_done')],
          ['budget', field(e, 'budget')],
          ['trigger', field(e, 'trigger')],
        ),
      ));
    } else if (isUiRebuildEvent(e)) {
      attribution.push(makeEntry(
        e,
        'UI Rebuild',
        summaryKv(
          ['section', field(e, 'section')],
          ['visible', field(e, 'visible')],
        ),
        summaryKv(
          ['dirty', field(e, 'dirty')],
          ['text_new', field(e, 'text_new')],
          ['text_reused', field(e, 'text_reused')],
        ),
      ));
    } else if (isWorkDrainEvent(e)) {
      const SessionScoped = field(e, 'scope') === 'session';
      attribution.push(makeEntry(
        e,
        SessionScoped ? 'Session Summary' : 'Work Drain',
        summaryKv(
          ['kind', field(e, 'kind', event === PERF_EVENT.LIST_DRAIN_SUMMARY ? 'merge' : '')],
          ['count', field(e, 'count')],
          ['bytes', field(e, 'bytes')],
          ['stop', field(e, 'stop')],
        ),
        summaryKv(
          ['source', field(e, 'source', event === PERF_EVENT.LIST_DRAIN_SUMMARY ? PERF_EVENT.LIST_DRAIN_SUMMARY : '')],
          ['requested', field(e, 'requested')],
          ['pending', field(e, 'pending')],
          ['loading', field(e, 'loading')],
          ['loaded', field(e, 'loaded')],
        ),
      ));
    }
  }
  return attribution.sort((a, b) => b.durationMs - a.durationMs);
}

export function sectionPerformanceTop(entries: PerfEntry[], limit: number = 10): SectionPerfStats[] {
  const bySection = new Map<string, { page: string; section: string; durations: number[] }>();
  for (const e of entries) {
    if (!isUiRebuildEvent(e)) {
      continue;
    }
    const page = field(e, 'page', 'unknown');
    const section = field(e, 'section', 'unknown');
    const key = `${page}\u0000${section}`;
    const bucket = bySection.get(key) ?? { page, section, durations: [] };
    bucket.durations.push(fieldDuration(e));
    bySection.set(key, bucket);
  }

  return [...bySection.values()]
    .map(({ page, section, durations }) => {
      const p = calcPercentiles(durations);
      return {
        page,
        section,
        count: p.count,
        avg: p.avg,
        p95: p.p95,
        max: p.max,
      };
    })
    .sort((a, b) => b.p95 - a.p95 || b.max - a.max || b.avg - a.avg)
    .slice(0, limit);
}

export function pageBreakdown(entries: PerfEntry[], spikeThreshold: number = BUDGET.h60): PageStats[] {
  const byPage = new Map<string, PerfEntry[]>();
  for (const e of entries) {
    const page = e.fields.page ?? e.fields.page_name ?? 'unknown';
    const list = byPage.get(page) ?? [];
    list.push(e);
    byPage.set(page, list);
  }

  const stats: PageStats[] = [];
  for (const [page, list] of byPage) {
    const durations = list.map(e => entryDurationMs(e) ?? e.durationMs);
    const p = calcPercentiles(durations);
    const bp = boxPlotStats(durations);
    stats.push({
      page,
      count: list.length,
      avg: p.avg,
      min: bp.whiskerLow,
      max: bp.whiskerHigh,
      p95: p.p95,
      spikes: durations.filter(d => d > spikeThreshold).length,
      boxPlot: [bp.whiskerLow, bp.q1, bp.median, bp.q3, bp.whiskerHigh],
      outliers: bp.outliers,
    });
  }
  return stats.sort((a, b) => b.avg - a.avg);
}

/** 箱线图统计 (Tukey) */
export interface BoxPlotResult {
  q1: number;
  median: number;
  q3: number;
  iqr: number;
  whiskerLow: number;
  whiskerHigh: number;
  outliers: number[];
}

export function boxPlotStats(values: number[]): BoxPlotResult {
  const sorted = [...values].sort((a, b) => a - b);
  const n = sorted.length;
  if (n === 0) return { q1: 0, median: 0, q3: 0, iqr: 0, whiskerLow: 0, whiskerHigh: 0, outliers: [] };

  const rank = (q: number) => sorted[Math.min(n - 1, Math.ceil((q / 100) * n) - 1)];
  const q1 = rank(25);
  const median = rank(50);
  const q3 = rank(75);
  const iqr = q3 - q1;
  const fenceLow = q1 - 1.5 * iqr;
  const fenceHigh = q3 + 1.5 * iqr;

  const inliers = sorted.filter(v => v >= fenceLow && v <= fenceHigh);
  const outliers = sorted.filter(v => v < fenceLow || v > fenceHigh);

  return {
    q1,
    median,
    q3,
    iqr,
    whiskerLow: inliers.length > 0 ? inliers[0] : sorted[0],
    whiskerHigh: inliers.length > 0 ? inliers[inliers.length - 1] : sorted[n - 1],
    outliers,
  };
}

/** 核密度估计 (Gaussian KDE, Silverman bandwidth) */
export function kde(values: number[], nPoints: number = 80): { x: number; y: number }[] {
  if (values.length === 0) return [];
  const n = values.length;
  const sorted = [...values].sort((a, b) => a - b);
  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const std = Math.sqrt(sorted.reduce((s, v) => s + (v - mean) ** 2, 0) / n);
  // Silverman's rule of thumb
  const h = std > 0 ? 1.06 * std * Math.pow(n, -0.2) : 1;

  const lo = Math.max(0, sorted[0] - 3 * h);
  const hi = sorted[n - 1] + 3 * h;
  const step = (hi - lo) / (nPoints - 1);

  const points: { x: number; y: number }[] = [];
  for (let i = 0; i < nPoints; i++) {
    const x = lo + i * step;
    let density = 0;
    for (const v of values) {
      const u = (x - v) / h;
      density += Math.exp(-0.5 * u * u);
    }
    density /= (n * h * Math.sqrt(2 * Math.PI));
    points.push({ x: parseFloat(x.toFixed(3)), y: parseFloat(density.toFixed(6)) });
  }
  return points;
}

/** QQ 图数据（相对正态分布） */
export function qqNorm(values: number[]): { theoretical: number; sample: number }[] {
  const n = values.length;
  if (n === 0) return [];
  const sorted = [...values].sort((a, b) => a - b);
  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const std = Math.sqrt(sorted.reduce((s, v) => s + (v - mean) ** 2, 0) / n);

  // 采样最多 500 个点，避免大数据量渲染缓慢
  const step = Math.max(1, Math.floor(n / 500));
  const result: { theoretical: number; sample: number }[] = [];
  for (let i = 0; i < n; i += step) {
    const p = (i + 0.5) / n;
    // 正态分布逆CDF近似 (Beasley-Springer-Moro)
    const z = normInv(p);
    result.push({ theoretical: mean + std * z, sample: sorted[i] });
  }
  return result;
}

/** 标准正态逆CDF近似 */
function normInv(p: number): number {
  // Rational approximation (Abramowitz & Stegun)
  if (p <= 0) return -4;
  if (p >= 1) return 4;
  const a = [-3.969683028665376e1, 2.209460984245205e2, -2.759285104469687e2, 1.383577518672690e2, -3.066479806614716e1, 2.506628277459239e0];
  const b = [-5.447609879822406e1, 1.615858368580409e2, -1.556989798598866e2, 6.680131188771972e1, -1.328068155288572e1];
  const c = [-7.784894002430293e-3, -3.223964580411365e-1, -2.400758277161838e0, -2.549732539343734e0, 4.374664141464968e0, 2.938163982698783e0];
  const d = [7.784695709041462e-3, 3.224671290700398e-1, 2.445134137142996e0, 3.754408661907416e0];

  const pLow = 0.02425, pHigh = 1 - pLow;
  let q, r;
  if (p < pLow) {
    q = Math.sqrt(-2 * Math.log(p));
    return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
  } else if (p <= pHigh) {
    q = p - 0.5; r = q * q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q / (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
  } else {
    q = Math.sqrt(-2 * Math.log(1 - p));
    return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
  }
}

/** 整体性能判定 */
export type Verdict = 'PASS' | 'WARN' | 'FAIL';

export function computeVerdict(p: Percentiles, spikeCount: number): Verdict {
  if (p.p99 >= BUDGET.h60Double || spikeCount >= 5) return 'FAIL';
  if (p.p99 >= BUDGET.h60 || spikeCount >= 1) return 'WARN';
  return 'PASS';
}

/** 生成自动叙事文本 */
export function generateNarrative(p: Percentiles, spikes: SpikeInfo[], compliance240: number, compliance120: number, compliance60: number, biased: boolean, samplingThresholdMs: number = p.min): string {
  if (p.count === 0) {
    return '本次 session 没有可用的 frame-time 样本，无法判断渲染性能。请确认日志包含 perf/menu 或 perf/gameclient 的 duration_ms 数据后重新生成报告。';
  }

  const verdict = computeVerdict(p, spikes.length);
  const verdictText = verdict === 'PASS' ? '性能表现良好' : verdict === 'WARN' ? '存在轻微性能问题' : '存在显著性能问题';

  const lines: string[] = [];
  lines.push(`本次 session 共采集 ${p.count} 帧渲染数据，整体 ${verdictText}。`);

  if (biased) {
    lines.push(`注意：当前采样阈值估计 p5=${samplingThresholdMs.toFixed(1)}ms（当前默认 4ms），日志可能仅包含超过阈值的帧。实际合规率可能高于日志所示。建议确认 qm_perf_debug_threshold_ms 4 后重新采集完整帧分布。`);
  } else {
    lines.push(`帧预算合规率：240Hz (4.17ms) 为 ${compliance240.toFixed(1)}%，120Hz (8.33ms) 为 ${compliance120.toFixed(1)}%，60Hz (16.67ms) 为 ${compliance60.toFixed(1)}%。`);
  }

  if (spikes.length > 0) {
    const worst = spikes[0];
    lines.push(`检测到 ${spikes.length} 个性能尖峰（>16.67ms），最大尖峰耗时 ${worst.durationMs.toFixed(1)}ms（超标 ${(worst.durationMs / 16.67).toFixed(1)}x），出现在 ${worst.page || '未知'} 页面。`);
  } else {
    lines.push('未检测到超过 16.67ms 阈值的性能尖峰。');
  }

  lines.push(`中位数帧耗时 ${p.p50.toFixed(1)}ms；p99 达 ${p.p99.toFixed(1)}ms，反映尾部帧体验。`);

  return lines.join(' ');
}

// ── 会话对比 ─────────────────────────────────────

export interface SessionSnapshot {
  /** 来源日志文件名 */
  file: string;
  /** 百分位 */
  percentiles: Percentiles;
  /** 尖峰数 */
  spikeCount: number;
  /** 尖峰列表（Top-5） */
  topSpikes: SpikeInfo[];
  /** 帧预算合规率 */
  compliance: { h240: number; h120: number; h60: number };
  /** 判定 */
  verdict: Verdict;
  /** 总帧数 */
  totalFrames: number;
  /** 操作路径签名，用于判断两个日志是否可严格对比 */
  operation: OperationSignature;
}

/** 从 entries 生成会话快照 */
export function snapshot(entries: PerfEntry[], sourceFile: string): SessionSnapshot {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const p = calcPercentiles(durations);
  const spikes = detectSpikes(frameEntries, BUDGET.h60);
  return {
    file: sourceFile,
    percentiles: p,
    spikeCount: spikes.length,
    topSpikes: spikes.slice(0, 5),
    compliance: {
      h240: complianceRate(durations, BUDGET.h240),
      h120: complianceRate(durations, BUDGET.h120),
      h60: complianceRate(durations, BUDGET.h60),
    },
    verdict: frameEntries.length === 0 ? 'WARN' : computeVerdict(p, spikes.length),
    totalFrames: frameEntries.length,
    operation: operationSignature(entries),
  };
}

export interface MetricDelta {
  /** 指标名 */
  name: string;
  /** 旧值 */
  before: number;
  /** 新值 */
  after: number;
  /** 差值 (after - before) */
  delta: number;
  /** 变化百分比 */
  changePercent: number;
  /** good/bad/neutral — 正数不一定好（spikes 增加 = 坏） */
  direction: 'better' | 'worse' | 'neutral';
}

export interface ComparisonResult {
  previous: SessionSnapshot;
  current: SessionSnapshot;
  /** 通用指标对比（p50/p95/p99/max/spikes） */
  metrics: MetricDelta[];
  /** 合规率对比 */
  compliance: MetricDelta[];
  /** 判定变化 */
  verdictChanged: boolean;
  /** 自动生成的对比叙事 */
  narrative: string;
  /** 对比是否来自同一类操作路径 */
  operation: OperationComparison;
}

function calcDelta(name: string, before: number, after: number, lowerIsBetter: boolean): MetricDelta {
  const delta = after - before;
  const changePercent = before !== 0 ? (delta / before) * 100 : 0;
  const direction = Math.abs(delta) < 0.001 ? 'neutral'
    : lowerIsBetter ? (delta < 0 ? 'better' : 'worse')
    : (delta > 0 ? 'better' : 'worse');
  return { name, before, after, delta, changePercent, direction };
}

/** 对比两个 session */
export function compareSessions(previous: SessionSnapshot, current: SessionSnapshot): ComparisonResult {
  const p = previous.percentiles;
  const c = current.percentiles;

  const metrics: MetricDelta[] = [
    calcDelta('p50', p.p50, c.p50, true),
    calcDelta('p95', p.p95, c.p95, true),
    calcDelta('p99', p.p99, c.p99, true),
    calcDelta('max', p.max, c.max, true),
    calcDelta('spikes', previous.spikeCount, current.spikeCount, true),
    calcDelta('mean', p.avg, c.avg, true),
    calcDelta('stddev', p.std, c.std, true),
  ];

  const compliance: MetricDelta[] = [
    calcDelta('240Hz 合规', previous.compliance.h240, current.compliance.h240, false),
    calcDelta('120Hz 合规', previous.compliance.h120, current.compliance.h120, false),
    calcDelta('60Hz 合规', previous.compliance.h60, current.compliance.h60, false),
  ];

  const verdictChanged = previous.verdict !== current.verdict;
  const operation = compareOperationSignatures(previous.operation, current.operation);
  const improvements = metrics.filter(m => m.direction === 'better').length;
  const regressions = metrics.filter(m => m.direction === 'worse').length;

  const lines: string[] = [];
  if (verdictChanged) {
    lines.push(`判定从 ${previous.verdict} 变为 ${current.verdict}。`);
  }
  if (improvements > 0) lines.push(`${improvements} 项指标改善。`);
  if (regressions > 0) lines.push(`${regressions} 项指标退化。`);

  // 最显著的变化
  const significant = [...metrics, ...compliance]
    .filter(m => m.direction !== 'neutral')
    .sort((a, b) => Math.abs(b.changePercent) - Math.abs(a.changePercent));

  if (significant.length > 0) {
    const top = significant[0];
    const trend = top.direction === 'better' ? '↓' : '↑';
    lines.push(`最显著变化：${top.name} ${top.before.toFixed(1)} → ${top.after.toFixed(1)}ms (${trend}${Math.abs(top.changePercent).toFixed(0)}%)。`);
  }

  const narrative = lines.join(' ');

  return { previous, current, metrics, compliance, verdictChanged, operation, narrative };
}
