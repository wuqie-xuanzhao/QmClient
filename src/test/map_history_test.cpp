// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/components/tclient/map_history.h>
#include <game/client/components/qmclient/map_history_ui.h>

#include <gtest/gtest.h>

#include <limits>

using namespace QmMapHistory;

TEST(MapHistoryUi, ResponsiveGridUsesOneTwoOrThreeColumns)
{
	EXPECT_EQ(QmMapHistoryUi::GridColumns(507.0f), 1);
	EXPECT_EQ(QmMapHistoryUi::GridColumns(508.0f), 2);
	EXPECT_EQ(QmMapHistoryUi::GridColumns(765.0f), 2);
	EXPECT_EQ(QmMapHistoryUi::GridColumns(766.0f), 3);
	EXPECT_EQ(QmMapHistoryUi::GridColumns(2000.0f), 3);
}

TEST(MapHistoryUi, NarrowControlsStackBeforeCardGridBecomesCramped)
{
	EXPECT_TRUE(QmMapHistoryUi::StackControls(599.0f));
	EXPECT_FALSE(QmMapHistoryUi::StackControls(600.0f));
}

TEST(MapHistory, RepeatedVisitsAggregateByStableMapId)
{
	CMapHistory History;
	SMapHistoryRecord &First = History.RecordVisit("Kobra", "sha256:a", 100, "2026-07-01");
	First.m_DeathCount = 3;
	History.UpdatePlayTime("sha256:a", 12000);
	History.MarkFinished("sha256:a", 10000, 15000);

	SMapHistoryRecord &Second = History.RecordVisit("Kobra", "sha256:a", 200, "2026-07-02");
	EXPECT_EQ(History.Size(), 1u);
	EXPECT_EQ(&First, &Second);
	EXPECT_EQ(Second.m_LastEnteredAt, 200);
	EXPECT_EQ(Second.m_LastPlayedDate, "2026-07-02");
	EXPECT_EQ(Second.m_DeathCount, 3);
	EXPECT_FALSE(Second.m_Finished);
	EXPECT_EQ(Second.m_FinishTimeMs, 0);
}

TEST(MapHistory, LimitEvictsOldFinishedBeforeUnfinished)
{
	CMapHistory History;
	History.RecordVisit("Finished old", "finished-old", 10, "2026-07-01");
	History.MarkFinished("finished-old", 1000, 1000);
	History.RecordVisit("Unfinished old", "unfinished-old", 20, "2026-07-01");
	History.RecordVisit("Finished new", "finished-new", 30, "2026-07-01");
	History.MarkFinished("finished-new", 1000, 1000);

	History.ApplyLimit(2);
	EXPECT_EQ(History.Find("finished-old"), nullptr);
	EXPECT_NE(History.Find("unfinished-old"), nullptr);
	EXPECT_NE(History.Find("finished-new"), nullptr);

	History.ApplyLimit(1);
	EXPECT_EQ(History.Find("finished-new"), nullptr);
	EXPECT_NE(History.Find("unfinished-old"), nullptr);
}

TEST(MapHistory, DeathsAndFinishUpdateCurrentRecord)
{
	CMapHistory History;
	History.RecordVisit("A", "id-a", 100, "2026-07-01");
	EXPECT_TRUE(History.AddDeath("id-a"));
	EXPECT_TRUE(History.AddDeath("id-a", 2));
	EXPECT_TRUE(History.MarkFinished("id-a", 98765, 100000));

	const SMapHistoryRecord *pRecord = History.Find("id-a");
	ASSERT_NE(pRecord, nullptr);
	EXPECT_EQ(pRecord->m_DeathCount, 3);
	EXPECT_TRUE(pRecord->m_Finished);
	EXPECT_EQ(pRecord->m_FinishTimeMs, 98765);
	EXPECT_EQ(pRecord->m_PlayTimeMs, 100000);
}

TEST(MapHistory, DeathCountSaturatesInsteadOfOverflowing)
{
	CMapHistory History;
	History.RecordVisit("A", "id-a", 100, "2026-07-01");
	EXPECT_TRUE(History.AddDeath("id-a", std::numeric_limits<int>::max()));
	EXPECT_TRUE(History.AddDeath("id-a"));

	const SMapHistoryRecord *pRecord = History.Find("id-a");
	ASSERT_NE(pRecord, nullptr);
	EXPECT_EQ(pRecord->m_DeathCount, std::numeric_limits<int>::max());
}

