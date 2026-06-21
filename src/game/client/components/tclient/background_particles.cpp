#include "background_particles.h"

#include <base/math.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr int MAX_BACKGROUND_PARTICLES = 200;
	constexpr float MIN_VIEW_SIZE = 1.0f;
	constexpr float MAX_EFFECTIVE_SIZE = 24.0f;
	constexpr float PROJ_DIST = 600.0f;
	constexpr int SHAPE_CUBE = 1;
	constexpr int SHAPE_HEART = 2;
	constexpr int SHAPE_MIXED = 3;
	constexpr int SHAPE_SPHERE = 4;
	constexpr int SHAPE_PYRAMID = 5;
	constexpr int SHAPE_DIAMOND = 6;
	constexpr int SHAPE_RING = 7;
	constexpr int SHAPE_STAR = 8;
	constexpr int SHAPE_CRESCENT = 9;
	constexpr int SHAPE_FIRST = SHAPE_CUBE;
	constexpr int SHAPE_LAST = SHAPE_CRESCENT;
	constexpr int REAL_SHAPE_COUNT = 8;
	constexpr int SPHERE_SEGMENTS = 24;
	constexpr int RING_SEGMENTS = 32;
	constexpr int RING_RADIALS = 8;
	constexpr int CRESCENT_SEGMENTS = 28;
	constexpr int STAR_POINTS = 10;
	constexpr int STAR_VERTICES = STAR_POINTS * 2;
	constexpr int STAR_EDGES = STAR_POINTS * 3;
	constexpr int HEART_POINTS = 96;
	constexpr int HEART_LOW_POINTS = 24;
	constexpr int HEART_LAYERS = 5;
	constexpr float HEART_THICKNESS = 0.35f;

	const std::array<vec3, 8> s_aCubeVertices = {{
		vec3(-1.0f, -1.0f, -1.0f),
		vec3(1.0f, -1.0f, -1.0f),
		vec3(1.0f, 1.0f, -1.0f),
		vec3(-1.0f, 1.0f, -1.0f),
		vec3(-1.0f, -1.0f, 1.0f),
		vec3(1.0f, -1.0f, 1.0f),
		vec3(1.0f, 1.0f, 1.0f),
		vec3(-1.0f, 1.0f, 1.0f),
	}};

	const std::array<std::array<int, 2>, 12> s_aCubeEdges = {{
		{{0, 1}},
		{{1, 2}},
		{{2, 3}},
		{{3, 0}},
		{{4, 5}},
		{{5, 6}},
		{{6, 7}},
		{{7, 4}},
		{{0, 4}},
		{{1, 5}},
		{{2, 6}},
		{{3, 7}},
	}};

	const std::array<vec3, 5> s_aPyramidVertices = {{
		vec3(-1.0f, -1.0f, -0.65f),
		vec3(1.0f, -1.0f, -0.65f),
		vec3(1.0f, 1.0f, -0.65f),
		vec3(-1.0f, 1.0f, -0.65f),
		vec3(0.0f, 0.0f, 1.15f),
	}};

	const std::array<std::array<int, 2>, 8> s_aPyramidEdges = {{
		{{0, 1}},
		{{1, 2}},
		{{2, 3}},
		{{3, 0}},
		{{0, 4}},
		{{1, 4}},
		{{2, 4}},
		{{3, 4}},
	}};

	const std::array<vec3, 6> s_aDiamondVertices = {{
		vec3(0.0f, -1.25f, 0.0f),
		vec3(1.0f, 0.0f, 0.0f),
		vec3(0.0f, 0.0f, 1.0f),
		vec3(-1.0f, 0.0f, 0.0f),
		vec3(0.0f, 0.0f, -1.0f),
		vec3(0.0f, 1.25f, 0.0f),
	}};

	const std::array<std::array<int, 2>, 12> s_aDiamondEdges = {{
		{{0, 1}},
		{{0, 2}},
		{{0, 3}},
		{{0, 4}},
		{{5, 1}},
		{{5, 2}},
		{{5, 3}},
		{{5, 4}},
		{{1, 2}},
		{{2, 3}},
		{{3, 4}},
		{{4, 1}},
	}};

	float ClampDelta(float Delta)
	{
		return std::clamp(Delta, 0.0f, 0.05f);
	}

	float ConfigAlpha()
	{
		return std::clamp(g_Config.m_Qm3DParticlesAlpha, 1, 100) / 100.0f;
	}

	float ClampedSizeMin()
	{
		return (float)std::clamp(g_Config.m_Qm3DParticlesSizeMin, 2, 64);
	}

	float ClampedSizeMax()
	{
		const int SizeMin = std::clamp(g_Config.m_Qm3DParticlesSizeMin, 2, 64);
		const int SizeMax = std::clamp(g_Config.m_Qm3DParticlesSizeMax, SizeMin, 64);
		return (float)SizeMax;
	}

	vec3 RotateVec3(const vec3 &V, const vec3 &Rot)
	{
		vec3 Result = V;

		const float Cz = std::cos(Rot.z);
		const float Sz = std::sin(Rot.z);
		Result = vec3(Result.x * Cz - Result.y * Sz, Result.x * Sz + Result.y * Cz, Result.z);

		const float Cx = std::cos(Rot.x);
		const float Sx = std::sin(Rot.x);
		Result = vec3(Result.x, Result.y * Cx - Result.z * Sx, Result.y * Sx + Result.z * Cx);

		const float Cy = std::cos(Rot.y);
		const float Sy = std::sin(Rot.y);
		Result = vec3(Result.x * Cy + Result.z * Sy, Result.y, -Result.x * Sy + Result.z * Cy);

		return Result;
	}

	vec2 ProjectPoint(const vec3 &Pos, const vec2 &Center)
	{
		const float Scale = std::clamp(PROJ_DIST / (PROJ_DIST + Pos.z), 0.5f, 1.6f);
		const vec2 Rel = vec2(Pos.x - Center.x, Pos.y - Center.y);
		return Center + Rel * Scale;
	}

	const std::array<vec3, HEART_POINTS> &HeartVertices()
	{
		static std::array<vec3, HEART_POINTS> s_aVerts;
		static bool s_Initialized = false;
		if(!s_Initialized)
		{
			for(int i = 0; i < HEART_POINTS; i++)
			{
				const float T = 2.0f * pi * (float)i / (float)HEART_POINTS;
				const float X = 16.0f * std::pow(std::sin(T), 3.0f);
				const float Y = 13.0f * std::cos(T) - 5.0f * std::cos(2.0f * T) - 2.0f * std::cos(3.0f * T) - std::cos(4.0f * T);
				s_aVerts[i] = vec3(X, -Y, 0.0f);
			}
			s_Initialized = true;
		}
		return s_aVerts;
	}

	const std::array<vec3, HEART_LOW_POINTS> &HeartLowVertices()
	{
		static std::array<vec3, HEART_LOW_POINTS> s_aVerts;
		static bool s_Initialized = false;
		if(!s_Initialized)
		{
			const auto &HighRes = HeartVertices();
			for(int i = 0; i < HEART_LOW_POINTS; i++)
			{
				const int Src = std::clamp((i * HEART_POINTS) / HEART_LOW_POINTS, 0, HEART_POINTS - 1);
				s_aVerts[i] = HighRes[Src];
			}
			s_Initialized = true;
		}
		return s_aVerts;
	}

	const std::array<vec3, STAR_VERTICES> &StarVertices()
	{
		static std::array<vec3, STAR_VERTICES> s_aVerts;
		static bool s_Initialized = false;
		if(!s_Initialized)
		{
			for(int i = 0; i < STAR_POINTS; i++)
			{
				const float T = -0.5f * pi + 2.0f * pi * (float)i / (float)STAR_POINTS;
				const float Radius = (i % 2) == 0 ? 1.18f : 0.48f;
				const vec3 Front(std::cos(T) * Radius, std::sin(T) * Radius, -0.26f);
				const vec3 Back(Front.x, Front.y, 0.26f);
				s_aVerts[i] = Front;
				s_aVerts[i + STAR_POINTS] = Back;
			}
			s_Initialized = true;
		}
		return s_aVerts;
	}

	const std::array<std::array<int, 2>, STAR_EDGES> &StarEdges()
	{
		static std::array<std::array<int, 2>, STAR_EDGES> s_aEdges;
		static bool s_Initialized = false;
		if(!s_Initialized)
		{
			for(int i = 0; i < STAR_POINTS; i++)
			{
				const int Next = (i + 1) % STAR_POINTS;
				s_aEdges[i] = {{i, Next}};
				s_aEdges[i + STAR_POINTS] = {{i + STAR_POINTS, Next + STAR_POINTS}};
				s_aEdges[i + STAR_POINTS * 2] = {{i, i + STAR_POINTS}};
			}
			s_Initialized = true;
		}
		return s_aEdges;
	}

	template<size_t NumVertices, size_t NumEdges>
	void RenderWireShape(IGraphics *pGraphics, vec2 CameraCenter, const std::array<vec3, NumVertices> &aVertices, const std::array<std::array<int, 2>, NumEdges> &aEdges, vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color)
	{
		pGraphics->SetColor(Color);

		const vec3 RenderPos(Pos.x, Pos.y, 0.0f);
		std::array<vec2, NumVertices> aProjected;
		for(size_t i = 0; i < NumVertices; i++)
		{
			const vec3 Vertex = RotateVec3(aVertices[i] * Size, Rotation) + RenderPos;
			aProjected[i] = ProjectPoint(Vertex, CameraCenter);
		}

		std::array<IGraphics::CLineItem, NumEdges> aLines;
		for(size_t i = 0; i < NumEdges; i++)
		{
			const auto &Edge = aEdges[i];
			aLines[i] = IGraphics::CLineItem(aProjected[Edge[0]], aProjected[Edge[1]]);
		}
		pGraphics->LinesDraw(aLines.data(), aLines.size());
	}

	template<size_t NumVertices>
	void RenderWireLoop(IGraphics *pGraphics, vec2 CameraCenter, const std::array<vec3, NumVertices> &aVertices, vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color)
	{
		pGraphics->SetColor(Color);

		const vec3 RenderPos(Pos.x, Pos.y, 0.0f);
		std::array<vec2, NumVertices> aProjected;
		for(size_t i = 0; i < NumVertices; i++)
		{
			const vec3 Vertex = RotateVec3(aVertices[i] * Size, Rotation) + RenderPos;
			aProjected[i] = ProjectPoint(Vertex, CameraCenter);
		}

		std::array<IGraphics::CLineItem, NumVertices> aLines;
		for(size_t i = 0; i < NumVertices; i++)
			aLines[i] = IGraphics::CLineItem(aProjected[i], aProjected[(i + 1) % NumVertices]);
		pGraphics->LinesDraw(aLines.data(), aLines.size());
	}
}

