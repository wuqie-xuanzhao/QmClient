# DDNet / QmClient 客户端质量审查工作流与提示词

## 0. 使用方法

这份文档不是一次性全仓扫描清单，也不是只提供灵感的参考列表。命中的模板必须真正执行。推荐工作方式：

1. 填写任务参数。
2. 拼接“全局审查约束”和“统一输出合同”。
3. 范围不明确时先运行“路由扫描”。
4. 有匹配功能级模板时优先执行该模板，再选择直接相关的横向专项模板补足风险维度。
5. 没有匹配功能级模板时，按本文合同现场构造一个功能级精准模板，不能退回泛化 code review。
6. 用户确认 findings 后，再规划或修改代码。

### 任务参数

```text
扫描范围：{当前 diff / commit / 指定目录 / 指定功能}
对照基线：{HEAD^ / upstream/master / 当前有效 spec / 无}
目标平台：{Windows / Linux / macOS / Android / 全部}
审查模式：{只读报告 / 报告后经确认修复}
finding 上限：{默认 10}
重点场景：{玩家操作路径、服务器状态、分辨率或数据规模}
```

默认值：

- 未指定范围时，只扫描当前 diff 及其直接调用链。
- 未指定审查模式时，按“只读报告”处理，不修改代码。
- 未指定 finding 上限时，最多输出 10 个已验证问题。
- 旧文档仅作线索；实现依据以当前有效 spec、代码和测试为准。

### 模板执行合同

模板是审查的执行主体，路由扫描和通用规则只负责确定范围、证据标准与模板选择，不能代替模板本身。

- 具体功能命中第 3 节模板时，必须执行所有直接匹配的功能级模板；不得只读标题或推荐入口。
- 第 2 节横向模板用于补充正确性、生命周期、性能、安全、兼容性等风险维度，不能替代功能级玩家场景。
- 不要在阅读 diff 前机械限制模板数量。若命中超过 3 个互相独立的模板，拆成多轮审查并分别收口。
- 每个模板中的每条失效假设都必须映射到真实入口、owner、调用链或状态转换，并得出明确结果。
- 没有现成功能模板时，必须根据玩家操作路径、功能状态、生命周期事件、平台/数据规模和实际符号，构造本轮临时功能模板。

执行时维护内部覆盖表：

| 模板               | 失效假设         | 真实代码路径            | 结果                                      | 证据或验证                     |
| ------------------ | ---------------- | ----------------------- | ----------------------------------------- | ------------------------------ |
| 功能级或横向模板名 | 被验证的具体假设 | 入口、owner、关键调用链 | 已确认 / 已排除 / 待运行时验证 / 超出范围 | 文件行号、测试、日志或实测需求 |

最终报告不必展开所有已排除项，但必须列出：

- 实际执行的模板；
- 已验证、已排除和待运行时验证的假设数量；
- 因范围或环境未执行的模板与原因。

只有完成模板假设映射和验证，才能声称该模板已审查。只做通用静态扫描、只列检查清单或只复述模板内容，不算执行。

### 全局审查约束

```text
你正在审查 DDNet / QmClient。先读取当前有效的 spec/plan、实际代码、直接调用方和相关测试，再形成结论。

硬约束：
- 优先检查正确性、未定义行为、生命周期、线程安全、热路径性能和玩家可见回退。
- 不因现代 C++ 偏好重写 DDNet 既有实现。
- 未经明确批准，不建议改变协议、Demo/skin/map 格式、物理、预测、碰撞、输入、时序、回放或 rank 可达性。
- 区分 QmClient/TClient 定制与上游 DDNet 代码，不把上游遗产自动列为本轮整改范围。
- 每个 finding 必须有当前代码证据、明确触发条件和具体玩家影响。
- 无法从静态代码确认的问题放入“待运行时验证”，不伪装成确定缺陷。
- 修复建议保持最小，不顺手重构无关代码。
- build、focused test、全量测试和 gate 是不同证据，不得互相替代。
- 审查模式为“只读报告”时，不修改文件、配置、Git 状态或外部系统。
```

### 执行阶段

```text
阶段 1：探索
- 定位入口、调用链、owner、状态边界和已有测试。
- 只收集候选风险，不急于下结论。

阶段 2：验证
- 为每个候选风险寻找真实触发路径。
- 排除仅属于风格偏好、历史兼容需求或无玩家影响的误报。

阶段 3：报告
- 只输出通过验证的 findings，按严重程度排序。
- 其余不确定项进入“待运行时验证”。

阶段 4：后续
- 仅在用户确认后进入修复计划或代码修改。
```

### 停止条件

- 已覆盖用户指定范围及直接影响面。
- 达到 finding 上限时，保留高严重度和高置信度问题。
- 没有确定缺陷时，明确写“未发现确定缺陷”，不要补凑数量。
- 视觉、网络抖动、音频设备和跨平台结论缺少实测时，必须留下验证 gap。
- 不为提高覆盖率扩展到与当前任务无关的上游模块。

