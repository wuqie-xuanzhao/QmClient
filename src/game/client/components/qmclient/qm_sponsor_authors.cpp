#include "qm_sponsor_authors.h"

#include <base/math.h>

const std::array<QmSponsorAuthors::SAuthor, 3> &QmSponsorAuthors::Authors()
{
	static constexpr std::array<SAuthor, 3> s_aAuthors = {{
		{"qmclient-community-author-xuanmeng", "璇梦"},
		{"qmclient-community-author-dyl", "DYL"},
		{"qmclient-community-author-xiari", "夏日"},
	}};
	return s_aAuthors;
}

float QmSponsorAuthors::RowsHeight(float TeeSize, float LineSpacing)
{
	return Authors().size() * (maximum(0.0f, TeeSize) + maximum(0.0f, LineSpacing));
}
