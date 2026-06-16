# Tee 页面静止花屏与 Preview Maintenance FBO 冲突排查报告

日期：2026-06-02
状态：**已修复并验证**

## 现象

用户在 `Settings -> Tee` 页面观察到两类问题：

1. 静止不滚动时持续出现花屏/闪烁。
2. 皮肤列表滚动到已加载区域时帧率下降明显。

在更早阶段，还出现过进入服务器列表时左上角出现设置页缩影的问题。该问题已先行修复，本报告聚焦 Tee 页面仍然残留的静止花屏。

## 最终结论

Tee 页面残余花屏的直接原因，不是 page FBO 或 section FBO 命中错误，而是 **preview cache maintenance 在可见 Tee 页面内持续执行重型 render-target/readback 生成链**，导致可见页面渲染过程中反复进行：

- `CreateRenderTarget`
- `BeginRenderTarget`
- `RenderTee`
- `EndRenderTarget`
- `ReadRenderTarget`
- `DestroyRenderTarget`

这条链发生在 `ProcessSettingsSkinPreviewCacheMaintenanceJob(...)` -> `RenderSettingsSkinPreviewCacheLayerImage(...)` 路径中。  
在可见 Tee 页面静止时，该 maintenance 更容易进入执行窗口，因此表现为“静止一直闪，滚动时反而好一些”。

## 查因流程

### 1. 先排除跨页面 settings warmup 污染

初始怀疑是 settings runtime warmup 在非设置页也运行，导致跨页面 FBO/状态泄漏。  
证据是服务器列表左上角会出现设置页缩影。

已修复项：

- 只允许在当前实际位于 `Settings` 页且 UI idle 时执行 `PrewarmSettingsRuntimeCaches(...)`

修复后：

- 服务器列表缩影现象消失
- 说明跨页面 warmup 泄漏判断成立
- 但 Tee 页面静止花屏仍然存在，说明还有第二条独立问题链

### 2. 沿着“每帧仍在更新的渲染路径”继续排查

用户指出一个关键观察：

- 花屏像是“每帧覆盖出来的”
- 滚动时反而好一些

这意味着应优先查：

- 当前页面内仍会持续执行的 FBO/render-target/readback 路径
- 而不是只看 warmup 是否命中了旧缓存

### 3. 用代码结构先排除 Tee 页 page/section FBO

检查结果：

- Tee 整页 page FBO 已禁用
- Tee section static FBO 也未启用
  - `skin-list`
  - `identity`
  - `country-list`

因此 Tee 页当前仍可能持续触发 render target 的路径，只剩 preview maintenance。

### 4. 用 perf log 验证不是 page/section runtime cache

用户提供日志：

- `C:\Users\11054\AppData\Roaming\DDNet\dumps\QmClient_Perf\qm_perf_2026-06-02_04-22-23.log`

关键证据：

- `page=settings:tee ... page_fbo=miss ... reason=page_fbo_unsupported`
- `page=settings:tee ... section_fbo=miss ... reason=section_fbo_not_ready`

这说明在花屏发生时：

- Tee 页 page FBO 没有被使用
- Tee 页 section FBO 也没有被使用

因此可见异常不是 runtime cache 回放造成的。

### 5. 锁定 preview maintenance

剩余唯一涉及 FBO/readback 的路径是：

- `ProcessSettingsSkinPreviewCacheMaintenanceJob(...)`
- `RenderSettingsSkinPreviewCacheLayerImage(...)`

它会在 Tee 页面空闲窗口里尝试生成 preview cache artifact。  
而这条路径恰好符合用户观察：

- 静止时触发
- 滚动时被抑制
- 已加载区域更容易触发更多后台生成

## 修复方式

最终修复不是简单删除 preview generation，而是 **把 generation 从可见 Tee 页面移走**：

- 在 **可见 Tee 页面** 内，禁止 preview maintenance 的重型 render-target/readback 生成链
- 保留现有缓存消费路径：
  - `FindTextures(...)`
  - `LoadTexturesFromDisk(...)`
