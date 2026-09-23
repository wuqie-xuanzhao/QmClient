// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_TRAIL_BAND_SECTION_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_TRAIL_BAND_SECTION_H

#include <base/color.h>
#include <base/vmath.h>

#include <algorithm>

struct SQmTrailBandSection
{
	vec2 m_aPos[4];
	ColorRGBA m_aColor[4];
};

// 相邻线段共用截面；柔边的四个顶点和颜色只计算一次。
inline SQmTrailBandSection QmPrepareTrailBandSection(vec2 Position, float Left, float Right, ColorRGBA Color, vec2 Normal, float Softness, float PixelSize)
{
	const float Width = std::max(Left, Right);
	const float Feather = std::clamp(std::max(Softness, PixelSize / std::max(Width, PixelSize)), 0.02f, 1.0f);
	const float aOffsets[] = {-Left, -Left * (1 - Feather), Right * (1 - Feather), Right};
	const float aAlphas[] = {0, 1, 1, 0};
	SQmTrailBandSection Section;
	for(int Edge = 0; Edge < 4; ++Edge)
	{
		Section.m_aPos[Edge] = Position + Normal * aOffsets[Edge];
		Section.m_aColor[Edge] = Color.WithMultipliedAlpha(aAlphas[Edge]);
	}
	return Section;
}

#endif