void CBackgroundParticles::OnReset()
{
	ResetParticles();
}

void CBackgroundParticles::ResetParticles()
{
	m_vParticles.clear();
	m_vRenderOrder.clear();
	m_LastConfiguredCount = -1;
}

void CBackgroundParticles::CurrentWorldView(float &Left, float &Top, float &Right, float &Bottom) const
{
	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_Zoom;
	float aPoints[4];
	Graphics()->MapScreenToWorld(Center.x, Center.y, 100, 100, 100, 0, 0, Graphics()->ScreenAspect(), Zoom, aPoints);
	Left = minimum(aPoints[0], aPoints[2]);
	Top = minimum(aPoints[1], aPoints[3]);
	Right = maximum(aPoints[0], aPoints[2]);
	Bottom = maximum(aPoints[1], aPoints[3]);

	if(Right - Left < MIN_VIEW_SIZE)
		Right = Left + MIN_VIEW_SIZE;
	if(Bottom - Top < MIN_VIEW_SIZE)
		Bottom = Top + MIN_VIEW_SIZE;
}

ColorRGBA CBackgroundParticles::ParticleColor() const
{
	if(g_Config.m_Qm3DParticlesColorMode == 2)
	{
		return color_cast<ColorRGBA>(ColorHSLA(random_float(), random_float(0.55f, 0.85f), random_float(0.55f, 0.75f), 1.0f));
	}
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_Qm3DParticlesColor, true));
}

