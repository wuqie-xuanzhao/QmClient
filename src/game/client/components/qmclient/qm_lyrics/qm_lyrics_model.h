#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MODEL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MODEL_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace QmLyrics
{

	struct SLyricsWord
	{
		int64_t m_StartMs = 0;
		int64_t m_EndMs = 0;
		std::string m_Text;
	};

	struct SLineTextWidthCache
	{
		bool m_Valid = false;
		float m_FontSize = 0.0f;
		size_t m_ContextHash = 0;
		size_t m_TextHash = 0;
		float m_RawTextWidth = 0.0f;
		float m_SpaceWidth = 0.0f;
		std::vector<float> m_vWordWidths;
	};

	struct STextWidthCache
	{
		bool m_Valid = false;
		float m_FontSize = 0.0f;
		size_t m_ContextHash = 0;
		size_t m_TextHash = 0;
		float m_TextWidth = 0.0f;
	};

	struct SLyricsLine
	{
		int64_t m_StartMs = 0;
		int64_t m_EndMs = 0; // 下一行 StartMs 或最后一个词 EndMs
		std::string m_RawText;
		std::vector<SLyricsWord> m_vWords; // 行级时间轴时为空
		std::optional<std::string> m_Translation;
		std::optional<std::string> m_Transliteration;
		mutable SLineTextWidthCache m_TextWidthCache;
		mutable STextWidthCache m_TranslationWidthCache;
		mutable STextWidthCache m_TransliterationWidthCache;
	};

	enum class EFormat
	{
		PLAIN,
		LRC_STANDARD,
		LRC_ENHANCED,
		ESLRC,
		TTML,
		KRC,
		QRC,
	};

	struct SLyricsTrack
	{
		EFormat m_Format = EFormat::LRC_STANDARD;
		std::vector<SLyricsLine> m_vLines; // 按 m_StartMs 升序
		std::optional<std::string> m_Title; // 来自 LRC [ti:] 或 TTML metadata
		std::optional<std::string> m_Artist; // 来自 LRC [ar:]
		std::optional<std::string> m_Album; // 来自 LRC [al:]
		int64_t m_OffsetMs = 0; // 整体偏移，来自 LRC [offset:] 或调用方手动
	};

} // namespace QmLyrics

#endif
