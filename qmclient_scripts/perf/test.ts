import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseLine, parseLog, parseLogWithDiagnostics } from './lib/parse.ts';
import {
  compareOperationSignatures,
  operationSignature,
} from './lib/quality_core.ts';
import {
  reportQuality,
  summarizeForBundle,
} from './lib/quality.ts';
import { generateReport } from './lib/report.ts';
import {
  compareSessions,
  computeVerdict,
  fpsSummaries,
  isSamplingBiased,
  isFrameTimeEntry,
  isListFrameEvent,
  isPageSwitchEvent,
  isUiRebuildEvent,
  isWorkDrainEvent,
  pagePerformanceAttribution,
  settingsTextAnalysis,
  snapshot,
} from './lib/stats.ts';

const FIXTURE_DIR = join(dirname(fileURLToPath(import.meta.url)), 'test');

function readFixture(name: string): string {
  return readFileSync(join(FIXTURE_DIR, name), 'utf-8');
}

function testParseKeepsEventOnlyPerfLines() {
  const line = '2026-06-04 12:00:00 I perf/interaction: event=scroll_begin frame=42 page=settings:tee visible_rows=8';
  const entry = parseLine(line);
  assert.ok(entry);
  assert.equal(entry.system, 'perf/interaction');
  assert.equal(entry.fields.event, 'scroll_begin');

  const entries = parseLog([
    line,
    '2026-06-04 12:00:01 I perf/skin-ux: event=first_visible_ready dur_ms=123.500 frame=45 page=settings:tee',
  ].join('\n'));
  assert.equal(entries.length, 2);
}

function testParseSupportsJsonLinesEvents() {
  const entry = parseLine('{"timestamp":"2026-06-04T12:00:02","system":"perf/device","event":"sample","frame":77,"gpu_util_percent":61.5}');
  assert.ok(entry);
  assert.equal(entry?.system, 'perf/device');
  assert.equal(entry?.fields.event, 'sample');
  assert.equal(entry?.fields.frame, 77);

  const prefixed = parseLine('2026-06-04 12:00:03 I perf/settings-invalidate: {"system":"perf/settings-invalidate","frame":88,"session":9,"reason":"config_hash_changed","text":1}');
  assert.ok(prefixed);
  assert.equal(prefixed?.system, 'perf/settings-invalidate');
  assert.equal(prefixed?.fields.reason, 'config_hash_changed');
  assert.equal(prefixed?.fields.frame, 88);
}

function testParseLogWithDiagnosticsCountsInvalidLines() {
  const result = parseLogWithDiagnostics([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    'not a perf line',
    '',
    '{"timestamp":"2026-06-04T12:00:02","system":"perf/device","event":"sample","frame":77}',
    '{broken json',
  ].join('\n'));

  assert.equal(result.entries.length, 2);
  assert.equal(result.diagnostics.totalLines, 4);
  assert.equal(result.diagnostics.invalidLines, 2);
}

function testReportIncludesInteractionAndDeviceSections() {
  const entries = parseLog(readFixture('sample.log'));
  const html = generateReport(entries, 'sample.log', null);
  assert.match(html, /交互窗口/);
  assert.match(html, /Tee 收敛/);
  assert.match(html, /设备资源/);
  assert.match(html, /页面性能归因/);
  assert.match(html, /Section Top-10/);
}

function testReportShowsGenerationDuration() {
  const entries = parseLog(readFixture('sample.log'));
  const html = generateReport(entries, 'sample.log', null, undefined, 123.456);

  assert.match(html, /Report Generation/);
  assert.match(html, /123\.5ms/);
}

function testReportAttributesPagePerformanceEvents() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=12.500 frame=10',
    '2026-06-04 12:00:01 I perf/interaction: event=list_frame page=server_browser items_total=1200 rows_visible=18 rows_processed=18 rows_skipped=1182 dur_ms=5.250 frame=11',
    '2026-06-04 12:00:02 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 visible=1 dirty=config text_new=2 text_reused=8 frame=12',
    '2026-06-04 12:00:03 I perf/settings-resource: event=work_drain page=settings:tee kind=upload count=4 bytes=8192 dur_ms=9.000 stop=budget frame=13',
    '2026-06-04 12:00:04 I perf/skin-ux: event=list_drain_summary page=settings:tee dur_ms=18.000 requested=5 pending=2 loading=1 loaded=10 frame=14',
  ].join('\n'));
  const html = generateReport(entries, 'qm_perf_attribution.log', null);
  assert.match(html, /页面性能归因/);
  assert.doesNotMatch(html, /Page Switch/);
  assert.match(html, /page_switch/);
  assert.match(html, /List Interaction/);
  assert.match(html, /UI Rebuild/);
  assert.match(html, /Work Drain/);
  assert.match(html, /server_browser/);
  assert.match(html, /rows_processed/);
  assert.match(html, /stop=budget/);
  assert.match(html, /kind=merge/);
  assert.match(html, /requested=5 pending=2 loading=1 loaded=10/);
}

function testServerBrowserListFrameAttributionUsesRowCounts() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=list_frame page=server_browser items_total=1200 rows_visible=18 rows_rendered=18 rows_iterated=1200 rows_skipped=1182 dur_ms=5.250 frame=11 source=server_browser',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'List Interaction');
  assert.match(attribution[0].summary, /rows_visible=18/);
  assert.match(attribution[0].details, /rows_rendered=18/);
  assert.match(attribution[0].details, /rows_iterated=1200/);
  assert.match(attribution[0].details, /rows_skipped=1182/);
}

