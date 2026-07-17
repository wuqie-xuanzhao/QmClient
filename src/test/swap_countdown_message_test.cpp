// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/components/tclient/swap_countdown_message.h>

#include <gtest/gtest.h>

TEST(SwapCountdownMessage, ParsesEnglishStartMessage)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Outgoing;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha has requested to swap with you. To complete the swap process please wait 3 seconds and then type /swap Alpha.", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Start);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Incoming);
	EXPECT_STREQ(aCounterpart, "Alpha");
}

TEST(SwapCountdownMessage, ParsesChineseStartMessage)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Outgoing;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha 请求与你交换位置。请等待 3 秒后输入 /swap Alpha 完成交换。", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Start);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Incoming);
	EXPECT_STREQ(aCounterpart, "Alpha");
}

TEST(SwapCountdownMessage, ParsesEnglishOutgoingStartMessage)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Incoming;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("You have requested to swap with Beta. Use /cancelswap to cancel the request.", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Start);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Outgoing);
	EXPECT_STREQ(aCounterpart, "Beta");
}

TEST(SwapCountdownMessage, ParsesChineseOutgoingStartMessage)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Incoming;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("你已向 Beta 发出交换请求。输入 /cancelswap 可取消", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Start);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Outgoing);
	EXPECT_STREQ(aCounterpart, "Beta");
}

TEST(SwapCountdownMessage, ParsesIncomingCancelMessages)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Outgoing;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha has canceled swap with you.", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Cancel);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Incoming);
	EXPECT_STREQ(aCounterpart, "Alpha");

	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha 已取消与你的交换。", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Cancel);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Incoming);
	EXPECT_STREQ(aCounterpart, "Alpha");
}

TEST(SwapCountdownMessage, ParsesOutgoingCancelMessages)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Incoming;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("You have canceled swap with Beta.", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Cancel);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Outgoing);
	EXPECT_STREQ(aCounterpart, "Beta");

	EXPECT_TRUE(ParseSwapCountdownMessage("你已取消与 Beta 的交换。", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Cancel);
	EXPECT_EQ(Direction, ESwapCountdownMessageDirection::Outgoing);
	EXPECT_STREQ(aCounterpart, "Beta");
}

TEST(SwapCountdownMessage, ParsesCompleteMessages)
{
	ESwapCountdownMessageAction Action = ESwapCountdownMessageAction::None;
	ESwapCountdownMessageDirection Direction = ESwapCountdownMessageDirection::Outgoing;
	char aCounterpart[64];
	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha has swapped with Beta.", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Complete);
	EXPECT_STREQ(aCounterpart, "");

	EXPECT_TRUE(ParseSwapCountdownMessage("Alpha 与 Beta 已完成交换。", Action, Direction, aCounterpart, sizeof(aCounterpart)));
	EXPECT_EQ(Action, ESwapCountdownMessageAction::Complete);
	EXPECT_STREQ(aCounterpart, "");
}
