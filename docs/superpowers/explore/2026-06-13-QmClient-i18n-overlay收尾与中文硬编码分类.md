---
type: question
date: 2026-06-13
status: active
confidence: medium
scope:
  - src/game/client/components/chat.cpp
  - src/game/client/components/menus.cpp
  - src/game/client/components/menus_browser.cpp
  - src/game/client/components/menus_ingame.cpp
  - src/game/client/components/hud.cpp
  - src/game/client/components/assets_resource_registry.cpp
  - src/game/client/gameclient.cpp
  - src/test/qm_new_ui_menu_branch_test.cpp
  - qmclient_scripts/languages_qmclient
  - data/languages/simplified_chinese.txt
commit: cecb13e74
related:
  - file: 2026-06-12-QmClient-i18n-调用点与分类清单.md
    relation: complements
  - file: 2026-06-12-QmClient-i18n-系统现状与优化方向调查.md
    relation: complements
---

## Quick Answer

QmClient 语言 overlay 的主收口已经完成：运行时 overlay 加载链已删除，`data/qmclient/languages/` 目录也已删除；当前残留的相关问题主要转成了“旧脚本尾巴”和“非 `Localize` 中文硬编码的职责判断”，不再是双轨语言资源模型本身。`src/game/client` 里的中文硬编码并不应该一刀切全改 `Localize()`：语言选择器里的语言自称、服务器相关中文提示、基于中文服务器消息做匹配的规则、以及资源分类别名，属于四种不同职责，需要分别处理。`data/languages/simplified_chinese.txt` 当前共有 8819 行、2940 个 key，其中至少 58 个 key 有重复定义候选，这说明后续很值得补一个“词库查重/未引用项盘点”工具，而不该继续手工收。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|-----------|----------|----------|
| 1 | overlay 运行时加载链已移除 | 结构测试现在显式断言 `LoadQmClientLanguageOverlay(...)` 不存在，并断言 `data/qmclient/languages` 目录不存在 | `src/test/qm_new_ui_menu_branch_test.cpp:271`, `src/test/qm_new_ui_menu_branch_test.cpp:277`, `src/test/qm_new_ui_menu_branch_test.cpp:740` |
| 2 | overlay 目录已不再作为运行时资源存在 | 本轮检查 `data/qmclient/languages` 返回 `missing`；仓库代码侧仅剩测试断言和脚本文案中的“legacy overlay removed”描述 | `src/test/qm_new_ui_menu_branch_test.cpp:740`, `qmclient_scripts/languages_qmclient/validate.py:57`, `qmclient_scripts/languages_qmclient/validate.py:60` |
| 3 | 语言选择器里的中文/日文等不是普通 UI 文案，而是目标语言自称 | 聊天翻译菜单与 QmClient 翻译设置页都使用 `s_apLangNames[] = {"中文", "English", "日本語", ...}` 作为下拉展示值，这些值直接对应语言 code，而不是当前界面语言 | `src/game/client/components/chat.cpp:3399`, `src/game/client/components/qmclient/menus_qmclient.cpp:4163`, `src/game/client/components/qmclient/menus_qmclient.cpp:4498`, `src/game/client/components/qmclient/menus_qmclient.cpp:4525` |
| 4 | 有一类中文硬编码应保留中文，因为它们是在描述或匹配中文服务器语义 | 举报流程按钮/状态直接面向中文服务器生态；HUD 里 swap 和开关文本也直接以中文展示；更关键的是 HUD notification 规则大量以中文服务器消息为匹配输入 | `src/game/client/components/menus_ingame.cpp:332`, `src/game/client/components/menus_ingame.cpp:460`, `src/game/client/components/hud.cpp:742`, `src/game/client/components/qmclient/hud_notifications/hud_notification_rules.cpp:314`, `src/game/client/components/qmclient/hud_notification_static_rules.h:127` |
| 5 | 还有一类中文硬编码更适合抽成统一映射/国际化函数，而不是继续散落 | `menus.cpp` 的“统计/概览/客户端启动时间”等是普通菜单 UI 文案；`menus_browser.cpp` 的“新增分类/重命名/取消”等是通用按钮文案；这些不依赖中文服务器协议，也不是语言自称 | `src/game/client/components/menus.cpp:2195`, `src/game/client/components/menus.cpp:2309`, `src/game/client/components/menus_browser.cpp:2946`, `src/game/client/components/menus_browser.cpp:2987` |
| 6 | 旧 overlay 维护脚本已经没有正向调用链，属于可删遗留物 | `copy_fix_qmclient.py` / `update_all_qmclient.py` 只在彼此和旧调研文档中出现，仓库代码、运行时和当前脚本主链都不再引用它们 | `qmclient_scripts/languages_qmclient/update_all_qmclient.py:3`, `qmclient_scripts/languages_qmclient/copy_fix_qmclient.py:46`, `docs/dyl/QmClient_docs/02-调研报告/脚本工具调研报告.md:173` |
| 7 | 简中词库已经大到值得工具化治理 | 当前 `data/languages/simplified_chinese.txt` 共有 8819 行；按 DDNet 文本格式过滤后有 2940 个 key，其中 58 个 key 出现重复候选，前 20 个重复项包括 `Add Friend`、`Auto`、`Connect Dummy` 等 | `data/languages/simplified_chinese.txt`, PowerShell统计结果（本轮） |

