// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CACHE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_CACHE_H

#include "qm_lyrics_match.h"
#include "qm_lyrics_model.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class IStorage;

namespace QmLyrics
{

	// 缓存条目元数据（不含歌词正文，正文存独立文件）。
	struct SCacheEntry
	{
		std::string m_Key; // 规范化后的查询键
		std::string m_FileName; // 子目录下的相对文件名，如 "abcd1234.json"
		std::string m_Source; // "lrclib" / "qq" / "netease" 等
		float m_Score = 0.0f; // 当时的匹配评分（0..100）
		int64_t m_LastUsedAt = 0; // unix 秒
		int64_t m_StoredAt = 0; // unix 秒（TTL 用）
	};

	// 全部条目索引（内存形态）。
	class CCacheIndex
	{
	public:
		// LRU 上限：超出时按 m_LastUsedAt 升序淘汰，返回被淘汰的文件名列表。
		std::vector<std::string> Upsert(const SCacheEntry &Entry, int MaxEntries);

		// 查找；命中时更新 m_LastUsedAt 为 NowUnixSec 并返回条目指针；未命中返回 nullptr。
		const SCacheEntry *Lookup(std::string_view Key, int64_t NowUnixSec);
		const SCacheEntry *Find(std::string_view Key) const;

		// 按 key 删除条目；如果需要清理 payload，pFileName 接收原文件名。
		bool Remove(std::string_view Key, std::string *pFileName = nullptr);

		// 删除过期条目（StoredAt + TtlDays 早于 NowUnixSec）。TtlDays<=0 视作永不过期。
		// 返回被淘汰的文件名列表。
		std::vector<std::string> EvictExpired(int TtlDays, int64_t NowUnixSec);

		size_t Size() const { return m_mEntries.size(); }
		const std::unordered_map<std::string, SCacheEntry> &All() const { return m_mEntries; }
		void Clear() { m_mEntries.clear(); }

		// 序列化为索引 JSON 字符串（紧凑）。
		std::string ToJson() const;

		// 反序列化。失败返回 false，pErr 收到诊断。成功时索引被替换。
		bool FromJson(std::string_view Json, char *pErr = nullptr, size_t ErrSize = 0);

	private:
		std::unordered_map<std::string, SCacheEntry> m_mEntries;
	};

	// 由 (title, artist, album, duration) 算缓存 key 和文件名。
	// Key：归一化后的字段用 '|' 拼接。
	// FileNameForKey 是旧版 sha256 JSON payload 文件名，仅用于向后兼容。
	std::string BuildCacheKey(std::string_view Title, std::string_view Artist, std::string_view Album, int DurationSec);
	std::string FileNameForKey(std::string_view Key);
	// 新版缓存为真实 LRC 正文 + 同名 .meta.json。只在 Collision 时附加短 hash。
	std::string FileNameForTrack(std::string_view Title, std::string_view Artist, std::string_view Key, bool Collision = false);
	std::string ChooseCachePayloadFileName(const CCacheIndex &Index, std::string_view Title, std::string_view Artist, std::string_view Key, std::string_view IgnoreKey = {});
	bool IsValidCachePayloadFileName(std::string_view FileName);

	struct SCachePayload
	{
		std::string m_RawText;
		std::string m_TranslationText;
		std::string m_TransliterationText;
		EFormat m_Format = EFormat::LRC_STANDARD;
		SMatchCandidate m_Metadata;
		std::string m_Source;
	};

	bool LoadCacheIndex(IStorage *pStorage, CCacheIndex *pOut);
	bool SaveCacheIndex(IStorage *pStorage, const CCacheIndex &Index);
	bool LoadCachePayload(IStorage *pStorage, const char *pFileName, SCachePayload *pOut);
	bool SaveCachePayload(IStorage *pStorage, const char *pFileName, const SCachePayload &Payload);
	void RemoveCachePayload(IStorage *pStorage, const char *pFileName);
	bool CommitCacheEntry(IStorage *pStorage, CCacheIndex *pIndex, const SCacheEntry &Entry, const SCachePayload &Payload, int MaxEntries);
	bool MigrateLegacyCacheEntry(IStorage *pStorage, CCacheIndex *pIndex, std::string_view LegacyKey, std::string_view TrackKey, std::string_view Title, std::string_view Artist, int MaxEntries);

} // namespace QmLyrics

#endif
