#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_KUGOU_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_KUGOU_H

#include "qm_lyrics_source.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class IHttp;

namespace QmLyrics
{

	// 三个阶段的 URL 构造（路径根据 Lyricify-Lyrics-Helper 协议归纳）。
	//
	// 一阶段：mobilecdn.kugou.com 搜索曲目，拿 FileHash。
	std::string BuildKugouSearchUrl(const SMatchQuery &Query);
	//
	// 二阶段：lyrics.kugou.com 用 hash 查歌词候选，拿 id + accesskey。
	std::string BuildKugouLyricSearchUrl(const std::string &FileHash, int DurationMs);
	//
	// 三阶段：lyrics.kugou.com 下载 KRC base64。
	std::string BuildKugouLyricDownloadUrl(const std::string &Id, const std::string &AccessKey);

	// 解析一阶段响应。返回最高匹配分的 (FileHash, DurationMs)；找不到返回 false。
	struct SKugouSearchHit
	{
		std::string m_FileHash;
		int m_DurationMs = 0;
		std::string m_Title;
		std::string m_Artist;
	};
	bool ParseKugouSearchResponse(const char *pBody, size_t BodyLen,
		const SMatchQuery &Query,
		SKugouSearchHit *pOut,
		char *pErr = nullptr, size_t ErrSize = 0);

	// 解析二阶段响应。取 score 最高的 candidate。
	struct SKugouLyricCandidate
	{
		std::string m_Id;
		std::string m_AccessKey;
	};
	bool ParseKugouLyricSearchResponse(const char *pBody, size_t BodyLen,
		SKugouLyricCandidate *pOut,
		char *pErr = nullptr, size_t ErrSize = 0);

	// 解析三阶段响应，base64 decode + KRC 解密 + zlib inflate → enhanced KRC 文本。
	bool ParseKugouLyricDownloadResponse(const char *pBody, size_t BodyLen,
		std::string *pOutKrcText,
		char *pErr = nullptr, size_t ErrSize = 0);

	// KRC 文本格式解析为 SLyricsTrack。
	// KRC 格式：每行 `[start,dur]<offset,dur,0>词<offset,dur,0>词`，时间单位 ms。
	bool ParseKrcText(const char *pText, size_t TextSize, SLyricsTrack *pOut,
		char *pErr = nullptr, size_t ErrSize = 0);

	// 酷狗数据源：Search → LyricSearch → KRC download。
	class CLyricsSourceKugou : public IQmLyricsSource
	{
	public:
		CLyricsSourceKugou(IHttp *pHttp, int TimeoutMs = 8000);
		~CLyricsSourceKugou() override;

		const char *Id() const override { return "kugou"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

		// 内部状态结构只给同 .cpp 的 helper 使用。
		struct SImpl;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
