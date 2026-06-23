#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SONG_SEARCH_MAP_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SONG_SEARCH_MAP_H

#include "qm_lyrics_match.h"
#include "qm_lyrics_source.h"

#include <string>
#include <string_view>
#include <vector>

class IStorage;

namespace QmLyrics
{

	struct SSongSearchMapEntry
	{
		std::string m_OriginalTitle;
		std::string m_OriginalArtist;
		std::string m_OriginalAlbum;
		std::string m_MappedTitle;
		std::string m_MappedArtist;
		std::string m_MappedAlbum;
		bool m_IsMarkedAsPureMusic = false;
		std::string m_LyricsSearchProvider;
	};

	std::string CanonicalLyricsProviderId(std::string_view Provider);
	std::string SongSearchMapToJson(const std::vector<SSongSearchMapEntry> &vEntries);
	bool SongSearchMapFromJson(std::string_view Json, std::vector<SSongSearchMapEntry> *pvEntries, char *pErr = nullptr, size_t ErrSize = 0);
	bool LoadSongSearchMap(IStorage *pStorage, std::vector<SSongSearchMapEntry> *pvEntries);
	bool SaveSongSearchMap(IStorage *pStorage, const std::vector<SSongSearchMapEntry> &vEntries);

	const SSongSearchMapEntry *FindSongSearchMapping(const SMatchQuery &Query, const std::vector<SSongSearchMapEntry> &vEntries);
	SMatchQuery ApplySongSearchMapping(const SMatchQuery &Query, const SSongSearchMapEntry &Entry);
	SSourceCandidate BuildPureMusicCandidate(const SMatchQuery &Query);

} // namespace QmLyrics

#endif
