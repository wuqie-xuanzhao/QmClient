/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_BINDS_DEEPFLY_MODE_H
#define GAME_CLIENT_COMPONENTS_BINDS_DEEPFLY_MODE_H

#include <base/system.h>

#include <cstddef>

// NOLINTNEXTLINE(misc-use-internal-linkage)
enum EDeepflyMode
{
	DEEPFLY_MODE_NONE = -1,
	DEEPFLY_MODE_NORMAL = 0,
	DEEPFLY_MODE_DF = 1,
	DEEPFLY_MODE_HDF = 2,
	DEEPFLY_MODE_CUSTOM = 3,
};

static void NormalizeBindCommand(const char *pCommand, char *pNormalized, size_t NormalizedSize)
{
	size_t OutLen = 0;
	bool PendingSpace = false;

	while(*pCommand != '\0' && str_isspace(*pCommand))
		++pCommand;

	while(*pCommand != '\0' && OutLen + 1 < NormalizedSize)
	{
		if(str_isspace(*pCommand))
		{
			PendingSpace = OutLen > 0;
		}
		else
		{
			if(PendingSpace && OutLen + 1 < NormalizedSize)
			{
				pNormalized[OutLen++] = ' ';
				PendingSpace = false;
			}
			pNormalized[OutLen++] = *pCommand;
		}
		++pCommand;
	}

	while(OutLen > 0 && pNormalized[OutLen - 1] == ' ')
		--OutLen;
	pNormalized[OutLen] = '\0';
}

template<typename F>
static void ForEachTopLevelBindCommand(const char *pCommand, F &&Fn)
{
	if(!pCommand)
		return;

	char aCurrentCommand[1024];
	size_t CurrentLen = 0;
	bool InQuotes = false;
	bool EscapeNext = false;

	const auto FlushCommand = [&]() {
		aCurrentCommand[CurrentLen] = '\0';

		char aNormalized[1024];
		NormalizeBindCommand(aCurrentCommand, aNormalized, sizeof(aNormalized));
		if(aNormalized[0] != '\0')
		{
			Fn(aNormalized);
		}

		CurrentLen = 0;
	};

	while(*pCommand != '\0')
	{
		const char c = *pCommand++;
		if(c == ';' && !InQuotes)
		{
			FlushCommand();
			EscapeNext = false;
			continue;
		}

		if(CurrentLen + 1 < sizeof(aCurrentCommand))
		{
			aCurrentCommand[CurrentLen++] = c;
		}

		if(EscapeNext)
		{
			EscapeNext = false;
			continue;
		}

		if(c == '\\')
		{
			EscapeNext = true;
		}
		else if(c == '"')
		{
			InQuotes = !InQuotes;
		}
	}

	FlushCommand();
}

bool IsDeepflyFireCommand(const char *pCommand);
bool IsDeepflyDummyHammerToggleCommand(const char *pCommand);
bool IsIndirectDeepflyScriptCommand(const char *pCommand);
bool IsDeepflyAuxiliaryCommand(const char *pCommand);
int DetectDeepflyModeFromBindCommand(const char *pCommand);

#endif
