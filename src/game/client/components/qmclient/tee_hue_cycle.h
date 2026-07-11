#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_TEE_HUE_CYCLE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_TEE_HUE_CYCLE_H

#include <base/color.h>

#include <game/client/render.h>

struct SQmTeeHueCycleConfig
{
	bool m_Enabled = false;
	bool m_PlayerUsesCustomColors = false;
	bool m_TClientRainbowTees = false;
	int m_SpeedDegreesPerSecond = 0;
	double m_TimeSeconds = 0.0;
	int m_SixupIndex = 0;
};

struct SQmLocalTeeHueCycleEligibility
{
	bool m_IsLocal = false;
	bool m_IsDummy = false;
	bool m_DummyEnabled = false;
	bool m_UseCustomColors = false;
	bool m_UseCustomColors7 = false;
};

bool QmShouldApplyLocalTeeHueCycle(const SQmLocalTeeHueCycleEligibility &Eligibility);

float QmTeeHueCyclePhase(double TimeSeconds, int SpeedDegreesPerSecond);
ColorRGBA QmCycleTeeHueColor(ColorRGBA Color, float Phase);
bool QmShouldCycleTeeHue(const CTeeRenderInfo &Info, const SQmTeeHueCycleConfig &Config);
bool QmApplyTeeHueCycle(CTeeRenderInfo &Info, const SQmTeeHueCycleConfig &Config);

#endif
