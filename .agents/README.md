# 项目 Agent Skills

本目录是 **Codex CLI** 与 **Oh My Pi (OMP)** 共用的项目级 skills 根。  
正文统一用**中文**撰写（命令、路径、标识符保持原文）。

**可执行 AI 工作流规则只在这里**——不要再往 `docs/` 写 agent 规则正文。

## 布局

```text
.agents/skills/<skill-name>/SKILL.md          # 规则正文（整合进 skill）
.agents/skills/<skill-name>/references/       # 仅按需专项（如 advanced、长提示词）
```

- Codex：扫描 `.agents/skills`
- OMP：`agents` provider 扫描 `.agent[s]/skills`
- 每 skill **单层**目录；主规则写进 `SKILL.md`，不要拆成「docs 目录 + 摘要 skill」双轨

## 当前 skills

| 名称 | 用途 | 正文位置 |
|------|------|----------|
| `qmclient-cpp-conventions` | C++ 兼容性、风格、热路径 | `SKILL.md`；专项 `references/advanced/` |
| `qmclient-verification-gate` | gate / 构建测试 / 证据 | `SKILL.md` |
| `qmclient-git-commit` | commit / PR / Release 文案 | `SKILL.md` |
| `qmclient-code-review` | 审查立场与 findings | `SKILL.md` |
| `qmclient-i18n-workflow` | 翻译流水线 | `SKILL.md` |
| `qmclient-i18n-audit` | i18n 污染与绿线 | `SKILL.md` |
| `qmclient-codegraph-usage` | codegraph 优先 | `SKILL.md` |
| `audit-qmclient-quality` | 客户端质量审计 | `SKILL.md` + `references/quality-scan-prompt.md` |

### audit-qmclient-quality 细则

| 文件 | 内容 |
|------|------|
| `SKILL.md` | 审计模式、加载顺序、advanced 路由、模板强制、完成标准 |
| `references/quality-scan-prompt.md` | 质量扫描长提示词（第 0–5 节模板合同） |
| `agents/openai.yaml` | Codex/OMP 展示名与默认 prompt |

## 文档边界

**放进 skills**

- 跨会话稳定、agent 每次可执行的规则
- 一份 skill 一个主题；主规则整合进 `SKILL.md`
- 仅真正「按需加载、很长」的专项进 `references/`（如 advanced、质量扫描长提示词）
- 可机械化约束优先脚本 / gate

**不要放进 skills**

- 改动历史、会话交接、feature status JSON
- 与 `docs/superpowers/` 重叠的任务计划 / 探索记录

**正确放置**

| 内容 | 位置 |
|------|------|
| 可执行规则 / 工作流 | `.agents/skills/*/SKILL.md`（+ 可选 references） |
| 执行计划、验证证据 | `docs/superpowers/plans/` |
| 稳定规格 | `docs/superpowers/specs/` |
| 探索记录 | `docs/superpowers/explore/` |
| 脚本说明 | `qmclient_scripts/` |

## 用户级 skills

勿再依赖 `~/.claude/skills` 全局链路做 QmClient 任务；避免与项目 skill 抢同名。