### 统一输出合同

```text
先列 findings，再给总结。

[严重/重要/轻微] 标题
文件:行号或符号
触发条件：
问题：
玩家影响：
代码证据：
最小建议：
验证方式：

最后补充：
- 总体结论：正确 / 需要修复 / 不安全
- 待运行时验证
- 已运行验证与结果
- 未运行验证及剩余 gap
```

## 1. 路由扫描

范围较大或类别不明确时，先运行这个模板。路由扫描只选择后续专项，不尝试完成全仓审计。

```text
结合全局审查约束，快速扫描指定范围，判断风险主要落在哪些领域：
UI/HUD/文本布局；交互帧、预热与调度；缓存、资源与生命周期；Jobs 与线程；
性能观测；i18n；皮肤与 Assets；HUD 通知与聊天；Server Browser 与网络状态；
Demo/Ghost；语音与音频；外部输入安全；上游同步；预测与快照；测试；文档漂移；跨平台发布。

最多选择 3 个高风险领域。每个领域给出：
1. 为什么与当前范围相关；
2. 关键代码入口；
3. 已看到的风险信号；
4. 推荐使用的专项模板。

不要在路由阶段输出未经验证的正式 findings。
```

### 1.1 子代理调度

不要按专项模板数量逐个派发子代理。当前 diff 通常不需要子代理或只需要 1～2 个；只有范围覆盖至少 3 个互相独立的模块时，才考虑并行。

硬约束：

- 主代理先读 diff/spec 和调用链，再决定是否派发。
- 最多同时派发 3 个子代理，为主代理保留一个并发槽位。
- 一个子代理只负责一个有明确路径边界的风险域。
- 子代理不得继续派发子代理，不得询问边界，不得修改代码。
- 子代理返回的是候选 findings，不是最终结论。
- 主代理必须等待所有子代理返回，不能提前宣布完成。
- 主代理负责去重、验证触发路径、处理冲突并剔除误报。

```text
扫描范围：{diff / commit / 目录 / 功能}
对照基线：{基线}
审查模式：只读
最终 finding 上限：{默认 10}

先由主代理读取当前有效文档、实际 diff、直接调用链和已有测试，执行风险路由。

只有识别出 2～3 个互相独立的高风险域时才派发子代理。每个子代理任务必须写清：
- 精确目录、文件或符号范围；
- 需要回答的 2～4 个问题；
- 明确禁止扩展的区域；
- 最多 5 个候选 findings；
- 每项必须包含文件行号、触发路径和玩家影响；
- 没有确定问题时明确报告“未发现确定问题”。

子代理不得修改代码、询问边界或派发下级代理。

等待所有子代理返回后，主代理必须：
1. 合并并去重候选问题；
2. 重新核对代码证据和真实触发条件；
3. 剔除风格意见、无玩家影响问题和重复项；
4. 按统一输出合同给出最终 findings；
5. 单列待运行时验证；
6. 在用户确认前不修改代码。
```

## 2. 专项扫描模板

使用专项模板时，在前面拼接“任务参数”和“全局审查约束”，在后面拼接“统一输出合同”。

### 2.1 UI、HUD 与文本布局

推荐入口：`src/game/client/components/menus*.cpp`、HUD 组件、`src/game/client/QmUi/`、文本渲染和 UI 测试。

```text
专项扫描 UI、HUD、菜单与文本布局：
- 4:3、16:9、21:9、超宽屏、720p、4K、高 DPI 和非整数 UI scale；
- 中英日韩、长翻译、超长昵称/Clan/服务器名下的截断、重叠和错误省略；
- TextWidth、换行、基线、图标字体与最终渲染是否一致；
- 卡片、列表、下拉框、弹窗和 tooltip 的裁剪、越界与滚动跟随；
- 嵌套滚动的 wheel owner、拖拽取消、焦点和命中区域；
- 单列/双列响应式顺序、卡片测量高度、搜索跳转和目标 reveal；
- 低帧率、首次 glyph、资源预热和窗口动态变化时的布局稳定性。
```

### 2.2 交互帧、预热与调度

推荐入口：菜单页面、`settings_*`、`CSectionLoader`、frame scheduler、Demo metadata 调度和性能测试。

```text
专项扫描打开页面、切 tab、滚动、筛选和首次显示时的交互帧：
- prewarm、render-only 和正常渲染是否错误共享业务写入；
- placeholder 与真实内容高度是否一致；
- 不可见 section/列表行是否仍处理，dirty 是否过宽或过窄；
- scheduler token、backlog、consumer 和 stop reason 是否真实生效；
- decode/upload/merge/publish 是否在切页帧集中 drain；
- 预热结果是否过期、污染状态或让首帧与稳定帧行为不同；
- 是否有同路径 p50/p95/p99、max、spike count 和 work_drain 证据。

不要把 FBO 当作默认路线；先归因，再建议不做、少做、分帧做或异步做。
```