int CBackgroundParticles::ParticleType() const
{
	const int Type = std::clamp(g_Config.m_Qm3DParticlesType, SHAPE_FIRST, SHAPE_LAST);
	if(Type != SHAPE_MIXED)
		return Type;

	const std::array<int, REAL_SHAPE_COUNT> aRealShapes = {
		SHAPE_CUBE,
		SHAPE_HEART,
		SHAPE_SPHERE,
		SHAPE_PYRAMID,
		SHAPE_DIAMOND,
		SHAPE_RING,
		SHAPE_STAR,
		SHAPE_CRESCENT,
	};
	const int ShapeIndex = std::clamp((int)random_float((float)REAL_SHAPE_COUNT), 0, REAL_SHAPE_COUNT - 1);
	return aRealShapes[ShapeIndex];
}

void CBackgroundParticles::SpawnParticle(SParticle &Particle, bool Initial, float Left, float Top, float Right, float Bottom)
{
	const float Margin = (float)std::clamp(g_Config.m_Qm3DParticlesViewMargin, 0, 1000);
	const float DepthRange = (float)std::clamp(g_Config.m_Qm3DParticlesDepth, 10, 1000);
	const float SizeMin = ClampedSizeMin();
	const float SizeMax = ClampedSizeMax();
	Particle.m_Depth = random_float(0.0f, DepthRange);
	const float DepthFactor = std::clamp(Particle.m_Depth / DepthRange, 0.0f, 1.0f);
	Particle.m_Size = random_float(SizeMin, SizeMax);
	Particle.m_Rotation = vec3(random_float(-0.35f, 0.35f), random_float(-0.35f, 0.35f), random_float(0.0f, 2.0f * pi));
	Particle.m_RotationSpeed = vec3(random_float(-0.08f, 0.08f), random_float(-0.08f, 0.08f), random_float(-0.2f, 0.2f)) * (1.0f - DepthFactor * 0.45f);
	const float Speed = (float)std::clamp(g_Config.m_Qm3DParticlesSpeed, 1, 500) * (1.0f - DepthFactor * 0.60f);
	Particle.m_DriftVel = random_direction() * random_float(0.35f, 1.0f) * Speed;
	Particle.m_PushVel = vec2(0.0f, 0.0f);
	Particle.m_Age = 0.0f;
	Particle.m_Life = random_float(8.0f, 18.0f);
	Particle.m_PulsePhase = random_float(0.0f, 2.0f * pi);
	Particle.m_TwinklePhase = random_float(0.0f, 2.0f * pi);
	Particle.m_TrailSample = 0.0f;
	Particle.m_Type = ParticleType();
	Particle.m_Color = ParticleColor();

	if(Initial)
	{
		Particle.m_Pos = vec2(random_float(Left - Margin, Right + Margin), random_float(Top - Margin, Bottom + Margin));
		Particle.m_Age = random_float(0.0f, Particle.m_Life * 0.75f);
		ResetParticleTrail(Particle);
		return;
	}

	switch((int)random_float(4.0f))
	{
	case 0:
		Particle.m_Pos = vec2(Left - Margin, random_float(Top - Margin, Bottom + Margin));
		break;
	case 1:
		Particle.m_Pos = vec2(Right + Margin, random_float(Top - Margin, Bottom + Margin));
		break;
	case 2:
		Particle.m_Pos = vec2(random_float(Left - Margin, Right + Margin), Top - Margin);
		break;
	default:
		Particle.m_Pos = vec2(random_float(Left - Margin, Right + Margin), Bottom + Margin);
		break;
	}
	ResetParticleTrail(Particle);
}

