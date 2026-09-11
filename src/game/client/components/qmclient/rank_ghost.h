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
	void OnReset() override;

	// 供菜单按钮调用：请求当前地图的官方 rank 影子
	void RequestCurrentMapGhost(int Rank = 1);

	// rank 影子统一存放在 ghosts/rank_ghost 子目录，Ghost 页列表据此识别这些条目
	static constexpr const char *GHOST_ROOT = "ghosts";
	static constexpr const char *GHOST_SUBDIR = "rank_ghost";

private:
	// watchable.jsonl 中的一条预生成回放记录
	struct SEntry
	{
		std::string m_Map;
		int m_Rank = 0;
		std::string m_Time; // 完成时间（秒），保留服务端字符串形式
		std::string m_Demo; // 回放文件名（服务端以 .demo.gz 命名，实际传输已解压）
		std::string m_Names; // 完成玩家，逗号分隔
		int m_Cid = 0; // 录制时完成玩家的 client id
		int64_t m_Ts = 0; // 完成时间戳，用于同名次多条记录时取最新
	};

	enum class EStage
	{
		IDLE,
		FETCH_MANIFEST,
		FETCH_DEMO,
		PARSE,
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
	// 命令可能在 autoexec / 客户端初始化早期执行，此时不触碰网络与聊天组件，
	// 只登记请求，实际任务在主循环里启动
	bool m_StartPending = false;
	bool m_UnloadPending = false;
	std::string m_PendingNotify;

	// 索引
	std::vector<SEntry> m_vEntries;
	bool m_ManifestLoaded = false;
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
	void LookupInManifest();

	void StartManifestFetch();
	void StartDemoFetch();
	void StartParse();
	void FinishParse();

	void UpdateManifestStage();
	void UpdateDemoStage();
	void UpdateParseStage();

	bool ParseManifest(const unsigned char *pData, size_t DataSize);
	const SEntry *FindEntry(const char *pMap, int Rank) const;

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
