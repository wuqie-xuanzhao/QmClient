---
status: active
date_updated: 2026-07-11
scope: QmLiveClient 人工验收
---

# QmLiveClient 验收手册

本文只记录自动化测试无法替代的人工验收。实现行为与回归断言以当前源码和 `src/test/qm_live_client_test.cpp` 为准。

## 自动化基线

Windows 最终验收运行完整 C++ 测试入口：

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

Linux/macOS：

```sh
cmake --build cmake-build-release --target run_cxx_tests -j 14
```

仅定位 QmLive 问题时可以运行过滤测试；过滤结果不能代替最终全量测试：

```powershell
cmake-build-release/testrunner.exe --gtest_filter=QmLive*
```

## 导播与兼容模式

1. 启动启用了 `sv_qm_live_observer 1` 的 `QmLiveServer`，再用 `QmLiveClient` 连接。
2. 确认客户端以只读 observer 进入，不占玩家名额；直播画面不显示普通排名、武器 HUD、右下旁观文字或 dummy 操作入口。
3. 打开控制台执行 `dummy_connect`，确认不会建立 dummy 连接，并出现 `Dummy connection is disabled for QmLive director.` 拒绝日志。
4. 准备至少两个有效 DDRace 队伍，检查队伍条人数、展开成员、单人跟随、多人同框、滚动和切队；未选队伍保持低透明度可见。
5. 按住左键移动临时自由镜头，松开后应吸附到镜头附近的有效队伍。队伍瞬间失效时应稳定回退，不选择无效玩家。
6. 在菜单、聊天、控制台、旁观者弹窗和表情弹窗打开时点击，确认不会误触自由镜头；点击导播面板只执行面板操作。
7. 连接普通 DDNet 服务端，确认服务端不支持 live observer 时仍保持连接并进入兼容导播；进入 spectator 前不得发出移动、武器、聊天、kill、dummy 或加入队伍等玩法输入。
8. 连接没有 DDRace team 数据的服务端，确认导播回退到全局玩家列表，点击玩家可按普通 spectator 流程跟随。

## 录制与回放

1. 使用 `qm_live_match_record_start` / `qm_live_match_record_stop` 完成一段比赛录制，确认 `.demo` 写入 `demos/qm_live/matches/` 且可由标准 demo 播放器打开。
2. 播放录制内容并执行 seek/restart，确认完成排名和队伍上下文按当前时间重建，不残留跳转前状态。
3. 使用 `qm_live_team_filter <team>` 和 `qm_live_team_filter_off` 切换单队预览，确认画面、音效和完成提示遵循当前过滤配置；关闭过滤后恢复完整比赛。
4. 缺失、损坏或与 demo 不匹配的 `.qmlive.json` sidecar 不应阻止标准 demo 播放。

## 长时间与直播输出

1. 在真实比赛服务器连续运行至少 30 分钟，覆盖加入、死亡、重生、换队和当前队伍消失；队伍条持续更新，主视角不无故跳队。
2. 比赛中通过换图投票，确认新地图自动下载/载入，换图后 observer/兼容导播状态和队伍或玩家列表恢复。
3. 用 OBS/直播姬捕获窗口，确认输出只包含预期的比赛画面、自由镜头状态和导播 UI，无普通客户端 HUD 或无关文案。
4. 同时运行普通 QmClient 与 QmLiveClient，确认普通客户端和 `QmLiveServer` 上的普通玩家仍可正常使用 HUD、聊天、dummy、投票、换队和玩法输入。

人工验收结果应记录环境、服务端类型、持续时间和失败截图/日志；未执行的项目必须作为 gap 汇报。
