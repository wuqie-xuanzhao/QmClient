#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_FRIEND_ONLINE_TRACKER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_FRIEND_ONLINE_TRACKER_H

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace qm_friend_notify
{
	struct CFriend
	{
		std::string m_Key;
		std::string m_Name;
		std::string m_Map;
		std::string m_Server;
		bool m_IsFriend = true;
	};

	class COnlineTracker
	{
		struct CState
		{
			CFriend m_Friend;
			int m_MissingSnapshots = 0;
		};

		std::unordered_map<std::string, CState> m_Online;
		std::unordered_set<std::string> m_ObservedServers;

	public:
		void Reset()
		{
			m_Online.clear();
			m_ObservedServers.clear();
		}

		// 每次传入独立成功刷新的完整快照；可用集合只包含玩家资料完整的服务器。
		std::vector<CFriend> Update(const std::vector<CFriend> &vFriends, const std::unordered_set<std::string> &AvailableServers)
		{
			std::vector<CFriend> vNotifications;
			std::unordered_set<std::string> Seen;
			for(const CFriend &Friend : vFriends)
			{
				auto It = m_Online.find(Friend.m_Key);
				bool Known = It != m_Online.end();
				if(!Known)
				{
					// 同服同名只改变战队时迁移身份，不产生新的上线事件。
					It = std::find_if(m_Online.begin(), m_Online.end(), [&](const auto &Entry) {
						return Entry.second.m_Friend.m_Name == Friend.m_Name && Entry.second.m_Friend.m_Server == Friend.m_Server;
					});
					Known = It != m_Online.end();
					if(Known)
						m_Online.erase(It);
				}
				if(!Known && Friend.m_IsFriend && m_ObservedServers.find(Friend.m_Server) != m_ObservedServers.end())
					vNotifications.push_back(Friend);
				m_Online[Friend.m_Key] = {Friend, 0};
				Seen.insert(Friend.m_Key);
			}

			for(auto It = m_Online.begin(); It != m_Online.end();)
			{
				if(Seen.find(It->first) != Seen.end())
				{
					++It;
					continue;
				}
				if(AvailableServers.find(It->second.m_Friend.m_Server) == AvailableServers.end())
				{
					// 整服缺失或资料不完整不能证明玩家离线，也不能延续缺席证据。
					It->second.m_MissingSnapshots = 0;
					++It;
				}
				else if(++It->second.m_MissingSnapshots >= 2)
					It = m_Online.erase(It);
				else
					++It;
			}

			// 各服首次完整出现时单独建立静默基线，避免暂缺服务器补齐后误报。
			m_ObservedServers.insert(AvailableServers.begin(), AvailableServers.end());
			return vNotifications;
		}
	};
}

#endif
