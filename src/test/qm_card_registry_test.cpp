#include <game/client/QmUi/QmCardRegistry.h>

#include <gtest/gtest.h>

#include <set>
#include <string>

// 意图：注册表是迁移兜底与 SmartDefaults 的唯一依据，必须全覆盖、无重复、命名权威。
TEST(QmCardRegistry, CoversAllCardsNoDuplicates)
{
	const auto &Reg = qm_card_registry::Defaults();
	// 栖梦 38 + Tclient 15 + deck 16 = 69
	EXPECT_GE(Reg.size(), 69u);
	std::set<std::string> Ids;
	for(const auto &E : Reg)
		EXPECT_TRUE(Ids.insert(E.m_pStableId).second) << "重复 stableId: " << E.m_pStableId;
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
