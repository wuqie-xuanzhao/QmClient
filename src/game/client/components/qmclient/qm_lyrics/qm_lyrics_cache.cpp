#include "qm_lyrics_cache.h"

#include "qm_lyrics_match.h"

#include <base/hash.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/storage.h>

#include <algorithm>
#include <cctype>
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

		bool IsWindowsReservedBaseName(std::string_view Name)
		{
			std::string Upper(Name);
			std::transform(Upper.begin(), Upper.end(), Upper.begin(), [](unsigned char C) { return (char)std::toupper(C); });
			if(Upper == "CON" || Upper == "PRN" || Upper == "AUX" || Upper == "NUL")
				return true;
			return Upper.size() == 4 && (Upper.substr(0, 3) == "COM" || Upper.substr(0, 3) == "LPT") && Upper[3] >= '1' && Upper[3] <= '9';
		}

		std::string SanitizeFileNameComponent(std::string_view Input)
		{
			const std::string InputCopy(Input);
			const bool ValidUtf8 = str_utf8_check(InputCopy.c_str()) != 0;
			std::string Out;
			Out.reserve(std::min<size_t>(Input.size(), 80));
			for(unsigned char C : Input)
			{
				const bool InvalidAscii = C < 0x20 || C == '<' || C == '>' || C == ':' || C == '"' || C == '/' || C == '\\' || C == '|' || C == '?' || C == '*';
				if(InvalidAscii || (!ValidUtf8 && C >= 0x80))
					Out.push_back('_');
				else
					Out.push_back((char)C);
			}
			while(!Out.empty() && (Out.back() == ' ' || Out.back() == '.'))
				Out.pop_back();
			if(Out.size() > 80)
			{
				Out.resize(80);
				while(!Out.empty() && !str_utf8_check(Out.c_str()))
					Out.pop_back();
			}
			if(Out.empty())
				Out = "Unknown";
			if(IsWindowsReservedBaseName(Out))
				Out.insert(Out.begin(), '_');
			return Out;
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

	std::string FileNameForTrack(std::string_view Title, std::string_view Artist, std::string_view Key, bool Collision)
	{
		std::string Out = SanitizeFileNameComponent(Title);
		Out.push_back('+');
		Out.append(SanitizeFileNameComponent(Artist));
		if(Collision)
		{
			const SHA256_DIGEST Digest = sha256(Key.data(), Key.size());
			char aHex[SHA256_MAXSTRSIZE];
			sha256_str(Digest, aHex, sizeof(aHex));
			Out.push_back('-');
			Out.append(aHex, 8);
		}
		Out.append(".lrc");
		return Out;
	}

	std::string ChooseCachePayloadFileName(const CCacheIndex &Index, std::string_view Title, std::string_view Artist, std::string_view Key, std::string_view IgnoreKey)
	{
		auto Conflicts = [&](std::string_view FileName) {
			for(const auto &Pair : Index.All())
			{
				if(!IgnoreKey.empty() && std::string_view(Pair.first) == IgnoreKey)
					continue;
				const std::string ExistingFileName = Pair.second.m_FileName;
				const std::string CandidateFileName(FileName);
				if(str_comp_nocase(ExistingFileName.c_str(), CandidateFileName.c_str()) == 0)
					return true;
			}
			return false;
		};

		std::string FileName = FileNameForTrack(Title, Artist, Key);
		if(!Conflicts(FileName))
			return FileName;
		FileName = FileNameForTrack(Title, Artist, Key, true);
		if(!Conflicts(FileName))
			return FileName;

		const SHA256_DIGEST Digest = sha256(Key.data(), Key.size());
		char aHex[SHA256_MAXSTRSIZE];
		sha256_str(Digest, aHex, sizeof(aHex));
		FileName = std::string("Lyrics-") + aHex + ".lrc";
		if(!Conflicts(FileName))
			return FileName;

		for(unsigned CollisionIndex = 1;; ++CollisionIndex)
		{
			char aSuffix[16];
			str_format(aSuffix, sizeof(aSuffix), "-%u.lrc", CollisionIndex);
			FileName = std::string("Lyrics-") + aHex + aSuffix;
			if(!Conflicts(FileName))
				return FileName;
		}
	}

	bool IsValidCachePayloadFileName(std::string_view FileName)
	{
		if(FileName.size() == 21 && FileName.substr(16) == ".json")
		{
			for(size_t i = 0; i < 16; ++i)
			{
				const char C = FileName[i];
				const bool HexDigit = (C >= '0' && C <= '9') || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F');
				if(!HexDigit)
					return false;
			}
			return true;
		}
		if(FileName.size() <= 4 || FileName.size() > 180 || FileName.substr(FileName.size() - 4) != ".lrc")
			return false;
		const std::string Base(FileName.substr(0, FileName.size() - 4));
		if(Base.empty() || Base == "." || Base == ".." || IsWindowsReservedBaseName(Base) || !str_utf8_check(Base.c_str()))
			return false;
		if(Base.back() == ' ' || Base.back() == '.')
			return false;
		for(unsigned char C : Base)
		{
			if(C < 0x20 || C == '<' || C == '>' || C == ':' || C == '"' || C == '/' || C == '\\' || C == '|' || C == '?' || C == '*')
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

	const SCacheEntry *CCacheIndex::Find(std::string_view Key) const
	{
		const auto It = m_mEntries.find(std::string(Key));
		return It == m_mEntries.end() ? nullptr : &It->second;
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
		constexpr const char *CACHE_INDEX_TMP = "qmclient/lyrics/index.json.tmp";

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

		std::string CacheMetaFileName(const char *pFileName)
		{
			std::string FileName = pFileName != nullptr ? pFileName : "";
			if(FileName.size() >= 4 && FileName.substr(FileName.size() - 4) == ".lrc")
				FileName.resize(FileName.size() - 4);
			FileName.append(".meta.json");
			return FileName;
		}

		bool IndexReferencesFileName(const CCacheIndex &Index, std::string_view FileName)
		{
			const std::string Candidate(FileName);
			for(const auto &Pair : Index.All())
			{
				if(str_comp_nocase(Pair.second.m_FileName.c_str(), Candidate.c_str()) == 0)
					return true;
			}
			return false;
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
		IOHANDLE File = pStorage->OpenFile(CACHE_INDEX_TMP, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const std::string Json = Index.ToJson();
		const bool WriteOk = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		const bool SyncOk = WriteOk && io_sync(File) == 0;
		const bool CloseOk = io_close(File) == 0;
		if(!SyncOk || !CloseOk || !pStorage->RenameFile(CACHE_INDEX_TMP, CACHE_INDEX, IStorage::TYPE_SAVE))
		{
			pStorage->RemoveFile(CACHE_INDEX_TMP, IStorage::TYPE_SAVE);
			return false;
		}
		return true;
	}

	bool LoadCachePayload(IStorage *pStorage, const char *pFileName, SCachePayload *pOut)
	{
		if(pStorage == nullptr || pFileName == nullptr || !IsValidCachePayloadFileName(pFileName) || pOut == nullptr)
			return false;
		const std::string Path = CachePayloadPath(pFileName);
		const bool ReadablePair = std::string_view(pFileName).substr(str_length(pFileName) - 4) == ".lrc";
		char *pRawText = nullptr;
		std::string MetadataPath = Path;
		if(ReadablePair)
		{
			pRawText = pStorage->ReadFileStr(Path.c_str(), IStorage::TYPE_SAVE);
			if(pRawText == nullptr)
				return false;
			MetadataPath = CachePayloadPath(CacheMetaFileName(pFileName).c_str());
		}
		char *pJsonText = pStorage->ReadFileStr(MetadataPath.c_str(), IStorage::TYPE_SAVE);
		if(pJsonText == nullptr)
		{
			free(pRawText);
			return false;
		}

		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, pJsonText, str_length(pJsonText), aJsonErr);
		free(pJsonText);
		if(pRoot == nullptr)
		{
			free(pRawText);
			return false;
		}
		if(pRoot->type != json_object)
		{
			json_value_free(pRoot);
			free(pRawText);
			return false;
		}

		SCachePayload Payload;
		Payload.m_RawText = ReadablePair ? pRawText : JsonString(&(*pRoot)["raw"]);
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
		free(pRawText);
		if(Payload.m_RawText.empty())
			return false;
		*pOut = std::move(Payload);
		return true;
	}

	bool SaveCachePayload(IStorage *pStorage, const char *pFileName, const SCachePayload &Payload)
	{
		if(pFileName == nullptr || !IsValidCachePayloadFileName(pFileName) || Payload.m_RawText.empty() || !EnsureCacheDir(pStorage))
			return false;

		const bool ReadablePair = std::string_view(pFileName).substr(str_length(pFileName) - 4) == ".lrc";
		CJsonStringWriter Writer;
		Writer.BeginObject();
		if(!ReadablePair)
		{
			Writer.WriteAttribute("raw");
			Writer.WriteStrValue(Payload.m_RawText.c_str());
		}
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
		if(ReadablePair)
		{
			IOHANDLE RawFile = pStorage->OpenFile(Path.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!RawFile)
				return false;
			const bool RawOk = io_write(RawFile, Payload.m_RawText.data(), (unsigned)Payload.m_RawText.size()) == Payload.m_RawText.size();
			io_close(RawFile);
			if(!RawOk)
			{
				pStorage->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
				return false;
			}
		}
		const std::string JsonPath = ReadablePair ? CachePayloadPath(CacheMetaFileName(pFileName).c_str()) : Path;
		IOHANDLE File = pStorage->OpenFile(JsonPath.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
		{
			if(ReadablePair)
				pStorage->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
			return false;
		}
		const bool Ok = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		io_close(File);
		if(!Ok && ReadablePair)
		{
			pStorage->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
			pStorage->RemoveFile(JsonPath.c_str(), IStorage::TYPE_SAVE);
		}
		return Ok;
	}

	void RemoveCachePayload(IStorage *pStorage, const char *pFileName)
	{
		if(pStorage == nullptr || pFileName == nullptr || !IsValidCachePayloadFileName(pFileName))
			return;
		const std::string Path = CachePayloadPath(pFileName);
		pStorage->RemoveFile(Path.c_str(), IStorage::TYPE_SAVE);
		if(std::string_view(pFileName).substr(str_length(pFileName) - 4) == ".lrc")
			pStorage->RemoveFile(CachePayloadPath(CacheMetaFileName(pFileName).c_str()).c_str(), IStorage::TYPE_SAVE);
	}

	bool CommitCacheEntry(IStorage *pStorage, CCacheIndex *pIndex, const SCacheEntry &Entry, const SCachePayload &Payload, int MaxEntries)
	{
		if(pStorage == nullptr || pIndex == nullptr)
			return false;

		const bool PayloadWasReferenced = IndexReferencesFileName(*pIndex, Entry.m_FileName);
		if(!SaveCachePayload(pStorage, Entry.m_FileName.c_str(), Payload))
			return false;

		CCacheIndex UpdatedIndex = *pIndex;
		const std::vector<std::string> vEvicted = UpdatedIndex.Upsert(Entry, MaxEntries);
		if(!SaveCacheIndex(pStorage, UpdatedIndex))
		{
			if(!PayloadWasReferenced)
				RemoveCachePayload(pStorage, Entry.m_FileName.c_str());
			return false;
		}

		*pIndex = std::move(UpdatedIndex);
		for(const std::string &FileName : vEvicted)
		{
			if(!IndexReferencesFileName(*pIndex, FileName))
				RemoveCachePayload(pStorage, FileName.c_str());
		}
		return true;
	}

	bool MigrateLegacyCacheEntry(IStorage *pStorage, CCacheIndex *pIndex, std::string_view LegacyKey, std::string_view TrackKey, std::string_view Title, std::string_view Artist, int MaxEntries)
	{
		if(pStorage == nullptr || pIndex == nullptr || LegacyKey.empty() || TrackKey.empty())
			return false;
		const SCacheEntry *pLegacyEntry = pIndex->Find(LegacyKey);
		if(pLegacyEntry == nullptr)
			return false;
		const SCacheEntry LegacyEntry = *pLegacyEntry;
		if(LegacyKey == TrackKey && std::string_view(LegacyEntry.m_FileName).ends_with(".lrc"))
			return true;

		SCachePayload Payload;
		if(!LoadCachePayload(pStorage, LegacyEntry.m_FileName.c_str(), &Payload))
			return false;

		SCacheEntry MigratedEntry = LegacyEntry;
		MigratedEntry.m_Key = std::string(TrackKey);
		// The legacy file must remain untouched until the replacement index is durable.
		// Treat it as a collision even when it already has the readable target name.
		MigratedEntry.m_FileName = ChooseCachePayloadFileName(*pIndex, Title, Artist, TrackKey);
		if(!SaveCachePayload(pStorage, MigratedEntry.m_FileName.c_str(), Payload))
			return false;

		CCacheIndex UpdatedIndex = *pIndex;
		UpdatedIndex.Remove(LegacyKey);
		const std::vector<std::string> vEvicted = UpdatedIndex.Upsert(MigratedEntry, MaxEntries);
		if(!SaveCacheIndex(pStorage, UpdatedIndex))
		{
			if(!IndexReferencesFileName(*pIndex, MigratedEntry.m_FileName))
				RemoveCachePayload(pStorage, MigratedEntry.m_FileName.c_str());
			return false;
		}

		*pIndex = std::move(UpdatedIndex);
		if(!IndexReferencesFileName(*pIndex, LegacyEntry.m_FileName))
			RemoveCachePayload(pStorage, LegacyEntry.m_FileName.c_str());
		for(const std::string &FileName : vEvicted)
		{
			if(!IndexReferencesFileName(*pIndex, FileName))
				RemoveCachePayload(pStorage, FileName.c_str());
		}
		return true;
	}

} // namespace QmLyrics
