#ifndef GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGSKINMETRICS_H
#define GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGSKINMETRICS_H

#include <game/client/QmUi/SettingsPageLayout.h>

inline float ResolveQmVisualSkinAppearanceHeight(const SSettingsContentMetrics &Metrics)
{
	// 描边五行、色调三行、阴影一行；两条色调说明使用小字号行高。
	return 9.0f * Metrics.m_RowStep + 2.0f * (Metrics.m_SmallSize + Metrics.m_LineSpacing);
}

inline float ResolveQmVisualSkinTransitionHeight(const SSettingsContentMetrics &Metrics, const bool Enabled)
{
	// 偷皮和动画开关始终可见，五行高级参数仅在开启动画时显示。
	return (2.0f + (Enabled ? 5.0f : 0.0f)) * Metrics.m_RowStep;
}

#endif // GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGSKINMETRICS_H
