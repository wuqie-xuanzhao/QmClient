#include <game/client/QmUi/QmCardRegistry.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// 意图：注册表是迁移兜底与 SmartDefaults 的唯一依据，必须全覆盖、无重复、命名权威。
TEST(QmCardRegistry, CoversAllCardsNoDuplicates)
{
	const auto &Reg = qm_card_registry::Defaults();
	// 栖梦 38 + 当前 Tclient 19 + deck 16 = 73
	EXPECT_GE(Reg.size(), 73u);
	std::set<std::string> Ids;
	for(const auto &E : Reg)
		EXPECT_TRUE(Ids.insert(E.m_pStableId).second) << "重复 stableId: " << E.m_pStableId;
}

// 意图：注册表必须覆盖当前 Tclient 运行时 section 的 stable card id。
// 这些 id 是全局 Search/迁移/默认补位的事实来源，漏掉会让对应卡片从全局卡片系统消失。
TEST(QmCardRegistry, CoversCurrentTClientSectionIds)
{
	const char *apIds[] = {
		"tclient:visual-font-cursor",
		"tclient:visual-nameplates",
		"tclient:visual-effects",
		"tclient:input",
		"tclient:anti-latency-tools",
		"tclient:improved-anti-ping",
		"tclient:execute-on-join",
		"tclient:voting",
		"tclient:auto-reply",
		"tclient:player-indicator",
		"tclient:pet",
		"tclient:hud",
		"tclient:tee-status-bar",
		"tclient:tile-outlines",
		"tclient:ghost-tools",
		"tclient:rainbow",
		"tclient:tee-trails",
		"tclient:background-draw",
		"tclient:finish-name",
	};
	for(const char *pId : apIds)
	{
		const auto *pDefault = qm_card_registry::FindByStableId(pId);
		ASSERT_NE(pDefault, nullptr) << pId;
		EXPECT_STREQ(pDefault->m_pDefaultTab, "tclient") << pId;
	}
}

// 意图：注册表必须覆盖当前 settings deck 卡片 stable id。
// deck 卡原来只有内存顺序，没有长期持久化；全局卡片系统必须给它们默认 placement。
TEST(QmCardRegistry, CoversCurrentSettingsDeckIds)
{
	const char *apIds[] = {
		"deck:graphics-display",
		"deck:graphics-visual",
		"deck:graphics-backend",
		"deck:graphics-modes",
		"deck:sound-toggle",
		"deck:sound-volume",
		"deck:sound-audio-pack",
		"deck:ddnet-demo",
		"deck:ddnet-gameplay",
		"deck:ddnet-background",
		"deck:ddnet-miscellaneous",
		"deck:tclient-bind-wheel-editor",
		"deck:tclient-bind-wheel-preview",
		"deck:tclient-status-bar-settings",
		"deck:tclient-status-bar-items",
		"deck:tclient-status-bar-preview",
	};
	for(const char *pId : apIds)
		ASSERT_NE(qm_card_registry::FindByStableId(pId), nullptr) << pId;
}

// 意图：deck 默认 placement 的 order 是 tab+column 内的局部顺序，不是跨所有设置页的全局序号。
// 否则首次迁移/缺失补位会在每个非 Qm 设置页留下大空洞，拖拽持久化后的默认顺序不稳定。
TEST(QmCardRegistry, SettingsDeckDefaultOrdersAreLocalToTabAndColumn)
{
	std::map<std::string, std::vector<int>> OrdersByGroup;
	for(const auto &E : qm_card_registry::Defaults())
	{
		const std::string Id = E.m_pStableId;
		if(Id.rfind("deck:", 0) != 0)
			continue;

		const std::string Tab = E.m_pDefaultTab != nullptr ? E.m_pDefaultTab : "";
		OrdersByGroup[Tab + "|" + std::to_string(static_cast<int>(E.m_DefaultColumn))].push_back(E.m_DefaultOrder);
	}

	ASSERT_FALSE(OrdersByGroup.empty());
	for(auto &Group : OrdersByGroup)
	{
		std::vector<int> &Orders = Group.second;
		std::sort(Orders.begin(), Orders.end());
		for(size_t i = 0; i < Orders.size(); ++i)
			EXPECT_EQ(Orders[i], static_cast<int>(i)) << Group.first;
	}
}