void CBackgroundParticles::EnsureParticleCount(float Left, float Top, float Right, float Bottom)
{
	const int Count = std::clamp(g_Config.m_Qm3DParticlesCount, 1, MAX_BACKGROUND_PARTICLES);
	if(m_LastConfiguredCount == Count && (int)m_vParticles.size() == Count)
		return;

	m_LastConfiguredCount = Count;
	if((int)m_vParticles.size() > Count)
	{
		m_vParticles.resize(Count);
		return;
	}

	const size_t OldSize = m_vParticles.size();
	m_vParticles.resize(Count);
	for(size_t ParticleIndex = OldSize; ParticleIndex < m_vParticles.size(); ++ParticleIndex)
		SpawnParticle(m_vParticles[ParticleIndex], true, Left, Top, Right, Bottom);
}

void CBackgroundParticles::ResetParticleTrail(SParticle &Particle) const
{
	Particle.m_TrailSample = 0.0f;
	Particle.m_TrailCount = 0;
	for(vec2 &TrailPos : Particle.m_aTrailPos)
		TrailPos = Particle.m_Pos;
}

void CBackgroundParticles::UpdateParticleTrail(SParticle &Particle, float Delta) const
{
	if(!g_Config.m_Qm3DParticlesTrail)
	{
		Particle.m_TrailCount = 0;
		Particle.m_TrailSample = 0.0f;
		return;
	}

	const int TrailLength = std::clamp(g_Config.m_Qm3DParticlesTrailLength, 2, MAX_TRAIL_POINTS);
	constexpr float SampleInterval = 0.045f;
	Particle.m_TrailSample += Delta;
	if(Particle.m_TrailCount > 0 && Particle.m_TrailSample < SampleInterval)
		return;

	Particle.m_TrailSample = 0.0f;
	for(int TrailIndex = minimum(TrailLength - 1, Particle.m_TrailCount); TrailIndex > 0; --TrailIndex)
		Particle.m_aTrailPos[TrailIndex] = Particle.m_aTrailPos[TrailIndex - 1];
	Particle.m_aTrailPos[0] = Particle.m_Pos;
	Particle.m_TrailCount = minimum(Particle.m_TrailCount + 1, TrailLength);
}

