// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>

#include <engine/client/serverbrowser_http_parse.h>
#include <engine/client/serverbrowser_ping_cache.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/sqlite.h>
#include <engine/storage.h>

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <memory>
#include <string>
#include <vector>

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
