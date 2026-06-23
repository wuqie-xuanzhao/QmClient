#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_PARSER_LRC_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_PARSER_LRC_H

#include "qm_lyrics_model.h"

#include <cstddef>
#include <string>

namespace QmLyrics
{

	// 解析标准 LRC、增强 LRC 或 ESLRC 格式。
	//
	// 标准 LRC：`[mm:ss.xx]文本`，多个时间戳前缀代表该行在多个时间点出现（摊开为多行）。
	//   元数据 `[ar:][ti:][al:][by:][offset:]`，offset 为毫秒级整体偏移。
	// 增强 LRC：行首 `[mm:ss.xx]`，行中 `<mm:ss.xx>词` 给词级时间。
	// ESLRC：与增强 LRC 同构。
	//
	// 返回的 SLyricsTrack 中：
	// - m_Format 自动识别为 LRC_STANDARD / LRC_ENHANCED（ESLRC 不可静态区分，按 LRC_ENHANCED 处理）
	// - m_vLines 按 StartMs 升序
	// - 每行 m_EndMs = 下一行 StartMs；最后一行用最大词 EndMs 或 StartMs + 5000ms 兜底
	// - m_Title / m_Artist / m_Album / m_OffsetMs 从元数据 tag 填入
	//
	// 返回 false 表示无可解析的行（输入为空、全为元数据或全为无效行）。
	// pErr 收到一行简短诊断（最多 ErrSize-1 个字符），错误时设置；成功时清空首字节。
	bool ParseLrc(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr = nullptr, size_t ErrSize = 0);

} // namespace QmLyrics

#endif
