// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/system.h>
#include <base/thread.h>

#include <engine/engine.h>
#include <engine/http.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_amll_ttml_db.h>

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace QmLyrics;

namespace
{
	class CRecordingHttp : public IHttp
	{
	public:
		std::shared_ptr<IHttpRequest> m_pLastRequest;
		int m_RunCount = 0;

		void Run(std::shared_ptr<IHttpRequest> pRequest) override
		{
			m_pLastRequest = std::move(pRequest);
			++m_RunCount;
		}
		bool HasIpresolveBug() const override { return false; }
	};

	class CDeferredEngine : public IEngine
	{
		CJobPool m_Pool;
		std::vector<std::shared_ptr<IJob>> m_vQueuedJobs;
		bool m_Shutdown = false;

	public:
		CDeferredEngine()
		{
			m_Pool.Init(1);
		}

		~CDeferredEngine() override
		{
			ShutdownJobs();
		}

		void Init() override {}

		void AddJob(std::shared_ptr<IJob> pJob) override
		{
			m_vQueuedJobs.push_back(std::move(pJob));
		}

		void ShutdownJobs() override
		{
			if(m_Shutdown)
				return;
			m_Pool.Shutdown();
			m_Shutdown = true;
		}

		void SetAdditionalLogger(std::shared_ptr<ILogger> &&pLogger) override
		{
			(void)pLogger;
		}

		size_t QueuedJobCount() const
		{
			return m_vQueuedJobs.size();
		}

		void RunQueuedJobs()
		{
			std::vector<std::shared_ptr<IJob>> vJobs = std::move(m_vQueuedJobs);
			m_vQueuedJobs.clear();
			for(const std::shared_ptr<IJob> &pJob : vJobs)
			{
				m_Pool.Add(pJob);
				while(!pJob->Done())
					thread_yield();
			}
		}
	};

	void WriteStorageText(IStorage *pStorage, const char *pPath, const std::string &Text)
	{
		IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_NE(File, nullptr);
		ASSERT_EQ(io_write(File, Text.data(), Text.size()), Text.size());
		io_close(File);
	}

	void WriteFreshAmllIndex(IStorage *pStorage, const std::string &IndexText)
	{
		ASSERT_TRUE(pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE) || pStorage->FolderExists("qmclient", IStorage::TYPE_SAVE));
		ASSERT_TRUE(pStorage->CreateFolder("qmclient/lyrics", IStorage::TYPE_SAVE) || pStorage->FolderExists("qmclient/lyrics", IStorage::TYPE_SAVE));
		WriteStorageText(pStorage, "qmclient/lyrics/amll-ttml-db-index.jsonl", IndexText);
		WriteStorageText(pStorage, "qmclient/lyrics/amll-ttml-db-last-updated.txt", std::to_string(time_timestamp()));
	}

} // namespace

TEST(QmLyricsSourceAmllTtmlDb, BuildsIndexAndLyricUrls)
{
	EXPECT_EQ(BuildAmllTtmlDbIndexUrl("https://example.test/base/"), "https://example.test/base/metadata/raw-lyrics-index.jsonl");
	EXPECT_EQ(BuildAmllTtmlDbLyricUrl("artist/song file.ttml", "https://example.test/base/"), "https://example.test/base/raw-lyrics/artist/song%20file.ttml");
}

TEST(QmLyricsSourceAmllTtmlDb, ParsesIndexLineMetadata)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	Q.m_DurationSec = 269;
	const char *pLine = R"({
		"rawLyricFile": "jay/sunny-day.ttml",
		"metadata": [
			["musicName", ["Sunny Day"]],
			["artists", ["Jay"]],
			["album", ["Album A"]],
			["duration", [269]]
		]
	})";
	SAmllTtmlDbIndexHit Hit;
	ASSERT_TRUE(ParseAmllTtmlDbIndexLine(pLine, std::strlen(pLine), Q, &Hit));
	EXPECT_EQ(Hit.m_RawLyricFile, "jay/sunny-day.ttml");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
	EXPECT_EQ(Hit.m_Metadata.m_Artist, "Jay");
	EXPECT_EQ(Hit.m_Metadata.m_Album, "Album A");
	EXPECT_EQ(Hit.m_Metadata.m_DurationSec, 269);
	EXPECT_GT(Hit.m_Score, 90.0f);
}

