# 服务仓库索引

`qmclient_scripts/` 下的服务均为**独立 Git 仓库**，各自有独立 remote，可单独克隆、部署与提 issue。
主仓库通过 `.gitignore` 排除这些目录，不会把它们并入 QmClient 主仓库历史。

## 仓库一览

| 目录 | 仓库 | 语言 | 运行形态 | 用途 |
| --- | --- | --- | --- | --- |
| `qmclient_center_server/` | [wxj881027/qmclient-center-server](https://github.com/wxj881027/qmclient-center-server) | Node.js | HTTP `:8080` + WebSocket | 中心服务：识别同步、实时通道、编辑器协作、称号、新闻、游玩时长、开发者名牌 |
| `qmclient_voice_server/` | [wxj881027/qmclient-voicesrv](https://github.com/wxj881027/qmclient-voicesrv) | Rust | HTTP/WebSocket `:9987` | 语音中继（`RV01` 协议）与身份/在线同步 |
| `qmclient_titles/` | [wxj881027/qmclient-titles](https://github.com/wxj881027/qmclient-titles) | Node.js | HTTP `:19092` | 赞助者称号：兑换码、资料绑定、在场样式分配 |

## 部署约定

各仓库的 `deploy/` 下提供 systemd 单元与 nginx 片段，路径与账号按通用约定编写（`/opt/qmclient-*` + 专用系统账号），
部署到具体主机时按实际情况调整。所有密钥一律通过 `EnvironmentFile` 注入，仓库内只保留 `*.env.example` 占位符。

## 更新方式

这些目录是独立仓库的工作副本，直接在其中 `git pull` / `git push`：

```bash
cd qmclient_scripts/qmclient_center_server
git pull
```