void CBackgroundParticles::ApplyPlayerPush(SParticle &Particle, float Delta) const
{
	const float Radius = (float)std::clamp(g_Config.m_Qm3DParticlesPushRadius, 0, 1000);
	const float Strength = (float)std::clamp(g_Config.m_Qm3DParticlesPushStrength, 0, 2000);
	if(Radius <= 0.0f || Strength <= 0.0f)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		const auto &ClientData = GameClient()->m_aClients[ClientId];
		if(!ClientData.m_Active)
			continue;

		const vec2 Diff = Particle.m_Pos - ClientData.m_RenderPos;
		const float Dist = length(Diff);
		if(Dist <= 0.001f || Dist >= Radius)
			continue;

		const float Force = 1.0f - Dist / Radius;
		Particle.m_PushVel += normalize(Diff) * Strength * Force * Delta * 10.0f;
	}
}

void CBackgroundParticles::UpdateParticle(SParticle &Particle, float Delta, float Left, float Top, float Right, float Bottom)
{
	Particle.m_Age += Delta;
	if(Particle.m_Age >= Particle.m_Life)
	{
		SpawnParticle(Particle, false, Left, Top, Right, Bottom);
		return;
	}

	ApplyPlayerPush(Particle, Delta);
	Particle.m_Pos += (Particle.m_DriftVel + Particle.m_PushVel) * Delta;
	Particle.m_PushVel *= std::pow(0.12f, Delta);
	Particle.m_Rotation += Particle.m_RotationSpeed * Delta;
	UpdateParticleTrail(Particle, Delta);

	const float Margin = (float)std::clamp(g_Config.m_Qm3DParticlesViewMargin, 0, 1000);
	if(Particle.m_Pos.x < Left - Margin || Particle.m_Pos.x > Right + Margin ||
		Particle.m_Pos.y < Top - Margin || Particle.m_Pos.y > Bottom + Margin)
	{
		SpawnParticle(Particle, false, Left, Top, Right, Bottom);
	}
}

void CBackgroundParticles::ApplyParticleCollisions(float Delta)
{
	if(!g_Config.m_Qm3DParticlesCollide)
		return;

	for(size_t LeftIndex = 0; LeftIndex < m_vParticles.size(); ++LeftIndex)
	{
		for(size_t RightIndex = LeftIndex + 1; RightIndex < m_vParticles.size(); ++RightIndex)
		{
			SParticle &Left = m_vParticles[LeftIndex];
			SParticle &Right = m_vParticles[RightIndex];
			const vec2 Diff = Right.m_Pos - Left.m_Pos;
			const float Dist = length(Diff);
			const float MinDist = (Left.m_Size + Right.m_Size) * 0.45f;
			if(Dist <= 0.001f || Dist >= MinDist)
				continue;

			const vec2 Dir = Diff / Dist;
			const float Push = (MinDist - Dist) * Delta * 12.0f;
			Left.m_PushVel -= Dir * Push;
			Right.m_PushVel += Dir * Push;
		}
	}
}

float CBackgroundParticles::ParticleAlpha(const SParticle &Particle) const
{
	const float FadeIn = std::max(0.001f, g_Config.m_Qm3DParticlesFadeInMs / 1000.0f);
	const float FadeOut = std::max(0.001f, g_Config.m_Qm3DParticlesFadeOutMs / 1000.0f);
	const float In = std::clamp(Particle.m_Age / FadeIn, 0.0f, 1.0f);
	const float Out = std::clamp((Particle.m_Life - Particle.m_Age) / FadeOut, 0.0f, 1.0f);
	return ConfigAlpha() * minimum(In, Out);
}

