# Spec Review：全局卡片架构（统一拖拽 + 让位动画 + 持久化）

- 日期：2026-06-29
- 审查对象：`docs/superpowers/specs/2026-06-29-全局卡片架构-统一拖拽与持久化.html`
- 审查方式：spec 主张 vs 真实代码逐条核实
- 核实依据：`src/game/client/QmUi/QmCardOrderModel.h`、`src/game/client/components/qmclient/menus_qmclient.cpp:7792–7929`、全仓 Grep 符号
- status: active
- **后续状态（2026-06-29 更新）**：本 review 的 findings 已全部处理——`offset` 已清除（spec §3）、迁移边界已补（spec §2.2.1）、让位边界已落实（spec §3.4）、工时/依赖已修正（spec §6）、性能地基已写入（spec §7/B1 + 设计文档 §8）。唯 §五「愿景待验证」已被用户确认推翻（见下）。本文件保留作历史追溯。

## 总体结论

**方向对、骨架可用，但有 4 处硬伤必须修订才能进入实施。**

spec 对现状的把握出乎意料地扎实——"已建 `CModel`""让位框架在但效果不对""两套持久化"三条均经代码核实属实，不是空谈。让位动画的修复方向（预览布局写入 `TargetRect`）也正确。但它在三处"想当然了"：根因里凭空写了个不存在的 `offset` 变量；持久化迁移边界一个没写；"全局 Model 跨页保留"最根本的产品假设从未被质疑。按 CLAUDE.md「规则 1：不确定时先问」「规则 2：简单优先」，这三点不解决就动工会返工。

> 用户决策（2026-06-29）：架构方向（全局单例 Model）暂不质疑，先按当前 spec 把 Review 做完并据以修订；全局 Model 假设的验证标记为后续待办，不阻塞本 spec。

---

## 一、对现状的理解 — ✅ 大体准确，1 处技术错误

| spec 主张 | 核实 | 证据 |
|---|---|---|
| `qm_card_order::CModel` 已建（阶段2 Step1） | ✅ 属实 | `QmUi/QmCardOrderModel.h:11-50`，有 `Move`/`NormalizeColumns`/`ColumnIndices`/`Serialize`/`Parse`/`IsDirty`；测试 `qm_card_order_model_test.cpp` 覆盖 5 用例 |
| 让位机制存在（`m_TargetRect` + lerp） | ✅ 属实，且实现比 spec 更细 | `menus_qmclient.cpp:2206-2207` 双坐标；`:7899-7900` 指数 lerp；`:7892-7895` 首次初始化直接设 TargetRect（spec 未提的健壮细节） |
| 拖拽中 order 不变 → 无让位 | ✅ 根因准确 | `UpdateDropPreview()`(`:7920`) 每帧只算 `s_DropPreview` 落点，不改 `m_OrderInColumn`；`CommitDropPreview()`(`:7792-7865`) 松手才改 order+序列化 |
| 两套持久化（`QmSidebarCardOrder` / Tclient 内存 vector） | ✅ 属实 | `config_variables_qmclient.h` + `m_TClientLeftCardOrder`，死标志 `m_TClientSettingsCardDeckOrderDirty` 真在 |
| 松手归位靠 "offset 不硬切 0，`offset *= (1-LerpT)`" | 🔴 **技术错误** | 当前让位是 per-card `DisplayRect` lerp 追 `TargetRect`，代码里**没有 Column 级 `offset` 变量** |

### 🔴 Finding ①（重要）— 根因里的 `offset` 概念不存在

spec 3.1/3.3 描述了一个"Column 偏移 offset"机制，但 `menus_qmclient.cpp` 的让位是每张卡各自 `Disp += (Target - Disp) * 0.25f`（`:7899`），根本没有 `offset` 变量。按 spec 3.3 去实现，开发者会发现"没有 offset 可 lerp"。

**修订建议**：删掉 `offset` 概念，根因重写为——

> 拖拽中 order 不变 → 布局每帧算出的 `TargetRect` 不含被拖卡的腾位 → 其他卡的 `DisplayRect` lerp 追一个静止目标 → 全部停在原位 → 无让位视觉效果。