- 保留 live fallback：
  - `RenderTee(...)`
- 保留 pending candidate 队列，不在离开 Tee 页面时清空
- 在 **非 Tee 的设置页** 空闲帧中继续 pump maintenance，把 preview artifact 延后生成

也就是说：

- 可见行仍然优先吃已有缓存
- 缓存 miss 时仍正常 live 画 tee
- 可见 Tee 页面不再承担 preview generation
- preview generation 仍然存在，但被推迟到更安全的 settings idle 上下文
- 如果离开 Tee 页面时存在 active maintenance job，当前 candidate 会先回灌到 pending 队列，再由后续 idle maintenance 继续处理

对应代码点：

- `src/game/client/components/settings_skin_preview_cache.h`
- `src/game/client/components/settings_skin_preview_cache.cpp`
- `src/game/client/components/menus_settings.cpp`

核心变化有两点：

1. `SettingsSkinPreviewCacheShouldRunMaintenance(...)` 增加 `HeavyRenderTargetWorkAllowed` 门禁，并在 Tee 可见页调用处显式传入 `false`
2. `PumpSettingsSkinPreviewCacheMaintenance(...)` 被抽成统一 helper，用于在非 Tee 设置页的 idle 帧继续处理 pending generation

## 修复结果

用户复测结果：

- “花屏好了”

这说明：

- Tee 页面残余花屏确实来自 preview maintenance 的重型 FBO/readback 链
- 之前对 page/section FBO 的怀疑不是主因
- 真正的问题是“可见页里做生成”，而不是“缓存消费本身”

## 影响与取舍

当前修复是一个行为重排，而不是功能删除：

### 保留

- 可见页缓存消费
- live tee fallback
- 背景 source skin warmup / ready 队列

### 改变

- 可见 Tee 页面内不再即时生成 preview artifact
- preview artifact 会在离开 Tee 页面后的安全窗口继续生成

这样做的目的是把“缓存消费”和“缓存生产”拆开，让用户可见帧只负责稳定显示，把 render-target/readback 重活移出交互路径。

## 后续建议

如果后续还要继续优化，方向应是进一步降低 maintenance 对主线程的干扰，例如：

1. 把 generation 的批次预算做得更细，减少单帧连续 layer 生成
2. 给非 Tee idle maintenance 增加更明确的时间预算/统计日志
3. 如果后端能力允许，再考虑更彻底的异步 readback/编码链

目标应是：

- 保留缓存带来的二次打开加速
- 但不在可见 Tee 页面里做任何 render-target/readback 生成

## 验证记录

Command: `cmake-build-release\\testrunner.exe --gtest_filter=SettingsSkinPreviewCache.*:SettingsWarmup.*:SettingsRuntimeCache.*:SettingsResourceJobs.*`
Result: PASS, 117/117
Scope: 验证 preview maintenance 门禁、deferred generation 调度和相关 warmup/resource/cache 回归
Gaps: 未覆盖真实运行时 GPU/FBO 视觉路径，只能由人工复测补齐

Command: `qmclient_scripts\\cmake-windows.cmd --build cmake-build-release --target game-client -j 10`
Result: PASS
Scope: 验证客户端可编译
Gaps: 未包含完整 docs gate

Command: `qmclient_scripts\\cmake-windows.cmd --build cmake-build-release --target testrunner -j 10`
Result: PASS
Scope: 验证测试目标可构建
Gaps: 只证明编译，不证明行为

Command: `cmake-build-release\\testrunner.exe`
Result: PASS, 982/982
Scope: 全量回归
Gaps: 仍需人工视觉复测确认 GPU/驱动相关表现

Command: `python qmclient_scripts/gate/check_docs.py`
Result: PASS
Scope: 文档入口与治理一致性
Gaps: 不覆盖运行时行为

Command: 用户手工复测 `Settings -> Tee`
Result: 花屏消失
Scope: 直接验证本 bug 的用户可见症状
Gaps: 尚未恢复后台 preview generation 的最终形态
