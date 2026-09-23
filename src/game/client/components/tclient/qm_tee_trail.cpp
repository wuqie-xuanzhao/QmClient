// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_tee_trail.h"

#include <base/math.h>

#include <game/client/components/qmclient/trail_band_geometry.h>
#include <game/client/components/qmclient/trail_band_section.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
	using namespace qm_tee_trail;

	float Smooth(float X)
	{
		X = std::clamp(X, 0.0f, 1.0f);
		return X * X * (3.0f - 2.0f * X);
	}

	unsigned Hash(unsigned X)
	{
		X ^= X >> 16;
		X *= 0x7feb352du;
		X ^= X >> 15;
		X *= 0x846ca68bu;
		return X ^ (X >> 16);
	}

	float Random(unsigned X)
	{
		return (Hash(X) & 0xffffffu) / float(0xffffffu);
	}

	// 噪声坐标固定在累计弧长上，不使用节点下标或当前帧时间重新播种。
	float Noise(double S, unsigned Seed, bool Sharp = false)
	{
		const double Cell = std::floor(S);
		const unsigned Index = static_cast<unsigned>(static_cast<int64_t>(Cell));
		float T = float(S - Cell);
		if(!Sharp)
			T = Smooth(T);
		return mix(Random(Index + Seed), Random(Index + 1u + Seed), T) * 2.0f - 1.0f;
	}

	vec2 Unit(vec2 V, vec2 Fallback = vec2(1, 0))
	{
		const float Len = length(V);
		return Len > 0.0001f ? V / Len : Fallback;
	}

	ColorRGBA Tint(ColorRGBA A, ColorRGBA B, float T)
	{
		return ColorRGBA(mix(A.r, B.r, T), mix(A.g, B.g, T), mix(A.b, B.b, T), mix(A.a, B.a, T));
	}

	struct SStyle
	{
		float m_Life, m_Width, m_Wave, m_Wavelength, m_Jag, m_Rise;
		ColorRGBA m_Outer, m_Main, m_Core, m_Accent, m_Cold;
		bool m_GlowBody;
	};

	const SStyle s_aStyles[STYLE_COUNT] = {
		{1.0f, 1.0f, 0, 90, 0, 0, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, false},
		{0.9f, 0.86f, 0.10f, 35, 0.42f, 0, {0.018f, 0.006f, 0.013f, 1}, {0.008f, 0.004f, 0.008f, 1}, {1, 0.015f, 0.07f, 1}, {0.82f, 0.008f, 0.045f, 1}, {0.015f, 0.003f, 0.009f, 1}, false},
		{0.85f, 0.48f, 0.035f, 110, 0, 0, {0.12f, 0.18f, 0.9f, 1}, {0.02f, 0.8f, 1, 1}, {0.88f, 0.98f, 1, 1}, {0.92f, 0.04f, 0.86f, 1}, {0.22f, 0.06f, 0.55f, 1}, true},
		{1.45f, 0.63f, 0.75f, 105, 0, 9, {0.08f, 0.25f, 0.62f, 1}, {0.22f, 0.7f, 0.98f, 1}, {0.78f, 0.98f, 1, 1}, {0.5f, 0.8f, 1, 1}, {0.08f, 0.2f, 0.45f, 1}, true},
		{1.15f, 1.0f, 0.24f, 115, 0, 2, {0.025f, 0.029f, 0.04f, 1}, {0.003f, 0.004f, 0.007f, 1}, {0.42f, 0.46f, 0.52f, 1}, {0.19f, 0.22f, 0.27f, 1}, {0.002f, 0.003f, 0.004f, 1}, false},
		{1.05f, 0.94f, 0.21f, 48, 0.1f, 42, {0.065f, 0.007f, 0.004f, 1}, {0.68f, 0.045f, 0.012f, 1}, {1, 0.68f, 0.13f, 1}, {1, 0.22f, 0.018f, 1}, {0.045f, 0.008f, 0.006f, 1}, false},
	};

	struct SSample
	{
		vec2 m_Pos, m_Normal;
		ColorRGBA m_Tint;
		double m_Distance;
		float m_Age, m_Width, m_Alpha, m_Energy, m_Head;
	};

	struct SBandPoint
	{
		vec2 m_Pos;
		float m_Left, m_Right;
		ColorRGBA m_Color;
	};

	void PushQuad(std::vector<SQuad> &vOut, vec2 A, vec2 B, vec2 C, vec2 D, ColorRGBA Ca, ColorRGBA Cb, ColorRGBA Cc, ColorRGBA Cd, bool Additive)
	{
		if(vOut.size() >= MAX_QUADS || std::max({Ca.a, Cb.a, Cc.a, Cd.a}) < 0.001f)
			return;
		vOut.push_back({{A, B, C, D}, {Ca, Cb, Cc, Cd}, Additive});
	}

	SQmTrailBandSection PrepareBandSection(const SBandPoint &Point, vec2 Normal, float Softness, float PixelSize)
	{
		return QmPrepareTrailBandSection(Point.m_Pos, Point.m_Left, Point.m_Right, Point.m_Color, Normal, Softness, PixelSize);
	}

	// 同一个横截面与柔边算法用于主体、细丝和短裂纹。相邻截面共用顶点，没有圆点接缝。
	void EmitPreparedBand(std::vector<SQuad> &vOut, const SQmTrailBandSection &A, const SQmTrailBandSection &B, bool Additive)
	{
		for(int Strip = 0; Strip < 3; ++Strip)
			PushQuad(vOut, A.m_aPos[Strip], B.m_aPos[Strip], B.m_aPos[Strip + 1], A.m_aPos[Strip + 1],
				A.m_aColor[Strip], B.m_aColor[Strip], B.m_aColor[Strip + 1], A.m_aColor[Strip + 1], Additive);
	}

	void EmitBand(std::vector<SQuad> &vOut, const SBandPoint &A, const SBandPoint &B, vec2 Na, vec2 Nb, float Softness, float PixelSize, bool Additive)
	{
		EmitPreparedBand(vOut, PrepareBandSection(A, Na, Softness, PixelSize), PrepareBandSection(B, Nb, Softness, PixelSize), Additive);
	}

	std::array<SBandPoint, 4> Shape(const SSample &S, int Style, bool Preset, unsigned Seed)
	{
		const SStyle &Spec = s_aStyles[Style];
		const double D = S.m_Distance;
		// 多个非整数倍谐波共用连续相位，不能提前取模，否则每个波长都会出现断口。
		const double Phase = D / Spec.m_Wavelength * (2 * pi) + Random(Seed) * 2 * pi;
		const float AgeSeconds = S.m_Age / 50.0f;
		const float Root = Smooth(S.m_Head / 16.0f);
		float Wave = std::sin(Phase + (Style == STYLE_BLACK_FLASH ? 0.0f : AgeSeconds * 1.5f));
		float Offset = Spec.m_Wave * Wave * Root;
		float Left = 1.0f + Spec.m_Jag * Noise(D / 7.0, Seed, true);
		float Right = 1.0f + Spec.m_Jag * Noise(D / 9.0, Seed + 73u, true);
		float WidthScale = 1.0f, Alpha = 1.0f;
		float Rise = Spec.m_Rise * AgeSeconds * Root;
		float CoreOffset = 0.0f, CoreWidth = 0.15f;
		float AccentOffset = -0.5f, AccentWidth = 0.055f, AccentAlpha = 0.4f;
		float CoreAlpha = 0.85f;

		// 风格只定义截面、扰动与配色；采样、寿命、细分、接缝和绘制全部共用。
		switch(Style)
		{
		case STYLE_BLACK_FLASH:
			Offset += 0.3f * Noise(D / 11.0, Seed + 5u, true) * Root;
			WidthScale = 0.72f + 0.45f * std::abs(Noise(D / 17.0, Seed + 12u, true));
			CoreOffset = Noise(D / 8.0, Seed + 39u, true) * 0.55f;
			CoreWidth = 0.065f;
			CoreAlpha *= Smooth((Noise(D / 19.0, Seed + 117u, true) + 0.45f) * 3.0f);
			AccentOffset = CoreOffset + Noise(D / 15.0, Seed + 93u, true) * 0.65f;
			AccentAlpha = CoreAlpha * (0.35f + S.m_Energy * 0.65f);
			break;
		case STYLE_VOID:
			Left += 0.16f * std::sin(Phase * 1.7f + AgeSeconds);
			Right += 0.12f * std::cos(Phase * 1.3f);
			WidthScale = 0.9f + 0.15f * Wave;
			Alpha = 0.35f + 0.65f * Smooth((Noise(D / 35.0, Seed + 51u) + 0.7f) * 2.0f);
			CoreOffset = -0.8f * Left;
			CoreWidth = 0.022f;
			CoreAlpha = 0.13f * Smooth(Noise(D / 41.0, Seed + 63u) + 0.25f);
			AccentOffset = 0.85f * Right;
			AccentWidth = 0.016f;
			AccentAlpha = 0.06f;
			break;
		case STYLE_INFERNO:
			Rise *= 0.9f + 0.18f * std::sin(Phase * 1.6f + AgeSeconds * 3);
			Offset += 0.1f * std::sin(Phase * 2.1f + AgeSeconds * 4) * Root;
			Left += 0.3f * std::max(0.0f, Wave) * Root;
			Right += 0.35f * std::max(0.0f, -Wave) * Root;
			CoreOffset = 0.1f * std::sin(Phase * 1.8f + AgeSeconds * 4);
			CoreWidth = 0.22f;
			AccentOffset = -0.6f + 0.4f * Wave;
			AccentWidth = 0.1f * (0.6f + 0.4f * Wave);
			AccentAlpha = 0.55f;
			break;
		case STYLE_EXO:
			CoreWidth = 0.12f;
			AccentOffset = 0.73f;
			AccentWidth = 0.05f;
			AccentAlpha = 0.7f;
			break;
		case STYLE_SPIRIT:
			CoreOffset = 0.16f * std::sin(Phase + 0.9f + AgeSeconds * 1.5f);
			CoreWidth = 0.19f;
			AccentOffset = -0.65f * std::sin(Phase + AgeSeconds * 1.5f);
			AccentWidth = 0.075f;
			AccentAlpha = 0.32f;
			break;
		default: break;
		}

		const float aWidths[] = {1.4f, 1.0f, CoreWidth, AccentWidth};
		const float aOffsets[] = {0, 0, CoreOffset, AccentOffset};
		const float aAlphas[] = {Spec.m_GlowBody ? 0.22f : 0.65f, 0.94f, CoreAlpha, AccentAlpha};
		ColorRGBA aColors[] = {Spec.m_Outer, Spec.m_Main, Spec.m_Core, Spec.m_Accent};
		if(Style == STYLE_EXO)
			aColors[1] = Tint(Spec.m_Main, Spec.m_Accent, 0.22f + 0.22f * std::sin(Phase));
		if(Style == STYLE_INFERNO)
		{
			const float Cool = Smooth(S.m_Age / 30.0f);
			aColors[1] = Tint(Spec.m_Main, Spec.m_Cold, Cool);
			aColors[2] = Tint(Spec.m_Core, Spec.m_Main, Cool);
		}
		if(!Preset || Style == STYLE_ORIGINAL)
		{
			const ColorRGBA Base = S.m_Tint.WithAlpha(1);
			const ColorRGBA Black(0, 0, 0, 1), White(1, 1, 1, 1);
			const bool Dark = Style == STYLE_BLACK_FLASH || Style == STYLE_VOID;
			aColors[0] = Tint(Base, Black, Dark ? 0.97f : 0.65f);
			aColors[1] = Tint(Base, Black, Dark ? 0.985f : 0.05f);
			aColors[2] = Tint(Base, White, Style == STYLE_BLACK_FLASH ? 0.0f : 0.72f);
			aColors[3] = Tint(Base, White, 0.35f);
		}
		const float W = S.m_Width * Spec.m_Width * WidthScale;
		std::array<SBandPoint, 4> Result;
		for(int Layer = 0; Layer < 4; ++Layer)
		{
			Result[Layer].m_Pos = S.m_Pos + S.m_Normal * (S.m_Width * Offset + W * aOffsets[Layer]) - vec2(0, Rise);
			Result[Layer].m_Left = W * aWidths[Layer] * (Layer < 2 ? Left : 1.0f);
			Result[Layer].m_Right = W * aWidths[Layer] * (Layer < 2 ? Right : 1.0f);
			Result[Layer].m_Color = aColors[Layer].WithAlpha(std::clamp(S.m_Alpha * Alpha * aAlphas[Layer], 0.0f, 1.0f));
			if(Style == STYLE_ORIGINAL)
				Result[Layer].m_Color = S.m_Tint.WithAlpha(S.m_Alpha);
		}
		return Result;
	}
}

