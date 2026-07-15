---
name: qmclient-cpp-conventions
description: >
  QmClient / DDNet C++ 开发约定：兼容性边界、DDNet 风格优先、热路径警惕、配置前缀、
  内存/线程边界，以及性能/重构/安全等专项规则路由。修改任何 .cpp/.h、碰引擎/客户端逻辑、
  做性能优化或行为保持型重构时都要用本 skill；不要只凭通用现代 C++ 习惯动手。
---

# QmClient C++ 开发约定

## 何时使用

- 改 `src/**/*.cpp` / `*.h`
- 碰配置变量、组件生命周期、渲染/tick 路径
- 性能优化、jobs/线程、缓存/纹理、安全输入、行为保持重构

权威全文：

| 场景 | 读 |
|------|-----|
| 默认（每次 C++ 改动） | `references/ddnet-development.md` |
| 性能优化 / 长帧 | `references/advanced/performance-workflow.md` |
| 性能量化系统本身 | `references/advanced/perf-system-workflow.md` |
| 行为保持重构 | `references/advanced/refactor-workflow.md` |
| 新特性 / 新配置 | `references/advanced/feature-introduction.md` |
| 下载/文件/外部输入 | `references/advanced/safety-security.md` |
| 缓存/纹理/指针生命周期 | `references/advanced/memory-lifetime.md` |
| jobs / 后台 / GPU | `references/advanced/threading-jobs.md` |
| debug 面板 / 复现包 | `references/advanced/observability-debugging.md` |
| 固定场景 / A/B 回归 | `references/advanced/regression-prevention.md` |
| 专项索引 | `references/advanced/README.md` |

只读与当前任务相关的 reference，不要一次灌进全部 advanced。

## 核心原则

**DDNet 兼容性优先于泛化的现代 C++ 偏好。**

### 默认可改

- `src/game/client/components/qmclient/`
- `src/game/client/QmUi/`
- QmClient 配置头、翻译、文档、metadata
- `qmclient_scripts/`

### 默认不改（需明确批准）

- 上游引擎核心、服务端玩法、地图编辑器
- 第三方库、CI release 工作流
- **协议字段、物理、预测、snapshot、输入、碰撞、时序、回放语义**
- 根目录 `CMakeLists.txt`、序列化布局、文件格式

触碰前先说明风险，补丁尽量小。

## 配置与风格

- QmClient 配置用 `qm_` / `Qm` 前缀，**不用 `cl_`**
- 局部变量、方法、类名：大驼峰（`src/base` 等特例除外）
- 跟现有 DDNet 风格，不为「现代化」重写旧代码

## 现代 C++ 边界

可用（与周围一致时）：`constexpr`、`std::array`、`std::optional`、range-for、`auto`、结构化绑定、`[[nodiscard]]`、`if constexpr`。  
避免：无必要 `new`/`delete`、过早抽象、一次性代码硬抽接口。  
与通用现代 C++ 建议冲突时，**服从 DDNet 约束**（见 `references/ddnet-development.md`）。

## 热路径

先判断是否在：每帧 / 每 tick / 每玩家 / 每实体 / 每 snapshot / 每渲染项 / 文本布局。  
渲染与 tick 路径警惕堆分配；不引入明显浪费，也不过早优化。

## 内存 / 线程 / 错误

- 悬空指针/引用、缓存生命周期、静态初始化顺序
- 不为「以防万一」加线程/锁/原子；音频/图形/HTTP/存储先认清线程边界
- 文件、网络、解析、配置、控制台、资源或外部数据失败，**禁止静默吞掉**

## 改前检查

- 看附近源码、调用点、配置、翻译、测试
- 不懂现状不要写
- 改导出符号前必须 `lsp references`
- 专项风险按上表读 `references/advanced/*`

## 与其他 skill

- 验证 / gate → `qmclient-verification-gate`
- 提交文案 → `qmclient-git-commit`
- 代码审查 → `qmclient-code-review`
