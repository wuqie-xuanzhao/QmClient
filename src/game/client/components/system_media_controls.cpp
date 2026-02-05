#include "system_media_controls.h"

#if defined(CONF_FAMILY_WINDOWS)
#include <base/system.h>

#include <engine/image.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>
#endif

#if defined(CONF_FAMILY_WINDOWS)
using namespace winrt::Windows::Media::Control;

struct CSystemMediaControls::SWinrt
{
	GlobalSystemMediaTransportControlsSessionManager m_Manager{nullptr};
	GlobalSystemMediaTransportControlsSession m_Session{nullptr};
	CSystemMediaControls::SState m_State{};
	bool m_HasMedia = false;
	int64_t m_LastStateUpdate = 0;
	int64_t m_LastPropsUpdate = 0;
	bool m_WinrtInitialized = false;
	std::string m_AlbumArtKey;
};

struct CSystemMediaControls::SShared
{
	std::mutex m_Mutex;
	bool m_HasRequest = false;
	std::string m_RequestKey;
	winrt::agile_ref<winrt::Windows::Storage::Streams::IRandomAccessStreamReference> m_Thumbnail;
	std::vector<uint8_t> m_AlbumArtRgba;
	int m_AlbumArtWidth = 0;
	int m_AlbumArtHeight = 0;
	bool m_AlbumArtDirty = false;
};

static void ClearAlbumArtLocal(CSystemMediaControls::SWinrt *pWinrt, IGraphics *pGraphics)
{
	if(pGraphics && pWinrt->m_State.m_AlbumArt.IsValid())
	{
		pGraphics->UnloadTexture(&pWinrt->m_State.m_AlbumArt);
	}
	pWinrt->m_State.m_AlbumArt.Invalidate();
	pWinrt->m_State.m_AlbumArtWidth = 0;
	pWinrt->m_State.m_AlbumArtHeight = 0;
}

static void ClearState(CSystemMediaControls::SWinrt *pWinrt, IGraphics *pGraphics)
{
	ClearAlbumArtLocal(pWinrt, pGraphics);
	pWinrt->m_State = CSystemMediaControls::SState{};
	pWinrt->m_HasMedia = false;
	pWinrt->m_AlbumArtKey.clear();
}

static void ClearMetadata(CSystemMediaControls::SWinrt *pWinrt)
{
	pWinrt->m_State.m_aTitle[0] = '\0';
	pWinrt->m_State.m_aArtist[0] = '\0';
}

static void ClearSharedAlbumArt(CSystemMediaControls::SShared *pShared)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_AlbumArtRgba.clear();
	pShared->m_AlbumArtWidth = 0;
	pShared->m_AlbumArtHeight = 0;
	pShared->m_AlbumArtDirty = true;
}

static void SetSharedAlbumArt(CSystemMediaControls::SShared *pShared, std::vector<uint8_t> &&Pixels, int Width, int Height)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_AlbumArtRgba = std::move(Pixels);
	pShared->m_AlbumArtWidth = Width;
	pShared->m_AlbumArtHeight = Height;
	pShared->m_AlbumArtDirty = true;
}

static void QueueAlbumArtRequest(CSystemMediaControls::SShared *pShared, const winrt::Windows::Storage::Streams::IRandomAccessStreamReference &Thumbnail, const std::string &Key)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_Thumbnail = winrt::agile_ref<winrt::Windows::Storage::Streams::IRandomAccessStreamReference>(Thumbnail);
	pShared->m_RequestKey = Key;
	pShared->m_HasRequest = true;
}

static bool PopAlbumArtRequest(CSystemMediaControls::SShared *pShared, winrt::agile_ref<winrt::Windows::Storage::Streams::IRandomAccessStreamReference> &OutThumbnail)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	if(!pShared->m_HasRequest)
		return false;
	OutThumbnail = pShared->m_Thumbnail;
	pShared->m_HasRequest = false;
	return true;
}

