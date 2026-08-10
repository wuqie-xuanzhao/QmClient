#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_RED_PACKET_AUTO_CLAIM_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_RED_PACKET_AUTO_CLAIM_H

#include <base/str.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>

class CQmRedPacketAutoClaim
{
	static constexpr size_t MAX_HANDLED_ANNOUNCEMENTS = 64;
	std::deque<std::string> m_HandledAnnouncements;

	static bool FitsChatCharacterLimit(const std::string &Password)
	{
		size_t CharacterCount = 0;
		const char *pCursor = Password.c_str();
		while(*pCursor)
		{
			str_utf8_decode(&pCursor);
			if(++CharacterCount > MAX_PASSWORD_CHARACTERS)
				return false;
		}
		return true;
	}

	static bool ExtractPassword(const char *pMessage, std::string &OutPassword)
	{
		OutPassword.clear();
		if(pMessage == nullptr)
			return false;

		constexpr std::string_view RedPacketMarker = "红包";
		constexpr std::string_view PasswordMarker = "输入口令";
		constexpr std::string_view ClaimMarker = "即可抢";
		constexpr std::string_view PasswordStartMarker = "「";
		constexpr std::string_view PasswordEndMarker = "」";
		const std::string_view Message(pMessage);

		const size_t RedPacketPosition = Message.find(RedPacketMarker);
		if(RedPacketPosition == std::string_view::npos)
			return false;

		const size_t PasswordMarkerPosition = Message.find(PasswordMarker, RedPacketPosition + RedPacketMarker.size());
		if(PasswordMarkerPosition == std::string_view::npos)
			return false;

		const size_t PasswordStartMarkerPosition = Message.find(PasswordStartMarker, PasswordMarkerPosition + PasswordMarker.size());
		if(PasswordStartMarkerPosition == std::string_view::npos)
			return false;

		const size_t PasswordStart = PasswordStartMarkerPosition + PasswordStartMarker.size();
		const size_t PasswordEnd = Message.find(PasswordEndMarker, PasswordStart);
		if(PasswordEnd == std::string_view::npos || PasswordEnd == PasswordStart)
			return false;

		const size_t ClaimMarkerPosition = Message.find(ClaimMarker, PasswordEnd + PasswordEndMarker.size());
		if(ClaimMarkerPosition == std::string_view::npos)
			return false;

		const std::string_view Password = Message.substr(PasswordStart, PasswordEnd - PasswordStart);
		OutPassword.assign(Password);
		if(!FitsChatCharacterLimit(OutPassword))
		{
			OutPassword.clear();
			return false;
		}
		return true;
	}

public:
	static constexpr size_t MAX_PASSWORD_CHARACTERS = 256;

	bool TryPrepare(const char *pServerAddress, const char *pMainPlayerName, const char *pMessage, std::string &OutPassword)
	{
		OutPassword.clear();
		if(pServerAddress == nullptr || std::string_view(pServerAddress) != "110.42.41.209:8303")
			return false;
		if(pMainPlayerName == nullptr || std::string_view(pMainPlayerName) != "璇梦")
			return false;
		if(!ExtractPassword(pMessage, OutPassword))
			return false;
		if(std::find(m_HandledAnnouncements.begin(), m_HandledAnnouncements.end(), pMessage) != m_HandledAnnouncements.end())
		{
			OutPassword.clear();
			return false;
		}
		m_HandledAnnouncements.emplace_back(pMessage);
		if(m_HandledAnnouncements.size() > MAX_HANDLED_ANNOUNCEMENTS)
			m_HandledAnnouncements.pop_front();
		return true;
	}

	void Reset()
	{
		m_HandledAnnouncements.clear();
	}
};

#endif
