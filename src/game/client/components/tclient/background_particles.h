#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_BACKGROUND_PARTICLES_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_BACKGROUND_PARTICLES_H

#include <base/color.h>
#include <base/math.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

struct SBackgroundParticleMesh
{
	std::vector<vec3> m_vVertices;
	std::vector<std::array<int, 2>> m_vEdges;
	std::vector<std::array<int, 3>> m_vFaces;
};

struct SBackgroundParticleProjection
{
	vec2 m_CameraCenter = vec2(0.0f, 0.0f);
	float m_WorldLeft = 0.0f;
	float m_WorldTop = 0.0f;
	float m_WorldRight = 1.0f;
	float m_WorldBottom = 1.0f;
	float m_ViewportWidth = 1.0f;
	float m_ViewportHeight = 1.0f;
};

inline float BackgroundParticleDepthScale(float Depth, float DepthRange)
{
	const float SafeDepthRange = std::max(DepthRange, 0.001f);
	const float DepthFactor = std::clamp(Depth / SafeDepthRange, 0.0f, 1.0f);
	return 1.0f - DepthFactor * 0.72f;
}

inline vec3 BackgroundParticleRotateVertex(const vec3 &Vertex, const vec3 &Rotation)
{
	vec3 Result = Vertex;

	const float CosZ = std::cos(Rotation.z);
	const float SinZ = std::sin(Rotation.z);
	Result = vec3(Result.x * CosZ - Result.y * SinZ, Result.x * SinZ + Result.y * CosZ, Result.z);

	const float CosX = std::cos(Rotation.x);
	const float SinX = std::sin(Rotation.x);
	Result = vec3(Result.x, Result.y * CosX - Result.z * SinX, Result.y * SinX + Result.z * CosX);

	const float CosY = std::cos(Rotation.y);
	const float SinY = std::sin(Rotation.y);
	Result = vec3(Result.x * CosY + Result.z * SinY, Result.y, -Result.x * SinY + Result.z * CosY);

	return Result;
}

inline vec2 BackgroundParticleWorldToScreen(const SBackgroundParticleProjection &Projection, const vec2 &WorldPosition)
{
	const float WorldWidth = std::max(Projection.m_WorldRight - Projection.m_WorldLeft, 0.001f);
	const float WorldHeight = std::max(Projection.m_WorldBottom - Projection.m_WorldTop, 0.001f);
	return vec2(
		(WorldPosition.x - Projection.m_WorldLeft) / WorldWidth * Projection.m_ViewportWidth,
		(WorldPosition.y - Projection.m_WorldTop) / WorldHeight * Projection.m_ViewportHeight);
}

inline vec2 BackgroundParticleProjectVertex(const SBackgroundParticleProjection &Projection, const vec2 &WorldAnchor, const vec3 &RotatedLocalVertex, float Depth, float DepthRange)
{
	const float DepthScale = BackgroundParticleDepthScale(Depth, DepthRange);
	const vec2 DepthAnchor = Projection.m_CameraCenter + (WorldAnchor - Projection.m_CameraCenter) * DepthScale;
	return BackgroundParticleWorldToScreen(Projection, DepthAnchor) + vec2(RotatedLocalVertex.x, RotatedLocalVertex.y) * DepthScale;
}

