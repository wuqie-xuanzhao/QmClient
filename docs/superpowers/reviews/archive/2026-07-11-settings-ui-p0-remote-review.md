> **已归档，禁止作为实现依据。** 本文保留为历史记录；当前实现请以活动 spec/plan/skill 为准。

# Settings UI P0 远端基线逐提交审查

- 日期：2026-07-11
- 审查范围：`4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3^..8ee4fa22ba0172a66605b2c5033f0736d66ced34`
- 范围规模：12 个对象（11 个普通提交、1 个 merge 路由提交），89 个文件，18,161 行新增、5,881 行删除。
- 依据：当前有效规格、逐提交 patch、CodeGraph 调用链和独立只读深度审查。
- 总体结论：**可以 merge。** 原始范围的 2 个 P1、2 个 P2，以及独立审查追加的 1 个 P1、1 个 P2 均已在本次 no-ff merge 内收口。

## Findings

### [P1] 保存已有地图时，替换失败会不可逆删除原图

`src/game/editor/mapitems/map_io.cpp:66`，由 `6c4ee318d42dadbb6f1488a0f56acff6cdf5167d` 引入。

写完临时地图后先 `RemoveFile(real)`、再 `RenameFile(temp, real)`。删除成功、而重命名因权限、短暂锁定或存储错误失败时，旧图已经删除，新图仍在临时路径，用户地图丢失。

复现：保存已有 `.map`，令删除成功且随后替换移动失败。

合并决策：建立同目录、可恢复的替换策略；失败时至少保留一个可读版本。增加「目标替换失败后旧内容仍可恢复」回归测试。不得仅改善报错文本。

### [P1] 新增分身开关意外关闭 teamplay 中本体的色相循环

`src/game/client/components/players.cpp:1156`，由 `3dc609d8cc947719c929ca2fd3c005931f303f51` 引入。

提交原意是可选地把既有本体色相循环应用到分身，却把 `!GameClient()->IsTeamPlay()` 并入 `m_PlayerUsesCustomColors`。合并前本体只要启用自定义色就能循环；合并后进入 teamplay 时无论分身开关如何都会停止。

复现：本体启用 `qm_cycle_tee_hue` 与任一自定义 body/feet 色，进入 teamplay。

合并决策：本体沿用既有自定义色判断；分身开关只能门控 dummy 路径。提取可测试的 local/dummy 颜色可用性决策，覆盖 teamplay × 本体/分身 × 分身开关 × 0.6/0.7 自定义色组合。

### [P2] 地图历史原地截断写，异常中断会损坏已有历史

`src/game/client/components/tclient/tclient.cpp:4842`，由 `5374d8f1998365c223ba50869202c49f3432cb76` 引入。

`IOFLAG_WRITE` 直接覆盖 `qmclient/map_history.json`。磁盘满、部分写入失败或进程中断时旧 JSON 已被截断，下一启动解析失败后历史丢失。

合并决策：复用地图保存建立的单一安全替换机制，以临时文件完整写入、关闭并成功替换后才删除旧版本；增加失败路径测试，禁止复制「先删后改名」。

### [P2] Windows IME 候选偏移和终止符未经边界校验

`src/engine/client/input.cpp:974`，由 `6c4ee318d42dadbb6f1488a0f56acff6cdf5167d` 引入。

代码直接把 `CANDIDATELIST::dwOffset[i]` 转为 `LPCWSTR` 并转换 UTF-16，未验证 offset 表、每一个 offset 及缓冲区内 NUL 终止。异常或第三方 IME 数据可越界读取并令客户端崩溃。

合并决策：验证固定头、offset 表长度、候选 offset 和有界 UTF-16 终止；跳过不合法候选。增加越界 offset、无终止串和极端分页元数据测试。

## Merge 收口证据

| Finding | 收口方式 | 覆盖 |
|---|---|---|
| 地图保存替换丢失旧图 | `IStorage::ReplaceFileSafely(...)` 统一完成临时文件替换；地图保存和地图历史共用该路径。 | `storage_replace_test.cpp` 覆盖替换失败后仍保留可读版本。 |
| teamplay 本体色相循环回退 | 本体与 dummy 的可用性拆分，dummy 专属开关仅影响 dummy。 | `qm_tee_hue_cycle_test.cpp` 覆盖 teamplay、本体/dummy 与自定义色组合。 |
| 地图历史截断写 | 历史 JSON 写入完成并关闭后，再调用安全替换。 | `map_history_test.cpp` 覆盖历史标识与持久化边界。 |
| Windows IME 越界解析 | 对候选列表头、offset 表、单项 offset 与 UTF-16 终止做有界校验；非法项跳过。 | `qm_ime_platform_test.cpp` 覆盖越界 offset 与无终止候选。 |

