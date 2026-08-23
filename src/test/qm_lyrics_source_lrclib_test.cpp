// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/http.h>

#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_media_identity.h>
#include <game/client/components/qmclient/qm_lyrics/qm_lyrics_source_lrclib.h>

#include <gtest/gtest.h>

#include <cstring>

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

	std::vector<SSourceCandidate> ParseGet(const char *pBody)
	{
		char aErr[128];
		return ParseLrclibGetResponse(pBody, std::strlen(pBody), aErr, sizeof(aErr));
	}

	std::vector<SSourceCandidate> ParseSearch(const char *pBody)
	{
		char aErr[128];
		return ParseLrclibSearchResponse(pBody, std::strlen(pBody), aErr, sizeof(aErr));
	}

} // namespace

TEST(QmLyricsSourceLrclibUrl, GetBasicQuery)
{
	SSourceQuery Q;
	Q.m_Title = "Bohemian Rhapsody";
	Q.m_Artist = "Queen";
	Q.m_DurationSec = 354;
	const std::string Url = BuildLrclibGetUrl(Q);
	EXPECT_NE(Url.find("https://lrclib.net/api/get"), std::string::npos);
	EXPECT_NE(Url.find("track_name=Bohemian%20Rhapsody"), std::string::npos);
	EXPECT_NE(Url.find("artist_name=Queen"), std::string::npos);
	EXPECT_NE(Url.find("duration=354"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, GetOmitsEmptyAndZeroDuration)
{
	SSourceQuery Q;
	Q.m_Title = "Hello";
	const std::string Url = BuildLrclibGetUrl(Q);
	EXPECT_NE(Url.find("track_name=Hello"), std::string::npos);
	EXPECT_EQ(Url.find("artist_name"), std::string::npos);
	EXPECT_EQ(Url.find("duration"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, GetEncodesSpecialChars)
{
	SSourceQuery Q;
	Q.m_Title = "AC/DC & friends";
	const std::string Url = BuildLrclibGetUrl(Q);
	// '/' '&' ' ' 都被编码
	EXPECT_NE(Url.find("AC%2FDC%20%26%20friends"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, SearchCombinesTitleAndArtist)
{
	SSourceQuery Q;
	Q.m_Title = "Hello";
	Q.m_Artist = "Adele";
	Q.m_Album = "25";
	Q.m_DurationSec = 295;
	const std::string Url = BuildLrclibSearchUrl(Q);
	EXPECT_NE(Url.find("/api/search"), std::string::npos);
	EXPECT_NE(Url.find("track_name=Hello"), std::string::npos);
	EXPECT_NE(Url.find("artist_name=Adele"), std::string::npos);
	EXPECT_NE(Url.find("album_name=25"), std::string::npos);
	EXPECT_EQ(Url.find("durationMs"), std::string::npos);
}

TEST(QmLyricsSourceLrclibUrl, OfficialGetUsesConfiguredBaseAndMinimumSignature)
{
	SSourceQuery Q;
	Q.m_Title = "Hello";
	Q.m_Artist = "Adele";
	Q.m_Album = "25";
	Q.m_DurationSec = 295;
	ASSERT_TRUE(CanUseLrclibGet(Q));
	const std::string Url = BuildLrclibGetUrl(Q, "https://lyrics.example/");
	EXPECT_EQ(Url.find("https://lyrics.example/api/get?"), 0u);
	EXPECT_EQ(Url.find("get-cached"), std::string::npos);
	EXPECT_NE(Url.find("duration=295"), std::string::npos);

	Q.m_Album.clear();
	Q.m_DurationSec = 0;
	EXPECT_TRUE(CanUseLrclibGet(Q));
	Q.m_Artist.clear();
	EXPECT_FALSE(CanUseLrclibGet(Q));
	EXPECT_TRUE(ShouldFallbackLrclibGet(404, false));
	EXPECT_TRUE(ShouldFallbackLrclibGet(200, false));
	EXPECT_FALSE(ShouldFallbackLrclibGet(200, true));
	EXPECT_FALSE(ShouldFallbackLrclibGet(500, false));
}

TEST(QmLyricsSourceLrclibUrl, LongEncodedQuerySurvivesHttpRequestCopy)
{
	SSourceQuery Q;
	for(int i = 0; i < 40; ++i)
		Q.m_Title.append("\xE6\xAD\x8C"); // U+6B4C
	for(int i = 0; i < 20; ++i)
		Q.m_Artist.append("\xE6\x89\x8B"); // U+624B
	Q.m_Album = Q.m_Title;
	Q.m_DurationSec = 300;

	const std::string Url = BuildLrclibGetUrl(Q);
	ASSERT_GT(Url.size(), 255u);
	ASSERT_LT(Url.size(), 2048u);
	std::unique_ptr<IHttpRequest> pRequest = CreateHttpRequest(Url.c_str());
	EXPECT_EQ(std::strlen(pRequest->Url()), Url.size());
	EXPECT_STREQ(pRequest->Url(), Url.c_str());
}

TEST(QmLyricsSourceLrclibProxy, HttpRequestOwnsProxyString)
{
	std::unique_ptr<IHttpRequest> pRequest = CreateHttpRequest("https://lrclib.net/api/search?track_name=x");
	std::string Proxy = "http://127.0.0.1:7890";
	pRequest->Proxy(Proxy.c_str());
	Proxy.assign("changed");
	EXPECT_STREQ(pRequest->ProxyUrl(), "http://127.0.0.1:7890");

	pRequest->Proxy("");
	EXPECT_STREQ(pRequest->ProxyUrl(), "");
}

TEST(QmLyricsSourceLrclibProxy, SourceAppliesProxyAndKeeps404VisibleForFallback)
{
	CRecordingHttp Http;
	CLyricsSourceLrclib Source(&Http, 8000, "https://lyrics.example", "http://127.0.0.1:7890");
	SSourceQuery Query;
	Query.m_Title = "Hello";
	Query.m_Artist = "Adele";
	Query.m_Album = "25";
	Query.m_DurationSec = 295;
	Source.QueryAsync(Query, [](std::vector<SSourceCandidate>) {}, [](const char *) {});
	ASSERT_EQ(Http.m_RunCount, 1);
	auto pRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pRequest, nullptr);
	EXPECT_NE(std::string_view(pRequest->Url()).find("/api/get?"), std::string_view::npos);
	EXPECT_EQ(std::string_view(pRequest->Url()).find("get-cached"), std::string_view::npos);
	EXPECT_STREQ(pRequest->ProxyUrl(), "http://127.0.0.1:7890");
	EXPECT_FALSE(pRequest->FailOnErrorStatusEnabled());
}

TEST(QmLyricsSourceLrclibProxy, RuntimeOptionsReplaceProxyAndApplyProxyTimeoutFloor)
{
	CRecordingHttp Http;
	CLyricsSourceLrclib Source(&Http, 8000, "https://lyrics.example", "http://old-proxy:7890");
	SSourceQuery Query;
	Query.m_Title = "Hello";
	Query.m_Artist = "Adele";

	Source.UpdateHttpOptions(8000, "http://127.0.0.1:7890");
	Source.QueryAsync(Query, [](std::vector<SSourceCandidate>) {}, [](const char *) {});
	ASSERT_EQ(Http.m_RunCount, 1);
	auto pRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pRequest, nullptr);
	EXPECT_STREQ(pRequest->ProxyUrl(), "http://127.0.0.1:7890");
	EXPECT_EQ(pRequest->RequestTimeoutMs(), 15000);
	EXPECT_EQ(Source.EffectiveTimeoutMsForTests(), 15000);

	Source.UpdateHttpOptions(20000, "http://127.0.0.1:7891");
	Source.QueryAsync(Query, [](std::vector<SSourceCandidate>) {}, [](const char *) {});
	ASSERT_EQ(Http.m_RunCount, 2);
	pRequest = std::static_pointer_cast<IHttpRequest>(Http.m_pLastRequest);
	ASSERT_NE(pRequest, nullptr);
	EXPECT_STREQ(pRequest->ProxyUrl(), "http://127.0.0.1:7891");
	EXPECT_EQ(pRequest->RequestTimeoutMs(), 20000);
	EXPECT_EQ(Source.EffectiveTimeoutMsForTests(), 20000);

	EXPECT_TRUE(LrclibHttpOptionsChanged(8000, "", 8000, "http://127.0.0.1:7890"));
	EXPECT_TRUE(LrclibHttpOptionsChanged(8000, "http://127.0.0.1:7890", 16000, "http://127.0.0.1:7890"));
	EXPECT_FALSE(LrclibHttpOptionsChanged(16000, "http://127.0.0.1:7890", 16000, "http://127.0.0.1:7890"));
}

TEST(QmLyricsSourceLrclibProxy, RuntimeOptionsCancelGenerationBeforeSourceCallbacks)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp");
	const size_t StateMachine = Source.find("void CQmLyrics::TickStateMachine()");
	ASSERT_NE(StateMachine, std::string::npos);
	const size_t OptionsChanged = Source.find("const bool HttpOptionsChanged", StateMachine);
	const size_t RestartForOptions = Source.find("if(RestartSearchForHttpOptions)", StateMachine);
	const size_t CancelForOptions = Source.find("CancelAllSources(m_pImpl.get());", RestartForOptions);
	const size_t SourceCallbacks = Source.find("for(std::unique_ptr<QmLyrics::IQmLyricsSource> &pSource", StateMachine);
	ASSERT_NE(OptionsChanged, std::string::npos);
	ASSERT_NE(RestartForOptions, std::string::npos);
	ASSERT_NE(CancelForOptions, std::string::npos);
	ASSERT_NE(SourceCallbacks, std::string::npos);
	EXPECT_LT(OptionsChanged, SourceCallbacks);
	EXPECT_LT(RestartForOptions, CancelForOptions);
	EXPECT_LT(CancelForOptions, SourceCallbacks);
}

TEST(QmLyricsClockLifecycle, DisabledLyricsStillConsumesPlaybackSnapshot)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/qmclient/qm_lyrics/qm_lyrics.cpp");
	const size_t StateMachine = Source.find("void CQmLyrics::TickStateMachine()");
	ASSERT_NE(StateMachine, std::string::npos);
	const size_t DisabledGuard = Source.find("if(!g_Config.m_QmLyrics)", StateMachine);
	ASSERT_NE(DisabledGuard, std::string::npos);
	const size_t DisabledReturn = Source.find("\n\t\treturn;", DisabledGuard);
	ASSERT_NE(DisabledReturn, std::string::npos);
	const size_t SnapshotRead = Source.find("GetStateSnapshot", DisabledGuard);
	const size_t ClockUpdate = Source.find("UpdatePlaybackClock", DisabledGuard);
	ASSERT_NE(SnapshotRead, std::string::npos);
	ASSERT_NE(ClockUpdate, std::string::npos);
	EXPECT_LT(SnapshotRead, DisabledReturn);
	EXPECT_LT(ClockUpdate, DisabledReturn);
}

TEST(QmLyricsMediaIdentity, DurationEnrichmentDoesNotChangeTrack)
{
	CSystemMediaControls::SState State;
	str_copy(State.m_aSourceAppId, "player");
	str_copy(State.m_aTitle, "Drown");
	str_copy(State.m_aArtist, "ZABO");
	QmLyrics::SMediaIdentity Identity;
	QmLyrics::SetMediaIdentity(&Identity, State);

	State.m_DurationMs = 161000;
	EXPECT_TRUE(QmLyrics::MediaIdentityEquals(Identity, State));

	str_copy(State.m_aTitle, "Another song");
	EXPECT_FALSE(QmLyrics::MediaIdentityEquals(Identity, State));
}

TEST(HttpRequestLogging, ExplicitAbortIsNotARequestFailure)
{
	EXPECT_FALSE(HttpShouldLogFailure(EHttpState::ABORTED, true));
	EXPECT_TRUE(HttpShouldLogFailure(EHttpState::ABORTED, false));
	EXPECT_TRUE(HttpShouldLogFailure(EHttpState::ERROR, true));
}

TEST(HttpRequestLogging, OnlyProgressCallbackMarksExplicitAbortCause)
{
	const std::string Header = ReadTestSourceFile("src/engine/http.h");
	const std::string Source = ReadTestSourceFile("src/engine/shared/http_curl.cpp");
	EXPECT_NE(Header.find("std::atomic<bool> m_AbortTriggeredByProgressCallback"), std::string::npos);

	const size_t ProgressCallback = Source.find("int CHttpRequestCurl::ProgressCallback");
	const size_t Completion = Source.find("void CHttpRequestCurl::OnCompletionInternal");
	ASSERT_NE(ProgressCallback, std::string::npos);
	ASSERT_NE(Completion, std::string::npos);
	EXPECT_NE(Source.find("m_AbortTriggeredByProgressCallback", ProgressCallback), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetSyncedLyricsBecomesEnhanced)
{
	const char *pBody = R"({
		"id": 123,
		"trackName": "Hello",
		"artistName": "Adele",
		"albumName": "25",
		"duration": 295,
		"syncedLyrics": "[00:01.00]<00:01.00>Hello <00:01.50>world",
		"plainLyrics": "Hello world"
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_Metadata.m_Title, "Hello");
	EXPECT_EQ(vC[0].m_Metadata.m_Artist, "Adele");
	EXPECT_EQ(vC[0].m_Metadata.m_DurationSec, 295);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::LRC_ENHANCED);
	EXPECT_NE(vC[0].m_RawText.find("[00:01.00]"), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetSyncedWithoutInlineTagsIsStandard)
{
	const char *pBody = R"({
		"trackName": "Plain Song",
		"artistName": "X",
		"duration": 100,
		"syncedLyrics": "[00:01.00]Plain"
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::LRC_STANDARD);
}

TEST(QmLyricsSourceLrclibParse, GetFallsBackToPlainWhenSyncedEmpty)
{
	const char *pBody = R"({
		"trackName": "X",
		"plainLyrics": "Line 1\nLine 2",
		"syncedLyrics": ""
	})";
	const auto vC = ParseGet(pBody);
	ASSERT_EQ(vC.size(), 1u);
	EXPECT_EQ(vC[0].m_FormatHint, EFormat::PLAIN);
	EXPECT_NE(vC[0].m_RawText.find("Line 1"), std::string::npos);
}

