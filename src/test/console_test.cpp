#include "test.h"

#include <engine/console.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/gamecore.h>

#include <gtest/gtest.h>

#include <vector>

namespace
{
	struct SCommandResults
	{
		std::vector<int> m_vVictims;
	};

	void ConVictim(IConsole::IResult *pResult, void *pUser)
	{
		static_cast<SCommandResults *>(pUser)->m_vVictims.push_back(pResult->GetVictim());
	}
}

TEST(Console, QuotedVictimArgumentsAreValidated)
{
	auto pConsole = CreateConsole(CFGFLAG_SERVER);
	SCommandResults Results;
	pConsole->Register("victim", "v", CFGFLAG_SERVER, ConVictim, &Results, "");

	pConsole->ExecuteLine("victim \"all\"", 42);
	ASSERT_EQ(Results.m_vVictims.size(), MAX_CLIENTS);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		EXPECT_EQ(Results.m_vVictims[i], i);
	}

	Results.m_vVictims.clear();
	pConsole->ExecuteLine("victim \"me\"", 42);
	ASSERT_EQ(Results.m_vVictims.size(), 1);
	EXPECT_EQ(Results.m_vVictims[0], 42);

	Results.m_vVictims.clear();
	pConsole->ExecuteLine("victim \"3\"", 42);
	ASSERT_EQ(Results.m_vVictims.size(), 1);
	EXPECT_EQ(Results.m_vVictims[0], 3);

	Results.m_vVictims.clear();
	pConsole->ExecuteLine("victim \"\"", 42);
	EXPECT_TRUE(Results.m_vVictims.empty());
}