修复方向（spec 3.2 核心）正确：让 `UpdateDropPreview` 把被拖卡放到 DropIndex 后重排、把预览 `TargetRect` 写进 `m_TargetRect`，其他卡即 lerp 追变化的目标。

### 实现细节张力

`note.md` 05:06 结论"让位必须 SPRING+MERGE_TARGET，TWEEN 会抖"，当前是固定 `LerpT=0.25` 指数 lerp。指数 lerp 对让位基本够用（≈临界阻尼 spring），但 spec 3.2 未提该用 `ResolveUiAnimValue` 的 SPRING 重载，快速连续拖拽/跨列时可能不够顺。建议补一句。

---

## 二、安全性 — 🔴 持久化迁移边界几乎全遗漏（最大硬伤）

spec 2.2 迁移逻辑仅一句"首次启动检测旧 config，合并到 `qm_global_card_order`"。这是**会丢用户数据**的高风险操作，以下边界 spec 均未写：

| 边界 | 风险 | spec 覆盖 |
|---|---|---|
| 迁移幂等：第 2 次启动新旧 config 并存，不能重复合并 | 重复/冲突 | ❌ |
| stableId 映射：栖梦旧 `chat_bubble:col:order`（无 page）→ 新 `qm:visual:card_appearance:page:col:order` | 旧顺序丢失 | ❌ |
| 格式兼容：旧 3 段 vs 新 4 段（多 page），`Parse` 要容忍字段数不足 | 解析失败 | ❌（只说"未知 id 跳过"） |
| buffer 溢出：`MACRO_CONFIG` 定长 buffer，卡片 37→80+ 可能超长 | 截断/崩溃 | ❌ |
| 降级回退：切回旧版，新 config 旧版不读 → 数据"消失" | 数据丢失 | ❌ |

**修订建议**：迁移逻辑写成独立子节，至少包含幂等标志、stableId 映射表、3/4 段容错、buffer 容量估算。CLAUDE.md「全局硬约束」要求改格式/序列化要谨慎。

### 🟢 设计冗余

`CModel` 已有 `IsDirty()`(`QmCardOrderModel.h:43`)，但 spec 2.2 说"每帧 str_comp 检测变化"——两条路径重复。应统一为 Dirty 触发序列化。

---

## 三、性能 — 🟡 方向 OK，缺量化与缓存策略

A2 要求 `UpdateDropPreview` **每帧**对所有卡重算预览布局。需补：

- 缓存策略：DropIndex 不变时预览布局不变，应只在 DropIndex/被拖列变化时重算，而非每帧全量 O(N)。
- 与渲染合批交互：`TargetRect` 每帧变 → `s_GlassCards` 位置每帧变 → 合批 cull(`:947`) 每帧重判，可能影响 1-drawcall 合批(`:893`)稳定性。
- "每帧 str_comp"成本：37 卡可接受，80+ 卡需监控——回到 §二用 Dirty 替代 str_comp。

---

## 四、路线与工作量 — 🟡 大体合理，3 处工时/依赖问题

骨架（B1→B2 地基、A1+A2 让位、C1→D1 迁移、E1 收敛）逻辑通顺，但：

| 阶段 | spec 工时 | 问题 |
|---|---|---|
| B2 持久化迁移 | 0.5 天 | 🔴 严重低估。考虑幂等+映射+格式兼容+buffer，至少 1.5–2 天，且高风险数据操作 |
| C1 Tclient 迁入+删 section_loader | 1 天 | 🟠 偏紧。Ctrl+drag→长按 0.3s + 补持久化；Grep 显示 `section_loader.h` 8 函数被 `menus.cpp`/`menus.h`/`menus_settings.cpp` 多处引用，删除影响面比"Tclient 用"大 |
| E1 视觉统一 | 依赖标 B1 | 🔴 依赖标错。E1 消除 `RenderQmSettingsGlassCard` underlay+栖梦合批割裂，前提是 C1+D1 已用统一 Model。依赖应为 **B1+C1+D1** |
| 全程 | — | 🟡 未算测试/gate 工时。按 DDNet 实践约占 30–50%，实际 8–9 天而非 6 天 |

A2↔E1 顺序无约束：两者都动卡片渲染路径，应让位(A2)先于视觉收敛(E1)。

