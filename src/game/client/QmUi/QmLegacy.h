// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_QMLEGACY_H
#define GAME_CLIENT_QMUI_QMLEGACY_H

#include "../ui_rect.h"
#include "QmLayout.h"

class CUiV2LegacyAdapter
{
public:
	static CUIRect ToCUIRect(const SUiLayoutBox &Box);
	static SUiLayoutBox FromCUIRect(const CUIRect &Rect);
};

#endif
