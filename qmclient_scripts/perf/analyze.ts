#!/usr/bin/env bun
// analyze.ts — QmClient 性能日志分析入口
// 用法: bun analyze.ts [log文件路径]
//       如果不传路径，自动读取 %APPDATA%/DDNet/dumps/QmClient_Perf/ 下最新日志
//       自动检测上一次报告对应的日志，生成对比分析

import { createReadStream, writeFileSync, readdirSync, statSync, mkdirSync, existsSync } from 'node:fs';
import { createInterface } from 'node:readline';
import { join, basename } from 'node:path';

const PERF_DIR = () => join(process.env.APPDATA ?? '', 'DDNet', 'dumps', 'QmClient_Perf');
const REPORT_DIR = () => join(PERF_DIR(), 'Perf_Report');

function listLogFiles(): { name: string; path: string; mtime: number }[] {
  const dir = PERF_DIR();
  try {
    return readdirSync(dir)
      .filter(f => f.startsWith('qm_perf_') && f.endsWith('.log'))
      .map(f => ({ name: f, path: join(dir, f), mtime: statSync(join(dir, f)).mtimeMs }))
      .sort((a, b) => b.mtime - a.mtime);
  } catch {
    return [];
  }
}

function findLatestLog(): string {
  const files = listLogFiles();
  if (files.length === 0) {
    console.error('无法找到性能日志文件。请确保:');
    console.error('  1. qm_perf_debug 1 和 qm_perf_logfile 1 已启用');
    console.error('  2. 至少运行过一次游戏客户端');
    process.exit(1);
  }
  return files[0].path;
}

/** 找到上一次报告对应的日志文件（当前日志的前一个） */
function findPreviousLog(currentLogPath: string): string | null {
  const files = listLogFiles();
  const currentName = basename(currentLogPath);
  const idx = files.findIndex(f => f.name === currentName);
  if (idx < 0 || idx >= files.length - 1) return null;
  return files[idx + 1].path; // 下一个 = 时间更早的
}

async function main() {
  const startedAt = performance.now();
  const { parseLine } = await import('./lib/parse.ts');
  const { generateReport } = await import('./lib/report.ts');
  const { snapshot, compareSessions } = await import('./lib/stats.ts');
  const { summarizeForBundle } = await import('./lib/quality.ts');

  async function parseLogFileWithDiagnostics(path: string) {
    const entries = [];
    let totalLines = 0;
    let invalidLines = 0;
    const lines = createInterface({
      input: createReadStream(path, { encoding: 'utf-8' }),
      crlfDelay: Infinity,
    });
    for await (const rawLine of lines) {
      const line = rawLine.trim();
      if (line.length === 0) {
        continue;
      }
      totalLines++;
      const entry = parseLine(line);
      if (entry === null) {
        invalidLines++;
        continue;
      }
      entries.push(entry);
    }
    return { entries, diagnostics: { totalLines, invalidLines } };
  }

  const logPath = process.argv[2] ?? findLatestLog();
  console.log(`读取: ${logPath}`);

  console.log('解析中...（流式读取）');
  const parsed = await parseLogFileWithDiagnostics(logPath);
  const entries = parsed.entries;
  console.log(`解析行数: ${parsed.diagnostics.totalLines}`);
  console.log(`有效条目: ${entries.length}`);
  if (parsed.diagnostics.invalidLines > 0) {
    console.log(`忽略无效行: ${parsed.diagnostics.invalidLines}`);
  }

  // 当前会话快照
  const currentSnapshot = snapshot(entries, logPath);

  // 自动查找上一次日志并生成对比
  let comparison = null;
  const prevLogPath = findPreviousLog(logPath);
  if (prevLogPath) {
    try {
      const prevParsed = await parseLogFileWithDiagnostics(prevLogPath);
      const prevEntries = prevParsed.entries;
      if (prevEntries.length > 0) {
        const prevSnapshot = snapshot(prevEntries, prevLogPath);
        comparison = compareSessions(prevSnapshot, currentSnapshot);
        console.log(`对比基线: ${basename(prevLogPath)} (${prevEntries.length} 条)`);
        if (comparison.verdictChanged) {
          console.log(`  判定变化: ${comparison.previous.verdict} → ${comparison.current.verdict}`);
        }
        const reg = comparison.metrics.filter(m => m.direction === 'worse').length;
        const imp = comparison.metrics.filter(m => m.direction === 'better').length;
        console.log(`  ${imp} 项改善, ${reg} 项退化`);
      }
    } catch (e) {
      console.log(`跳过对比: ${(e as Error).message}`);
    }
  } else {
    console.log('无历史日志可对比（首次分析）');
  }

  const reportHtml = generateReport(entries, logPath, comparison, parsed.diagnostics, performance.now() - startedAt);
  const summaryJson = summarizeForBundle(entries, logPath, parsed.diagnostics);

  // 输出到 Perf_Report/ 子目录
  const reportDir = REPORT_DIR();
  if (!existsSync(reportDir)) {
    mkdirSync(reportDir, { recursive: true });
  }

  const logName = basename(logPath).replace('.log', '');
  const outPath = join(reportDir, `${logName}_report.html`);
  writeFileSync(outPath, reportHtml, 'utf-8');
  const summaryText = JSON.stringify(summaryJson, null, 2);
  const archiveSummaryPath = join(reportDir, `${logName}_summary.json`);
  const bundleSummaryPath = join(reportDir, 'perf_summary.json');
  writeFileSync(archiveSummaryPath, summaryText, 'utf-8');
  writeFileSync(bundleSummaryPath, summaryText, 'utf-8');
  console.log(`报表已生成: ${outPath}`);
  console.log(`摘要已生成: ${archiveSummaryPath}`);
  console.log(`Debug bundle 摘要已生成: ${bundleSummaryPath}`);
}

main();
