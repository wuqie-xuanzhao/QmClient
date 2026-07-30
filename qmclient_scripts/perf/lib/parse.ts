// 请抬头享受阳光｜日子很好 我很我---------致咩子
// parse.ts — 解析 QmClient 性能日志（支持旧 key=value 格式 + JSON Lines）

export interface PerfEntry {
  /** 原始日志时间戳 (ISO) */
  timestamp: string;
  /** 系统标签，如 "perf/menu", "perf/gameclient", "perf/section" */
  system: string;
  /** stage 名称 */
  stage: string;
  /** 耗时 (ms) */
  durationMs: number;
  /** 所有 key=value 对（stage/duration 已提取） */
  fields: Record<string, string>;
}

export interface ParseDiagnostics {
  totalLines: number;
  invalidLines: number;
}

export interface ParseResult {
  entries: PerfEntry[];
  diagnostics: ParseDiagnostics;
}

const LOG_LINE_RE = /^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})\s+\w\s+(perf\/\S+):\s+(.+)$/;
const KV_RE = /(\w+)=(\S+)/g;

function parseTimestamp(raw: string): string {
  return raw.replace(' ', 'T');
}

export function parseLine(line: string): PerfEntry | null {
  const parseJsonPayload = (jsonText: string, fallbackTimestamp = '', fallbackSystem = ''): PerfEntry | null => {
    try {
      const obj = JSON.parse(jsonText);
      return {
        timestamp: obj.t ?? obj.timestamp ?? fallbackTimestamp,
        system: obj.sys ?? obj.system ?? fallbackSystem,
        stage: obj.stage ?? '',
        durationMs: obj.dur ?? obj.duration_ms ?? 0,
        fields: obj,
      };
    } catch {
      return null;
    }
  };

  // 尝试 JSON Lines 格式
  if (line.startsWith('{')) {
    const parsed = parseJsonPayload(line);
    if (parsed) return parsed;
  }

  // 旧 key=value 格式
  const match = line.match(LOG_LINE_RE);
  if (!match) return null;

  const [, rawTs, system, fieldsStr] = match;
  if (fieldsStr.startsWith('{')) {
    const parsed = parseJsonPayload(fieldsStr, parseTimestamp(rawTs), system);
    if (parsed) return parsed;
  }
  const fields: Record<string, string> = {};
  let matchKv: RegExpExecArray | null;
  const re = new RegExp(KV_RE.source, 'g');
  while ((matchKv = re.exec(fieldsStr)) !== null) {
    fields[matchKv[1]] = matchKv[2];
  }

  return {
    timestamp: parseTimestamp(rawTs),
    system,
    stage: fields.stage ?? fields.stage_name ?? '',
    durationMs: parseFloat(fields.duration_ms ?? fields.dur ?? '0'),
    fields,
  };
}

export function parseLog(content: string): PerfEntry[] {
  return parseLogWithDiagnostics(content).entries;
}

export function parseLogWithDiagnostics(content: string): ParseResult {
  const entries: PerfEntry[] = [];
  let totalLines = 0;
  let invalidLines = 0;

  for (const rawLine of content.split('\n')) {
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

  return {
    entries,
    diagnostics: { totalLines, invalidLines },
  };
}

export function groupBySystem(entries: PerfEntry[]): Map<string, PerfEntry[]> {
  const map = new Map<string, PerfEntry[]>();
  for (const e of entries) {
    const list = map.get(e.system) ?? [];
    list.push(e);
    map.set(e.system, list);
  }
  return map;
}

export function groupByStage(entries: PerfEntry[]): Map<string, PerfEntry[]> {
  const map = new Map<string, PerfEntry[]>();
  for (const e of entries) {
    const list = map.get(e.stage) ?? [];
    list.push(e);
    map.set(e.stage, list);
  }
  return map;
}

export function groupByPage(entries: PerfEntry[]): Map<string, PerfEntry[]> {
  const map = new Map<string, PerfEntry[]>();
  for (const e of entries) {
    const page = e.fields.page ?? e.fields.page_name ?? 'unknown';
    const list = map.get(page) ?? [];
    list.push(e);
    map.set(page, list);
  }
  return map;
}