inline SBackgroundParticleMesh CreateBackgroundParticleBoxMesh(const vec3 &HalfExtent)
{
	SBackgroundParticleMesh Mesh;
	Mesh.m_vVertices = {
		vec3(-HalfExtent.x, -HalfExtent.y, -HalfExtent.z),
		vec3(HalfExtent.x, -HalfExtent.y, -HalfExtent.z),
		vec3(HalfExtent.x, HalfExtent.y, -HalfExtent.z),
		vec3(-HalfExtent.x, HalfExtent.y, -HalfExtent.z),
		vec3(-HalfExtent.x, -HalfExtent.y, HalfExtent.z),
		vec3(HalfExtent.x, -HalfExtent.y, HalfExtent.z),
		vec3(HalfExtent.x, HalfExtent.y, HalfExtent.z),
		vec3(-HalfExtent.x, HalfExtent.y, HalfExtent.z),
	};
	Mesh.m_vEdges = {
		{0, 1},
		{1, 2},
		{2, 3},
		{3, 0},
		{4, 5},
		{5, 6},
		{6, 7},
		{7, 4},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	};
	Mesh.m_vFaces = {
		{0, 2, 1},
		{0, 3, 2},
		{4, 5, 6},
		{4, 6, 7},
		{0, 1, 5},
		{0, 5, 4},
		{1, 2, 6},
		{1, 6, 5},
		{2, 3, 7},
		{2, 7, 6},
		{3, 0, 4},
		{3, 4, 7},
	};
	return Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticleCubeMesh()
{
	static const SBackgroundParticleMesh s_Mesh = CreateBackgroundParticleBoxMesh(vec3(1.0f, 1.0f, 1.0f));
	return s_Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticleBoxMesh()
{
	static const SBackgroundParticleMesh s_Mesh = CreateBackgroundParticleBoxMesh(vec3(1.2f, 0.75f, 0.55f));
	return s_Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticlePyramidMesh()
{
	static const SBackgroundParticleMesh s_Mesh = [] {
		SBackgroundParticleMesh Mesh;
		Mesh.m_vVertices = {
			vec3(-1.0f, -1.0f, -0.65f),
			vec3(1.0f, -1.0f, -0.65f),
			vec3(1.0f, 1.0f, -0.65f),
			vec3(-1.0f, 1.0f, -0.65f),
			vec3(0.0f, 0.0f, 1.15f),
		};
		Mesh.m_vEdges = {
			{0, 1},
			{1, 2},
			{2, 3},
			{3, 0},
			{0, 4},
			{1, 4},
			{2, 4},
			{3, 4},
		};
		Mesh.m_vFaces = {
			{0, 2, 1},
			{0, 3, 2},
			{0, 1, 4},
			{1, 2, 4},
			{2, 3, 4},
			{3, 0, 4},
		};
		return Mesh;
	}();
	return s_Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticleOctahedronMesh()
{
	static const SBackgroundParticleMesh s_Mesh = [] {
		SBackgroundParticleMesh Mesh;
		Mesh.m_vVertices = {
			vec3(0.0f, -1.25f, 0.0f),
			vec3(1.0f, 0.0f, 0.0f),
			vec3(0.0f, 0.0f, 1.0f),
			vec3(-1.0f, 0.0f, 0.0f),
			vec3(0.0f, 0.0f, -1.0f),
			vec3(0.0f, 1.25f, 0.0f),
		};
		Mesh.m_vEdges = {
			{0, 1},
			{0, 2},
			{0, 3},
			{0, 4},
			{5, 1},
			{5, 2},
			{5, 3},
			{5, 4},
			{1, 2},
			{2, 3},
			{3, 4},
			{4, 1},
		};
		Mesh.m_vFaces = {
			{0, 2, 1},
			{0, 3, 2},
			{0, 4, 3},
			{0, 1, 4},
			{5, 1, 2},
			{5, 2, 3},
			{5, 3, 4},
			{5, 4, 1},
		};
		return Mesh;
	}();
	return s_Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticleSphereMesh()
{
	static const SBackgroundParticleMesh s_Mesh = [] {
		constexpr int LongitudeSegments = 12;
		constexpr int LatitudeSegments = 6;
		SBackgroundParticleMesh Mesh;
		Mesh.m_vVertices.reserve(2 + (LatitudeSegments - 1) * LongitudeSegments);
		Mesh.m_vEdges.reserve((LatitudeSegments - 1) * LongitudeSegments + LatitudeSegments * LongitudeSegments);
		Mesh.m_vFaces.reserve(2 * (LatitudeSegments - 1) * LongitudeSegments);
		Mesh.m_vVertices.emplace_back(0.0f, 0.0f, 1.0f);
		for(int Latitude = 1; Latitude < LatitudeSegments; ++Latitude)
		{
			const float Phi = pi * (float)Latitude / (float)LatitudeSegments;
			const float RingRadius = std::sin(Phi);
			const float Z = std::cos(Phi);
			for(int Longitude = 0; Longitude < LongitudeSegments; ++Longitude)
			{
				const float Theta = 2.0f * pi * (float)Longitude / (float)LongitudeSegments;
				Mesh.m_vVertices.emplace_back(RingRadius * std::cos(Theta), RingRadius * std::sin(Theta), Z);
			}
		}
		const int BottomIndex = static_cast<int>(Mesh.m_vVertices.size());
		Mesh.m_vVertices.emplace_back(0.0f, 0.0f, -1.0f);
		auto RingVertex = [](int Latitude, int Longitude) {
			return 1 + (Latitude - 1) * LongitudeSegments + Longitude % LongitudeSegments;
		};

		for(int Latitude = 1; Latitude < LatitudeSegments; ++Latitude)
		{
			for(int Longitude = 0; Longitude < LongitudeSegments; ++Longitude)
			{
				const int NextLongitude = (Longitude + 1) % LongitudeSegments;
				Mesh.m_vEdges.push_back({RingVertex(Latitude, Longitude), RingVertex(Latitude, NextLongitude)});
			}
		}
		for(int Longitude = 0; Longitude < LongitudeSegments; ++Longitude)
		{
			const int NextLongitude = (Longitude + 1) % LongitudeSegments;
			Mesh.m_vEdges.push_back({0, RingVertex(1, Longitude)});
			Mesh.m_vFaces.push_back({0, RingVertex(1, NextLongitude), RingVertex(1, Longitude)});
			for(int Latitude = 1; Latitude < LatitudeSegments - 1; ++Latitude)
			{
				const int TopLeft = RingVertex(Latitude, Longitude);
				const int TopRight = RingVertex(Latitude, NextLongitude);
				const int BottomLeft = RingVertex(Latitude + 1, Longitude);
				const int BottomRight = RingVertex(Latitude + 1, NextLongitude);
				Mesh.m_vEdges.push_back({TopLeft, BottomLeft});
				Mesh.m_vFaces.push_back({TopLeft, TopRight, BottomRight});
				Mesh.m_vFaces.push_back({TopLeft, BottomRight, BottomLeft});
			}
			Mesh.m_vEdges.push_back({RingVertex(LatitudeSegments - 1, Longitude), BottomIndex});
			Mesh.m_vFaces.push_back({RingVertex(LatitudeSegments - 1, Longitude), RingVertex(LatitudeSegments - 1, NextLongitude), BottomIndex});
		}
		return Mesh;
	}();
	return s_Mesh;
}

inline const SBackgroundParticleMesh &BackgroundParticleTorusMesh()
{
	static const SBackgroundParticleMesh s_Mesh = [] {
		constexpr int MajorSegments = 12;
		constexpr int MinorSegments = 6;
		constexpr float MinorRadius = 0.36f;
		SBackgroundParticleMesh Mesh;
		Mesh.m_vVertices.reserve(MajorSegments * MinorSegments);
		Mesh.m_vEdges.reserve(MajorSegments * MinorSegments * 2);
		Mesh.m_vFaces.reserve(MajorSegments * MinorSegments * 2);
		for(int Major = 0; Major < MajorSegments; ++Major)
		{
			const float MajorAngle = 2.0f * pi * (float)Major / (float)MajorSegments;
			const float MajorCos = std::cos(MajorAngle);
			const float MajorSin = std::sin(MajorAngle);
			for(int Minor = 0; Minor < MinorSegments; ++Minor)
			{
				const float MinorAngle = 2.0f * pi * (float)Minor / (float)MinorSegments;
				const float RingRadius = 1.0f + MinorRadius * std::cos(MinorAngle);
				Mesh.m_vVertices.emplace_back(RingRadius * MajorCos, RingRadius * MajorSin, MinorRadius * std::sin(MinorAngle));
			}
		}
		auto VertexIndex = [](int Major, int Minor) {
			return (Major % MajorSegments) * MinorSegments + Minor % MinorSegments;
		};
		for(int Major = 0; Major < MajorSegments; ++Major)
		{
			for(int Minor = 0; Minor < MinorSegments; ++Minor)
			{
				const int Current = VertexIndex(Major, Minor);
				const int NextMajor = VertexIndex((Major + 1) % MajorSegments, Minor);
				const int NextMinor = VertexIndex(Major, (Minor + 1) % MinorSegments);
				const int Diagonal = VertexIndex((Major + 1) % MajorSegments, (Minor + 1) % MinorSegments);
				Mesh.m_vEdges.push_back({Current, NextMajor});
				Mesh.m_vEdges.push_back({Current, NextMinor});
				Mesh.m_vFaces.push_back({Current, NextMajor, Diagonal});
				Mesh.m_vFaces.push_back({Current, Diagonal, NextMinor});
			}
		}
		return Mesh;
	}();
	return s_Mesh;
}

class CBackgroundParticles : public CComponent
{
	static constexpr int MAX_TRAIL_POINTS = 6;

	struct SParticle
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_DriftVel = vec2(0.0f, 0.0f);
		vec2 m_PushVel = vec2(0.0f, 0.0f);
		float m_Depth = 0.0f;
		float m_Size = 1.0f;
		vec3 m_Rotation = vec3(0.0f, 0.0f, 0.0f);
		vec3 m_RotationSpeed = vec3(0.0f, 0.0f, 0.0f);
		float m_Age = 0.0f;
		float m_Life = 1.0f;
		float m_PulsePhase = 0.0f;
		float m_TwinklePhase = 0.0f;
		float m_TrailSample = 0.0f;
		int m_Type = 1;
		int m_TrailCount = 0;
		std::array<vec2, MAX_TRAIL_POINTS> m_aTrailPos;
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	};

	std::vector<SParticle> m_vParticles;
	std::vector<int> m_vRenderOrder;
	int m_LastConfiguredCount = -1;
	float m_LastLeft = 0.0f;
	float m_LastTop = 0.0f;
	float m_LastRight = 0.0f;
	float m_LastBottom = 0.0f;

	void ResetParticles();
	void EnsureParticleCount(float Left, float Top, float Right, float Bottom);
	void SpawnParticle(SParticle &Particle, bool Initial, float Left, float Top, float Right, float Bottom);
	void UpdateParticle(SParticle &Particle, float Delta, float Left, float Top, float Right, float Bottom);
	void ResetParticleTrail(SParticle &Particle) const;
	void UpdateParticleTrail(SParticle &Particle, float Delta) const;
	void ApplyPlayerPush(SParticle &Particle, float Delta) const;
	void ApplyParticleCollisions(float Delta);
	ColorRGBA ParticleColor() const;
	int ParticleType() const;
	float ParticleAlpha(const SParticle &Particle) const;
	void RenderParticle(const SParticle &Particle, const SBackgroundParticleProjection &Projection) const;
	void RenderParticleTrail(const SParticle &Particle, float Size, ColorRGBA Color, const SBackgroundParticleProjection &Projection) const;
	void RenderShape(int Type, vec2 WorldPos, float Depth, float Size, const vec3 &Rotation, ColorRGBA Color, const SBackgroundParticleProjection &Projection, vec2 ScreenOffset = vec2(0.0f, 0.0f)) const;
	void RenderMesh(const SBackgroundParticleMesh &Mesh, vec2 WorldPos, float Depth, float Size, const vec3 &Rotation, ColorRGBA Color, const SBackgroundParticleProjection &Projection, vec2 ScreenOffset) const;
	void CurrentWorldView(float &Left, float &Top, float &Right, float &Bottom) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
};

#endif
