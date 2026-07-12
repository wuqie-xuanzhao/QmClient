# QmClient 设置页 UI 统一 P6 迁移计划

**状态（2026-07-13，基于 `1fb68622ac`）：** P0-P5 公共 page/card/input/scroll 契约已存在。QmClient Overview 与 Contributors 已迁移并通过结构测试；公共 deck 已补齐普通标题拖拽与标题动作互斥契约。QmClient Visual/module、Global Search、TClient 主页和复杂子页仍未迁移。P7 不在本计划范围内。

## 目标与边界

目标是让 QmClient/TClient 设置页使用唯一的 `SettingsPageLayout`、`SettingsCardDeck`、`CScrollRegion` 和全局 `SettingsCardOrderModel()`，并从已迁移生产路径删除旧 glass、私有拖拽、私有滚动、TClient cache box/inset/height 双路径。

保留：现有 stable ID、搜索跳转、折叠/usage、新功能标记、PrewarmOnly、tab transition、配置读写、资源加载、telemetry 和 legacy order migration。不得改变协议、物理、预测、Demo/地图/skin/config 格式。

不做：P7 非卡片菜单性能收口；R1/R2/R3 公共组件扩面、信息架构重组和渲染管线改造。

## 当前代码审计

| 区域 | 当前事实 | 结论 |
|---|---|---|
| Overview | `ResolveSettingsPageLayout` + `CSettingsCardDeck` + `CScrollRegion` | 已迁移 |
| Contributors | 两张 full-column card，搜索路由/reveal、赞助名单换行和 QR 预热边界已收口 | 已迁移 |
| QmClient module | `menus_qmclient.cpp` 约 1457-8055 仍使用 `s_GlassCards`、`SQmModuleDragState`、`SQmModuleDropPreview`、`RegisterModuleCard`、旧 scroll/glass 和 local cached heights；约 37 个模块 | 未迁移 |
| QmClient Global Search | `RenderSettingsGlobalSearchContent` 仍使用 `BeginSettingsQmScrollContainer`、`RenderQmSettingsGlassCard` 和 `vGlassCards` | 未迁移 |
| TClient | 主页及复杂子页仍有 cache box/private inset/private drag/section height 路径 | 未迁移 |
| Adapter/model | `QmCardRegistry`、`SettingsCardOrderModel()` 和显式 model adapter 已存在；`QmModuleLayoutModel()` 过渡 singleton 仍被旧 renderer 使用 | 迁移完成前不得删除兼容入口 |

## 当前接手检查点

- 已验证：`game-client`、`testrunner`、Visual 迁移边界 focused tests；`check_docs.py` 通过。
- 已确认：Visual 的试迁移入口已撤回。旧 renderer 仍是生产路径，避免把缺少控件的半迁移路径交付。
- Slice 1 的硬边界：旧配置入口必须逐项保留（dropdown、颜色、scope/keybind、legacy toggle）；卡片测量必须和实际内容同源并随配置/本地化/UI scale 重算；折叠读写必须复用 canonical parser，不能追加裸 `;key;` 格式。
- 当前 gap：Visual、Functions/HUD、Global Search、TClient 尚未迁移；未做视觉验收、全量 C++ 回归和 default gate。

## 执行切片

### Slice 1：QmClient Visual module deck

范围：Visual tab 的 10 个 registry module（`ChatBubble`、`CameraView`、`SkinTransition`、`FocusMode`、`WeaponAnimation`、`Streamer`、`EntityOverlay`、`CollisionHitbox`、`TranslateUi`、`CardAppearance`）。

做法：

- 为每个 module 建立/校验 `qm:*` stable ID、默认 tab/column/order，提交 `SSettingsCardDefinition` 到公共 Deck。
- module content 只接收 canonical `m_ContentRect`；card frame、header、hit/drag rect、滚动和自动滚动归 Deck/`CScrollRegion`。
- 折叠、usage、新功能标记、P3 `InputField`/`NumericField`、dropdown、颜色控件、scope/keybind、legacy toggle 和 preview 行为保持不变。
- `m_Measure` 必须与实际 render 共用动态行数/高度来源；不能用固定估算值覆盖条件控件，不能让内容超出 canonical `m_ContentRect`。
- 折叠状态必须通过现有 canonical parser/serializer 读写，兼容历史 `key[:...]` 条目并保持持久化格式规范化。
- Visual slice 完成前不得删除 adapter 的 legacy migration；不得保留“公共 wrapper + 旧绘制”双路径。