TEST(MapHistory, JsonRoundTripPreservesRecords)
{
	CMapHistory History;
	History.RecordVisit("A", "id-a", 100, "2026-07-01");
	History.AddDeath("id-a", 4);
	History.UpdatePlayTime("id-a", 7000);
	History.RecordVisit("B", "id-b", 200, "2026-07-02");
	History.MarkFinished("id-b", 60000, 80000);

	const std::string Json = History.ToJson();
	CMapHistory Loaded;
	char aErr[128];
	ASSERT_TRUE(Loaded.FromJson(Json, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(Loaded.Size(), 2u);
	const SMapHistoryRecord *pA = Loaded.Find("id-a");
	ASSERT_NE(pA, nullptr);
	EXPECT_FALSE(pA->m_Finished);
	EXPECT_EQ(pA->m_DeathCount, 4);
	EXPECT_EQ(pA->m_PlayTimeMs, 7000);
	const SMapHistoryRecord *pB = Loaded.Find("id-b");
	ASSERT_NE(pB, nullptr);
	EXPECT_TRUE(pB->m_Finished);
	EXPECT_EQ(pB->m_FinishTimeMs, 60000);
}

TEST(MapHistory, FromJsonToleratesMissingFieldsAndRejectsMalformed)
{
	const char *pJson = R"({
		"entries": [
			{"map_name":"Legacy","last_entered":123,"deaths":-5,"finished":false,"finish_time_ms":777},
			{"map_id":"missing-name","last_entered":456},
			{"map_name":"Huge","map_id":"huge","deaths":999999999999},
			{"map_name":"Done","map_id":"done","finished":true,"finish_time_ms":5000}
		]
	})";
	CMapHistory History;
	char aErr[128];
	ASSERT_TRUE(History.FromJson(pJson, aErr, sizeof(aErr))) << aErr;
	EXPECT_NE(History.Find("Legacy"), nullptr);
	EXPECT_EQ(History.Find("missing-name"), nullptr);
	const SMapHistoryRecord *pLegacy = History.Find("Legacy");
	ASSERT_NE(pLegacy, nullptr);
	EXPECT_EQ(pLegacy->m_DeathCount, 0);
	EXPECT_EQ(pLegacy->m_FinishTimeMs, 0);
	const SMapHistoryRecord *pHuge = History.Find("huge");
	ASSERT_NE(pHuge, nullptr);
	EXPECT_EQ(pHuge->m_DeathCount, std::numeric_limits<int>::max());

	CMapHistory Kept;
	Kept.RecordVisit("Keep", "keep", 1, "2026-07-01");
	EXPECT_FALSE(Kept.FromJson("{\"entries\":[", aErr, sizeof(aErr)));
	EXPECT_NE(Kept.Find("keep"), nullptr);
}

TEST(MapHistory, SortedFiltersUnfinishedAndRecent)
{
	CMapHistory History;
	History.RecordVisit("Finished", "finished", 300, "2026-07-03");
	History.MarkFinished("finished", 1000, 1000);
	History.RecordVisit("Unfinished old", "unfinished-old", 100, "2026-07-01");
	History.RecordVisit("Unfinished new", "unfinished-new", 200, "2026-07-02");

	const std::vector<SMapHistoryRecord> Unfinished = History.Sorted(EMapHistoryFilter::UNFINISHED);
	ASSERT_EQ(Unfinished.size(), 2u);
	EXPECT_EQ(Unfinished[0].m_MapId, "unfinished-new");
	EXPECT_EQ(Unfinished[1].m_MapId, "unfinished-old");

	const std::vector<SMapHistoryRecord> Recent = History.Sorted(EMapHistoryFilter::RECENT);
	ASSERT_EQ(Recent.size(), 3u);
	EXPECT_EQ(Recent[0].m_MapId, "finished");
	EXPECT_EQ(Recent[1].m_MapId, "unfinished-new");
	EXPECT_EQ(Recent[2].m_MapId, "unfinished-old");
}
