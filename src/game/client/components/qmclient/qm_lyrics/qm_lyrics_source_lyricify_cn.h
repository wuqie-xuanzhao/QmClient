#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LYRICIFY_CN_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LYRICIFY_CN_H

#include "qm_lyrics_source.h"

#include <memory>
#include <string>
#include <string_view>

class IHttp;

namespace QmLyrics
{

	struct SQqMusicSearchHit
	{
		std::string m_Id;
		std::string m_Mid;
		SMatchCandidate m_Metadata;
	};

	struct SNeteaseSearchHit
	{
		std::string m_Id;
		SMatchCandidate m_Metadata;
	};

	std::string BuildQqMusicSearchJson(const SSourceQuery &Query);
	std::string BuildQqMusicQrcPostBody(const std::string &SongId);
	std::string BuildQqMusicLyricUrl(const std::string &SongMid, int64_t CurrentMillis);
	bool IsQqMusicFamilyPlayerId(std::string_view PlayerId);
	bool ShouldUseQqMusicDirectSongId(const SSourceQuery &Query);
	bool ParseQqMusicSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SQqMusicSearchHit *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool DecryptQqMusicQrcHex(std::string_view Hex, std::string *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool ParseQqMusicQrcText(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool ParseQqMusicQrcDownloadResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool ParseQqMusicLyricResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	std::string BuildNeteaseSearchUrl(const SSourceQuery &Query);
	std::string BuildNeteaseLyricUrl(const std::string &SongId);
	std::string BuildNeteaseLyricPostBody(const std::string &SongId);
	bool IsNeteaseFamilyPlayerId(std::string_view PlayerId);
	bool ShouldUseNeteaseDirectSongId(const SSourceQuery &Query);
	bool ParseNeteaseSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SNeteaseSearchHit *pOut, char *pErr = nullptr, size_t ErrSize = 0);
	bool ParseNeteaseLyricResponse(const char *pBody, size_t BodyLen, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	class CLyricsSourceQqMusic : public IQmLyricsSource
	{
	public:
		struct SImpl;

		CLyricsSourceQqMusic(IHttp *pHttp, int TimeoutMs = 8000);
		~CLyricsSourceQqMusic() override;

		const char *Id() const override { return "qq"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

	class CLyricsSourceNetease : public IQmLyricsSource
	{
	public:
		struct SImpl;

		CLyricsSourceNetease(IHttp *pHttp, int TimeoutMs = 8000);
		~CLyricsSourceNetease() override;

		const char *Id() const override { return "netease"; }
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

	private:
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
