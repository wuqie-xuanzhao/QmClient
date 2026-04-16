// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Tcme, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Tcme, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Tcme, ScriptName, Len, Def, Save, Desc) ;
#endif

#if defined(CONF_FAMILY_WINDOWS)
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "允许缩放时是否允许游戏中的任何分辨率（Windows 上有错误）")
#else
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "允许缩放时是否允许游戏中的任何分辨率（Windows 上有错误）")
#endif

MACRO_CONFIG_INT(TcShowChatClient, tc_show_chat_client, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示来自客户端的聊天消息，例如 echo")

MACRO_CONFIG_INT(TcShowFrozenText, tc_frozen_tees_text, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示你队伍中当前有多少tee被冻结。 （0 - 关闭，1 - 显示活动，2 - 显示冻结）")
MACRO_CONFIG_INT(TcShowFrozenHud, tc_frozen_tees_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示冻结的 tee HUD")
MACRO_CONFIG_INT(TcShowFrozenHudSkins, tc_frozen_tees_hud_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使用忍者皮肤或深色皮肤制作 HUD 上的冰冻 tee")

MACRO_CONFIG_INT(TcFrozenHudTeeSize, tc_frozen_tees_size, 15, 8, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冷冻tee中tee的尺寸。 （默认值：15）")
MACRO_CONFIG_INT(TcFrozenMaxRows, tc_frozen_tees_max_rows, 1, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冻结 tee HUD 显示中的最大行数")
MACRO_CONFIG_INT(TcFrozenHudTeamOnly, tc_frozen_tees_only_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "仅在队伍中渲染冻结 tee HUD 显示")

MACRO_CONFIG_INT(TcNameplatePingCircle, tc_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在名字板上显示一个圆圈来指示 ping")
MACRO_CONFIG_INT(TcNameplateCountry, tc_nameplate_country, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "名字板上显示国旗")
MACRO_CONFIG_INT(TcNameplateSkins, tc_nameplate_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在名字板中显示皮肤名称，有助于查找丢失的皮肤")

MACRO_CONFIG_INT(TcFakeCtfFlags, tc_fake_ctf_flags, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在玩家身上显示假 CTF 旗帜（0 = 关闭、1 = 红色、2 = 蓝色）")

MACRO_CONFIG_INT(TcLimitMouseToScreen, tc_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将鼠标限制在屏幕边界")
MACRO_CONFIG_INT(TcScaleMouseDistance, tc_scale_mouse_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "通过将最大距离缩放至 1000 来提高鼠标精度")

MACRO_CONFIG_INT(TcHammerRotatesWithCursor, tc_hammer_rotates_with_cursor, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "让你的锤子像其他武器一样旋转")

MACRO_CONFIG_INT(TcMiniVoteHud, tc_mini_vote_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用后，投票 UI 会变小")

// Anti Latency Tools
MACRO_CONFIG_INT(TcRemoveAnti, tc_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "减少冻结时的 antiping 与玩家预测")
MACRO_CONFIG_INT(TcUnfreezeLagTicks, tc_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "可移除的最大预测 tick 数")
MACRO_CONFIG_INT(TcUnfreezeLagDelayTicks, tc_remove_anti_delay_ticks, 25, 5, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冻结后需要多少个tick才能删除最大预测")

MACRO_CONFIG_INT(TcUnpredOthersInFreeze, tc_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "如果你被冻结，请不要预测其他玩家")
MACRO_CONFIG_INT(TcPredMarginInFreeze, tc_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冻结时启用更改预测裕度")
MACRO_CONFIG_INT(TcPredMarginInFreezeAmount, tc_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "设置冻结时的预测裕度应为多少")

MACRO_CONFIG_INT(TcShowOthersGhosts, tc_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在其他玩家无法预测的位置上显示鬼魂")
MACRO_CONFIG_INT(TcSwapGhosts, tc_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将预测的玩家显示为幽灵，将普通玩家显示为不可预测的玩家")
MACRO_CONFIG_INT(TcHideFrozenGhosts, tc_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "如果其他玩家的幽灵被冻结，则隐藏它们")

MACRO_CONFIG_INT(TcPredGhostsAlpha, tc_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "预测鬼魂的 Alpha (0-100)")
MACRO_CONFIG_INT(TcUnpredGhostsAlpha, tc_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "不可预测的幽灵的透明度（0-100）")
MACRO_CONFIG_INT(TcRenderGhostAsCircle, tc_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将幽灵渲染为圆形而不是 tee")

MACRO_CONFIG_INT(TcShowCenter, tc_show_center, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "绘制线条以显示屏幕/点击框的中心")
MACRO_CONFIG_INT(TcShowCenterWidth, tc_show_center_width, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "中心线宽度（由 tc_show_center 启用）")
MACRO_CONFIG_COL(TcShowCenterColor, tc_show_center_color, 1694498688, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "中心线颜色（由 tc_show_center 启用）") // transparent red

MACRO_CONFIG_INT(TcHookCollCursor, tc_hook_coll_cursor, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "钩子碰撞线长度跟随光标距离")

MACRO_CONFIG_INT(TcFastInput, tc_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在下一个 tick 前使用输入进行预测")
MACRO_CONFIG_INT(TcFastInputMode, tc_fast_input_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "快速输入模式 (0 = TClient, 1 = QmClient)")
MACRO_CONFIG_INT(TcFastInputAmount, tc_fast_input_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将应用多少毫秒的快速输入")
MACRO_CONFIG_INT(TcFastInputOthers, tc_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将快速输入应用到其他 tee")

MACRO_CONFIG_INT(TcAntiPingImproved, tc_antiping_improved, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "不同的 antiping 平滑算法，与 cl_antiping_smooth 不兼容")
MACRO_CONFIG_INT(TcAntiPingNegativeBuffer, tc_antiping_negative_buffer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "对 Gores 有帮助。允许内部确定性为负值，使预测更保守")
MACRO_CONFIG_INT(TcAntiPingStableDirection, tc_antiping_stable_direction, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "沿tee稳定轴乐观预测，以减少获得整体稳定性的延迟")
MACRO_CONFIG_INT(TcAntiPingUncertaintyScale, tc_antiping_uncertainty_scale, 150, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "按 ping 比例确定不确定性时长，100 = 1.0")

MACRO_CONFIG_INT(TcColorFreeze, tc_color_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在冷冻 tee上使用肤色")
MACRO_CONFIG_INT(TcColorFreezeDarken, tc_color_freeze_darken, 90, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冻结时使 tee颜色变深 (0-100)")
MACRO_CONFIG_INT(TcColorFreezeFeet, tc_color_freeze_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "还可以为冷冻 tee脚使用颜色")

// Revert Variables
MACRO_CONFIG_INT(TcSmoothPredictionMargin, tc_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使预测余量过渡平滑，导致更差的 ping 抖动调整（恢复 DDNet 更改）")
MACRO_CONFIG_INT(TcFreezeKatana, tc_frozen_katana, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在冻结的玩家上显示武士刀（恢复 DDNet 更改）")
MACRO_CONFIG_INT(TcOldTeamColors, tc_old_team_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使用彩虹队伍颜色（恢复 DDNet 更改）")
MACRO_CONFIG_INT(TcRevertHookLine, tc_revert_hook_line, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "恢复旧版单段钩子碰撞线行为")

// Water Fall (Death) Auto Emoticon and Chat
MACRO_CONFIG_INT(TcWaterFallEnabled, tc_waterfall_enabled, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "落水/死亡时自动发送爱心表情并聊天")
MACRO_CONFIG_INT(TcWaterFallEmoticon, tc_waterfall_emoticon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "落水/死亡时发送爱心表情")
MACRO_CONFIG_STR(TcWaterFallMessage, tc_waterfall_message, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "落水/死亡时发送的聊天消息（空=无消息）")

// Freeze Auto Emoticon and Chat
MACRO_CONFIG_INT(TcFreezeChatEnabled, tc_freeze_chat_enabled, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "进入冻结时自动发送表情并聊天")
MACRO_CONFIG_INT(TcFreezeChatEmoticon, tc_freeze_chat_emoticon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "进入冻结状态发送表情")
MACRO_CONFIG_INT(TcFreezeChatEmoticonId, tc_freeze_chat_emoticon_id, 7, 0, 15, CFGFLAG_CLIENT | CFGFLAG_SAVE, "进入冻结状态时发送的表情符号 ID (0-15)")
MACRO_CONFIG_STR(TcFreezeChatMessage, tc_freeze_chat_message, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "进入冻结状态时发送的聊天消息，以逗号分隔（空=无消息）")
MACRO_CONFIG_INT(TcFreezeChatChance, tc_freeze_chat_chance, 30, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "有机会发送冻结聊天消息（0-100%）")

// Outline Variables
MACRO_CONFIG_INT(TcOutline, tc_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "绘制轮廓")
MACRO_CONFIG_INT(TcOutlineEntities, tc_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "仅显示实体中的轮廓")
MACRO_CONFIG_INT(TcOutlineAlpha, tc_outline_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "全局轮廓不透明度")
MACRO_CONFIG_INT(TcOutlineSolidAlpha, tc_outline_solid_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "实体墙轮廓的不透明度")

MACRO_CONFIG_INT(TcOutlineSolid, tc_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在挂钩和脱钩周围绘制轮廓")
MACRO_CONFIG_INT(TcOutlineFreeze, tc_outline_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在冻结和深度周围绘制轮廓")
MACRO_CONFIG_INT(TcOutlineUnfreeze, tc_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在解冻和加深周围绘制轮廓")
MACRO_CONFIG_INT(TcOutlineKill, tc_outline_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在杀戮周围绘制轮廓")
MACRO_CONFIG_INT(TcOutlineTele, tc_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "绘制传送器周围的轮廓")

MACRO_CONFIG_INT(TcOutlineWidthSolid, tc_outline_width_solid, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "钩子和脱钩周围轮廓的宽度")
MACRO_CONFIG_INT(TcOutlineWidthFreeze, tc_outline_width_freeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "冻结和深度周围轮廓的宽度")
MACRO_CONFIG_INT(TcOutlineWidthUnfreeze, tc_outline_width_unfreeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "解冻和加深周围轮廓的宽度")
MACRO_CONFIG_INT(TcOutlineWidthKill, tc_outline_width_kill, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "杀戮周围轮廓的宽度")
MACRO_CONFIG_INT(TcOutlineWidthTele, tc_outline_width_tele, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "传送器周围轮廓的宽度")

MACRO_CONFIG_COL(TcOutlineColorSolid, tc_outline_color_solid, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "钩子和脱钩周围轮廓的颜色") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorFreeze, tc_outline_color_freeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "冻结图块周围轮廓的颜色") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorDeepFreeze, tc_outline_color_deep_freeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "深度冷冻周围轮廓的颜色") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorUnfreeze, tc_outline_color_unfreeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "解冻瓷砖周围轮廓的颜色") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorDeepUnfreeze, tc_outline_color_deep_unfreeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "深度解冻周围轮廓的颜色") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorKill, tc_outline_color_kill, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "杀戮周围轮廓的颜色") // 0 0 0
MACRO_CONFIG_COL(TcOutlineColorTele, tc_outline_color_tele, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "传送器周围轮廓的颜色") // 255 0 0 0

// Indicator Variables
MACRO_CONFIG_COL(TcIndicatorAlive, tc_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "玩家指示器中活着的tee的颜色")
MACRO_CONFIG_COL(TcIndicatorFreeze, tc_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "玩家指示器中冻结tee的颜色")
MACRO_CONFIG_COL(TcIndicatorSaved, tc_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "保存在玩家指示器中的tee颜色")
MACRO_CONFIG_INT(TcIndicatorOffset, tc_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) 指示器位置偏移")
MACRO_CONFIG_INT(TcIndicatorOffsetMax, tc_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) 可变偏移设置的最大指示器偏移")
MACRO_CONFIG_INT(TcIndicatorVariableDistance, tc_indicator_variable_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "tee距离越远，指示圈就越远")
MACRO_CONFIG_INT(TcIndicatorMaxDistance, tc_indicator_variable_max_distance, 1000, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "可变偏移量的最大发球距离")
MACRO_CONFIG_INT(TcIndicatorRadius, tc_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) 指示圆尺寸")
MACRO_CONFIG_INT(TcIndicatorOpacity, tc_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "指示圈的不透明度")
MACRO_CONFIG_INT(TcPlayerIndicator, tc_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示其他 tee 的径向指示器")
MACRO_CONFIG_INT(TcPlayerIndicatorFreeze, tc_player_indicator_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "仅在指示器中显示冻结的 tee")
MACRO_CONFIG_INT(TcIndicatorTeamOnly, tc_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "仅在队伍中时显示指示器")
MACRO_CONFIG_INT(TcIndicatorTees, tc_indicator_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示 tee而不是圆圈")
MACRO_CONFIG_INT(TcIndicatorHideVisible, tc_indicator_hide_visible_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "不显示屏幕内的 tee")

// Bind Wheel
MACRO_CONFIG_INT(TcResetBindWheelMouse, tc_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "打开bindwheel时重置鼠标位置")

// Regex chat matching
MACRO_CONFIG_STR(TcRegexChatIgnore, tc_regex_chat_ignore, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "根据正则表达式过滤掉聊天消息。")

// Misc visual
MACRO_CONFIG_INT(TcWhiteFeet, tc_white_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "将所有脚渲染为完美的白色基色")
MACRO_CONFIG_STR(TcWhiteFeetSkin, tc_white_feet_skin, 255, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "白脚底皮")
MACRO_CONFIG_INT(TcMovingTilesEntities, tc_moving_tiles_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在实体层显示移动图块")

MACRO_CONFIG_INT(TcMiniDebug, tc_mini_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示位置和角度")

MACRO_CONFIG_INT(TcShowhudDummyPosition, tc_showhud_dummy_position, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在运动信息HUD中显示分身位置")
MACRO_CONFIG_INT(TcShowhudDummySpeed, tc_showhud_dummy_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在运动信息HUD中显示虚拟速度")
MACRO_CONFIG_INT(TcShowhudDummyAngle, tc_showhud_dummy_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在运动信息 HUD 中显示虚拟角度")
MACRO_CONFIG_INT(TcShowLocalTimeSeconds, tc_show_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在本地时间显示中显示秒")

MACRO_CONFIG_INT(TcNotifyWhenLast, tc_last_notify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "当你最后到达时通知你")
MACRO_CONFIG_STR(TcNotifyWhenLastText, tc_last_notify_text, 64, "Last!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "最后通知的文本")
MACRO_CONFIG_COL(TcNotifyWhenLastColor, tc_last_notify_color, 256, CFGFLAG_CLIENT | CFGFLAG_SAVE, "最后通知的颜色")
MACRO_CONFIG_INT(TcNotifyWhenLastX, tc_last_notify_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "最后通知的水平位置（以屏幕宽度的百分比表示）")
MACRO_CONFIG_INT(TcNotifyWhenLastY, tc_last_notify_y, 1, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "最后通知的垂直位置（占屏幕高度的百分比）")
MACRO_CONFIG_INT(TcNotifyWhenLastSize, tc_last_notify_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "最后通知的字体大小")
MACRO_CONFIG_INT(TcJumpHint, tc_jump_hint, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "根据位置小数显示跳转提示")
MACRO_CONFIG_COL(TcJumpHintColor, tc_jump_hint_color, 256, CFGFLAG_CLIENT | CFGFLAG_SAVE, "跳转提示的颜色")
MACRO_CONFIG_INT(TcJumpHintX, tc_jump_hint_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "跳转提示的水平位置占屏幕宽度的百分比")
MACRO_CONFIG_INT(TcJumpHintY, tc_jump_hint_y, 5, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "跳转提示的垂直位置占屏幕高度的百分比")
MACRO_CONFIG_INT(TcJumpHintSize, tc_jump_hint_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "跳转提示的字体大小")

MACRO_CONFIG_INT(TcRenderCursorSpec, tc_cursor_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "自由视角旁观时渲染准星")
MACRO_CONFIG_INT(TcRenderCursorSpecAlpha, tc_cursor_in_spec_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "freeview 中光标的 Alpha")

// MACRO_CONFIG_INT(TcRenderNameplateSpec, tc_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "旁观时渲染名字板")

MACRO_CONFIG_INT(TcTinyTees, tc_tiny_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使 tee变小")
MACRO_CONFIG_INT(TcTinyTeeSize, tc_indicator_tees_size, 100, 85, 115, CFGFLAG_CLIENT | CFGFLAG_SAVE, "定义小 tee的尺寸")
MACRO_CONFIG_INT(TcTinyTeesOthers, tc_tiny_tees_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使其他 tee变小")

MACRO_CONFIG_INT(TcCursorScale, tc_cursor_scale, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "游戏中光标缩放的百分比（50 = 一半，200 = 双倍速度）")

// Profiles
MACRO_CONFIG_INT(TcProfileSkin, tc_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在配置文件中应用皮肤")
MACRO_CONFIG_INT(TcProfileName, tc_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在个人资料中应用姓名")
MACRO_CONFIG_INT(TcProfileClan, tc_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在个人资料中应用部落")
MACRO_CONFIG_INT(TcProfileFlag, tc_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在配置文件中应用标志")
MACRO_CONFIG_INT(TcProfileColors, tc_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在配置文件中应用颜色")
MACRO_CONFIG_INT(TcProfileEmote, tc_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在个人资料中应用表情")
MACRO_CONFIG_INT(TcProfileOverwriteClanWithEmpty, tc_profile_overwrite_clan_with_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "即使个人资料中的部落名称为空，也会覆盖部落名称")

// Rainbow
MACRO_CONFIG_INT(TcRainbowTees, tc_rainbow_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用 Rainbow 客户端")
MACRO_CONFIG_INT(TcRainbowHook, tc_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "彩虹钩")
MACRO_CONFIG_INT(TcRainbowWeapon, tc_rainbow_weapon, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "彩虹武器")

MACRO_CONFIG_INT(TcRainbowOthers, tc_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "为其他玩家启用 Rainbow 客户端")
MACRO_CONFIG_INT(TcRainbowMode, tc_rainbow_mode, 1, 1, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "彩虹模式（1：彩虹，2：脉冲，3：黑暗，4：随机）")
MACRO_CONFIG_INT(TcRainbowSpeed, tc_rainbow_speed, 100, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow 速度百分比（50 = 半速，200 = 双倍速）")

// War List
MACRO_CONFIG_INT(TcWarList, tc_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "切换敌对列表视觉效果")
MACRO_CONFIG_INT(TcWarListShowClan, tc_warlist_show_clan_if_war, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "如果发生敌对，在名字板上显示战队")
MACRO_CONFIG_INT(TcWarListReason, tc_warlist_reason, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示敌对原因")
MACRO_CONFIG_INT(TcWarListChat, tc_warlist_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在聊天中显示敌对色彩")
MACRO_CONFIG_INT(TcWarListScoreboard, tc_warlist_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在记分牌上显示敌对颜色")
MACRO_CONFIG_INT(TcWarListAllowDuplicates, tc_warlist_allow_duplicates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "允许重复的敌对条目")
MACRO_CONFIG_INT(TcWarListSpectate, tc_warlist_spectate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在观众菜单中显示敌对色彩")

MACRO_CONFIG_INT(TcWarListIndicator, tc_warlist_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使用warlist作为指标")
MACRO_CONFIG_INT(TcWarListIndicatorColors, tc_warlist_indicator_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示敌对颜色而不是冻结颜色")
MACRO_CONFIG_INT(TcWarListIndicatorAll, tc_warlist_indicator_all, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示所有组")
MACRO_CONFIG_INT(TcWarListIndicatorEnemy, tc_warlist_indicator_enemy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示第一组的玩家")
MACRO_CONFIG_INT(TcWarListIndicatorTeam, tc_warlist_indicator_team, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示第二组的玩家")

// Status Bar
MACRO_CONFIG_INT(TcStatusBar, tc_statusbar, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用状态栏")

MACRO_CONFIG_INT(TcStatusBar12HourClock, tc_statusbar_12_hour_clock, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "本地时间使用 12 小时制")
MACRO_CONFIG_INT(TcStatusBarLocalTimeSeconds, tc_statusbar_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示本地时间秒数")
MACRO_CONFIG_INT(TcStatusBarHeight, tc_statusbar_height, 8, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏高度")

MACRO_CONFIG_COL(TcStatusBarColor, tc_statusbar_color, 3221225472, CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏背景颜色")
MACRO_CONFIG_COL(TcStatusBarTextColor, tc_statusbar_text_color, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏文字颜色")
MACRO_CONFIG_INT(TcStatusBarAlpha, tc_statusbar_alpha, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏背景 Alpha")
MACRO_CONFIG_INT(TcStatusBarTextAlpha, tc_statusbar_text_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏文本 Alpha")

MACRO_CONFIG_INT(TcStatusBarLabels, tc_statusbar_labels, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在状态栏条目上显示标签")
MACRO_CONFIG_STR(TcStatusBarScheme, tc_statusbar_scheme, 128, "ac pf r", CFGFLAG_CLIENT | CFGFLAG_SAVE, "状态栏项目的显示顺序")

// Trails
MACRO_CONFIG_INT(TcTeeTrail, tc_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用 tee路径")
MACRO_CONFIG_INT(TcTeeTrailOthers, tc_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "为其他玩家显示开球路线")
MACRO_CONFIG_INT(TcTeeTrailWidth, tc_tee_trail_width, 15, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "发球道宽度")
MACRO_CONFIG_INT(TcTeeTrailLength, tc_tee_trail_length, 25, 5, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "tee轨迹长度")
MACRO_CONFIG_INT(TcTeeTrailAlpha, tc_tee_trail_alpha, 80, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "发球道透明度")
MACRO_CONFIG_COL(TcTeeTrailColor, tc_tee_trail_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "tee轨迹颜色")
MACRO_CONFIG_INT(TcTeeTrailTaper, tc_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "锥形 tee轨迹长度")
MACRO_CONFIG_INT(TcTeeTrailFade, tc_tee_trail_fade, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "随着长度的推移逐渐淡出轨迹 alpha")
MACRO_CONFIG_INT(TcTeeTrailColorMode, tc_tee_trail_color_mode, 1, 1, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee Trail颜色模式（1：纯色，2：当前Tee颜色，3：彩虹，4：基于Tee速度的颜色，5：随机）")

// Chat Reply
MACRO_CONFIG_INT(TcAutoReplyMuted, tc_auto_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "自动回复静音玩家消息")
MACRO_CONFIG_STR(TcAutoReplyMutedMessage, tc_auto_reply_muted_message, 128, "I have muted you", CFGFLAG_CLIENT | CFGFLAG_SAVE, "回复静音玩家的消息")
MACRO_CONFIG_INT(TcAutoReplyMinimized, tc_auto_reply_minimized, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "游戏最小化时自动回复")
MACRO_CONFIG_STR(TcAutoReplyMinimizedMessage, tc_auto_reply_minimized_message, 128, "I am not tabbed in", CFGFLAG_CLIENT | CFGFLAG_SAVE, "游戏最小化时回复的消息")

// Voting
MACRO_CONFIG_INT(TcAutoVoteWhenFar, tc_auto_vote_when_far, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "如果你距离地图较远，则自动投反对票")
MACRO_CONFIG_STR(TcAutoVoteWhenFarMessage, tc_auto_vote_when_far_message, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "自动远投票发生时发送的消息，留空以禁用")
MACRO_CONFIG_INT(TcAutoVoteWhenFarTime, tc_auto_vote_when_far_time, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "自动投票还有多久才会发生")

// Font
MACRO_CONFIG_STR(TcCustomFont, tc_custom_font, 255, "DejaVu Sans", CFGFLAG_CLIENT | CFGFLAG_SAVE, "自定义字体")

// Bg Draw
MACRO_CONFIG_INT(TcBgDrawWidth, tc_bg_draw_width, 5, 1, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "背景描边的宽度")
MACRO_CONFIG_INT(TcBgDrawFadeTime, tc_bg_draw_fade_time, 0, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "中风消失之前的时间（0 = 从不）")
MACRO_CONFIG_INT(TcBgDrawMaxItems, tc_bg_draw_max_items, 128, 0, 2048, CFGFLAG_CLIENT | CFGFLAG_SAVE, "最大行程数")
MACRO_CONFIG_COL(TcBgDrawColor, tc_bg_draw_color, 14024576, CFGFLAG_CLIENT | CFGFLAG_SAVE, "背景绘制描边的颜色")
MACRO_CONFIG_INT(TcBgDrawAutoSaveLoad, tc_bg_draw_auto_save_load, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "自动保存和加载背景图")

// Translate
MACRO_CONFIG_STR(TcTranslateBackend, tc_translate_backend, 32, "tencentcloud", CFGFLAG_CLIENT | CFGFLAG_SAVE, "翻译后端（ftapi、libretranslate、tencentcloud）")
MACRO_CONFIG_STR(TcTranslateTarget, tc_translate_target, 16, "zh", CFGFLAG_CLIENT | CFGFLAG_SAVE, "翻译目标语言代码（如 zh、en、ja、zh-TW）")
MACRO_CONFIG_STR(TcTranslateEndpoint, tc_translate_endpoint, 256, "https://tmt.tencentcloudapi.com/", CFGFLAG_CLIENT | CFGFLAG_SAVE, "对于需要它的后端，要使用的端点（必须是 https）")
MACRO_CONFIG_STR(TcTranslateKey, tc_translate_key, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "对于需要它的后端，要使用 api key")
MACRO_CONFIG_STR(TcTranslateRegion, tc_translate_region, 32, "ap-guangzhou", CFGFLAG_CLIENT | CFGFLAG_SAVE, "腾讯云翻译地域（如 ap-guangzhou）")
MACRO_CONFIG_INT(TcTranslateAuto, tc_translate_auto, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "自动翻译消息，只有部分后端支持（FTApi 不支持）")

// Animations
MACRO_CONFIG_INT(TcAnimateWheelTime, tc_animate_wheel_time, 350, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "表情和绑定轮动画的持续时间，以毫秒为单位（0 == 无动画，1000 = 1 秒）")

// Pets
MACRO_CONFIG_INT(TcPetShow, tc_pet_show, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示宠物")
MACRO_CONFIG_STR(TcPetSkin, tc_pet_skin, 24, "twinbop", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "宠物皮肤")
MACRO_CONFIG_INT(TcPetSize, tc_pet_size, 60, 10, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "宠物的体型占普通玩家的比例")
MACRO_CONFIG_INT(TcPetAlpha, tc_pet_alpha, 90, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "宠物的 Alpha（100 = 完全不透明，50 = 半透明）")

// Change name near finish
MACRO_CONFIG_INT(TcChangeNameNearFinish, tc_change_name_near_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "接近完成时尝试更改你的名字")
MACRO_CONFIG_STR(TcFinishName, tc_finish_name, 16, "nameless tee", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "当 tc_change_name_near_finish 为 1 时，接近完成时要更改的名称")

// Flags
MACRO_CONFIG_INT(TcTClientSettingsTabs, tc_tclient_settings_tabs, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "用于禁用设置选项卡的位标志")

// Volleyball
MACRO_CONFIG_INT(TcVolleyBallBetterBall, tc_volleyball_better_ball, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "使排球中冻结的玩家看起来更像排球（0 = 禁用，1 = 在排球地图中，2 = 始终）")
MACRO_CONFIG_STR(TcVolleyBallBetterBallSkin, tc_volleyball_better_ball_skin, 24, "beachball", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "玩家皮肤用于更好的排球")

// Mod
MACRO_CONFIG_INT(TcShowPlayerHitBoxes, tc_show_player_hit_boxes, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示玩家命中框（1 = 预测，2 = 预测和未预测）")

// Legacy Chat Bubble Settings (tc_ prefix)
MACRO_CONFIG_INT(TcHideChatBubblesLegacy, tc_hide_chat_bubbles, 0, 0, 1, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_INT(TcChatBubbleLegacy, tc_chat_bubble, 1, 0, 1, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_INT(TcChatBubbleDurationLegacy, tc_chat_bubble_duration, 10, 1, 30, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_INT(TcChatBubbleAlphaLegacy, tc_chat_bubble_alpha, 80, 0, 100, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_INT(TcChatBubbleFontSizeLegacy, tc_chat_bubble_font_size, 20, 8, 32, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_COL(TcChatBubbleBgColorLegacy, tc_chat_bubble_bg_color, 404232960, CFGFLAG_CLIENT | CFGFLAG_COLALPHA, "旧版聊天气泡设置")
MACRO_CONFIG_COL(TcChatBubbleTextColorLegacy, tc_chat_bubble_text_color, 4294967295, CFGFLAG_CLIENT, "旧版聊天气泡设置")
MACRO_CONFIG_INT(TcChatBubbleAnimationLegacy, tc_chat_bubble_animation, 0, 0, 2, CFGFLAG_CLIENT, "旧版聊天气泡设置")

MACRO_CONFIG_INT(TcModWeapon, tc_mod_weapon, 0, 0, 1, CFGFLAG_CLIENT, "当你指向某人并射击时运行命令（默认杀死），仅在远程控制台中验证时有效")
MACRO_CONFIG_STR(TcModWeaponCommand, tc_mod_weapon_command, 256, "rcon kill_pl", CFGFLAG_CLIENT | CFGFLAG_SAVE, "使用 tc_mod_weapon 运行的命令，id 附加到命令末尾")

// Run on join
MACRO_CONFIG_STR(TcExecuteOnConnect, tc_execute_on_connect, 100, "Run a console command before connect", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(TcExecuteOnJoin, tc_execute_on_join, 100, "Run a console command on join", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(TcExecuteOnJoinDelay, tc_execute_on_join_delay, 2, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "勾选执行 tc_execute_on_join 之前的延迟")

// Custom Communities
MACRO_CONFIG_STR(TcCustomCommunitiesUrl, tc_custom_communities_url, 256, "https://raw.githubusercontent.com/SollyBunny/ddnet-custom-communities/refs/heads/main/custom-communities-ddnet-info.json", CFGFLAG_CLIENT | CFGFLAG_SAVE, "用于从中获取自定义社区的 URL（必须是 https），为空则禁用")

// Discord RPC
MACRO_CONFIG_INT(TcDiscordRPC, tc_discord_rpc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "切换discord RPC（需要重新启动）") // broken

// Sidebar
MACRO_CONFIG_INT(TcSidebarEnable, tc_sidebar_enable, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "启用侧边栏")
MACRO_CONFIG_INT(TcSidebarWidth, tc_sidebar_width, 200, 100, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "侧边栏的宽度（以像素为单位）")
MACRO_CONFIG_INT(TcSidebarOpacity, tc_sidebar_opacity, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "侧边栏背景不透明度（0-100）")
MACRO_CONFIG_INT(TcSidebarPosition, tc_sidebar_position, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "侧边栏位置（0=左，1=右，2=自定义）")
MACRO_CONFIG_INT(TcSidebarShowInGame, tc_sidebar_show_in_game, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "游戏中显示侧边栏")
MACRO_CONFIG_INT(TcSidebarShowInMenu, tc_sidebar_show_in_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在菜单中显示侧边栏")
MACRO_CONFIG_INT(TcSidebarShowInSpec, tc_sidebar_show_in_spec, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在旁观者模式下显示侧边栏")

MACRO_CONFIG_INT(TcSidebarShowFPS, tc_sidebar_show_fps, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示 FPS")
MACRO_CONFIG_INT(TcSidebarShowPing, tc_sidebar_show_ping, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示 ping")
MACRO_CONFIG_INT(TcSidebarShowSpeed, tc_sidebar_show_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示玩家速度")
MACRO_CONFIG_INT(TcSidebarShowPosition, tc_sidebar_show_position, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示玩家位置")
MACRO_CONFIG_INT(TcSidebarShowTime, tc_sidebar_show_time, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示本地时间")
MACRO_CONFIG_INT(TcSidebarShowServerInfo, tc_sidebar_show_server_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "在侧边栏中显示服务器信息")

// UI Settings
MACRO_CONFIG_INT(TcUiShowTClient, tc_ui_show_tclient, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示 TClient 配置变量")
MACRO_CONFIG_INT(TcUiShowQimeng, tc_ui_show_qimeng, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示栖梦配置变量")
MACRO_CONFIG_INT(TcUiShowDDNet, tc_ui_show_ddnet, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "显示 DDNet 配置变量")
MACRO_CONFIG_INT(TcUiCompactList, tc_ui_compact_list, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "对配置变量使用紧凑列表视图")
MACRO_CONFIG_INT(TcUiOnlyModified, tc_ui_only_modified, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "只显示修改的配置变量")

// Config Tags Filter Settings (default 0 = not filtering, 1 = show only this tag)
MACRO_CONFIG_INT(TcUiTagVisual, tc_ui_tag_visual, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示视觉效果类")
MACRO_CONFIG_INT(TcUiTagHud, tc_ui_tag_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示界面显示类")
MACRO_CONFIG_INT(TcUiTagInput, tc_ui_tag_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示输入优化类")
MACRO_CONFIG_INT(TcUiTagChat, tc_ui_tag_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示聊天相关类")
MACRO_CONFIG_INT(TcUiTagAudio, tc_ui_tag_audio, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示语音音效类")
MACRO_CONFIG_INT(TcUiTagAutomation, tc_ui_tag_automation, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示自动化功能类")
MACRO_CONFIG_INT(TcUiTagSocial, tc_ui_tag_social, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示社交功能类")
MACRO_CONFIG_INT(TcUiTagCamera, tc_ui_tag_camera, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示相机视野类")
MACRO_CONFIG_INT(TcUiTagGameplay, tc_ui_tag_gameplay, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示游戏玩法类")
MACRO_CONFIG_INT(TcUiTagMisc, tc_ui_tag_misc, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Configs筛选: 显示其他杂项")
