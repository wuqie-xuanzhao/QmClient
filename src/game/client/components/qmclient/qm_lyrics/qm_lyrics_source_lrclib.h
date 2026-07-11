#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LRCLIB_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LRCLIB_H

#include "qm_lyrics_source.h"

#include <memory>
#include <string>

class IHttp;

namespace QmLyrics
{

	// 构造 LRCLIB GET URL：https://lrclib.net/api/get?artist_name=...&track_name=...&album_name=...&duration=N
	// duration<=0 时不附 duration 参数。返回完整 URL（含 URL 编码）。
	std::string BuildLrclibGetUrl(const SSourceQuery &Query, const char *pBaseUrl = "https://lrclib.net");
	bool CanUseLrclibGet(const SSourceQuery &Query);

	// 构造 LRCLIB 搜索 URL：https://lrclib.net/api/search?track_name=...&artist_name=...&album_name=...
	std::string BuildLrclibSearchUrl(const SSourceQuery &Query, const char *pBaseUrl = "https://lrclib.net");
	bool ShouldFallbackLrclibGet(int StatusCode, bool HasParsedCandidate);
	int EffectiveLrclibHttpTimeoutMs(int TimeoutMs, const char *pProxy);
	bool LrclibHttpOptionsChanged(int PreviousTimeoutMs, const char *pPreviousProxy, int TimeoutMs, const char *pProxy);

	// 解析 /api/get 单条响应 JSON → 0..1 个候选。
	// 200 + 非空 syncedLyrics 视为一条候选；syncedLyrics 空但 plainLyrics 非空视为无时间戳候选。
	// 404 / 空 body / 解析失败 → 返回空 vector，pErr 收到诊断。
	std::vector<SSourceCandidate> ParseLrclibGetResponse(const char *pBody, size_t BodyLen, char *pErr = nullptr, size_t ErrSize = 0);

	// 解析 /api/search 列表响应 JSON → 0..N 个候选（不按评分排序，由上层 Score 排）。
	std::vector<SSourceCandidate> ParseLrclibSearchResponse(const char *pBody, size_t BodyLen, char *pErr = nullptr, size_t ErrSize = 0);

	// LRCLIB 数据源：先尝试 /api/get，无果再 /api/search。
	// 需注入 IHttp 实例 + HTTP 超时（毫秒）；不持有所有权。
	class CLyricsSourceLrclib : public IQmLyricsSource
	{
	public:
		CLyricsSourceLrclib(IHttp *pHttp, int TimeoutMs = 8000, const char *pBaseUrl = "https://lrclib.net", const char *pProxy = "");
		~CLyricsSourceLrclib() override;

		const char *Id() const override { return "lrclib"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;
		void UpdateHttpOptions(int TimeoutMs, const char *pProxy);

		// 测试钩子：暴露当前 in-flight 请求 done 状态，单测用。
		bool BusyForTests() const;
		int EffectiveTimeoutMsForTests() const;

		// 内部状态结构（public 是因为同 .cpp 中的匿名 namespace helper 需要访问；
		// 不要在 .h 之外直接用）。
		struct SImpl;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
