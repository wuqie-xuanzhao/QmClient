// 请抬头享受阳光｜日子很好 我很我---------致咩子
import type { PerfEntry } from './parse.ts';

interface PendingValue {
  revision: number;
  entry: PerfEntry;
  chunks: (string | undefined)[];
}

const sensitiveName = (name: string) => /password|secret|token|api_key|apikey|llm_key_|libre_key|cookie|sp_dc|authorization/i.test(name);

/** 逐项合并分段，只保留完整的初始值和最新值，配置变更历史仍保存在原日志。 */
export class PerfConfigurationCollector {
  private pending = new Map<string, PendingValue>();
  private complete = new Map<string, PerfEntry>();
  private malformed = false;
  private snapshots = new Map<string, PerfEntry>();

  add(entry: PerfEntry): void {
    const f = entry.fields;
    if (f.event === 'config_snapshot') {
      const expected = Number(f.expected_variables);
      if (!Number.isSafeInteger(expected) || expected < 0) this.malformed = true;
      else this.snapshots.set(String(f.session ?? ''), entry);
      return;
    }
    const name = String(f.name ?? '');
    const revision = Number(f.revision);
    const part = Number(f.part ?? 0);
    const parts = Number(f.parts ?? 1);
    if (!name || !Number.isSafeInteger(revision) || revision < 0 ||
        !Number.isInteger(parts) || parts < 1 || parts > 1024 ||
        !Number.isInteger(part) || part < 0 || part >= parts || typeof f.value !== 'string') {
      this.malformed = true;
      return;
    }
    const key = JSON.stringify([f.session ?? '', name, Number(f.initial) === 1]);
    const previous = this.complete.get(key);
    if (previous && Number(previous.fields.revision) > revision) return;
    let pending = this.pending.get(key);
    if (pending && pending.revision > revision) return;
    if (!pending || pending.revision !== revision) {
      pending = { revision, entry, chunks: Array.from({ length: parts }) };
      this.pending.set(key, pending);
    }
    if (pending.chunks.length !== parts) {
      this.malformed = true;
      return;
    }
    pending.chunks[part] = f.value;
    if (pending.chunks.some(chunk => chunk === undefined)) return;
    const redacted = String(f.type) === 'string' && (sensitiveName(name) || Number(f.redacted) === 1);
    const fields = {
      ...pending.entry.fields,
      value: redacted ? '<redacted>' : pending.chunks.join(''),
      redacted: redacted ? '1' : '0',
      part: '0',
      parts: '1',
    };
    this.complete.set(key, { ...pending.entry, fields });
    this.pending.delete(key);
  }

  get incomplete(): boolean {
    if (this.malformed || this.pending.size > 0) return true;
    for (const [session, snapshot] of this.snapshots) {
      const received = [...this.complete.values()].filter(e => String(e.fields.session ?? '') === session && Number(e.fields.initial) === 1).length;
      if (received !== Number(snapshot.fields.expected_variables)) return true;
    }
    return false;
  }
  entries(): PerfEntry[] { return [...this.snapshots.values(), ...this.complete.values()]; }
}

export interface PerfConfigurationVariable {
  session: string;
  name: string;
  owner: string;
  type: string;
  value: string | number;
  initialValue: string | number | null;
  isDefault: boolean;
  redacted: boolean;
  changed: boolean;
  revision: number;
}

export interface PerfConfigurationSummary {
  available: boolean;
  incomplete: boolean;
  variables: PerfConfigurationVariable[];
}

export function configurationSummary(entries: PerfEntry[]): PerfConfigurationSummary {
  const collector = new PerfConfigurationCollector();
  for (const entry of entries) {
    if (entry.system === 'perf/config') collector.add(entry);
  }
  const values = new Map<string, { initial?: PerfEntry; current?: PerfEntry }>();
  for (const entry of collector.entries()) {
    if (entry.fields.event !== 'config_value') continue;
    const key = JSON.stringify([entry.fields.session ?? '', entry.fields.name]);
    const value = values.get(key) ?? {};
    if (Number(entry.fields.initial) === 1) value.initial = entry;
    if (!value.current || Number(entry.fields.revision) >= Number(value.current.fields.revision)) value.current = entry;
    values.set(key, value);
  }
  const typed = (entry: PerfEntry): string | number => {
    const { type, value } = entry.fields;
    return (type === 'int' || type === 'color') && Number.isFinite(Number(value)) ? Number(value) : String(value);
  };
  const variables = [...values.values()].map(({ initial, current }) => {
    const row = current!;
    return {
      session: String(row.fields.session ?? ''),
      name: String(row.fields.name),
      owner: String(row.fields.owner ?? 'ddnet'),
      type: String(row.fields.type),
      value: typed(row),
      initialValue: initial ? typed(initial) : null,
      isDefault: Number(row.fields.is_default) === 1,
      redacted: Number(row.fields.redacted) === 1,
      changed: initial !== undefined && initial.fields.value !== row.fields.value,
      revision: Number(row.fields.revision),
    };
  }).sort((a, b) => a.owner.localeCompare(b.owner) || a.name.localeCompare(b.name) || a.session.localeCompare(b.session));
  return { available: variables.length > 0, incomplete: collector.incomplete, variables };
}
