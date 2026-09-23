// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/client/friends.h>
#include <engine/client/serverbrowser.h>
#include <engine/client/serverbrowser_http_parse.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/favorites.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/sqlite.h>
#include <engine/storage.h>

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <memory>
#include <string>
#include <vector>

class CServerBrowserTestAccess
{
public:
	static void Initialize(CServerBrowser &Browser, IFriends *pFriends, IFavorites *pFavorites)
	{
		Browser.m_pFriends = pFriends;
		Browser.m_pFavorites = pFavorites;
	}
	static void Add(CServerBrowser &Browser, const NETADDR &Address, const CServerInfo &Info)
	{
		Browser.SetInfo(Browser.Add(&Address, 1), Info);
	}
	static void ReplaceFirstAddress(CServerBrowser &Browser, const NETADDR &Address)
	{
		Browser.ReplaceEntry(Browser.m_vpServerlist[0], &Address, 1);
	}
	static void SetFirstInfo(CServerBrowser &Browser, const CServerInfo &Info)
	{
		Browser.SetInfo(Browser.m_vpServerlist[0], Info);
	}
	static void Sort(CServerBrowser &Browser) { Browser.Sort(); }
};

namespace
{
	class CCountingFriends : public CFriends
	{
	public:
		mutable int m_Queries = 0;

		int GetFriendState(const char *pName, const char *pClan) const override
		{
			++m_Queries;
			return CFriends::GetFriendState(pName, pClan);
		}
	};

	class CServerBrowserStateTest : public ::testing::Test
	{
	protected:
		std::unique_ptr<CConfig> m_pSavedConfig;
		CCountingFriends m_Friends;
		std::unique_ptr<IFavorites> m_pFavorites = CreateFavorites();
		CServerBrowser m_Browser;

		void SetUp() override
		{
			m_pSavedConfig = std::make_unique<CConfig>(g_Config);
			g_Config.m_BrFilterEmpty = g_Config.m_BrFilterFull = g_Config.m_BrFilterPw = 0;
			g_Config.m_BrFilterCountry = g_Config.m_BrFilterFriends = g_Config.m_BrFilterSpectators = 0;
			g_Config.m_BrFilterUnfinishedMap = g_Config.m_BrFilterLogin = g_Config.m_BrFilterConnectingPlayers = 0;
			g_Config.m_BrFilterServerAddress[0] = g_Config.m_BrFilterGametype[0] = '\0';
			g_Config.m_BrFilterString[0] = g_Config.m_BrExcludeString[0] = '\0';
			g_Config.m_BrSort = IServerBrowser::SORT_NAME;
			g_Config.m_BrSortOrder = 0;
			g_Config.m_ClFriendsIgnoreClan = 0;
			CServerBrowserTestAccess::Initialize(m_Browser, &m_Friends, m_pFavorites.get());
		}

		void TearDown() override { g_Config = *m_pSavedConfig; }

		void AddServer(const NETADDR &Address)
		{
			CServerInfo Info{};
			str_copy(Info.m_aName, "Example");
			Info.m_NumClients = Info.m_NumPlayers = 1;
			Info.m_MaxClients = Info.m_MaxPlayers = 16;
			CServerInfo::CClient Client{};
			str_copy(Client.m_aName, "Alice");
			str_copy(Client.m_aClan, "Clan");
			Client.m_Player = true;
			Info.m_vClients.push_back(Client);
			CServerBrowserTestAccess::Add(m_Browser, Address, Info);
		}
	};
}

TEST_F(CServerBrowserStateTest, FriendStateIsReusedUntilFriendRevisionOrClanModeChanges)
{
	NETADDR Address;
	ASSERT_FALSE(net_addr_from_str(&Address, "127.0.0.1:8303"));
	AddServer(Address);
	CServerBrowserTestAccess::Sort(m_Browser);
	ASSERT_EQ(m_Friends.m_Queries, 1);
	EXPECT_EQ(m_Browser.Get(0)->m_FriendState, IFriends::FRIEND_NO);

	CServerBrowserTestAccess::Sort(m_Browser);
	EXPECT_EQ(m_Friends.m_Queries, 1);
	m_Friends.AddFriend("Alice", "Clan");
	CServerBrowserTestAccess::Sort(m_Browser);
	EXPECT_EQ(m_Friends.m_Queries, 2);
	EXPECT_EQ(m_Browser.Get(0)->m_FriendState, IFriends::FRIEND_PLAYER);

	g_Config.m_ClFriendsIgnoreClan = 1;
	CServerBrowserTestAccess::Sort(m_Browser);
	EXPECT_EQ(m_Friends.m_Queries, 3);

	CServerInfo Updated = *m_Browser.Get(0);
	str_copy(Updated.m_vClients[0].m_aName, "Bob");
	CServerBrowserTestAccess::SetFirstInfo(m_Browser, Updated);
	CServerBrowserTestAccess::Sort(m_Browser);
	EXPECT_EQ(m_Friends.m_Queries, 4);
	EXPECT_EQ(m_Browser.Get(0)->m_FriendState, IFriends::FRIEND_NO);
}