### 2.3 缓存、资源与生命周期

推荐入口：`src/game/client/QmUi/`、QmClient 组件、Assets 设置、纹理/文本/预览缓存和测试。

```text
专项扫描缓存、纹理、文本容器、UI element、预览资源和跨帧状态：
- 裸指针、引用、string_view 和 vector 元素地址是否跨帧失效；
- cache key 是否包含语言、UI scale、renderer、资源版本和相关配置；
- hit/miss/disabled/corrupt/evicted/restore 是否都有安全 fallback；
- 切页、切服、切图、语言变化和图形设备重建后的失效与释放；
- 当前可见资源是否被错误驱逐，内存/显存是否 double count；
- 注册表、回调、纹理句柄和 text container 是否成对解除；
- 命中率是否真的代表减少工作，而不是统计口径自循环。
```

### 2.4 Jobs、线程与主线程发布

推荐入口：资源 jobs、皮肤加载、异步设备采样、HTTP/翻译/语音模块和 jobs/thread 测试。

```text
画出 request -> worker -> result queue -> main-thread publish -> consumer 的真实路径：
- GPU 上传、UI state 和渲染对象修改是否只在主线程；
- request key、去重、优先级和取消条件是否完整；
- 旧 job 是否覆盖新请求，发布前是否检查 generation/version/owner；
- 锁是否进入渲染或音频热路径，是否有竞态、死锁或锁内重活；
- drain 是否受 count/bytes/duration 预算约束；
- 队列满、失败、取消和过期结果是否保留 live/source fallback。

不要把“加锁”当作默认修复；必须说明共享状态 owner 和触发时序。
```

### 2.5 性能量化与观测真实性

推荐入口：`src/game/client/components/qmclient/monitoring/`、`perf_logging.h`、`qmclient_scripts/perf/` 和监控合同测试。

```text
把性能量化系统作为生产诊断系统审查：
- event/page/tab/dur_ms/count/bytes/stop 的单位和缺省语义；
- page_switch 是否被错误计入耗时归因；
- 空数据、缺字段和畸形行是否被伪装成 0、100% 或优秀结论；
- p95/p99、样本标准差、阈值、样本量和采样偏差是否正确；
- KPI、文字结论、图表和 verdict 是否共享阈值；
- 未命中日志阈值时是否仍构造昂贵 payload；
- 自动基线是否被误写为严格 A/B 回归；
- C++ 与 TypeScript 字段是否有跨语言合同测试。
```

### 2.6 i18n 与文本生成链

推荐入口：`qmclient_scripts/languages_qmclient/`、维护源 TOML、draft、`data/languages/` 和本轮本地化调用点。

```text
专项扫描源码 key -> active key -> TOML -> draft -> 生成产物 -> 运行时加载：
- Localize、Localizable 和 Register help 文本是否正确提取；
- UI 文案、业务数据、服务器透传和玩家输入是否正确分类；
- 占位符、%%、换行、转义和 UTF-8 合同是否保持；
- active key、维护源、draft 和生成产物是否漂移；
- 生成顺序、重复项、unused 和历史译法是否可审计；
- 语言切换后文本缓存和布局是否失效；
- 英文 fallback 和缺翻译状态是否可见；
- 脚本是否重写、重排或污染未审核条目。
```

### 2.7 皮肤、Assets 与预览管线

推荐入口：skins、`menus_settings_assets.cpp`、`settings_resource_preview.*`、资源注册表、迁移和预览测试。

```text
专项扫描文件发现、解析、decode、GPU upload、注册、预览和游戏内应用：
- 排序/筛选是否每帧重建，是否只处理 visible range；
- preview scale、长宽比、裁剪、颜色和动画是否与实战一致；
- source/live/cache 路径是否产生不同结果；
- 同名资源、local-only、缺 metadata、删除和热重载；
- 页面切换或资源版本变化后旧 job 是否错误发布；
- 大图、批量资源和低显存环境的预算；
- 当前选择、默认资源和迁移是否丢失配置。
```

### 2.8 HUD 通知、聊天与外部文本

推荐入口：`hud_notifications/`、聊天组件、好友通知、广播、规则目录和相关测试。

```text
专项扫描通知分类、规则优先级、触发、去重、布局和生命周期：
- must_i18n、business_data、服务器透传和玩家输入是否混淆；
- alias、白名单、黑名单和静态规则是否冲突；
- 同一 snapshot/event 是否重复触发，切服后是否残留；
- 超长 UTF-8、换行、控制字符和恶意文本；
- 首次出现新 glyph 是否同步建字形导致长帧；
- 通知堆叠、过期、动画、声音和聊天可见性是否一致；
- 上游静态规则与 QmClient 规则是否在同步后漂移。
```

### 2.9 Server Browser 与网络状态

