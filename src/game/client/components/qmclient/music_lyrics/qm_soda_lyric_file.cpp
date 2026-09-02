#include "qm_soda_lyric_file.h"

#include "music_lyrics_krc.h"

#include <engine/external/json-parser/json.h>

#include <game/client/components/qmclient/netease/netease_lyric_parser.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace QmSodaLyricFile
{
	namespace
	{
		const json_value *Field(const json_value *pObject, const char *pName)
		{
			if(pObject == nullptr || pObject->type != json_object || pName == nullptr)
				return nullptr;
			for(unsigned int Index = 0; Index < pObject->u.object.length; ++Index)
			{
				const auto &Entry = pObject->u.object.values[Index];
				if(Entry.name != nullptr && std::string_view(Entry.name, Entry.name_length) == pName)
					return Entry.value;
			}
			return nullptr;
		}

		std::string StringValue(const json_value *pValue)
		{
			if(pValue == nullptr || pValue->type != json_string || pValue->u.string.ptr == nullptr)
				return {};
			return std::string(pValue->u.string.ptr, pValue->u.string.length);
		}

		int64_t Int64Value(const json_value *pValue)
		{
			if(pValue == nullptr)
				return 0;
			if(pValue->type == json_integer)
				return (int64_t)pValue->u.integer;
			if(pValue->type == json_double)
				return (int64_t)pValue->u.dbl;
			return 0;
		}

		// 把独立 LRC 翻译轨按行起始时间与主歌词行对齐。
		void AlignTranslations(const NeteaseLyrics::STimeline &Main, std::string_view TranslationLrc, std::vector<std::string> *pOut)
		{
			if(pOut == nullptr)
				return;
			pOut->clear();
			if(TranslationLrc.empty())
				return;
			NeteaseLyrics::STimeline Translation;
			if(!NeteaseLyrics::ParseLrc(TranslationLrc, &Translation))
				return;
			for(const NeteaseLyrics::SLine &Line : Main.m_vLines)
			{
				std::string Matched;
				int64_t BestDelta = INT64_MAX;
				for(const NeteaseLyrics::SLine &TransLine : Translation.m_vLines)
				{
					int64_t Delta = TransLine.m_StartMs - Line.m_StartMs;
					if(Delta < 0)
						Delta = -Delta;
					if(Delta < BestDelta)
					{
						BestDelta = Delta;
						Matched = TransLine.m_Text;
					}
				}
				// 允许 ±150ms 对齐误差;超过视为无匹配。
				pOut->push_back(BestDelta <= 150 ? Matched : std::string());
			}
		}
	}

	bool ParseLyricFileJson(std::string_view Json, QmMusicLyrics::SLyricsData *pOut, std::string *pError)
	{
		if(pOut == nullptr)
			return false;
		*pOut = {};
		json_value *pRoot = json_parse(Json.data(), Json.size());
		if(pRoot == nullptr || pRoot->type != json_object)
		{
			if(pRoot != nullptr)
				json_value_free(pRoot);
			if(pError)
				*pError = "invalid json";
			return false;
		}
		pOut->m_Song.m_Title = StringValue(Field(pRoot, "title"));
		pOut->m_Song.m_Artist = StringValue(Field(pRoot, "artist"));
		pOut->m_Song.m_Album = StringValue(Field(pRoot, "album"));
		pOut->m_Song.m_CoverPath = StringValue(Field(pRoot, "coverUrl"));
		pOut->m_Song.m_DurationMs = Int64Value(Field(pRoot, "durationMs"));
		const std::string LyricType = StringValue(Field(pRoot, "lyricType"));
		const std::string LyricContent = StringValue(Field(pRoot, "lyricContent"));
		const std::string TranslationLrc = StringValue(Field(pRoot, "translationLrc"));
		json_value_free(pRoot);

		bool Parsed = false;
		std::string Error;
		if(LyricType == "krc")
			Parsed = QmMusicLyrics::ParseKrcText(LyricContent, &pOut->m_Timeline, &Error);
		else
			Parsed = NeteaseLyrics::ParseLrc(LyricContent, &pOut->m_Timeline, &Error);
		if(!Parsed)
		{
			if(pError)
				*pError = Error.empty() ? "lyric parse failed" : Error;
			return false;
		}
		AlignTranslations(pOut->m_Timeline, TranslationLrc, &pOut->m_vTranslations);
		return true;
	}
} // namespace QmSodaLyricFile
