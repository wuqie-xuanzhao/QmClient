/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMMOTION_H
#define GAME_CLIENT_QMUI_QMMOTION_H

#include "QmAnim.h"

#include <algorithm>

namespace qm_motion
{
	inline int NormalizeMotionLevel(int MotionLevel)
	{
		return std::clamp(MotionLevel, 0, 2);
	}

	inline SUiAnimTransition ApplyMotionLevel(SUiAnimTransition Transition, int MotionLevel)
	{
		switch(NormalizeMotionLevel(MotionLevel))
		{
		case 0:
			Transition.m_DurationSec = 0.0f;
			Transition.m_DelaySec = 0.0f;
			Transition.m_Driver = EUiAnimDriver::TWEEN;
			Transition.m_Easing = EEasing::LINEAR;
			break;
		case 1:
			Transition.m_DurationSec *= 0.45f;
			Transition.m_DelaySec *= 0.25f;
			Transition.m_Spring.m_Damping *= 1.35f;
			Transition.m_Spring.m_RestEpsilon *= 2.0f;
			Transition.m_Spring.m_RestVelocity *= 2.0f;
			break;
		case 2:
		default:
			break;
		}
		return Transition;
	}
} // namespace qm_motion

#endif