function testQmUiRuntimeAttributionUsesDedicatedKind() {
  const entries = parseLog([
    '2026-06-10 12:00:00 I perf/ui_runtime: {"system":"perf/ui_runtime","frame":"1","session":"7","page":"settings:qmclient","event":"ui_runtime","operation":"settings_qmclient","nodes":"120","anim_ms":"0.200","active_anims":"3","queued_anims":"1","render_bridge_ms":"0.050","duration_ms":"0.400"}',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'UI Runtime');
  assert.equal(attribution[0].page, 'settings:qmclient');
  assert.match(attribution[0].summary, /operation=settings_qmclient/);
}

function testFpsSummaryTelemetryFeedsReportAndBundleSummary() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":60.000,"fps_min":45.000,"fps_max":120.000,"frame_ms_avg":16.667,"frame_ms_p95":22.000,"frame_ms_p99":25.000,"frame_ms_max":25.000,"cap_limited":0}',
    '2026-06-11 02:00:00 I perf/menu: {"system":"perf/menu","frame":10,"stage":"settings_page_content","duration_ms":7.000,"page":"tee"}',
  ].join('\n'));

  const fps = fpsSummaries(entries);
  assert.equal(fps.length, 1);
  assert.equal(fps[0].operation, 'settings_open');
  assert.equal(fps[0].context, 'online');
  assert.equal(fps[0].fpsMin, 45);
  assert.equal(fps[0].frameMsP99, 25);

  const summary = summarizeForBundle(entries, 'qm_perf_fps.log', { invalidLines: 0, totalLines: 2 });
  assert.equal(summary.fps.available, true);
  assert.equal(summary.fps.summaries[0].page, 'settings:tee');
  assert.equal(summary.quality.warnings.some(w => /ingame\/online/i.test(w)), false);

  const html = generateReport(entries, 'qm_perf_fps.log', null);
  assert.match(html, /FPS 摘要/);
  assert.match(html, /FPS Avg/);
  assert.match(html, /FPS Min/);
  assert.match(html, /FPS Max/);
  assert.match(html, /Frame Avg/);
  assert.match(html, /Frame Max/);
  assert.match(html, /settings_open/);
  assert.match(html, /60\.0/);
  assert.match(html, /45\.0/);
  assert.match(html, /120\.0/);
}

function testMissingFpsSummaryWarnsThatSettingsAcceptanceIsIncomplete() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/menu: {"system":"perf/menu","frame":10,"stage":"settings_page_content","duration_ms":7.000,"page":"tee"}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_missing_fps.log', { invalidLines: 0, totalLines: 1 });

  assert.equal(summary.fps.available, false);
  assert.match(summary.quality.warnings.join('\n'), /missing fps_summary/i);
  assert.match(summary.quality.warnings.join('\n'), /ingame\/online/i);

  const html = generateReport(entries, 'qm_perf_missing_fps.log', null);
  assert.match(html, /缺少 fps_summary/);
  assert.match(html, /不足以验收/);
}

function testTargetSettingsVerdictIgnoresServerBrowserFrames() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/menu: {"system":"perf/menu","frame":1,"stage":"offline_page_content","duration_ms":200.000,"page":"internet"}',
    '2026-06-11 02:00:01 I perf/menu: {"system":"perf/menu","frame":2,"stage":"settings_page_content","duration_ms":7.000,"page":"tee"}',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_tab_switch","context":"offline","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.250,"fps_avg":120.000,"fps_min":100.000,"fps_max":180.000,"frame_ms_avg":8.000,"frame_ms_p95":9.000,"frame_ms_p99":10.000,"frame_ms_max":10.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_mixed.log', { invalidLines: 0, totalLines: 3 });

  assert.equal(summary.targetSettings.verdict, 'PASS');
  assert.equal(summary.targetSettings.spikeCount, 0);
  assert.equal(summary.verdict, 'FAIL');
}

function testTargetSettingsVerdictUsesIngameEscFpsSummaryWindow() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"ingame_esc_open","context":"online","page":"game","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":60.000,"fps_min":30.000,"fps_max":120.000,"frame_ms_avg":16.667,"frame_ms_p95":20.000,"frame_ms_p99":45.000,"frame_ms_max":45.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_ingame_esc.log', { invalidLines: 0, totalLines: 1 });

  assert.equal(summary.targetSettings.verdict, 'FAIL');
  assert.equal(summary.targetSettings.verdictAvailable, true);
  assert.equal(summary.targetSettings.spikeCount, 1);
  assert.doesNotMatch(summary.quality.warnings.join('\n'), /missing ingame\/online/i);
}

function testStableTextCoverageBlocksSettingsAcceptanceEvenWithBackgroundHotspots() {
  const entries = parseLog([
    '2026-06-11 01:59:58 I perf/settings-text: event=settings_text_prebuild built=120 reused=24 remaining=3 budget=160 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 01:59:59 I perf/settings: event=settings_text_miss scope=target_settings page=settings:tee tab=appearance subtab=assets key=qm.ui.preview reason=cache_miss',
    '2026-06-11 02:00:00 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
    '2026-06-11 02:00:03 I perf/interaction: event=list_frame page=server_browser items_total=3200 rows_visible=24 rows_rendered=24 rows_iterated=3200 rows_skipped=3176 dur_ms=48.500 frame=22 source=server_browser',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_stable_text_blocker.log', { invalidLines: 0, totalLines: 4 });

  assert.equal(summary.targetSettings.verdict, 'PASS');
  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.missCount, 1);
  assert.equal(summary.targetSettings.stableTextCoverage.staleCount, 0);
  assert.equal(summary.targetSettings.stableTextCoverage.prebuildRemainingBeforeTarget, 3);
  assert.match(summary.targetSettings.stableTextCoverage.samples.join('\n'), /settings_text_miss/);

  const html = generateReport(entries, 'qm_perf_stable_text_blocker.log', null);
  assert.match(html, /不足以验收/);
  assert.match(html, /stable text/i);
  assert.match(html, /hit rate=0\.0%/);
  assert.match(html, /reuse rate=0\.0%/);
  assert.match(html, /text_new=0/);
  assert.match(html, /text_reused=0/);
  assert.doesNotMatch(html, /server browser.*PASS/i);
}

function testStableTextCoverageIgnoresOrdinarySettingsPagesOutsideTargetWindow() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_plan_collection units_done=10 units_total=10 remaining=0 budget=1 complete=1 dirty=0 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tee tab=appearance subtab=assets operation=settings_open frame=40 candidates=12 hits=12 reused=12 miss=0 stale=0 text_new=0 text_reused=12 planned=12 unplanned=0',
    '2026-06-11 02:00:01 I perf/settings-text: event=settings_text_prebuild built=40 reused=10 remaining=7 budget=80 phase=before_target scope=settings operation=settings_open',
    '2026-06-11 02:00:01 I perf/settings-text: event=settings_text_miss scope=settings page=settings:qmclient tab=0 subtab=-1 key=settings:12:0:-1:qmclient-theme-title:fs140:al8:mw2600:us100:cm1:ch1 reason=missing operation=settings_open frame=42',
    '2026-06-11 02:00:02 I perf/settings-text: event=settings_text_stale scope=settings page=settings:tclient tab=0 subtab=-1 key=settings:11:0:-1:tclient-theme-title:fs140:al8:mw2600:us100:cm1:ch1 reason=style operation=settings_tab_switch frame=43',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_ordinary_settings_text.log', { invalidLines: 0, totalLines: 4 });

  assert.equal(summary.targetSettings.verdict, 'PASS');
  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, false);
  assert.equal(summary.targetSettings.stableTextCoverage.missCount, 0);
  assert.equal(summary.targetSettings.stableTextCoverage.staleCount, 0);
  assert.equal(summary.targetSettings.stableTextCoverage.prebuildRemainingBeforeTarget, 0);
  assert.equal(summary.targetSettings.stableTextCoverage.utilizationAvailable, true);
  assert.equal(summary.targetSettings.stableTextCoverage.candidateTotal, 12);
  assert.equal(summary.targetSettings.stableTextCoverage.hitCount, 12);
  assert.equal(summary.targetSettings.stableTextCoverage.reuseCount, 12);
  assert.equal(summary.targetSettings.stableTextCoverage.hitRate, 100);
  assert.equal(summary.targetSettings.stableTextCoverage.reuseRate, 100);
  assert.equal(summary.targetSettings.stableTextCoverage.samples.join('\n'), '');
}