void CBackgroundParticles::RenderCube(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	RenderWireShape(Graphics(), GameClient()->m_Camera.m_Center, s_aCubeVertices, s_aCubeEdges, Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderHeart(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	Graphics()->SetColor(Color);

	const auto &aVerts = HeartLowVertices();
	const float Scale = Size * 0.055f;
	const float LayerStep = HEART_LAYERS > 1 ? 2.0f / (float)(HEART_LAYERS - 1) : 0.0f;
	const vec3 RenderPos(Pos.x, Pos.y, 0.0f);
	std::array<std::array<vec2, HEART_LOW_POINTS>, HEART_LAYERS> aProjected;
	std::array<float, HEART_LAYERS> aLayerZ;

	for(int Layer = 0; Layer < HEART_LAYERS; Layer++)
	{
		const float LayerT = -1.0f + LayerStep * (float)Layer;
		const float Z = LayerT * (Size * HEART_THICKNESS);
		aLayerZ[Layer] = Z;
		const float LayerScale = 1.0f - std::abs(LayerT) * 0.08f;
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			const vec3 Local(aVerts[i].x * Scale * LayerScale, aVerts[i].y * Scale * LayerScale, Z);
			const vec3 Vertex = RotateVec3(Local, Rotation) + RenderPos;
			aProjected[Layer][i] = ProjectPoint(Vertex, GameClient()->m_Camera.m_Center);
		}
	}

	for(int Layer = 0; Layer < HEART_LAYERS; Layer++)
	{
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aRingLines;
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			const int Next = (i + 1) % HEART_LOW_POINTS;
			aRingLines[i] = IGraphics::CLineItem(aProjected[Layer][i], aProjected[Layer][Next]);
		}
		Graphics()->LinesDraw(aRingLines.data(), aRingLines.size());
	}

	for(int Layer = 0; Layer < HEART_LAYERS - 1; Layer++)
	{
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aVertical;
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aDiagonal;
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			const int Next = (i + 1) % HEART_LOW_POINTS;
			aVertical[i] = IGraphics::CLineItem(aProjected[Layer][i], aProjected[Layer + 1][i]);
			aDiagonal[i] = IGraphics::CLineItem(aProjected[Layer][i], aProjected[Layer + 1][Next]);
		}
		Graphics()->LinesDraw(aVertical.data(), aVertical.size());
		Graphics()->LinesDraw(aDiagonal.data(), aDiagonal.size());
	}

	if(HEART_LAYERS >= 2)
	{
		const int Front = 0;
		const int Back = HEART_LAYERS - 1;
		const vec2 CenterFront = ProjectPoint(RotateVec3(vec3(0.0f, 0.0f, aLayerZ[Front]), Rotation) + RenderPos, GameClient()->m_Camera.m_Center);
		const vec2 CenterBack = ProjectPoint(RotateVec3(vec3(0.0f, 0.0f, aLayerZ[Back]), Rotation) + RenderPos, GameClient()->m_Camera.m_Center);
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aFront;
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aBack;
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			aFront[i] = IGraphics::CLineItem(CenterFront, aProjected[Front][i]);
			aBack[i] = IGraphics::CLineItem(CenterBack, aProjected[Back][i]);
		}
		Graphics()->LinesDraw(aFront.data(), aFront.size());
		Graphics()->LinesDraw(aBack.data(), aBack.size());
	}
}

void CBackgroundParticles::RenderSphere(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	std::array<vec3, SPHERE_SEGMENTS> aCircle;

	for(int Axis = 0; Axis < 3; Axis++)
	{
		for(int i = 0; i < SPHERE_SEGMENTS; i++)
		{
			const float T = 2.0f * pi * (float)i / (float)SPHERE_SEGMENTS;
			const float C = std::cos(T);
			const float S = std::sin(T);
			switch(Axis)
			{
			case 0: aCircle[i] = vec3(C, S, 0.0f); break;
			case 1: aCircle[i] = vec3(C, 0.0f, S); break;
			default: aCircle[i] = vec3(0.0f, C, S); break;
			}
		}
		RenderWireLoop(Graphics(), GameClient()->m_Camera.m_Center, aCircle, Pos, Size, Rotation, Color);
	}
}

