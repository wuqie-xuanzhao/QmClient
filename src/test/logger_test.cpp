// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/logger.h>
#include <base/system.h>

#include <gtest/gtest.h>

TEST(Logger, PrefixFileLoggerWritesOnlyMatchingSystems)
{
	CTestInfo Info;

	{
		IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_WRITE);
		ASSERT_TRUE(File);
		std::unique_ptr<ILogger> pLogger = log_logger_prefix_file(File, "perf/");
		ASSERT_TRUE(pLogger);

		CLogMessage PerfMessage;
		PerfMessage.m_Level = LEVEL_INFO;
		str_copy(PerfMessage.m_aSystem, "perf/settings-resource");
		str_copy(PerfMessage.m_aLine, "perf line");
		PerfMessage.m_LineLength = str_length(PerfMessage.m_aLine);

		CLogMessage MenuMessage = PerfMessage;
		str_copy(MenuMessage.m_aSystem, "menu");
		str_copy(MenuMessage.m_aLine, "menu line");
		MenuMessage.m_LineLength = str_length(MenuMessage.m_aLine);

		pLogger->Log(&PerfMessage);
		pLogger->Log(&MenuMessage);
	}

	IOHANDLE File = io_open(Info.m_aFilename, IOFLAG_READ);
	ASSERT_TRUE(File);
	char *pOutput = io_read_all_str(File);
	io_close(File);
	ASSERT_TRUE(pOutput);
	EXPECT_NE(str_find(pOutput, "perf line"), nullptr);
	EXPECT_EQ(str_find(pOutput, "menu line"), nullptr);
	free(pOutput);
	fs_remove(Info.m_aFilename);
}

TEST(Logger, PrefixRouterKeepsMatchingSystemsOutOfFallbackLogger)
{
	std::shared_ptr<CMemoryLogger> pPerfLogger = std::make_shared<CMemoryLogger>();
	std::shared_ptr<CMemoryLogger> pFallbackLogger = std::make_shared<CMemoryLogger>();
	std::unique_ptr<ILogger> pRouter = log_logger_prefix_router(pPerfLogger, pFallbackLogger, "perf/");

	CLogMessage PerfMessage;
	PerfMessage.m_Level = LEVEL_INFO;
	str_copy(PerfMessage.m_aSystem, "perf/settings-resource");
	str_copy(PerfMessage.m_aLine, "perf line");
	PerfMessage.m_LineLength = str_length(PerfMessage.m_aLine);

	CLogMessage MenuMessage = PerfMessage;
	str_copy(MenuMessage.m_aSystem, "menu");
	str_copy(MenuMessage.m_aLine, "menu line");
	MenuMessage.m_LineLength = str_length(MenuMessage.m_aLine);

	pRouter->Log(&PerfMessage);
	pRouter->Log(&MenuMessage);

	const std::vector<CLogMessage> vPerfLines = pPerfLogger->Lines();
	const std::vector<CLogMessage> vFallbackLines = pFallbackLogger->Lines();
	ASSERT_EQ(vPerfLines.size(), 1);
	ASSERT_EQ(vFallbackLines.size(), 1);
	EXPECT_STREQ(vPerfLines[0].m_aSystem, "perf/settings-resource");
	EXPECT_STREQ(vFallbackLines[0].m_aSystem, "menu");
}

TEST(Logger, PrefixRouterDoesNotQueueFallbackMessagesInUnresolvedPrefixLogger)
{
	std::shared_ptr<CFutureLogger> pPerfLogger = std::make_shared<CFutureLogger>();
	std::shared_ptr<CMemoryLogger> pFallbackLogger = std::make_shared<CMemoryLogger>();
	std::unique_ptr<ILogger> pRouter = log_logger_prefix_router(pPerfLogger, pFallbackLogger, "perf/");

	CLogMessage MenuMessage;
	MenuMessage.m_Level = LEVEL_INFO;
	str_copy(MenuMessage.m_aSystem, "menu");
	str_copy(MenuMessage.m_aLine, "menu line");
	MenuMessage.m_LineLength = str_length(MenuMessage.m_aLine);
	pRouter->Log(&MenuMessage);

	std::shared_ptr<CMemoryLogger> pResolvedPerfLogger = std::make_shared<CMemoryLogger>();
	pPerfLogger->Set(pResolvedPerfLogger);

	EXPECT_TRUE(pResolvedPerfLogger->Lines().empty());
	ASSERT_EQ(pFallbackLogger->Lines().size(), 1);
	EXPECT_STREQ(pFallbackLogger->Lines()[0].m_aSystem, "menu");
}
