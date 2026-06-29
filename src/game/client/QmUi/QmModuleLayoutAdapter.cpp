#include "QmModuleLayoutAdapter.h"

#include <base/system.h>

namespace qm_module
{
	int QmModuleColumnToInt(EQmModuleColumn Column)
	{
		switch(Column)
		{
		case EQmModuleColumn::Full: return 0;
		case EQmModuleColumn::Left: return 1;
		case EQmModuleColumn::Right: return 2;
		}
		return 0;
	}

	EQmModuleColumn QmModuleColumnFromInt(int Column)
	{
		switch(Column)
		{
		case 1: return EQmModuleColumn::Left;
		case 2: return EQmModuleColumn::Right;
		default: return EQmModuleColumn::Full;
		}
	}

	const char *QmModuleColumnToString(EQmModuleColumn Column)
	{
		switch(Column)
		{
		case EQmModuleColumn::Full: return "full";
		case EQmModuleColumn::Left: return "left";
		case EQmModuleColumn::Right: return "right";
		}
		return "full";
	}

	bool ParseQmModuleColumnString(const char *pStr, EQmModuleColumn *pOut)
	{
		if(pStr == nullptr || pOut == nullptr)
			return false;
		if(str_comp(pStr, "full") == 0)
		{
			*pOut = EQmModuleColumn::Full;
			return true;
		}
		if(str_comp(pStr, "left") == 0)
		{
			*pOut = EQmModuleColumn::Left;
			return true;
		}
		if(str_comp(pStr, "right") == 0)
		{
			*pOut = EQmModuleColumn::Right;
			return true;
		}
		return false;
	}

	const char *QmModuleStableId(EQmModuleId Id)
	{
		switch(Id)
		{
		case EQmModuleId::Info: return "qm:info";
		case EQmModuleId::ChatBubble: return "qm:chat_bubble";
		case EQmModuleId::GoresActor: return "qm:gores_actor";
		case EQmModuleId::Gores: return "qm:gores";
		case EQmModuleId::FocusMode: return "qm:focus_mode";
		case EQmModuleId::KeyBinds: return "qm:key_binds";
		case EQmModuleId::MiniFeatures: return "qm:mini_features";
		case EQmModuleId::JumpHint: return "qm:jump_hint";
		case EQmModuleId::SkinTransition: return "qm:skin_transition";
		case EQmModuleId::CameraView: return "qm:camera_view";
		case EQmModuleId::DummyMiniView: return "qm:dummy_miniview";
		case EQmModuleId::Coords: return "qm:coords";
		case EQmModuleId::Streamer: return "qm:streamer";
		case EQmModuleId::FriendNotify: return "qm:friend_notify";
		case EQmModuleId::BlockWords: return "qm:block_words";
		case EQmModuleId::Translate: return "qm:translate";
		case EQmModuleId::TranslateUi: return "qm:translate_ui";
		case EQmModuleId::QiaFen: return "qm:qiafen"; // 持久化 key，非 UI 名 keyword_reply
		case EQmModuleId::PieMenu: return "qm:pie_menu";
		case EQmModuleId::EntityOverlay: return "qm:entity_overlay";
		case EQmModuleId::Laser: return "qm:laser";
		case EQmModuleId::PlayerStats: return "qm:player_stats";
		case EQmModuleId::CollisionHitbox: return "qm:collision_hitbox";
		case EQmModuleId::FavoriteMaps: return "qm:favorite_maps";
		case EQmModuleId::HJAssist: return "qm:hj_assist";
		case EQmModuleId::SpeedrunTimer: return "qm:speedrun_timer";
		case EQmModuleId::DebugGraph: return "qm:debug_graph";
		case EQmModuleId::InputOverlay: return "qm:input_overlay";
		case EQmModuleId::HudNotifications: return "qm:hud_notifications";
		case EQmModuleId::Voice: return "qm:voice";
		case EQmModuleId::DynamicIsland: return "qm:dynamic_island";
		case EQmModuleId::SystemMediaControls: return "qm:system_media_controls";
		case EQmModuleId::Lyrics: return "qm:lyrics";
		case EQmModuleId::Background3D: return "qm:background_3d";
		case EQmModuleId::WeaponTrajectory: return "qm:weapon_trajectory";
		case EQmModuleId::WeaponAnimation: return "qm:weapon_animation";
		case EQmModuleId::CardAppearance: return "qm:card_appearance";
		}
		return nullptr;
	}

	bool QmModuleIdFromStableId(const char *pStableId, EQmModuleId *pOut)
	{
		if(pStableId == nullptr || pOut == nullptr)
			return false;
		for(size_t i = 0; i < QmModuleCount; ++i)
		{
			EQmModuleId Id = static_cast<EQmModuleId>(i);
			const char *pStable = QmModuleStableId(Id);
			if(pStable != nullptr && str_comp(pStable, pStableId) == 0)
			{
				*pOut = Id;
				return true;
			}
		}
		return false;
	}
} // namespace qm_module
