#ifndef ENGINE_CLIENT_QUAD_ROTATION_CACHE_H
#define ENGINE_CLIENT_QUAD_ROTATION_CACHE_H

#include <base/vmath.h>

#include <cmath>

class CQmQuadRotationCache
{
	float m_Angle = 0.0f;
	vec2 m_Direction = vec2(1.0f, 0.0f);

public:
	vec2 Get(float Angle)
	{
		// 仅在 CPU 需要旋转顶点时调用；连续相同角度复用结果，不量化角度。
		if(Angle != m_Angle)
		{
			m_Direction = vec2(std::cos(Angle), std::sin(Angle));
			m_Angle = Angle;
		}
		return m_Direction;
	}
};

#endif
