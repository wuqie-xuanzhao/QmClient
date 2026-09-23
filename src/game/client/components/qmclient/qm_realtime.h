#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_H

#include <cstddef>
#include <cstdint>
#include <string>

enum class EQmRealtimeEvent
{
	INVALID,
	UNKNOWN,
	PING,
	PONG,
	STATE,
	BROADCAST,
	SPONSORS,
	TITLES,
	USERS,
	DEVELOPERS,
	PLAYTIME,
	TIME,
	TITLE_PROFILE,
	TITLE_STATUS,
	EMOTICON,
	ERROR,
};

struct SQmRealtimeMessage
{
	EQmRealtimeEvent m_Event = EQmRealtimeEvent::INVALID;
	std::string m_Type;
	bool m_HasOnlineUsers = false;
	bool m_HasOnlineDummies = false;
	int m_OnlineUsers = 0;
	int m_OnlineDummies = 0;
	bool m_StatePayloadValid = false;
	bool m_HasBroadcast = false;
	std::string m_BroadcastMarkdown;
	int m_BroadcastVersion = 0;
	bool m_EmoticonPayloadValid = false;
	int m_EmoticonPlayerId = -1;
	int m_EmoticonId = -1;
	bool m_EmoticonLaunch = false;
	bool m_EmoticonSuperLaunch = false;
	bool m_HasRealtimeData = false;
	bool m_HasServerTime = false;
	int64_t m_ServerTime = 0;
	bool m_HasPlaytimeSeconds = false;
	int64_t m_PlaytimeSeconds = -1;
	bool m_HasTitleProfile = false;
	std::string m_TitleText;
	std::string m_TitleBoundName;
	std::string m_TitleStyle;
	bool m_HasTitleAuthenticated = false;
	bool m_TitleAuthenticated = false;
	uint64_t m_EmoticonSequence = 0;
	std::string m_EmoticonClientId;
	std::string m_EmoticonPlayerName;
	std::string m_EmoticonServerAddress;
};

bool ParseQmRealtimeMessage(const char *pData, size_t Size, SQmRealtimeMessage &OutMessage);

#endif
