#include <game/client/components/qmclient/qm_sponsor_authors.h>

#include <gtest/gtest.h>

TEST(QmSponsorAuthors, ProvidesTheThreeDisplayedAuthors)
{
	const auto &Authors = QmSponsorAuthors::Authors();
	ASSERT_EQ(Authors.size(), 3u);
	EXPECT_STREQ(Authors[0].m_pName, "璇梦");
	EXPECT_STREQ(Authors[1].m_pName, "DYL");
	EXPECT_STREQ(Authors[2].m_pName, "夏日");
}

TEST(QmSponsorAuthors, RowsHeightUsesEveryPreviewRowAndSpacing)
{
	EXPECT_FLOAT_EQ(QmSponsorAuthors::RowsHeight(50.0f, 4.0f), 162.0f);
	EXPECT_FLOAT_EQ(QmSponsorAuthors::RowsHeight(-50.0f, -4.0f), 0.0f);
}
