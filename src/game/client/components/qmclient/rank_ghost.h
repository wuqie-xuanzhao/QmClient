// 官方 Rank 影子：从 ddnet.org/watch 的预生成回放里提取指定名次玩家的轨迹，
// 转换成本地 ghost 后加载渲染，让玩家正常跑图时跟随对照（不打断游戏连接）。
//
// 数据来源与官方 watch 页面（https://ddnet.org/watch/）一致：
//   https://ddnet.org/watch/watchable.jsonl   预生成回放索引（地图 + 名次）
//   https://ddnet.org/watch/demos/<file>      回放文件（标准 demo，内嵌地图）
//
// 处理流程：下载索引 -> 按地图/名次查找 -> 下载回放 -> 顺序解析 run 区间轨迹
//           -> 写 ghost 文件 -> 交给 CGhost 加载（出发时自动播放）。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_RANK_GHOST_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_RANK_GHOST_H

#include "rank_demo_manifest.h"

#include <engine/console.h>
#include <engine/http.h>
#include <engine/shared/demo.h>

#include <game/client/component.h>

#include <memory>
#include <string>
#include <vector>

class CRankGhost : public CComponent, private CDemoPlayer::IListener
{
public:
	CRankGhost();
	~CRankGhost() override;

	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnUpdate() override;
	void OnMapLoad() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnGhostsUnloaded();
	void OnGhostLoaded(const char *pStoragePath, int Slot);
	void OnGhostUnloaded(int Slot);

	// 供菜单按钮调用：请求当前地图的官方 rank 影子
	void RequestCurrentMapGhost(int Rank = 1);

	// ===== Rank 1 页面接口 =====
	// 清单加载状态（页面据此显示加载中/失败）
	enum class EManifestState
	{
		UNKNOWN,
		LOADING,
		READY,
		FAILED,
	};
	EManifestState ManifestState() const;
	// 幂等：清单缺失或过期时自动拉取（不打断进行中的任务）
	void EnsureManifest();
	// 强制重新拉取清单（显式刷新操作，可打断当前任务）
	void RefreshManifest();
	// 收集指定地图与名次的条目：同一 demo 的多条成员记录合并为一条，solo 在前
	std::vector<qmclient::rank_demo::SEntry> CollectRankEntries(const char *pMap, int Rank) const;
	// 按 manifest 的 demo 文件名请求：下载并转换为影子（不退出服务器）
	void RequestGhostForDemo(const char *pDemoName);
	// 卸载当前加载的 rank 影子（qm_rank_ghost_off 的编程接口）
	void RequestGhostOff();
	// 按 manifest 的 demo 文件名仅下载回放到缓存（供回放播放/分享）
	void RequestDemoDownload(const char *pDemoName);
	// 影子/下载任务是否进行中
	bool IsBusy() const;
	// 条目缓存状态查询（路径出参返回可读缓存路径）
	bool IsEntryDemoCached(const qmclient::rank_demo::SEntry &Entry, char *pDemoPath, size_t DemoPathSize) const;
	bool IsEntryGhostCached(const qmclient::rank_demo::SEntry &Entry, char *pGhostPath, size_t GhostPathSize) const;
	// 该条目的影子是否已加载激活
	bool IsEntryGhostActive(const qmclient::rank_demo::SEntry &Entry) const;
	// 删除该条目的 demo 与 ghost 缓存（激活中的影子会先卸载）
	void DeleteEntryCache(const qmclient::rank_demo::SEntry &Entry);
	// 由条目构建可读的缓存路径：<map>_rank<N>_<kind>_<time>s_<uuid8>.demo/.gho
	static void BuildEntryCachePaths(const qmclient::rank_demo::SEntry &Entry, char *pDemoPath, size_t DemoPathSize, char *pGhostPath, size_t GhostPathSize);

	// rank 影子统一存放在 ghosts/rank_ghost 子目录，Ghost 页列表据此识别这些条目
	static constexpr const char *GHOST_ROOT = "ghosts";
	static constexpr const char *GHOST_SUBDIR = "rank_ghost";

private:
	// watchable.jsonl 中的一条预生成回放记录（解析实现与 demo 浏览器共用）
	using SEntry = qmclient::rank_demo::SEntry;