qm_tee_trail::SPreparedCurve qm_tee_trail::PrepareCurve(vec2 P0, vec2 P1, vec2 P2, vec2 P3)
{
	// 限制三次曲线切向量，折返时压短到零，避免普通 Catmull-Rom 在尖角处过冲。
	const float D = distance(P1, P2);
	const vec2 Segment = Unit(P2 - P1);
	const vec2 Before = Unit(P1 - P0, Segment);
	const vec2 After = Unit(P3 - P2, Segment);
	const vec2 M1 = Unit(Before + Segment, Segment) * D * std::max(0.0f, dot(Before, Segment));
	const vec2 M2 = Unit(Segment + After, Segment) * D * std::max(0.0f, dot(Segment, After));
	return {P1, P2, M1, M2};
}

vec2 qm_tee_trail::SPreparedCurve::Evaluate(float T) const
{
	return m_Start * (2 * T * T * T - 3 * T * T + 1) + m_StartTangent * (T * T * T - 2 * T * T + T) + m_End * (-2 * T * T * T + 3 * T * T) + m_EndTangent * (T * T * T - T * T);
}

void qm_tee_trail::CTrailState::Reset()
{
	m_First = m_Count = 0;
	m_LastTime = -1.0;
	m_Carry = 0.0;
	m_LastSpeed = 0.0f;
	m_Head = CTrailPart();
}

