// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_lyrics_song_search_map.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/storage.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace QmLyrics
{

	namespace
	{
		constexpr const char *SONG_SEARCH_MAP = "qmclient/lyrics/song-search-map.json";
		constexpr const char *SONG_SEARCH_MAP_DIR = "qmclient/lyrics";

		std::string TrimCopy(std::string_view Text)
		{
			std::string Work(Text);
			const char *pTrimmed = str_utf8_skip_whitespaces(Work.c_str());
			if(pTrimmed != Work.c_str())
				Work.erase(0, pTrimmed - Work.c_str());
			str_utf8_trim_right(Work.data());
			Work.resize(str_length(Work.c_str()));
			return Work;
		}

		void EscapeJsonString(std::string_view In, std::string *pOut)
		{
			pOut->push_back('"');
			for(char C : In)
			{
				switch(C)
				{
				case '"': pOut->append("\\\""); break;
				case '\\': pOut->append("\\\\"); break;
				case '\b': pOut->append("\\b"); break;
				case '\f': pOut->append("\\f"); break;
				case '\n': pOut->append("\\n"); break;
				case '\r': pOut->append("\\r"); break;
				case '\t': pOut->append("\\t"); break;
				default:
					if((unsigned char)C < 0x20)
					{
						char aBuf[8];
						str_format(aBuf, sizeof(aBuf), "\\u%04x", (unsigned)(unsigned char)C);
						pOut->append(aBuf);
					}
					else
					{
						pOut->push_back(C);
					}
				}
			}
			pOut->push_back('"');
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		bool JsonBool(const json_value *pVal, bool Default = false)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_boolean)
				return pVal->u.boolean != 0;
			if(pVal->type == json_integer)
				return pVal->u.integer != 0;
			if(pVal->type == json_string)
				return str_comp_nocase(pVal->u.string.ptr, "true") == 0 || str_comp(pVal->u.string.ptr, "1") == 0;
			return Default;
		}

		bool EnsureSongSearchMapDir(IStorage *pStorage)
		{
			if(pStorage == nullptr)
				return false;
			pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			return pStorage->CreateFolder(SONG_SEARCH_MAP_DIR, IStorage::TYPE_SAVE) || pStorage->FolderExists(SONG_SEARCH_MAP_DIR, IStorage::TYPE_SAVE);
		}

		std::string NormalizeProviderKey(std::string_view Provider)
		{
			std::string Out;
			Out.reserve(Provider.size());
			for(char C : Provider)
			{
				const unsigned char Ch = (unsigned char)C;
				if(std::isalnum(Ch) != 0)
					Out.push_back((char)std::tolower(Ch));
			}
			return Out;
		}

		const json_value *FirstJsonField(const json_value &Object, const char *pNameA, const char *pNameB)
		{
			const json_value &A = Object[pNameA];
			if(A.type != json_none)
				return &A;
			const json_value &B = Object[pNameB];
			if(B.type != json_none)
				return &B;
			return nullptr;
		}

	} // namespace

	std::string CanonicalLyricsProviderId(std::string_view Provider)
	{
		const std::string Key = NormalizeProviderKey(Provider);
		if(Key == "qq")
			return "qq";
		if(Key == "kugou")
			return "kugou";
		if(Key == "netease")
			return "netease";
		if(Key == "lrclib")
			return "lrclib";
		if(Key == "amllttmldb")
			return "amll-ttml-db";
		if(Key == "localmusicfile")
			return "local-music-file";
		if(Key == "locallrcfile" || Key == "locallrc")
			return "local-lrc";
		if(Key == "localeslrcfile" || Key == "localeslrc")
			return "local-eslrc";
		if(Key == "localttmlfile" || Key == "localttml")
			return "local-ttml";
		if(Key == "applemusic")
			return "apple-music";
		return "";
	}

	std::string SongSearchMapToJson(const std::vector<SSongSearchMapEntry> &vEntries)
	{
		std::string Out;
		Out.reserve(vEntries.size() * 192);
		Out.append("{\"version\":1,\"entries\":[");
		bool First = true;
		for(const SSongSearchMapEntry &Entry : vEntries)
		{
			if(Entry.m_OriginalTitle.empty() && Entry.m_OriginalArtist.empty() && Entry.m_OriginalAlbum.empty())
				continue;
			if(!First)
				Out.push_back(',');
			First = false;
			Out.push_back('{');
			Out.append("\"OriginalTitle\":");
			EscapeJsonString(Entry.m_OriginalTitle, &Out);
			Out.append(",\"OriginalArtist\":");
			EscapeJsonString(Entry.m_OriginalArtist, &Out);
			Out.append(",\"OriginalAlbum\":");
			EscapeJsonString(Entry.m_OriginalAlbum, &Out);
			Out.append(",\"MappedTitle\":");
			EscapeJsonString(Entry.m_MappedTitle, &Out);
			Out.append(",\"MappedArtist\":");
			EscapeJsonString(Entry.m_MappedArtist, &Out);
			Out.append(",\"MappedAlbum\":");
			EscapeJsonString(Entry.m_MappedAlbum, &Out);
			Out.append(",\"IsMarkedAsPureMusic\":");
			Out.append(Entry.m_IsMarkedAsPureMusic ? "true" : "false");
			Out.append(",\"LyricsSearchProvider\":");
			EscapeJsonString(Entry.m_LyricsSearchProvider, &Out);
			Out.push_back('}');
		}
		Out.append("]}");
		return Out;
	}

	bool SongSearchMapFromJson(std::string_view Json, std::vector<SSongSearchMapEntry> *pvEntries, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pvEntries == nullptr)
			return false;
		if(Json.empty())
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "empty input", ErrSize);
			return false;
		}

		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, Json.data(), Json.size(), aJsonErr);
		if(pRoot == nullptr)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, aJsonErr, ErrSize);
			return false;
		}
		if(pRoot->type != json_object)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "root not object", ErrSize);
			return false;
		}

		std::vector<SSongSearchMapEntry> vNewEntries;
		const json_value &Entries = (*pRoot)["entries"];
		if(Entries.type == json_array)
		{
			vNewEntries.reserve(Entries.u.array.length);
			for(unsigned int i = 0; i < Entries.u.array.length; ++i)
			{
				const json_value *pItem = Entries.u.array.values[i];
				if(pItem == nullptr || pItem->type != json_object)
					continue;
				SSongSearchMapEntry Entry;
				Entry.m_OriginalTitle = JsonString(FirstJsonField(*pItem, "OriginalTitle", "originalTitle"));
				Entry.m_OriginalArtist = JsonString(FirstJsonField(*pItem, "OriginalArtist", "originalArtist"));
				Entry.m_OriginalAlbum = JsonString(FirstJsonField(*pItem, "OriginalAlbum", "originalAlbum"));
				Entry.m_MappedTitle = JsonString(FirstJsonField(*pItem, "MappedTitle", "mappedTitle"));
				Entry.m_MappedArtist = JsonString(FirstJsonField(*pItem, "MappedArtist", "mappedArtist"));
				Entry.m_MappedAlbum = JsonString(FirstJsonField(*pItem, "MappedAlbum", "mappedAlbum"));
				Entry.m_IsMarkedAsPureMusic = JsonBool(FirstJsonField(*pItem, "IsMarkedAsPureMusic", "isMarkedAsPureMusic"));
				Entry.m_LyricsSearchProvider = JsonString(FirstJsonField(*pItem, "LyricsSearchProvider", "lyricsSearchProvider"));
				if(Entry.m_OriginalTitle.empty() && Entry.m_OriginalArtist.empty() && Entry.m_OriginalAlbum.empty())
					continue;
				vNewEntries.push_back(std::move(Entry));
			}
		}
		else
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "entries missing/not array", ErrSize);
			return false;
		}

		json_value_free(pRoot);
		*pvEntries = std::move(vNewEntries);
		return true;
	}

	bool LoadSongSearchMap(IStorage *pStorage, std::vector<SSongSearchMapEntry> *pvEntries)
	{
		if(pStorage == nullptr || pvEntries == nullptr)
			return false;
		char *pJson = pStorage->ReadFileStr(SONG_SEARCH_MAP, IStorage::TYPE_SAVE);
		if(pJson == nullptr)
			return false;
		char aErr[128];
		const bool Ok = SongSearchMapFromJson(pJson, pvEntries, aErr, sizeof(aErr));
		free(pJson);
		return Ok;
	}

	bool SaveSongSearchMap(IStorage *pStorage, const std::vector<SSongSearchMapEntry> &vEntries)
	{
		if(!EnsureSongSearchMapDir(pStorage))
			return false;
		IOHANDLE File = pStorage->OpenFile(SONG_SEARCH_MAP, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const std::string Json = SongSearchMapToJson(vEntries);
		const bool Ok = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		io_close(File);
		return Ok;
	}

	const SSongSearchMapEntry *FindSongSearchMapping(const SMatchQuery &Query, const std::vector<SSongSearchMapEntry> &vEntries)
	{
		for(const SSongSearchMapEntry &Entry : vEntries)
		{
			if(Query.m_Title == Entry.m_OriginalTitle &&
				Query.m_Artist == Entry.m_OriginalArtist &&
				Query.m_Album == Entry.m_OriginalAlbum)
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	SMatchQuery ApplySongSearchMapping(const SMatchQuery &Query, const SSongSearchMapEntry &Entry)
	{
		SMatchQuery Result = Query;
		Result.m_Title = TrimCopy(Entry.m_MappedTitle);
		Result.m_Artist = TrimCopy(Entry.m_MappedArtist);
		Result.m_Album = TrimCopy(Entry.m_MappedAlbum);
		return Result;
	}

	SSourceCandidate BuildPureMusicCandidate(const SMatchQuery &Query)
	{
		SSourceCandidate Candidate;
		Candidate.m_RawText = "[00:00.000]🎶🎶🎶\n[99:00.000]";
		Candidate.m_FormatHint = EFormat::LRC_STANDARD;
		Candidate.m_Metadata.m_Title = Query.m_Title;
		Candidate.m_Metadata.m_Artist = Query.m_Artist;
		Candidate.m_Metadata.m_Album = Query.m_Album;
		Candidate.m_Metadata.m_DurationSec = Query.m_DurationSec;
		Candidate.m_SourceId = "song-search-map";
		Candidate.m_SourceScore = 1.0f;
		return Candidate;
	}

} // namespace QmLyrics