TEST(QmLyricsSourceAmllTtmlDb, FindsBestIndexMatch)
{
	SSourceQuery Q;
	Q.m_Title = "Sunny Day";
	Q.m_Artist = "Jay";
	Q.m_Album = "Album A";
	const char *pIndex = R"({"rawLyricFile":"other.ttml","metadata":[["musicName",["Other"]],["artists",["Nobody"]],["album",["Other"]]]}
{"rawLyricFile":"jay/sunny-day.ttml","metadata":[["musicName",["Sunny Day"]],["artists",["Jay"]],["album",["Album A"]]]}
)";
	SAmllTtmlDbIndexHit Hit;
	ASSERT_TRUE(FindAmllTtmlDbBestMatch(pIndex, Q, &Hit));
	EXPECT_EQ(Hit.m_RawLyricFile, "jay/sunny-day.ttml");
	EXPECT_EQ(Hit.m_Metadata.m_Title, "Sunny Day");
}

TEST(QmLyricsSourceAmllTtmlDb, LyricResponseBecomesTtmlCandidate)
{
	SAmllTtmlDbIndexHit Hit;
	Hit.m_RawLyricFile = "jay/sunny-day.ttml";
	Hit.m_Metadata.m_Title = "Sunny Day";
	Hit.m_Metadata.m_Artist = "Jay";
	Hit.m_Score = 98.0f;
	const char *pBody = R"(<tt xmlns="http://www.w3.org/ns/ttml"><body><div><p begin="00:01.000" end="00:03.000">Hello</p></div></body></tt>)";
	SSourceCandidate Candidate;
	ASSERT_TRUE(ParseAmllTtmlDbLyricResponse(pBody, std::strlen(pBody), Hit, &Candidate));
	EXPECT_EQ(Candidate.m_SourceId, "amll-ttml-db");
	EXPECT_EQ(Candidate.m_FormatHint, EFormat::TTML);
	EXPECT_EQ(Candidate.m_Metadata.m_Title, "Sunny Day");
	EXPECT_NE(Candidate.m_RawText.find("<tt"), std::string::npos);
}

TEST(QmLyricsSourceAmllTtmlDb, FreshIndexSearchIsDeferredOffTheCallingThread)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteFreshAmllIndex(pStorage.get(),
		R"({"rawLyricFile":"other.ttml","metadata":[["musicName",["Other"]],["artists",["Nobody"]]]}
{"rawLyricFile":"jay/sunny-day.ttml","metadata":[["musicName",["Sunny Day"]],["artists",["Jay"]]]}
)");

	CRecordingHttp Http;
	CDeferredEngine Engine;
	CLyricsSourceAmllTtmlDb Source(&Http, pStorage.get(), &Engine, 8000, "https://example.test/base");
	SSourceQuery Query;
	Query.m_Title = "Sunny Day";
	Query.m_Artist = "Jay";
	int DoneCount = 0;
	Source.QueryAsync(Query, [&DoneCount](std::vector<SSourceCandidate>) { ++DoneCount; }, [](const char *) {});

	EXPECT_EQ(Engine.QueuedJobCount(), 1u);
	EXPECT_EQ(Http.m_RunCount, 0);
	EXPECT_EQ(DoneCount, 0);

	Engine.RunQueuedJobs();
	EXPECT_EQ(Http.m_RunCount, 0);
	Source.Tick();

	ASSERT_EQ(Http.m_RunCount, 1);
	auto pRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pRequest, nullptr);
	EXPECT_NE(std::string_view(pRequest->Url()).find("/raw-lyrics/jay/sunny-day.ttml"), std::string_view::npos);
	EXPECT_EQ(DoneCount, 0);
}

