#include <game/client/components/qmclient/keyword_reply_rules.h>

#include <gtest/gtest.h>

#include <cstring>

TEST(KeywordReplyRules, EncodesMultilineRulesForSingleLineConfig)
{
	const char *pRules = "你好=>在\n虾米=>在的";
	char aEncoded[128];
	QmKeywordReplyRules::EncodeForConfig(pRules, aEncoded, sizeof(aEncoded));

	EXPECT_STREQ(aEncoded, "你好=>在\\n虾米=>在的");
	EXPECT_EQ(strchr(aEncoded, '\n'), nullptr);
}

TEST(KeywordReplyRules, DecodesSecondLineChineseKeyword)
{
	const char *pEncoded = "你好=>在\\n虾米=>在的";
	char aDecoded[128];
	QmKeywordReplyRules::DecodeFromConfig(pEncoded, aDecoded, sizeof(aDecoded));

	EXPECT_STREQ(aDecoded, "你好=>在\n虾米=>在的");
}

TEST(KeywordReplyRules, KeepsLiteralBackslashN)
{
	const char *pRules = "路径=>C:\\new";
	char aEncoded[128];
	QmKeywordReplyRules::EncodeForConfig(pRules, aEncoded, sizeof(aEncoded));

	char aDecoded[128];
	QmKeywordReplyRules::DecodeFromConfig(aEncoded, aDecoded, sizeof(aDecoded));

	EXPECT_STREQ(aEncoded, "路径=>C:\\\\new");
	EXPECT_STREQ(aDecoded, pRules);
}
