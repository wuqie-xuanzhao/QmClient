// 碰撞体积可视化的无状态几何辅助。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_COLLISION_HITBOX_LOGIC_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_COLLISION_HITBOX_LOGIC_H

#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <vector>

struct SCollisionHitboxLine
{
	vec2 m_From;
	vec2 m_To;
};

// 返回线段扫掠指定半径后的闭合胶囊轮廓。
// 零长度线段会退化为圆，Radius 非正或输入不是有限值时返回空结果。
inline std::vector<SCollisionHitboxLine> BuildHitboxCapsuleOutline(vec2 From, vec2 To, float Radius, int ArcSegments = 16)
{
	std::vector<SCollisionHitboxLine> vLines;
	if(Radius <= 0.0f || !std::isfinite(Radius) || !std::isfinite(From.x) || !std::isfinite(From.y) || !std::isfinite(To.x) || !std::isfinite(To.y))
		return vLines;

	ArcSegments = std::clamp(ArcSegments, 2, 64);
	const vec2 Delta = To - From;
	const float SegmentLength = length(Delta);
	if(!std::isfinite(SegmentLength) || SegmentLength <= 1e-6f)
	{
		const int CircleSegments = ArcSegments * 2;
		vLines.reserve(CircleSegments);
		for(int Index = 0; Index < CircleSegments; ++Index)
		{
			const float Angle0 = 2.0f * pi * (float)Index / (float)CircleSegments;
			const float Angle1 = 2.0f * pi * (float)(Index + 1) / (float)CircleSegments;
			vLines.push_back({From + vec2(std::cos(Angle0), std::sin(Angle0)) * Radius,
				From + vec2(std::cos(Angle1), std::sin(Angle1)) * Radius});
		}
		return vLines;
	}

	const vec2 Direction = Delta / SegmentLength;
	const vec2 Normal(-Direction.y, Direction.x);
	vLines.reserve(2 + ArcSegments * 2);
	vLines.push_back({From + Normal * Radius, To + Normal * Radius});
	vLines.push_back({To - Normal * Radius, From - Normal * Radius});

	const float DirectionAngle = std::atan2(Direction.y, Direction.x);
	const auto AppendArc = [&vLines, ArcSegments, Radius](vec2 Center, float StartAngle) {
		for(int Index = 0; Index < ArcSegments; ++Index)
		{
			const float Angle0 = StartAngle - pi * (float)Index / (float)ArcSegments;
			const float Angle1 = StartAngle - pi * (float)(Index + 1) / (float)ArcSegments;
			vLines.push_back({Center + vec2(std::cos(Angle0), std::sin(Angle0)) * Radius,
				Center + vec2(std::cos(Angle1), std::sin(Angle1)) * Radius});
		}
	};

	// 终点半圆向线段前进方向鼓出，起点半圆向反方向鼓出。
	AppendArc(To, DirectionAngle + pi / 2.0f);
	AppendArc(From, DirectionAngle - pi / 2.0f);
	return vLines;
}

#endif
