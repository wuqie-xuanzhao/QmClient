# Tee 皮肤列表 WebP 磁盘缓存设计

> **部分内容已过时** — 部分描述不再反映当前代码状态，请以较新的相关文档为准。

## 背景

当前 `设置 -> Tee` 的皮肤列表已经完成了以下稳定性与预热修复：

1. 皮肤列表从“整批发布”改成了增量发布，列表可更早显示。
2. 可见区与下方预取区会更积极地触发皮肤资源加载。
3. Tee 设置页激活时会临时提高 skin finalize / GPU upload budget。
4. `RequestLoad()` 生命周期 bug 已修复，`std::bad_optional_access` 崩溃路径已经收口。

但列表预览仍然存在一个产品问题：

- 同一会话内切页返回、列表 refresh、滚动回看时，tee preview 仍然主要依赖现场 `RenderTee(...)` 组合绘制。
- 当前 settings runtime cache 主要解决“页面 / section FBO 缓存”，不是“皮肤列表 preview 结果复用”。
- 如果直接缓存“最终着色后的 tee preview”，自定义 body / feet color 会迅速打碎命中率，收益很低。

因此这次设计的目标不是继续加预算，而是：

**为 Tee 设置页皮肤列表引入颜色无关、可跨会话复用的 WebP 磁盘缓存。**

## 设计目标

1. `设置 -> Tee` 皮肤列表的 preview 在同会话滚动、切页返回、refresh 后尽量复用缓存，而不是重新完整生成。
2. 缓存必须复用现有 tee 着色链路，不重写玩家实际渲染逻辑。
3. 缓存必须避免把 body / feet color 编进缓存 key；颜色继续在最终绘制阶段叠加。
4. 缓存落在 DDNet save 目录下，用户可见、可删除、可清理。
5. 缓存命名简单可读，失效依赖版本号 + 内容 hash，不引入数据库或复杂 manifest。
6. 缓存读写失败时必须安全回退到当前即时渲染路径，不能影响皮肤列表可用性。

## 非目标

1. 不缓存整个 Tee 设置页。
2. 不缓存 hover / selected / favorite / queue / tooltip / search highlight 等交互态。
3. 不改变玩家实际游戏内 tee 渲染语义。
4. 不把这个方案扩展到所有设置页或所有资产预览。
5. 不引入 SQLite、JSON manifest 或额外缓存索引文件。

## 当前代码事实

1. `CTeeRenderInfo::Apply()` 同时持有 `m_OriginalRenderSkin` 与 `m_ColorableRenderSkin`。
2. `CTeeRenderInfo::ApplyColors()` 只设置 `m_CustomColoredSkin`、`m_ColorBody`、`m_ColorFeet`。
3. `CRenderTools::RenderTee()` 会在渲染阶段依据 `m_CustomColoredSkin` 选择 original/colorable 贴图，并对 body / feet 调用 `Graphics()->SetColor(...)`。
4. `CSkins::LoadSkinFinish()` 已经为每个皮肤生成 `m_OriginalSkin` 与灰度化后的 `m_ColorableSkin`。
5. 仓库已经链接 libwebp，并支持 `CImageLoader::LoadWebP(...)`；但当前没有统一的 `SaveWebP(...)` helper。
6. `menus_settings_assets.cpp` 已经有 Workshop WebP thumb 缓存路径、异步图片解码、失败回退与磁盘清理的本地模式可复用。
7. `menus.cpp` / `section_loader.cpp` 已经存在 render target API，可用于在主线程离屏录制 preview 图。

## 关键工程前提

当前引擎**没有**现成的“对任意缓存 preview bitmap 再按 body / feet 通道重新着色”的公开 API。

现有 `RenderTee()` 只支持：

1. 从 `CTeeRenderInfo` 的 original / colorable 皮肤贴图直接绘制；
2. 在绘制 body / feet 时当场 `SetColor(...)`；
3. 使用现有 tee quad 布局输出最终像素。

因此本设计不能偷换成“先缓存一张普通 tee 图，再在之后神奇地重新按 body / feet 分色”。如果要保持缓存颜色无关，必须先补最小能力：

1. **tee preview layer 输出支持**：能够显式录制 outline / body / feet / eyes 等 preview layer；
2. **render target readback 支持**：能够把离屏 target 内容读回 `CImageInfo` 以便编码成 WebP。

这两项都必须作为方案的一部分明确实现。

## 关键结论

**颜色叠加逻辑本来就是通用的现有逻辑。**

因此本设计不重写 `RenderTee()` 主逻辑，而是在 Tee 设置页列表 preview 上方增加一层：

