#include <base/system.h>

#include <game/client/QmUi/SettingsCardDeck.h>
#include <game/client/QmUi/SettingsCardDeckLogic.h>

#include <gtest/gtest.h>

TEST(SettingsCardDeckLogic, MovesGraphicsAcrossColumnsWithoutOverwritingOtherDecks)
{
	settings_card_deck_logic::CLogic Logic;
	Logic.Load("graphics", "deck:graphics-display|sound|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;deck:ddnet-demo|ddnet|left|0;qm:chat_bubble|visual|left|0;");

	ASSERT_TRUE(Logic.Move("deck:graphics-visual", 2, 0));
	EXPECT_EQ(Logic.StableIdOrder(1), (std::vector<std::string>{"deck:graphics-display", "deck:graphics-backend"}));
	EXPECT_EQ(Logic.StableIdOrder(2), (std::vector<std::string>{"deck:graphics-visual", "deck:graphics-modes"}));
	EXPECT_FALSE(Logic.Move("deck:sound-toggle", 2, 0));
	EXPECT_FALSE(Logic.Move("deck:graphics-visual", 3, 0));

	char aSerialized[8192];
	ASSERT_TRUE(Logic.SerializeMerged("deck:graphics-display|sound|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;deck:ddnet-demo|ddnet|left|0;qm:chat_bubble|visual|left|0;", aSerialized, sizeof(aSerialized)));
	EXPECT_NE(str_find(aSerialized, "deck:graphics-display|graphics|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:graphics-visual|graphics|right|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:sound-toggle|sound|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:ddnet-demo|ddnet|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "qm:chat_bubble|visual|left|0"), nullptr);

	settings_card_deck_logic::CLogic Reloaded;
	Reloaded.Load("graphics", aSerialized);
	EXPECT_EQ(Reloaded.StableIdOrder(1), (std::vector<std::string>{"deck:graphics-display", "deck:graphics-backend"}));
	EXPECT_EQ(Reloaded.StableIdOrder(2), (std::vector<std::string>{"deck:graphics-visual", "deck:graphics-modes"}));
}

TEST(SettingsCardDeck, CommitsGenericDeckProjection)
{
	char aGlobalOrder[8192] = "deck:graphics-display|graphics|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;";
	settings_card_deck::CDeck Deck;
	Deck.Load("graphics", aGlobalOrder, sizeof(aGlobalOrder));

	ASSERT_TRUE(Deck.CommitDrop("deck:graphics-visual", 2, 0));
	EXPECT_EQ(Deck.ColumnForStableId("deck:graphics-visual"), 2);
	EXPECT_NE(str_find(aGlobalOrder, "deck:graphics-visual|graphics|right|0"), nullptr);
	EXPECT_NE(str_find(aGlobalOrder, "deck:sound-toggle|sound|left|0"), nullptr);
	EXPECT_NE(Deck.OrderedStableIds().size(), 0u);
}
TEST(SettingsCardDeck, CommitFailureKeepsProjectionConsistentWithGlobalOrder)
{
	char aGlobalOrder[] = "deck:graphics-display|graphics|left|0;deck:graphics-visual|graphics|left|1;";
	settings_card_deck::CDeck Deck;
	Deck.Load("graphics", aGlobalOrder, sizeof(aGlobalOrder));

	ASSERT_FALSE(Deck.CommitDrop("deck:graphics-visual", 2, 0));
	EXPECT_EQ(Deck.ColumnForStableId("deck:graphics-visual"), 1);
	EXPECT_NE(str_find(aGlobalOrder, "deck:graphics-visual|graphics|left|1"), nullptr);
}
