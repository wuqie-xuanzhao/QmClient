---
name: audit-qmclient-quality
description: 用功能级与横向精准模板审计 QmClient 当前 diff/模块/规格/发布态；验证玩家场景与失效假设，覆盖 UI、正确性、生命周期、线程 Jobs、性能、安全、兼容、测试与文档漂移，并路由到测试/gate/人工验证。
---

# QmClient 客户端质量审计

## 本轮模式（只选一种主模式）
- **只读审计**（默认）：只报告，不改代码/文档  
- **修复发现**：用户明确要求修时，验证后按 TDD 最小改  
- **自动化加固**：把可确定复现的问题固化为测试/脚本/gate  
- **提交/发布审计**：查 diff、验证证据、跨平台 gap、发布阻断  

未授权实现时，停在证据充分的报告，不扩成全仓重构。

## 加载权威规则
1. 定位仓库根，先读 `AGENTS.md`
2. 读当前有效 spec/plan；带过时 banner 的仅作历史对照
3. 读本 skill 的 [`references/quality-scan-prompt.md`](references/quality-scan-prompt.md)：
   - 每次读第 0、1、4 节（参数、模板合同、阶段、停止条件、finding 合同、自动化边界）
   - 有功能命中时读第 3 节功能模板
   - 再读第 2 节相关横向专项
   - 无功能模板时按第 0 节构造临时功能模板
   - 提交/发布审计额外读第 5 节
4. 改代码 → `qmclient-cpp-conventions`；审查 → `qmclient-code-review`；验证 → `qmclient-verification-gate`
5. 仅在触发对应风险时读 `qmclient-cpp-conventions/references/advanced/*`（见下表），禁止一次全载

## Advanced 路由
| 触发风险 | 必读 |
|----------|------|
| 新功能 / 新配置 / 用户可见行为 | `../qmclient-cpp-conventions/references/advanced/feature-introduction.md` |
| 卡顿 / 长帧 / 性能 | `../qmclient-cpp-conventions/references/advanced/performance-workflow.md` |
| telemetry / perf 系统本身 | `../qmclient-cpp-conventions/references/advanced/perf-system-workflow.md` |
| 缓存 / 纹理 / 指针 / UI element / 异步结果 | `../qmclient-cpp-conventions/references/advanced/memory-lifetime.md` |
| 后台任务 / 取消 / 队列 / 主线程 / GPU context | `../qmclient-cpp-conventions/references/advanced/threading-jobs.md` |
| debug 面板 / 日志 / 复现证据 | `../qmclient-cpp-conventions/references/advanced/observability-debugging.md` |
| 文件 / 下载 / 解压 / 外部输入 / 隐私 | `../qmclient-cpp-conventions/references/advanced/safety-security.md` |
| 行为保持型重构 | `../qmclient-cpp-conventions/references/advanced/refactor-workflow.md` |
| 回归 / 固定场景 / release checklist | `../qmclient-cpp-conventions/references/advanced/regression-prevention.md` |

advanced 路径相对 `qmclient-cpp-conventions/references/advanced/`。与 DDNet 兼容性冲突时优先 DDNet，并记录冲突。

## 强制精准模板
通用扫描只做路由，不能当主体：
1. 先匹配功能级模板（玩家路径 + 功能状态）  
2. 再选横向专项补生命周期 / 线程 / 性能 / 安全 / 兼容 / 验证  
3. 每条失效假设映射到真实符号、owner、调用链、状态与已有测试  
4. 标记：`已确认` / `已排除` / `待运行时验证` / `超出范围`，并留证据  
5. 无模板时自建临时模板，至少覆盖玩家场景、状态矩阵、reset/reconnect、失败回退、平台边界、可自动化部分  

finding 上限只限输出条数，不限假设覆盖。独立模板超过 3 个则拆多轮，禁止一次宽泛扫描糊弄。

## 执行步骤
1. **定范围**：目标、基线（默认当前 diff）、平台、玩家场景、是否可改、finding 上限  
2. **读实现**：先 diff；结构用 CodeGraph；字面/配置键/日志用搜索。不以旧文档定论  
3. **映射模板**：记录选用理由与未选边界  
4. **跑假设**：从玩家可见后果反推代码路径；维护覆盖表  
5. **验证候选**：可达路径、失败条件、现有保护、最小复现；证不了就降为 gap  
6. **输出**：先 findings 再结论；含证据位置、触发、影响、修复方向、测试建议、模板覆盖摘要  
7. **回归闭环**：能测的进 C++ 测试 / gate / 固定人工场景，避免 AI 反复发现同一确定问题  

## 子代理
主代理先路由。仅用户要求并行或规则要求独立审查时，派边界互斥的只读子任务：
- 一任务一模板、明确范围与 finding 合同  
- 禁止子代理再派子代理、禁止反问边界  
- 全部返回后再汇总；未返回不得声称完成  
- 主代理去重、定最终严重级别  

## 自动化落点
| 问题性质 | 首选 |
|----------|------|
| 纯函数 / parser / 状态机 / 生命周期 / 配置迁移 | C++ GoogleTest |
| 文档链接 / 生成物 / 命名 / 版本 / 静态合同 | Python 脚本或 gate |
| 构建 / 打包 / 跨语言合同 | 集成测试或 gate |
| 视觉 / 手感 / 真机 GPU/IME/触控/音频 | 固定场景 + 人工证据 |
| 风险路由 / 未知边界 / 跨模块候选 | AI 审计 |

新增 gate：确定、无副作用、可解释、给修复命令、自带合同测试。gate 不自动改代码、不刷 allowlist、不把 AI 判断当阻断。

## 完成标准
- 列出已执行模板与假设计数（确认 / 排除 / 待运行时）  
- 每个命中模板都追到真实代码路径  
- 区分已验证 finding、待验证假设、未执行项  
- 修复模式：focused 验证 + 仓库要求的全量入口与匹配 gate；未跑记 gap  
- 不以 build 成功代替测试 / gate / 视觉 / 跨平台  
- 不弱化测试迁就实现；不回退他人并行改动  
