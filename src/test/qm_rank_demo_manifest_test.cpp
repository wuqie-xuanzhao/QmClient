#include <game/client/components/qmclient/rank_demo_manifest.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(QmRankDemoManifest, SelectsLatestRankOneEntryCaseInsensitively)
{
	const std::string Manifest =
		std::string(R"({"status":"ok","map":"Teleport 2","rank":1,"ts":1700,"time":"105.00","demo":"old.demo.gz","cid":1,"names":["old"]})") +
		"\n" + R"({"status":"ok","map":"Teleport 2","rank":1,"ts":1800,"time":"104.14","demo":"26f401bb-dee0-4874-8c58-ea4dd24a66b7-bc9084494e74caeb.demo.gz","cid":0,"names":["Markus777777","Namzar"]})" +
		"\n" + R"({"status":"ok","map":"Teleport 2","rank":2,"ts":1900,"demo":"rank2.demo.gz"})";
	std::vector<qmclient::rank_demo::SEntry> Entries;
	ASSERT_TRUE(qmclient::rank_demo::ParseManifest(reinterpret_cast<const unsigned char *>(Manifest.data()), Manifest.size(), Entries));
	ASSERT_EQ(Entries.size(), 3U);
	const auto *pEntry = qmclient::rank_demo::FindLatest(Entries, "teleport 2", 1);
	ASSERT_NE(pEntry, nullptr);
	EXPECT_EQ(pEntry->m_Demo, "26f401bb-dee0-4874-8c58-ea4dd24a66b7-bc9084494e74caeb.demo.gz");
	EXPECT_EQ(pEntry->m_Names, "Markus777777, Namzar");
}

TEST(QmRankDemoManifest, RejectsNonOkEntriesAndUnsafeDemoNames)
{
	const std::string Manifest =
		R"({"status":"failed","map":"Map","rank":1,"ts":4,"demo":"failed.demo.gz"})
{"status":"ok","map":"Map","rank":1,"ts":5,"demo":"../escape.demo.gz"}
{"status":"ok","map":"Map","rank":1,"ts":6,"demo":"safe.demo.gz"})";
	std::vector<qmclient::rank_demo::SEntry> Entries;
	ASSERT_TRUE(qmclient::rank_demo::ParseManifest(reinterpret_cast<const unsigned char *>(Manifest.data()), Manifest.size(), Entries));
	ASSERT_EQ(Entries.size(), 1U);
	EXPECT_EQ(Entries[0].m_Demo, "safe.demo.gz");
}

TEST(QmRankDemoManifest, AcceptsManifestWithoutTrailingNewline)
{
	const std::string Manifest = R"({"status":"ok","map":"Map","rank":1,"ts":1,"demo":"map.demo.gz"})";
	std::vector<qmclient::rank_demo::SEntry> Entries;
	EXPECT_TRUE(qmclient::rank_demo::ParseManifest(reinterpret_cast<const unsigned char *>(Manifest.data()), Manifest.size(), Entries));
	ASSERT_EQ(Entries.size(), 1U);
	EXPECT_EQ(Entries[0].m_Map, "Map");
}

TEST(QmRankDemoManifest, AcceptsLeadingBomAndWhitespace)
{
	const std::string Manifest = "\xef\xbb\xbf  \t{\"status\":\"ok\",\"map\":\"Map\",\"rank\":1,\"ts\":1,\"demo\":\"map.demo.gz\"} \r\n";
	std::vector<qmclient::rank_demo::SEntry> Entries;
	EXPECT_TRUE(qmclient::rank_demo::ParseManifest(reinterpret_cast<const unsigned char *>(Manifest.data()), Manifest.size(), Entries));
	ASSERT_EQ(Entries.size(), 1U);
	EXPECT_EQ(Entries[0].m_Demo, "map.demo.gz");
}
