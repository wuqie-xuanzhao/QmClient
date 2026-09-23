#include "qm_realtime.h"

#include <engine/shared/json.h>
#include <engine/shared/protocol.h>

#include <generated/client_data.h>

#include <game/client/components/emoticon.h>

#include <algorithm>

namespace
{
	const json_value *ObjectField(const json_value *pObject, const char *pName)
	{
		return pObject && pObject->type == json_object ? json_object_get(pObject, pName) : nullptr;
	}
	bool IntegerField(const json_value *pObject, const char *pName, int &Out)
	{
		const json_value *pValue = ObjectField(pObject, pName);
		if(!pValue || pValue->type != json_integer)
			return false;
		Out = std::clamp((int)pValue->u.integer, 0, 1000000);
		return true;
	}
	bool Int64Field(const json_value *pObject, const char *pName, int64_t &Out)
	{
		const json_value *pValue = ObjectField(pObject, pName);
		if(!pValue || pValue->type != json_integer)
			return false;
		Out = std::clamp<int64_t>(pValue->u.integer, 0, 4102444800LL);
		return true;
	}
	const char *StringField(const json_value *pObject, const char *pName)
	{
		const json_value *pValue = ObjectField(pObject, pName);
		return pValue && pValue->type == json_string ? pValue->u.string.ptr : nullptr;
	}
}

