// QmClient voice/distribution 静态源码合同。
#include "test.h"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

TEST(QmClientDistributionContract, SuccessLogsAreLatchedUntilFailureOrReset)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/components/qmclient/qmclient.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string Header = HeaderBuffer.str();
	EXPECT_NE(Header.find("bool m_QmClientDistributionSuccessLatched = false;"), std::string::npos);

	std::ifstream SourceFile(TestSourcePath("src/game/client/components/qmclient/qmclient.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();

	EXPECT_NE(Source.find("if(!m_QmClientDistributionSuccessLatched)\n\t\tLogQmClientDistributionRequestEvent(\"request\", aUrl);"), std::string::npos);
	EXPECT_NE(Source.find("m_QmClientDistributionSuccessLatched = true;"), std::string::npos);
	EXPECT_NE(Source.find("LogQmClientDistributionEvent(\"parse_ok\", Result.m_OnlineUserCount, Result.m_OnlineDummyCount, (int)Result.m_vLocalServerMarks.size());"), std::string::npos);
	EXPECT_NE(Source.find("m_QmClientDistributionSuccessLatched = false;\n\t\t\tLogQmClientDistributionFailureEvent(\"parse_failed\", \"users payload could not be parsed\");"), std::string::npos);
	EXPECT_NE(Source.find("m_QmClientDistributionSuccessLatched = false;\n\t\tLogQmClientDistributionFailureEvent(\"request_failed\", aFailure);"), std::string::npos);
	EXPECT_NE(Source.find("m_QmClientDistributionSuccessLatched = false;\n\tm_aQmClientAuthToken[0] = '\\0';"), std::string::npos);
}
