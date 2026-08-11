---
name: qmclient-codegraph-usage
description: 在 QmClient 用 codegraph CLI/MCP 做结构化查询（定义、调用关系、影响面）；何时优先于 grep/read。
---

# QmClient codegraph 用法

## 何时使用
查定义、调用者/被调用者、影响面、签名时，优先 codegraph，避免 grep + 通读循环。

## 环境
- CLI：PATH 中的 `codegraph`（Scoop 常见路径 `D:\Scoop\apps\nodejs-lts\current\bin\codegraph`）
- 索引：仓库根 `.codegraph/`
- OMP MCP：`~/.omp/agent/mcp.json` 指向真实 `codegraph.cmd`，args 为 `serve --mcp --no-watch`
- 改 MCP 配置后需**重启 OMP 会话**
- 无默认项目时调用带 `projectPath`（如 `E:\Coding\DDNet\QmClient`）

## 常用命令
| 问题 | 命令 |
|------|------|
| 索引健康 / pending | `codegraph status` |
| 同步变更 | `codegraph sync` |
| 按名搜符号 | `codegraph query search <name>` |
| 谁调用了 X | `codegraph callers <symbol>` |
| X 调用了谁 | `codegraph callees <symbol>` |
| 改 X 的影响面 | `codegraph impact <symbol>` |
| 受影响测试 | `codegraph affected [files...]` |

## 选型
| 问题 | 工具 |
|------|------|
| 定义 / 调用 / 影响面 / 签名 | codegraph |
| 字符串、注释、日志原文 | grep / search |

## 约束
- 有 pending 先 `codegraph sync`  
- function / method / class 索引最完整；成员字段可能需文本搜索  
- 信任 AST 结果，不要用 grep 再「验证」整条调用链  

## 常见坑
- 索引滞后：banner 点名的文件用 `read`，其余信 codegraph  
- 无 `.codegraph/`：在项目根 `codegraph init -i`  
