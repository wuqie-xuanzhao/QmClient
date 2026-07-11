#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MEDIA_IDENTITY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MEDIA_IDENTITY_H

#include <base/system.h>

#include <game/client/components/system_media_controls.h>

namespace QmLyrics
{

	struct SMediaIdentity
	{
		bool m_Valid = false;
		char m_aSourceAppId[128] = {};
		char m_aTitle[128] = {};
		char m_aArtist[128] = {};
		char m_aAlbum[128] = {};
		char m_aNeteaseSongId[128] = {};
		char m_aQqMusicSongId[128] = {};
		char m_aLinkedFileName[128] = {};
	};

	inline bool MediaIdentityEquals(const SMediaIdentity &Identity, const CSystemMediaControls::SState &State)
	{
		return Identity.m_Valid &&
		       str_comp(Identity.m_aSourceAppId, State.m_aSourceAppId) == 0 &&
		       str_comp(Identity.m_aTitle, State.m_aTitle) == 0 &&
		       str_comp(Identity.m_aArtist, State.m_aArtist) == 0 &&
		       str_comp(Identity.m_aAlbum, State.m_aAlbum) == 0 &&
		       str_comp(Identity.m_aNeteaseSongId, State.m_aNeteaseSongId) == 0 &&
		       str_comp(Identity.m_aQqMusicSongId, State.m_aQqMusicSongId) == 0 &&
		       str_comp(Identity.m_aLinkedFileName, State.m_aLinkedFileName) == 0;
	}

	inline void SetMediaIdentity(SMediaIdentity *pIdentity, const CSystemMediaControls::SState &State)
	{
		pIdentity->m_Valid = true;
		str_copy(pIdentity->m_aSourceAppId, State.m_aSourceAppId);
		str_copy(pIdentity->m_aTitle, State.m_aTitle);
		str_copy(pIdentity->m_aArtist, State.m_aArtist);
		str_copy(pIdentity->m_aAlbum, State.m_aAlbum);
		str_copy(pIdentity->m_aNeteaseSongId, State.m_aNeteaseSongId);
		str_copy(pIdentity->m_aQqMusicSongId, State.m_aQqMusicSongId);
		str_copy(pIdentity->m_aLinkedFileName, State.m_aLinkedFileName);
	}

} // namespace QmLyrics

#endif
