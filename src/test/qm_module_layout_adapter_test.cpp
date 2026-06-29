#include <game/client/QmUi/QmModuleLayoutAdapter.h>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using namespace qm_module;

// 测试用小规模 defaults（3 卡：Full/Left/Right 各一），复用真实 EQmModuleId 占位。
// ParseLegacyQmLayout 按 m_pKey 匹配，与 m_Id 数值无关。
static std::vector<SQmModuleEntry> MakeTestDefaults()
{
	return {
		{EQmModuleId::Info, EQmModuleColumn::Full, 0, "info"},
		{EQmModuleId::ChatBubble, EQmModuleColumn::Left, 0, "chat_bubble"},
		{EQmModuleId::CameraView, EQmModuleColumn::Right, 0, "camera_view"},
	};
}

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

// 意图：Parse/Serialize 往返保持用户布局，缺失卡用 defaults 兜底（栖梦已验证的"全量+补全"模型）。
TEST(QmModuleLayoutAdapter, ParseSerializeRoundtripKeepsDefaults)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	ParseLegacyQmLayout("chat_bubble:left:0;camera_view:right:0", Defaults, Out);
	char aBuf[256];
	SerializeLegacyQmLayout(Out, aBuf, sizeof(aBuf));
	const std::string Result(aBuf);
	EXPECT_NE(Result.find("chat_bubble:left:0"), std::string::npos);
	EXPECT_NE(Result.find("camera_view:right:0"), std::string::npos);
	EXPECT_NE(Result.find("info:full:0"), std::string::npos); // 缺失卡 defaults 兜底
}

// 意图：Full 列保护——info（默认 Full）不可改列；非 Full 卡不可拖成 Full（回退默认列）。
TEST(QmModuleLayoutAdapter, FullColumnProtection)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	ParseLegacyQmLayout("info:left:0;chat_bubble:full:0", Defaults, Out);
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[0].m_Column, EQmModuleColumn::Full); // info 强制 Full
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left); // chat_bubble 解析成 full 回退 Left
}

// 意图：容错——未知 key 跳过；重复 key 取首次；非法 column/order 字段跳过整条（回退默认）。
TEST(QmModuleLayoutAdapter, ParseToleratesBadKeys)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	ParseLegacyQmLayout("unknown:left:0;chat_bubble:left:0;chat_bubble:right:1;camera_view:bad:0", Defaults, Out);
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left); // chat_bubble 取首次 left:0
	EXPECT_EQ(Out[1].m_OrderInColumn, 0);
	EXPECT_EQ(Out[2].m_Column, EQmModuleColumn::Right); // camera_view 非法 column → 默认 right
}

// 意图：空 config 走 defaults + Normalize（栖梦 SmartDefaults 的回退路径）。
TEST(QmModuleLayoutAdapter, EmptyConfigReturnsDefaults)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	ParseLegacyQmLayout("", Defaults, Out);
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[0].m_Column, EQmModuleColumn::Full);
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left);
	EXPECT_EQ(Out[2].m_Column, EQmModuleColumn::Right);
}

// 意图：order 空洞由 Normalize 连续化（消除拖拽/迁移后的间距）。
TEST(QmModuleLayoutAdapter, NormalizeFillsOrderGaps)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	ParseLegacyQmLayout("chat_bubble:left:5", Defaults, Out);
	EXPECT_EQ(Out[1].m_OrderInColumn, 0); // Left 列仅 chat_bubble，Normalize 后 0
}