function testStableTextCoverageIncludesLaterTargetWindows() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=32 reused=8 remaining=0 budget=64 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tee tab=appearance subtab=assets operation=settings_open frame=40 candidates=8 hits=8 reused=8 miss=0 stale=0 text_new=0 text_reused=8 planned=8 unplanned=0',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
    '2026-06-11 02:00:03 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:qmclient tab=0 subtab=0 operation=settings_subtab_switch frame=77 candidates=4 hits=3 reused=3 miss=1 stale=0 text_new=0 text_reused=3 planned=3 unplanned=1',
    '2026-06-11 02:00:03 I perf/settings: event=settings_text_miss scope=target_settings page=settings:qmclient tab=0 subtab=0 key=late_key reason=missing operation=settings_subtab_switch frame=77',
    '2026-06-11 02:00:04 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_subtab_switch","context":"online","page":"settings:qmclient","tab":"0","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_later_target_window_text.log', { invalidLines: 0, totalLines: 6 });

  assert.equal(summary.targetSettings.verdict, 'PASS');
  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.missCount, 1);
  assert.equal(summary.targetSettings.stableTextCoverage.candidateTotal, 12);
  assert.match(summary.targetSettings.stableTextCoverage.samples.join('\n'), /late_key/);
}

function testStableTextCoverageSeparatesPlanCoverageFromUtilization() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=32 reused=8 remaining=0 budget=64 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tclient tab=4 subtab=4 operation=settings_open frame=40 candidates=10 hits=7 reused=7 miss=3 stale=0 text_new=0 text_reused=7 planned=7 unplanned=3',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_miss scope=target_settings page=settings:tclient tab=4 subtab=4 key=missing_a reason=missing plan_status=missing_descriptor operation=settings_open frame=40',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_miss scope=target_settings page=settings:tclient tab=4 subtab=4 key=missing_b reason=missing plan_status=missing_descriptor operation=settings_open frame=40',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_stale scope=target_settings page=settings:tclient tab=4 subtab=4 key=stale_c reason=style plan_status=key_mismatch operation=settings_open frame=40',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tclient","tab":"4","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_plan_coverage.log', { invalidLines: 0, totalLines: 6 });

  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.planCoverageAvailable, true);
  assert.equal(summary.targetSettings.stableTextCoverage.planCandidateCount, 7);
  assert.equal(summary.targetSettings.stableTextCoverage.visibleCandidateCount, 10);
  assert.equal(summary.targetSettings.stableTextCoverage.unplannedVisibleCount, 3);
  assert.equal(summary.targetSettings.stableTextCoverage.keyMismatchCount, 1);
  assert.match(summary.targetSettings.stableTextCoverage.consistencyWarnings.join('\n'), /unplanned visible stable text/);
  assert.match(summary.targetSettings.stableTextCoverage.samples.join('\n'), /plan_status=missing_descriptor/);

  const html = generateReport(entries, 'qm_perf_plan_coverage.log', null);
  assert.match(html, /Plan Coverage/);
  assert.match(html, /Unplanned/);
  assert.match(html, /Key Mismatch/);
  assert.match(html, /missing_descriptor/);
  assert.match(html, /key_mismatch/);
}

function testStableTextCoverageUsesPrebuildRemainingBeforeTargetOnly() {
  const entries = parseLog([
    '2026-06-11 01:59:59 I perf/settings-text: event=settings_text_prebuild built=32 reused=8 remaining=5 budget=64 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tee tab=appearance subtab=assets operation=settings_open frame=40 candidates=8 hits=8 reused=8 miss=0 stale=0 text_new=0 text_reused=8 planned=8 unplanned=0',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
    '2026-06-11 02:00:02 I perf/settings-text: event=settings_text_prebuild built=5 reused=40 remaining=0 budget=64 phase=before_target scope=target_settings operation=settings_open',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_prebuild_after_target.log', { invalidLines: 0, totalLines: 4 });

  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.prebuildRemainingBeforeTarget, 5);
}

function testStableTextCoverageBlocksWhenPlanCollectionIncompleteBeforeTarget() {
  const entries = parseLog([
    '2026-06-11 01:59:59 I perf/settings-text: event=settings_text_plan_collection units_done=2 units_total=10 remaining=8 budget=1 complete=0 dirty=0 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=32 reused=8 remaining=0 budget=64 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tee tab=appearance subtab=assets operation=settings_open frame=40 candidates=8 hits=8 reused=8 miss=0 stale=0 text_new=0 text_reused=8 planned=8 unplanned=0',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_plan_collection_incomplete.log', { invalidLines: 0, totalLines: 4 });

  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.planCollectionAvailable, true);
  assert.equal(summary.targetSettings.stableTextCoverage.planCollectionComplete, false);
  assert.equal(summary.targetSettings.stableTextCoverage.planCollectionRemainingBeforeTarget, 8);
  assert.equal(summary.targetSettings.stableTextCoverage.prebuildRemainingBeforeTarget, 0);
  assert.match(summary.targetSettings.stableTextCoverage.consistencyWarnings.join('\n'), /stable text plan collection incomplete/);

  const html = generateReport(entries, 'qm_perf_plan_collection_incomplete.log', null);
  assert.match(html, /Plan Collection/);
  assert.match(html, /Collection Remaining/);
  assert.match(html, /Container Remaining/);
  assert.match(html, /Visible Coverage/);
}

function testStableTextCoverageBlocksWhenTargetUsageIsMissing() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=32 reused=8 remaining=0 budget=64 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_open","context":"online","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":144.000,"fps_min":120.000,"fps_max":180.000,"frame_ms_avg":6.944,"frame_ms_p95":8.000,"frame_ms_p99":9.000,"frame_ms_max":9.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_missing_usage.log', { invalidLines: 0, totalLines: 2 });

  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, true);
  assert.equal(summary.targetSettings.stableTextCoverage.utilizationAvailable, false);
  assert.match(summary.targetSettings.stableTextCoverage.consistencyWarnings.join('\n'), /missing settings_text_usage/);
}