bool ParseQmRealtimeMessage(const char *pData, size_t Size, SQmRealtimeMessage &OutMessage)
{
	OutMessage = {};
	if(!pData || !Size)
		return false;
	json_value *pRoot = json_parse(pData, Size);
	if(!pRoot || pRoot->type != json_object)
	{
		if(pRoot)
			json_value_free(pRoot);
		return false;
	}
	const json_value *pType = ObjectField(pRoot, "type");
	if(!pType || pType->type != json_string || !pType->u.string.ptr[0])
	{
		json_value_free(pRoot);
		return false;
	}
	OutMessage.m_Type = pType->u.string.ptr;
	if(str_comp(pType->u.string.ptr, "ping") == 0)
		OutMessage.m_Event = EQmRealtimeEvent::PING;
	else if(str_comp(pType->u.string.ptr, "pong") == 0)
		OutMessage.m_Event = EQmRealtimeEvent::PONG;
	else if(str_comp(pType->u.string.ptr, "state") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::STATE;
		const json_value *pDataObject = ObjectField(pRoot, "data");
		if(pDataObject && pDataObject->type == json_object)
		{
			OutMessage.m_StatePayloadValid = true;
			OutMessage.m_HasOnlineUsers = IntegerField(pDataObject, "online_users", OutMessage.m_OnlineUsers);
			OutMessage.m_HasOnlineDummies = IntegerField(pDataObject, "online_dummies", OutMessage.m_OnlineDummies);
		}
	}
	else if(str_comp(pType->u.string.ptr, "broadcast") == 0)
	{
		constexpr size_t MAX_QM_MARKDOWN_BROADCAST_BYTES = 64 * 1024;
		OutMessage.m_Event = EQmRealtimeEvent::BROADCAST;
		const json_value *pDataObject = ObjectField(pRoot, "data");
		const json_value *pMarkdown = ObjectField(pDataObject, "markdown");
		if(pMarkdown && pMarkdown->type == json_string && pMarkdown->u.string.length <= MAX_QM_MARKDOWN_BROADCAST_BYTES)
		{
			OutMessage.m_HasBroadcast = true;
			OutMessage.m_BroadcastMarkdown.assign(pMarkdown->u.string.ptr, pMarkdown->u.string.length);
			IntegerField(pDataObject, "version", OutMessage.m_BroadcastVersion);
		}
	}
	else if(str_comp(pType->u.string.ptr, "emoticon") != 0)
	{
		static const struct
		{
			const char *m_pName;
			EQmRealtimeEvent m_Event;
		} aEvents[] = {
			{"sponsors", EQmRealtimeEvent::SPONSORS},
			{"titles", EQmRealtimeEvent::TITLES},
			{"users", EQmRealtimeEvent::USERS},
			{"developers", EQmRealtimeEvent::DEVELOPERS},
			{"playtime", EQmRealtimeEvent::PLAYTIME},
			{"time", EQmRealtimeEvent::TIME},
			{"title_profile", EQmRealtimeEvent::TITLE_PROFILE},
			{"title_status", EQmRealtimeEvent::TITLE_STATUS},
			{"error", EQmRealtimeEvent::ERROR},
		};
		for(const auto &Event : aEvents)
		{
			if(str_comp(pType->u.string.ptr, Event.m_pName) == 0)
			{
				OutMessage.m_Event = Event.m_Event;
				const json_value *pDataObject = ObjectField(pRoot, "data");
				OutMessage.m_HasRealtimeData = pDataObject && pDataObject->type == json_object;
				if(OutMessage.m_HasRealtimeData)
				{
					OutMessage.m_HasServerTime = Int64Field(pDataObject, "server_time", OutMessage.m_ServerTime) || Int64Field(pDataObject, "time", OutMessage.m_ServerTime);
					OutMessage.m_HasPlaytimeSeconds = Int64Field(pDataObject, "playtime_seconds", OutMessage.m_PlaytimeSeconds) || Int64Field(pDataObject, "playtime", OutMessage.m_PlaytimeSeconds);
					if(Event.m_Event == EQmRealtimeEvent::TITLE_PROFILE)
					{
						const char *pTitle = StringField(pDataObject, "title");
						const char *pBoundName = StringField(pDataObject, "bound_name");
						const char *pStyle = StringField(pDataObject, "style");
						OutMessage.m_HasTitleProfile = pTitle || pBoundName || pStyle;
						if(pTitle)
							OutMessage.m_TitleText = pTitle;
						if(pBoundName)
							OutMessage.m_TitleBoundName = pBoundName;
						if(pStyle)
							OutMessage.m_TitleStyle = pStyle;
					}
					if(Event.m_Event == EQmRealtimeEvent::TITLE_STATUS)
					{
						const json_value *pAuthenticated = ObjectField(pDataObject, "authenticated");
						if(pAuthenticated && pAuthenticated->type == json_boolean)
						{
							OutMessage.m_HasTitleAuthenticated = true;
							OutMessage.m_TitleAuthenticated = pAuthenticated->u.boolean;
						}
					}
				}
				break;
			}
		}
	}
	else if(str_comp(pType->u.string.ptr, "emoticon") == 0)
	{
		OutMessage.m_Event = EQmRealtimeEvent::EMOTICON;
		const json_value *pDataObject = ObjectField(pRoot, "data");
		int PlayerId = -1;
		int EmoticonId = -1;
		const json_value *pPlayerName = ObjectField(pDataObject, "player_name");
		const json_value *pServerAddress = ObjectField(pDataObject, "server_address");
		const json_value *pLaunch = ObjectField(pDataObject, "launch");
		const json_value *pSuperLaunch = ObjectField(pDataObject, "super_launch");
		const json_value *pLaunchMode = ObjectField(pDataObject, "launch_mode");
		const bool HasPlayerId = IntegerField(pDataObject, "player_id", PlayerId);
		const bool HasEmoticonId = IntegerField(pDataObject, "emoticon", EmoticonId);
		OutMessage.m_EmoticonPayloadValid = HasPlayerId && HasEmoticonId && PlayerId >= 0 && PlayerId < MAX_CLIENTS && EmoticonId >= 0 && EmoticonId < NUM_EMOTICONS;
		OutMessage.m_EmoticonPlayerId = PlayerId;
		OutMessage.m_EmoticonId = EmoticonId;
		OutMessage.m_EmoticonLaunch = (pLaunch && pLaunch->type == json_boolean && pLaunch->u.boolean) || (pLaunchMode && pLaunchMode->type == json_boolean && pLaunchMode->u.boolean);
		OutMessage.m_EmoticonSuperLaunch = pSuperLaunch && pSuperLaunch->type == json_boolean && pSuperLaunch->u.boolean;
		if(pPlayerName && pPlayerName->type == json_string)
			OutMessage.m_EmoticonPlayerName = pPlayerName->u.string.ptr;
		if(pServerAddress && pServerAddress->type == json_string)
			OutMessage.m_EmoticonServerAddress = pServerAddress->u.string.ptr;
		const json_value *pClientId = ObjectField(pDataObject, "client_id");
		if(pClientId && pClientId->type == json_string)
			OutMessage.m_EmoticonClientId = pClientId->u.string.ptr;
		int Sequence = 0;
		if(IntegerField(pDataObject, "sequence", Sequence) && Sequence >= 0)
			OutMessage.m_EmoticonSequence = static_cast<uint64_t>(Sequence);
		OutMessage.m_HasRealtimeData = pDataObject && pDataObject->type == json_object;
	}
	if(OutMessage.m_Event == EQmRealtimeEvent::INVALID)
		OutMessage.m_Event = EQmRealtimeEvent::UNKNOWN;
	json_value_free(pRoot);
	return true;
}
