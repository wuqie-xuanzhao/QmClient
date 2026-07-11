# QmClient 中心 HTTP 服务

此目录包含一个独立的 Node.js/Express 服务，负责版本查询、健康检查、游玩时长、编辑器协作，以及可选的在线识别兼容接口。实际契约以 `server.js` 中注册的路由为准。

## 启动

```bash
cd qmclient_scripts/qmclient_center_server
npm install
AUTH_SECRET="replace-with-random-long-secret" PORT=8080 npm start
```

生产环境必须显式设置并持久化 `AUTH_SECRET`。未设置时每次启动都会生成随机值，已有 token 会立即失效。

## 客户端与部署边界

当前客户端把服务拆成两组：

- 中心 HTTP 服务默认位于 `42.194.185.210:8080`。当前客户端直接访问 `/healthz` 和 `/playtime/*`；服务端另提供 `/client/version`，客户端保留对应的 `TCLIENT_INFO_URL` 常量。这些路径都不带 `/qm` 前缀。
- 在线识别、远程粒子和语音在线状态使用 `qm_voice_server` 指定的语音服务，默认位于 `42.194.185.210:9987`。客户端访问 `/qm/token`、`/qm/report` 和 `/qm/users.json`。

本目录的 Express 服务原生注册识别兼容路由 `/token`、`/report` 和 `/users.json`，不原生注册 `/qm/*`。如果部署时让本服务承接客户端的识别请求，反向代理必须进行以下映射，或由上游语音服务实现相同契约：

| 客户端路径 | Express 上游路径 |
|---|---|
| `/qm/token` | `/token` |
| `/qm/report` | `/report` |
| `/qm/users.json` | `/users.json` |

不要在没有路径映射的情况下把 `qm_voice_server` 直接指向本服务端口。修改固定中心 HTTP 地址时，检查 `src/game/client/components/qmclient/qmclient.cpp` 中的 `TCLIENT_INFO_URL`、`QMCLIENT_HEALTH_URL` 和 `QMCLIENT_PLAYTIME_*_URL`；修改识别服务地址时使用配置项 `qm_voice_server`。

## 路由

| 方法 | 路径 | 用途 |
|---|---|---|
| `GET` | `/healthz` | 健康检查与客户端时间同步 |
| `GET` | `/client/version?current=<version>` | 查询 GitHub 最新 release，并比较客户端版本 |
| `GET` | `/token` | 获取短期识别 token |
| `POST` | `/report` | 上报当前服务器上的本地玩家状态 |
| `GET` | `/users.json` | 获取仍在有效期内的在线识别记录 |
| `POST` | `/playtime/start` | 开始或恢复游玩时长会话 |
| `POST` | `/playtime/stop` | 停止会话并结算时长 |
| `POST` | `/playtime/query` | 查询累计与当前会话时长 |
| `POST` | `/editor/collab/create` | 创建编辑器协作房间 |
| `POST` | `/editor/collab/join` | 加入编辑器协作房间 |
| `POST` | `/editor/collab/leave` | 离开编辑器协作房间 |
| `POST` | `/editor/collab/push` | 上传新的地图修订 |
| `GET` | `/editor/collab/pull` | 拉取房间状态和新修订 |

识别上报的主体结构为：

```json
{
  "server_address": "1.2.3.4:8303",
  "auth_token": "token-from-get-token",
  "client_type": "qm",
  "machine_hash": "qm314a5af9fb19ffc659077aa05e4a2689",
  "timestamp": 1739436900,
  "players": [
    {
      "player_name": "Q1menG",
      "dummy": false,
      "foot_particles_enabled": true,
      "remote_particles_enabled": true,
      "voice_supported": true
    }
  ]
}
```

`client_type` 支持 `qm` / `arg`，并兼容 `qmclient` / `arghena` 别名。服务端以玩家名为优先身份键，并继续接受合法的 `player_id` 兼容输入。

## 环境变量

| 变量 | 默认值 | 说明 |
|---|---|---|
| `PORT` | `8080` | HTTP 监听端口 |
| `AUTH_SECRET` | 随机值 | token 签名密钥；生产环境必须固定 |
| `TOKEN_TTL_SEC` | `300` | token 有效期 |
| `REPORT_TTL_SEC` | `90` | 在线记录有效期 |
| `TIME_SKEW_SEC` | `600` | 上报时间允许偏差 |
| `RATE_LIMIT_PER_MIN` | `120` | 单 IP 每分钟请求上限 |
| `REQUIRE_IP_BIND` | `1` | 是否把 token 绑定到请求 IP |
| `TRUST_PROXY` | `0` | 是否信任 Express 反向代理地址 |
| `MAX_PLAYERS_PER_REPORT` | `8` | 单次上报玩家数上限 |
| `MAX_SERVER_ADDRESS_LEN` | `128` | 服务端地址长度上限 |
| `MAX_CLIENT_ID_LEN` | `64` | 客户端标识长度上限 |
| `MAX_PLAYER_NAME_LEN` | `32` | 玩家名长度上限 |
| `PLAYTIME_DB_FILE` | `playtime_db.json` | 游玩时长持久化文件 |
| `CLIENT_RELEASE_OWNER` | `wxj881027` | GitHub release 仓库所有者 |
| `CLIENT_RELEASE_REPO` | `QmClient` | GitHub release 仓库名 |
| `CLIENT_LATEST_VERSION` | `2.36.0` | GitHub 不可用时的回退版本 |
| `CLIENT_RELEASES_API_URL` | GitHub latest release API | 自定义 release 查询地址 |
| `CLIENT_VERSION_CACHE_TTL_SEC` | `300` | 成功查询缓存时间 |
| `CLIENT_VERSION_RETRY_DELAY_SEC` | `60` | 查询失败后的重试间隔 |
| `CLIENT_VERSION_FETCH_TIMEOUT_MS` | `5000` | GitHub 请求超时 |
| `CLIENT_LATEST_TAG` | `v${CLIENT_LATEST_VERSION}` | 回退 tag |
| `CLIENT_RELEASE_URL` | 对应回退 tag 的 GitHub URL | 回退下载页 |
| `EDITOR_COLLAB_MEMBER_TTL_SEC` | `45` | 协作成员有效期 |
| `EDITOR_COLLAB_ROOM_TTL_SEC` | `300` | 空闲房间有效期 |
| `EDITOR_COLLAB_MAX_MAP_BASE64_LEN` | `25165824` | 地图 Base64 最大长度 |

## 生产部署

- 放在 HTTPS 反向代理后，并根据代理拓扑正确配置 `TRUST_PROXY`。
- 持久化 `AUTH_SECRET` 和 `PLAYTIME_DB_FILE`，限制数据库文件访问权限。
- 当前 token、在线用户和协作房间保存在单进程内存中；多实例部署前必须改用共享存储或保证会话粘滞。
- 为 `/editor/collab/*` 设置与 32 MiB 请求体相匹配的代理限制，并限制公网访问频率。
