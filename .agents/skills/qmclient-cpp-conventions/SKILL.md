---
name: qmclient-cpp-conventions
description: QmClient C++ 开发的兼容性约束、范围边界、DDNet 风格优先原则、热路径警惕、配置项前缀约定，遵循 docs/ai-workflow/ddnet-development.md
---

# QmClient C++ 开发约定

## 适用场景
在 QmClient 中做 C++ 实现、重构或调试时，遵循 `docs/ai-workflow/ddnet-development.md` 的兼容性和风格约束。核心原则：**DDNet 兼容性优先于泛化的现代 C++ 偏好**。

## 范围边界（改动前必读）

### QmClient 常规范围（可直接改）
- `src/game/client/components/qmclient/`
- `src/game/client/QmUi/`
- QmClient 配置头、翻译、文档、metadata
- `qmclient_scripts/`

### 需明确批准才能动（默认不改）
- 上游引擎核心、服务端玩法、地图编辑器
- 第三方库、CI release 工作流
- **协议字段、物理、预测、snapshot、输入、碰撞、时序、回放语义**
- 根目录 `CMakeLists.txt`、序列化布局、文件格式定义

触碰这些区域时：先指出风险，再开始实现，补丁保持最小范围。

## 配置项约定
- QmClient 配置项统一使用 `qm_` / `Qm` 前缀，**不用 `cl_` 前缀**
- 配置域保持分离（client / server 独立）

## 风格
- 局部变量、方法、类名用大驼峰（UpperCamelCase），`src/base` 等特殊区域除外
- 优先遵循现有 DDNet 风格，而非泛化 C++ 风格
- **不要为了"现代化"重写既有代码**，遵循 DDNet 现有实现模式

## 现代 C++ 使用边界
可用（如和当前模块风格匹配）：`constexpr`、`std::array`、`std::optional`、range-for、`auto`、结构化绑定、`[[nodiscard]]`、`if constexpr`。
避免：原始 `new`/`delete`（除非周围代码本身就是这种方式）、过早抽象、为只用一次的代码引入抽象。

冲突规则：通用现代 C++ 最佳实践（见 context7）与 DDNet 既有风格或兼容性约束冲突时，**优先服从 DDNet 约束**。

## 热路径警惕
DDNet 是实时联网游戏。先判断代码是否跑在：每帧 / 每 tick / 每玩家 / 每实体 / 每 snapshot / 每渲染项 / 文本布局路径上。
- 渲染/tick 路径中的堆分配要特别警惕
- 不要过早优化，但也不要把明显的热路径浪费带进去

## 内存与线程
- 检查悬空指针/引用、缓存生命周期、静态状态初始化顺序
- 不要"以防万一"引入线程/锁/原子变量；碰到音频/图形/HTTP/存储时先识别线程边界

## 错误处理
不要静默忽略文件、网络、解析、配置、控制台、资源或外部数据失败。

## 编码约束
- 使用 UTF-8，保留原 BOM 状态（如有）
- 修改前检测原文件换行符（CRLF/LF）和缩进风格（Tab/空格数），修改后保持一致

## 修改前检查
- 检查附近源码、调用点、配置变量、翻译和测试
- 不理解现状时不要直接写代码
- 改动导出符号前必须跑 `lsp references` 确认所有调用点
