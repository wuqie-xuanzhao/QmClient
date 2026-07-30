// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qmclient_utils.h"

#include <base/str.h>

#include <engine/shared/json.h>

#include <algorithm>
#include <unordered_map>

namespace
{

	const json_value *JsonObjectField(const json_value *pObject, const char *pName)
	{
		if(!pObject || pObject->type != json_object)
			return &json_value_none;
		return json_object_get(pObject, pName);
	}
	bool JsonReadBoolean(const json_value *pValue, bool &OutValue)
	{
		if(!pValue)
			return false;

		if(pValue->type == json_boolean)
		{
			OutValue = json_boolean_get(pValue) != 0;
			return true;
		}
		if(pValue->type == json_integer)
		{
			OutValue = pValue->u.integer != 0;
			return true;
		}
		if(pValue->type == json_double)
		{
			OutValue = pValue->u.dbl != 0.0;
			return true;
		}
		return false;
	}
	EClientBrand JsonReadClientBrand(const json_value *pEntry)
	{
		const json_value *pClientType = JsonObjectField(pEntry, "client_type");
		if(pClientType == &json_value_none)
			pClientType = JsonObjectField(pEntry, "type");
		if(pClientType == &json_value_none || pClientType->type != json_string)
			return EClientBrand::QM;

		const char *pType = pClientType->u.string.ptr;
		if(str_comp_nocase(pType, "arg") == 0 || str_comp_nocase(pType, "arghena") == 0)
			return EClientBrand::ARG;
		if(str_comp_nocase(pType, "qm") == 0 || str_comp_nocase(pType, "qmclient") == 0 || str_comp_nocase(pType, "q1meng") == 0)
			return EClientBrand::QM;
		return EClientBrand::QM;
	}
} // namespace

bool ParseQmClientUsersJson(const json_value *pRoot, const char *pServerAddress, SQmClientUsersParseResult &OutResult)
{
	OutResult = SQmClientUsersParseResult();
	if(!pRoot)
		return false;

	const json_value *pUsers = pRoot;
	if(pUsers->type == json_object)
	{
		const json_value *pUsersField = JsonObjectField(pUsers, "users");
		if(pUsersField != &json_value_none)
			pUsers = pUsersField;
	}

	if(pUsers->type != json_array)
		return false;

	OutResult.m_Parsed = true;
	std::unordered_map<std::string, size_t> ServerIndexByAddress;
	ServerIndexByAddress.reserve(pUsers->u.array.length);
	OutResult.m_vServerDistribution.reserve(pUsers->u.array.length);

	const char *pLocalServerAddress = pServerAddress ? pServerAddress : "";
	for(unsigned Index = 0; Index < pUsers->u.array.length; ++Index)
	{
		const json_value *pEntry = pUsers->u.array.values[Index];
		if(!pEntry || pEntry->type != json_object)
			continue;

		const json_value *pServerAddressField = JsonObjectField(pEntry, "server_address");
		if(pServerAddressField == &json_value_none)
			pServerAddressField = JsonObjectField(pEntry, "server");
		if(pServerAddressField == &json_value_none || pServerAddressField->type != json_string)
			continue;

		const json_value *pPlayerName = JsonObjectField(pEntry, "player_name");
		if(pPlayerName == &json_value_none)
			pPlayerName = JsonObjectField(pEntry, "name");
		if(pPlayerName == &json_value_none || pPlayerName->type != json_string || pPlayerName->u.string.ptr[0] == '\0')
			continue;

		bool Dummy = false;
		const json_value *pDummy = JsonObjectField(pEntry, "dummy");
		if(pDummy != &json_value_none)
			JsonReadBoolean(pDummy, Dummy);

		const std::string ServerAddress = pServerAddressField->u.string.ptr;
		const auto ItServer = ServerIndexByAddress.find(ServerAddress);
		if(ItServer == ServerIndexByAddress.end())
		{
			ServerIndexByAddress[ServerAddress] = OutResult.m_vServerDistribution.size();
			OutResult.m_vServerDistribution.push_back({ServerAddress, 0, 0});
		}
		SQmClientServerDistribution &ServerDistribution = OutResult.m_vServerDistribution[ServerIndexByAddress[ServerAddress]];
		if(Dummy)
		{
			++ServerDistribution.m_DummyCount;
			++OutResult.m_OnlineDummyCount;
		}
		else
		{
			++ServerDistribution.m_UserCount;
			++OutResult.m_OnlineUserCount;
		}

		if(str_comp(ServerAddress.c_str(), pLocalServerAddress) != 0)
			continue;

		SQmClientRecognitionMark &Mark = OutResult.m_vLocalServerMarks.emplace_back();
		Mark.m_Name = pPlayerName->u.string.ptr;
		Mark.m_ClientBrand = JsonReadClientBrand(pEntry);

		const json_value *pQidField = JsonObjectField(pEntry, "qid");
		if(pQidField != &json_value_none && pQidField->type == json_string)
			Mark.m_Qid = pQidField->u.string.ptr;

		const json_value *pFootParticlesEnabled = JsonObjectField(pEntry, "foot_particles_enabled");
		JsonReadBoolean(pFootParticlesEnabled, Mark.m_FootParticlesEnabled);

		const json_value *pRemoteParticlesEnabled = JsonObjectField(pEntry, "remote_particles_enabled");
		JsonReadBoolean(pRemoteParticlesEnabled, Mark.m_RemoteParticlesEnabled);

		Mark.m_VoiceSupported = true;
		const json_value *pVoiceSupported = JsonObjectField(pEntry, "voice_supported");
		if(pVoiceSupported != &json_value_none)
			JsonReadBoolean(pVoiceSupported, Mark.m_VoiceSupported);
	}

	std::sort(OutResult.m_vServerDistribution.begin(), OutResult.m_vServerDistribution.end(), [](const SQmClientServerDistribution &Left, const SQmClientServerDistribution &Right) {
		if(Left.m_UserCount != Right.m_UserCount)
			return Left.m_UserCount > Right.m_UserCount;
		if(Left.m_DummyCount != Right.m_DummyCount)
			return Left.m_DummyCount > Right.m_DummyCount;
		return str_comp(Left.m_ServerAddress.c_str(), Right.m_ServerAddress.c_str()) < 0;
	});
	return true;
}
