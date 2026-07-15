# 项目 Agent Skills

本目录是 **Codex CLI** 与 **Oh My Pi (OMP)** 共用的项目级 skills 根。  
正文统一用**中文**撰写（命令、路径、标识符保持原文）。

原 `docs/ai-workflow/` 稳定规则已合并到各 skill 的 `SKILL.md` + `references/`；  
agent 可执行入口以本目录为准。

## 布局

```text
.agents/skills/<skill-name>/SKILL.md
.agents/skills/<skill-name>/references/   # 按需加载的细则全文
```

- Codex：扫描 `.agents/skills`（亦兼容 `.codex/skills`）
- OMP：`agents` provider 扫描 `.agent[s]/skills`
- 每 skill **单层**目录，勿再嵌套 `group/skill/SKILL.md`
- 细则放 `references/`，触发描述写在 frontmatter `description`（偏主动触发）

## 当前 skills

| 名称 | 用途 | 细则 |
|------|------|------|
| `qmclient-cpp-conventions` | C++ 兼容性、风格、热路径、专项 advanced 路由 | `references/ddnet-development.md` + `references/advanced/` |
| `qmclient-verification-gate` | gate 分层、构建测试串行、证据格式 | `references/verification.md` |
| `qmclient-git-commit` | commit / PR / 汇报 / Release 文案 | `references/git-workflow.md` |
| `qmclient-code-review` | 审查立场与 findings 格式 | `references/review.md` |
| `qmclient-i18n-workflow` | 翻译 extract→generate→validate | （skill 内） |
| `qmclient-i18n-audit` | i18n 污染排查与绿线复检 | （skill 内） |
| `qmclient-codegraph-usage` | codegraph 优先于 grep 循环 | （skill 内） |
| `audit-qmclient-quality` | 客户端质量审计模板 | 长文仍可联读 `docs/superpowers/prompts/` |

## 文档边界（原 meta）

**放进 skills 的内容**

- 跨会话稳定、agent 每次可执行的规则
- 一份 skill 一个主题；细则进 `references/`
- 可机械化的约束优先交给脚本 / gate，而不是扩 prose

**不要放进 skills**

- 改动历史、会话交接、feature status JSON
- 与 `docs/superpowers/` 重叠的任务计划 / 探索记录
- 仅为脚本清单服务的重复说明

**正确放置**

| 内容 | 位置 |
|------|------|
| 可执行规则 / 工作流 | `.agents/skills/*/SKILL.md` + `references/` |
| 执行计划、验证证据 | `docs/superpowers/plans/` |
| 稳定规格 | `docs/superpowers/specs/` |
| 探索记录 | `docs/superpowers/explore/` |
| 长文提示词 | `docs/superpowers/prompts/` |
| 脚本说明 | `qmclient_scripts/` |

`docs/ai-workflow/` 仅保留**跳转桩**，便于旧链接与 archive 文档解析。

## 用户级 skills

`~/.claude/skills` 的历史 symlink 已清理；勿再依赖全局 Claude Code / Superpowers 链路做 QmClient 任务。  
需要全局通用 skill 时再按工具文档单独安装，避免与项目 skill 抢同名。