TEST(QmLyricsSourceLrclibParse, GetEmptyLyricsReturnsNoCandidates)
{
	const char *pBody = R"({"trackName":"X","syncedLyrics":"","plainLyrics":""})";
	const auto vC = ParseGet(pBody);
	EXPECT_EQ(vC.size(), 0u);
}

TEST(QmLyricsSourceLrclibParse, GetMalformedJsonReturnsEmpty)
{
	char aErr[128];
	const auto vC = ParseLrclibGetResponse("not json", 8, aErr, sizeof(aErr));
	EXPECT_EQ(vC.size(), 0u);
	EXPECT_GT(std::strlen(aErr), 0u);
}

TEST(QmLyricsSourceLrclibParse, SearchReturnsMultipleCandidates)
{
	const char *pBody = R"([
		{"trackName":"A","artistName":"X","duration":100,"syncedLyrics":"[00:01.00]A1"},
		{"trackName":"B","artistName":"Y","duration":200,"syncedLyrics":"[00:02.00]B1"},
		{"trackName":"C","syncedLyrics":""}
	])";
	const auto vC = ParseSearch(pBody);
	ASSERT_EQ(vC.size(), 2u);
	EXPECT_EQ(vC[0].m_Metadata.m_Title, "A");
	EXPECT_EQ(vC[1].m_Metadata.m_Title, "B");
}

TEST(QmLyricsSourceLrclibParse, SearchEmptyArray)
{
	const auto vC = ParseSearch("[]");
	EXPECT_EQ(vC.size(), 0u);
}

TEST(QmLyricsSourceLrclibParse, SearchObjectInsteadOfArrayReturnsEmpty)
{
	// 防御：服务端返回非数组也别崩
	const auto vC = ParseSearch(R"({"error":"not found"})");
	EXPECT_EQ(vC.size(), 0u);
}
