#include "QmCardRegistry.h"

#include <base/system.h>

namespace qm_card_registry
{
	static const std::vector<SCardDefault> &DefaultsTable()
	{
		// clang-format off
		static const std::vector<SCardDefault> s_aDefaults = {
			// === 栖梦侧栏模块（38）· qm:<key>（显式默认值齐全，来源 s_aQmModuleDefaults）===
			{"qm:info", nullptr, ECardColumn::Full, 0},
			{"qm:chat_bubble", "visual", ECardColumn::Left, 0},
			{"qm:camera_view", "visual", ECardColumn::Right, 0},
			{"qm:skin_transition", "visual", ECardColumn::Left, 1},
			{"qm:focus_mode", "visual", ECardColumn::Left, 2},
			{"qm:weapon_animation", "visual", ECardColumn::Right, 1},
			{"qm:entity_overlay", "visual", ECardColumn::Right, 2},
			{"qm:collision_hitbox", "visual", ECardColumn::Right, 5},
			{"qm:streamer", "visual", ECardColumn::Left, 10},
			{"qm:translate_ui", "visual", ECardColumn::Left, 15},
			{"qm:card_appearance", "visual", ECardColumn::Left, 17},
			{"qm:gores_actor", "function", ECardColumn::Left, 3},
			{"qm:gores", "function", ECardColumn::Left, 4},
			{"qm:key_binds", "function", ECardColumn::Left, 5},
			{"qm:mini_features", "function", ECardColumn::Left, 6},
			{"qm:jump_hint", "function", ECardColumn::Left, 7},
			{"qm:weapon_trajectory", "function", ECardColumn::Left, 8},
			{"qm:coords", "hud", ECardColumn::Left, 9},
			{"qm:friend_notify", "function", ECardColumn::Left, 11},
			{"qm:block_words", "function", ECardColumn::Left, 12},
			{"qm:qiafen", "function", ECardColumn::Left, 13}, // UI 名 keyword_reply，以持久化 key qiafen 为权威
			{"qm:translate", "function", ECardColumn::Left, 14},
			{"qm:pie_menu", "function", ECardColumn::Left, 16},
			{"qm:favorite_maps", "function", ECardColumn::Right, 6},
			{"qm:hj_assist", "function", ECardColumn::Right, 7},
			{"qm:player_stats", "hud", ECardColumn::Right, 4},
			{"qm:speedrun_timer", "hud", ECardColumn::Right, 8},
			{"qm:debug_graph", "hud", ECardColumn::Right, 9},
			{"qm:input_overlay", "hud", ECardColumn::Right, 10},
			{"qm:hud_notifications", "hud", ECardColumn::Right, 11},
			{"qm:voice", "hud", ECardColumn::Right, 12},
			{"qm:dummy_miniview", "hud", ECardColumn::Right, 13},
			{"qm:dynamic_island", "hud", ECardColumn::Right, 14},
			{"qm:system_media_controls", "hud", ECardColumn::Right, 15},
			{"qm:lyrics", "hud", ECardColumn::Right, 16},
			{"qm:background_3d", "hud", ECardColumn::Right, 17},
			{"qm:nameplate_text", "hud", ECardColumn::Right, 18}, // 数据债：原无 tab 归属，B1 补 hud
			{"qm:laser", "visual", ECardColumn::Right, 3}, // 数据债：原无 tab 归属，B1 补 visual

			// === Tclient section（15）· tclient:<name>（id 不变；column/order 按附录序显式化）===
			{"tclient:visual-nameplates", "tclient", ECardColumn::Left, 0},
			{"tclient:visual-effects", "tclient", ECardColumn::Left, 1},
			{"tclient:input", "tclient", ECardColumn::Left, 2},
			{"tclient:anti-latency-tools", "tclient", ECardColumn::Left, 3},
			{"tclient:improved-anti-ping", "tclient", ECardColumn::Left, 4},
			{"tclient:execute-on-join", "tclient", ECardColumn::Left, 5},
			{"tclient:voting", "tclient", ECardColumn::Left, 6},
			{"tclient:player-indicator", "tclient", ECardColumn::Left, 7},
			{"tclient:tee-status-bar", "tclient", ECardColumn::Left, 8},
			{"tclient:tile-outlines", "tclient", ECardColumn::Left, 9},
			{"tclient:ghost-tools", "tclient", ECardColumn::Left, 10},
			{"tclient:rainbow", "tclient", ECardColumn::Left, 11},
			{"tclient:tee-trails", "tclient", ECardColumn::Left, 12},
			{"tclient:background-draw", "tclient", ECardColumn::Left, 13},
			{"tclient:finish-name", "tclient", ECardColumn::Left, 14},

			// === 设置 deck（16）· deck:<page>-<card>（原无持久化；tab=归属页，column/order 按附录序显式化）===
			{"deck:graphics-display", "graphics", ECardColumn::Left, 0},
			{"deck:graphics-visual", "graphics", ECardColumn::Left, 1},
			{"deck:graphics-backend", "graphics", ECardColumn::Left, 2},
			{"deck:graphics-modes", "graphics", ECardColumn::Left, 3},
			{"deck:sound-toggle", "sound", ECardColumn::Left, 4},
			{"deck:sound-volume", "sound", ECardColumn::Left, 5},
			{"deck:sound-audio-pack", "sound", ECardColumn::Left, 6},
			{"deck:ddnet-demo", "ddnet", ECardColumn::Left, 7},
			{"deck:ddnet-gameplay", "ddnet", ECardColumn::Left, 8},
			{"deck:ddnet-background", "ddnet", ECardColumn::Left, 9},
			{"deck:ddnet-miscellaneous", "ddnet", ECardColumn::Left, 10},
			{"deck:tclient-bind-wheel-editor", "tclient-bind-wheel", ECardColumn::Left, 11},
			{"deck:tclient-bind-wheel-preview", "tclient-bind-wheel", ECardColumn::Left, 12},
			{"deck:tclient-status-bar-settings", "tclient-status-bar", ECardColumn::Left, 13},
			{"deck:tclient-status-bar-items", "tclient-status-bar", ECardColumn::Left, 14},
			{"deck:tclient-status-bar-preview", "tclient-status-bar", ECardColumn::Left, 15},
		};
		// clang-format on
		return s_aDefaults;
	}

	const std::vector<SCardDefault> &Defaults()
	{
		return DefaultsTable();
	}

	const SCardDefault *FindByStableId(const char *pStableId)
	{
		if(pStableId == nullptr)
			return nullptr;
		for(const SCardDefault &D : DefaultsTable())
		{
			if(str_comp(D.m_pStableId, pStableId) == 0)
				return &D;
		}
		return nullptr;
	}
} // namespace qm_card_registry
