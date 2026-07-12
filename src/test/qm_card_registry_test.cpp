#include <game/client/QmUi/QmCardRegistry.h>
#include <game/localization.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

static qm_card_order::CModel RegistryModelAfterRoundTrip()
{
	const std::vector<qm_card_order::SEntry> Defaults = qm_card_registry::BuildDefaultEntries();
	qm_card_order::CModel Source;
	Source.SetEntries(Defaults);
	char aSerialized[32768];
	EXPECT_TRUE(Source.Serialize(aSerialized, sizeof(aSerialized)));
	qm_card_order::CModel Reloaded;
	EXPECT_TRUE(Reloaded.LoadMerged(aSerialized, Defaults));
	return Reloaded;
}
// 意图：注册表是迁移兜底与 SmartDefaults 的唯一依据，必须全覆盖、无重复、命名权威。
TEST(QmCardRegistry, CoversAllCardsNoDuplicates)
{
	const auto &Reg = qm_card_registry::Defaults();
	std::set<std::string> Ids;
	for(const auto &E : Reg)
		EXPECT_TRUE(Ids.insert(E.m_pStableId).second) << "重复 stableId: " << E.m_pStableId;
}

TEST(QmCardRegistry, P6RegistersQmClientOverviewCards)
{
	const auto *pIntro = qm_card_registry::FindByStableId("deck:qmclient-overview-intro");
	const auto *pGuide = qm_card_registry::FindByStableId("deck:qmclient-overview-guide");
	ASSERT_NE(pIntro, nullptr);
	ASSERT_NE(pGuide, nullptr);
	EXPECT_STREQ(pIntro->m_pDefaultTab, "qmclient-overview");
	EXPECT_STREQ(pGuide->m_pDefaultTab, "qmclient-overview");
	EXPECT_EQ(pIntro->m_DefaultColumn, qm_card_registry::ECardColumn::Full);
	EXPECT_EQ(pGuide->m_DefaultColumn, qm_card_registry::ECardColumn::Full);
	EXPECT_LT(pIntro->m_DefaultOrder, pGuide->m_DefaultOrder);
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
		"deck:general-game",
		"deck:general-language",
		"deck:general-client",
		"deck:general-recording",
		"deck:tee-identity",
		"deck:tee-skin-options",
		"deck:tee-skin-list",
		"deck:graphics-display",
		"deck:player-identity",
		"deck:player-country",
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
		"deck:appearance-hud-main",
		"deck:appearance-hud-ddrace",
		"deck:appearance-chat-settings",
		"deck:appearance-chat-messages",
		"deck:appearance-chat-preview",
		"deck:appearance-name-plate-settings",
		"deck:appearance-name-plate-preview",
		"deck:appearance-hook-collision-main",
		"deck:appearance-hook-collision-preview",
		"deck:appearance-info-messages",
		"deck:appearance-laser-enhanced",
		"deck:appearance-laser-colors",
		"deck:appearance-laser-preview",
		"deck:controls-mouse", "deck:controls-controller", "deck:controls-movement", "deck:controls-weapon",
		"deck:controls-voting", "deck:controls-chat", "deck:controls-dummy", "deck:controls-miscellaneous", "deck:controls-custom",
	};
	for(const char *pId : apIds)
		ASSERT_NE(qm_card_registry::FindByStableId(pId), nullptr) << pId;
}

TEST(QmCardRegistry, ControlsStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "controls", 1), (std::vector<std::string>{"deck:controls-mouse", "deck:controls-controller", "deck:controls-movement", "deck:controls-weapon"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "controls", 2), (std::vector<std::string>{"deck:controls-voting", "deck:controls-chat", "deck:controls-dummy", "deck:controls-miscellaneous", "deck:controls-custom"}));
}

// 意图：General 的默认卡片顺序必须随全局 model 往返持久化，避免重启后列投影或 Search 跳转丢失 placement。
TEST(QmCardRegistry, GeneralStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "general", 1),
		(std::vector<std::string>{"deck:general-game", "deck:general-client"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "general", 2),
		(std::vector<std::string>{"deck:general-language", "deck:general-recording"}));
} // 意图：appearance deck 的默认 placement 必须与运行时子页和列顺序对齐。

// 意图：Player 页的身份和国家选择必须在重启后保持左右两列的默认 placement，
// 否则全局 Search 或自定义排序会丢失目标卡片。
TEST(QmCardRegistry, PlayerStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "player", 1),
		(std::vector<std::string>{"deck:player-identity"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "player", 2),
		(std::vector<std::string>{"deck:player-country"}));
}

