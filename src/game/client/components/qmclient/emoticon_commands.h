#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_EMOTICON_COMMANDS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_EMOTICON_COMMANDS_H

#include <engine/console.h>
#include <engine/shared/config.h>

namespace QmEmoticon
{
	// 两个命令使用相同的参数解析与 Emote 入口，强制发射只对本次调用生效。
	template<typename TEmoticon>
	void RegisterCommands(IConsole *pConsole, TEmoticon *pEmoticon)
	{
		pConsole->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) { static_cast<TEmoticon *>(pUserData)->Emote(pResult->GetInteger(0)); }, pEmoticon, "Use emote");
		pConsole->Register("shot_emote", "i[emote-id]", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) { static_cast<TEmoticon *>(pUserData)->Emote(pResult->GetInteger(0), true); }, pEmoticon, "Launch emote");
	}
}

#endif
