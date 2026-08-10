---
type: question
date: 2026-06-26
status: active
confidence: high
scope:
  - tmp/ddnet-official-simplified_chinese.txt
  - qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml
  - qmclient_scripts/languages_qmclient/translations/i18n/
  - docs/superpowers/plans/archive/2026-06-24-i18n-gores-laser-hook-cleanup.md
commit: d68cd11ded
related:
  - file: 2026-06-12-QmClient-i18n-调用点与分类清单.md
    relation: complements
---

## Quick Answer

官方 DDNet 简中翻译不能被 QmClient 当前生成的 `data/languages/*.txt` 替代；本轮术语调查以 `ddnet/ddnet` 的 `data/languages/simplified_chinese.txt` 为基线，只对照 QmClient 的维护源 TOML。核心结论是：官方简中把 `Hook` 译为“钩索”，`Hook collision line` / `Hook collisions` 常用“钩索辅助线”，旧计划里“hook = 钩子”的前提不成立。

多数基础词与 QmClient 当前维护源一致，例如 `Clan=战队`、`Dummy=分身`、`Map=地图`、`Hammer=锤子`、`Shotgun=霰弹枪`、`Laser=激光`、`Tee=Tee`。明确差异是 `Grenade`：官方简中为“榴弹枪”，而 QmClient 当前术语表和菜单维护源为“榴弹炮”；后续如果要统一 DDNet/core 术语，应优先按官方简中修正，而不是按 QmClient 当前生成文本反推。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|---|---|---|
| 1 | 官方 DDNet 将 `Hook` 译为“钩索”，不是“钩子” | `Hook` 对应 `== 钩索`，同一文件后段再次出现 `Hook == 钩索` | `tmp/ddnet-official-simplified_chinese.txt:181`, `tmp/ddnet-official-simplified_chinese.txt:1954` |
| 2 | 官方 DDNet 将 hook collision UI 的简中口径主要写作“钩索辅助线” | `Hook collisions == 钩索辅助线`、`Hook collision line == 钩索辅助线`；标题大小写版 `Hook Collisions == 瞄准辅助` 是上下文标题译法，不代表单词 `Hook` 改译 | `tmp/ddnet-official-simplified_chinese.txt:799`, `tmp/ddnet-official-simplified_chinese.txt:1309`, `tmp/ddnet-official-simplified_chinese.txt:1327` |
| 3 | QmClient 当前维护源的 Hook 单词已经与官方一致，但旧计划仍要求改成“钩子” | `key = "Hook"` 的简中为“钩索”；旧计划 Task 2 写“hook = 钩子”并要求替换“钩索” | `qmclient_scripts/languages_qmclient/translations/i18n/menus.toml:3330`, `docs/superpowers/plans/archive/2026-06-24-i18n-gores-laser-hook-cleanup.md:95` |
| 4 | `Grenade` 是当前最明确的官方差异 | 官方 `Grenade == 榴弹枪`，QmClient 术语表和菜单维护源均写“榴弹炮” | `tmp/ddnet-official-simplified_chinese.txt:169`, `qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml:212`, `qmclient_scripts/languages_qmclient/translations/i18n/menus.toml:3234` |
| 5 | 多数基础词可确认当前 QmClient 维护源与官方简中一致 | 官方 `Clan=战队`、`Dummy=分身`、`Map=地图`、`Hammer=锤子`、`Shotgun=霰弹枪`、`Laser=激光`、`Tee=Tee`；QmClient 对应维护源也使用这些译法 | `tmp/ddnet-official-simplified_chinese.txt:58`, `tmp/ddnet-official-simplified_chinese.txt:1052`, `tmp/ddnet-official-simplified_chinese.txt:208`, `tmp/ddnet-official-simplified_chinese.txt:172`, `tmp/ddnet-official-simplified_chinese.txt:352`, `tmp/ddnet-official-simplified_chinese.txt:520`, `tmp/ddnet-official-simplified_chinese.txt:1780` |
| 6 | `data/languages/*.txt` 不能作为术语权威，只能作为生成结果核对 | 本仓库验证文档明确写 `translations/i18n/*.toml` 是维护源，`data/languages/*.txt` 是运行时生成产物 | `docs/ai-workflow/verification.md:11`, `docs/ai-workflow/verification.md:21` |