// 意图：Tee 页的身份、选项和皮肤列表必须在重启后保持全宽/左列 placement，
// 否则全局 Search 或自定义排序会丢失皮肤列表的目标卡片。
TEST(QmCardRegistry, TeeStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "tee", 0),
		(std::vector<std::string>{"deck:tee-identity", "deck:tee-skin-list"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "tee", 1),
		(std::vector<std::string>{"deck:tee-skin-options"}));
}

// 意图：Graphics 试点卡片的 placement 已是公共 registry 的事实源；补齐滚动时不得破坏序列化后的列顺序。
TEST(QmCardRegistry, GraphicsPilotPlacementSurvivesSerialization)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "graphics", 1),
		(std::vector<std::string>{"deck:graphics-display", "deck:graphics-visual", "deck:graphics-backend"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "graphics", 2),
		(std::vector<std::string>{"deck:graphics-modes"}));
}

// 意图：Sound 页的音频包卡必须默认位于右列，和运行时 Deck 的双列内容密度保持一致。
TEST(QmCardRegistry, SoundStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "sound", 1),
		(std::vector<std::string>{"deck:sound-toggle", "deck:sound-volume"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "sound", 2),
		(std::vector<std::string>{"deck:sound-audio-pack"}));
}

// 意图：DDNet 标准页的四张卡片按默认视觉顺序分布在左右列，重启后不能改变阅读顺序。
TEST(QmCardRegistry, DDNetStandardPageCardsPersistInVisualOrder)
{
	const qm_card_order::CModel Model = RegistryModelAfterRoundTrip();
	EXPECT_EQ(Model.StableIdOrder("deck:", "ddnet", 1),
		(std::vector<std::string>{"deck:ddnet-demo", "deck:ddnet-gameplay"}));
	EXPECT_EQ(Model.StableIdOrder("deck:", "ddnet", 2),
		(std::vector<std::string>{"deck:ddnet-background", "deck:ddnet-miscellaneous"}));
}

// 否则全局默认补位会把不同 appearance 子页混在同一个 tab 下，或留下 order 空洞。
TEST(QmCardRegistry, AppearanceDeckDefaultsUseSubPagePlacements)
{
	struct SExpectedPlacement
	{
		const char *m_pStableId;
		const char *m_pTab;
		qm_card_registry::ECardColumn m_Column;
		int m_Order;
	};
	const SExpectedPlacement aExpected[] = {
		{"deck:appearance-hud-main", "appearance-hud", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-hud-ddrace", "appearance-hud", qm_card_registry::ECardColumn::Right, 0},
		{"deck:appearance-chat-settings", "appearance-chat", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-chat-messages", "appearance-chat", qm_card_registry::ECardColumn::Right, 0},
		{"deck:appearance-chat-preview", "appearance-chat", qm_card_registry::ECardColumn::Left, 1},
		{"deck:appearance-name-plate-settings", "appearance-name-plate", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-name-plate-preview", "appearance-name-plate", qm_card_registry::ECardColumn::Right, 0},
		{"deck:appearance-hook-collision-main", "appearance-hook-collision", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-hook-collision-preview", "appearance-hook-collision", qm_card_registry::ECardColumn::Right, 0},
		{"deck:appearance-info-messages", "appearance-info-messages", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-laser-enhanced", "appearance-laser", qm_card_registry::ECardColumn::Left, 0},
		{"deck:appearance-laser-colors", "appearance-laser", qm_card_registry::ECardColumn::Left, 1},
		{"deck:appearance-laser-preview", "appearance-laser", qm_card_registry::ECardColumn::Right, 0},
	};
	for(const SExpectedPlacement &Expected : aExpected)
	{
		const auto *pDefault = qm_card_registry::FindByStableId(Expected.m_pStableId);
		ASSERT_NE(pDefault, nullptr) << Expected.m_pStableId;
		EXPECT_STREQ(pDefault->m_pDefaultTab, Expected.m_pTab) << Expected.m_pStableId;
		EXPECT_EQ(pDefault->m_DefaultColumn, Expected.m_Column) << Expected.m_pStableId;
		EXPECT_EQ(pDefault->m_DefaultOrder, Expected.m_Order) << Expected.m_pStableId;
	}
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
	EXPECT_GE(Model.StateIndexForStableId("deck:appearance-chat-preview"), 0);

	char aBuf[4096];
	Model.Serialize(aBuf, sizeof(aBuf));
	const std::string Serialized(aBuf);
	EXPECT_NE(Serialized.find("tclient:auto-reply|tclient|left|8;"), std::string::npos);
	EXPECT_NE(Serialized.find("deck:sound-audio-pack|sound|right|0;"), std::string::npos);
	EXPECT_NE(Serialized.find("deck:appearance-chat-preview|appearance-chat|left|1;"), std::string::npos);
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

// 意图：公共 Search 的 stable ID、标题和跳转目标都必须来自 registry/model，不能为 Search 建第二张 metadata 表。
TEST(QmCardRegistry, SearchReturnsCanonicalStableIdAndNavigationTarget)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	const auto Results = qm_card_registry::SearchCards("monitor", Model);
	const auto It = std::find_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return Result.m_pStableId != nullptr && std::string(Result.m_pStableId) == "deck:graphics-display";
	});
	ASSERT_NE(It, Results.end());
	EXPECT_EQ(std::count_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return Result.m_pStableId != nullptr && std::string(Result.m_pStableId) == "deck:graphics-display";
	}),
		1);
	EXPECT_STREQ(It->m_Target.m_pTab, "graphics");
	EXPECT_STREQ(It->m_Target.m_pStableId, It->m_pStableId);
	EXPECT_EQ(It->m_Description, Localize("Window and monitor"));
}

// 意图：历史/裁剪 model 缺少卡片时，Search 必须使用 registry 默认 tab，保证导航目标仍有效。
TEST(QmCardRegistry, SearchFallsBackToDefaultTabWhenModelDoesNotContainCard)
{
	qm_card_order::CModel Model;
	const auto Results = qm_card_registry::SearchCards("monitor", Model);
	const auto It = std::find_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return Result.m_pStableId != nullptr && std::string(Result.m_pStableId) == "deck:graphics-display";
	});
	ASSERT_NE(It, Results.end());
	EXPECT_STREQ(It->m_Target.m_pTab, "graphics");
}

