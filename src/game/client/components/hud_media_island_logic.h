/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H
#define GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H

#include <base/system.h>

#include <engine/graphics.h>

#include <cstdint>

struct SHudMediaIslandTrackInput
{
	const char *m_pTitle = "";
	const char *m_pArtist = "";
	const char *m_pAlbum = "";
	IGraphics::CTextureHandle m_Cover;
	bool m_HasCover = false;
	int64_t m_DurationMs = 0;
};

struct SHudMediaIslandTrackSnapshot
{
	char m_aTitle[128] = {};
	char m_aArtist[128] = {};
	char m_aAlbum[128] = {};
	IGraphics::CTextureHandle m_Cover;
	bool m_HasCover = false;
	int64_t m_DurationMs = 0;

	void Reset()
	{
		m_aTitle[0] = '\0';
		m_aArtist[0] = '\0';
		m_aAlbum[0] = '\0';
		m_Cover = IGraphics::CTextureHandle();
		m_HasCover = false;
		m_DurationMs = 0;
	}

	void SetFrom(const SHudMediaIslandTrackInput &Input)
	{
		str_copy(m_aTitle, Input.m_pTitle != nullptr ? Input.m_pTitle : "", sizeof(m_aTitle));
		str_copy(m_aArtist, Input.m_pArtist != nullptr ? Input.m_pArtist : "", sizeof(m_aArtist));
		str_copy(m_aAlbum, Input.m_pAlbum != nullptr ? Input.m_pAlbum : "", sizeof(m_aAlbum));
		m_Cover = Input.m_Cover;
		m_HasCover = Input.m_HasCover && Input.m_Cover.IsValid();
		m_DurationMs = Input.m_DurationMs;
	}

	bool HasMeaningfulIdentity() const
	{
		return m_aTitle[0] != '\0' || m_aArtist[0] != '\0' || m_aAlbum[0] != '\0';
	}
};

enum class EHudMediaIslandTrackUpdate
{
	NONE,
	FIRST_IDENTITY,
	TRACK_CHANGED,
};

inline bool QmHudMediaIslandTrackChanged(const SHudMediaIslandTrackSnapshot &Current, const SHudMediaIslandTrackInput &Next)
{
	if(!Current.HasMeaningfulIdentity())
		return false;

	const char *pNextTitle = Next.m_pTitle != nullptr ? Next.m_pTitle : "";
	const char *pNextArtist = Next.m_pArtist != nullptr ? Next.m_pArtist : "";
	const char *pNextAlbum = Next.m_pAlbum != nullptr ? Next.m_pAlbum : "";
	const bool HasComparableTitle = Current.m_aTitle[0] != '\0' && pNextTitle[0] != '\0';
	const bool HasComparableArtist = Current.m_aArtist[0] != '\0' && pNextArtist[0] != '\0';
	const bool HasComparableAlbum = Current.m_aAlbum[0] != '\0' && pNextAlbum[0] != '\0';

	return (HasComparableTitle && str_comp(Current.m_aTitle, pNextTitle) != 0) ||
	       (HasComparableArtist && str_comp(Current.m_aArtist, pNextArtist) != 0) ||
	       (HasComparableAlbum && str_comp(Current.m_aAlbum, pNextAlbum) != 0);
}

inline bool QmHudMediaIslandTrackInputHasMeaningfulIdentity(const SHudMediaIslandTrackInput &Input)
{
	return (Input.m_pTitle != nullptr && Input.m_pTitle[0] != '\0') ||
	       (Input.m_pArtist != nullptr && Input.m_pArtist[0] != '\0') ||
	       (Input.m_pAlbum != nullptr && Input.m_pAlbum[0] != '\0');
}

inline EHudMediaIslandTrackUpdate QmHudMediaIslandUpdateTrackSnapshots(
	SHudMediaIslandTrackSnapshot &Current,
	SHudMediaIslandTrackSnapshot &Outgoing,
	bool &HasTrackIdentity,
	bool &TrackTransitionActive,
	bool &TrackTransitionNeedsNodeReset,
	int64_t &TrackTransitionStartTick,
	int64_t Now,
	const SHudMediaIslandTrackInput &Next)
{
	if(!HasTrackIdentity || !Current.HasMeaningfulIdentity())
	{
		Current.SetFrom(Next);
		Outgoing.Reset();
		HasTrackIdentity = true;
		TrackTransitionActive = false;
		TrackTransitionNeedsNodeReset = false;
		TrackTransitionStartTick = 0;
		return EHudMediaIslandTrackUpdate::FIRST_IDENTITY;
	}

	if(!QmHudMediaIslandTrackChanged(Current, Next))
	{
		if(QmHudMediaIslandTrackInputHasMeaningfulIdentity(Next))
			Current.SetFrom(Next);
		return EHudMediaIslandTrackUpdate::NONE;
	}

	Outgoing = Current;
	Current.SetFrom(Next);
	TrackTransitionActive = true;
	TrackTransitionNeedsNodeReset = true;
	TrackTransitionStartTick = Now;
	return EHudMediaIslandTrackUpdate::TRACK_CHANGED;
}

#endif