出口：Visual 生产函数无 `s_GlassCards`/private drag/register/old scroll；registry 全覆盖；focused tests、`testrunner` build、`game-client` build、quick gate；人工检查 1280x720/100% 英文和 960x720/125% 简中下滚动、折叠、拖拽、输入、搜索 reveal。

### Slice 2：QmClient Functions 与 HUD module deck

范围：Functions 13 个 module、HUD 12 个 module，沿用 Slice 1 的 callback/deck/model 结构。

出口：分别按 tab 验收，不以 Overview/Contributors 结果代替；覆盖长卡片、动态高度、dropdown popup wheel、折叠/usage、PrewarmOnly 和跨列拖拽。两 tab 都通过 focused tests、build、quick gate 后才进入 Slice 3。

### Slice 3：QmClient Global Search 与 Config 入口

范围：Global Search 搜索卡、搜索输入、结果跳转/reveal；QmClient Config 复用 TClient config browser 的唯一内容 viewport。

做法：删除旧 `BeginSettingsQmScrollContainer`/`FinishSettingsQmScrollContainer`、`RenderQmSettingsGlassCard`、`vGlassCards` 生产调用；搜索结果和输入使用公共 card/input/scroll。Config 不再外包第二层 card/scroll。

出口：空结果、跨页跳转、Qm/TClient/Graphics stable ID、IME/clear、popup wheel、PrewarmOnly 均有结构或行为测试；Global Search 仍能 reveal 目标卡片。

### Slice 4：TClient 主页与复杂子页

范围：TClient Settings 主页/`CSectionLoader`、BindWheel、StatusBar、ChatBinds、WarList、Info、Profiles、Configs。

做法：

- `CSectionLoader` 只负责 stable ID 对应的 content measurement/cache 和 placeholder/compact/full 调度；三种路径共用 measured content height。
- Deck 独占完整 card frame；删除 cache box、private inset、private drag/drop/order、page-static cached height 和第二套 scroll。
- 保留输入、dropdown、selection、profile/config business semantics。

出口：每组子页有稳定 registry ID；`section_height_measured` 与 `section_height_rendered` 差值不超过 `0.01f`；主/子页结构删除测试、SectionLoader callback 计数测试、focused build、全量 C++ 测试和人工滚动/弹窗检查通过。

### Slice 5：P6 总验收

只在 Slice 1-4 全部通过后执行：

- 删除无调用的 Qm glass/scroll/style、TClient cache/drag/inset 声明和实现；保留必要 legacy parser/migration API。
- 串行运行 P6 focused tests、`run_cxx_tests`、`run_rust_tests`（如涉及）、`game-client`、`check_docs.py`、`check_gate.py --mode default`、`git diff --check`。
- 完成所有页面人工矩阵并记录截图/结果和未验证 gap。
- 获取独立只读 review；findings 收口后才可把 P6 标记完成。

## 统一阻断条件

- 仍存在公共 wrapper 包裹旧绘制、双 scroll state、私有 drag/order coordinator 或第二份 card model。
- `PrewarmOnly` 驱动动画、滚动、点击、配置写入或资源解码副作用。
- content measurement 与实际绘制高度不一致，或 resize/language/config 变化后卡片跳动。
- 搜索结果只改变 tab 不 reveal 目标卡片；popup 首个 wheel 泄漏到父滚动。
- registry 缺 stable ID、默认 placement 重复、legacy order migration 丢失或错误把 Full card 移入列。
- 出现协议/玩法/配置格式变化，或新增 source key 未完成 i18n 生成链。

## 验证口径

每个 slice 至少记录：

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <what this proves>
Gaps: <what was not verified>
```

Windows 默认入口：

```powershell
git submodule update --init --recursive
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
cmd /c call qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_gate.py --mode quick
```

同一 build 目录的目标必须串行。过滤测试只用于定位，最终交付必须补全量测试；未完成视觉验收或全量测试必须明确写成 gap。
