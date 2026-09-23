// 请抬头享受阳光｜日子很好 我很我---------致咩子
import type { PerfEntry, ParseDiagnostics, ParseResult } from './parse.ts';
import { PerfConfigurationCollector } from './configuration.ts';

/** 独立保留帧和明细的均匀蓄水池样本，避免大日志全部驻留内存。 */
class Reservoir {
  entries: PerfEntry[] = [];
  seen = 0;
  private randomState = 123456789;

  constructor(private capacity: number) {
    if (!Number.isInteger(capacity) || capacity < 1) throw new Error('sample capacity must be positive');
  }
  add(entry: PerfEntry): void {
    this.seen++;
    if (this.entries.length < this.capacity) {
      this.entries.push(entry);
      return;
    }
    this.randomState = (Math.imul(this.randomState, 1664525) + 1013904223) >>> 0;
    const index = Math.floor((this.randomState / 4294967296) * this.seen);
    if (index < this.capacity) this.entries[index] = entry;
  }
}

export function expandFrameBatch(entry: PerfEntry): PerfEntry[] {
  if (entry.system !== 'perf/frame' || entry.fields.event !== 'frame_batch') return [entry];
  const fields = entry.fields as Record<string, unknown>;
  const frames = fields.frames;
  const durations = fields.durations_ms;
  if (!Array.isArray(frames) || !Array.isArray(durations) || frames.length !== durations.length ||
      frames.length < 1 || frames.length > 64 ||
      frames.some(frame => !Number.isSafeInteger(frame) || frame < 0) ||
      durations.some(duration => typeof duration !== 'number' || !Number.isFinite(duration) || duration < 0)) {
    throw new Error('invalid performance frame batch');
  }
  // 同批次各帧按累计时间还原时间轴，帧编号沿用客户端原始编号。
  const zoned = /(Z|[+-]\d{2}:\d{2})$/.test(entry.timestamp);
  let end = Date.parse(zoned ? entry.timestamp : entry.timestamp + 'Z');
  const result: PerfEntry[] = [];
  for (let i = frames.length - 1; i >= 0; i--) {
    const timestamp = Number.isFinite(end) ? new Date(end).toISOString().replace(zoned ? /$/ : /Z$/, '') : entry.timestamp;
    result.push({
      timestamp, system: 'perf/frame', stage: 'render_interval', durationMs: durations[i],
      fields: {
        event: 'frame_sample', session: String(fields.session ?? ''),
        frame: String(frames[i]), duration_ms: String(durations[i]),
      },
    });
    end -= durations[i];
  }
  return result.reverse();
}

export class PerfLogCollector {
  private frames: Reservoir;
  private details: Reservoir;
  private events: PerfEntry[] = [];
  private eventsSeen = 0;
  private configs = new PerfConfigurationCollector();
  private totalEntries = 0;

  constructor(frameLimit = 100000, detailLimit = 50000, private eventLimit = 20000) {
    this.frames = new Reservoir(frameLimit);
    this.details = new Reservoir(detailLimit);
    if (!Number.isInteger(eventLimit) || eventLimit < 1) throw new Error('event capacity must be positive');
  }

  add(entry: PerfEntry): void {
    this.totalEntries++;
    if (entry.system === 'perf/config') {
      this.configs.add(entry);
    } else if (entry.system === 'perf/frame') {
      this.frames.add(entry);
    } else if (entry.system === 'perf/stutter' || entry.system === 'perf/fps' || entry.system === 'perf/session') {
      // 窗口事件保留最近一段，尽量保持同一窗口的组件记录在一起。
      this.events[this.eventsSeen++ % this.eventLimit] = entry;
    } else {
      this.details.add(entry);
    }
  }

  finish(diagnostics: ParseDiagnostics): ParseResult {
    const offset = this.eventsSeen > this.eventLimit ? this.eventsSeen % this.eventLimit : 0;
    const events = [...this.events.slice(offset), ...this.events.slice(0, offset)];
    const entries = [...this.frames.entries, ...this.details.entries, ...events, ...this.configs.entries()]
      .sort((a, b) => a.timestamp.localeCompare(b.timestamp) || Number(a.fields.frame ?? 0) - Number(b.fields.frame ?? 0));
    return {
      entries,
      diagnostics: {
        ...diagnostics,
        totalEntries: this.totalEntries,
        retainedEntries: entries.length,
        sampledEntries: this.frames.seen - this.frames.entries.length + this.details.seen - this.details.entries.length + this.eventsSeen - events.length,
        configurationIncomplete: this.configs.incomplete,
      },
    };
  }
}