## Details

### 非 `Localize` 中文硬编码的四类职责

1. 语言自称 / 语言选择器展示值
   代表项：`中文`、`English`、`日本語`。
   这类文本的职责是“标识目标语言”，不是“翻译当前界面”。优先保留原生自称，不建议走 `Localize()`。

2. 中文服务器生态提示 / 中文优先交互
   代表项：举报流程按钮、Axiom 限制提示、`Swap:%d秒`。
   这类内容如果主要服务中文服务器生态，保留中文通常更合理；若后续要国际化，也应该通过独立函数或资源层做，而不是先机械改英文。

3. 服务器消息匹配 / 规则识别字符串
   代表项：`hud_notification_rules.cpp`、`hud_notification_static_rules.h` 里的中文句子。
   这类文本不是 UI，而是“输入模式匹配”。必须按服务器真实输出保留，否则规则会失效。

4. 普通 UI 文案 / 可本地化按钮标题
   代表项：`统计`、`概览`、`新增分类`、`重命名`、`取消`。
   这类是最应该进入 `Localize()` 或统一国际化函数的区域，后续可继续收。

### `validate.py` 当前状态

本轮执行 `python qmclient_scripts/languages_qmclient/validate.py` 时，脚本文案出现了“前两项 OK，但末尾仍打印 `1 files with errors!`”的自相矛盾输出；同时用 `cmd /c ... & echo EXITCODE` 检查时，直接从 shell 返回了 `9009`，说明这里还混着环境/调用口径问题，不能把它当成稳定的最终验收口径。

这意味着：

- overlay 是否删除，不能只靠这条脚本文案判断；
- 当前更可信的证据仍然是结构测试断言和目录实查；
- `validate.py` 自身还需要单独修。

## Exploration Scope

- Focused directory: `src/game/client`, `qmclient_scripts/languages_qmclient`, `data/languages`
- Files involved: `chat.cpp`, `menus.cpp`, `menus_browser.cpp`, `menus_ingame.cpp`, `hud.cpp`, `assets_resource_registry.cpp`, `gameclient.cpp`, `qm_new_ui_menu_branch_test.cpp`, `generate_all.py`, `migrate_chinese_keys_to_english.py`, `twlang_qmclient.py`, `validate.py`
- Skipped: 全量逐条核对 `simplified_chinese.txt` 的“代码未引用项”；这一步需要专门脚本，否则人工成本过高且容易误判

## Confidence Notes

**confidence: medium**

- overlay 运行时删除状态有充分证据，可信度高。
- 非 `Localize` 中文硬编码的职责分类，已覆盖主要类型，但还没有把 `src/game/client` 全目录逐条判定完毕。
- `validate.py` 当前存在环境/输出不一致问题，因此不能对“脚本级最终验收”给出高置信度。

## Open Questions

- `validate.py` 为何在打印错误文案时仍返回成功，是否是 PowerShell / Python 入口混用导致的执行口径问题。
- `simplified_chinese.txt` 的 58 个重复 key 中，哪些是历史 DDNet 基表重复，哪些是这轮 merge 新引入。
- 如何自动找出“代码中已无引用”的简中 key，并区分“运行时动态字符串仍需保留”与“可安全删除项”。

## Added Script

已补只读审查脚本：`qmclient_scripts/languages_qmclient/review_duplicate_entries.py`。

用途：

- 默认审查 `data/languages/simplified_chinese.txt`，也可用 `--all-languages` 审查 `data/languages/*.txt`。
- 终端报告会列出重复 key、相似 key、空译文、重复译文、相似译文、疑似未使用项。
- 每个条目显示文件、key 行号、译文行号、context、key 内容和译文内容。
- 脚本不自动删除或改写语言文件；报告给 AI 或维护者手工判断后再修改。

推荐命令：

```bash
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --show-groups 30 --show-unused 80
python qmclient_scripts/languages_qmclient/review_duplicate_entries.py --all-languages --show-groups 10 --show-unused 20
```

## Related Documents

- `2026-06-12-QmClient-i18n-调用点与分类清单.md` — 记录了第一轮收口前的调用点基线
- `2026-06-12-QmClient-i18n-系统现状与优化方向调查.md` — 解释了 overlay 双轨模型为何会造成长期漂移

## Next Steps

下一步应优先把 `validate.py` 修成可信口径，并基于 `review_duplicate_entries.py` 的报告手工收口重复、相似和疑似未使用语言项，再继续第二轮普通 UI 中文硬编码收口。
