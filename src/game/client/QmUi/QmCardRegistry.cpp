#include "QmCardRegistry.h"

#include <base/system.h>

#include <engine/shared/localization.h>

#include <game/localization.h>

#include <utility>

namespace qm_card_registry
{
	static const std::vector<SCardDefault> &DefaultsTable()
	{
		// clang-format off
		static const std::vector<SCardDefault> s_aDefaults = {
			// === 栖梦侧栏模块（38）· qm:<key>（显式默认值齐全，来源 s_aQmModuleDefaults）===
			{"qm:info", "visual", ECardColumn::Full, 0, "QmClient", "qmclient info"},
			{"qm:chat_bubble", "visual", ECardColumn::Left, 0, "Chat bubble", "消息气泡 liaotian qipao chat bubble typing 预览 yulan 镜头缩放 suofang 持续时间 chixu 透明度 touming 字体大小 ziti 最大宽度 kuandu 垂直偏移 pianyi 圆角 yuanjiao visual"},
			{"qm:camera_view", "visual", ECardColumn::Right, 0, "Camera view", "镜头 jingtou camera drift 漂移 piaoyi dynamic fov 动态视野 dongtai shiye 纵横比 zonghengbi aspect ratio preset 预设 yushe 自定义 zidinyi 视野视角 shijiao visual"},
			{"qm:skin_transition", "visual", ECardColumn::Left, 1, "Skin transition", "皮肤切换 pifu qiehuan skin transition 换皮 huanpi 动画 donghua 开关 kaiguan 类型 leixing 时长 shichang 强度 qiangdu easing 缓动 huandong 锤中偷皮 chuizhong toupi 故障 guzhang glitch 抖动 doudong 弹性 tanxing elastic Tee外观 tee waiguan 循环色调 xunhuan sediao hue 速度 sudu 分身 fenshen dummy visual"},
			{"qm:focus_mode", "visual", ECardColumn::Left, 2, "Focus mode", "禅模式 zhuanzhi moshi focus mode zen mode 隐藏 yincang hud 名字 mingzi 特效 texiao 计分板 jifenban 沉浸 chenjing 无干扰 wuganrao 聊天 liaotian chat 非必要UI visual"},
			{"qm:weapon_animation", "visual", ECardColumn::Right, 1, "Weapon animation", "武器动画 wuqi donghua weapon animation 切换武器动画 qiehuan wuqi donghua weapon switch animation 滑入 huaru 旋转 xuanzhuan visual"},
			{"qm:entity_overlay", "visual", ECardColumn::Right, 2, "Entity overlay", "实体层颜色 shiti ceng yanse 实体层 shiti entity overlay 死亡透明度 siwang 冻结透明度 dongjie 解冻透明度 jiedong 深度冻结 shendu dongjie 深度解冻 shendu jiedong 传送透明度 chuansong cp点透明度 cp checkpoint 开关透明度 kaiguan 叠层透明度 dieceng visual"},
			{"qm:collision_hitbox", "visual", ECardColumn::Right, 5, "Collision hitbox", "碰撞箱模式 pengzhuangxiang moshi 碰撞体积可视化 pengzhuang tiji keshihua collision hitbox hitbox mode 显示碰撞 武器交互 透明度 visual"},
			{"qm:streamer", "visual", ECardColumn::Left, 10, "Streamer mode", "主播模式 zhubo moshi 直播 zhibo 隐私 yinsi 非好友昵称改id feihaoyou nicheng id 非好友皮肤默认 pifu moren 计分板默认国旗 guoqi visual"},
			{"qm:translate_ui", "function", ECardColumn::Left, 15, "Translate UI", "fanyi ui 颜色 yanse color 按钮 anniu button 菜单 caidan menu rgba 自定义 zidingyi custom function"},
			{"qm:card_appearance", "visual", ECardColumn::Left, 17, "Card appearance", "卡片外观 kapian waiguan card appearance backdrop blur 毛玻璃 maoboli 圆角 yuanjiao corner segments visual"},
			{"qm:gores_actor", "function", ECardColumn::Left, 3, "Gores actor", "gores 演员 actor 掉水 diaoshui 自动发言 zidong fayan 表情 biaoqing 表情id emoticon 发送概率 gaolv function"},
			{"qm:gores", "function", ECardColumn::Left, 4, "Gores", "gores kog king of gores 锤枪切换 chuichang qiehuan 自动切枪 zidong qieqiang 自动切锤 zidong qiechui gun hammer prevweapon fire 开火后切锤 kaihuo qiechui 拿到其他武器停用 快速输入 kuaisu shuru fast input 快速输入其他玩家 function"},
			{"qm:key_binds", "function", ECardColumn::Left, 5, "Key binds", "按键绑定 anjian bangding bind 快捷键 kuaijiejian 常用绑定 changyong bangding 武器辅助线 fuzhuxian 异常断开 yichang duankai timeout disconnect function"},
			{"qm:mini_features", "function", ECardColumn::Left, 6, "Mini features", "梦的小功能 meng xiaogongneng 粒子拖尾 lizi tuowei 远程粒子 yuancheng lizi 计分板查分 chafen 聊天框淡出 liaotian danchu 表情选择 biaoqing xuanze 动画优化 donghua youhua 复读 fudu 锤人换皮 chuiren huanpi 随机表情 suiji biaoqing 连击 lianji combo 说话不弹表情 shuo hua biaoqing 本地彩虹名字 caihong mingzi 计分板Qm标识 qm biaoshi scoreboard badge 更新 gengxin 版本 banben 过旧 guojiu 提示 tishi outdated version warning 新版UI xinban ui settings page shezhi yemian 新版IME xinban ime 输入法 shurufa 候选栏 houxuanlan 自动管理 zidong guanli 进程优先级 jincheng youxianji 协作制图 xiezuo zhitu 多人制图 duoren zhitu function"},
			{"qm:jump_hint", "function", ECardColumn::Left, 7, "Jump hint", "位置跳跃提示 tiaoyue tishi jump hint position edge jump color yanse 颜色 horizontal position shuiping weizhi vertical position chuizhi weizhi font size ziti function"},
			{"qm:weapon_trajectory", "function", ECardColumn::Left, 8, "Weapon trajectory", "武器辅助线 wuqi fuzhuxian weapon trajectory 弹道辅助线 dandao fuzhuxian 线宽 xian kuan 透明度 toumingdu 始终显示 shizhong xianshi 按键显示 anjian xianshi function"},
			{"qm:coords", "hud", ECardColumn::Left, 9, "Coordinates", "显示坐标 xianshi zuobiao coords position 自己坐标 ziji 他人坐标 taren 显示x xianshi x 显示y xianshi y 对齐提示 duiqi tishi 严格对齐 yange duiqi hud"},
			{"qm:friend_notify", "function", ECardColumn::Left, 11, "Friend notify", "好友提醒 haoyou tixing 好友上线 shangxian 自动刷新 zidong shuaxin 服务器列表 fuwuqi liebiao 刷新间隔 jiange 进图打招呼 jintu dazhaohu 大字显示 dazi xianshi function"},
			{"qm:block_words", "function", ECardColumn::Left, 12, "Block words", "屏蔽词 pingbici block words 控制台显示 kongzhitai 启用列表 qiyong liebiao 按词长替换 cichang tihuan 多字符替换 duozifu tihuan function"},
			{"qm:qiafen", "function", ECardColumn::Left, 13, "Keyword reply", "关键词回复 guanjianci huifu 自动回复 zidong huifu 冷却 lengque dummy 发言 fayan 规则 guize 改名 gaiming 自动改名 zidong gaiming keyword reply qiafen function"}, // UI 名 keyword_reply，以持久化 key qiafen 为权威
			{"qm:translate", "function", ECardColumn::Left, 14, "Translate", "翻译 fanyi translate 腾讯云 tengxunyun 智谱AI zhipuai 大模型 LLM 自动翻译 zidong fanyi 主动翻译 zhudong fanyi [ru] 目标语言 mubiao yuyan 端点 duandian endpoint 地域 diyu region secret id key api key 密钥 秘钥 凭证 glm-4.5-flash glm-4-flash 模型 model 中文跳过 zhongwen tiaoguo 服务器消息跳过 function"},
			{"qm:pie_menu", "function", ECardColumn::Left, 16, "Pie menu", "饼菜单 bingcaidan pie menu 启用 qiyong ui大小 daxiao 不透明度 butouming 检测距离 jiance juli 改名名单 gaiming mingdan function"},
			{"qm:favorite_maps", "function", ECardColumn::Right, 6, "Favorite maps", "收藏地图 shoucang ditu favorite maps 地图管理 ditu guanli 收藏 shoucang 取消收藏 quxiao shoucang function"},
			{"qm:hj_assist", "function", ECardColumn::Right, 7, "HJ assist", "hj辅助 hj fuzhu 解冻辅助 jiedong fuzhu 自动取消旁观 quxiao pangguan 自动切换 qiehuan tee 自动关闭聊天 guanbi liaotian function"},
			{"qm:player_stats", "hud", ECardColumn::Right, 4, "Player stats", "玩家统计 wanjia tongji player stats gores hud 显示统计 xianshi tongji 进服重置 jinfu chongzhi"},
			{"qm:speedrun_timer", "hud", ECardColumn::Right, 8, "Speedrun timer", "速通计时器 sutong jishiqi speedrun timer 倒计时 daojishi 倒数 daoshu 小时 xiaoshi 分钟 fenzhong 秒 miao 毫秒 haomiao 自动关闭 zidong guanbi hud"},
			{"qm:debug_graph", "hud", ECardColumn::Right, 9, "Debug graph", "调试图表 tiaoshi tubiao debug graph monitoring hud 不透明度 touming 透明度 面板 mianban 快捷键 kuaijiejian 按键 anjian"},
			{"qm:input_overlay", "hud", ECardColumn::Right, 10, "Input overlay", "按键显示 anjian xianshi input overlay 按键叠加 anjian diejia 大小 daxiao 不透明度 butouming 水平位置 shuiping weizhi 垂直位置 chuizhi weizhi hud"},
			{"qm:hud_notifications", "hud", ECardColumn::Right, 11, "HUD notifications", "通知栏 tongzhi lan notification toast echo 系统提示 xitong tishi 黑名单 heimingdan 右侧 youce 动画 donghua 背景 beijing 文字 wenzi hud"},
			{"qm:voice", "hud", ECardColumn::Right, 12, "Voice", "语音 yuyin voice chat 麦克风 maikefeng mic 静音 jingyin 音量 yinliang 语音激活 vad 阈值 yuzhi 释放延迟 shifang yanchi 服务器 fuwuqi token 叠加层 diejiaceng 按住说话 ptt push to talk 全图收听 quantu 衰减 shuijian 距离 juli 半径 banjing 测试 ceshi 本地 bendi 回环 huihuan 设备 shebei 输入 shuru 左右声道定位 左右 zuoyou 声道 shengdao 立体声 stereo 高级 gaoji advanced hud"},
			{"qm:dummy_miniview", "hud", ECardColumn::Right, 13, "Dummy mini view", "分身小窗 fenshen xiaochuang dummy mini view 预览 yulan 缩放 suofang 小窗大小 daxiao 离开视角 offscreen 自动显示 zidong xianshi hud"},
			{"qm:dynamic_island", "hud", ECardColumn::Right, 14, "Dynamic island", "灵动岛 lld lingdongdao dynamic island hud 顶部 dingbu 背景 beijing 颜色 yanse 透明度 touming 黑底 heidi 原版 yuanban 默认 moren classic old style"},
			{"qm:system_media_controls", "hud", ECardColumn::Right, 15, "System media controls", "系统媒体控制 xitong meiti kongzhi smtc media controls 启用系统媒体 qiyong 显示歌曲信息 gequ xinxi 上一个 shangyige 播放暂停 bofang zanting 下一个 xiayige hud"},
			{"qm:lyrics", "hud", ECardColumn::Right, 16, "Lyrics", "歌词 geci lyrics lyric qrc yrc lrc smtc qq music netease lrclib 灵动岛 lingdongdao hud 吸附 xifu 颜色 yanse 首字 shouzi 放大 fangda 指示器 zhishiqi 预览 yulan 偏移 pianyi"},
			{"qm:background_3d", "hud", ECardColumn::Right, 17, "3D background", "3d背景 3d beijing background particles 粒子 lizi 方块 fangkuai cube 爱心 aixin heart 球体 qiuti sphere 金字塔 jinzita pyramid 钻石 zuanshi diamond 圆环 yuanhuan ring 星形 xingxing star 月牙 yueya crescent 混合 hunhe mixed 数量 shuliang 速度 sudu 尺寸 chicun 深度 shendu 透明度 touming 颜色 yanse 随机 suiji 自定义 zidingyi 辉光 huiguang 拖尾 tuowei trail 脉冲 maichong pulse 闪烁 shanshuo twinkle 推动 tuidong 碰撞 pengzhuang 淡入 danru 淡出 danchu hud"},
			{"qm:nameplate_text", "hud", ECardColumn::Right, 18, "Nameplate text", "nameplate text hud 名字 mingzi 名牌 mingpai 文字 wenzi"}, // 数据债：原无 tab 归属，B1 补 hud
			{"qm:laser", "visual", ECardColumn::Right, 3, "Laser", "激光设置 jiguang laser 增强特效 zengqiang texiao 辉光强度 huiguang qiangdu 激光大小 daxiao 半透明 bantouming 圆角端点 yuanjiao duandian 脉冲速度 maichong sudu 脉冲幅度 maichong fudu visual"}, // 数据债：原无 tab 归属，B1 补 visual

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

			// === 设置 deck · deck:<page>-<card>（原无持久化；tab=归属页/子页，column/order 按运行时卡片顺序显式化）===
			{"deck:qmclient-overview-intro", "qmclient-overview", ECardColumn::Full, 0, "QmClient overview", "qmclient overview guide"},
			{"deck:qmclient-overview-guide", "qmclient-overview", ECardColumn::Full, 1, "Page guide", "qmclient page guide tabs"},
			{"deck:qmclient-contributors-community", "qmclient-contributors", ECardColumn::Full, 0, "QmClient Community", "community links qmclient"},
			{"deck:qmclient-contributors-sponsors", "qmclient-contributors", ECardColumn::Full, 1, "Sponsor support", "sponsor support qmclient"},
			{"deck:global-search-input", "global-search", ECardColumn::Full, 0, "Feature Search", "global search feature cards"},
			{"deck:global-search-results", "global-search", ECardColumn::Full, 1, "Search", "global search result cards"},
			{"deck:general-game", "general", ECardColumn::Left, 0, Localizable("Game"), "general game camera weapon"},
			{"deck:general-language", "general", ECardColumn::Right, 0, Localizable("Language"), "general language localization"},
			{"deck:general-client", "general", ECardColumn::Left, 1, Localizable("Client"), "general client theme files"},
			{"deck:general-recording", "general", ECardColumn::Right, 1, Localizable("Demo"), "general demo screenshot csv recording"},
			{"deck:player-identity", "player", ECardColumn::Left, 0, Localizable("Player"), "player dummy name clan identity"},
			{"deck:player-country", "player", ECardColumn::Right, 0, Localizable("Choose country flag"), "player dummy country flag"},
			{"deck:tee-identity", "tee", ECardColumn::Full, 0, "Player", "tee player dummy identity preview"},
			{"deck:tee-skin-options", "tee", ECardColumn::Left, 0, "Skin", "tee skin options colors eyes"},
			{"deck:tee-skin-list", "tee", ECardColumn::Full, 1, "Search", "tee skins search filter list"},
			{"deck:graphics-display", "graphics", ECardColumn::Left, 0, Localizable("Graphics display"), "graphics display monitor window", Localizable("Window and monitor")},
			{"deck:graphics-visual", "graphics", ECardColumn::Left, 1, Localizable("Visual"), "graphics visual rendering", Localizable("Rendering options")},
			{"deck:graphics-backend", "graphics", ECardColumn::Left, 2, Localizable("Graphics backend"), "graphics backend renderer selection", Localizable("Renderer selection")},
			{"deck:graphics-modes", "graphics", ECardColumn::Right, 0, Localizable("Display modes"), "display modes graphics resolutions", Localizable("Available resolutions")},
			{"deck:sound-toggle", "sound", ECardColumn::Left, 0, "Sound", "sound toggle audio"},
			{"deck:sound-volume", "sound", ECardColumn::Left, 1, "Volume", "volume sound audio"},
			{"deck:sound-audio-pack", "sound", ECardColumn::Right, 0, "Audio packs", "audio pack audio packs sound"},
			{"deck:ddnet-demo", "ddnet", ECardColumn::Left, 0, "Demo", "demo ddnet"},
			{"deck:ddnet-gameplay", "ddnet", ECardColumn::Left, 1, "Gameplay", "gameplay ddnet"},
			{"deck:ddnet-background", "ddnet", ECardColumn::Right, 0, "Background", "background ddnet"},
			{"deck:ddnet-miscellaneous", "ddnet", ECardColumn::Right, 1, "Miscellaneous", "miscellaneous ddnet"},
			{"deck:tclient-bind-wheel-editor", "tclient-bind-wheel", ECardColumn::Left, 0, "Bind Wheel", "bind wheel tclient"},
			{"deck:tclient-bind-wheel-preview", "tclient-bind-wheel", ECardColumn::Left, 1, "Preview", "preview bind wheel tclient"},
			{"deck:tclient-status-bar-settings", "tclient-status-bar", ECardColumn::Left, 0, "Status Bar", "status bar tclient"},
			{"deck:tclient-status-bar-items", "tclient-status-bar", ECardColumn::Left, 1, "Status Bar Codes", "status bar codes tclient"},
			{"deck:tclient-status-bar-preview", "tclient-status-bar", ECardColumn::Left, 2, "Preview", "preview status bar tclient"},
			{"deck:tclient-chat-binds-kaomoji", "tclient-chat-binds", ECardColumn::Left, 0, "Kaomoji", "chat binds kaomoji tclient"},
			{"deck:tclient-chat-binds-warlist", "tclient-chat-binds", ECardColumn::Right, 0, "Warlist", "chat binds warlist tclient"},
			{"deck:tclient-chat-binds-other", "tclient-chat-binds", ECardColumn::Left, 1, "Other", "chat binds other tclient"},
			{"deck:tclient-warlist-entries", "tclient-warlist", ECardColumn::Left, 0, "War Entries", "war list entries tclient"},
			{"deck:tclient-warlist-editor", "tclient-warlist", ECardColumn::Right, 0, "Edit Entry", "war list entry editor tclient"},
			{"deck:tclient-warlist-settings", "tclient-warlist", ECardColumn::Right, 1, "Settings", "war list settings tclient"},
			{"deck:tclient-warlist-groups", "tclient-warlist", ECardColumn::Left, 1, "War Groups", "war list groups tclient"},
			{"deck:tclient-warlist-players", "tclient-warlist", ECardColumn::Left, 2, "Online Players", "war list online players tclient"},
			{"deck:tclient-info-links", "tclient-info", ECardColumn::Left, 0, "TClient Links", "tclient links discord website github support"},
			{"deck:tclient-info-files", "tclient-info", ECardColumn::Left, 1, "Config Files", "config files settings profiles war list chat binds"},
			{"deck:tclient-info-developers", "tclient-info", ECardColumn::Right, 0, "TClient Developers", "tclient developers tater sollybunny pebox teero chillerdragon"},
			{"deck:tclient-info-tabs", "tclient-info", ECardColumn::Right, 1, "Hide Settings Tabs", "hide settings tabs bind wheel war list chat binds status bar"},
			{"deck:tclient-profiles-actions", "tclient-profiles", ECardColumn::Left, 0, "Profiles", "profiles save load delete override current preview"},
			{"deck:tclient-profiles-options", "tclient-profiles", ECardColumn::Right, 0, "Profile Options", "profiles dummy custom colors overwrite clan"},
			{"deck:tclient-profiles-list", "tclient-profiles", ECardColumn::Left, 1, "Saved Profiles", "profiles file saved profile list"},
			{"deck:tclient-configs-actions", "tclient-configs", ECardColumn::Left, 0, "Config Changes", "configs apply clear staged changes"},
			{"deck:tclient-configs-filters", "tclient-configs", ECardColumn::Right, 0, "Config Filters", "configs search domains tags compact modified"},
			{"deck:tclient-configs-list", "tclient-configs", ECardColumn::Left, 1, "Configuration", "configs variables values reset"},
			{"deck:appearance-hud-main", "appearance-hud", ECardColumn::Left, 0, "HUD", "appearance hud main"},
			{"deck:appearance-hud-ddrace", "appearance-hud", ECardColumn::Right, 0, "DDRace HUD", "appearance ddrace hud"},
			{"deck:appearance-chat-settings", "appearance-chat", ECardColumn::Left, 0, "Chat", "appearance chat settings"},
			{"deck:appearance-chat-messages", "appearance-chat", ECardColumn::Right, 0, "Messages", "appearance chat messages"},
			{"deck:appearance-chat-preview", "appearance-chat", ECardColumn::Left, 1, "Preview", "appearance chat preview"},
			{"deck:appearance-name-plate-settings", "appearance-name-plate", ECardColumn::Left, 0, "Name Plate", "appearance name plate settings"},
			{"deck:appearance-name-plate-preview", "appearance-name-plate", ECardColumn::Right, 0, "Preview", "appearance name plate preview"},
			{"deck:appearance-hook-collision-main", "appearance-hook-collision", ECardColumn::Left, 0, "Hook collision line", "appearance hook collision line"},
			{"deck:appearance-hook-collision-preview", "appearance-hook-collision", ECardColumn::Right, 0, "Preview", "appearance hook collision preview"},
			{"deck:appearance-info-messages", "appearance-info-messages", ECardColumn::Left, 0, "Info Messages", "appearance info messages"},
			{"deck:appearance-laser-enhanced", "appearance-laser", ECardColumn::Left, 0, "Laser settings", "appearance laser enhanced settings"},
			{"deck:appearance-laser-colors", "appearance-laser", ECardColumn::Left, 1, "Laser colors", "appearance laser colors weapons entities"},
			{"deck:appearance-laser-preview", "appearance-laser", ECardColumn::Right, 0, "Preview", "appearance laser preview"},
			{"deck:controls-mouse", "controls", ECardColumn::Left, 0, "Mouse", "controls mouse sensitivity"},
			{"deck:controls-controller", "controls", ECardColumn::Left, 1, "Controller", "controls controller joystick"},
			{"deck:controls-movement", "controls", ECardColumn::Left, 2, "Movement", "controls movement binds"},
			{"deck:controls-weapon", "controls", ECardColumn::Left, 3, "Weapon", "controls weapon binds"},
			{"deck:controls-voting", "controls", ECardColumn::Right, 0, "Voting", "controls voting binds"},
			{"deck:controls-chat", "controls", ECardColumn::Right, 1, "Chat", "controls chat binds"},
			{"deck:controls-dummy", "controls", ECardColumn::Right, 2, "Dummy", "controls dummy binds"},
			{"deck:controls-miscellaneous", "controls", ECardColumn::Right, 3, "Miscellaneous", "controls miscellaneous binds"},
			{"deck:controls-custom", "controls", ECardColumn::Right, 4, "Custom", "controls custom binds"},
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

	namespace
	{
		bool SearchTextMatches(const char *pText, const char *pQuery)
		{
			return pText != nullptr && pText[0] != '\0' && str_utf8_find_nocase(pText, pQuery) != nullptr;
		}
	}

	std::vector<SCardSearchResult> SearchCards(const char *pQuery, const qm_card_order::CModel &Model)
	{
		std::vector<SCardSearchResult> vResults;
		if(pQuery == nullptr || pQuery[0] == '\0')
			return vResults;

		const std::vector<SCardDefault> &vDefaults = DefaultsTable();
		vResults.reserve(vDefaults.size());
		for(const SCardDefault &Default : vDefaults)
		{
			if(Default.m_pStableId == nullptr)
				continue;
			const char *pLocalizedTitle = Default.m_pTitle != nullptr ? Localize(Default.m_pTitle) : "";
			const char *pLocalizedDescription = Default.m_pDescription != nullptr ? Localize(Default.m_pDescription) : "";
			if(!SearchTextMatches(pLocalizedTitle, pQuery) &&
				!SearchTextMatches(pLocalizedDescription, pQuery) &&
				!SearchTextMatches(Default.m_pSearchKeywords, pQuery))
				continue;

			const int StateIndex = Model.FindByStableId(Default.m_pStableId);
			const char *pCurrentTab = StateIndex >= 0 ? Model.Entry(StateIndex).m_pDefaultTab : nullptr;
			if(pCurrentTab == nullptr)
				pCurrentTab = Default.m_pDefaultTab;
			SCardSearchResult Result;
			Result.m_pStableId = Default.m_pStableId;
			Result.m_Title = pLocalizedTitle;
			Result.m_Description = pLocalizedDescription;
			Result.m_Target = {pCurrentTab, Default.m_pStableId};
			vResults.push_back(std::move(Result));
		}
		return vResults;
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
