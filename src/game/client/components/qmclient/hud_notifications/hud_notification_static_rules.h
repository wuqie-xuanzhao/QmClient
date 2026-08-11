// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_STATIC_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_STATIC_RULES_H

// Compatibility layer for pre-semantic static categories that are still consumed by
// hud_notification_rules.cpp. The semantic upstream/alias tables are the canonical
// source for the migrated static families; do not reintroduce a mixed total-table macro here.

#define QM_HUD_NOTIFICATION_STATIC_TEAM_RULES(X) \
	X("Team save disabled for teams in practice mode", "Team save disabled for teams in practice mode") \
	X("Team load already in progress", "Team load already in progress") \
	X("You have to be in a team (from 1-63)", "You have to be in a team (from 1-63)") \
	X("Team can't be loaded while racing", "Team can't be loaded while racing") \
	X("Team can't be loaded while in team 0 mode", "Team can't be loaded while in team 0 mode") \
	X("Team can't be loaded while practice is enabled", "Team can't be loaded while practice is enabled") \
	X("Could not find your Team", "Could not find your Team") \
	X("To save all players in your team have to be alive and not in '/spec'", "To save all players in your team have to be alive and not in '/spec'") \
	X("Your team has not started yet", "Your team has not started yet") \
	X("Team can't be saved while in team 0 mode", "Team can't be saved while in team 0 mode") \
	X("Team can't be saved while a dragger is active", "Team can't be saved while a dragger is active") \
	X("Your team was killed because it couldn't finish anymore and hasn't entered /practice mode", "Your team was killed because it couldn't finish anymore and hasn't entered /practice mode") \
	X("This team started already", "This team started already") \
	X("这个队伍已经开始比赛了", "This team started already") \
	X("You are in this team already", "You are in this team already") \
	X("You can't change teams while you are dead/a spectator.", "You can't change teams while you are dead/a spectator.") \
	X("你死亡或处于旁观状态时，不能切换队伍。", "You can't change teams while you are dead/a spectator.") \
	X("You can't join super team if you don't have super rights", "You can't join super team if you don't have super rights") \
	X("You have started racing already", "You have started racing already") \
	X("You have used practice mode already", "You have used practice mode already") \
	X("你已经使用过练习模式了", "You have used practice mode already") \
	X("This team is currently saving", "This team is currently saving") \
	X("这个队伍当前正在存档", "This team is currently saving") \
	X("Your team is currently saving", "Your team is currently saving") \
	X("Start holding the hook before loading the savegame to keep the hook", "Start holding the hook before loading the savegame to keep the hook") \
	X("Your team has been killed because it contains an invalid tee state", "Your team has been killed because it contains an invalid tee state") \
	X("You died, but will stay in practice until you use kill.", "You died, but will stay in practice until you use kill.") \
	X("This team was disbanded because there are more players than allowed in the team.", "This team was disbanded because there are more players than allowed in the team.") \
	X("你的队伍已被解锁队伍图块解除锁定", "Your team was unlocked by an unlock team tile") \
	X("Enter /practice mode or restart to avoid the entire team being killed in 60 seconds", "Enter /practice mode or restart to avoid the entire team being killed in 60 seconds") \
	X("Join a team to enable practice mode, which means you can use /r, but can't earn a rank.", "Join a team to enable practice mode, which means you can use /r, but can't earn a rank.") \
	X("Practice mode can't be enabled in team 0 mode.", "Practice mode can't be enabled in team 0 mode.") \
	X("Practice mode can't be enabled while team save or load is in progress", "Practice mode can't be enabled while team save or load is in progress") \
	X("Team is already in practice mode", "Team is already in practice mode") \
	X("Practice mode enabled for your team, happy practicing!", "Practice mode enabled for your team, happy practicing!") \
	X("This team can't be locked", "This team can't be locked") \
	X("Teams are disabled", "Teams are disabled") \
	X("Invites are disabled", "Invites are disabled") \
	X("/map is disabled", "/map is disabled") \
	X("Practice mode is disabled", "Practice mode is disabled") \
	X("Save-function is disabled on this server", "Save-function is disabled on this server") \
	X("You must join a team and play with somebody or else you can't play", "You must join a team and play with somebody or else you can't play") \
	X("No empty team left.", "No empty team left.") \
	X("You can't change teams that fast!", "You can't change teams that fast!") \
	X("This team is locked using /lock. Only members of the team can unlock it using /lock.", "This team is locked using /lock. Only members of the team can unlock it using /lock.") \
	X("This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.", "This team is locked using /lock. Only members of the team can invite you or unlock it using /lock.") \
	X("Player not found", "Player not found") \
	X("Player already invited", "Player already invited") \
	X("Can't invite this quickly", "Can't invite this quickly") \
	X("Can't invite players to this team", "Can't invite players to this team") \
	X("Team mode change disabled", "Team mode change disabled") \
	X("Team mode change is disabled on this server.", "Team mode change is disabled on this server.") \
	X("This team can't have the mode changed", "This team can't have the mode changed") \
	X("Team mode can't be changed while racing", "Team mode can't be changed while racing")