推荐入口：`src/engine/client/serverbrowser*`、菜单浏览器、ping cache、HTTP master、monitoring 和 TClient statusbar。

```text
专项扫描服务器列表、ping cache、HTTP 数据、筛选排序和网络状态展示：
- 刷新、取消、超时、重试和旧响应覆盖新请求；
- HTTP/LAN/ping cache 的合并与去重；
- 外部字段长度、UTF-8 和畸形响应；
- 大列表更新下的长帧、选择跳动和滚动稳定性；
- 断网、DNS 失败、空列表和部分成功状态；
- snapshot/prediction latency、jitter、packet loss 和速率的来源与单位；
- 本地化标签和极端数值下的状态栏宽度；
- 监控采样是否干扰网络或渲染线程。
```

### 2.10 Demo、Ghost 与回放兼容

推荐入口：Demo、Ghost、`menus_demo.cpp`、`race_demo.*` 和 snapshot 测试。

```text
专项保护历史格式和确定性回放语义：
- 录制停止、切图、断线和异常退出时文件关闭；
- 旧版本与损坏 demo/ghost 的读取和安全失败；
- snapshot 翻译、tick、seek、pause、speed 和插值；
- 回放模式是否执行在线模式副作用；
- metadata/date 是否按预算读取；
- 重命名、删除、目录切换后选择索引是否失效；
- 路径、扩展名和超长文件名校验；
- 冲突解决是否削弱历史兼容测试。
```

### 2.11 语音与音频管线

推荐入口：`src/game/client/components/qmclient/voice/`、sound、地图声音、通知声音和 voice 测试。

```text
专项扫描语音捕获、缓冲、发送、播放和普通音频生命周期：
- callback、worker、网络和播放线程的 owner；
- 无设备、权限拒绝、热插拔和重初始化；
- ring buffer 溢出/欠载、格式转换和延迟；
- 静音、按键说话、失焦、切服和销毁后是否继续采集；
- 实时回调中是否有锁、分配、日志和格式化；
- 丢包、乱序、抖动和长时间无数据后的恢复；
- 音量与设备状态是否在模块间污染；
- 异常路径是否存在隐私风险。
```

### 2.12 更新、下载与外部输入安全

推荐入口：`update_version.*`、updater、HTTP、翻译/歌词源、脚本、文件导入导出和 storage。

```text
画出不可信输入从 source 到 parser、storage 和 runtime consumer 的路径：
- URL、重定向、TLS、版本、哈希/签名和来源；
- 临时文件、原子替换、失败回滚和文件占用；
- 文件名、相对路径、归档路径、符号链接和 storage root；
- 响应大小、超时、重试、压缩炸弹和畸形格式；
- API key、token、用户路径和聊天内容是否进入日志；
- 外部命令参数和 shell 转义；
- 取消、关机和断网后是否留下半成品；
- 离线和失败状态是否有可恢复 fallback。
```

### 2.13 上游同步与 QmClient 边界

推荐入口：目标 commit/diff、QmClient/QmUi/TClient 组件、配置、翻译、测试和同步文档。

```text
专项检查上游修复是否完整落地，同时保留 QmClient/TClient 定制：
- commit 父关系、merge commit 内实际补丁和依赖顺序；
- 上游调用链、测试意图及格式/协议边界；
- 冲突解决是否只做到编译通过；
- 本地配置、菜单、翻译、监控和测试是否被绕过；
- 是否重复移植本地已覆盖的修改；
- Cargo.lock、生成文件和子模块是否由正确工具更新；
- 官方 tag/版本是否真实存在；
- 测试是否被削弱以适应冲突结果。

逐项分类：直接移植、需适配、已覆盖、应跳过、需人工决定。
```

### 2.14 预测、快照与玩法兼容

推荐入口：`src/game/client/prediction/`、snapshot 翻译、实体、输入和对应测试。

```text
默认只报告风险，不授权修改核心玩法语义：
- client/predicted/snapshot tick 与 render time 是否混用；
- entity id、owner、copy/destroy 和 world iteration；
- projectile、laser、hook、dragger、door、tele、speedup、switch 和 tune；
- dummy、spectator、demo playback、六版协议和高延迟路径；
- 浮点/整数转换、迭代顺序和平台确定性；
- 输入计数、prediction reset 和 reconnect；
- 防御性校验是否拒绝合法旧地图/demo/snapshot；
- 测试是否表达对地图完成和玩家操作的真实影响。

分类：确定行为变化、潜在兼容风险、仅健壮性改进。
```

### 2.15 测试有效性与回归防护

推荐入口：当前 diff、`src/test/`、gate 和有效 spec/plan 的验收合同。

```text
专项判断测试是否保护玩家可见意图：
- 目标行为回归时测试是否真的失败；
- 结构测试与运行时测试的职责；
- 精确源码字符串断言是否过度脆弱；
- 修改测试后是否重建 testrunner；
- focused test 后是否补全量测试；
- merge 是否削弱父分支断言；
- 空数据、最大数据、非法索引、取消、重连和平台差异；
- 性能测试是否有固定场景、基线和样本可信度；
- build/test/quick/default/full gate 是否被正确表述。

分类：缺失测试、脆弱测试、错误测试、验证证据缺口。
```