function testAssetsVisibleFirstAdmissionAppearsInSummaryAndReport() {
  const entries = parseLog([
    '2026-06-14 02:15:22 I perf/assets: {"system":"perf/assets","frame":40,"page":"assets","stage":"assets_preview_draw_workshop_cards","duration_ms":41.332,"tab":1,"combined":797,"local_total":23,"remote_total":774,"rendered":42,"thumb_starts":12,"visible_first":1,"visible_starts":12,"prefetch_starts":0,"background_starts":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_assets_visible_first.log', { invalidLines: 0, totalLines: 1 });

  assert.equal(summary.assetsPreviewAdmission.available, true);
  assert.equal(summary.assetsPreviewAdmission.visibleFirstAvailable, true);
  assert.equal(summary.assetsPreviewAdmission.maxDurationMs, 41.332);
  assert.equal(summary.assetsPreviewAdmission.maxRendered, 42);
  assert.equal(summary.assetsPreviewAdmission.maxThumbStarts, 12);
  assert.equal(summary.assetsPreviewAdmission.visibleStarts, 12);
  assert.equal(summary.assetsPreviewAdmission.prefetchStarts, 0);
  assert.equal(summary.assetsPreviewAdmission.backgroundStarts, 0);

  const html = generateReport(entries, 'qm_perf_assets_visible_first.log', null);
  assert.match(html, /Assets Visible-First Admission/);
  assert.match(html, /VISIBLE-FIRST/);
  assert.match(html, /Visible Starts/);
  assert.match(html, /Prefetch Starts/);
  assert.match(html, /Background Starts/);
}

function testAssetsVisibleReadyRequiresEveryPreflightReady() {
  const entries = parseLog([
    '2026-06-14 03:33:42 I perf/assets: {"system":"perf/assets","frame":40,"page":"assets","stage":"assets_visible_preflight","duration_ms":1.000,"tab":1,"visible_count":24,"half_visible_count":6,"ready_count":24,"not_ready_count":0,"visible_ready":1,"geometry_stable":1,"thumb_starts_before_visible":0,"thumb_starts_during_draw":0}',
    '2026-06-14 03:33:43 I perf/assets: {"system":"perf/assets","frame":41,"page":"assets","stage":"assets_visible_preflight","duration_ms":1.000,"tab":1,"visible_count":24,"half_visible_count":6,"ready_count":20,"not_ready_count":4,"visible_ready":0,"geometry_stable":1,"thumb_starts_before_visible":4,"thumb_starts_during_draw":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_assets_mixed_visible_ready.log', { invalidLines: 0, totalLines: 2 });

  assert.equal(summary.assetsVisibleReady.available, true);
  assert.equal(summary.assetsVisibleReady.visibleReadyAvailable, false);
  assert.equal(summary.assetsVisibleReady.notReadyCount, 4);
  assert.match(summary.quality.warnings.join('\n'), /not_ready_count > 0/);
}

function testAssetsGeometryIgnoresEmptyOrMissingFieldSamples() {
  const entries = parseLog([
    '2026-06-14 03:33:42 I perf/assets: {"system":"perf/assets","frame":40,"page":"assets","stage":"assets_visible_preflight","duration_ms":1.000,"tab":1,"visible_count":24,"half_visible_count":6,"ready_count":24,"not_ready_count":0,"visible_ready":1,"geometry_stable":1,"thumb_starts_before_visible":0,"thumb_starts_during_draw":0}',
    '2026-06-14 03:33:43 I perf/assets: {"system":"perf/assets","frame":41,"page":"assets","stage":"assets_preview_draw_workshop_cards","duration_ms":1.000,"tab":1,"combined":0,"rendered":0,"thumb_starts":0,"visible_first":1,"visible_starts":0,"prefetch_starts":0,"background_starts":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_assets_empty_draw.log', { invalidLines: 0, totalLines: 2 });

  assert.equal(summary.assetsVisibleReady.available, true);
  assert.equal(summary.assetsVisibleReady.geometryStable, true);
  assert.doesNotMatch(summary.quality.warnings.join('\n'), /geometry unstable/);
}

function testDemoBrowserPhasesAppearInSummaryAndReport() {
  const entries = parseLog([
    '2026-06-14 04:00:00 I perf/interaction: event=demo_browser_startup page=demo_browser items_total=120 items_scanned=120 items_done=120 remaining=0 metadata_remaining=240 budget=0 dur_ms=12.000 trigger=populate source=demos sort=3 fetch_info=1',
    '2026-06-14 04:00:01 I perf/interaction: event=demo_browser_header_fetch page=demo_browser items_total=120 items_scanned=2 items_done=2 visible_first=20 visible_end=40 visible_scanned=2 visible_done=2 background_scanned=0 background_done=0 remaining=118 budget=2 dur_ms=4.500 trigger=list_frame source=demos sort=3 fetch_info=1',
    '2026-06-14 04:00:02 I perf/interaction: event=demo_browser_date_fetch page=demo_browser items_total=120 items_scanned=4 items_done=4 visible_first=20 visible_end=40 visible_scanned=4 visible_done=4 background_scanned=0 background_done=0 remaining=116 budget=4 dur_ms=3.200 trigger=list_frame source=demos sort=3 fetch_info=1',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_demo_browser.log', { invalidLines: 0, totalLines: 3 });
  assert.equal(summary.demoBrowser.available, true);
  assert.equal(summary.demoBrowser.startupCount, 1);
  assert.equal(summary.demoBrowser.headerFetchCount, 1);
  assert.equal(summary.demoBrowser.dateFetchCount, 1);
  assert.equal(summary.demoBrowser.visibleScanned, 6);
  assert.equal(summary.demoBrowser.visibleDone, 6);
  assert.equal(summary.demoBrowser.backgroundScanned, 0);
  assert.equal(summary.demoBrowser.maxRemaining, 118);
  assert.equal(summary.demoBrowser.maxMetadataRemaining, 240);
  const html = generateReport(entries, 'qm_perf_demo_browser.log');
  assert.match(html, /Demo Browser/);
  assert.match(html, /demo_browser_header_fetch/);
  assert.match(html, /visible_scanned=2/);
  assert.match(html, /visible scanned/);
  assert.match(html, /metadata_remaining=240/);
  assert.match(html, /remaining=118/);
}

function testSettingsTextAnalysisAggregatesMissesByLocationReasonAndOperation() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=10 reused=4 remaining=2 budget=16 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:01 I perf/settings: event=settings_text_miss scope=target_settings page=settings:tee tab=appearance subtab=assets key=alpha reason=missing operation=settings_open frame=42',
    '2026-06-11 02:00:02 I perf/settings: event=settings_text_stale scope=target_settings page=settings:tee tab=appearance subtab=assets key=beta reason=style operation=settings_open frame=43',
    '2026-06-11 02:00:03 I perf/settings: event=settings_text_miss scope=target_settings page=settings:tee tab=appearance subtab=assets key=gamma reason=missing operation=settings_tab_switch frame=44',
    '2026-06-11 02:00:04 I perf/settings: event=settings_text_miss scope=settings page=settings:qmclient tab=0 subtab=-1 key=delta reason=cache_miss operation=settings_open frame=45',
  ].join('\n'));

  const analysis = settingsTextAnalysis(entries);

  assert.equal(analysis.summary.missCount, 3);
  assert.equal(analysis.summary.staleCount, 1);
  assert.equal(analysis.summary.prebuildCount, 1);
  assert.equal(analysis.summary.remainingPositiveCount, 1);
  assert.equal(analysis.topByLocation[0].location, 'settings:tee / appearance / assets');
  assert.equal(analysis.topByLocation[0].missCount, 2);
  assert.equal(analysis.topByLocation[0].staleCount, 1);
  assert.equal(analysis.topByReason[0].reason, 'missing');
  assert.equal(analysis.topByReason[0].missCount, 2);
  assert.equal(analysis.topByOperation[0].operation, 'settings_open');
  assert.equal(analysis.topByOperation[0].missCount, 2);
  assert.equal(analysis.prebuildSeries[0].built, 10);
  assert.equal(analysis.eventTimeline[0].event, 'settings_text_prebuild');
  assert.equal(analysis.eventTimeline.at(-1)?.event, 'settings_text_miss');
}

function testReportIncludesTextPoolMissAnalysisCharts() {
  const longKey = 'settings:1:-1:-1:settings-shell-title:fs160:al10:mw1475:us100:cm1:ch369197882:very-long-stable-text-key-that-must-not-wrap-vertically';
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_prebuild built=10 reused=4 remaining=2 budget=16 phase=before_target scope=target_settings operation=settings_open',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_usage scope=target_settings page=settings:tee tab=appearance subtab=assets operation=settings_open frame=40 candidates=8 hits=6 reused=5 miss=2 stale=0 text_new=1 text_reused=5 planned=6 unplanned=2',
    `2026-06-11 02:00:01 I perf/settings: event=settings_text_miss scope=target_settings page=settings:tee tab=appearance subtab=assets key=${longKey} reason=missing operation=settings_open frame=42`,
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_text_analysis.log', null);

  assert.match(html, /文本池 Miss 分析/);
  assert.match(html, /chart-text-location-top/);
  assert.match(html, /chart-text-reason-top/);
  assert.match(html, /chart-text-operation-top/);
  assert.match(html, /chart-text-prebuild/);
  assert.match(html, /chart-text-timeline/);
  assert.match(html, /settings_text_prebuild/);
  assert.match(html, /settings_text_miss/);
  assert.match(html, /<h2>文本池 Miss 分析<\/h2>[\s\S]*Stable Text Coverage[\s\S]*Hit Rate[\s\S]*75\.0%[\s\S]*Reuse Rate[\s\S]*62\.5%/);
  assert.match(html, /<h2>文本池 Miss 分析<\/h2>[\s\S]*Text New[\s\S]*1[\s\S]*Text Reused[\s\S]*5/);
  assert.match(html, /table-scroll/);
  assert.match(html, /sample-key-cell/);
  assert.match(html, /white-space:nowrap/);
  assert.match(html, new RegExp(`title="${longKey}`));
  assert.doesNotMatch(html, new RegExp(`>\\s*${longKey}\\s*<`));
}

function testAdaptiveBudgetEventsAppearInSummaryAndReport() {
  const entries = parseLog([
    '2026-06-14 05:00:00 I perf/settings-resource: event=settings_adaptive_budget mode=idle reason=progress frame_ms_avg=5.000 frame_ms_p95=6.000 target_ms=8.333 visible_tokens=8 prefetch_tokens=4 background_tokens=3 gpu_upload_tokens=2 text_tokens=6 demo_tokens=4 backlog=120 scroll=0 jump_scroll=0',
    '2026-06-14 05:00:01 I perf/settings-resource: event=settings_adaptive_budget mode=frame_pressure reason=frame_pressure frame_ms_avg=14.000 frame_ms_p95=20.000 target_ms=8.333 visible_tokens=2 prefetch_tokens=0 background_tokens=0 gpu_upload_tokens=1 text_tokens=1 demo_tokens=1 backlog=120 scroll=0 jump_scroll=0',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_adaptive_budget.log', { invalidLines: 0, totalLines: 2 });
  assert.equal(summary.adaptiveBudget.available, true);
  assert.equal(summary.adaptiveBudget.eventCount, 2);
  assert.equal(summary.adaptiveBudget.framePressureCount, 1);
  assert.equal(summary.adaptiveBudget.maxBackgroundTokens, 3);
  assert.equal(summary.adaptiveBudget.maxVisibleTokens, 8);

  const html = generateReport(entries, 'qm_perf_adaptive_budget.log');
  assert.match(html, /Adaptive Budget/);
  assert.match(html, /frame_pressure/);
  assert.match(html, /Visible Tokens/);
}

function testAdaptiveBudgetMissingWarnsWhenResourceWindowsExist() {
  const entries = parseLog([
    '2026-06-14 05:00:00 I perf/assets: {"system":"perf/assets","stage":"assets_preview_draw_workshop_cards","duration_ms":4.000,"tab":1,"combined":10,"rendered":10,"thumb_starts":1,"visible_first":1,"visible_starts":1,"prefetch_starts":0,"background_starts":0}',
    '2026-06-14 05:00:01 I perf/interaction: event=demo_browser_header_fetch page=demo_browser items_total=10 items_scanned=1 items_done=1 remaining=9 budget=1 dur_ms=1.000 trigger=list_frame source=demos sort=3 fetch_info=1',
    '2026-06-14 05:00:02 I perf/settings-text: event=settings_text_prebuild built=1 reused=0 remaining=1 budget=1 phase=before_target scope=target_settings operation=settings_open',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_missing_adaptive_budget.log', { invalidLines: 0, totalLines: 3 });
  assert.equal(summary.adaptiveBudget.available, false);
  assert.match(summary.quality.warnings.join('\n'), /adaptive budget telemetry missing/);
}

function testReportPreservesFailVerdictWhenStableTextIsNotBlocking() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"ingame_esc_open","context":"online","page":"game","tab":"none","sample_frames":30,"sample_seconds":0.500,"fps_avg":60.000,"fps_min":30.000,"fps_max":120.000,"frame_ms_avg":16.667,"frame_ms_p95":20.000,"frame_ms_p99":45.000,"frame_ms_max":45.000,"cap_limited":0}',
    '2026-06-11 02:00:00 I perf/settings-text: event=settings_text_plan_collection units_done=1 units_total=1 remaining=0 budget=1 complete=1 dirty=0 phase=before_target scope=target_settings operation=ingame_esc_open',
    '2026-06-11 02:00:01 I perf/settings-text: event=settings_text_prebuild built=80 reused=20 remaining=0 budget=100 phase=before_target scope=target_settings operation=ingame_esc_open',
    '2026-06-11 01:59:59 I perf/settings-text: event=settings_text_usage scope=target_settings page=game tab=none subtab=-1 operation=ingame_esc_open frame=41 candidates=20 hits=20 reused=20 miss=0 stale=0 text_new=0 text_reused=20 planned=20 unplanned=0',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_fail_verdict.log', { invalidLines: 0, totalLines: 2 });
  assert.equal(summary.targetSettings.verdict, 'FAIL');
  assert.equal(summary.targetSettings.verdictAvailable, true);
  assert.equal(summary.targetSettings.stableTextCoverage.acceptanceBlocked, false);

  const html = generateReport(entries, 'qm_perf_fail_verdict.log', null);
  assert.match(html, /当前目标判定：<span class="badge bad">FAIL<\/span>/);
  assert.doesNotMatch(html, /当前目标判定：<span class="badge bad">不足以验收<\/span>/);
}

function testGenericIngamePageContentDoesNotSatisfyOnlineCoverageWarning() {
  const entries = parseLog([
    '2026-06-11 02:00:00 I perf/menu: {"system":"perf/menu","frame":1,"stage":"ingame_page_content","duration_ms":7.000,"page":"game"}',
    '2026-06-11 02:00:01 I perf/fps: {"system":"perf/fps","event":"fps_summary","operation":"settings_tab_switch","context":"offline","page":"settings:tee","tab":"none","sample_frames":30,"sample_seconds":0.250,"fps_avg":120.000,"fps_min":100.000,"fps_max":180.000,"frame_ms_avg":8.000,"frame_ms_p95":9.000,"frame_ms_p99":10.000,"frame_ms_max":10.000,"cap_limited":0}',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_generic_ingame.log', { invalidLines: 0, totalLines: 2 });

  assert.match(summary.quality.warnings.join('\n'), /ingame\/online/i);
}

function testPerfEventClassifiersKeepBoundariesTight() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=12.500 frame=11',
    '2026-06-04 12:00:02 I perf/interaction: event=list_frame page=server_browser dur_ms=5.250 frame=12',
    '2026-06-04 12:00:03 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 frame=13',
    '2026-06-04 12:00:04 I perf/interaction: event=section page=settings:tee section=not-a-section dur_ms=99.000 frame=14',
    '2026-06-04 12:00:05 I perf/settings-resource: event=work_drain page=settings:tee dur_ms=9.000 frame=15',
    '2026-06-04 12:00:06 I perf/device: event=sample frame=16 gpu_util_percent=61.5',
  ].join('\n'));

  assert.equal(isFrameTimeEntry(entries[0]), true);
  assert.equal(isPageSwitchEvent(entries[1]), true);
  assert.equal(isListFrameEvent(entries[2]), true);
  assert.equal(isUiRebuildEvent(entries[3]), true);
  assert.equal(isUiRebuildEvent(entries[4]), false);
  assert.equal(isWorkDrainEvent(entries[5]), true);
  assert.equal(isFrameTimeEntry(entries[6]), false);
}

function testPageSwitchBoundaryDoesNotEnterDurationAttribution() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=0.000 frame=10',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 visible=1 dirty=config text_new=2 text_reused=8 frame=11',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'UI Rebuild');
  assert.equal(attribution[0].durationMs, 4.75);
}

function testSnapshotIgnoresEventOnlyTelemetry() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8 first_visible_skin=default',
    '2026-06-04 12:00:02 I perf/device: event=sample frame=12 gpu_util_percent=61.5 cpu_process_percent=12 memory_process_mb=1024',
    '2026-06-04 12:00:03 I perf/menu: stage=settings_page_content duration_ms=10.000 frame=13 page=settings:tee',
  ].join('\n'));

  const current = snapshot(entries, 'qm_perf_current.log');

  assert.equal(current.totalFrames, 2);
  assert.equal(current.percentiles.min, 6);
  assert.equal(current.percentiles.p50, 6);
  assert.equal(current.percentiles.p95, 10);
  assert.equal(current.compliance.h240, 0);
}