#define QM_HUD_NOTIFICATION_STATIC_SWAP_RESCUE_RULES(X) \
	X("Unknown argument. Check '/rescuemode list'", "Unknown argument. Check '/rescuemode list'") \
	X("未知救援模式参数", "Unknown argument. Check '/rescuemode list'") \
	X("There is nowhere to go back to.", "There is nowhere to go back to.") \
	X("You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.", "You're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled.") \
	X("You haven't previously teleported. Use /tp before using this command.", "You haven't previously teleported. Use /tp before using this command.") \
	X("There is no teleporter with that index on the map.", "There is no teleporter with that index on the map.") \
	X("There is no checkpoint teleporter with that index on the map.", "There is no checkpoint teleporter with that index on the map.") \
	X("Can't enable team 0 mode with practice mode on.", "Can't enable team 0 mode with practice mode on.") \
	X("Can't swap with yourself", "Can't swap with yourself") \
	X("Player is on a different team", "Player is on a different team") \
	X("You and other player need to have started the map", "You and other player need to have started the map") \
	X("Need to have started the map to swap with a player.", "Need to have started the map to swap with a player.") \
	X("You and the other player must not be paused.", "You and the other player must not be paused.") \
	X("Swap is disabled on this server.", "Swap is disabled on this server.") \
	X("Swap is not available on forced solo servers.", "Swap is not available on forced solo servers.") \
	X("Join a team to use swap feature, which means you can swap positions with each other.", "Join a team to use swap feature, which means you can swap positions with each other.") \
	X("You do not have a pending swap request.", "You do not have a pending swap request.")

#define QM_HUD_NOTIFICATION_STATIC_VOTE_MODERATION_RULES(X) \
	X("You are running a vote, please try again after the vote is done!", "You are running a vote, please try again after the vote is done!") \
	X("你正在发起投票，请等当前投票结束后再试", "You are running a vote, please try again after the vote is done!") \
	X("Invalid option", "Invalid option") \
	X("Server does not allow voting to kick players", "Server does not allow voting to kick players") \
	X("Invalid client id to kick", "Invalid client id to kick") \
	X("You can't kick yourself", "You can't kick yourself") \
	X("You can't kick authorized players", "You can't kick authorized players") \
	X("You can kick only your team member", "You can kick only your team member") \
	X("Server does not allow voting to move players to spectators", "Server does not allow voting to move players to spectators") \
	X("Invalid client id to move to spectators", "Invalid client id to move to spectators") \
	X("You can't move yourself to spectators", "You can't move yourself to spectators") \
	X("You can't move authorized players to spectators", "You can't move authorized players to spectators") \
	X("You can only move your team member to spectators", "You can only move your team member to spectators") \
	X("Kill Protection enabled. If you really want to join the spectators, first type /kill", "Kill Protection enabled. If you really want to join the spectators, first type /kill") \
	X("You can only vote after logging in.", "You can only vote after logging in.") \
	X("You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.", "You are not allowed to vote because we're currently checking for VPNs. Try again in ~30 seconds.") \
	X("You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.", "You are not allowed to vote because you appear to be using a VPN. Try connecting without a VPN or contacting an admin if you think this is a mistake.") \
	X("Wait for current vote to end before calling a new one.", "Wait for current vote to end before calling a new one.")

