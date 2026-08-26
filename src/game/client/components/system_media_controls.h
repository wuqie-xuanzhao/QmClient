// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_H
#define GAME_CLIENT_COMPONENTS_SYSTEM_MEDIA_CONTROLS_H

#include <engine/graphics.h>

#include <game/client/component.h>
#include <game/client/components/qmclient/netease_hook/qm_netease_hook_provider.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace SystemMediaControls
{
	constexpr uint32_t ALBUM_ART_MAX_DIMENSION = 256;

	inline bool AnyMediaSourceEnabled(bool SmtcEnabled, bool NeteaseHookEnabled)
	{
		return SmtcEnabled || NeteaseHookEnabled;
	}

	inline bool ShouldStopNeteaseHookForConfigurationChange(bool ConfigurationInitialized, bool WasHookEnabled)
	{
		return ConfigurationInitialized && WasHookEnabled;
	}

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

	inline float AlbumArtCircleMaskAlpha(float PixelCenterX, float PixelCenterY, uint32_t Width, uint32_t Height, float Feather)
	{
		if(Width == 0 || Height == 0 || Feather <= 0.0f)
			return 0.0f;

		const float CenterX = Width * 0.5f;
		const float CenterY = Height * 0.5f;
		const float RadiusX = CenterX - 0.5f;
		const float RadiusY = CenterY - 0.5f;
		if(RadiusX <= 0.0f || RadiusY <= 0.0f)
			return 0.0f;

		const float NormalizedX = (PixelCenterX - CenterX) / RadiusX;
		const float NormalizedY = (PixelCenterY - CenterY) / RadiusY;
		const float NormalizedDistance = std::sqrt(NormalizedX * NormalizedX + NormalizedY * NormalizedY);
		const float DistanceInside = (1.0f - NormalizedDistance) * std::min(RadiusX, RadiusY);
		const float Coverage = std::clamp(DistanceInside / Feather, 0.0f, 1.0f);
		return Coverage * Coverage * (3.0f - 2.0f * Coverage);
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
		bool m_HookValid = false;
		uint64_t m_HookSequence = 0;
		char m_aHookCurrentLine[256] = {};
		int64_t m_HookCurrentLineStartMs = -1;
		int64_t m_HookCurrentLineEndMs = -1;
		char m_aHookAlbumArtPath[QmNeteaseHook::MAX_COVER_PATH_BYTES] = {};
		char m_aHookAlbumArtUrl[QmNeteaseHook::MAX_COVER_URL_BYTES] = {};
		IGraphics::CTextureHandle m_AlbumArt;
		IGraphics::CTextureHandle m_AlbumArtCircular;
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
	void ClearHookAlbumArt();
	void SyncNeteaseHookConfiguration();

	std::unique_ptr<CQmNeteaseHookProvider> m_pNeteaseHook;
	bool m_HookHasMedia = false;
	bool m_NeteaseHookConfigInitialized = false;
	bool m_LastNeteaseHookEnabled = false;
	std::string m_LastNeteaseHookHelperPath;
	SState m_HookState{};
	std::string m_HookAlbumArtPath;
	IGraphics::CTextureHandle m_HookAlbumArt;
	IGraphics::CTextureHandle m_HookAlbumArtCircular;
	int m_HookAlbumArtWidth = 0;
	int m_HookAlbumArtHeight = 0;
#if SYSTEM_MEDIA_CONTROLS_WINRT_ENABLED
	std::unique_ptr<SWinrt> m_pWinrt;
	std::unique_ptr<SShared> m_pShared;
	std::thread m_Thread;
	std::atomic_bool m_StopThread{false};

	void ThreadMain();
#endif
};

#endif