void CBackgroundParticles::RenderPyramid(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	RenderWireShape(Graphics(), GameClient()->m_Camera.m_Center, s_aPyramidVertices, s_aPyramidEdges, Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderDiamond(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	RenderWireShape(Graphics(), GameClient()->m_Camera.m_Center, s_aDiamondVertices, s_aDiamondEdges, Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderRing(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	constexpr int RingVertices = RING_SEGMENTS * 2;
	constexpr int RingEdges = RING_SEGMENTS * 2 + RING_RADIALS;
	std::array<vec3, RingVertices> aVertices;
	std::array<std::array<int, 2>, RingEdges> aEdges;

	for(int i = 0; i < RING_SEGMENTS; i++)
	{
		const float T = 2.0f * pi * (float)i / (float)RING_SEGMENTS;
		const float C = std::cos(T);
		const float S = std::sin(T);
		aVertices[i] = vec3(C * 1.15f, S * 1.15f, 0.0f);
		aVertices[i + RING_SEGMENTS] = vec3(C * 0.64f, S * 0.64f, 0.0f);
		aEdges[i] = {{i, (i + 1) % RING_SEGMENTS}};
		aEdges[i + RING_SEGMENTS] = {{i + RING_SEGMENTS, ((i + 1) % RING_SEGMENTS) + RING_SEGMENTS}};
	}

	for(int i = 0; i < RING_RADIALS; i++)
	{
		const int VertexIndex = (i * RING_SEGMENTS) / RING_RADIALS;
		aEdges[RING_SEGMENTS * 2 + i] = {{VertexIndex, VertexIndex + RING_SEGMENTS}};
	}

	RenderWireShape(Graphics(), GameClient()->m_Camera.m_Center, aVertices, aEdges, Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderStar(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	RenderWireShape(Graphics(), GameClient()->m_Camera.m_Center, StarVertices(), StarEdges(), Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderCrescent(vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	constexpr int CrescentVertices = CRESCENT_SEGMENTS * 2;
	std::array<vec3, CrescentVertices> aVertices;
	constexpr float Start = -0.78f * pi;
	constexpr float End = 0.78f * pi;

	for(int i = 0; i < CRESCENT_SEGMENTS; i++)
	{
		const float T = Start + (End - Start) * (float)i / (float)(CRESCENT_SEGMENTS - 1);
		aVertices[i] = vec3(std::cos(T), std::sin(T), 0.0f);
	}
	for(int i = 0; i < CRESCENT_SEGMENTS; i++)
	{
		const float T = End - (End - Start) * (float)i / (float)(CRESCENT_SEGMENTS - 1);
		aVertices[i + CRESCENT_SEGMENTS] = vec3(std::cos(T) * 0.58f + 0.36f, std::sin(T) * 0.58f, 0.18f);
	}

	RenderWireLoop(Graphics(), GameClient()->m_Camera.m_Center, aVertices, Pos, Size, Rotation, Color);
}

void CBackgroundParticles::RenderShape(int Type, vec2 Pos, float Size, const vec3 &Rotation, ColorRGBA Color) const
{
	switch(Type)
	{
	case SHAPE_HEART:
		RenderHeart(Pos, Size, Rotation, Color);
		break;
	case SHAPE_SPHERE:
		RenderSphere(Pos, Size, Rotation, Color);
		break;
	case SHAPE_PYRAMID:
		RenderPyramid(Pos, Size, Rotation, Color);
		break;
	case SHAPE_DIAMOND:
		RenderDiamond(Pos, Size, Rotation, Color);
		break;
	case SHAPE_RING:
		RenderRing(Pos, Size, Rotation, Color);
		break;
	case SHAPE_STAR:
		RenderStar(Pos, Size, Rotation, Color);
		break;
	case SHAPE_CRESCENT:
		RenderCrescent(Pos, Size, Rotation, Color);
		break;
	default:
		RenderCube(Pos, Size, Rotation, Color);
		break;
	}
}

void CBackgroundParticles::RenderParticleTrail(const SParticle &Particle, vec2 Center, float Parallax, float Size, ColorRGBA Color) const
{
	if(!g_Config.m_Qm3DParticlesTrail || Particle.m_TrailCount <= 1)
		return;

	const int TrailLength = std::clamp(g_Config.m_Qm3DParticlesTrailLength, 2, MAX_TRAIL_POINTS);
	const int TrailCount = minimum(Particle.m_TrailCount, TrailLength);
	const float TrailAlpha = std::clamp(g_Config.m_Qm3DParticlesTrailAlpha, 1, 100) / 100.0f;

	for(int TrailIndex = TrailCount - 1; TrailIndex >= 1; --TrailIndex)
	{
		const float Fade = (float)(TrailCount - TrailIndex) / (float)TrailCount;
		const vec2 Pos = Center + (Particle.m_aTrailPos[TrailIndex] - Center) * Parallax;
		const float TrailSize = Size * (0.82f + 0.18f * Fade);
		RenderShape(Particle.m_Type, Pos, TrailSize, Particle.m_Rotation, Color.WithMultipliedAlpha(TrailAlpha * Fade));
	}
}

void CBackgroundParticles::RenderParticle(const SParticle &Particle, vec2 Center) const
{
	const float DepthRange = (float)std::clamp(g_Config.m_Qm3DParticlesDepth, 10, 1000);
	const float DepthFactor = std::clamp(Particle.m_Depth / DepthRange, 0.0f, 1.0f);
	const float Parallax = 1.0f - DepthFactor * 0.72f;
	const vec2 Pos = Center + (Particle.m_Pos - Center) * Parallax;
	float Size = minimum(Particle.m_Size, MAX_EFFECTIVE_SIZE) * (0.72f - DepthFactor * 0.25f);
	if(g_Config.m_Qm3DParticlesPulse)
	{
		const float PulseStrength = std::clamp(g_Config.m_Qm3DParticlesPulseStrength, 0, 50) / 100.0f;
		const float PulseSpeed = std::clamp(g_Config.m_Qm3DParticlesPulseSpeed, 10, 300) / 100.0f;
		Size *= maximum(0.2f, 1.0f + std::sin(Particle.m_Age * PulseSpeed * 2.0f * pi + Particle.m_PulsePhase) * PulseStrength);
	}

	float Alpha = ParticleAlpha(Particle) * (1.0f - DepthFactor * 0.35f);
	if(g_Config.m_Qm3DParticlesTwinkle)
	{
		const float TwinkleStrength = std::clamp(g_Config.m_Qm3DParticlesTwinkleStrength, 0, 100) / 100.0f;
		const float TwinkleWave = 0.5f + 0.5f * std::sin(Particle.m_Age * 3.2f + Particle.m_TwinklePhase);
		const float TwinkleAlpha = 0.35f + 0.65f * TwinkleWave;
		Alpha *= 1.0f + (TwinkleAlpha - 1.0f) * TwinkleStrength;
	}
	if(Alpha <= 0.01f || Size <= 0.5f)
		return;

	const ColorRGBA Color = Particle.m_Color.WithMultipliedAlpha(Alpha);
	RenderParticleTrail(Particle, Center, Parallax, Size, Color);

	if(g_Config.m_Qm3DParticlesGlow)
	{
		const float GlowAlpha = std::clamp(g_Config.m_Qm3DParticlesGlowAlpha, 1, 100) / 100.0f;
		const float GlowOffset = (float)std::clamp(g_Config.m_Qm3DParticlesGlowOffset, 1, 20);
		const vec2 GlowPos = Pos - vec2(GlowOffset, GlowOffset);
		const ColorRGBA GlowColor = Color.WithMultipliedAlpha(GlowAlpha);
		RenderShape(Particle.m_Type, GlowPos, Size, Particle.m_Rotation, GlowColor);
	}

	RenderShape(Particle.m_Type, Pos, Size, Particle.m_Rotation, Color);
}

void CBackgroundParticles::OnRender()
{
	if(!g_Config.m_Qm3DParticles)
	{
		m_LastConfiguredCount = -1;
		return;
	}

	float Left;
	float Top;
	float Right;
	float Bottom;
	CurrentWorldView(Left, Top, Right, Bottom);
	EnsureParticleCount(Left, Top, Right, Bottom);
	if(m_vParticles.empty())
		return;

	const float Delta = ClampDelta(Client()->RenderFrameTime());
	const bool ViewJumped = absolute(Left - m_LastLeft) + absolute(Top - m_LastTop) + absolute(Right - m_LastRight) + absolute(Bottom - m_LastBottom) > (Right - Left + Bottom - Top);
	m_LastLeft = Left;
	m_LastTop = Top;
	m_LastRight = Right;
	m_LastBottom = Bottom;
	if(ViewJumped)
	{
		for(SParticle &Particle : m_vParticles)
			SpawnParticle(Particle, true, Left, Top, Right, Bottom);
	}

	for(SParticle &Particle : m_vParticles)
		UpdateParticle(Particle, Delta, Left, Top, Right, Bottom);
	ApplyParticleCollisions(Delta);

	m_vRenderOrder.clear();
	m_vRenderOrder.reserve(m_vParticles.size());
	for(size_t ParticleIndex = 0; ParticleIndex < m_vParticles.size(); ++ParticleIndex)
		m_vRenderOrder.push_back((int)ParticleIndex);
	std::sort(m_vRenderOrder.begin(), m_vRenderOrder.end(), [&](int LeftIndex, int RightIndex) {
		return m_vParticles[LeftIndex].m_Depth > m_vParticles[RightIndex].m_Depth;
	});

	Graphics()->MapScreen(Left, Top, Right, Bottom);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	const vec2 Center = GameClient()->m_Camera.m_Center;
	for(const int ParticleIndex : m_vRenderOrder)
		RenderParticle(m_vParticles[ParticleIndex], Center);
	Graphics()->LinesEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
