# 项目 Agent Skills

本目录是 **Codex CLI** 与 **Oh My Pi (OMP)** 共用的项目级 skills 根。  
正文统一用**中文**撰写（命令、路径、标识符保持原文）。

## 布局

```text
.agents/skills/<skill-name>/SKILL.md
```

- Codex：扫描 `.agents/skills`（亦兼容 `.codex/skills`）
- OMP：`agents` provider 扫描 `.agent[s]/skills`
- 每 skill **单层**目录，勿再嵌套 `group/skill/SKILL.md`

## 当前 skills（精简集）

| 名称 | 用途 |
|------|------|
| `audit-qmclient-quality` | 客户端质量审计（功能级 / 横向模板） |
| `qmclient-i18n-workflow` | 翻译工作流；配置 Desc 必须英文 source key |
| `qmclient-i18n-audit` | i18n 污染排查、CJK 迁移与绿线复检 |
| `qmclient-git-commit` | commit / PR / 汇报文案 |
| `qmclient-verification-gate` | gate 分层与验证证据 |
| `qmclient-cpp-conventions` | C++ 兼容性与风格边界 |
| `qmclient-codegraph-usage` | codegraph 优先于 grep 循环 |

已移除（一次性或环境类，不进项目真相源）：
- `qmclient-slider-input-migration`
- `github-actions-version-review`
- `fix-windows-python-exit49`

## 与文档的关系

- 长文提示词仍放 `docs/superpowers/prompts/`（如质量扫描提示词正文）
- **可执行入口**以本目录 `SKILL.md` 为准
- `docs/superpowers/prompts/*/SKILL.md` 仅跳转说明

## 用户级 skills

`~/.claude/skills` 的历史 symlink 已清理；勿再依赖全局 Claude Code / Superpowers 链路做 QmClient 任务。  
需要全局通用 skill 时再按工具文档单独安装，避免与项目 skill 抢同名。