- 录制颜色无关的 preview 中间结果。
- 将该中间结果编码为 WebP 落盘。
- 运行时读取 WebP，上传为纹理，再按现有 body / feet color 叠加规则完成最终显示。

## 推荐方案

采用方案：

**颜色无关的 Tee preview layer WebP 磁盘缓存 + 运行时颜色后叠加。**

### 为什么不是“最终彩色图缓存”

因为如果最终彩色图进入 key：

- `SkinName + UseCustomColor + BodyColor + FeetColor + PreviewSize + Emote ...`

命中率会被颜色组合快速稀释，缓存空间与生成成本都不优雅。

### 为什么不是“只做内存缓存”

内存缓存能解决同会话重复绘制，但用户明确希望：

- 直接使用磁盘缓存
- 支持跨会话复用
- 缩略图使用 WebP 减少空间

因此第一阶段就直接落地磁盘缓存，但仍保持回退逻辑简单。

## 总体架构

新增一个专用于 Tee 设置页列表 preview 的缓存模块，例如：

```cpp
class CSettingsSkinPreviewCache;
```

职责拆分：

| 模块 | 职责 |
|------|------|
| `CSkins` | 继续负责皮肤资源加载、状态机与 `m_OriginalSkin` / `m_ColorableSkin` 生成 |
| `CSettingsSkinPreviewCache` | 管理 preview key、磁盘路径、内存命中、WebP 读写、清理策略 |
| `menus_settings.cpp` Tee list 渲染段 | 查询缓存、触发录制、使用缓存纹理绘制 preview |
| `RenderTee` 现有逻辑 | 继续作为录制时与 fallback 时的真实绘制实现 |
| `CImageLoader` | 新增 `SaveWebP(...)`，统一负责 WebP 编码 |

## 缓存粒度

缓存粒度不是整张最终 tee 图，而是**颜色无关的 preview layer 图**。

初版建议缓存一张已经适配 Tee 列表尺寸的**颜色无关中间图**，其像素语义必须满足：

1. body / feet 部分来自 `m_ColorableSkin` 灰度贴图；
2. outline、eyes 等不依赖 body/feet color 的部分保留原有表现；
3. 最终列表绘制时仍能基于现有颜色逻辑叠加 body / feet color。

实现上允许两种等价方式：

### A. 分层缓存

- outline layer
- body grayscale layer
- feet grayscale layer
- eyes layer

### B. 颜色无关 draw-plan / intermediate 渲染缓存

- 先记录颜色无关离屏结果
- 最终显示阶段再乘 body / feet color

**推荐优先实现 A（分层缓存）**，因为它更直接、更容易验证颜色正确性。

## 缓存 key

缓存 key 必须只包含会影响“颜色无关 preview 内容”的字段：

```text
Skin identity
Preview cache version
Skin content hash / resource revision
Preview size bucket
Preview pose variant
```

明确不进入 key 的内容：

- body color
- feet color
- selected / hover
- favorite / queue
- search filter
- scrollbar state

### 预览姿态约束

Tee 设置页列表 preview 当前使用的是稳定姿态：

- `CAnimState::GetIdle()`
- 固定方向 `vec2(1.0f, 0.0f)`
- 固定列表项尺寸桶

因此初版 key 可以收敛到：

```text
SkinName + CacheVersion + SkinContentHash + SizeBucket
```

如果未来列表 preview 的 emote / direction 变成动态，再把 variant 纳入 key。

## 文件名与目录

缓存目录：

```text
qmclient/skins/preview_cache/
```

Windows save 路径示例：

```text
C:/Users/11054/AppData/Roaming/DDNet/qmclient/skins/preview_cache/
```

文件名规则：

```text
<sanitized-skin-name>--v<cache-version>--<hash>.webp
```

例如：

```text
default--v1--91ab23ce55d0.webp
beast--v1--7f3c1d8a2b44.webp
```

要求：

1. 皮肤名需 `str_sanitize_filename`。
2. `hash` 来自皮肤内容 / 预览版本 / 尺寸桶组合后的稳定 hash。
3. 不单独维护 manifest；磁盘文件名就是主索引。

## 缓存数据来源

### 读路径

1. 列表项进入可见区 / 预取区。
2. `RequestLoad(...)` 确保 `CSkinContainer` 资源进入 `LOADED`。
3. 计算 preview cache key 与磁盘路径。
4. 若内存已有纹理句柄，直接命中。
5. 否则若磁盘 WebP 文件存在，异步解码并上传纹理。
6. 若磁盘 miss，再安排主线程离屏录制并生成 WebP。

### 写路径

