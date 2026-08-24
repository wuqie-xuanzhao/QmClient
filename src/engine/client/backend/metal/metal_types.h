#ifndef ENGINE_CLIENT_BACKEND_METAL_METAL_TYPES_H
#define ENGINE_CLIENT_BACKEND_METAL_METAL_TYPES_H

#if defined(__METAL_VERSION__)
using SMetalFloat4 = float4;
using SMetalFloat4x4 = float4x4;
#else
#include <cstddef>

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
