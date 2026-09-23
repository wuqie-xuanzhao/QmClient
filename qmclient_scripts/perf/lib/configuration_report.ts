// 请抬头享受阳光｜日子很好 我很我---------致咩子
import type { PerfConfigurationSummary } from './configuration.ts';

function escape(value: unknown): string {
  return String(value ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

export function configurationReport(summary: PerfConfigurationSummary): string {
  const groups = [['ddnet', 'DDNet'], ['qmclient', 'QmClient'], ['tclient', 'TClient']];
  const tables = groups.map(([owner, title]) => {
    const rows = summary.variables.filter(v => v.owner === owner);
    if (rows.length === 0) return '';
    return '<details open><summary>' + title + ' · ' + rows.length + ' 项</summary>' +
      '<div style="overflow-x:auto"><table><thead><tr><th>配置项</th><th>类型</th><th>当前值</th><th>开始采集时</th><th>状态</th></tr></thead><tbody>' +
      rows.map(v => '<tr data-config-row><td class="mono">' + escape(v.name) + '</td><td>' + escape(v.type) +
        '</td><td style="white-space:pre-wrap;overflow-wrap:anywhere;max-width:36em">' + escape(v.value) +
        '</td><td style="white-space:pre-wrap;overflow-wrap:anywhere;max-width:24em">' +
        (v.changed ? escape(v.initialValue) : v.initialValue === null ? '未记录' : '同当前值') +
        '</td><td>' + (v.redacted ? '已脱敏' : v.changed ? '采集中已更改' : v.isDefault ? '默认值' : '自定义') + '</td></tr>').join('') +
      '</tbody></table></div></details>';
  }).join('');
  return '<section class="section" id="configuration"><h2>当前客户端配置</h2>' +
    (summary.available
      ? '<p class="body-text">记录采集开始时和最新的运行时配置，包含默认值和关闭项。密码、Token、密钥和 Cookie 已脱敏；不包含按键绑定。采集期间的完整变更记录见原始日志。</p>' +
        '<input id="configuration-filter" type="search" placeholder="搜索配置名或配置值" aria-label="搜索配置" style="width:100%;padding:8px;margin-bottom:12px">' + tables
      : '<p class="body-text">此日志未记录客户端配置。使用统一性能诊断开关重新采集后可查看。</p>') +
    (summary.incomplete ? '<p class="body-text">配置分段未完整写入，以下仅展示已完整接收的值。</p>' : '') + '</section>';
}
