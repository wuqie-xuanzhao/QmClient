#ifndef GAME_EDITOR_ENUMS_H
#define GAME_EDITOR_ENUMS_H

#include <engine/shared/localization.h>

constexpr const char *GAME_TILE_OP_NAMES[] = {
	Localizable("Air", "Editor"),
	Localizable("Hookable", "Editor"),
	Localizable("Death", "Editor"),
	Localizable("Unhookable", "Editor"),
	Localizable("Hookthrough", "Editor"),
	Localizable("Freeze", "Editor"),
	Localizable("Unfreeze", "Editor"),
	Localizable("Deep Freeze", "Editor"),
	Localizable("Deep Unfreeze", "Editor"),
	Localizable("Blue Check-Tele", "Editor"),
	Localizable("Red Check-Tele", "Editor"),
	Localizable("Live Freeze", "Editor"),
	Localizable("Live Unfreeze", "Editor"),
};
enum class EGameTileOp
{
	AIR,
	HOOKABLE,
	DEATH,
	UNHOOKABLE,
	HOOKTHROUGH,
	FREEZE,
	UNFREEZE,
	DEEP_FREEZE,
	DEEP_UNFREEZE,
	BLUE_CHECK_TELE,
	RED_CHECK_TELE,
	LIVE_FREEZE,
	LIVE_UNFREEZE,
};

constexpr const char *AUTOMAP_REFERENCE_NAMES[] = {
	Localizable("Game Layer", "Editor"),
	Localizable("Hookable", "Editor"),
	Localizable("Death", "Editor"),
	Localizable("Unhookable", "Editor"),
	Localizable("Freeze", "Editor"),
	Localizable("Unfreeze", "Editor"),
	Localizable("Deep Freeze", "Editor"),
	Localizable("Deep Unfreeze", "Editor"),
	Localizable("Live Freeze", "Editor"),
	Localizable("Live Unfreeze", "Editor"),
};

#endif
