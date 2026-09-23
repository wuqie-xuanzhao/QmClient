#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_PREPARED_MEDIA_ART_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_PREPARED_MEDIA_ART_H

#include <base/mem.h>

#include <engine/image.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

// 后台准备可移交的 RGBA 缓冲；未上传、被替换或关闭时只回收 CPU 内存。
class CQmPreparedMediaArt
{
	static void PrepareImage(CImageInfo &Image, const std::vector<uint8_t> &Pixels, int Width, int Height)
	{
		if(Width <= 0 || Height <= 0)
			return;
		const size_t ExpectedSize = (size_t)Width * (size_t)Height * 4;
		if(Pixels.size() < ExpectedSize)
			return;
		Image.m_pData = static_cast<uint8_t *>(malloc(ExpectedSize));
		if(Image.m_pData == nullptr)
			return;
		Image.m_Width = (size_t)Width;
		Image.m_Height = (size_t)Height;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		mem_copy(Image.m_pData, Pixels.data(), ExpectedSize);
	}

public:
	CImageInfo m_Original;
	CImageInfo m_Circular;

	CQmPreparedMediaArt(const std::vector<uint8_t> &Original, const std::vector<uint8_t> &Circular, int Width, int Height)
	{
		PrepareImage(m_Original, Original, Width, Height);
		PrepareImage(m_Circular, Circular, Width, Height);
	}

	CQmPreparedMediaArt(const CQmPreparedMediaArt &) = delete;
	CQmPreparedMediaArt &operator=(const CQmPreparedMediaArt &) = delete;

	~CQmPreparedMediaArt()
	{
		m_Original.Free();
		m_Circular.Free();
	}
};

#endif
