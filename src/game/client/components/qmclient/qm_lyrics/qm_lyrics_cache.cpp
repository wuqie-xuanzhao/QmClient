#include "qm_lyrics_cache.h"

#include "qm_lyrics_match.h"

#include <base/hash.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/storage.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace QmLyrics
{

	namespace
	{

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

		int64_t JsonInt(const json_value *pVal, int64_t Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return pVal->u.integer;
			if(pVal->type == json_double)
				return (int64_t)pVal->u.dbl;
			return Default;
		}

		double JsonDouble(const json_value *pVal, double Default = 0.0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_double)
				return pVal->u.dbl;
			if(pVal->type == json_integer)
				return (double)pVal->u.integer;
			return Default;
		}

	} // anonymous namespace

	std::string BuildCacheKey(std::string_view Title, std::string_view Artist, std::string_view Album, int DurationSec)
	{
		std::string Out;
		Out.reserve(Title.size() + Artist.size() + Album.size() + 32);
		Out.append(NormalizeForMatch(Title));
		Out.push_back('|');
		Out.append(NormalizeForMatch(Artist));
		Out.push_back('|');
		Out.append(NormalizeForMatch(Album));
		Out.push_back('|');
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d", DurationSec);
		Out.append(aBuf);
		return Out;
	}

	std::string FileNameForKey(std::string_view Key)
	{
		const SHA256_DIGEST Digest = sha256(Key.data(), Key.size());
		char aHex[SHA256_MAXSTRSIZE];
		sha256_str(Digest, aHex, sizeof(aHex));
		// 前 16 hex 字符 + ".json"
		char aOut[32];
		str_format(aOut, sizeof(aOut), "%.16s.json", aHex);
		return aOut;
	}

	bool IsValidCachePayloadFileName(std::string_view FileName)
	{
		if(FileName.size() != 21 || FileName.substr(16) != ".json")
			return false;
		for(size_t i = 0; i < 16; ++i)
		{
			const char C = FileName[i];
			const bool HexDigit = (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F');
			if(!HexDigit)
				return false;
		}
		return true;
	}

	std::vector<std::string> CCacheIndex::Upsert(const SCacheEntry &Entry, int MaxEntries)
	{
		std::vector<std::string> vEvicted;
		auto It = m_mEntries.find(Entry.m_Key);
		if(It != m_mEntries.end())
		{
			// 替换：旧文件名若不同则淘汰
			if(It->second.m_FileName != Entry.m_FileName)
				vEvicted.push_back(It->second.m_FileName);
			It->second = Entry;
		}
		else
		{
			m_mEntries.emplace(Entry.m_Key, Entry);
		}

		if(MaxEntries > 0 && (int)m_mEntries.size() > MaxEntries)
		{
			// 按 LastUsedAt 升序淘汰至 MaxEntries
			std::vector<std::pair<int64_t, std::string>> vSorted;
			vSorted.reserve(m_mEntries.size());
			for(const auto &Pair : m_mEntries)
				vSorted.emplace_back(Pair.second.m_LastUsedAt, Pair.first);
			std::sort(vSorted.begin(), vSorted.end(), [](const auto &A, const auto &B) {
				return A.first < B.first;
			});
			const int Excess = (int)m_mEntries.size() - MaxEntries;
			for(int i = 0; i < Excess; ++i)
			{
				auto It2 = m_mEntries.find(vSorted[i].second);
				if(It2 != m_mEntries.end())
				{
					vEvicted.push_back(It2->second.m_FileName);
					m_mEntries.erase(It2);
				}
			}
		}
		return vEvicted;
	}

	const SCacheEntry *CCacheIndex::Lookup(std::string_view Key, int64_t NowUnixSec)
	{
		auto It = m_mEntries.find(std::string(Key));
		if(It == m_mEntries.end())
			return nullptr;
		It->second.m_LastUsedAt = NowUnixSec;
		return &It->second;
	}

	bool CCacheIndex::Remove(std::string_view Key, std::string *pFileName)
	{
		auto It = m_mEntries.find(std::string(Key));
		if(It == m_mEntries.end())
			return false;
		if(pFileName != nullptr)
			*pFileName = It->second.m_FileName;
		m_mEntries.erase(It);
		return true;
	}

	std::vector<std::string> CCacheIndex::EvictExpired(int TtlDays, int64_t NowUnixSec)
	{
		std::vector<std::string> vEvicted;
		if(TtlDays <= 0)
			return vEvicted;
		const int64_t TtlSec = (int64_t)TtlDays * 86400;
		for(auto It = m_mEntries.begin(); It != m_mEntries.end();)
		{
			if(NowUnixSec - It->second.m_StoredAt > TtlSec)
			{
				vEvicted.push_back(It->second.m_FileName);
				It = m_mEntries.erase(It);
			}
			else
			{
				++It;
			}
		}
		return vEvicted;
	}

	std::string CCacheIndex::ToJson() const
	{
		std::string Out;
		Out.reserve(m_mEntries.size() * 128);
		Out.append("{\"entries\":[");
		bool First = true;
		for(const auto &Pair : m_mEntries)
		{
			if(!First)
				Out.push_back(',');
			First = false;
			Out.push_back('{');
			Out.append("\"key\":");
			EscapeJsonString(Pair.second.m_Key, &Out);
			Out.append(",\"file\":");
			EscapeJsonString(Pair.second.m_FileName, &Out);
			Out.append(",\"source\":");
			EscapeJsonString(Pair.second.m_Source, &Out);
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), ",\"score\":%.2f", Pair.second.m_Score);
			Out.append(aBuf);
			str_format(aBuf, sizeof(aBuf), ",\"used\":%lld", (long long)Pair.second.m_LastUsedAt);
			Out.append(aBuf);
			str_format(aBuf, sizeof(aBuf), ",\"stored\":%lld", (long long)Pair.second.m_StoredAt);
			Out.append(aBuf);
			Out.push_back('}');
		}
		Out.append("]}");
		return Out;
	}

	bool CCacheIndex::FromJson(std::string_view Json, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
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

		std::unordered_map<std::string, SCacheEntry> mNew;
		const json_value &Entries = (*pRoot)["entries"];
		if(Entries.type != json_array)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "entries missing/not array", ErrSize);
			return false;
		}

		for(unsigned int i = 0; i < Entries.u.array.length; ++i)
		{
			const json_value *pItem = Entries.u.array.values[i];
			if(pItem == nullptr || pItem->type != json_object)
				continue;
			SCacheEntry Entry;
			Entry.m_Key = JsonString(&(*pItem)["key"]);
			Entry.m_FileName = JsonString(&(*pItem)["file"]);
			Entry.m_Source = JsonString(&(*pItem)["source"]);
			Entry.m_Score = (float)JsonDouble(&(*pItem)["score"]);
			Entry.m_LastUsedAt = JsonInt(&(*pItem)["used"]);
			Entry.m_StoredAt = JsonInt(&(*pItem)["stored"]);
			if(Entry.m_Key.empty() || !IsValidCachePayloadFileName(Entry.m_FileName))
				continue;
			mNew.emplace(Entry.m_Key, std::move(Entry));
		}

		json_value_free(pRoot);
		m_mEntries = std::move(mNew);
		return true;
	}

	namespace
	{

		constexpr const char *CACHE_DIR = "qmclient/lyrics";
		constexpr const char *CACHE_INDEX = "qmclient/lyrics/index.json";

		bool EnsureCacheDir(IStorage *pStorage)
		{
			if(pStorage == nullptr)
				return false;
			pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			return pStorage->CreateFolder(CACHE_DIR, IStorage::TYPE_SAVE) || pStorage->FolderExists(CACHE_DIR, IStorage::TYPE_SAVE);
		}

		std::string CachePayloadPath(const char *pFileName)
		{
			std::string Path = CACHE_DIR;
			Path.push_back('/');
			Path.append(pFileName != nullptr ? pFileName : "");
			return Path;
		}

		EFormat FormatFromInt(int Value)
		{
			switch(Value)
			{
			case 0: return EFormat::PLAIN;
			case 1: return EFormat::LRC_STANDARD;
			case 2: return EFormat::LRC_ENHANCED;
			case 3: return EFormat::ESLRC;
			case 4: return EFormat::TTML;
			case 5: return EFormat::KRC;
			case 6: return EFormat::QRC;
			default: return EFormat::LRC_STANDARD;
			}
		}

	} // anonymous namespace

	bool LoadCacheIndex(IStorage *pStorage, CCacheIndex *pOut)
	{
		if(pStorage == nullptr || pOut == nullptr)
			return false;
		char *pJson = pStorage->ReadFileStr(CACHE_INDEX, IStorage::TYPE_SAVE);
		if(pJson == nullptr)
			return false;
		char aErr[128];
		const bool Ok = pOut->FromJson(pJson, aErr, sizeof(aErr));
		free(pJson);
		return Ok;
	}

	bool SaveCacheIndex(IStorage *pStorage, const CCacheIndex &Index)
	{
		if(!EnsureCacheDir(pStorage))
			return false;
		IOHANDLE File = pStorage->OpenFile(CACHE_INDEX, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const std::string Json = Index.ToJson();
		const bool Ok = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		io_close(File);
		return Ok;
	}

	bool LoadCachePayload(IStorage *pStorage, const char *pFileName, SCachePayload *pOut)
	{
		if(pStorage == nullptr || pFileName == nullptr || !IsValidCachePayloadFileName(pFileName) || pOut == nullptr)
			return false;
		const std::string Path = CachePayloadPath(pFileName);
		char *pJsonText = pStorage->ReadFileStr(Path.c_str(), IStorage::TYPE_SAVE);
		if(pJsonText == nullptr)
			return false;

		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, pJsonText, str_length(pJsonText), aJsonErr);
		free(pJsonText);
		if(pRoot == nullptr)
			return false;
		if(pRoot->type != json_object)
		{
			json_value_free(pRoot);
			return false;
		}

		SCachePayload Payload;
		Payload.m_RawText = JsonString(&(*pRoot)["raw"]);
		Payload.m_TranslationText = JsonString(&(*pRoot)["translation"]);
		Payload.m_TransliterationText = JsonString(&(*pRoot)["transliteration"]);
		Payload.m_Format = FormatFromInt((int)JsonInt(&(*pRoot)["format"], (int)EFormat::LRC_STANDARD));
		Payload.m_Source = JsonString(&(*pRoot)["source"]);
		const json_value &Metadata = (*pRoot)["metadata"];
		if(Metadata.type == json_object)
		{
			Payload.m_Metadata.m_Title = JsonString(&Metadata["title"]);
			Payload.m_Metadata.m_Artist = JsonString(&Metadata["artist"]);
			Payload.m_Metadata.m_Album = JsonString(&Metadata["album"]);
			Payload.m_Metadata.m_DurationSec = (int)JsonInt(&Metadata["duration"]);
		}
		json_value_free(pRoot);
		if(Payload.m_RawText.empty())
			return false;
		*pOut = std::move(Payload);
		return true;
	}

	bool SaveCachePayload(IStorage *pStorage, const char *pFileName, const SCachePayload &Payload)
	{
		if(pFileName == nullptr || !IsValidCachePayloadFileName(pFileName) || Payload.m_RawText.empty() || !EnsureCacheDir(pStorage))
			return false;

		CJsonStringWriter Writer;
		Writer.BeginObject();
		Writer.WriteAttribute("raw");
		Writer.WriteStrValue(Payload.m_RawText.c_str());
		Writer.WriteAttribute("translation");
		Writer.WriteStrValue(Payload.m_TranslationText.c_str());
		Writer.WriteAttribute("transliteration");
		Writer.WriteStrValue(Payload.m_TransliterationText.c_str());
		Writer.WriteAttribute("format");
		Writer.WriteIntValue((int)Payload.m_Format);
		Writer.WriteAttribute("source");
		Writer.WriteStrValue(Payload.m_Source.c_str());
		Writer.WriteAttribute("metadata");
		Writer.BeginObject();
		Writer.WriteAttribute("title");
		Writer.WriteStrValue(Payload.m_Metadata.m_Title.c_str());
		Writer.WriteAttribute("artist");
		Writer.WriteStrValue(Payload.m_Metadata.m_Artist.c_str());
		Writer.WriteAttribute("album");
		Writer.WriteStrValue(Payload.m_Metadata.m_Album.c_str());
		Writer.WriteAttribute("duration");
		Writer.WriteIntValue(Payload.m_Metadata.m_DurationSec);
		Writer.EndObject();
		Writer.EndObject();
		std::string Json = Writer.GetOutputString();

		const std::string Path = CachePayloadPath(pFileName);
		IOHANDLE File = pStorage->OpenFile(Path.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const bool Ok = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		io_close(File);
		return Ok;
	}

	void RemoveCachePayload(IStorage *pStorage, const char *pFileName)
	{
		if(pStorage == nullptr || pFileName == nullptr || !IsValidCachePayloadFileName(pFileName))
			return;
		const std::string Path = CachePayloadPath(pFileName);
		pStorage->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
	}

} // namespace QmLyrics
