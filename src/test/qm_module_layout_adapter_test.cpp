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

// 真实栖梦 s_aQmModuleDefaults 全 37 卡（key/column/order 与 menus_qmclient.cpp 一致）。
// 用于全量往返自洽测试——这是"行为等价"承诺的可信凭证。
static std::vector<SQmModuleEntry> MakeAll37Defaults()
{
	return {
		{EQmModuleId::Info, EQmModuleColumn::Full, 0, "info"},
		{EQmModuleId::ChatBubble, EQmModuleColumn::Left, 0, "chat_bubble"},
		{EQmModuleId::SkinTransition, EQmModuleColumn::Left, 1, "skin_transition"},
		{EQmModuleId::FocusMode, EQmModuleColumn::Left, 2, "focus_mode"},
		{EQmModuleId::GoresActor, EQmModuleColumn::Left, 3, "gores_actor"},
		{EQmModuleId::Gores, EQmModuleColumn::Left, 4, "gores"},
		{EQmModuleId::KeyBinds, EQmModuleColumn::Left, 5, "key_binds"},
		{EQmModuleId::MiniFeatures, EQmModuleColumn::Left, 6, "mini_features"},
		{EQmModuleId::JumpHint, EQmModuleColumn::Left, 7, "jump_hint"},
		{EQmModuleId::WeaponTrajectory, EQmModuleColumn::Left, 8, "weapon_trajectory"},
		{EQmModuleId::Coords, EQmModuleColumn::Left, 9, "coords"},
		{EQmModuleId::Streamer, EQmModuleColumn::Left, 10, "streamer"},
		{EQmModuleId::FriendNotify, EQmModuleColumn::Left, 11, "friend_notify"},
		{EQmModuleId::BlockWords, EQmModuleColumn::Left, 12, "block_words"},
		{EQmModuleId::Translate, EQmModuleColumn::Left, 14, "translate"},
		{EQmModuleId::TranslateUi, EQmModuleColumn::Left, 15, "translate_ui"},
		{EQmModuleId::QiaFen, EQmModuleColumn::Left, 13, "qiafen"},
		{EQmModuleId::PieMenu, EQmModuleColumn::Left, 16, "pie_menu"},
		{EQmModuleId::CameraView, EQmModuleColumn::Right, 0, "camera_view"},
		{EQmModuleId::WeaponAnimation, EQmModuleColumn::Right, 1, "weapon_animation"},
		{EQmModuleId::EntityOverlay, EQmModuleColumn::Right, 2, "entity_overlay"},
		{EQmModuleId::Laser, EQmModuleColumn::Right, 3, "laser"},
		{EQmModuleId::PlayerStats, EQmModuleColumn::Right, 4, "player_stats"},
		{EQmModuleId::CollisionHitbox, EQmModuleColumn::Right, 5, "collision_hitbox"},
		{EQmModuleId::FavoriteMaps, EQmModuleColumn::Right, 6, "favorite_maps"},
		{EQmModuleId::HJAssist, EQmModuleColumn::Right, 7, "hj_assist"},
		{EQmModuleId::SpeedrunTimer, EQmModuleColumn::Right, 8, "speedrun_timer"},
		{EQmModuleId::DebugGraph, EQmModuleColumn::Right, 9, "debug_graph"},
		{EQmModuleId::InputOverlay, EQmModuleColumn::Right, 10, "input_overlay"},
		{EQmModuleId::HudNotifications, EQmModuleColumn::Right, 11, "hud_notifications"},
		{EQmModuleId::Voice, EQmModuleColumn::Right, 12, "voice"},
		{EQmModuleId::DummyMiniView, EQmModuleColumn::Right, 13, "dummy_miniview"},
		{EQmModuleId::DynamicIsland, EQmModuleColumn::Right, 14, "dynamic_island"},
		{EQmModuleId::SystemMediaControls, EQmModuleColumn::Right, 15, "system_media_controls"},
		{EQmModuleId::Lyrics, EQmModuleColumn::Right, 16, "lyrics"},
		{EQmModuleId::Background3D, EQmModuleColumn::Right, 17, "background_3d"},
		{EQmModuleId::CardAppearance, EQmModuleColumn::Left, 17, "card_appearance"},
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

// 意图：ParseQmModuleColumnString 必须与栖梦 ParseQmModuleColumn 等价——
// 接受大小写不敏感（LEFT/Right/FULL）+ 整数形式 0/1/2（历史/手改 config 可能含这些）。
TEST(QmModuleLayoutAdapter, ParseColumnAcceptsNocaseAndInteger)
{
	EQmModuleColumn Col;
	ASSERT_TRUE(ParseQmModuleColumnString("LEFT", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Left);
	ASSERT_TRUE(ParseQmModuleColumnString("Right", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Right);
	ASSERT_TRUE(ParseQmModuleColumnString("FULL", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Full);
	ASSERT_TRUE(ParseQmModuleColumnString("0", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Full);
	ASSERT_TRUE(ParseQmModuleColumnString("1", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Left);
	ASSERT_TRUE(ParseQmModuleColumnString("2", &Col));
	EXPECT_EQ(Col, EQmModuleColumn::Right);
	EXPECT_FALSE(ParseQmModuleColumnString("3", &Col)); // 越界整数
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
	EXPECT_TRUE(ParseLegacyQmLayout("chat_bubble:left:0;camera_view:right:0", Defaults, Out));
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
	EXPECT_TRUE(ParseLegacyQmLayout("info:left:0;chat_bubble:full:0", Defaults, Out));
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[0].m_Column, EQmModuleColumn::Full); // info 强制 Full
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left); // chat_bubble 解析成 full 回退 Left
}

// 意图：容错——未知 key 跳过；重复 key 取首次；非法 column/order 字段跳过整条（回退默认）。
TEST(QmModuleLayoutAdapter, ParseToleratesBadKeys)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	EXPECT_TRUE(ParseLegacyQmLayout("unknown:left:0;chat_bubble:left:0;chat_bubble:right:1;camera_view:bad:0", Defaults, Out));
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left); // chat_bubble 取首次 left:0
	EXPECT_EQ(Out[1].m_OrderInColumn, 0);
	EXPECT_EQ(Out[2].m_Column, EQmModuleColumn::Right); // camera_view 非法 column → 默认 right
}

// 意图：空 config 返回 raw defaults（未经 SmartDefaults 均衡），且 Parsed=false。
// SmartDefaults 均衡由调用方（SyncQmModuleLayout）负责，适配器不掺和渲染高度耦合的均衡算法。
TEST(QmModuleLayoutAdapter, EmptyConfigReturnsRawDefaultsNotParsed)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	EXPECT_FALSE(ParseLegacyQmLayout("", Defaults, Out)); // 空 config 不算解析到
	ASSERT_EQ(Out.size(), 3u);
	EXPECT_EQ(Out[0].m_Column, EQmModuleColumn::Full); // raw defaults
	EXPECT_EQ(Out[1].m_Column, EQmModuleColumn::Left);
	EXPECT_EQ(Out[2].m_Column, EQmModuleColumn::Right);
}

// 意图：全未知 key（损坏 config / 迁移失败）Parsed=false，调用方据此走 SmartDefaults 回退。
TEST(QmModuleLayoutAdapter, ZeroParsedOnAllUnknownReturnsFalse)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	EXPECT_FALSE(ParseLegacyQmLayout("unknown1:left:0;unknown2:right:1", Defaults, Out));
	ASSERT_EQ(Out.size(), Defaults.size()); // 回退 raw defaults
}

// 意图：order 空洞由 Normalize 连续化（消除拖拽/迁移后的间距）。
TEST(QmModuleLayoutAdapter, NormalizeFillsOrderGaps)
{
	auto Defaults = MakeTestDefaults();
	std::vector<SQmModuleEntry> Out;
	EXPECT_TRUE(ParseLegacyQmLayout("chat_bubble:left:5", Defaults, Out));
	EXPECT_EQ(Out[1].m_OrderInColumn, 0); // Left 列仅 chat_bubble，Normalize 后 0
}

// 意图：真实 37 卡 serialize→parse 等价（行为等价回归基线的核心凭证）。
TEST(QmModuleLayoutAdapter, AllLegacyKeysRoundtripPreservesColumns)
{
	auto Defaults = MakeAll37Defaults();
	ASSERT_EQ(Defaults.size(), QmModuleCount);
	char aConfig[2048];
	SerializeLegacyQmLayout(Defaults, aConfig, sizeof(aConfig));
	std::vector<SQmModuleEntry> Out;
	EXPECT_TRUE(ParseLegacyQmLayout(aConfig, Defaults, Out));
	ASSERT_EQ(Out.size(), Defaults.size());
	for(size_t i = 0; i < Defaults.size(); ++i)
	{
		EXPECT_EQ(Out[i].m_Id, Defaults[i].m_Id) << "卡 " << Defaults[i].m_pKey;
		EXPECT_EQ(Out[i].m_Column, Defaults[i].m_Column) << "卡 " << Defaults[i].m_pKey << " 列变化";
	}
}

// === Step 2: CModel 接入 ===

// 意图：CModel 路径（Load+Serialize）必须等价 Step 1b 旧基线（Parse+Serialize），否则 CModel 接入破坏等价性。
TEST(QmModuleLayoutAdapter, LoadSerializeMatchesLegacyBaseline)
{
	auto Defaults = MakeAll37Defaults();
	const char *pConfig = "chat_bubble:left:0;camera_view:right:0;coords:left:3";
	std::vector<SQmModuleEntry> Legacy;
	ParseLegacyQmLayout(pConfig, Defaults, Legacy);
	char aLegacy[2048];
	SerializeLegacyQmLayout(Legacy, aLegacy, sizeof(aLegacy));
	LoadQmLayoutIntoModel(pConfig, Defaults);
	char aModel[2048];
	SerializeQmLayoutFromModel(aModel, sizeof(aModel));
	EXPECT_STREQ(aModel, aLegacy);
}

// 意图：Move 改 CModel 布局，Serialize 反映新位置；Move 后 dirty（触发序列化）。
TEST(QmModuleLayoutAdapter, MoveUpdatesModelAndSetsDirty)
{
	auto Defaults = MakeAll37Defaults();
	LoadQmLayoutIntoModel("chat_bubble:left:0;camera_view:right:0", Defaults);
	EXPECT_FALSE(IsQmLayoutModelDirty()); // Load 后清 dirty
	EXPECT_TRUE(MoveQmModuleInModel(EQmModuleId::ChatBubble, EQmModuleColumn::Right, 0));
	EXPECT_TRUE(IsQmLayoutModelDirty()); // Move 置 dirty
	char aBuf[2048];
	SerializeQmLayoutFromModel(aBuf, sizeof(aBuf));
	const std::string Result(aBuf);
	EXPECT_NE(Result.find("chat_bubble:right:"), std::string::npos);
	EXPECT_EQ(Result.find("chat_bubble:left:"), std::string::npos);
}

// 意图：Full 保护——非 Full 卡不可拖成 Full（目标 Full 拒绝）。
TEST(QmModuleLayoutAdapter, MoveRejectsFullTarget)
{
	auto Defaults = MakeAll37Defaults();
	LoadQmLayoutIntoModel("chat_bubble:left:0", Defaults);
	EXPECT_FALSE(MoveQmModuleInModel(EQmModuleId::ChatBubble, EQmModuleColumn::Full, 0));
}
