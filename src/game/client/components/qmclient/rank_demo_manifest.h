#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_RANK_DEMO_MANIFEST_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_RANK_DEMO_MANIFEST_H

#include <base/str.h>

#include <engine/shared/json.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace qmclient::rank_demo
{
	struct SEntry
	{
		std::string m_Map;
		int m_Rank = 0;
		std::string m_Time;
		std::string m_Demo;
		std::string m_Names;
		int m_Cid = 0;
		int64_t m_Ts = std::numeric_limits<int64_t>::min();
	};

	inline const json_value *Field(const json_value *pObject, const char *pName)
	{
		if(pObject == nullptr || pObject->type != json_object)
			return nullptr;
		return json_object_get(pObject, pName);
	}

	inline bool ReadString(const json_value *pObject, const char *pName, std::string &Out, size_t MaxLength)
	{
		const json_value *pValue = Field(pObject, pName);
		if(pValue == nullptr || pValue->type != json_string)
			return false;
		const char *pString = json_string_get(pValue);
		if(pString == nullptr || pString[0] == '\0' || str_length(pString) > MaxLength)
			return false;
		Out = pString;
		return true;
	}

	inline bool ReadInt(const json_value *pObject, const char *pName, int64_t &Out)
	{
		const json_value *pValue = Field(pObject, pName);
		if(pValue == nullptr || pValue->type != json_integer)
			return false;
		Out = pValue->u.integer;
		return true;
	}

	inline bool ReadTime(const json_value *pObject, const char *pName, std::string &Out)
	{
		const json_value *pValue = Field(pObject, pName);
		if(pValue == nullptr)
			return false;
		if(pValue->type == json_string)
		{
			const char *pString = json_string_get(pValue);
			if(pString == nullptr || pString[0] == '\0' || str_length(pString) > 32)
				return false;
			Out = pString;
			return true;
		}
		if(pValue->type == json_double || pValue->type == json_integer)
		{
			char aBuffer[32];
			if(pValue->type == json_double)
				str_format(aBuffer, sizeof(aBuffer), "%.2f", pValue->u.dbl);
			else
				str_format(aBuffer, sizeof(aBuffer), "%lld", (long long)pValue->u.integer);
			Out = aBuffer;
			return true;
		}
		return false;
	}

	inline bool IsSafeDemoName(const std::string &Demo)
	{
		if(Demo.empty() || Demo.size() > 256)
			return false;
		for(const char Character : Demo)
		{
			if(static_cast<unsigned char>(Character) < 0x20 || Character == '/' || Character == '\\' || Character == '?' || Character == '#')
				return false;
		}
		return true;
	}

	inline bool ParseEntry(const json_value *pRoot, SEntry &Entry)
	{
		std::string Status;
		if(!ReadString(pRoot, "status", Status, 16) || Status != "ok")
			return false;
		if(!ReadString(pRoot, "map", Entry.m_Map, 128) || !ReadString(pRoot, "demo", Entry.m_Demo, 256) || !IsSafeDemoName(Entry.m_Demo))
			return false;

		int64_t Value = 0;
		if(!ReadInt(pRoot, "rank", Value) || Value <= 0 || Value > 9999)
			return false;
		Entry.m_Rank = static_cast<int>(Value);
		if(ReadTime(pRoot, "time", Entry.m_Time) == false)
			Entry.m_Time.clear();
		if(ReadInt(pRoot, "cid", Value) && Value >= 0 && Value <= 63)
			Entry.m_Cid = static_cast<int>(Value);
		if(ReadInt(pRoot, "ts", Value))
			Entry.m_Ts = Value;

		const json_value *pNames = Field(pRoot, "names");
		if(pNames != nullptr && pNames->type == json_array)
		{
			for(unsigned i = 0; i < pNames->u.array.length && Entry.m_Names.size() < 256; ++i)
			{
				const json_value *pName = pNames->u.array.values[i];
				if(pName == nullptr || pName->type != json_string)
					continue;
				const char *pString = json_string_get(pName);
				if(pString == nullptr || pString[0] == '\0' || str_length(pString) > 64)
					continue;
				if(!Entry.m_Names.empty())
					Entry.m_Names += ", ";
				Entry.m_Names += pString;
			}
		}
		return true;
	}

	inline bool ParseManifest(const unsigned char *pData, size_t DataSize, std::vector<SEntry> &Entries)
	{
		Entries.clear();
		if(pData == nullptr || DataSize == 0)
			return false;

		const char *pCurrent = reinterpret_cast<const char *>(pData);
		const char *pEnd = pCurrent + DataSize;
		while(pCurrent < pEnd)
		{
			const char *pLineEnd = static_cast<const char *>(memchr(pCurrent, '\n', static_cast<size_t>(pEnd - pCurrent)));
			if(pLineEnd == nullptr)
				pLineEnd = pEnd;
			const size_t LineSize = static_cast<size_t>(pLineEnd - pCurrent);
			if(LineSize > 1 && pCurrent[0] == '{')
			{
				json_value *pRoot = JsonParse(pCurrent, LineSize);
				if(pRoot != nullptr)
				{
					SEntry Entry;
					if(ParseEntry(pRoot, Entry))
						Entries.push_back(std::move(Entry));
					json_value_free(pRoot);
				}
			}
			if(pLineEnd == pEnd)
				break;
			pCurrent = pLineEnd + 1;
		}
		return !Entries.empty();
	}

	inline const SEntry *FindLatest(const std::vector<SEntry> &Entries, const char *pMap, int Rank)
	{
		const SEntry *pBest = nullptr;
		for(const SEntry &Entry : Entries)
		{
			if(Entry.m_Rank != Rank || pMap == nullptr || str_comp_nocase(Entry.m_Map.c_str(), pMap) != 0)
				continue;
			if(pBest == nullptr || Entry.m_Ts >= pBest->m_Ts)
				pBest = &Entry;
		}
		return pBest;
	}
} // namespace qmclient::rank_demo

#endif