审查修复后已重新完成 `check_gate.py --mode default`：11 项通过、1 项既有配置变量 warning、0 项失败；其中 C++ 全量为 2,072 tests / 178 suites，Rust 单测与 doctest 通过。`n`n独立只读深度审查没有 P0，追加发现并已修复 1 个 P1、1 个 P2：翻译语言弹窗的关闭热区从整列限制到标题栏；IME 候选面板的 presentation alpha / scale 不再只写入状态，而是连续参与面板、候选行、选中背景与分页绘制。对应的 `QmChatInteractions.TranslateButtonSitsBeforeInputAndPopupCanCloseItself`、`QmImePresentationSource.PopupUsesContinuousRedirectablePresentationState` 已在修复后定向通过。

## Commit decisions

| Commit | UI 相关路径 | 非 UI 路径 | 版本影响 | 决定与冲突规则 |
|---|---|---|---|---|
| `4f76dcb4e7b5bc86bba9a271b563a8b6a34d53f3` fix(menus): 修复 Linux 环境下连按 ESC 键导致游戏崩溃的问题 | `menus.cpp` MOTD 生命周期 | 翻译测试头 | 无 | 接受；保留完整清理，删除旧半重置 wrapper。 |
| `3dc609d8cc947719c929ca2fd3c005931f303f51` feat: 为 Tee 色相循环新增“同时应用于分身”功能 | 设置、菜单、翻译、配置 | 渲染接线、测试 | 中间 2.74.13 | 接受并修复 P1；新增配置保留，本体恢复 teamplay 语义。 |
| `6c4ee318d42dadbb6f1488a0f56acff6cdf5167d` feat: 更新版本号至 2.74.14，并增强 IME 候选列表处理逻辑 | IME、保存提示 | Windows 输入、Vulkan、地图写入 | 中间 2.74.14 | 接受并修复 P1/P2；错误处理改为有界、可恢复单一路径。 |
| `d161bd10adffdff42a9b85d09f7336a64a708f60` 重构 IME 候选词弹窗与管理器，优化文件浏览器逻辑，并引入媒体灵动岛逻辑 | `QmUi`、IME popup、菜单、文件浏览器、聊天、HUD | 媒体岛、输入策略、CMake | 中间 2.74.15 | 接受；保留 presentation-state 单一路径；composition-only 展示留人工验收。 |
| `f5d799ec0f967d9e85d563b4a0c25fc99a321445` feat: 更新版本号至 2.74.16，并增强聊天和媒体岛逻辑测试 | 聊天、媒体岛测试 | 无 | 中间 2.74.16 | 接受；保留测试意图。 |
| `bbdabda911bb8f9058f18d78ce45cdc113464c4a` feat: 更新 QmClient 版本号至 2.74.17 | 无 | 无 | 中间 2.74.17 | 接受；最终以 tip 为准，禁止倒退或重复 tag。 |
| `57c321de2a02a72abab638bd68117bcd4cd0f521` 增强 Gores 距离场逻辑与编辑器 Quad 缩放功能 | 设置、编辑器 Quad 缩放 | Gores、渲染辅助 | 中间 2.74.18 | 接受；不扩大编辑器业务。 |
| `5374d8f1998365c223ba50869202c49f3432cb76` feat：实现地图历史记录与管理功能 | 浏览器历史、图标 | JSON 模型、持久化 | 中间 2.74.20 | 接受并修复 P2；与地图保存共用替换机制。 |
| `1ea8259dc3dd894d02fe5c69a0046fccec20dff4` Merge pull request #171 from Royikiss/fix/esc-crash | 无 | 无 | 无 | routing/covered；只路由 `4f76...`，不重复应用。 |
| `8f2446baec1fc0757ed18bd9cae8474254a5a238` refactor：将编辑器中的“包络线”术语统一重命名为“动画” | 编辑器文案、弹窗、快捷操作 | 编辑器命名/map IO | 中间 2.74.21 | 接受；格式/协议冲突必须停止。 |
| `20933933bc0e53cf2b6909c2fcfec65e4abbe46d` 重构聊天展示逻辑，并新增服务端控制皮肤功能 | 聊天、Qm 菜单模式 | skin 信息、Infection 谓词 | 中间 2.74.22 | 接受；保留单一 server-controlled skin 路径。 |
| `8ee4fa22ba0172a66605b2c5033f0736d66ced34` feat: 更新 QmClient 版本号至 2.74.23，并修复聊天展示测试中的偏移量检查 | 聊天、菜单、监控测试 | 无 | 最终 2.74.23 | 接受；远端已有 `v2.74.23`，P0 不额外 bump。 |

## Range conclusion

11 个普通提交已审查，1 个 merge 路由关系已记录。当前 `dyl_dev` 已以 `git merge --no-ff --no-commit 8ee4fa22ba0172a66605b2c5033f0736d66ced34` 进入 merge，并已收口上述 2 个 P1、2 个 P2 和独立审查追加的 1 个 P1、1 个 P2；独立审查无 P0，merge commit 可在最终 staged diff 与 gate 复核后创建。

设置卡片、公共输入、滚动、dropdown、动效 runtime 和性能基础设施仍由 active spec 约束；地图保存、地图历史、IME 平台边界、Gores、编辑器术语、聊天与 server-controlled skin 的业务改变不因规格自动进入 P1–P7，除本报告列出的合并安全修复外均保持范围隔离。