// 意图：tab 是 model placement 而不是 Search metadata；移动后 Search 必须使用当前 tab，且不复制 registry 条目。
TEST(QmCardRegistry, SearchUsesCurrentModelTabWithoutDuplicatingMetadata)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	Model.MoveToTab("deck:graphics-display", "appearance-hud", 2, 0);
	const auto Results = qm_card_registry::SearchCards("monitor", Model);
	const auto It = std::find_if(Results.begin(), Results.end(), [](const qm_card_registry::SCardSearchResult &Result) {
		return Result.m_pStableId != nullptr && std::string(Result.m_pStableId) == "deck:graphics-display";
	});
	ASSERT_NE(It, Results.end());
	EXPECT_STREQ(It->m_Target.m_pTab, "appearance-hud");
}
// 意图：全局搜索页替代旧 QmClient 模块搜索后，registry 必须承接旧中文/拼音功能词。
// 否则用户搜索旧入口能搜到的具体功能时，会在新 Search 页漏掉对应 qm:* 卡片。
TEST(QmCardRegistry, QmCardsPreserveLegacyModuleSearchKeywords)
{
	struct SExpectedKeyword
	{
		const char *m_pStableId;
		const char *m_pKeyword;
	};
	const SExpectedKeyword aExpected[] = {
		{"qm:mini_features", "粒子拖尾"},
		{"qm:mini_features", "候选栏"},
		{"qm:friend_notify", "自动刷新"},
		{"qm:block_words", "屏蔽词"},
		{"qm:speedrun_timer", "速通"},
		{"qm:voice", "按住说话"},
		{"qm:background_3d", "月牙"},
		{"qm:chat_bubble", "消息气泡"},
	};
	for(const SExpectedKeyword &Expected : aExpected)
	{
		const auto *pDefault = qm_card_registry::FindByStableId(Expected.m_pStableId);
		ASSERT_NE(pDefault, nullptr) << Expected.m_pStableId;
		ASSERT_NE(pDefault->m_pSearchKeywords, nullptr) << Expected.m_pStableId;
		EXPECT_NE(std::string(pDefault->m_pSearchKeywords).find(Expected.m_pKeyword), std::string::npos)
			<< Expected.m_pStableId << " missing keyword " << Expected.m_pKeyword;
	}
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

// 意图：全局卡片的默认 placement 必须包含 tab，否则搜索结果和按 tab 筛选没有稳定落点。
TEST(QmCardRegistry, EveryRegisteredCardHasDefaultTabPlacement)
{
	for(const auto &E : qm_card_registry::Defaults())
	{
		EXPECT_NE(E.m_pDefaultTab, nullptr) << E.m_pStableId;
		if(E.m_pDefaultTab != nullptr)
			EXPECT_NE(E.m_pDefaultTab[0], '\0') << E.m_pStableId;
	}
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
