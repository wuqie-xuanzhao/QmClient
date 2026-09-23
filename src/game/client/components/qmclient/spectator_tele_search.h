#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_TELE_SEARCH_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SPECTATOR_TELE_SEARCH_H

#include <base/vmath.h>

#include <engine/keys.h>

#include <game/mapitems.h>

namespace qm_spectator_tele
{
	inline int DigitFromKey(int Key)
	{
		if(Key == KEY_0 || Key == KEY_KP_0)
			return 0;
		if(Key >= KEY_1 && Key <= KEY_9)
			return Key - KEY_1 + 1;
		if(Key >= KEY_KP_1 && Key <= KEY_KP_9)
			return Key - KEY_KP_1 + 1;
		return -1;
	}

	inline int ParseNumber(const char *pText)
	{
		int Number = 0;
		for(; *pText; ++pText)
		{
			if(*pText < '0' || *pText > '9')
				return 0;
			Number = Number * 10 + *pText - '0';
			if(Number > 255)
				return 0;
		}
		return Number;
	}

	inline int StepNumber(int Number, int Direction)
	{
		if(Number < 1 || Number > 255)
			return Direction < 0 ? 255 : 1;
		return (Number - 1 + Direction + 255) % 255 + 1;
	}

	inline int FindNext(const CTeleTile *pTiles, int Width, int Height, int Number, int LastIndex)
	{
		if(pTiles == nullptr || Width <= 0 || Height <= 0 || Number < 1 || Number > 255)
			return -1;
		const int TileCount = Width * Height;
		if(LastIndex < 0 || LastIndex >= TileCount)
			LastIndex = -1;
		const auto Matches = [&](int Index) {
			const CTeleTile &Tile = pTiles[Index];
			return Tile.m_Number == Number && IsValidTeleTile(Tile.m_Type) && IsTeleTileNumberUsedAny(Tile.m_Type);
		};
		const vec2 LastPos(LastIndex % Width, LastIndex / Width);
		for(int Index = LastIndex + 1; Index < TileCount; ++Index)
		{
			if(!Matches(Index))
				continue;
			// 沿用编辑器的距离规则，跳过上一个目标附近的同编号格子。
			if(LastIndex == -1 || distance(LastPos, vec2(Index % Width, Index / Width)) >= 10.0f)
				return Index;
		}
		// 到末尾后从地图起点重新查找；只有一处时仍定位到该处。
		for(int Index = 0; Index <= LastIndex; ++Index)
		{
			if(Matches(Index))
				return Index;
		}
		return -1;
	}
}

#endif