void qm_tee_trail::CTrailState::Push(const CTrailPart &Point)
{
	if(m_Count == MAX_POINTS)
	{
		m_First = (m_First + 1) % MAX_POINTS;
		--m_Count;
	}
	m_aPoints[(m_First + m_Count++) % MAX_POINTS] = Point;
}

void qm_tee_trail::CTrailState::Update(vec2 Position, double Time, float Speed, float Life, bool Break)
{
	if(!std::isfinite(Position.x) || !std::isfinite(Position.y) || !std::isfinite(Time) || !std::isfinite(Speed) || Time < 0)
	{
		Reset();
		return;
	}
	Speed = std::max(0.0f, Speed);
	Life = std::clamp(Life, 1.0f, 400.0f);
	const double Dt = Time - m_LastTime;
	const float Dist = distance(Position, m_LastPos);
	// 长帧间隔不猜测漏掉的运动；正常高速位移由速度预算区分，允许补齐多个中间点。
	if(Break || Dt < 0 || Dt > 12.5 || (m_LastTime >= 0 && Dist > 48.0 + std::max(Speed, m_LastSpeed) * std::max(Dt, 0.0) * 2.5))
		Reset();
	while(m_Count > 0 && Time - m_aPoints[m_First].m_Time >= m_aPoints[m_First].m_Life)
	{
		m_First = (m_First + 1) % MAX_POINTS;
		--m_Count;
	}
	if(m_LastTime < 0 || m_Count == 0)
	{
		m_Head = CTrailPart();
		m_Head.m_Pos = Position;
		m_Head.m_Time = Time;
		m_Head.m_Tick = int(Time);
		m_Head.m_Life = Life;
		m_Head.m_Speed = Speed;
		m_Carry = 0;
		Push(m_Head);
	}
	else if(Dt > 0 && Speed >= MIN_SPEED && Dist / Dt >= MIN_SPEED && Dist > 0.0001f)
	{
		const double StartDistance = m_Head.m_Distance;
		const double EndDistance = StartDistance + Dist;
		double Along = SAMPLE_SPACING - m_Carry;
		// 极端速度也只保留最后一个容量窗口，不为将被覆盖的点做无用循环。
		if((Dist - Along) / SAMPLE_SPACING > MAX_POINTS)
			Along += std::floor((Dist - Along) / SAMPLE_SPACING - MAX_POINTS) * SAMPLE_SPACING;
		for(; Along <= Dist + 0.00001; Along += SAMPLE_SPACING)
		{
			const float T = std::clamp(float(Along / Dist), 0.0f, 1.0f);
			CTrailPart Point;
			Point.m_Pos = mix(m_LastPos, Position, T);
			Point.m_Time = m_LastTime + Dt * T;
			Point.m_Tick = int(Point.m_Time);
			Point.m_Distance = StartDistance + Along;
			Point.m_Speed = mix(m_LastSpeed, Speed, T);
			Point.m_Life = mix(m_Head.m_Life, Life, T);
			Push(Point);
		}
		m_Carry = std::max(0.0, std::fmod(m_Carry + Dist + 0.00001, double(SAMPLE_SPACING)) - 0.00001);
		m_Head.m_Pos = Position;
		m_Head.m_Time = Time;
		m_Head.m_Tick = int(Time);
		m_Head.m_Distance = EndDistance;
		m_Head.m_Speed = Speed;
		m_Head.m_Life = Life;
	}
	m_LastPos = Position;
	m_LastTime = Time;
	m_LastSpeed = Speed;
}

