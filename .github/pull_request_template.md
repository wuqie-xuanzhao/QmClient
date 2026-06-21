<!-- 提示：PR 标题请遵循 Conventional Commits + scope 规范：<type>(<scope>): <中文简述>
  feat(client): 添加新功能
  fix(gate): 修复门禁检查
  refactor(qmclient): 重构马甲组件
  perf(render): 性能优化
  docs(workflow): 文档更新
  build(ci): 构建系统或依赖变更
-->

## 变更说明

<!-- 描述这次 PR 做了什么、为什么需要这个改动。Reviewer 只读这段话就能理解 PR 的目的。 -->

## 改动内容

<!-- 按类型分组列出具体改动。可用类型：feat / fix / refactor / perf / docs / build。删除未涉及的类型。 -->

### feat

-

### fix

-

### refactor

-

### perf

-

### docs

-

### build

-

## 测试方法

<!-- 描述如何验证这次改动，给出可复现的步骤。建议包含：运行了哪些测试、测试环境说明。 -->

1. 运行 `ctest` 确认现有测试全部通过
2. 手动启动客户端/服务器，验证功能按预期工作
3. 检查编译无新增警告

## 检查清单

<!-- 提交 PR 前逐项确认。删除不适用的条目。 -->

- [ ] 已自查代码，无明显错误
- [ ] 代码风格符合项目规范
- [ ] 新增或更新了对应的测试
- [ ] 本地测试通过
- [ ] 不引入新的编译警告
- [ ] 无未处理的 FIXME / TODO / HACK 标记

### 兼容性

- [ ] 未修改物理行为（影响已有地图）
- [ ] 未破坏网络协议兼容性
- [ ] 未破坏 demo、skin 等文件格式兼容性
- [ ] 未让已有 rank 变得不可达
- [ ] 未破坏存档或配置文件的向后兼容性

### 视觉变更（如适用）

- [ ] 附带截图或录屏

### 高风险区域（涉及时勾选）

- [ ] 涉及 physics / collision / prediction
- [ ] 涉及 snapshot / input / timer
- [ ] 涉及 replay / demo / map behaviour
- [ ] 涉及 protocol fields

<!-- 若勾选了高风险区域，请在「变更说明」中补充 client / server / shared 影响侧。 -->

## 关联 Issue

<!-- 使用 Closes #123 或 Fixes #456 可在合并时自动关闭对应 Issue -->

## 截图 / 录屏

<!-- 涉及 UI 变更时附带截图或录屏。建议直接拖拽图片到此处。 -->

## 备注（可选）

<!-- 其他需要 Reviewer 了解的内容，如已知限制、后续计划、替代方案等。 -->

---

<!-- 构建和其他检查会自动运行。如果某些检查失败不必担心，资深开发者可以在合并前协助解决。 -->