#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCORE_HUD_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCORE_HUD_LAYOUT_H

#include <algorithm>

struct SQmScoreHudLayout
{
	float m_BoxLeft;
	float m_BoxWidth;
	float m_RankX;
	float m_TeeX;
};

inline SQmScoreHudLayout QmScoreHudLayout(float Right, float ScoreWidth, float RankTextWidth, float TeeSize)
{
	constexpr float Split = 3.0f;
	constexpr float ImageSize = 16.0f;
	// 保留原最小宽度，并补足人物绘制尺寸与图标占位的差值和文字间距。
	const float PosSize = std::max(16.0f, RankTextWidth + Split + TeeSize - ImageSize);
	const float BoxWidth = ScoreWidth + ImageSize + 2.0f * Split + PosSize;
	const float BoxLeft = Right - BoxWidth;
	return {BoxLeft, BoxWidth, BoxLeft + Split, Right - ScoreWidth - TeeSize / 2.0f - Split};
}

#endif
