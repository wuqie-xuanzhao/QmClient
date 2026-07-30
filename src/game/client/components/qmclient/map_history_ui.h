// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MAP_HISTORY_UI_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MAP_HISTORY_UI_H

namespace QmMapHistoryUi
{

inline constexpr float CARD_GAP = 8.0f;
inline constexpr float CARD_ROW_HEIGHT = 96.0f;
inline constexpr float LIST_SCROLLBAR_WIDTH = 20.0f;

constexpr int GridColumns(float AvailableWidth)
{
	if(AvailableWidth >= 766.0f)
		return 3;
	if(AvailableWidth >= 508.0f)
		return 2;
	return 1;
}

constexpr bool StackControls(float AvailableWidth)
{
	return AvailableWidth < 600.0f;
}

} // namespace QmMapHistoryUi

#endif
