// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace SystemMediaControls
{
	constexpr uint32_t ALBUM_ART_MAX_DIMENSION = 256;

	struct SAlbumArtDecodeSize
	{
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

	inline SAlbumArtDecodeSize CalculateAlbumArtDecodeSize(uint32_t Width, uint32_t Height)
	{
		if(Width == 0 || Height == 0)
			return {};
		if(Width <= ALBUM_ART_MAX_DIMENSION && Height <= ALBUM_ART_MAX_DIMENSION)
			return {Width, Height};

		if(Width >= Height)
		{
			const uint32_t ScaledHeight = (uint32_t)(((uint64_t)Height * ALBUM_ART_MAX_DIMENSION + Width / 2) / Width);
			return {ALBUM_ART_MAX_DIMENSION, ScaledHeight > 0 ? ScaledHeight : 1};
		}
		const uint32_t ScaledWidth = (uint32_t)(((uint64_t)Width * ALBUM_ART_MAX_DIMENSION + Height / 2) / Height);
		return {ScaledWidth > 0 ? ScaledWidth : 1, ALBUM_ART_MAX_DIMENSION};
	}

} // namespace SystemMediaControls

#if defined(CONF_FAMILY_WINDOWS) && defined(_MSC_VER)
#define SYSTEM_MEDIA_CONTROLS_WINRT_ENABLED 1
#else
#define SYSTEM_MEDIA_CONTROLS_WINRT_ENABLED 0
#endif

class CSystemMediaControls : public CComponent
{
public:
	struct SState
	{
		bool m_CanPlay = false;
		bool m_CanPause = false;
		bool m_CanPrev = false;
		bool m_CanNext = false;
		bool m_Playing = false;
		char m_aSourceAppId[128] = {};
		char m_aTitle[128] = {};
		char m_aArtist[128] = {};
		char m_aAlbum[128] = {};
		char m_aNeteaseSongId[128] = {};
		char m_aQqMusicSongId[128] = {};
		char m_aLinkedFileName[128] = {};
		int64_t m_PositionMs = 0;
		int64_t m_DurationMs = 0;
		int64_t m_PositionUpdatedTick = 0;
		uint64_t m_TimelineGeneration = 0;
		double m_PlaybackRate = 1.0;
		IGraphics::CTextureHandle m_AlbumArt;
		int m_AlbumArtWidth = 0;
		int m_AlbumArtHeight = 0;
	};

#if SYSTEM_MEDIA_CONTROLS_WINRT_ENABLED
	struct SWinrt;
	struct SShared;
#endif

	CSystemMediaControls();
	~CSystemMediaControls() override;
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;

	bool GetStateSnapshot(SState &State) const;
	void Previous();
	void PlayPause();
	void Next();

private:
#if SYSTEM_MEDIA_CONTROLS_WINRT_ENABLED
	std::unique_ptr<SWinrt> m_pWinrt;
	std::unique_ptr<SShared> m_pShared;
	std::thread m_Thread;
	std::atomic_bool m_StopThread{false};

	void ThreadMain();
#endif
};

#endif