function testComparisonReportWarnsAboutAutomaticBaseline() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=8.000 frame=10 page=settings:tee',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const comparison = compareSessions(
    snapshot(previousEntries, 'qm_perf_previous.log'),
    snapshot(currentEntries, 'qm_perf_current.log'),
  );

  const html = generateReport(currentEntries, 'qm_perf_current.log', comparison);

  assert.match(html, /自动选择的上一份日志/);
  assert.match(html, /不作为严格回归判定/);
}

function testComparisonReportMarksDifferentOperationsAsAdvisory() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=demo_browser duration_ms=8.000 frame=10 page=demo_browser',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const comparison = compareSessions(
    snapshot(previousEntries, 'qm_perf_previous.log'),
    snapshot(currentEntries, 'qm_perf_current.log'),
  );

  const html = generateReport(currentEntries, 'qm_perf_current.log', comparison);

  assert.equal(comparison.operation.comparable, false);
  assert.match(html, /advisory only/i);
  assert.match(html, /page set differs/i);
}

function testOperationCompatibilityChecksEventsAndStages() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=page_switch page=settings:tee dur_ms=4.000 frame=11',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_render_total duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=list_frame page=settings:tee dur_ms=4.000 frame=11',
  ].join('\n'));

  const result = compareOperationSignatures(operationSignature(previousEntries), operationSignature(currentEntries));

  assert.equal(result.comparable, false);
  assert.match(result.reason, /event set differs|stage set differs/i);
}

