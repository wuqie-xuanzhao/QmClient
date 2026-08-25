#ifndef ENGINE_CLIENT_BACKEND_METAL_METAL_TYPES_H
#define ENGINE_CLIENT_BACKEND_METAL_METAL_TYPES_H

#if defined(__METAL_VERSION__)
using SMetalFloat4 = float4;
using SMetalFloat4x4 = float4x4;
#else
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

enum class EMetalTextureFormat
{
	RGBA8,
	R8,
};

struct SMetalTextureLayout
{
	size_t m_BytesPerPixel = 0;
	size_t m_RowBytes = 0;
	size_t m_DataBytes = 0;
	size_t m_MipLevels = 0;
};

inline size_t MetalTextureBytesPerPixel(EMetalTextureFormat Format)
{
	return Format == EMetalTextureFormat::R8 ? 1 : 4;
}

inline bool MetalCheckedMul(size_t A, size_t B, size_t &Result)
{
	if(A != 0 && B > std::numeric_limits<size_t>::max() / A)
	{
		Result = 0;
		return false;
	}
	Result = A * B;
	return true;
}

inline size_t MetalMipLevelCount(size_t Width, size_t Height, bool NoMipmaps)
{
	if(Width == 0 || Height == 0)
		return 0;
	if(NoMipmaps)
		return 1;

	size_t Levels = 1;
	for(size_t Size = Width > Height ? Width : Height; Size > 1; Size >>= 1)
		++Levels;
	return Levels;
}

inline bool MetalTextureLayout(size_t Width, size_t Height, EMetalTextureFormat Format, bool NoMipmaps, SMetalTextureLayout &Layout)
{
	Layout = {};
	if(Width == 0 || Height == 0)
		return false;

	Layout.m_BytesPerPixel = MetalTextureBytesPerPixel(Format);
	if(!MetalCheckedMul(Width, Layout.m_BytesPerPixel, Layout.m_RowBytes))
		return false;

	Layout.m_MipLevels = MetalMipLevelCount(Width, Height, NoMipmaps);
	if(Layout.m_MipLevels == 0)
		return false;

	for(size_t Mip = 0, MipWidth = Width, MipHeight = Height; Mip < Layout.m_MipLevels; ++Mip)
	{
		size_t RowBytes = 0;
		size_t LevelBytes = 0;
		if(!MetalCheckedMul(MipWidth, Layout.m_BytesPerPixel, RowBytes) || !MetalCheckedMul(RowBytes, MipHeight, LevelBytes) ||
			Layout.m_DataBytes > std::numeric_limits<size_t>::max() - LevelBytes)
		{
			Layout = {};
			return false;
		}
		Layout.m_DataBytes += LevelBytes;
		MipWidth = MipWidth > 1 ? MipWidth / 2 : 1;
		MipHeight = MipHeight > 1 ? MipHeight / 2 : 1;
	}
	return true;
}

inline bool MetalValidateSubregion(size_t TextureWidth, size_t TextureHeight, size_t X, size_t Y, size_t Width, size_t Height)
{
	return Width > 0 && Height > 0 && X <= TextureWidth && Y <= TextureHeight && Width <= TextureWidth - X && Height <= TextureHeight - Y;
}

struct SMetalTextureHandle
{
	size_t m_Slot = std::numeric_limits<size_t>::max();
	uint32_t m_Generation = 0;

	bool IsValid() const { return m_Generation != 0 && m_Slot != std::numeric_limits<size_t>::max(); }
};

class CMetalTextureRegistry
{
	std::vector<uint32_t> m_vGenerations;
	std::vector<bool> m_vAllocated;

public:
	SMetalTextureHandle Allocate(size_t Slot)
	{
		if(Slot >= m_vGenerations.size())
		{
			m_vGenerations.resize(Slot + 1, 0);
			m_vAllocated.resize(Slot + 1, false);
		}
		if(m_vAllocated[Slot])
			return {};
		uint32_t &Generation = m_vGenerations[Slot];
		Generation = Generation == std::numeric_limits<uint32_t>::max() ? 1 : Generation + 1;
		m_vAllocated[Slot] = true;
		return {Slot, Generation};
	}

	bool Release(SMetalTextureHandle Handle)
	{
		if(!IsValid(Handle))
			return false;
		m_vAllocated[Handle.m_Slot] = false;
		return true;
	}

	bool IsValid(SMetalTextureHandle Handle) const
	{
		return Handle.IsValid() && Handle.m_Slot < m_vGenerations.size() && m_vAllocated[Handle.m_Slot] && m_vGenerations[Handle.m_Slot] == Handle.m_Generation;
	}
};

struct SMetalFloat4
{
	float m_v[4];
};

struct alignas(16) SMetalFloat4x4
{
	float m_v[16];
};
#endif

#if defined(__METAL_VERSION__)
struct SMetalUniforms
#else
struct alignas(16) SMetalUniforms
#endif
{
	SMetalFloat4x4 m_MVP;
	SMetalFloat4 m_Color;
};

#if !defined(__METAL_VERSION__)
static_assert(alignof(SMetalUniforms) == 16);
static_assert(sizeof(SMetalFloat4x4) == 64);
static_assert(sizeof(SMetalFloat4) == 16);
static_assert(sizeof(SMetalUniforms) == 80);
static_assert(offsetof(SMetalUniforms, m_MVP) == 0);
static_assert(offsetof(SMetalUniforms, m_Color) == 64);
#endif

#endif
