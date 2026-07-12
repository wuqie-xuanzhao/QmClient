// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES_H

#define QM_HUD_NOTIFICATION_STATIC_UPSTREAM_RULES(X) \
	X("You will receive whispers", WhispersOn) \
	X("You will not receive any further whispers", WhispersOff) \
	X("You will now see all tees on this server, no matter the distance", ShowAllOn) \
	X("You will no longer see all tees on this server", ShowAllOff) \
	X("Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.", RescueDisabled) \
	X("Unknown emote. Use /emote to see available emotes.", UnknownEmote) \
	X("Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee ", TimeoutCodeSet) \
	X("Team save already in progress", TeamSaveInProgress)

#endif
