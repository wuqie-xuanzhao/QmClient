---
name: qmclient-codegraph-usage
description: 在 QmClient 项目中使用 codegraph CLI/MCP 做结构化代码查询（调用关系、定义、影响面），以及何时该用 codegraph 而非 grep/read
---

# QmClient codegraph 使用指南

## 适用场景
在 QmClient 项目中做代码结构查询（调用关系、定义位置、影响面、签名）时，优先用 codegraph 而不是 grep/read 循环。

## 安装与 MCP
- CLI: `D:\Scoop\apps\nodejs-lts\current\bin\codegraph`（PATH 可直接 `codegraph`）
- 索引目录: 仓库根 `.codegraph/`（约 200MB SQLite）
- OMP MCP 配置: `~/.omp/agent/mcp.json` 中 `codegraph` 必须指向存在的二进制：
  - 正确: `D:\Scoop\apps\nodejs-lts\current\bin\codegraph.cmd`
  - args: `["serve", "--mcp", "--no-watch"]`
  - 错误历史: `C:\Users\11054\AppData\Local\codegraph\current\bin\codegraph.cmd`（已不存在）→ `Transport closed`
- 改 `mcp.json` 后需**重启 OMP 会话**才会加载 `codegraph_*` 工具
- 无默认项目时工具仍可用，调用时传 `projectPath`（如 `E:\Coding\DDNet\QmClient`）

## 常用 CLI 命令

| 问题 | 命令 |
|---|---|
| 索引健康/pending | `codegraph status` |
| 同步自上次索引的变更 | `codegraph sync` |
| 按名称搜符号 | `codegraph query search <name> -k <kind> -l <limit> -j` |
| 查调用者 | `codegraph callers <symbol>` |
| 查被调用者 | `codegraph callees <symbol>` |
| 修改某符号的影响面 | `codegraph impact <symbol>` |
| 受源文件变更影响的测试 | `codegraph affected [files...]` |
| 项目文件结构 | `codegraph files` |

## 何时用 codegraph vs grep/read

| 问题 | 工具 |
|---|---|
| "X 定义在哪？" / "找名为 X 的符号" | codegraph search |
| "谁调用了 Y？" | codegraph callers |
| "Y 调用了什么？" | codegraph callees |
| "改 Z 会影响什么？" | codegraph impact |
| "Y 的签名/源码/docstring" | codegraph query / node |
| "某个字符串/注释/日志内容" | grep/search（codegraph 不索引文本内容） |

## 重要约束
- **有 pending changes 先 sync**：`codegraph status` 显示 pending files 时先 `codegraph sync`
- **function/method/class 索引最完整**；成员变量/字段名可能搜不到，用文本搜索补充
- **不要 grep 后再用 codegraph 重建路径**；信任 AST 结果

## 常见坑
- 索引 lag：status/banner 提示文件被编辑过 → 对这些文件用 `read`，其余信 codegraph
- `.codegraph/` 不存在 → `codegraph init -i`
