---
type: implementation
date: 2026-06-10
status: active
confidence: high
related:
  - docs/superpowers/explore/2026-06-05-QmClient汉化与i18n现状探索.md
---

# QmClient i18n overlay 收敛记录

## 速答

当前 QmClient i18n 口径已收敛为：

- 源码使用英文 key，匹配 DDNet `Localize()` 模型。
- 运行时先加载 `data/languages/<lang>.txt`，再以 additive overlay 方式加载 `data/qmclient/languages/<lang>.txt`。
- `data/qmclient/languages/english.txt` 保持为空，英文环境走源码 key fallback。
- 非英文 QmClient overlay 只补当前语言 DDNet base 尚未覆盖的 key。
- 生成脚本会保留已有人工译文，缺译文时使用英文占位，不再混用中文 key 或 english 反向映射。

## 本次修复

- `qmclient_scripts/languages_qmclient/extract_strings.py`
  - 统一扫描 `Localize`、`Localizable`、`TCLocalize`、`TCLocalizable`。
  - 扫描范围收敛为 QmClient/TClient/QmUi 相关源码和 `gameclient.cpp` 中的 QmClient 字符串。
  - 输出 `extracted_strings.txt` 时可从任意工作目录稳定运行。

- `qmclient_scripts/languages_qmclient/generate_all.py`
  - 按 `extracted_strings.txt` 和对应 DDNet base 语言文件计算 overlay key 集。
  - 保留已有人工译文，补齐缺失 key，删除不再需要的 key。
  - 简中缺译文时使用英文占位；品牌名和字段名可显式 passthrough。

- `qmclient_scripts/languages_qmclient/validate.py`
  - 校验 `extracted_strings.txt` 是否与当前源码提取结果一致。
  - 校验每个 QmClient 语言文件的 key 集和顺序是否匹配生成规则。
  - 保留原有解析校验，防止格式损坏。

- `CMakeLists.txt`
  - 将 `qmclient/languages/simplified_chinese.txt` 加入 data 清单，避免打包遗漏简中 overlay。

## 当前事实

- `extracted_strings.txt` 当前包含 1202 个 key。
- 中文 key 数量为 0。
- `english.txt` 当前为空文件。
- `simplified_chinese.txt` 当前为 656 个 overlay 条目。
- 多数其它语言当前为 956 个 overlay 条目；差异来自对应 DDNet base 语言文件已覆盖的 key 数不同。

## 验证

```text
Command: python qmclient_scripts/languages_qmclient/extract_strings.py
Result: pass, extracted 1202 unique localization strings
Scope: 验证源码提取入口覆盖当前 QmClient i18n 字符串
```

```text
Command: python qmclient_scripts/languages_qmclient/generate_all.py
Result: pass, regenerated all 39 QmClient language files
Scope: 验证 overlay 文件可由当前提取结果复现
```

```text
Command: python qmclient_scripts/languages_qmclient/validate.py
Result: pass, all 39 language files parse and match expected key sets
Scope: 验证 QmClient i18n 不再出现源码/提取文件/语言文件三套口径漂移
```

## 风险边界

- Client：有影响，限定在菜单/HUD/提示文本本地化资源。
- Server：无影响。
- Shared：无影响。
- Tooling：有影响，限定在 QmClient 语言提取、生成、校验脚本。
- 协议影响：无。
- 预测影响：无。
- 物理影响：无。
- Demo 影响：无。
- 地图影响：无。
- 兼容性风险：低；运行时加载模型保持不变，仅修正 overlay 资源和生成链路。
