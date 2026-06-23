#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_AMLL_TTML_DB_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_AMLL_TTML_DB_H

#include "qm_lyrics_source.h"

#include <memory>
#include <string>
#include <string_view>

class IHttp;
class IStorage;

namespace QmLyrics
{

	struct SAmllTtmlDbIndexHit
	{
		std::string m_RawLyricFile;
		SMatchCandidate m_Metadata;
		float m_Score = 0.0f;
	};

	std::string BuildAmllTtmlDbIndexUrl(const char *pBaseUrl = nullptr);
	std::string BuildAmllTtmlDbLyricUrl(const std::string &RawLyricFile, const char *pBaseUrl = nullptr);
	bool ParseAmllTtmlDbIndexLine(const char *pLine, size_t LineLen, const SSourceQuery &Query, SAmllTtmlDbIndexHit *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool FindAmllTtmlDbBestMatch(std::string_view IndexText, const SSourceQuery &Query, SAmllTtmlDbIndexHit *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool ParseAmllTtmlDbLyricResponse(const char *pBody, size_t BodyLen, const SAmllTtmlDbIndexHit &Hit, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	class CLyricsSourceAmllTtmlDb : public IQmLyricsSource
	{
	public:
		struct SImpl;

		CLyricsSourceAmllTtmlDb(IHttp *pHttp, IStorage *pStorage, int TimeoutMs = 8000, const char *pBaseUrl = nullptr);
		~CLyricsSourceAmllTtmlDb() override;

		const char *Id() const override { return "amll-ttml-db"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