void qm_tee_trail::CTrailState::Export(std::vector<CTrailPart> &vOut) const
{
	vOut.clear();
	if(m_Count == 0 || m_LastTime - m_Head.m_Time >= m_Head.m_Life)
		return;
	vOut.reserve(MAX_POINTS + 1);
	vOut.push_back(m_Head);
	for(size_t i = m_Count; i > 0; --i)
	{
		const CTrailPart &Point = m_aPoints[(m_First + i - 1) % MAX_POINTS];
		if(distance(vOut.back().m_Pos, Point.m_Pos) > 0.001f)
			vOut.push_back(Point);
	}
}

int qm_tee_trail::ResolveStyle(int Style)
{
	return Style > STYLE_ORIGINAL && Style < STYLE_COUNT ? Style : STYLE_ORIGINAL;
}

float qm_tee_trail::Lifetime(int Style, int Length, float Speed)
{
	const float Energy = std::clamp(Speed / 30.0f, 0.0f, 1.0f);
	return std::clamp(Length, 5, 200) * s_aStyles[ResolveStyle(Style)].m_Life * (0.75f + 0.45f * Energy);
}

void qm_tee_trail::BuildEffect(const std::vector<CTrailPart> &vTrail, int Style, bool UsePresetPalette, double CurTime, float Width, int Seed, std::vector<SQuad> &vOut, float PixelSize, bool Taper, bool Fade)
{
	vOut.clear();
	if(vTrail.size() < 2 || !std::isfinite(CurTime) || !std::isfinite(Width) || !std::isfinite(PixelSize))
		return;
	Style = ResolveStyle(Style);
	PixelSize = std::clamp(PixelSize, 0.025f, 8.0f);
	Width = Width > 0 ? Width : (Style == STYLE_ORIGINAL ? PixelSize * 0.5f : 4.0f);
	const size_t Count = std::min(vTrail.size(), MAX_POINTS + 1);
	std::array<float, MAX_POINTS + 1> aLengths{};
	for(size_t i = 0; i < Count; ++i)
	{
		const auto &P = vTrail[i];
		if(!std::isfinite(P.m_Pos.x) || !std::isfinite(P.m_Pos.y) || !std::isfinite(P.m_Time) || !std::isfinite(P.m_Distance))
			return;
		const double Time = P.m_Time >= 0 ? P.m_Time : P.m_Tick;
		if(Time > CurTime + 0.01)
			return;
		if(i > 0)
		{
			const float D = distance(vTrail[i - 1].m_Pos, P.m_Pos);
			if(D > 192.0f)
				return;
			aLengths[i] = aLengths[i - 1] + D;
		}
	}
	const float Total = aLengths[Count - 1];
	if(Total < 0.01f)
		return;

	std::array<SSample, MAX_RENDER_POINTS> aSamples;
	size_t SampleCount = 0;
	// 每段至少一个截面，剩余预算用于屏幕空间细分，不会因达到上限丢掉整条尾部。
	const float Step = std::max({PixelSize * 2.5f, 0.75f, Total / float(MAX_RENDER_POINTS - Count)});
	for(size_t i = 0; i + 1 < Count; ++i)
	{
		const auto &A = vTrail[i];
		const auto &B = vTrail[i + 1];
		const float Segment = aLengths[i + 1] - aLengths[i];
		const int Divisions = std::max(1, int(Segment / Step));
		const SPreparedCurve Curve = PrepareCurve(i > 0 ? vTrail[i - 1].m_Pos : A.m_Pos * 2 - B.m_Pos, A.m_Pos, B.m_Pos, i + 2 < Count ? vTrail[i + 2].m_Pos : B.m_Pos * 2 - A.m_Pos);
		for(int j = 0; j < Divisions + (i + 2 == Count ? 1 : 0); ++j)
		{
			if(SampleCount == MAX_RENDER_POINTS)
				break;
			const float T = float(j) / Divisions;
			SSample &S = aSamples[SampleCount++];
			const double Birth = mix(A.m_Time >= 0 ? A.m_Time : double(A.m_Tick), B.m_Time >= 0 ? B.m_Time : double(B.m_Tick), double(T));
			S.m_Age = std::max(0.0f, float(CurTime - Birth));
			const float Life = mix(A.m_Life > 0 ? A.m_Life : 25.0f * s_aStyles[Style].m_Life, B.m_Life > 0 ? B.m_Life : 25.0f * s_aStyles[Style].m_Life, T);
			const float Remaining = std::clamp(1.0f - S.m_Age / Life, 0.0f, 1.0f);
			const float Speed = A.m_Speed >= 0 && B.m_Speed >= 0 ? mix(A.m_Speed, B.m_Speed, T) : Segment / std::max(0.01f, float(std::abs(A.m_Tick - B.m_Tick)));
			S.m_Energy = std::clamp(Speed / 30.0f, 0.0f, 1.0f);
			S.m_Head = mix(aLengths[i], aLengths[i + 1], T);
			S.m_Distance = A.m_Time >= 0 ? mix(A.m_Distance, B.m_Distance, double(T)) : -double(S.m_Head);
			S.m_Tint = Tint(A.m_Col, B.m_Col, T);
			const float Tail = Smooth((Total - S.m_Head) / std::max(1.0f, std::min(Total, 30.0f)));
			const float Tapering = (Style != STYLE_ORIGINAL || Taper) ? std::pow(Remaining, 0.65f) * Tail : 1.0f;
			S.m_Width = Width * (Style == STYLE_ORIGINAL ? 1.0f : 0.7f + 0.4f * S.m_Energy) * Tapering;
			S.m_Alpha = std::clamp(S.m_Tint.a, 0.0f, 1.0f) * Remaining * Remaining * Tail;
			if(Fade)
				S.m_Alpha *= 1.0f - S.m_Head / Total;
			S.m_Pos = Curve.Evaluate(T);
		}
	}
	if(SampleCount < 2)
		return;
	vOut.reserve(MAX_QUADS);
	std::array<std::array<SBandPoint, MAX_RENDER_POINTS>, 4> aaBands;
	const unsigned StableSeed = unsigned(Seed) * 0x9e3779b9u;
	if(Style == STYLE_ORIGINAL)
	{
		// 原版只有主体层，宽度系数为 1，扰动与抬升为 0；不准备未使用的特效层。
		for(size_t i = 0; i < SampleCount; ++i)
		{
			const SSample &S = aSamples[i];
			aaBands[1][i] = {S.m_Pos, S.m_Width, S.m_Width, S.m_Tint.WithAlpha(S.m_Alpha)};
		}
	}
	else
	{
		for(size_t i = 0; i < SampleCount; ++i)
		{
			const vec2 Before = aSamples[i > 0 ? i - 1 : i].m_Pos;
			const vec2 After = aSamples[i + 1 < SampleCount ? i + 1 : i].m_Pos;
			const vec2 Tangent = Unit(After - Before);
			aSamples[i].m_Normal = vec2(-Tangent.y, Tangent.x);
		}
		for(size_t i = 0; i < SampleCount; ++i)
		{
			const auto aLayers = Shape(aSamples[i], Style, UsePresetPalette, StableSeed);
			for(int Layer = 0; Layer < 4; ++Layer)
				aaBands[Layer][i] = aLayers[Layer];
		}
	}
	for(int Layer = Style == STYLE_ORIGINAL ? 1 : 0; Layer < (Style == STYLE_ORIGINAL ? 2 : 4); ++Layer)
	{
		auto &aBand = aaBands[Layer];
		std::array<vec2, MAX_RENDER_POINTS> aNormals;
		QmPrepareTrailBandJoins(aBand.data(), SampleCount, aNormals.data());

		const bool Additive = Style != STYLE_ORIGINAL && (s_aStyles[Style].m_GlowBody || (Layer >= 2 && Style != STYLE_VOID));
		const float Softness = Layer == 0 ? 0.85f : (Style == STYLE_SPIRIT || (Style == STYLE_INFERNO && Layer >= 2) ? 0.65f : (Style == STYLE_BLACK_FLASH ? 0.06f : 0.22f));
		SQmTrailBandSection Previous = PrepareBandSection(aBand[0], aNormals[0], Softness, PixelSize);
		for(size_t i = 1; i < SampleCount; ++i)
		{
			const SQmTrailBandSection Current = PrepareBandSection(aBand[i], aNormals[i], Softness, PixelSize);
			EmitPreparedBand(vOut, Previous, Current, Additive);
			Previous = Current;
		}
	}

	// 余烬 / 灵光按历史锚点选取，每玩家最多四个；不创建全局粒子或每帧随机对象。
	if(Style == STYLE_SPIRIT || Style == STYLE_INFERNO)
	{
		int Details = 0;
		for(size_t i = 1; i + 1 < Count && Details < 4; ++i)
		{
			const auto &P = vTrail[i];
			const unsigned Id = unsigned(static_cast<int64_t>(std::floor(P.m_Time >= 0 ? P.m_Distance / SAMPLE_SPACING : -aLengths[i] / SAMPLE_SPACING)));
			if(Hash(Id + StableSeed) % 13 != 0)
				continue;
			const float Age = std::max(0.0f, float(CurTime - (P.m_Time >= 0 ? P.m_Time : double(P.m_Tick))));
			const float Life = P.m_Life > 0 ? P.m_Life : 25.0f * s_aStyles[Style].m_Life;
			const float Gain = Smooth(Age / 5) * std::pow(std::clamp(1 - Age / Life, 0.0f, 1.0f), 2.0f) * Smooth((Total - aLengths[i]) / 30.0f);
			if(Gain < 0.01f)
				continue;
			++Details;
			const float Phase = Random(Id + StableSeed + 9u) * 2 * pi;
			const vec2 Center = P.m_Pos + vec2(std::sin(Phase + Age / 30) * Width * 0.75f, -Age * (Style == STYLE_INFERNO ? 0.9f : 0.4f) - Width * 0.5f);
			const ColorRGBA Color = (UsePresetPalette ? s_aStyles[Style].m_Core : P.m_Col).WithAlpha(P.m_Col.a * Gain * 0.55f);
			const float Radius = (Style == STYLE_INFERNO ? 0.8f : 1.2f) * (0.75f + Random(Id));
			if(Style == STYLE_INFERNO)
			{
				// 少量附着在历史锚点上的翻卷火舌，仍走同一带状柔边绘制，不堆粒子。
				const vec2 Back = Unit(vTrail[i + 1].m_Pos - P.m_Pos);
				const vec2 Root = P.m_Pos - vec2(0, Age * 0.65f);
				const float Span = Width * (0.9f + Random(Id + 51u) * 0.6f);
				const vec2 C1 = Root + Back * (Span * 0.4f) - vec2(0, Span * 0.2f);
				const vec2 C2 = Root + Back * (Span * 1.5f) - vec2(0, Span * 1.1f);
				const vec2 Tip = Root + Back * (Span * 1.2f) - vec2(0, Span * 1.5f);
				SBandPoint Previous;
				vec2 PreviousNormal(0, 1);
				for(int StepIndex = 0; StepIndex <= 6; ++StepIndex)
				{
					const float T = StepIndex / 6.0f, U = 1 - T;
					const vec2 Pos = Root * (U * U * U) + C1 * (3 * U * U * T) + C2 * (3 * U * T * T) + Tip * (T * T * T);
					const vec2 Tangent = Unit((C1 - Root) * (U * U) + (C2 - C1) * (2 * U * T) + (Tip - C2) * (T * T));
					const vec2 Normal(-Tangent.y, Tangent.x);
					const ColorRGBA TipColor = UsePresetPalette ? s_aStyles[Style].m_Main : P.m_Col;
					const SBandPoint Current{Pos, Width * 0.2f * U * U, Width * 0.2f * U * U, Tint(Color, TipColor, T).WithAlpha(Color.a * U * 0.65f)};
					if(StepIndex > 0)
						EmitBand(vOut, Previous, Current, PreviousNormal, Normal, 0.65f, PixelSize, true);
					Previous = Current;
					PreviousNormal = Normal;
				}
			}
			for(int Side = 0; Side < 8; ++Side)
			{
				const vec2 A = Center + direction(Side * pi / 4) * Radius * 2;
				const vec2 B = Center + direction((Side + 1) * pi / 4) * Radius * 2;
				PushQuad(vOut, Center, A, B, Center, Color, Color.WithAlpha(0), Color.WithAlpha(0), Color, true);
			}
		}
	}
}