### 2.16 文档、规格与实现漂移

推荐入口：当前有效 specs/plans、`docs/ai-workflow/`、实际代码和测试。

```text
先按日期、status、过时 banner 和 supersedes 关系确定权威文档：
- 文档声称未实现但代码已完成，或声称完成但仅有局部实现；
- 文件、函数、配置、命令和构建目录是否漂移；
- 多份有效文档是否决策冲突；
- build、focused test、gate 和全量回归是否混写；
- 视觉、跨平台、性能和已知 gap 是否明确；
- 实施步骤是否依赖未记录历史；
- 文档入口变化是否同步治理检查；
- 旧文档是否双向标记替代关系。

分类：当前有效、部分过时、已实现待回填、缺失实现、互相冲突。优先维护一个权威文档。
```

### 2.17 跨平台构建与发布

推荐入口：CMake、Rust bridge、平台代码、workflows、打包脚本、版本文件和 release note 工具。

```text
专项扫描 Windows、Linux、macOS、Android 和不同 renderer 的合并/发布风险：
- MSVC、Clang、GCC 的类型、warning、include 和链接差异；
- 32/64 位、结构布局、路径、文件锁和大小写敏感；
- OpenGL/Vulkan、HiDPI、窗口和图形设备重建；
- Rust/C++ bridge、Cargo.lock、绑定和 features；
- 子模块初始化和共享 build 目录串行约束；
- package_default、运行时资源、语言文件和 metadata；
- 版本、tag、docs/info.json、release note 和 workflow；
- 已验证平台与仅由静态检查推断的平台。

结论限定为：可提交、可合并但有平台 gap、不可发布。
```

## 3. 功能级精准模板

功能级模板围绕明确玩家场景和失效假设设计。它们比横向专项更适合重复审查具体功能，也更容易把稳定发现沉淀成 C++ 测试。

### 3.1 Gores、自动切锤与快速输入

推荐入口：`CGameClient` 的 fast-input 路径、`controls.cpp`、TClient/Gores 状态、QmClient Gores 设置和相关测试。

```text
玩家场景：进入 Gores 模式，在 main/dummy 间切换，拾取武器、死亡、切图、断线重连，并切换自动武器与快速输入配置。

验证以下假设：
- 自动启用/关闭没有在离开 Gores、切图或断线时恢复原状态；
- main 与 dummy 的输入、weapon、fire counter 或 wanted weapon 被交叉污染；
- 自动切锤与快速输入同时生效时产生重复 fire/hook 边沿；
- 配置中途关闭后仍保留旧 action 或预测偏移；
- spectator/demo/offline 路径仍执行在线输入副作用；
- 高频 tick 路径重复查找配置、绑定或构造状态。

C++ 优先覆盖：模式状态机、输入合并、fire/hook 边沿、配置组合、reset/reconnect、main/dummy 参数矩阵。
游戏内验证：高延迟下手感、真实地图可达性和视觉引导线。
```

### 3.2 武器轨迹、激光、钩索与绘制顺序

推荐入口：`weapon_trajectory.*`、players/items/effects 渲染、激光与钩索预览、Appearance 设置和菜单分支测试。

```text
玩家场景：使用 grenade/laser/shotgun/hook，切换预测、观察者、dummy、透明度和轨迹显示模式。

验证以下假设：
- prev/current character、owner id 或预测 tick 使用错误；
- 轨迹计算与 DDNet 实际武器/调参语义不一致；
- invalid owner、spectator、demo 或非本地玩家错误显示；
- alpha、颜色、线宽、端点圆角和 weapon body 绘制顺序不一致；
- clip/map screen/texture state 未恢复，污染后续渲染；
- 极端 zoom、纵横比和边界坐标导致 NaN、越界或大循环。

C++ 优先覆盖：显示 gating、owner/scope、轨迹采样边界、绘制调用顺序、配置 clamp、无效 snapshot。
游戏内验证：真实轨迹吻合、端点遮挡、不同 renderer 的线宽与抗锯齿。
```

### 3.3 QmLyrics、媒体时钟与歌词源

推荐入口：`src/game/client/components/qmclient/qm_lyrics/` 与 `src/test/qm_lyrics*_test.cpp`。

```text
玩家场景：播放、暂停、seek、切歌、无 metadata、离线和歌词源超时；加载 LRC、TTML 与多来源结果。

验证以下假设：
- media clock 在 pause/seek/rate change 后漂移；
- 同时间戳、多行、空行、负 offset 和超长歌词解析错误；
- 旧请求在切歌后覆盖新歌词；
- cache key 未包含歌曲身份、source、版本或解析选项；
- 多来源匹配排序不稳定或错误选择低置信度结果；
- 网络失败时覆盖已有可用歌词；
- karaoke 分段与普通行渲染在边界时间跳动。

C++ 优先覆盖：parser、clock、match/ranking、cache key、source fallback、请求 generation、渲染时间片选择。
人工验证：字体排版、卡拉 OK 动画节奏和真实媒体播放器集成。
```