TEST(QmLyricsSourceAmllTtmlDb, CanceledIndexSearchCannotPublishTheOldSong)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteFreshAmllIndex(pStorage.get(),
		R"({"rawLyricFile":"artist/song-a.ttml","metadata":[["musicName",["Song A"]],["artists",["Artist"]]]}
{"rawLyricFile":"artist/song-b.ttml","metadata":[["musicName",["Song B"]],["artists",["Artist"]]]}
)");

	CRecordingHttp Http;
	CDeferredEngine Engine;
	CLyricsSourceAmllTtmlDb Source(&Http, pStorage.get(), &Engine, 8000, "https://example.test/base");
	SSourceQuery FirstQuery;
	FirstQuery.m_Title = "Song A";
	FirstQuery.m_Artist = "Artist";
	SSourceQuery LatestQuery = FirstQuery;
	LatestQuery.m_Title = "Song B";
	int DoneCount = 0;
	Source.QueryAsync(FirstQuery, [&DoneCount](std::vector<SSourceCandidate>) { ++DoneCount; }, [](const char *) {});
	Source.QueryAsync(LatestQuery, [&DoneCount](std::vector<SSourceCandidate>) { ++DoneCount; }, [](const char *) {});

	ASSERT_EQ(Engine.QueuedJobCount(), 2u);
	Engine.RunQueuedJobs();
	Source.Tick();

	ASSERT_EQ(Http.m_RunCount, 1);
	auto pRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pRequest, nullptr);
	EXPECT_NE(std::string_view(pRequest->Url()).find("/raw-lyrics/artist/song-b.ttml"), std::string_view::npos);
	EXPECT_EQ(std::string_view(pRequest->Url()).find("song-a.ttml"), std::string_view::npos);
	EXPECT_EQ(DoneCount, 0);
}

TEST(QmLyricsSourceAmllTtmlDb, ParsedIndexIsReusedAndRescoredForTheNextSong)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteFreshAmllIndex(pStorage.get(),
		R"({"rawLyricFile":"artist/song-a.ttml","metadata":[["musicName",["Song A"]],["artists",["Artist"]]]}
{"rawLyricFile":"artist/song-b.ttml","metadata":[["musicName",["Song B"]],["artists",["Artist"]]]}
)");

	CRecordingHttp Http;
	CDeferredEngine Engine;
	CLyricsSourceAmllTtmlDb Source(&Http, pStorage.get(), &Engine, 8000, "https://example.test/base");
	SSourceQuery Query;
	Query.m_Title = "Song A";
	Query.m_Artist = "Artist";
	Source.QueryAsync(Query, [](std::vector<SSourceCandidate>) {}, [](const char *) {});
	Engine.RunQueuedJobs();
	Source.Tick();
	ASSERT_EQ(Http.m_RunCount, 1);
	auto pFirstRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pFirstRequest, nullptr);
	EXPECT_NE(std::string_view(pFirstRequest->Url()).find("/raw-lyrics/artist/song-a.ttml"), std::string_view::npos);

	// Keep the file fresh but replace its contents. The second query must use the
	// immutable parsed cache from the first job and rescore it for the new song.
	WriteStorageText(pStorage.get(), "qmclient/lyrics/amll-ttml-db-index.jsonl",
		R"({"rawLyricFile":"wrong/song.ttml","metadata":[["musicName",["Wrong"]],["artists",["Nobody"]]]}
)");
	Query.m_Title = "Song B";
	Source.QueryAsync(Query, [](std::vector<SSourceCandidate>) {}, [](const char *) {});
	Engine.RunQueuedJobs();
	Source.Tick();

	ASSERT_EQ(Http.m_RunCount, 2);
	auto pSecondRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pSecondRequest, nullptr);
	EXPECT_NE(std::string_view(pSecondRequest->Url()).find("/raw-lyrics/artist/song-b.ttml"), std::string_view::npos);
	EXPECT_EQ(std::string_view(pSecondRequest->Url()).find("wrong/song.ttml"), std::string_view::npos);
}