---

## 五、"全局卡片架构为何" — ⚠️ 没讲透，核心假设值得质疑

> ⛔ **本节已被推翻**（2026-06-29 用户确认）：全局卡片 = **跨页面组件编辑器**，跨页排布/迁移是真实需求，全局单例 Model 方向确认（见 spec §1 / 设计文档 §1）。原文保留作追溯。

spec 第 1 节给的是功能层动机，架构层动机与核心产品假设未交代。核心歧义：

- **(a) 同一张卡片跨页共享**：栖梦卡与 Tclient 卡内容完全不同，不会跨页复用 → (a) 不成立。
- **(b) 每页独立卡片集，仅存同一 config**：那"全局 Model"本质是"统一存储 + 每页 filter"，不需要"全局"概念。

若是 (b)，"全局 Model"是过度设计——带来全局耦合+迁移风险（§二），仅为未验证的"跨页保留顺序"UX 目标。违反 CLAUDE.md「规则 2：简单优先」。

> 用户决策：本 Review 不改架构方向，但 spec 应把该假设显式化为"待验证假设"，供未来确认。

---

## 六、让位动画边界遗漏 — 🟡

spec 3.2 未覆盖：

- 跨列拖拽的实时预览：`CommitDropPreview`(`:7824-7849`) 支持跨列 erase+insert，但拖拽中实时的跨列让位（源列腾位+目标列插入）怎么算两列 `TargetRect`？spec 只一句"基于新位置算每卡 TargetRect"太笼统。
- Full 列模块：`:7795` `if(m_Column==Full) return false`——Full 卡不拖不让位，全局 Model 里 column 怎么标、跨页怎么处理未写。
- 搜索过滤态：`:7914` 搜索时 `ResetModuleDragState`，预览布局要不要尊重过滤后的可见集未提。
- 折叠卡高度（路线图阶段 5 `qm_settings_card_collapsed`）：折叠卡 `TargetRect` 高度变小，让位预览用折叠高还是展开高未联动。
- 页面切换抖动：全局 Model 下切页时旧页 `ModuleCards` 还在 lerp 收敛，新页 `TargetRect` 突变可能抖。

---

## Findings 汇总（按严重级别）

| 级别 | finding | 位置 |
|---|---|---|
| 🔴 | 持久化迁移边界全遗漏（幂等/stableId映射/格式兼容/buffer/降级）— 数据丢失风险 | 2.2 |
| 🔴 | 根因里的 `offset` 概念不存在于代码，3.3 实现指引误导 | 3.1/3.3 |
| 🟠 | "全局 Model 跨页保留"核心产品假设未验证（用户决策：不改方向，spec 内显式化为待验证假设） | 1/2.1 |
| 🟠 | C1 删 section_loader 影响面被低估（多处引用，非仅 Tclient） | 4.1 |
| 🟡 | E1 依赖标错（应为 B1+C1+D1） | 6 |
| 🟡 | 工时低估（B2/C1）+ 未计测试 gate | 6 |
| 🟡 | 让位用 SPRING 重载（note 结论）vs 当前固定 lerp，spec 未明确 | 3.2 |
| 🟡 | 让位边界：跨列预览/Full卡/搜索态/折叠高度/页面切换抖动均未覆盖 | 3 |
| 🟢 | Dirty 与"每帧 str_comp"两条序列化检测路径重复 | 2.2 |
| 🟢 | spec 2.1 正文把目标态(`page`字段)当现状描述，与 B1"加 page"矛盾 | 2.1 vs B1 |

---

## 修订优先级（本 Review 落地后的 spec v2 改动清单）

1. 🔴 重写 3.1 根因（删 offset）+ 重写 3.3 松手归位（改为 per-card DisplayRect 收敛）
2. 🔴 2.2 新增"持久化迁移边界"子节
3. 🟡 第 6 节路线表：E1 依赖改 B1+C1+D1、B2/C1 工时上调、补测试 gate 说明
4. 🟡 第 3 节补让位边界子节
5. 🟡 2.1/3.2 小修（page 表述一致性、SPRING 重载、Dirty 替代 str_comp）
6. 🟢 第 1 节把"全局 Model 跨页保留"显式化为待验证假设