### 3.4 QmLive、观察者与比赛回放

推荐入口：`src/game/client/gameclient.cpp` 中 LiveObserver/QmLive 状态、sidecar、director、overlay input 和 `qm_live_client_test.cpp`。

```text
玩家场景：在线 Live Observer、普通 observer、QmLive demo 和正常游戏之间切换；选择队伍、自由视角、手动跟随、seek 和回放结束。

验证以下假设：
- presentation mode 切换未完整保存/恢复 observer 状态；
- sidecar 缺失、损坏或不匹配时仍污染普通 demo；
- seek 回退后 team/finish event 只增量应用，没有重建；
- follow client 离线或换队后索引失效；
- manual follow、director 自动选择和 team filter 相互覆盖；
- overlay 抢占聊天、鼠标模式或普通 spectator 输入；
- 录制停止、异常退出或切图留下不完整 sidecar。

C++ 优先覆盖：presentation 状态机、save/restore/reset、sidecar 校验、seek 重建、team selection、输入 gating。
游戏内验证：完整比赛流程、复杂队伍变化、鼠标与聊天体验。
```

### 3.5 脚本系统与命令边界

推荐入口：`scripting.*`、`scripting/impl.*`、console/config/storage 接口和外部输入安全专项。

```text
玩家场景：加载、执行、重载、禁用和删除脚本；脚本报错、超时或访问已经销毁的客户端状态。

验证以下假设：
- 脚本路径或 include 可越过 storage root；
- 脚本在客户端未初始化、切服或销毁后持有失效接口；
- 单个脚本异常影响其他脚本或主循环；
- 回调未注销，重载后重复触发；
- 命令参数、配置或聊天数据未经边界处理；
- 每帧脚本无预算，造成长帧；
- 权限边界无法区分只读查询和有副作用操作。

C++ 优先覆盖：路径校验、注册/注销、生命周期、错误隔离、命令参数和预算决策。
运行时验证：真实解释器行为、恶意脚本、长时间运行与资源消耗。
```

### 3.6 IME、文本输入与剪贴板

推荐入口：IME 平台实现、`lineinput.*`、UI 输入框、clipboard 调用和 `qm_ime_platform_test.cpp`。

```text
玩家场景：中文 composition、候选翻页、提交/取消、窗口失焦、切换输入框、复制粘贴长 UTF-8 文本。

验证以下假设：
- UTF-16/UTF-8 offset、candidate count 或 page size 越界；
- composition 在失焦、关闭菜单或切换输入框后残留；
- 系统候选窗与自绘候选窗同时显示；
- selection/cursor 使用字节索引和 codepoint 索引混算；
- clipboard 空值、超长文本、换行和非法 UTF-8 未处理；
- password/secret 输入被复制、记录或显示；
- 平台不支持路径错误退化为无输入。

C++ 优先覆盖：编码转换、offset clamp、candidate state、focus lifecycle、paste sanitization 和平台策略选择。
人工验证：Windows/macOS/Linux 原生 IME、系统候选窗位置和剪贴板集成。
```

### 3.7 Android、触控与软键盘

推荐入口：`touch_controls.*`、`menus_ingame_touch_controls.*`、输入系统和移动端平台代码。

```text
玩家场景：多指移动/瞄准/开火/钩索，打开编辑器移动按钮，旋转屏幕，呼出软键盘，应用或取消未保存设置。

验证以下假设：
- finger id 复用、抬起或系统取消后 action 卡住；
- 多指同时操作时同一 action owner 被覆盖；
- safe area、纵横比和旋转后按钮越界或重叠；
- editor 的 cache/save/reset 与真实按钮指针失效；
- 切菜单、失焦和暂停后输入状态未清理；
- 软键盘遮挡输入框或改变 viewport 后布局未重算；
- 不可见按钮仍接收触摸。

C++ 优先覆盖：finger/action 状态机、几何 clamp、重叠检测、缓存应用/取消、visibility 和 reset。
设备验证：真实多点触控、软键盘、安全区域、旋转和不同刷新率。
```

### 3.8 图形后端、Shader 与设备重建

推荐入口：`src/engine/client/backend/`、`graphics_threaded.*`、shader 资源、Vulkan/OpenGL 初始化和恢复测试。