TEST_F(CServerBrowserStateTest, AddressReplacementInvalidatesFriendStateAndFriendListRevision)
{
	NETADDR Address;
	ASSERT_FALSE(net_addr_from_str(&Address, "127.0.0.1:8303"));
	const uint64_t EmptyRevision = m_Browser.FriendListRevision();
	AddServer(Address);
	EXPECT_NE(m_Browser.FriendListRevision(), EmptyRevision);
	CServerBrowserTestAccess::Sort(m_Browser);
	ASSERT_EQ(m_Friends.m_Queries, 1);

	const uint64_t LoadedRevision = m_Browser.FriendListRevision();
	ASSERT_FALSE(net_addr_from_str(&Address, "127.0.0.1:8304"));
	CServerBrowserTestAccess::ReplaceFirstAddress(m_Browser, Address);
	EXPECT_NE(m_Browser.FriendListRevision(), LoadedRevision);
	ASSERT_NE(m_Browser.Find(Address), nullptr);
	CServerBrowserTestAccess::Sort(m_Browser);
	EXPECT_EQ(m_Friends.m_Queries, 2);
	EXPECT_EQ(m_Browser.Get(0)->m_aAddresses[0], Address);
}

TEST(ServerBrowser, PingCache)
{
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;

	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";
	auto pPingCache = std::unique_ptr<IServerBrowserPingCache>(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	NETADDR Localhost4, Localhost6, OtherLocalhost4, OtherLocalhost6;
	ASSERT_FALSE(net_addr_from_str(&Localhost4, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&Localhost6, "[::1]:8304"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost4, "127.0.0.1:8305"));
	ASSERT_FALSE(net_addr_from_str(&OtherLocalhost6, "[::1]:8306"));
	EXPECT_LT(net_addr_comp(&Localhost4, &Localhost6), 0);
	NETADDR aLocalhostBoth[2] = {Localhost4, Localhost6};

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->Load();

	EXPECT_EQ(pPingCache->NumEntries(), 0);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	// Newer pings overwrite older.
	pPingCache->CachePing(Localhost4, 123);
	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost4, 345);
	pPingCache->CachePing(Localhost4, 456);
	pPingCache->CachePing(Localhost4, 567);
	pPingCache->CachePing(Localhost4, 678);
	pPingCache->CachePing(Localhost4, 789);
	pPingCache->CachePing(Localhost4, 890);
	pPingCache->CachePing(Localhost4, 901);
	pPingCache->CachePing(Localhost4, 135);
	pPingCache->CachePing(Localhost4, 246);
	pPingCache->CachePing(Localhost4, 357);
	pPingCache->CachePing(Localhost4, 468);
	pPingCache->CachePing(Localhost4, 579);
	pPingCache->CachePing(Localhost4, 680);
	pPingCache->CachePing(Localhost4, 791);
	pPingCache->CachePing(Localhost4, 802);
	pPingCache->CachePing(Localhost4, 913);

	EXPECT_EQ(pPingCache->NumEntries(), 1);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 913);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), -1);

	pPingCache->CachePing(Localhost4, 234);
	pPingCache->CachePing(Localhost6, 345);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 234);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// Port doesn't matter for overwriting.
	pPingCache->CachePing(Localhost4, 1337);
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	pPingCache.reset(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));

	// Persistence.
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
	EXPECT_EQ(pPingCache->GetPing(aLocalhostBoth, 2), 345);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&OtherLocalhost6, 1), 345);

	// 重复加载必须复位读取语句，周期性缓存刷新不能把 SQLITE_DONE 当作失败。
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&Localhost4, 1), 1337);
	EXPECT_EQ(pPingCache->GetPing(&Localhost6, 1), 345);
}