static void UpdateAlbumArtData(CSystemMediaControls::SShared *pShared, const winrt::Windows::Storage::Streams::IRandomAccessStreamReference &Thumbnail)
{
	try
	{
		const auto Stream = Thumbnail.OpenReadAsync().get();
		const auto Decoder = winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(Stream).get();
		const uint32_t Width = Decoder.PixelWidth();
		const uint32_t Height = Decoder.PixelHeight();
		if(Width == 0 || Height == 0)
		{
			ClearSharedAlbumArt(pShared);
			return;
		}

		const auto PixelData = Decoder.GetPixelDataAsync(
							       winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8,
							       winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied,
							       winrt::Windows::Graphics::Imaging::BitmapTransform(),
							       winrt::Windows::Graphics::Imaging::ExifOrientationMode::IgnoreExifOrientation,
							       winrt::Windows::Graphics::Imaging::ColorManagementMode::DoNotColorManage)
							       .get();

		const auto Pixels = PixelData.DetachPixelData();
		const size_t ExpectedSize = (size_t)Width * (size_t)Height * 4;
		if(Pixels.size() < ExpectedSize)
		{
			ClearSharedAlbumArt(pShared);
			return;
		}

		std::vector<uint8_t> Copy(Pixels.begin(), Pixels.begin() + ExpectedSize);
		SetSharedAlbumArt(pShared, std::move(Copy), (int)Width, (int)Height);
	}
	catch(const winrt::hresult_error &)
	{
		ClearSharedAlbumArt(pShared);
	}
}

static void ApplyRoundedMask(std::vector<uint8_t> &Pixels, int Width, int Height, float Radius)
{
	if(Pixels.empty() || Width <= 0 || Height <= 0 || Radius <= 0.0f)
		return;

	const float MaxRadius = 0.5f * (float)std::min(Width, Height);
	const float R = std::min(Radius, MaxRadius);
	if(R <= 0.0f)
		return;

	const float Left = R;
	const float Right = (float)Width - R;
	const float Top = R;
	const float Bottom = (float)Height - R;
	const float OuterR2 = R * R;
	const float InnerR = R - 1.0f;
	const float InnerR2 = InnerR > 0.0f ? InnerR * InnerR : 0.0f;
	const bool UseSoftEdge = InnerR > 0.0f;

	for(int y = 0; y < Height; ++y)
	{
		const float Fy = (float)y + 0.5f;
		for(int x = 0; x < Width; ++x)
		{
			const float Fx = (float)x + 0.5f;
			float Dx = 0.0f;
			float Dy = 0.0f;
			bool Corner = false;

			if(Fx < Left && Fy < Top)
			{
				Dx = Left - Fx;
				Dy = Top - Fy;
				Corner = true;
			}
			else if(Fx > Right && Fy < Top)
			{
				Dx = Fx - Right;
				Dy = Top - Fy;
				Corner = true;
			}
			else if(Fx < Left && Fy > Bottom)
			{
				Dx = Left - Fx;
				Dy = Fy - Bottom;
				Corner = true;
			}
			else if(Fx > Right && Fy > Bottom)
			{
				Dx = Fx - Right;
				Dy = Fy - Bottom;
				Corner = true;
			}

			if(!Corner)
				continue;

			const float Dist2 = Dx * Dx + Dy * Dy;
			if(Dist2 <= (UseSoftEdge ? InnerR2 : OuterR2))
				continue;

			float Alpha = 0.0f;
			if(UseSoftEdge && Dist2 < OuterR2)
			{
				const float Dist = std::sqrt(Dist2);
				Alpha = std::clamp(R - Dist, 0.0f, 1.0f);
			}

			const size_t Index = (size_t)(y * Width + x) * 4;
			if(Alpha <= 0.0f)
			{
				Pixels[Index + 0] = 0;
				Pixels[Index + 1] = 0;
				Pixels[Index + 2] = 0;
				Pixels[Index + 3] = 0;
			}
			else if(Alpha < 1.0f)
			{
				Pixels[Index + 0] = (uint8_t)std::round(Pixels[Index + 0] * Alpha);
				Pixels[Index + 1] = (uint8_t)std::round(Pixels[Index + 1] * Alpha);
				Pixels[Index + 2] = (uint8_t)std::round(Pixels[Index + 2] * Alpha);
				Pixels[Index + 3] = (uint8_t)std::round(Pixels[Index + 3] * Alpha);
			}
		}
	}
}

