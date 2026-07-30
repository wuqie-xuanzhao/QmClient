// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <engine/shared/json.h>

#include <gtest/gtest.h>

TEST(Json, Escape)
{
	char aBuf[128];
	char aSmall[2];
	char aSix[6];
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), ""), "");
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "a"), "a");
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "\n"), "\\n");
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "\\"), "\\\\"); // https://www.xkcd.com/1638/
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "\x1b"), "\\u001b"); // escape
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "愛"), "愛");
	EXPECT_STREQ(EscapeJson(aBuf, sizeof(aBuf), "😂"), "😂");

	// Truncations
	EXPECT_STREQ(EscapeJson(aSmall, sizeof(aSmall), "\\"), "");
	EXPECT_STREQ(EscapeJson(aSix, sizeof(aSix), "\x01"), "");
	EXPECT_STREQ(EscapeJson(aSix, sizeof(aSix), "aaaaaa"), "aaaaa");
}
