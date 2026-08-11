#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATE_TEXT_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATE_TEXT_EFFECTS_H

#include <game/client/render.h>

#include <algorithm>

inline float QmNameplateTextEffectPadding(int Effects, int BorderRange, int GlowRange)
{
	float Padding = 0.0f;
	if((Effects & QM_TEXT_EFFECT_BORDER) != 0)
		Padding = (float)std::clamp(BorderRange, 1, 4);
	if((Effects & QM_TEXT_EFFECT_GLOW) != 0)
		Padding = std::max(Padding, (float)std::clamp(GlowRange, 1, 12));
	return Padding;
}

#endif