static void ApplySharedAlbumArt(CSystemMediaControls::SShared *pShared, CSystemMediaControls::SWinrt *pWinrt, IGraphics *pGraphics)
{
	if(!pShared || !pWinrt || !pGraphics)
		return;

	bool AlbumArtDirty = false;
	int AlbumArtWidth = 0;
	int AlbumArtHeight = 0;
	std::vector<uint8_t> AlbumArtPixels;
	{
		std::scoped_lock Lock(pShared->m_Mutex);
		if(pShared->m_AlbumArtDirty)
		{
			AlbumArtDirty = true;
			AlbumArtWidth = pShared->m_AlbumArtWidth;
			AlbumArtHeight = pShared->m_AlbumArtHeight;
			AlbumArtPixels = std::move(pShared->m_AlbumArtRgba);
			pShared->m_AlbumArtRgba.clear();
			pShared->m_AlbumArtDirty = false;
		}
	}

	if(!AlbumArtDirty)
		return;

	ClearAlbumArtLocal(pWinrt, pGraphics);

	const size_t ExpectedSize = (size_t)AlbumArtWidth * (size_t)AlbumArtHeight * 4;
	if(AlbumArtWidth > 0 && AlbumArtHeight > 0 && AlbumArtPixels.size() >= ExpectedSize)
	{
		const float RoundingRatio = 2.0f / 14.0f;
		const float Radius = (float)std::min(AlbumArtWidth, AlbumArtHeight) * RoundingRatio;
		ApplyRoundedMask(AlbumArtPixels, AlbumArtWidth, AlbumArtHeight, Radius);

		CImageInfo Image;
		Image.m_Width = (size_t)AlbumArtWidth;
		Image.m_Height = (size_t)AlbumArtHeight;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.m_pData = static_cast<uint8_t *>(malloc(ExpectedSize));
		if(Image.m_pData)
		{
			mem_copy(Image.m_pData, AlbumArtPixels.data(), ExpectedSize);
			pWinrt->m_State.m_AlbumArt = pGraphics->LoadTextureRawMove(Image, 0, "smtc_album_art");
			if(pWinrt->m_State.m_AlbumArt.IsValid())
			{
				pWinrt->m_State.m_AlbumArtWidth = AlbumArtWidth;
				pWinrt->m_State.m_AlbumArtHeight = AlbumArtHeight;
			}
		}
	}
}
#endif

CSystemMediaControls::CSystemMediaControls() = default;
CSystemMediaControls::~CSystemMediaControls() = default;

