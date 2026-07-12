// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_APPLE_MUSIC_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_APPLE_MUSIC_H

#include "qm_lyrics_source.h"

#include <memory>
#include <string>
#include <string_view>

class IHttp;

namespace QmLyrics
{

	struct SAppleMusicSearchHit
	{
		std::string m_Id;
		std::string m_Reference;
		SMatchCandidate m_Metadata;
		float m_Score = 0.0f;
	};

	std::string BuildAppleMusicBrowseUrl();
	std::string BuildAppleMusicIndexJsUrl(std::string_view BrowseHtml);
	std::string ParseAppleMusicAccessToken(std::string_view JsText);
	bool ParseAppleMusicStorefrontResponse(const char *pBody, size_t BodyLen, std::string *pStorefront, std::string *pLanguage, char *pErr = nullptr, size_t ErrSize = 0);
	std::string BuildAppleMusicSearchUrl(const std::string &Storefront, const std::string &Language, const SSourceQuery &Query);
	bool ParseAppleMusicSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SAppleMusicSearchHit *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	std::string BuildAppleMusicLyricsUrl(const std::string &Storefront, const std::string &Language, const std::string &SongId);
	bool ParseAppleMusicLyricsResponse(const char *pBody, size_t BodyLen, const SAppleMusicSearchHit &Hit, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	class CLyricsSourceAppleMusic : public IQmLyricsSource
	{
	public:
		struct SImpl;

		CLyricsSourceAppleMusic(IHttp *pHttp, int TimeoutMs = 8000, const char *pMediaUserToken = nullptr);
		~CLyricsSourceAppleMusic() override;

		const char *Id() const override { return "apple-music"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