## Details

### 官方基线

本轮使用的官方来源是 `https://raw.githubusercontent.com/ddnet/ddnet/master/data/languages/simplified_chinese.txt`，下载到本地 `tmp/ddnet-official-simplified_chinese.txt` 后做 line-based 对照。只调查简体中文；繁中、日文、韩文等语言没有纳入本轮结论。

### 术语分类

可直接采用官方简中口径的 DDNet/core 术语：

| English | 官方简中 | 说明 |
|---|---|---|
| Clan | 战队 | QmClient 当前一致 |
| Dummy | 分身 | QmClient 当前一致 |
| Map | 地图 | QmClient 当前一致 |
| Team | 队伍 | QmClient 当前一致 |
| Score | 分数 | QmClient 当前一致 |
| Time | 用时 | QmClient 当前一致 |
| Tee | Tee | 保留英文品牌/角色名 |
| Hammer | 锤子 | QmClient 当前一致 |
| Shotgun | 霰弹枪 | QmClient 当前一致 |
| Laser | 激光 | QmClient 当前一致 |
| Hook | 钩索 | 旧计划的“钩子”应撤回 |
| Hook collision line | 钩索辅助线 | `Hook Collisions` 标题可按上下文为“瞄准辅助” |
| Grenade | 榴弹枪 | QmClient 当前为“榴弹炮”，应作为待修正差异 |

不能直接用官方简中推出唯一术语的项：

- `Strong hook` / `Weak hook`：官方简中当前没有独立 source key 命中。本轮只能确认 `hook` 基词是“钩索”，不能直接证明“强钩/弱钩”或“强钩索/弱钩索”哪一个是官方译法。
- QmClient 专属功能，如皮肤队列、Q 弹 Tee、Gores 自动逻辑、AI 翻译服务：官方 DDNet 没有对应 source key，应走项目内上下文命名，但不能反过来覆盖 DDNet/core 基础术语。

## Exploration Scope

- 已看官方 DDNet 简中 raw 文件：`tmp/ddnet-official-simplified_chinese.txt`。
- 已看 QmClient 术语表：`qmclient_scripts/languages_qmclient/prompt_assets/terminology.toml`。
- 已抽查 QmClient 维护源：`qmclient_scripts/languages_qmclient/translations/i18n/menus.toml`、`server_browser.toml`、`loading.toml`、`misc.toml`。
- 已看仓库验证文档对维护源和生成产物的定义：`docs/ai-workflow/verification.md`。
- 未将 `data/languages/simplified_chinese.txt` 作为权威，只把它排除为生成产物。

## Confidence Notes

**confidence: high**

- 官方来源来自 `ddnet/ddnet` 当前 `master` raw 文件，并保存为本地 `tmp/` 快照后逐行核对。
- 结论只覆盖官方简中明确存在的 source key；没有官方 key 的 `Strong hook` / `Weak hook` 已明确降级为未确认。
- QmClient 对照读取的是 `translations/i18n/*.toml` 与 `prompt_assets/terminology.toml`，不是运行时生成的 `data/languages/*.txt`。

## Open Questions

- `Strong hook` / `Weak hook` 在 QmClient 简中里应最终采用“强钩索/弱钩索”还是更短的“强钩/弱钩”，仍需要项目内 UI 宽度、玩家习惯和上下文确认。
- `Grenade` 是否要立刻从“榴弹炮”改为官方“榴弹枪”，需要单独执行 i18n 数据修改和完整语言链验证。

## Related Documents

- `2026-06-12-QmClient-i18n-调用点与分类清单.md`：补充 i18n 调用点和迁移清单，本文件只回答官方简中术语基线。
- `docs/superpowers/plans/archive/2026-06-24-i18n-gores-laser-hook-cleanup.md`：该计划的 hook 术语前提已被本文件纠正，实施前必须按本调查更新 Task 2 / Task 7。

## Next Steps

实施 i18n 术语修正时，先改 `prompt_assets/terminology.toml` 与 `translations/i18n/*.toml` 的简中维护源，再按语言链生成运行时文件并验证；不要直接手改 `data/languages/*.txt`。
