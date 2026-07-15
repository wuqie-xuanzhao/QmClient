---
name: qmclient-slider-input-migration
description: "Migrate DDNet/QmClient settings scrollbar options to ui_widget::SliderInputField while preserving text-plan collection, DoEditBox background options, and handling active-input scrollbar interaction correctly."
---

# QmClient 设置页滑动条+数值输入框迁移指南

## 适用场景
- 需要把设置页里旧的 `DoSettingsScrollbarOption` / `DoSliderWithScaledValue` 改写成统一的 `ui_widget::SliderInputField` 组合控件。
- 需要给 legacy 输入框背景色或避免 `DoEditBox` 双重绘制。

## 关键约束
1. `CUi::SEditBoxRenderOptions` 必须定义在 `CUi` 类内部；添加时要特别检查 `DoButton_Menu` 等成员是否被误删。
2. Legacy 设置输入框背景应使用 `ms_LightButtonColorFunction` + `ScaleBackgroundAlpha`，而不是 `SURFACE_ELEVATED`。
3. `DoSettingsScrollbarOption` 的函数体里必须仍能看到 `BuildSettingsScrollbarTextStyle(` 调用，否则测试 `QmMonitoringHelpers.SettingsStableTextPlanKeysMatchVisibleWrappers` 会失败。

## 迁移步骤

### 1. 扩展底层 `DoEditBox`
- 在 `src/game/client/ui.h` 的 `CUi` 类中加入：
  ```cpp
  struct SEditBoxRenderOptions { bool m_DrawBackground = true; };
  ```
- 新增 7 参数 `DoEditBox` 重载，内部用 `RenderOptions.m_DrawBackground` 控制是否绘制背景。
- **常见 bug**：在 `RenderOnly()` 分支加条件后，容易忘记在正常渲染分支也加。两个分支都必须：
  ```cpp
  if(RenderOptions.m_DrawBackground)
      pRect->Draw(...);
  ```
  否则 legacy 输入框会双重绘制背景。

### 2. 在 `UiForms` 中实现/扩展 `SliderInputField`
- 定义 `SSliderInputFieldOptions`：label、suffix、scale、flags、maxText、fontSize、labelAlign、valueMultiplier。
- `SliderInputField` 内部：
  - 滚动条使用 `Min / ValueMultiplier .. Max / ValueMultiplier` 缩放范围。
  - 输入框显示和编辑真实值（`Min..Max`）。
  - 支持 `SCROLLBAR_OPTION_INFINITE` 和 `SCROLLBAR_OPTION_NOCLAMPVALUE`。
  - 保留 `Ctrl+滚轮` 在区域内微调。
- 如需 legacy 输入框，使用 `LegacyTextFieldEx` 并传入 `SEditBoxRenderOptions{m_DrawBackground = false}`，由外层绘制一次背景。
- **输入框激活时的滚动条处理**：当 `pInput->IsActive()` 为 true 时，不要调用 `CUi::DoScrollbarH`（因为它会处理鼠标拖拽并改变视觉位置），应改为静态绘制滑块。参考 `DoScrollbarH(pColorInner)` 的非激活渲染逻辑：
  ```cpp
  CUIRect Rail = ScrollBar;
  CUIRect Handle;
  Rail.VSplitLeft(8.0f, &Handle, nullptr);
  Handle.x += (Rail.w - Handle.w) * Normalized;
  CUIRect Slider;
  Handle.VMargin(-2.0f, &Slider);
  Slider.HMargin(-3.0f, &Slider);
  Slider.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(ColorRGBA(0.15f, 0.15f, 0.15f, 1.0f)), IGraphics::CORNER_ALL, 5.0f);
  Slider.Margin(2.0f, &Slider);
  Slider.Draw(Ctx.m_pUi->ScaleBackgroundAlpha(Inner), IGraphics::CORNER_ALL, 3.0f);
  ```
- `SCROLLBAR_OPTION_MULTILINE` 在 `SliderInputField` 中无意义，因为控件已自行拆分布局；建议加注释说明被忽略。

### 3. 在 `CMenus` 中接入
- 在 `menus.h` 中声明：
  ```cpp
  bool DoSettingsSliderInputField(...);
  CLineInputNumber *GetSettingsSliderInput(const void *pId);
  ```
- 在 `menus.h` 中加入 `std::unordered_map<const void *, std::unique_ptr<CLineInputNumber>> m_vpSettingsSliderInputs;`。
- `GetSettingsSliderInput` 按 `pId` 懒创建 `CLineInputNumber`。
- `DoSettingsScrollbarOption` 仍负责：
  - 处理 `SCROLLBAR_OPTION_DELAYUPDATE`（走旧版 `Ui()->DoScrollbarOption`）。
  - 文本计划收集：调用 `SplitSettingsScrollbarRects` + `BuildSettingsScrollbarTextStyle` + `CollectMenuTextPlanItem`；收集时直接返回。
  - 然后调用 `DoSettingsSliderInputField` 做实际渲染/交互。
- `DoSettingsSliderInputField` 只负责组合 `SliderInputField`；不要在这里再次做文本计划收集。

### 4. 迁移 `DoSliderWithScaledValue`
- 设置 `Options.m_ValueMultiplier = Scale`。
- 调用 `ui_widget::SliderInputField`，传入真实 `Min/Max`。
- 返回结果即为变化状态。

## 验证
- 构建 `game-client`。
- 构建并运行 `testrunner`：重点检查 `QmMonitoringHelpers.SettingsStableTextPlanKeysMatchVisibleWrappers`。
- 运行 `python qmclient_scripts/gate/check_gate.py --mode quick`。
- 运行 `python qmclient_scripts/gate/check_gate.py --mode default`。
- **手动检查**：激活数值输入框后，尝试拖拽滚动条，确认滑块视觉位置不变化。

## 常见坑
- 修改 `ui.h` 后若 `ui.cpp` 报 `DoButton_Menu` 或 `m_BackgroundAlphaScale` 不存在，通常是把 `DoButton_Menu` 声明误删导致 `CUi` 类作用域断裂；检查大括号匹配。
- 把文本计划收集放到 `DoSettingsSliderInputField` 会导致 `DoSettingsScrollbarOption` 的函数体找不到 `BuildSettingsScrollbarTextStyle`，从而测试失败。
- `NoClampValue` 只应在颜色选择器等延迟更新场景或需要保留越界值的滑块使用；普通设置滑块保持默认 clamp。
- `DoScrollbarH` 在输入框激活时仍会被鼠标影响，必须显式用静态绘制替代。
