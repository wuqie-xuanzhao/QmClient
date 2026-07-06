#include "QmCardRegistry.h"

#include <base/system.h>

namespace qm_card_registry
{
	static const std::vector<SCardDefault> &DefaultsTable()
	{
		// clang-format off
		static const std::vector<SCardDefault> s_aDefaults = {
			// === 栖梦侧栏模块（38）· qm:<key>（显式默认值齐全，来源 s_aQmModuleDefaults）===
			{"qm:info", nullptr, ECardColumn::Full, 0, "QmClient", "qmclient info"},
			{"qm:chat_bubble", "visual", ECardColumn::Left, 0, "Chat bubble", "chat bubble visual"},
			{"qm:camera_view", "visual", ECardColumn::Right, 0, "Camera view", "camera view visual"},
			{"qm:skin_transition", "visual", ECardColumn::Left, 1, "Skin transition", "skin transition visual"},
			{"qm:focus_mode", "visual", ECardColumn::Left, 2, "Focus mode", "focus mode visual"},
			{"qm:weapon_animation", "visual", ECardColumn::Right, 1, "Weapon animation", "weapon animation visual"},
			{"qm:entity_overlay", "visual", ECardColumn::Right, 2, "Entity overlay", "entity overlay visual"},
			{"qm:collision_hitbox", "visual", ECardColumn::Right, 5, "Collision hitbox", "collision hitbox visual"},
			{"qm:streamer", "visual", ECardColumn::Left, 10, "Streamer mode", "streamer mode visual"},
			{"qm:translate_ui", "visual", ECardColumn::Left, 15, "Translate UI", "translate ui visual"},
			{"qm:card_appearance", "visual", ECardColumn::Left, 17, "Card appearance", "card appearance visual"},
			{"qm:gores_actor", "function", ECardColumn::Left, 3, "Gores actor", "gores actor function"},
			{"qm:gores", "function", ECardColumn::Left, 4, "Gores", "gores function"},
			{"qm:key_binds", "function", ECardColumn::Left, 5, "Key binds", "key binds function"},
			{"qm:mini_features", "function", ECardColumn::Left, 6, "Mini features", "mini features function"},
			{"qm:jump_hint", "function", ECardColumn::Left, 7, "Jump hint", "jump hint function"},
			{"qm:weapon_trajectory", "function", ECardColumn::Left, 8, "Weapon trajectory", "weapon trajectory function"},
			{"qm:coords", "hud", ECardColumn::Left, 9, "Coordinates", "coordinates coords hud"},
			{"qm:friend_notify", "function", ECardColumn::Left, 11, "Friend notify", "friend notify function"},
			{"qm:block_words", "function", ECardColumn::Left, 12, "Block words", "block words function"},
			{"qm:qiafen", "function", ECardColumn::Left, 13, "Keyword reply", "keyword reply qiafen function"}, // UI 名 keyword_reply，以持久化 key qiafen 为权威
			{"qm:translate", "function", ECardColumn::Left, 14, "Translate", "translate function"},
			{"qm:pie_menu", "function", ECardColumn::Left, 16, "Pie menu", "pie menu function"},
			{"qm:favorite_maps", "function", ECardColumn::Right, 6, "Favorite maps", "favorite maps function"},
			{"qm:hj_assist", "function", ECardColumn::Right, 7, "HJ assist", "hj assist function"},
			{"qm:player_stats", "hud", ECardColumn::Right, 4, "Player stats", "player stats hud"},
			{"qm:speedrun_timer", "hud", ECardColumn::Right, 8, "Speedrun timer", "speedrun timer hud"},
			{"qm:debug_graph", "hud", ECardColumn::Right, 9, "Debug graph", "debug graph hud"},
			{"qm:input_overlay", "hud", ECardColumn::Right, 10, "Input overlay", "input overlay hud"},
			{"qm:hud_notifications", "hud", ECardColumn::Right, 11, "HUD notifications", "hud notifications"},
			{"qm:voice", "hud", ECardColumn::Right, 12, "Voice", "voice hud"},
			{"qm:dummy_miniview", "hud", ECardColumn::Right, 13, "Dummy mini view", "dummy mini view hud"},
			{"qm:dynamic_island", "hud", ECardColumn::Right, 14, "Dynamic island", "dynamic island hud"},
			{"qm:system_media_controls", "hud", ECardColumn::Right, 15, "System media controls", "system media controls hud"},
			{"qm:lyrics", "hud", ECardColumn::Right, 16, "Lyrics", "lyrics hud"},
			{"qm:background_3d", "hud", ECardColumn::Right, 17, "3D background", "3d background hud"},
			{"qm:nameplate_text", "hud", ECardColumn::Right, 18, "Nameplate text", "nameplate text hud"}, // 数据债：原无 tab 归属，B1 补 hud
			{"qm:laser", "visual", ECardColumn::Right, 3, "Laser", "laser visual"}, // 数据债：原无 tab 归属，B1 补 visual

			// === Tclient section（19）· tclient:<name>（id 不变；column/order 按当前 section 顺序显式化）===
			{"tclient:visual-font-cursor", "tclient", ECardColumn::Left, 0, "Font cursor", "font cursor tclient visual"},
			{"tclient:visual-nameplates", "tclient", ECardColumn::Left, 1, "Nameplates", "nameplates tclient visual"},
			{"tclient:visual-effects", "tclient", ECardColumn::Left, 2, "Visual effects", "visual effects tclient"},
			{"tclient:input", "tclient", ECardColumn::Left, 3, "Input", "input tclient"},
			{"tclient:anti-latency-tools", "tclient", ECardColumn::Left, 4, "Anti latency tools", "anti latency tools tclient"},
			{"tclient:improved-anti-ping", "tclient", ECardColumn::Left, 5, "Improved anti ping", "improved anti ping tclient"},
			{"tclient:execute-on-join", "tclient", ECardColumn::Left, 6, "Execute on join", "execute on join tclient"},
			{"tclient:voting", "tclient", ECardColumn::Left, 7, "Voting", "voting tclient"},
			{"tclient:auto-reply", "tclient", ECardColumn::Left, 8, "Auto reply", "auto reply tclient"},
			{"tclient:player-indicator", "tclient", ECardColumn::Left, 9, "Player indicator", "player indicator tclient"},
			{"tclient:pet", "tclient", ECardColumn::Left, 10, "Pet", "pet tclient"},
			{"tclient:hud", "tclient", ECardColumn::Left, 11, "HUD", "hud tclient"},
			{"tclient:tee-status-bar", "tclient", ECardColumn::Left, 12, "Tee status bar", "tee status bar tclient"},
			{"tclient:tile-outlines", "tclient", ECardColumn::Left, 13, "Tile outlines", "tile outlines tclient"},
			{"tclient:ghost-tools", "tclient", ECardColumn::Left, 14, "Ghost tools", "ghost tools tclient"},
			{"tclient:rainbow", "tclient", ECardColumn::Left, 15, "Rainbow", "rainbow tclient"},
			{"tclient:tee-trails", "tclient", ECardColumn::Left, 16, "Tee trails", "tee trails tclient"},
			{"tclient:background-draw", "tclient", ECardColumn::Left, 17, "Background draw", "background draw tclient"},
			{"tclient:finish-name", "tclient", ECardColumn::Left, 18, "Finish name", "finish name tclient"},

			// === 设置 deck（24）· deck:<page>-<card>（原无持久化；tab=归属页/子页，column/order 按运行时卡片顺序显式化）===
			{"deck:graphics-display", "graphics", ECardColumn::Left, 0, "Graphics display", "graphics display"},
			{"deck:graphics-visual", "graphics", ECardColumn::Left, 1, "Visual", "graphics visual"},
			{"deck:graphics-backend", "graphics", ECardColumn::Left, 2, "Graphics backend", "graphics backend"},
			{"deck:graphics-modes", "graphics", ECardColumn::Left, 3, "Display modes", "display modes graphics"},
			{"deck:sound-toggle", "sound", ECardColumn::Left, 0, "Sound", "sound toggle audio"},
			{"deck:sound-volume", "sound", ECardColumn::Left, 1, "Volume", "volume sound audio"},
			{"deck:sound-audio-pack", "sound", ECardColumn::Left, 2, "Audio packs", "audio pack audio packs sound"},
			{"deck:ddnet-demo", "ddnet", ECardColumn::Left, 0, "Demo", "demo ddnet"},
			{"deck:ddnet-gameplay", "ddnet", ECardColumn::Left, 1, "Gameplay", "gameplay ddnet"},
			{"deck:ddnet-background", "ddnet", ECardColumn::Left, 2, "Background", "background ddnet"},
			{"deck:ddnet-miscellaneous", "ddnet", ECardColumn::Left, 3, "Miscellaneous", "miscellaneous ddnet"},
			{"deck:tclient-bind-wheel-editor", "tclient-bind-wheel", ECardColumn::Left, 0, "Bind Wheel", "bind wheel tclient"},
			{"deck:tclient-bind-wheel-preview", "tclient-bind-wheel", ECardColumn::Left, 1, "Preview", "preview bind wheel tclient"},
			{"deck:tclient-status-bar-settings", "tclient-status-bar", ECardColumn::Left, 0, "Status Bar", "status bar tclient"},
			{"deck:tclient-status-bar-items", "tclient-status-bar", ECardColumn::Left, 1, "Status Bar Codes", "status bar codes tclient"},
			{"deck:tclient-status-bar-preview", "tclient-status-bar", ECardColumn::Left, 2, "Preview", "preview status bar tclient"},
			{"deck:appearance-hud-main", "appearance-hud", ECardColumn::Left, 0, "HUD", "appearance hud main"},
			{"deck:appearance-hud-ddrace", "appearance-hud", ECardColumn::Right, 0, "DDRace HUD", "appearance ddrace hud"},
			{"deck:appearance-chat-settings", "appearance-chat", ECardColumn::Left, 0, "Chat", "appearance chat settings"},
			{"deck:appearance-chat-messages", "appearance-chat", ECardColumn::Right, 0, "Messages", "appearance chat messages"},
			{"deck:appearance-chat-preview", "appearance-chat", ECardColumn::Left, 1, "Preview", "appearance chat preview"},
			{"deck:appearance-hook-collision-main", "appearance-hook-collision", ECardColumn::Left, 0, "Hook collision line", "appearance hook collision line"},
			{"deck:appearance-hook-collision-preview", "appearance-hook-collision", ECardColumn::Right, 0, "Preview", "appearance hook collision preview"},
			{"deck:appearance-info-messages", "appearance-info-messages", ECardColumn::Left, 0, "Info Messages", "appearance info messages"},
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

	const char *MigrateLegacyKey(const char *pLegacyKey)
	{
		if(pLegacyKey == nullptr)
			return nullptr;
		// 从注册表派生：构造 stableId="qm:"+legacyKey 查表（DRY，不硬编码 key 列表）。
		// UI 名（如 keyword_reply）不在注册表，构造后查不到→nullptr，天然以持久化 key 为权威。
		char aStableId[128];
		str_format(aStableId, sizeof(aStableId), "qm:%s", pLegacyKey);
		const SCardDefault *D = FindByStableId(aStableId);
		return D != nullptr ? D->m_pStableId : nullptr;
	}

	std::vector<qm_card_order::SEntry> BuildDefaultEntries()
	{
		std::vector<qm_card_order::SEntry> vEntries;
		const std::vector<SCardDefault> &vDefaults = DefaultsTable();
		vEntries.reserve(vDefaults.size());
		for(const SCardDefault &Default : vDefaults)
		{
			int Column = 1;
			if(Default.m_DefaultColumn == ECardColumn::Full)
				Column = 0;
			else if(Default.m_DefaultColumn == ECardColumn::Right)
				Column = 2;
			vEntries.push_back({Default.m_pStableId, Default.m_pDefaultTab, Column, Default.m_DefaultOrder});
		}
		return vEntries;
	}
} // namespace qm_card_registry
