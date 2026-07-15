---
name: qmclient-cpp-conventions
description: QmClient C++ 约定：兼容性边界、DDNet 风格优先、热路径警惕、配置前缀；遵循 ddnet-development。
---

# QmClient C++ 开发约定

## 何时使用
在 QmClient 做 C++ 实现、重构或调试时。权威：`docs/ai-workflow/ddnet-development.md`。  
核心原则：**DDNet 兼容性优先于泛化的现代 C++ 偏好**。

## 范围边界

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

## 配置
- QmClient 配置用 `qm_` / `Qm` 前缀，**不用 `cl_`**
- client / server 配置域分离

## 风格
- 局部变量、方法、类名：大驼峰（`src/base` 等特例除外）  
- 跟现有 DDNet 风格，不为「现代化」重写旧代码  

## 现代 C++ 边界
可用（与周围一致时）：`constexpr`、`std::array`、`std::optional`、range-for、`auto`、结构化绑定、`[[nodiscard]]`、`if constexpr`。  
避免：无必要 `new`/`delete`、过早抽象、一次性代码硬抽接口。  

与通用现代 C++ 建议冲突时，**服从 DDNet 约束**。

## 热路径
先判断是否在：每帧 / 每 tick / 每玩家 / 每实体 / 每 snapshot / 每渲染项 / 文本布局。  
渲染与 tick 路径警惕堆分配；不引入明显浪费，也不过早优化。

## 内存与线程
- 悬空指针/引用、缓存生命周期、静态初始化顺序  
- 不为「以防万一」加线程/锁/原子；音频/图形/HTTP/存储先认清线程边界  

## 错误处理
文件、网络、解析、配置、控制台、资源或外部数据失败，**禁止静默吞掉**。

## 编码
- UTF-8；保留原 BOM  
- 保持原文件换行（CRLF/LF）与缩进（Tab/空格）  

## 改前检查
- 看附近源码、调用点、配置、翻译、测试  
- 不懂现状不要写  
- 改导出符号前必须 `lsp references`  