function testReportQualityExplainsMissingAndBiasedData() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=10.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/menu: stage=settings_page_content duration_ms=11.000 frame=11 page=settings:tee',
    '2026-06-04 12:00:02 I perf/menu: stage=settings_page_content duration_ms=12.000 frame=12 page=settings:tee',
    '2026-06-04 12:00:03 I perf/menu: stage=settings_page_content duration_ms=13.000 frame=13 page=settings:tee',
    '2026-06-04 12:00:04 I perf/menu: stage=settings_page_content duration_ms=14.000 frame=14 page=settings:tee',
    '2026-06-04 12:00:05 I perf/menu: stage=settings_page_content duration_ms=15.000 frame=15 page=settings:tee',
    '2026-06-04 12:00:06 I perf/menu: stage=settings_page_content duration_ms=16.000 frame=16 page=settings:tee',
    '2026-06-04 12:00:07 I perf/menu: stage=settings_page_content duration_ms=17.000 frame=17 page=settings:tee',
    '2026-06-04 12:00:08 I perf/menu: stage=settings_page_content duration_ms=18.000 frame=18 page=settings:tee',
    '2026-06-04 12:00:09 I perf/menu: stage=settings_page_content duration_ms=19.000 frame=19 page=settings:tee',
  ].join('\n'));

  const quality = reportQuality(entries, { invalidLines: 2, totalLines: 12 });

  assert.equal(quality.sampleCount, 10);
  assert.equal(quality.invalidLines, 2);
  assert.equal(quality.biased, true);
  assert.match(quality.warnings.join('\n'), /sampling threshold/i);
  assert.match(quality.warnings.join('\n'), /invalid lines/i);
  assert.deepEqual(quality.operation.pages, ['settings:tee']);
}

