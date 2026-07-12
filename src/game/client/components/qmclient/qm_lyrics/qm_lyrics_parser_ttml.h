// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_PARSER_TTML_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_PARSER_TTML_H

#include "qm_lyrics_model.h"

#include <cstddef>

namespace QmLyrics
{

	// 解析 TTML 歌词（Apple Music 和 LRCLIB 偶尔返回的格式）。
	//
	// 结构：<tt><body><div><p begin="..." end="...">[<span begin="..." end="...">词</span>]…</p>…
	//
	// 时间格式（必须支持三种）：
	// - `mm:ss.fff`（如 `01:23.456` 或 `0:01:23.456`）
	// - `Ss` 浮点秒（如 `83.456s`）
	// - `Sms` 毫秒（如 `83456ms`）
	//
	// 角色（`ttm:role` 或 `role`）：
	// - `x-translation` → 写入对应行 m_Translation
	// - `x-transliteration` → 写入对应行 m_Transliteration
	//
	// 返回 false 表示 XML 无效或没有可解析的 <p>。
	// 简化：不支持 SMIL 嵌入、外部 stylesheet；只处理纯文本和 <span>。
	bool ParseTtml(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	// TTML 时间字符串解析（公开作为单元测试入口）。
	// 支持三种格式；成功返回 true 并填 *pOutMs，失败返回 false。
	bool ParseTtmlTime(const char *pStr, int64_t *pOutMs);

} // namespace QmLyrics

#endif