#if defined(CONF_FAMILY_WINDOWS)
void CSystemMediaControls::ThreadMain()
{
	try
	{
		winrt::init_apartment(winrt::apartment_type::single_threaded);
	}
	catch(const winrt::hresult_error &)
	{
		return;
	}

	while(!m_StopThread)
	{
		winrt::agile_ref<winrt::Windows::Storage::Streams::IRandomAccessStreamReference> Thumbnail;
		bool HasRequest = false;
		{
			std::scoped_lock Lock(m_pShared->m_Mutex);
			if(m_pShared->m_HasRequest)
			{
				Thumbnail = m_pShared->m_Thumbnail;
				m_pShared->m_HasRequest = false;
				HasRequest = true;
			}
		}

		if(HasRequest)
		{
			auto Ref = Thumbnail.get();
			if(Ref)
			{
				UpdateAlbumArtData(m_pShared.get(), Ref);
			}
			else
			{
				ClearSharedAlbumArt(m_pShared.get());
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	winrt::uninit_apartment();
}
#endif

void CSystemMediaControls::OnInit()
{
#if defined(CONF_FAMILY_WINDOWS)
	m_pWinrt = std::make_unique<SWinrt>();
	try
	{
		winrt::init_apartment(winrt::apartment_type::single_threaded);
		m_pWinrt->m_WinrtInitialized = true;
	}
	catch(const winrt::hresult_error &)
	{
		m_pWinrt.reset();
		return;
	}

	try
	{
		m_pWinrt->m_Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
	}
	catch(const winrt::hresult_error &)
	{
	}

	m_pShared = std::make_unique<SShared>();
	m_StopThread = false;
	m_Thread = std::thread(&CSystemMediaControls::ThreadMain, this);
#endif
}

void CSystemMediaControls::OnShutdown()
{
#if defined(CONF_FAMILY_WINDOWS)
	m_StopThread = true;
	if(m_Thread.joinable())
	{
		m_Thread.join();
	}
	m_pShared.reset();
	if(m_pWinrt)
	{
		ClearState(m_pWinrt.get(), Graphics());
		m_pWinrt->m_Session = nullptr;
		m_pWinrt->m_Manager = nullptr;
		if(m_pWinrt->m_WinrtInitialized)
		{
			winrt::uninit_apartment();
		}
		m_pWinrt.reset();
	}
#endif
}

void CSystemMediaControls::OnUpdate()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!m_pWinrt)
		return;

	const int64_t Now = time_get();
	const int64_t StateInterval = time_freq() / 5;
	if(Now - m_pWinrt->m_LastStateUpdate >= StateInterval)
	{
		m_pWinrt->m_LastStateUpdate = Now;

		if(!m_pWinrt->m_Manager)
		{
			try
			{
				m_pWinrt->m_Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
			}
			catch(const winrt::hresult_error &)
			{
				ClearState(m_pWinrt.get(), Graphics());
				if(m_pShared)
					ClearSharedAlbumArt(m_pShared.get());
				ApplySharedAlbumArt(m_pShared.get(), m_pWinrt.get(), Graphics());
				return;
			}
		}

		if(!m_pWinrt->m_Manager)
		{
			ClearState(m_pWinrt.get(), Graphics());
			if(m_pShared)
				ClearSharedAlbumArt(m_pShared.get());
			ApplySharedAlbumArt(m_pShared.get(), m_pWinrt.get(), Graphics());
			return;
		}

		try
		{
			const auto Session = m_pWinrt->m_Manager.GetCurrentSession();
			m_pWinrt->m_Session = Session;
			if(!Session)
			{
				ClearState(m_pWinrt.get(), Graphics());
				if(m_pShared)
					ClearSharedAlbumArt(m_pShared.get());
				ApplySharedAlbumArt(m_pShared.get(), m_pWinrt.get(), Graphics());
				return;
			}

			const auto PlaybackInfo = Session.GetPlaybackInfo();
			const auto Controls = PlaybackInfo.Controls();
			m_pWinrt->m_State.m_CanPlay = Controls.IsPlayEnabled();
			m_pWinrt->m_State.m_CanPause = Controls.IsPauseEnabled();
			m_pWinrt->m_State.m_CanPrev = Controls.IsPreviousEnabled();
			m_pWinrt->m_State.m_CanNext = Controls.IsNextEnabled();
			m_pWinrt->m_State.m_Playing = PlaybackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
			m_pWinrt->m_HasMedia = true;

			const auto Timeline = Session.GetTimelineProperties();
			const int64_t Start100ns = Timeline.StartTime().count();
			const int64_t End100ns = Timeline.EndTime().count();
			const int64_t Position100ns = Timeline.Position().count();
			const int64_t Duration100ns = End100ns - Start100ns;
			const int64_t PositionRel100ns = Position100ns - Start100ns;
			m_pWinrt->m_State.m_DurationMs = Duration100ns > 0 ? Duration100ns / 10000 : 0;
			m_pWinrt->m_State.m_PositionMs = PositionRel100ns > 0 ? PositionRel100ns / 10000 : 0;

			const int64_t PropsInterval = time_freq();
			if(Now - m_pWinrt->m_LastPropsUpdate >= PropsInterval)
			{
				m_pWinrt->m_LastPropsUpdate = Now;
				try
				{
					const auto MediaProps = Session.TryGetMediaPropertiesAsync().get();
					const std::string Title = winrt::to_string(MediaProps.Title());
					const std::string Artist = winrt::to_string(MediaProps.Artist());

					if(!Title.empty())
					{
						str_copy(m_pWinrt->m_State.m_aTitle, Title.c_str(), sizeof(m_pWinrt->m_State.m_aTitle));
					}
					else
					{
						m_pWinrt->m_State.m_aTitle[0] = '\0';
					}

					if(!Artist.empty())
					{
						str_copy(m_pWinrt->m_State.m_aArtist, Artist.c_str(), sizeof(m_pWinrt->m_State.m_aArtist));
					}
					else
					{
						m_pWinrt->m_State.m_aArtist[0] = '\0';
					}

					const bool HasText = !Title.empty() || !Artist.empty();
					if(HasText)
					{
						const std::string NewKey = Title + "\n" + Artist;
						if(NewKey != m_pWinrt->m_AlbumArtKey)
						{
							m_pWinrt->m_AlbumArtKey = NewKey;
							if(m_pShared)
							{
								const auto Thumbnail = MediaProps.Thumbnail();
								if(Thumbnail)
									QueueAlbumArtRequest(m_pShared.get(), Thumbnail, NewKey);
								else
									ClearSharedAlbumArt(m_pShared.get());
							}
						}
					}
					else
					{
						m_pWinrt->m_AlbumArtKey.clear();
						if(m_pShared)
							ClearSharedAlbumArt(m_pShared.get());
					}
				}
				catch(const winrt::hresult_error &)
				{
					ClearMetadata(m_pWinrt.get());
				}
			}
		}
		catch(const winrt::hresult_error &)
		{
			ClearState(m_pWinrt.get(), Graphics());
			if(m_pShared)
				ClearSharedAlbumArt(m_pShared.get());
		}
	}

	ApplySharedAlbumArt(m_pShared.get(), m_pWinrt.get(), Graphics());
#endif
}

bool CSystemMediaControls::GetStateSnapshot(SState &State) const
{
#if defined(CONF_FAMILY_WINDOWS)
	if(m_pWinrt && m_pWinrt->m_HasMedia)
	{
		State = m_pWinrt->m_State;
		return true;
	}
#endif
	State = SState{};
	return false;
}

void CSystemMediaControls::Previous()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!m_pWinrt)
		return;

	if(!m_pWinrt->m_Manager)
	{
		try
		{
			m_pWinrt->m_Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
		}
		catch(const winrt::hresult_error &)
		{
			return;
		}
	}

	if(!m_pWinrt->m_Manager)
		return;

	try
	{
		const auto Session = m_pWinrt->m_Manager.GetCurrentSession();
		if(Session)
		{
			Session.TrySkipPreviousAsync().get();
		}
	}
	catch(const winrt::hresult_error &)
	{
	}
#endif
}

void CSystemMediaControls::PlayPause()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!m_pWinrt)
		return;

	if(!m_pWinrt->m_Manager)
	{
		try
		{
			m_pWinrt->m_Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
		}
		catch(const winrt::hresult_error &)
		{
			return;
		}
	}

	if(!m_pWinrt->m_Manager)
		return;

	try
	{
		const auto Session = m_pWinrt->m_Manager.GetCurrentSession();
		if(Session)
		{
			Session.TryTogglePlayPauseAsync().get();
		}
	}
	catch(const winrt::hresult_error &)
	{
	}
#endif
}

void CSystemMediaControls::Next()
{
#if defined(CONF_FAMILY_WINDOWS)
	if(!m_pWinrt)
		return;

	if(!m_pWinrt->m_Manager)
	{
		try
		{
			m_pWinrt->m_Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
		}
		catch(const winrt::hresult_error &)
		{
			return;
		}
	}

	if(!m_pWinrt->m_Manager)
		return;

	try
	{
		const auto Session = m_pWinrt->m_Manager.GetCurrentSession();
		if(Session)
		{
			Session.TrySkipNextAsync().get();
		}
	}
	catch(const winrt::hresult_error &)
	{
	}
#endif
}
