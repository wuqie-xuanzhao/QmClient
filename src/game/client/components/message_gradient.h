// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_MESSAGE_GRADIENT_H
#define GAME_CLIENT_COMPONENTS_MESSAGE_GRADIENT_H

#include <base/color.h>

#include <engine/textrender.h>

class CMessageGradient
{
public:
	static constexpr int MAX_COLORS = 7;
	static constexpr int MIN_COLORS = 1;

	static int Unpack(const char *pGradient, unsigned *pColors, int MaxColors);
	static void Pack(const unsigned *pColors, int NumColors, char *pGradient, int GradientSize);
	static void Reset(char *pGradient, int GradientSize);
	static void AddTextSplits(CTextCursor &Cursor, const char *pText, const char *pGradient, const ColorRGBA &FallbackColor);
};

#endif