```text
玩家场景：首次启动、切 renderer、切全屏/窗口、调整分辨率、设备丢失、驱动不支持和安全启动回退。

验证以下假设：
- shader 缺失、损坏或版本不匹配时错误继续初始化；
- pipeline/shader cache key 与设备、驱动或渲染状态不匹配；
- swapchain/窗口重建遗漏纹理、buffer、clip 或 screen state；
- backend fallback 保存了不可再次启动的配置；
- size/format 计算溢出导致分配错误；
- render thread 与主线程之间资源销毁顺序错误；
- OpenGL 与 Vulkan 路径产生不同玩家可见语义。

C++ 优先覆盖：格式/尺寸计算、配置 fallback、能力选择、错误分类和纯 cache-key 逻辑。
运行时验证：真实 GPU/驱动、设备重建、shader/pipeline 创建和画面一致性。
```

### 3.9 崩溃、卡死与 Debug Bundle

推荐入口：client hang report、日志、monitoring snapshot、debug bundle、storage 和 observability 文档。

```text
玩家场景：主线程长时间无响应、后台线程死锁、崩溃、磁盘满、报告写入失败和用户提交诊断包。

验证以下假设：
- watchdog 把正常长加载误判为 hang；
- dump/report 路径递归触发分配、锁或崩溃；
- 多线程同时写报告导致损坏；
- API key、服务器密码、聊天、玩家路径或个人信息未脱敏；
- 磁盘满/无权限时覆盖原有日志或再次卡死；
- 报告缺少版本、commit、平台、renderer、操作和最近事件；
- debug bundle 收集无边界文件或体积无限增长。

C++ 优先覆盖：路径、字段合同、脱敏、大小限制、fallback 和纯 watchdog 判定。
运行时验证：真实 hang/dump、崩溃处理器、磁盘/权限错误和操作系统限制。
```

### 3.10 配置持久化、默认值与迁移

推荐入口：QmClient 配置头、console chains、`settings_runtime_cache.*`、版本迁移、设置 UI 和配置测试。

```text
玩家场景：旧版本升级、非法手改配置、运行时切换、重启、恢复默认和配置文件只读。

验证以下假设：
- 默认值变化无迁移，意外启用新功能；
- UI clamp 只修显示，没有修运行时消费；
- config chain 在接口未初始化时执行副作用；
- qclient 配置错误使用 cl_ 前缀或与上游变量冲突；
- runtime cache 与持久配置版本/renderer/UI scale 不匹配；
- 多处 owner 写同一配置，保存顺序导致回退；
- 非法枚举、负值、极端数值进入索引、分配或渲染。

C++ 优先覆盖：默认值、迁移表、clamp、枚举 canonicalize、chain 初始化阶段、runtime cache key 和序列化往返。
人工验证：真实旧配置升级、只读文件和跨版本回退。
```

### 3.11 Main、Dummy、Spectator 与 Demo 多状态

推荐入口：`gameclient.cpp`、controls、spectator、HUD/QmClient 功能 gating、Demo playback 和 `qm_modes_test.cpp`。

```text
对同一功能建立状态矩阵：offline、online main、online dummy、spectator、demo playback、QmLive demo。

验证以下假设：
- 读取 local character 时未区分 main/dummy 或不存在的 local id；
- spectator/demo 路径错误发送网络、输入或配置副作用；
- main/dummy 切换后缓存、HUD、skin、weapon 或 follow target 仍引用旧对象；
- scope gating 只隐藏 UI，没有阻止业务行为；
- reset/reconnect 没有清空模式特有状态；
- preview 使用真实游戏状态并污染运行时；
- 同一测试只覆盖 online main，遗漏其他模式。

C++ 优先覆盖：状态矩阵、scope predicate、无 local player、切换/reset、side-effect gating 和 fallback。
游戏内验证：真实 dummy 操作、观察者切换和 demo seek。
```

## 4. AI 适用性与自动化测试

### 4.1 AI 是否适合

AI 适合重复执行，但不应成为最终判定器。最有效的职责是：

- 根据 diff 和调用链选择风险域；
- 找候选边界、遗漏状态和测试缺口；
- 对照 spec、代码、测试和上游行为；
- 生成最小测试场景；
- 审查新变更是否重新打开已知风险。

AI 不适合独立证明：

- 视觉是否专业、动画手感是否自然；
- GPU/驱动、原生 IME、麦克风和真实触控设备行为；
- 网络抖动下的主观操作体验；
- 性能是否改善而没有同场景量化数据；
- 协议、物理和地图兼容是否保持而没有官方基线。

### 4.2 一次性分析还是重复使用

| 工作                              | AI 使用方式       | 是否重复         | 应否固化                  |
| --------------------------------- | ----------------- | ---------------- | ------------------------- |
| 首次模块探索、调用链和 owner 梳理 | AI 很合适         | 模块大改后重跑   | 结论写入有效文档          |
| 当前 diff 风险路由                | AI 很合适         | 每个非简单 diff  | 不必单独固化              |
| 候选边界与异常状态生成            | AI 合适           | 功能变化后重跑   | 稳定边界转测试            |
| 已验证 bug 的回归检查             | AI 仅辅助         | 不应依赖 AI 重查 | 必须转自动化测试          |
| 视觉、手感和设备兼容              | AI 只能辅助列清单 | 每次相关改动     | 固定场景 + 人工/设备测试  |
| 性能归因                          | AI 可分析日志     | 同路径有新样本时 | perf 场景、阈值和报告合同 |
| 文档/实现漂移                     | AI 合适           | 里程碑或计划收口 | 能机械判断的部分进入 gate |

