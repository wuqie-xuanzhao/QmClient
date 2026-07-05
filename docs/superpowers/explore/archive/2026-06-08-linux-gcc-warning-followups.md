---
type: question
date: 2026-06-08
status: active
confidence: high
scope:
  - src/game/client/gameclient.cpp
  - src/game/client/components/menus_assets_editor.cpp
  - src/base/str.cpp
  - src/game/client/components/qmclient/perf_logging.h
  - AGENTS.md
  - docs/ai-workflow/verification.md
  - qmclient_scripts/scripts_overview.md
commit: HEAD
related:
  - file: 2026-06-02-设置页性能量化系统设计.md
    relation: references
---

## Quick Answer

当前 WSL Linux GCC 打包链路已经可用，剩余 warning 可以分成两类：一类是低风险源码卫生项，适合以后继续小步收敛；另一类更像真实缺陷或底层兼容性问题，不应混在“顺手消 warning”的补丁里。就当前证据看，`gameclient.cpp` 中未使用的 `LogSettingsLoadingPrewarmEvent` 属于低风险清理项，而 `menus_assets_editor.cpp` 的“返回局部变量地址”与 `str.cpp` 的底层字符串 helper 告警都值得单独立项分析。

工作流层面，现有根规则已经提到“至少覆盖 build/test/gate”，但入口文档没有把“不能只跑 build/test 代替 gate”写成更硬的验收要求。后续应把这条规则前置到 `AGENTS.md` / `CLAUDE.md` 一类入口文档，避免验收时只做构建与测试而绕过 `check_gate.py`。

## Key Evidence

| # | Conclusion | Evidence | Location |
|---|-----------|----------|----------|
| 1 | WSL Linux GCC 打包链路已成立，后续 warning 收敛可以基于真实 Linux 编译结果进行 | 验证文档已经记录 Linux/macOS 原生 `cmake` 构建口径，并新增了 Windows 宿主下 `WSL Ubuntu + GCC/G++ + CMake + Ninja` 的独立 Linux 构建目录示例 | `docs/ai-workflow/verification.md:24-41` |
| 2 | `LogSettingsLoadingPrewarmEvent` 是当前剩余 warning 中最像低风险卫生项的一类 | 该函数完整定义在 `gameclient.cpp` 顶部，直接读取 perf 配置并写日志 payload；本轮 Linux GCC 构建中它被报为 `defined but not used`，且文件中没有后续调用证据 | `src/game/client/gameclient.cpp:96-110` |
| 3 | `menus_assets_editor.cpp` 的 warning 不应被当成纯 cosmetic 清理 | `AssetsEditorGetCachedImage` 在栈上构造 `CImageInfo Loaded`，再把它 `std::move` 到缓存条目后返回 `&NewEntry.m_Image`；GCC 已经对该函数报出 “may return address of local variable”，说明这里至少需要单独验证对象语义和移动后状态，而不是直接顺手压 warning | `src/game/client/components/menus_assets_editor.cpp:507-525` |
| 4 | `str.cpp` warning 触到的是底层字符串 helper，而不是单一业务文件 | `str_copy` 当前先 `dst[0] = '\0'`，再调用 `strncat(dst, src, dst_size - 1)`；GCC 对这类模式报出 `stringop-overflow`，说明后续若要处理，应从基础字符串 helper 和调用模式角度统一看，不适合夹带在业务补丁中 | `src/base/str.cpp:6-10` |
| 5 | perf 开关与阈值已经有公共 helper，不需要继续在每个文件复制一层本地 wrapper | `QmPerfEnabled()` 和 `QmPerfThresholdMs()` 已集中定义在 `perf_logging.h`，分别统一读取 `m_QmPerfDebug` / `m_QmPerfLogfile` 与 `m_QmPerfDebugThresholdMs` | `src/game/client/components/qmclient/perf_logging.h:14-21` |
| 6 | 入口工作流虽然提到 gate，但没有把“不能只用 build/test 替代 gate”写成明确验收约束 | 根规则当前在“完成任务后”只写到“至少覆盖当前改动的 build/test/gate”，而验证文档则单列了 `quick/default/full` gate 模式；这说明 gate 已存在，但入口表达仍可更硬，以减少只跑构建测试的灰区 | `AGENTS.md:39-49`, `docs/ai-workflow/verification.md:68-80`, `qmclient_scripts/scripts_overview.md:107-145` |

## Details

### 适合以后继续顺手收敛的项

- 未使用的本地 helper 或日志函数，例如 `LogSettingsLoadingPrewarmEvent`
- 纯命名遮蔽、无符号/有符号比较这类不会改变行为的编译器告警
- 公共 perf helper 已存在但局部文件仍残留重复封装的场景

### 不适合混在“源码卫生小补丁”里的项

- 返回局部对象地址、生命周期、移动语义类 warning
- 底层字符串 helper、容器/内存管理模式类 warning
- 编译选项兼容性噪音，例如某些 clang-only 或 GCC-only flag 提示

### 对后续任务拆分的建议边界

如果以后继续处理 Linux GCC warning，建议至少拆成两个任务：

1. `warning-cleanup-low-risk`
   - 目标：清理未使用 helper、局部命名遮蔽、低风险类型告警
   - 验证：Linux `package_default` 或 `game-client` 构建
2. `warning-investigate-real-bugs`
   - 目标：分析 `menus_assets_editor.cpp`、`str.cpp` 等更像真实缺陷/底层问题的 warning
   - 验证：除 Linux 构建外，还需要针对相关模块补测试或更细的定点验证

## Exploration Scope

- Focused directory: `src/game/client/`, `src/base/`, 根工作流文档
- Files involved: `src/game/client/gameclient.cpp`, `src/game/client/components/menus_assets_editor.cpp`, `src/base/str.cpp`, `src/game/client/components/qmclient/perf_logging.h`, `AGENTS.md`, `docs/ai-workflow/verification.md`, `qmclient_scripts/scripts_overview.md`
- Skipped: `menus_assets_editor.cpp` 的完整对象语义和 `str_copy` 全仓库调用面未做深挖；这里只记录“为什么它们不像纯 cosmetic warning”

## Confidence Notes

**confidence: high**

- 剩余 warning 的分类判断直接来自当前 WSL Linux GCC 构建输出与真实源码位置
- 入口工作流与 gate 约束缺口来自根规则文档与验证文档对照，而不是推测
- 尚未深挖的部分已明确标记为后续调查范围，而没有在本报告中假装下结论

## Open Questions

- `AssetsEditorGetCachedImage` 的 warning 是 GCC 误报、移动语义边界问题，还是确实存在返回悬空地址风险？
- `str_copy` 的 `strncat` 模式是否应该整体替换为更直接的 bounded copy 实现？
- 现有 `check_gate.py` 模式里，哪些任务类型最适合默认要求 `quick`，哪些应直接要求 `default`？

## Related Documents

- `2026-06-02-设置页性能量化系统设计.md` — 相关于后续 perf 量化系统继续建设，但本文只聚焦 Linux GCC warning 与 gate 工作流缺口

## Next Steps

如果要继续收敛 Linux GCC warning，先做一个只覆盖低风险卫生项的小补丁；把 `menus_assets_editor.cpp` 和 `str.cpp` 留给单独的调查/修复任务。