1. 仅对 `LOADED` 的真实皮肤生成 cache。
2. 使用现有 render target API 在主线程离屏录制颜色无关 preview layer。
3. 得到 `CImageInfo` 后调用新增的 `CImageLoader::SaveWebP(...)`。
4. 写入成功后，将该结果保留在内存缓存中并供当前帧/后续帧复用。
5. 写入失败只记录日志并退回内存或即时渲染，不影响 UI。

## 与现有 visible / prefetch 逻辑的关系

保持现有调度优先级，不重新设计列表滚动策略。

当前已有：

- visible range
- visible 下方 2 行 prefetch
- Tee settings active budget boost

新缓存层只复用这些信号：

### visible item

- 优先查内存 / 磁盘缓存
- miss 时最高优先级生成

### prefetched item

- 次优先级生成磁盘 / 内存缓存

### 非 visible / 非 prefetched item

- 不主动生成
- 只保留已存在缓存

## 失效规则

### 必须失效

1. 皮肤源内容变化（通过内容 hash 自然失效）。
2. preview cache 版本变化。
3. preview size bucket 规则变化。
4. preview 录制逻辑变化。
5. Graphics backend / render target 录制语义变化到不兼容旧文件时。

### 不需要失效

1. body / feet color 变化。
2. selected / hover。
3. queue / favorite。
4. 搜索字符串变化。
5. 列表滚动位置变化。

## 清理策略

不引入 manifest，但必须有**最小清理机制**，避免目录无限增长。

初版建议：

1. 启动 Tee 设置页缓存系统时扫描 `qmclient/skins/preview_cache/`。
2. 删除不符合当前文件命名规则的残留文件。
3. 若缓存文件总数超过上限，按文件修改时间从旧到新裁剪。

建议上限：

- 默认按文件数裁剪，例如 512 或 1024 个文件。

原因：

- 文件数限制实现最简单；
- 无需额外持久化统计；
- 对 WebP 缩略图缓存已经足够。

## 失败回退

任何缓存步骤失败都不能阻塞 Tee 列表：

1. 磁盘文件不存在：回退到即时 preview。
2. WebP 解码失败：删除坏文件并回退到即时 preview。
3. render target 录制失败：本帧继续即时 preview。
4. WebP 写入失败：仍然可以使用当前帧生成的内存结果，不把错误扩大成 UI 空白。

## 测试策略

### 单元测试

新增 / 扩展测试覆盖：

1. 缓存文件名构造：皮肤名 sanitize、版本号、hash、`.webp` 后缀。
2. key / hash 输入不包含 body/feet color。
3. 同皮肤不同颜色命中同一磁盘 cache key。
4. 版本号变化导致路径变化。
5. 损坏 WebP 文件触发删除与 fallback。
6. 清理策略在超上限时删除最旧文件。

### 集成 / 行为验证

1. 第一次进入 `设置 -> Tee` 时生成 cache 文件。
2. 切到服务器页再回 Tee 页，preview 直接复用缓存而非重新完整生成。
3. refresh 后已存在且未失效的 preview 可继续命中。
4. 更改 body / feet color 时，不重新生成磁盘文件，但显示颜色正确变化。
5. 皮肤源文件改变后，旧 cache 不再命中，新 cache 文件重新生成。

## 风险与约束

1. 初版直接上 WebP，意味着必须新增 `SaveWebP(...)` 写路径；这属于新能力，但范围可控制在 `image_loader.*`。
2. render target 录制必须只在主线程发生，不能在后台线程直接触碰 GPU。
3. 磁盘缓存写入要避免在列表渲染热路径里无限同步阻塞；需要队列化或预算化地生成。
4. 因为当前仓库已有大量设置页 FBO / Workshop thumb 逻辑，必须复用其已有模式，避免再造一套完全平行的资源管理器。

## 推荐落地顺序

1. 新增 preview cache key / path / prune helper 与测试。
2. 新增 `SaveWebP(...)` 与最小 WebP 写测试。
3. 在 Tee 设置页 preview 渲染中接入“查 cache / 读 cache / miss fallback”。
4. 增加主线程离屏录制与 WebP 写入队列。
5. 跑文档检查、目标测试、客户端构建与手动 UI 验证。

## 验收标准

以下条件全部满足才算完成：

1. `设置 -> Tee` 皮肤列表会在 `qmclient/skins/preview_cache/` 下生成 `.webp` 缓存文件。
2. 缓存文件名符合 `皮肤名--版本--hash.webp` 规则。
3. body / feet color 改变时不会因为颜色不同生成不同磁盘文件。
4. 切页返回与同会话重复滚动时，preview 复用缓存，主观加载速度明显快于当前实现。
5. WebP 文件损坏或缺失时，列表仍正常 fallback 渲染，不崩溃。
6. 所有新增/修改测试通过，`game-client` 构建通过，文档检查通过。
