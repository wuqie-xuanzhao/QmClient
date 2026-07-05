#include "tee_hue_cycle.h"

#include <generated/protocol7.h>

#include <algorithm>
#include <cmath>

namespace
{
	float QmWrapHue(float Hue)
	{
		const float Wrapped = std::fmod(Hue, 1.0f);
		return Wrapped < 0.0f ? Wrapped + 1.0f : Wrapped;
	}

	bool QmValidSixupIndex(int SixupIndex)
	{
		return SixupIndex >= 0 && SixupIndex < NUM_DUMMIES;
	}

	bool QmSixupUsesBodyOrFeetCustomColor(const CTeeRenderInfo &Info, int SixupIndex)
	{
		if(!QmValidSixupIndex(SixupIndex))
			return false;

		const CTeeRenderInfo::CSixup &Sixup = Info.m_aSixup[SixupIndex];
		return Sixup.m_aUseCustomColors[protocol7::SKINPART_BODY] || Sixup.m_aUseCustomColors[protocol7::SKINPART_FEET];
	}
} // namespace

float QmTeeHueCyclePhase(double TimeSeconds, int SpeedDegreesPerSecond)
{
	const int ClampedSpeed = std::clamp(SpeedDegreesPerSecond, 0, 360);
	if(ClampedSpeed == 0)
		return 0.0f;

	const double Cycles = TimeSeconds * ClampedSpeed / 360.0;
	const double Phase = std::fmod(Cycles, 1.0);
	return (float)(Phase < 0.0 ? Phase + 1.0 : Phase);
}

ColorRGBA QmCycleTeeHueColor(ColorRGBA Color, float Phase)
{
	ColorHSLA Hsla = color_cast<ColorHSLA>(Color);
	Hsla.h = QmWrapHue(Hsla.h + Phase);
	return color_cast<ColorRGBA>(Hsla);
}

bool QmShouldCycleTeeHue(const CTeeRenderInfo &Info, const SQmTeeHueCycleConfig &Config)
{
	if(!Config.m_Enabled || !Config.m_PlayerUsesCustomColors || Config.m_TClientRainbowTees)
		return false;

	return Info.m_CustomColoredSkin || QmSixupUsesBodyOrFeetCustomColor(Info, Config.m_SixupIndex);
}

bool QmApplyTeeHueCycle(CTeeRenderInfo &Info, const SQmTeeHueCycleConfig &Config)
{
	if(!QmShouldCycleTeeHue(Info, Config))
		return false;

	const float Phase = QmTeeHueCyclePhase(Config.m_TimeSeconds, Config.m_SpeedDegreesPerSecond);
	bool Applied = false;
	if(Info.m_CustomColoredSkin)
	{
		Info.m_ColorBody = QmCycleTeeHueColor(Info.m_ColorBody, Phase);
		Info.m_ColorFeet = QmCycleTeeHueColor(Info.m_ColorFeet, Phase);
		Applied = true;
	}

	if(QmValidSixupIndex(Config.m_SixupIndex))
	{
		CTeeRenderInfo::CSixup &Sixup = Info.m_aSixup[Config.m_SixupIndex];
		if(Sixup.m_aUseCustomColors[protocol7::SKINPART_BODY])
		{
			Sixup.m_aColors[protocol7::SKINPART_BODY] = QmCycleTeeHueColor(Sixup.m_aColors[protocol7::SKINPART_BODY], Phase);
			Applied = true;
		}
		if(Sixup.m_aUseCustomColors[protocol7::SKINPART_FEET])
		{
			Sixup.m_aColors[protocol7::SKINPART_FEET] = QmCycleTeeHueColor(Sixup.m_aColors[protocol7::SKINPART_FEET], Phase);
			Applied = true;
		}
	}

	return Applied;
}