function testOperationCompatibilityIsExplicit() {
  const settingsEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const demoEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=demo_browser duration_ms=9.000 frame=10 page=demo_browser',
  ].join('\n'));

  const result = compareOperationSignatures(operationSignature(settingsEntries), operationSignature(demoEntries));

  assert.equal(result.comparable, false);
  assert.match(result.reason, /page set differs/i);
}

function testBundleSummaryIsStableJsonShape() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=4.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=11',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_current.log', { invalidLines: 0, totalLines: 2 });

  assert.equal(summary.sourceFile, 'qm_perf_current.log');
  assert.equal(summary.quality.sampleCount, 1);
  assert.deepEqual(summary.quality.operation.pages, ['settings:tee']);
  assert.equal(summary.attribution.top.length, 1);
  assert.equal(summary.attribution.top[0].kind, 'UI Rebuild');
  assert.equal(typeof summary.generatedAt, 'string');
}

function testSamplingBiasReportUsesP5Estimate() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=1.000 frame=10 page=settings:tee',
    ...Array.from({ length: 19 }, (_, i) =>
      `2026-06-04 12:00:${String(i + 1).padStart(2, '0')} I perf/menu: stage=settings_page_content duration_ms=10.000 frame=${i + 11} page=settings:tee`),
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_sampling_bias.log', null);

  assert.match(html, /p5=10\.0ms/);
  assert.doesNotMatch(html, /当前采样阈值为 1\.0ms/);
  assert.doesNotMatch(html, /当前采样阈值 1\.0ms/);
}

function testSamplingBiasUsesDefaultLoggingThreshold() {
  assert.equal(isSamplingBiased(Array(20).fill(4.1)), true);
}

function testKpiThresholdsAlignWithVerdictBudget() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=16.500 frame=10 page=settings:tee',
  ].join('\n'));
  const html = generateReport(entries, 'qm_perf_thresholds.log', null);

  assert.equal(computeVerdict(snapshot(entries, 'qm_perf_thresholds.log').percentiles, 0), 'PASS');
  assert.match(html, /<div class="kpi-label">p99<\/div><div class="kpi-value ok">16\.5<\/div>/);
  assert.match(html, /<div class="kpi-label">Max<\/div><div class="kpi-value ok">16\.5<\/div>/);
}