#define QM_HUD_NOTIFICATION_STATIC_STATUS_RULES(X) \
	X("Unknown parameter. Accepted values: default, gametimer, broadcast, both, none", "Unknown parameter. Accepted values: default, gametimer, broadcast, both, none") \
	X("Selected timertype is not supported by your client", "Selected timertype is not supported by your client") \
	X("Timer isn't displayed.", "Timer isn't displayed.") \
	X("Active moderator mode enabled for you.", "Active moderator mode enabled for you.") \
	X("Active moderator mode disabled for you.", "Active moderator mode disabled for you.") \
	X("Server kick/spec votes will now be actively moderated.", "Server kick/spec votes will now be actively moderated.") \
	X("Server kick/spec votes are no longer actively moderated.", "Server kick/spec votes are no longer actively moderated.") \
	X("You can see other players. To disable this use DDNet client and type /showothers", "You can see other players. To disable this use DDNet client and type /showothers") \
	X("Active moderator mode disabled because you are afk.", "Active moderator mode disabled because you are afk.") \
	X("The force pause timer is now over, you can exit with /spec", "The force pause timer is now over, you can exit with /spec") \
	X("Can't /spec that quickly.", "Can't /spec that quickly.") \
	X("Invalid spectator id used", "Invalid spectator id used") \
	X("Players are not allowed to chat from VPNs at this time", "Players are not allowed to chat from VPNs at this time") \
	X("You can't check your team while you are dead/a spectator.", "You can't check your team while you are dead/a spectator.") \
	X("Showing the team top 5 is not allowed on this server.", "Showing the team top 5 is not allowed on this server.") \
	X("Showing the top is not allowed on this server.", "Showing the top is not allowed on this server.") \
	X("Showing the times of others is not allowed on this server.", "Showing the times of others is not allowed on this server.") \
	X("Showing the team rank of other players is not allowed on this server.", "Showing the team rank of other players is not allowed on this server.") \
	X("Showing the rank of other players is not allowed on this server.", "Showing the rank of other players is not allowed on this server.") \
	X("Showing the global points of other players is not allowed on this server.", "Showing the global points of other players is not allowed on this server.") \
	X("Showing the global top points is not allowed on this server.", "Showing the global top points is not allowed on this server.") \
	X("Showing the checkpoint times is not allowed on this server.", "Showing the checkpoint times is not allowed on this server.") \
	X("Showing players from other teams is disabled", "Showing players from other teams is disabled") \
	X("本服务器不允许查看全局积分排行榜", "Showing the global top points is not allowed on this server.") \
	X("本服务器不允许查看 checkpoint 时间", "Showing the checkpoint times is not allowed on this server.") \
	X("Teams are available on this server ；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "Teams are available on this server; if the team is locked, any team member dying will kill the whole team") \
	X("Teams are not available on this server ；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "Teams are not available on this server; if the team is locked, any team member dying will kill the whole team") \
	X("本服务器允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "Teams are available on this server; if the team is locked, any team member dying will kill the whole team") \
	X("本服务器不允许组队；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "Teams are not available on this server; if the team is locked, any team member dying will kill the whole team") \
	X("You have to be in a team to play on this server and all of your team will die if the team is locked", "You have to be in a team to play on this server; if the team is locked, any team member dying will kill the whole team") \
	X("你必须加入队伍才能在本服务器游玩；队伍上锁后，队内任意玩家死亡都会导致全队死亡", "You have to be in a team to play on this server; if the team is locked, any team member dying will kill the whole team") \
	X("Players can collide on this server", "Players can collide on this server") \
	X("Players can't collide on this server", "Players can't collide on this server") \
	X("Players can hook each other on this server", "Players can hook each other on this server") \
	X("Players can't hook each other on this server", "Players can't hook each other on this server") \
	X("Scores are private on this server", "Scores are private on this server") \
	X("Scores are public on this server", "Scores are public on this server") \
	X("本服务器允许玩家碰撞", "Players can collide on this server") \
	X("本服务器不允许玩家碰撞", "Players can't collide on this server") \
	X("本服务器允许玩家互钩", "Players can hook each other on this server") \
	X("本服务器不允许玩家互钩", "Players can't hook each other on this server") \
	X("本服务器的成绩是私密的", "Scores are private on this server") \
	X("本服务器的成绩是公开的", "Scores are public on this server") \
	X("You will not receive any further global chat and server messages", "You will not receive any further global chat and server messages") \
	X("You will receive global chat and server messages", "You will receive global chat and server messages") \
	X("Command is not available on solo servers", "Command is not available on solo servers") \
	X("Emotes are disabled.", "Emotes are disabled.") \
	X("You can now use the preset eye emotes.", "You can now use the preset eye emotes.") \
	X("You don't have any eye emotes, remember to bind some.", "You don't have any eye emotes, remember to bind some.") \
	X("No player with this name found.", "No player with this name found.") \
	X("Invalid X coordinate.", "Invalid X coordinate.") \
	X("Invalid Y coordinate.", "Invalid Y coordinate.") \
	X("Can't recognize specified arguments. Usage: /tpxy x y, e.g. /tpxy 9 3.", "Can't recognize specified arguments. Usage: /tpxy x y, e.g. /tpxy 9 3.") \
	X("You can't hit others", "You can't hit others") \
	X("You can hit others", "You can hit others") \
	X("You can't collide with others", "You can't collide with others") \
	X("You can collide with others", "You can collide with others") \
	X("You can't hook others", "You can't hook others") \
	X("You can hook others", "You can hook others") \
	X("You have unlimited air jumps", "You have unlimited air jumps") \
	X("You don't have unlimited air jumps", "You don't have unlimited air jumps") \
	X("You have a jetpack gun", "You have a jetpack gun") \
	X("You lost your jetpack gun", "You lost your jetpack gun") \
	X("Teleport gun enabled", "Teleport gun enabled") \
	X("Teleport gun disabled", "Teleport gun disabled") \
	X("Teleport grenade enabled", "Teleport grenade enabled") \
	X("Teleport grenade disabled", "Teleport grenade disabled") \
	X("Teleport laser enabled", "Teleport laser enabled") \
	X("Teleport laser disabled", "Teleport laser disabled") \
	X("You can hammer hit others", "You can hammer hit others") \
	X("You can't hammer hit others", "You can't hammer hit others") \
	X("You can shoot others with shotgun", "You can shoot others with shotgun") \
	X("You can't shoot others with shotgun", "You can't shoot others with shotgun") \
	X("You can shoot others with grenade", "You can shoot others with grenade") \
	X("You can't shoot others with grenade", "You can't shoot others with grenade") \
	X("You can shoot others with laser", "You can shoot others with laser") \
	X("You can't shoot others with laser", "You can't shoot others with laser") \
	X("Endless hook has been activated", "Endless hook has been activated") \
	X("Endless hook has been deactivated", "Endless hook has been deactivated")

#endif
