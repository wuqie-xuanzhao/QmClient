# 项目 Agent Skills

本目录为 **Codex CLI** 与 **Oh My Pi (OMP)** 共用的项目级 skills 根。

## 布局

```text
.agents/skills/<skill-name>/SKILL.md
```

- Codex：扫描 `.agents/skills`（亦兼容 `.codex/skills`）
- OMP：`agents` provider 扫描 `.agent[s]/skills`（与 Claude/Codex 用户级 skills 并存；同名时按 provider 优先级 first-wins）

每 skill 单层目录，勿再嵌套 `group/skill/SKILL.md`（provider 默认不递归）。

## 当前 skills

| 名称 | 用途 |
|------|------|
| `audit-qmclient-quality` | QmClient 质量审计（原 `docs/superpowers/prompts/audit-qmclient-quality`） |
| `qmclient-i18n-workflow` | i18n 工作流与配置英文 source key 约定 |
| `qmclient-i18n-audit` | i18n 污染/迁移/validate 复检 |
| `qmclient-git-commit` | commit/PR 文案 |
| `qmclient-verification-gate` | gate 验证 |
| `qmclient-cpp-conventions` | C++ 兼容性边界 |
| `qmclient-codegraph-usage` | codegraph 优先 |
| `qmclient-slider-input-migration` | 设置页 Slider 迁移 |
| `github-actions-version-review` | Actions 版本审查 |
| `fix-windows-python-exit49` | Windows python exit 49 |

## 与文档的关系

- 长文提示词/模板仍可放在 `docs/superpowers/prompts/`（如质量扫描提示词正文）
- **可执行 skill 入口**以本目录 `SKILL.md` 为准；`docs/superpowers/prompts/*/SKILL.md` 仅保留跳转说明

## 用户级旧 skills

`~/.claude/skills` 下大量指向 `.cc-switch` 的 symlink 属于历史 Claude Code / Superpowers 安装，**不是本仓库真相源**。做 QmClient 任务时优先本目录 skills；全局旧 skill 可按需自行清理，避免与项目 skill 抢同名。
