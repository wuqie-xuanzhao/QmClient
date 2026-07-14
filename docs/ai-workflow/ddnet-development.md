# DDNet / QmClient 开发规则

在 QmClient 中做 C++ 实现、重构或调试时，使用这份文档。

## 兼容性优先

DDNet 兼容性优先级高于泛化的现代 C++ 偏好。

没有明确批准时，不要改这些：

- 网络协议字段或布局。
- Demo、皮肤、地图、配置或存档文件格式。
- 物理、碰撞、预测、快照、输入、时序、回放或地图行为。
- 任何会导致现有排名无法达成或现有地图变得更简单的改动。

如果任务触碰到这些区域，先指出风险，再开始实现，并把补丁保持在最小范围。

## 范围边界

QmClient 的常规范围包括：

- `src/game/client/components/qmclient/`
- `src/game/client/QmUi/`
- `src/engine/shared/config_variables_qmclient*.h`
- `src/game/version.h`
- `data/languages/simplified_chinese.txt`
- `docs/info.json`
- `qmclient_scripts/`
- `docs/ai-workflow/`、`docs/superpowers/`、`AGENTS.md` 和其他 agent/harness 文件

没有明确请求时，以下内容都算超范围：

- QmClient 配置以外的上游引擎核心。
- 服务端玩法、地图编辑器内部、协议、物理、碰撞、预测、快照、回放行为。
- `ddnet-libs/` 或 `src/engine/external/` 中的第三方库。
- Release CI workflow 行为。

## 风格

优先遵循现有 DDNet 风格，而不是泛化的 C++ 风格：

- 局部变量、方法和类名使用大驼峰（UpperCamelCase），`src/base` 等特殊区域除外。
- 沿用现有前缀：`m_` 成员、`g_` 全局、`s_` 静态、`p` 指针、`a` 固定数组、`v` 向量、`C` 类、`I` 接口。
- 优先使用语义化命名。短循环变量仅在作用域极小且含义明确时可接受。
- 优先使用 early return 和小而专注的函数，但不要拆分到影响 DDNet 风格可读性的程度。

## 现代 C++

如果和当前模块风格匹配，可以使用：

- `constexpr`
- `enum class`（使用 `E...` 命名，值大写）
- `std::optional`
- `std::variant`
- 移动语义
- `std::array`
- 谨慎限定范围的 `std::string_view`

避免：

- 原始 `new` / `delete`，除非周围代码本身就以这种方式管理对象所有权
- 不必要的宏
- `goto`
- `if` 条件内赋值
- 将整数当作布尔值使用
- 隐藏的所有权转移
- 不必要的堆分配
- 与当前模块风格不匹配的大范围模板或 RAII 重写

## 运行时与热路径

DDNet 是实时联网游戏。先判断代码是不是跑在每帧、每 tick、每玩家、每实体、每个 snapshot、每个渲染项或文本布局路径上。

要特别警惕：

- 渲染/tick 路径中的堆分配
- 重复的字符串构造
- 重复的排序或扫描
- 重复的 `TextWidth` 或布局计算
- 对未变更状态写入配置
- 频繁循环中的序列化/反序列化
- 额外的网络带宽或协议增长

不要过早优化，但也不要把明显的热路径浪费带进去。

## 错误处理与数据边界

- 不要静默忽略文件、网络、解析、配置、控制台、资源或外部数据失败。
- 校验索引、大小、指针和外部输入。
- 对开发者不变量使用 debug assertion，对用户/外部失败使用运行时处理。
- 遵循当前模块既有的错误传播风格，不要大面积改成异常驱动流程。

## 内存与生命周期

重点检查：

- 悬空指针或引用
- 返回局部数据的引用
- 迭代器失效
- 越界访问
- use-after-free 或 double free
- 未初始化读取
- `string_view` 或指针生命周期不匹配

当代码使用缓存、静态或全局状态时，要考虑初始化顺序和线程安全。

## 线程

不要为了“以防万一”就引入线程、锁或原子变量。如果代码碰到音频、图形、HTTP、存储、数据库、日志、平台代码或后台任务，先识别线程边界和共享可变状态。
