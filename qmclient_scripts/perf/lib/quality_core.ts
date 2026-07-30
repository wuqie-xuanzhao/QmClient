// 请抬头享受阳光｜日子很好 我很我---------致咩子
import type { PerfEntry } from './parse.ts';

export interface OperationSignature {
  pages: string[];
  stages: string[];
  systems: string[];
  events: string[];
}

export interface OperationComparison {
  comparable: boolean;
  reason: string;
}

function sortedUnique(values: string[]): string[] {
  return [...new Set(values.filter(v => v.length > 0))].sort();
}

function eventName(e: PerfEntry): string {
  const event = e.fields.event;
  return event === undefined || event === null ? '' : String(event);
}

export function operationSignature(entries: PerfEntry[]): OperationSignature {
  return {
    pages: sortedUnique(entries.map(e => String(e.fields.page ?? e.fields.page_name ?? 'unknown'))),
    stages: sortedUnique(entries.map(e => e.stage)),
    systems: sortedUnique(entries.map(e => e.system)),
    events: sortedUnique(entries.map(eventName)),
  };
}

function sameSet(a: string[], b: string[]): boolean {
  return a.length === b.length && a.every((value, index) => value === b[index]);
}

export function compareOperationSignatures(previous: OperationSignature, current: OperationSignature): OperationComparison {
  if (!sameSet(previous.pages, current.pages)) {
    return { comparable: false, reason: 'page set differs; comparison is advisory only' };
  }
  if (!sameSet(previous.systems, current.systems)) {
    return { comparable: false, reason: 'telemetry system set differs; comparison is advisory only' };
  }
  if (!sameSet(previous.events, current.events)) {
    return { comparable: false, reason: 'event set differs; comparison is advisory only' };
  }
  if (!sameSet(previous.stages, current.stages)) {
    return { comparable: false, reason: 'stage set differs; comparison is advisory only' };
  }
  return { comparable: true, reason: 'same page, stage, event, and telemetry system sets' };
}