function testReportIncludesSectionTop10() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/section: event=section page=settings:tee section=identity dur_ms=4.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=10',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=12.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=11',
    '2026-06-04 12:00:02 I perf/section: event=section page=settings:qmclient section=theme dur_ms=7.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=12',
    '2026-06-04 12:00:03 I perf/interaction: event=section page=settings:tee section=not-a-section dur_ms=99.000 frame=13',
    '2026-06-04 12:00:04 I perf/menu: stage=settings_page_content duration_ms=8.000 frame=14 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_sections.log', null);

  assert.match(html, /Section Top-10/);
  assert.match(html, /settings:tee/);
  assert.match(html, /identity/);
  assert.match(html, /12\.000ms/);
  assert.doesNotMatch(html, /not-a-section/);
}

function testReportShowsQualityAndUnavailableData() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_empty_frame_data.log', null, {
    totalLines: 2,
    invalidLines: 1,
  });

  assert.match(html, /样本可信度/);
  assert.match(html, /N\/A/);
  assert.match(html, /WARN/);
  assert.doesNotMatch(html, /<span class="verdict-banner ok">PASS<\/span>/);
  assert.doesNotMatch(html, /性能表现良好/);
  assert.match(html, /没有可用的 frame-time 样本/);
  assert.match(html, /no frame-time samples/i);
  assert.match(html, /invalid lines/i);
}

function testReportCoreKpisUseFrameTimeSamplesConsistently() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=1.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/gameclient: stage=render_frame duration_ms=20.000 frame=11 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_global_frame.log', null);

  assert.match(html, /<div class="kpi-label">p99<\/div><div class="kpi-value warn">20\.0<\/div>/);
  assert.match(html, /最大尖峰耗时 20\.0ms/);
}

function testReportTooltipsCanFloatOutsideCharts() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_tooltip.log', null);

  assert.match(html, /\.figure \.chart-wrap\{[^}]*overflow:visible/);
  assert.match(html, /const tooltipPosition = /);
  assert.match(html, /position: tooltipPosition/);
  assert.match(html, /pointer-events:none/);
}

function testSummaryJsonCanBeSerializedForDebugBundle() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'C:/tmp/qm_perf_current.log', { invalidLines: 0, totalLines: 1 });
  const parsed = JSON.parse(JSON.stringify(summary));

  assert.equal(parsed.sourceFile, 'qm_perf_current.log');
  assert.equal(parsed.quality.operation.pages[0], 'settings:tee');
  assert.equal(parsed.verdict, 'PASS');
}

function testSummaryJsonMarksUnavailableVerdictForEmptyFrameSamples() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_empty.log', { invalidLines: 0, totalLines: 1 });
  const current = snapshot(entries, 'qm_perf_empty.log');

  assert.equal(summary.verdict, 'WARN');
  assert.equal(summary.verdictAvailable, false);
  assert.equal(current.verdict, 'WARN');
  assert.equal(current.totalFrames, 0);
  assert.match(summary.quality.warnings.join('\n'), /no frame-time samples/i);
}

function testAnalyzeWritesBundleAndArchiveSummaryFiles() {
  const source = readFileSync(new URL('./analyze.ts', import.meta.url), 'utf-8');

  assert.match(source, /createReadStream/);
  assert.match(source, /createInterface/);
  assert.match(source, /parseLine/);
  assert.match(source, /perf_summary\.json/);
  assert.match(source, /\$\{logName\}_summary\.json/);
  assert.match(source, /summarizeForBundle/);
}

testParseKeepsEventOnlyPerfLines();
testParseSupportsJsonLinesEvents();
testParseLogWithDiagnosticsCountsInvalidLines();
testReportIncludesInteractionAndDeviceSections();
testReportShowsGenerationDuration();
testReportAttributesPagePerformanceEvents();
testServerBrowserListFrameAttributionUsesRowCounts();
testQmUiRuntimeAttributionUsesDedicatedKind();
testFpsSummaryTelemetryFeedsReportAndBundleSummary();
testMissingFpsSummaryWarnsThatSettingsAcceptanceIsIncomplete();
testTargetSettingsVerdictIgnoresServerBrowserFrames();
testTargetSettingsVerdictUsesIngameEscFpsSummaryWindow();
testStableTextCoverageBlocksSettingsAcceptanceEvenWithBackgroundHotspots();
testStableTextCoverageIgnoresOrdinarySettingsPagesOutsideTargetWindow();
testStableTextCoverageIncludesLaterTargetWindows();
testStableTextCoverageUsesPrebuildRemainingBeforeTargetOnly();
testStableTextCoverageBlocksWhenPlanCollectionIncompleteBeforeTarget();
testStableTextCoverageBlocksWhenTargetUsageIsMissing();
testAssetsVisibleFirstAdmissionAppearsInSummaryAndReport();
testAssetsVisibleReadyRequiresEveryPreflightReady();
testAssetsGeometryIgnoresEmptyOrMissingFieldSamples();
testDemoBrowserPhasesAppearInSummaryAndReport();
testSettingsTextAnalysisAggregatesMissesByLocationReasonAndOperation();
testReportIncludesTextPoolMissAnalysisCharts();
testAdaptiveBudgetEventsAppearInSummaryAndReport();
testAdaptiveBudgetMissingWarnsWhenResourceWindowsExist();
testReportPreservesFailVerdictWhenStableTextIsNotBlocking();
testGenericIngamePageContentDoesNotSatisfyOnlineCoverageWarning();
testPerfEventClassifiersKeepBoundariesTight();
testPageSwitchBoundaryDoesNotEnterDurationAttribution();
testSnapshotIgnoresEventOnlyTelemetry();
testComparisonReportWarnsAboutAutomaticBaseline();
testComparisonReportMarksDifferentOperationsAsAdvisory();
testOperationCompatibilityChecksEventsAndStages();
testReportQualityExplainsMissingAndBiasedData();
testOperationCompatibilityIsExplicit();
testBundleSummaryIsStableJsonShape();
testSamplingBiasReportUsesP5Estimate();
testSamplingBiasUsesDefaultLoggingThreshold();
testKpiThresholdsAlignWithVerdictBudget();
testReportIncludesSectionTop10();
testReportShowsQualityAndUnavailableData();
testReportCoreKpisUseFrameTimeSamplesConsistently();
testReportTooltipsCanFloatOutsideCharts();
testSummaryJsonCanBeSerializedForDebugBundle();
testSummaryJsonMarksUnavailableVerdictForEmptyFrameSamples();
testAnalyzeWritesBundleAndArchiveSummaryFiles();

console.log('qmclient perf tests passed');