	enum class EStage
	{
		IDLE,
		FETCH_MANIFEST,
		FETCH_DEMO,
		PARSE,
		// 仅下载回放（Rank 1 页面的“下载回放”动作），完成后不解析
		FETCH_DEMO_ONLY,
	};

	// 本次请求的目标：按地图名次自动挑选，或按 manifest 的 demo 名精确匹配
	enum class EPendingMode
	{
		GHOST,
		DEMO_ONLY,
	};

	// 解析期间的状态，定义在 cpp（避免头文件依赖 ghost/snapshot 数据结构）
	struct SParseState;

	static constexpr const char *MANIFEST_URL = "https://ddnet.org/watch/watchable.jsonl";
	static constexpr const char *DEMO_URL_PREFIX = "https://ddnet.org/watch/demos";
	static constexpr const char *DEMO_CACHE_DIR = "demos/rank_ghost";
	// 每帧最多推进的 demo tick 数，避免长时间阻塞渲染
	static constexpr int PARSE_TICKS_PER_FRAME = 2500;

	// 本次请求
	std::string m_PendingMap;
	int m_PendingRank = 1;
	// 按 manifest 的 demo 名精确请求时非空（优先于地图名次匹配）
	std::string m_PendingDemo;
	EPendingMode m_PendingMode = EPendingMode::GHOST;
	// 命令可能在 autoexec / 客户端初始化早期执行，此时不触碰网络与聊天组件，
	// 只登记请求，实际任务在主循环里启动
	bool m_StartPending = false;
	bool m_UnloadPending = false;
	std::string m_PendingNotify;

	// 索引
	std::vector<SEntry> m_vEntries;
	bool m_ManifestLoaded = false;
	int64_t m_ManifestLoadedAt = 0;
	bool m_ManifestFailed = false;
	int64_t m_ManifestFailedAt = 0;
	std::shared_ptr<IHttpRequest> m_pManifestRequest;

	// 当前阶段输入
	SEntry m_ActiveEntry;
	std::shared_ptr<IHttpRequest> m_pDemoRequest;
	char m_aDemoStoragePath[IO_MAX_PATH_LENGTH] = "";
	char m_aGhostStoragePath[IO_MAX_PATH_LENGTH] = "";

	EStage m_Stage = EStage::IDLE;
	std::unique_ptr<SParseState> m_pParse;

	// 已加载的 ghost 槽位
	int m_LoadedSlot = -1;
	// 影子已按哪一次跑图（LastRaceTick）对齐过；-1 表示尚未对齐
	int m_LastAlignedRaceTick = -1;
	// ghost 已缓存但地图还没加载时，等待地图出现再加载
	bool m_RetryLoadPending = false;
	int64_t m_RetryLoadDeadline = 0;
	int64_t m_RetryLoadNextAttempt = 0;

	static void ConRankGhost(IConsole::IResult *pResult, void *pUserData);
	static void ConRankGhostOff(IConsole::IResult *pResult, void *pUserData);

	void StartLookup(const char *pMap, int Rank);
	void StartPendingLookup(const char *pDemoName, EPendingMode Mode);
	void LookupInManifest();

	void StartManifestFetch();
	void StartDemoFetch();
	void StartParse();
	void FinishParse();

	void UpdateManifestStage();
	void UpdateDemoStage();
	void UpdateParseStage();

	bool ParseManifest(const unsigned char *pData, size_t DataSize);

	bool LoadGhostFile(const char *pStoragePath);
	void UnloadGhost();

	// 加载后把 Ghost 页列表刷新一遍，并关联我们占用的槽位
	void RefreshGhostList();
	// 玩家已在跑图时，让影子立即按当前进度对齐播放；此后每次重新出发都会重新对齐
	void AlignToCurrentRun();
	void NotifyLoaded(const char *pOwner, const char *pTimeText);

	void Fail(const char *pMessage);
	void AbortTask();
	void Echo(const char *pMessage) const;

	// CDemoPlayer::IListener
	void OnDemoPlayerSnapshot(void *pData, int Size) override;
	void OnDemoPlayerMessage(void *pData, int Size) override;
};

#endif
