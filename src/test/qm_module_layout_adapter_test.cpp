#include <game/client/QmUi/QmModuleLayoutAdapter.h>

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace qm_module;

TEST(QmModuleLayoutAdapter, ColumnIntRoundtrip)
{
	EXPECT_EQ(QmModuleColumnToInt(EQmModuleColumn::Full), 0);
	EXPECT_EQ(QmModuleColumnToInt(EQmModuleColumn::Left), 1);
	EXPECT_EQ(QmModuleColumnToInt(EQmModuleColumn::Right), 2);
	EXPECT_EQ(QmModuleColumnFromInt(0), EQmModuleColumn::Full);
	EXPECT_EQ(QmModuleColumnFromInt(1), EQmModuleColumn::Left);
	EXPECT_EQ(QmModuleColumnFromInt(2), EQmModuleColumn::Right);
}

TEST(QmModuleLayoutAdapter, ColumnStringRoundtrip)
{
	EXPECT_STREQ(QmModuleColumnToString(EQmModuleColumn::Full), "full");
	EXPECT_STREQ(QmModuleColumnToString(EQmModuleColumn::Left), "left");
	EXPECT_STREQ(QmModuleColumnToString(EQmModuleColumn::Right), "right");
	EQmModuleColumn Col;
	ASSERT_TRUE(ParseQmModuleColumnString("left", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Left);
	ASSERT_TRUE(ParseQmModuleColumnString("right", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Right);
	ASSERT_TRUE(ParseQmModuleColumnString("full", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Full);
	EXPECT_FALSE(ParseQmModuleColumnString("invalid", &Col));
	EXPECT_FALSE(ParseQmModuleColumnString(nullptr, &Col));
}

// 意图：stableId 映射是迁移兜底与 CModel 接入的单一事实源。
// QiaFen 以持久化 key qiafen 为权威（非 UI 名 keyword_reply），否则迁移丢用户布局。
TEST(QmModuleLayoutAdapter, StableIdMappingUsesPersistentKey)
{
	EXPECT_STREQ(QmModuleStableId(EQmModuleId::ChatBubble), "qm:chat_bubble");
	EXPECT_STREQ(QmModuleStableId(EQmModuleId::QiaFen), "qm:qiafen");
	EXPECT_STREQ(QmModuleStableId(EQmModuleId::Info), "qm:info");
}

TEST(QmModuleLayoutAdapter, StableIdReverseLookup)
{
	EQmModuleId Id;
	ASSERT_TRUE(QmModuleIdFromStableId("qm:chat_bubble", &Id));
	EXPECT_EQ(Id, EQmModuleId::ChatBubble);
	ASSERT_TRUE(QmModuleIdFromStableId("qm:qiafen", &Id));
	EXPECT_EQ(Id, EQmModuleId::QiaFen);
	EXPECT_FALSE(QmModuleIdFromStableId("qm:unknown", &Id));
	EXPECT_FALSE(QmModuleIdFromStableId("qm:nameplate_text", &Id)); // 栖梦枚举无它（已移除）
	EXPECT_FALSE(QmModuleIdFromStableId(nullptr, &Id));
}

// 意图：37 枚举的 stableId 必须唯一且双向可反查（迁移兜底全覆盖）。
TEST(QmModuleLayoutAdapter, AllModulesHaveUniqueReversibleStableId)
{
	std::set<std::string> Ids;
	for(size_t i = 0; i < QmModuleCount; ++i)
	{
		EQmModuleId Id = static_cast<EQmModuleId>(i);
		const char *pStable = QmModuleStableId(Id);
		ASSERT_NE(pStable, nullptr);
		EXPECT_TRUE(Ids.insert(pStable).second) << "重复 stableId: " << pStable;
		EQmModuleId Back;
		EXPECT_TRUE(QmModuleIdFromStableId(pStable, &Back));
		EXPECT_EQ(Back, Id);
	}
	EXPECT_EQ(Ids.size(), QmModuleCount);
}