TEST(ServerBrowser, PingCacheIgnoresExpiredEntries)
{
	// 过期缓存值不能当实测延迟用：延迟列应回落到地区估算，而不是显示很久以前的测量值。
	CTestInfo Info;
	Info.m_DeleteTestStorageFilesOnSuccess = true;

	auto pConsole = CreateConsole(CFGFLAG_CLIENT);
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr) << "Error creating test storage";

	NETADDR OldAddr, FreshAddr;
	ASSERT_FALSE(net_addr_from_str(&OldAddr, "127.0.0.1:8303"));
	ASSERT_FALSE(net_addr_from_str(&FreshAddr, "127.0.0.2:8303"));

	// 直接写入缓存数据库：一条 30 天前的记录和一条刚写入的记录。
	{
		CSqlite pDisk = SqliteOpen(pConsole.get(), pStorage.get(), "ddnet-cache.sqlite3");
		ASSERT_TRUE(pDisk) << "Error opening ping cache database";
		ASSERT_EQ(sqlite3_exec(pDisk.get(), "CREATE TABLE IF NOT EXISTS server_pings (ip_address TEXT PRIMARY KEY NOT NULL, ping INTEGER NOT NULL, utc_timestamp TEXT NOT NULL)", nullptr, nullptr, nullptr), SQLITE_OK);
		ASSERT_EQ(sqlite3_exec(pDisk.get(), "INSERT OR REPLACE INTO server_pings (ip_address, ping, utc_timestamp) VALUES ('127.0.0.1', 171, datetime('now', '-30 days')), ('127.0.0.2', 123, datetime('now'))", nullptr, nullptr, nullptr), SQLITE_OK);
	}

	const int OldMaxAgeHours = g_Config.m_QmPingCacheMaxAgeHours;
	g_Config.m_QmPingCacheMaxAgeHours = 72;

	auto pPingCache = std::unique_ptr<IServerBrowserPingCache>(CreateServerBrowserPingCache(pConsole.get(), pStorage.get()));
	pPingCache->Load();
	EXPECT_EQ(pPingCache->NumEntries(), 2);
	EXPECT_EQ(pPingCache->GetPing(&OldAddr, 1), -1);
	EXPECT_EQ(pPingCache->GetPing(&FreshAddr, 1), 123);

	// 0 表示永不过期：旧记录重新可见。
	g_Config.m_QmPingCacheMaxAgeHours = 0;
	EXPECT_EQ(pPingCache->GetPing(&OldAddr, 1), 171);

	g_Config.m_QmPingCacheMaxAgeHours = OldMaxAgeHours;
}

namespace
{
	std::string HttpListEntry(const char *pAddresses, const char *pName = "Example")
	{
		return std::string("{\"addresses\":") + pAddresses + R"(,"location":"eu","info":{"max_clients":16,"max_players":16,"passworded":false,"game_type":"DDRace","name":")" + pName + R"(","map":{"name":"Map"},"version":"0.6","clients":[]}})";
	}

	bool ParseHttpListForTest(const std::string &Text, std::vector<CServerInfo> &vServers)
	{
		json_value *pJson = JsonParse(Text.c_str(), Text.size());
		const bool Failed = ServerBrowserParseHttpList(pJson, &vServers);
		json_value_free(pJson);
		return Failed;
	}
}

TEST(ServerBrowserHttpParse, PreservesAddressPreferenceAndSkipsUnsupportedServers)
{
	std::vector<CServerInfo> vServers;
	const std::string Text = "{\"servers\":[" +
				 HttpListEntry(R"(["tw-0.7+udp://127.0.0.1:8303","tw-0.6+udp://127.0.0.1:8304"])", "Mixed") + "," +
				 HttpListEntry(R"(["invalid://127.0.0.1:8303"])") + "," +
				 HttpListEntry(R"(["tw-0.7+udp://127.0.0.1:8305"])", "Seven") + "]}";
	ASSERT_FALSE(ParseHttpListForTest(Text, vServers));
	ASSERT_EQ(vServers.size(), 2u);
	EXPECT_STREQ(vServers[0].m_aName, "Mixed");
	EXPECT_EQ(vServers[0].m_NumAddresses, 1);
	EXPECT_EQ(vServers[0].m_aAddresses[0].port, 8304);
	EXPECT_STREQ(vServers[1].m_aName, "Seven");
	EXPECT_EQ(vServers[1].m_aAddresses[0].port, 8305);
}

TEST(ServerBrowserHttpParse, InvalidResponsePreservesPublishedList)
{
	std::vector<CServerInfo> vServers(1);
	str_copy(vServers[0].m_aName, "Old list");
	for(const std::string &Text : {std::string("not json"), std::string("{}"),
		    "{\"servers\":[" + HttpListEntry(R"(["tw-0.6+udp://127.0.0.1:8303"])") + R"(,{"addresses":false,"info":{}}]})"})
	{
		EXPECT_TRUE(ParseHttpListForTest(Text, vServers));
		ASSERT_EQ(vServers.size(), 1u);
		EXPECT_STREQ(vServers[0].m_aName, "Old list");
	}
}

TEST(ServerBrowserHttpParse, EmptySuccessReplacesOldListAndSkipsInvalidInfo)
{
	std::vector<CServerInfo> vServers(1);
	EXPECT_FALSE(ParseHttpListForTest(R"({"servers":[]})", vServers));
	EXPECT_TRUE(vServers.empty());
	const std::string Text = R"({"servers":[{"addresses":["tw-0.6+udp://127.0.0.1:8303"],"info":{}},)" +
				 HttpListEntry(R"(["tw-0.6+udp://127.0.0.1:8304"])") + "]}";
	ASSERT_FALSE(ParseHttpListForTest(Text, vServers));
	ASSERT_EQ(vServers.size(), 1u);
	EXPECT_EQ(vServers[0].m_aAddresses[0].port, 8304);
}