// 意图：默认全局模型必须从注册表一键构建，包含 Qm/Tclient/deck 全部默认 placement，
// 供首次迁移和新增卡片补位复用，避免各页面各自拼全局配置。
TEST(QmCardRegistry, BuildDefaultEntriesCoversEveryRegisteredCard)
{
	const auto &Reg = qm_card_registry::Defaults();
	const std::vector<qm_card_order::SEntry> Entries = qm_card_registry::BuildDefaultEntries();
	ASSERT_EQ(Entries.size(), Reg.size());

	qm_card_order::CModel Model;
	Model.SetEntries(Entries);
	EXPECT_GE(Model.StateIndexForStableId("qm:chat_bubble"), 0);
	EXPECT_GE(Model.StateIndexForStableId("tclient:auto-reply"), 0);
	EXPECT_GE(Model.StateIndexForStableId("deck:sound-audio-pack"), 0);

	char aBuf[4096];
	Model.Serialize(aBuf, sizeof(aBuf));
	const std::string Serialized(aBuf);
	EXPECT_NE(Serialized.find("tclient:auto-reply|tclient|left|8;"), std::string::npos);
	EXPECT_NE(Serialized.find("deck:sound-audio-pack|sound|left|2;"), std::string::npos);
}

// 意图：全局搜索页不能只展示 stableId 占位符，注册表要提供可读标题与搜索关键词。
// 这样用户搜索 "audio pack" / "Audio packs" 时能找到音频包卡片。
TEST(QmCardRegistry, ProvidesSearchMetadataForGlobalCards)
{
	const auto *AudioPack = qm_card_registry::FindByStableId("deck:sound-audio-pack");
	ASSERT_NE(AudioPack, nullptr);
	ASSERT_NE(AudioPack->m_pTitle, nullptr);
	EXPECT_STREQ(AudioPack->m_pTitle, "Audio packs");
	ASSERT_NE(AudioPack->m_pSearchKeywords, nullptr);
	EXPECT_NE(std::string(AudioPack->m_pSearchKeywords).find("audio pack"), std::string::npos);

	const auto *TClientInput = qm_card_registry::FindByStableId("tclient:input");
	ASSERT_NE(TClientInput, nullptr);
	ASSERT_NE(TClientInput->m_pTitle, nullptr);
	EXPECT_STREQ(TClientInput->m_pTitle, "Input");
}

// 意图：QiaFen 三名分裂（枚举 QiaFen / UI 名 keyword_reply / 持久化 key qiafen）是迁移最大陷阱。
// 注册表必须以持久化 key 为权威，否则迁移丢用户布局。
TEST(QmCardRegistry, QiaFenUsesPersistentKeyNotUiName)
{
	const auto *E = qm_card_registry::FindByStableId("qm:qiafen");
	ASSERT_NE(E, nullptr);
	EXPECT_EQ(std::string(E->m_pStableId), "qm:qiafen"); // 不是 qm:keyword_reply
}

// 意图：laser/nameplate_text 原本未归入 tab 分组（数据债），注册表必须给它们 tab。
TEST(QmCardRegistry, DataDebtCardsHaveTabAssignment)
{
	EXPECT_NE(qm_card_registry::FindByStableId("qm:laser")->m_pDefaultTab, nullptr);
	EXPECT_NE(qm_card_registry::FindByStableId("qm:nameplate_text")->m_pDefaultTab, nullptr);
}

// 意图：B2 迁移要把栖梦旧 key（chat_bubble）映射到新 stableId（qm:chat_bubble）。
// 映射函数从注册表派生（DRY）；QiaFen 以持久化 key qiafen 为权威，UI 名 keyword_reply 不映射。
TEST(QmCardRegistry, MigratesLegacyKeyToNamespaced)
{
	EXPECT_EQ(std::string(qm_card_registry::MigrateLegacyKey("chat_bubble")), "qm:chat_bubble");
	EXPECT_EQ(std::string(qm_card_registry::MigrateLegacyKey("qiafen")), "qm:qiafen");
	EXPECT_EQ(qm_card_registry::MigrateLegacyKey("keyword_reply"), nullptr); // UI 名不映射
}

// 意图：栖梦 38 个 m_pKey 必须全部可映射（迁移兜底全覆盖，无遗漏）。
TEST(QmCardRegistry, AllQimengLegacyKeysMigratable)
{
	for(const auto &E : qm_card_registry::Defaults())
	{
		std::string Id = E.m_pStableId;
		if(Id.rfind("qm:", 0) == 0)
		{
			std::string Legacy = Id.substr(3);
			EXPECT_NE(qm_card_registry::MigrateLegacyKey(Legacy.c_str()), nullptr)
				<< "未映射的栖梦 key: " << Legacy;
		}
	}
}