原则：第一次用 AI 找问题，第二次用测试防问题。已经验证且可确定复现的问题，不应长期靠 AI 每次重新发现。

### 4.3 C++ 可以稳定覆盖的内容

- 纯函数：clamp、排序、匹配、路由、cache key、预算和几何计算。
- parser/formatter：LRC、TTML、sidecar、配置、版本、网络指标和 UTF 转换。
- 状态机：prewarm/read-only、Live presentation、IME candidate、touch finger、Gores、main/dummy/spectator/demo。
- 生命周期决策：generation、取消、发布条件、reset、cache invalidation。
- 索引与边界：空列表、最大列表、失效 selection、非法 client id、畸形 count。
- 调用顺序合同：先更新状态再渲染、weapon body 与 laser endpoint 顺序、主线程发布前验证。
- fake-interface 集成：模拟 storage、HTTP、input、client state、clock 和 job result。
- 配置合同：默认值、旧值迁移、canonicalize、序列化往返和 chain 初始化时机。

优先把逻辑提取为不依赖真实 GPU、窗口或网络的窄 helper，再用 GoogleTest 覆盖。不要为了测试大规模重构。

### 4.4 C++ 只能部分覆盖的内容

| 领域      | C++ 可覆盖                                 | 仍需其他验证                     |
| --------- | ------------------------------------------ | -------------------------------- |
| UI 布局   | 几何、测量、裁剪、滚轮 owner、focus/reveal | 截图和多分辨率视觉检查           |
| 渲染      | gating、参数、调用顺序、状态恢复约定       | OpenGL/Vulkan 实际像素与驱动     |
| 性能      | 预算、采样、聚合和日志合同                 | 固定场景 p95/p99 实测            |
| 网络      | parser、超时状态机、旧响应丢弃             | 真实延迟、丢包、重连             |
| 音频      | buffer、状态机、格式和生命周期             | 设备、驱动、实时延迟和听感       |
| IME/触控  | offset、状态机、几何和 reset               | 原生候选窗、软键盘和真机多点触控 |
| Demo/预测 | 格式解析、tick 边界和状态转换              | 官方回放、地图和可达性对照       |

### 4.5 不应滥用的测试

- 源码字符串测试只适合迁移期保护接线和禁用旧路径，不能替代行为测试。
- screenshot/golden 不能替代交互、输入和生命周期验证。
- focused test 只用于开发反馈，提交前仍需对应全量入口。
- build 通过不能证明运行时、视觉、性能或兼容性正确。
- flaky timing test 不应用宽松阈值隐藏不确定性，应改成可控 clock 或确定性调度。
- AI 生成的测试必须先验证：目标行为回归时它确实会失败。

### 4.6 自动化提升规则

发现问题后按以下顺序处理：

1. AI 给出候选问题和最小复现。
2. 主代理或开发者验证问题真实存在。
3. 能确定复现的，先写失败的 C++/脚本测试。
4. 不能在单测复现但可自动运行的，加入集成测试、固定 perf 场景或 gate。
5. 依赖视觉/设备的，建立明确人工 checklist，并保留截图或日志证据。
6. 修复完成后运行 focused test、对应全量测试和匹配的 gate。
7. 后续 AI 审查只检查新增风险和测试无法覆盖的 gap，不重复替代已有测试。

## 5. 提交前综合审查

这个模板只用于范围明确、实现完成的变更，不代替专项审查。

```text
结合全局审查约束，对当前 diff 做提交前综合审查：
1. 行为是否满足当前有效 spec/plan；
2. 是否存在正确性、生命周期、线程安全或兼容性 finding；
3. 是否引入热路径或交互帧性能回退；
4. 配置、翻译、生成产物和文档是否同步；
5. 测试是否验证意图，并区分 focused、全量和 gate；
6. 是否混入无关文件或用户并行修改；
7. 剩余 gap 是否如实记录。

最多输出 10 个 findings。没有确定问题时明确说明，并列出未执行的视觉、跨平台或运行时验证。
```

## 6. 快速调用示例

```text
扫描范围：当前工作区 diff
对照基线：HEAD
目标平台：Windows
审查模式：只读报告
finding 上限：8
重点场景：打开 Settings -> 切到 TClient -> 搜索 WarList 卡片 -> 滚动内部列表

请遵守本文“全局审查约束”和“统一输出合同”，执行“UI、HUD 与文本布局”以及“交互帧、预热与调度”专项扫描。先验证候选问题，再输出 findings；不要修改代码。
```
