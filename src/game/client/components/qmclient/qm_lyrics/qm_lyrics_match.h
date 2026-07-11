#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MATCH_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MATCH_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace QmLyrics
{
	// 候选歌词来源对照查询的元数据。
	struct SMatchCandidate
	{
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album;
		std::string m_FileName; // 本地文件候选的文件名，可空。
		int m_DurationSec = 0;
	};

	// 查询条件（来自 SMTC 当前播放）。
	struct SMatchQuery
	{
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album; // 可空
		std::string m_PlayerId; // SMTC SourceAppUserModelId
		std::string m_NeteaseSongId; // 来自 SMTC Genres: NCM-...
		std::string m_QqMusicSongId; // 来自 SMTC Genres: QQ-...
		std::string m_LinkedFileName; // 来自 SMTC Genres: FILENAME-...
		int m_DurationSec = 0; // 0 表示未知
	};

	// 归一化字符串：lowercase（ASCII 部分）、删 (feat.xxx)/(cover)/[live]/-remaster*
	// 等修饰、删标点、保留中日韩汉字、ASCII 去重音（café → cafe）。
	// 返回归一化后的字符串；空输入返回空。
	std::string NormalizeForMatch(std::string_view Input);

	// Levenshtein 距离（字符级，UTF-8 安全：以 UTF-8 码点为单位）。
	int LevenshteinDistance(std::string_view A, std::string_view B);

	// 把字符串按空白拆分为 token 集合后比对相似度（0..1）。
	// 行为：A 和 B 归一化 → 分词 → set 交集大小 × 2 / (|A| + |B|)。
	// 适合艺人名顺序不同（"Pink Floyd" vs "Floyd, Pink"）的情况。
	float TokenSetRatio(std::string_view A, std::string_view B);

	// duration 容差评分，对齐 BetterLyrics：
	// 差距 <= 1s 视作 1.0，差距 >= MaxTolMs 视作 0，中间线性插值。
	// 候选时长 0 表示远端无时长数据，返回 0。
	float DurationScore(int QuerySec, int CandidateSec, int MaxTolMs = 10000);

	// 综合评分（0..100）。
	// 对齐 BetterLyrics：title=30, artist=30, album=10, duration=30。
	// 字符串字段 trim/lower 后使用 Jaro-Winkler；双方都为空视作匹配。
	// 如果查询或候选缺少标题，则回退到排序 token 指纹。
	struct SMatchWeights
	{
		float m_TitleWeight = 30.0f;
		float m_ArtistWeight = 30.0f;
		float m_AlbumWeight = 10.0f;
		float m_DurationWeight = 30.0f;
	};

	float ScoreMatch(const SMatchQuery &Query, const SMatchCandidate &Candidate, const SMatchWeights &Weights = {});

	// 整体评分（0..100）的便捷封装，使用默认 SMatchWeights。
	inline float Score(const SMatchQuery &Q, const SMatchCandidate &C)
	{
		return ScoreMatch(Q, C, SMatchWeights{});
	}

	struct SCandidateApplyRank
	{
		size_t m_Index = 0;
		float m_Score = 0.0f;
		float m_SourceScore = 0.0f;
		int m_SourceOrder = 0x7fffffff;
	};

	float CandidateApplyRankScore(const SCandidateApplyRank &Rank);
	void SortCandidateApplyRanks(std::vector<SCandidateApplyRank> *pvRanks);
	bool ShouldPublishConcurrentSearch(int PendingSources, int64_t FirstCandidateTick, int64_t NowTick, int64_t TickFreq, int GraceMs);

} // namespace QmLyrics

#endif
