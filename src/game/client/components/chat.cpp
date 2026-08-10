/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "chat.h"

#include <base/log.h>

#include <engine/editor.h>
#include <engine/external/regex.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/shared/csv.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <game/client/QmUi/UiTokens.h>
#include <game/client/animstate.h>
#include <game/client/components/censor.h>
#include <game/client/components/console.h>
#include <game/client/components/message_gradient.h>
#include <game/client/components/qmclient/colored_parts.h>
#include <game/client/components/qmclient/modes.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <string>

char CChat::ms_aDisplayText[MAX_LINE_LENGTH] = "";

enum
{
	BLOCK_WORDS_MODE_REGEX = 0,
	BLOCK_WORDS_MODE_FULL,
	BLOCK_WORDS_MODE_BOTH
};

static constexpr float CHAT_SCROLLBAR_WIDTH = 5.0f;
static constexpr float CHAT_SCROLLBAR_MARGIN = 2.0f;
static constexpr float CHAT_SCROLLBAR_ALPHA_SCALE = 0.70f;

static int BlockWordsSeparatorLength(const char *pStr)
{
	const unsigned char C0 = (unsigned char)pStr[0];
	if(C0 == ',' || C0 == ';' || C0 == '|' || C0 == '\n' || C0 == '\r')
		return 1;
	if(C0 == 0xEF && (unsigned char)pStr[1] == 0xBC)
	{
		const unsigned char C2 = (unsigned char)pStr[2];
		if(C2 == 0x8C || C2 == 0x9B)
			return 3;
	}
	return 0;
}

static void ParseBlockWordsList(const char *pList, std::vector<std::string> &OutWords)
{
	OutWords.clear();
	if(!pList || pList[0] == '\0')
		return;

	char aBuf[1024];
	str_copy(aBuf, pList, sizeof(aBuf));
	char *pCursor = aBuf;

	while(*pCursor)
	{
		int SepLen = BlockWordsSeparatorLength(pCursor);
		while(*pCursor && SepLen > 0)
		{
			pCursor += SepLen;
			SepLen = BlockWordsSeparatorLength(pCursor);
		}

		char *pStart = pCursor;
		while(*pCursor && BlockWordsSeparatorLength(pCursor) == 0)
			pCursor++;

		if(pStart == pCursor)
			break;

		if(*pCursor)
		{
			const int CutLen = BlockWordsSeparatorLength(pCursor);
			*pCursor = '\0';
			pCursor += CutLen;
		}

		char *pToken = (char *)str_utf8_skip_whitespaces(pStart);
		str_utf8_trim_right(pToken);
		if(pToken[0] != '\0')
			OutWords.emplace_back(pToken);
	}
}

static void PushUniqueWord(std::vector<std::string> &OutWords, const std::string &Word)
{
	if(std::find(OutWords.begin(), OutWords.end(), Word) == OutWords.end())
		OutWords.push_back(Word);
}

namespace
{
	struct CBlockWordsCache
	{
		std::string m_List;
		int m_Mode = -1;
		std::vector<std::string> m_Words;
		std::vector<Regex> m_Regexes;
	};

	static constexpr const char *QM_CHAT_LOG_DIR = "qmclient/chat_log";
	static constexpr const char *QM_CHAT_LOG_PREFIX = "auto_chat_";
	static constexpr const char *QM_CHAT_LOG_EXTENSION = ".txt";

	struct SChatLogCleanupData
	{
		IStorage *m_pStorage = nullptr;
		time_t m_CutoffDate = 0;
	};

	static bool ExtractChatLogDate(const char *pFilename, time_t *pTimestamp)
	{
		const char *pDate = str_startswith(pFilename, QM_CHAT_LOG_PREFIX);
		if(pDate == nullptr || !str_endswith(pFilename, QM_CHAT_LOG_EXTENSION))
			return false;

		if(str_length(pFilename) != str_length(QM_CHAT_LOG_PREFIX) + 10 + str_length(QM_CHAT_LOG_EXTENSION))
			return false;

		char aDate[11];
		str_truncate(aDate, sizeof(aDate), pDate, 10);
		return timestamp_from_str(aDate, "%Y-%m-%d", pTimestamp);
	}

	static int ChatLogCleanupCallback(const char *pName, int IsDir, int DirType, void *pUser)
	{
		if(IsDir)
			return 0;

		SChatLogCleanupData *pData = (SChatLogCleanupData *)pUser;
		time_t FileDate = 0;
		if(!ExtractChatLogDate(pName, &FileDate) || FileDate >= pData->m_CutoffDate)
			return 0;

		char aFilename[IO_MAX_PATH_LENGTH];
		str_format(aFilename, sizeof(aFilename), "%s/%s", QM_CHAT_LOG_DIR, pName);
		if(!pData->m_pStorage->RemoveFile(aFilename, DirType))
			log_error("chat", "Failed to remove old chat log '%s'", aFilename);
		return 0;
	}

	static const char *ChatLogKind(int ClientId, int Team)
	{
		if(ClientId == -1)
			return "system";
		if(ClientId == -2)
			return "client";
		if(Team == TEAM_WHISPER_SEND)
			return "whisper-send";
		if(Team == TEAM_WHISPER_RECV)
			return "whisper-recv";
		if(Team == 1)
			return "team";
		return "public";
	}
} // namespace

static void UpdateBlockWordsCache(CBlockWordsCache &Cache)
{
	if(Cache.m_List == g_Config.m_QmBlockWordsList && Cache.m_Mode == g_Config.m_QmBlockWordsMode)
		return;

	Cache.m_List = g_Config.m_QmBlockWordsList;
	Cache.m_Mode = g_Config.m_QmBlockWordsMode;

	ParseBlockWordsList(Cache.m_List.c_str(), Cache.m_Words);
	Cache.m_Regexes.clear();

	if(Cache.m_Mode == BLOCK_WORDS_MODE_REGEX || Cache.m_Mode == BLOCK_WORDS_MODE_BOTH)
	{
		Cache.m_Regexes.reserve(Cache.m_Words.size());
		for(const auto &Pattern : Cache.m_Words)
		{
			Regex Re(Pattern);
			if(!Re.error().empty())
			{
				log_error("blocklist", "Invalid regex: %s", Pattern.c_str());
				continue;
			}
			Cache.m_Regexes.push_back(std::move(Re));
		}
	}
}

static bool ReplaceLiteralWords(std::string &Text, const std::vector<std::string> &Words, char Replacement, bool MultiReplace, std::vector<std::string> *pMatched)
{
	bool AnyReplaced = false;

	for(const auto &Word : Words)
	{
		if(Word.empty())
			continue;

		std::string Result;
		const char *pCursor = Text.c_str();
		const char *pMatchEnd = nullptr;
		bool WordReplaced = false;

		while(const char *pMatch = str_utf8_find_nocase(pCursor, Word.c_str(), &pMatchEnd))
		{
			Result.append(pCursor, pMatch - pCursor);
			if(MultiReplace)
				Result.append(pMatchEnd - pMatch, Replacement);
			else
				Result.push_back(Replacement);
			pCursor = pMatchEnd;
			WordReplaced = true;
		}

		if(WordReplaced)
		{
			Result.append(pCursor);
			Text.swap(Result);
			AnyReplaced = true;
			if(pMatched)
				PushUniqueWord(*pMatched, Word);
		}
	}

	return AnyReplaced;
}

static bool ReplaceRegexWords(std::string &Text, std::vector<Regex> &Regexes, char Replacement, bool MultiReplace, std::vector<std::string> *pMatched)
{
	bool AnyReplaced = false;

	for(Regex &Re : Regexes)
	{
		bool RegexMatched = false;
		std::string Result = Re.replace(Text, true, [&](const std::string &Str, int, int Group) {
			if(Group != 0)
				return std::string();
			RegexMatched = true;
			if(pMatched)
				PushUniqueWord(*pMatched, Str);
			if(MultiReplace)
				return std::string(Str.size(), Replacement);
			return std::string(1, Replacement);
		});

		if(RegexMatched)
		{
			Text.swap(Result);
			AnyReplaced = true;
		}
	}

	return AnyReplaced;
}

static bool ApplyBlockWords(std::string &Text, std::vector<std::string> *pMatched)
{
	if(!g_Config.m_QmBlockWordsEnabled || g_Config.m_QmBlockWordsList[0] == '\0')
		return false;

	static CBlockWordsCache s_Cache;
	UpdateBlockWordsCache(s_Cache);
	if(s_Cache.m_Words.empty())
		return false;

	const char Replacement = g_Config.m_QmBlockWordsReplacementChar[0] != '\0' ? g_Config.m_QmBlockWordsReplacementChar[0] : '*';
	const bool MultiReplace = g_Config.m_QmBlockWordsMultiReplace != 0;
	const int Mode = g_Config.m_QmBlockWordsMode;

	bool Replaced = false;
	if(Mode == BLOCK_WORDS_MODE_REGEX || Mode == BLOCK_WORDS_MODE_BOTH)
		Replaced |= ReplaceRegexWords(Text, s_Cache.m_Regexes, Replacement, MultiReplace, pMatched);
	if(Mode == BLOCK_WORDS_MODE_FULL || Mode == BLOCK_WORDS_MODE_BOTH)
		Replaced |= ReplaceLiteralWords(Text, s_Cache.m_Words, Replacement, MultiReplace, pMatched);

	return Replaced;
}

static constexpr int COMMAND_PREVIEW_TOKEN_LENGTH = 128;

static bool CommandPreviewNameIs(const char *pName, const char *pCommand)
{
	return str_comp_nocase(pName, pCommand) == 0;
}

static const char *ReadCommandPreviewToken(const char *pText, char *pToken, size_t TokenSize)
{
	if(TokenSize == 0)
		return pText;

	pToken[0] = '\0';
	if(pText == nullptr)
		return "";

	const char *pCursor = str_skip_whitespaces_const(pText);
	char *pDst = pToken;
	char *pEnd = pToken + TokenSize;

	if(*pCursor == '"')
	{
		pCursor++;
		while(*pCursor != '\0' && *pCursor != '"')
		{
			if(*pCursor == '\\' && pCursor[1] != '\0')
				pCursor++;
			if(pDst + 1 < pEnd)
				*pDst++ = *pCursor;
			pCursor++;
		}
		if(*pCursor == '"')
			pCursor++;
	}
	else
	{
		while(*pCursor != '\0' && !str_isspace(*pCursor))
		{
			if(pDst + 1 < pEnd)
				*pDst++ = *pCursor;
			pCursor++;
		}
	}

	*pDst = '\0';
	return pCursor;
}

static void CopyCommandPreviewRest(const char *pText, char *pBuf, size_t BufSize)
{
	if(BufSize == 0)
		return;

	pBuf[0] = '\0';
	if(pText == nullptr)
		return;

	const char *pRest = str_skip_whitespaces_const(pText);
	if(pRest[0] == '\0')
		return;

	if(pRest[0] == '"')
	{
		char aQuoted[COMMAND_PREVIEW_TOKEN_LENGTH];
		const char *pAfterQuoted = ReadCommandPreviewToken(pRest, aQuoted, sizeof(aQuoted));
		if(*str_skip_whitespaces_const(pAfterQuoted) == '\0')
		{
			str_copy(pBuf, aQuoted, BufSize);
			return;
		}
	}

	str_copy(pBuf, pRest, BufSize);
	str_utf8_trim_right(pBuf);
}

static void DoCachedChatPopupLabel(CUi *pUi, CUIElement &LabelUiElement, const CUIRect &Rect, const char *pText, float Size, int Align)
{
	SLabelProperties LabelProps;
	LabelProps.m_MaxWidth = maximum(0.0f, Rect.w - 2.0f);
	LabelProps.m_EllipsisAtEnd = true;
	pUi->DoLabelStreamed(*LabelUiElement.Rect(0), &Rect, pText, Size, Align, LabelProps);
}

static const char *ChatTranslateBackendWarning()
{
	if(str_comp_nocase(g_Config.m_QmTranslateBackend, "tencentcloud") == 0)
	{
		if(g_Config.m_QmTranslateTcSecretId[0] == '\0' || g_Config.m_QmTranslateTcSecretKey[0] == '\0')
			return Localize("⚠️ Tencent Cloud API not configured");
	}
	else if(str_comp_nocase(g_Config.m_QmTranslateBackend, "libretranslate") == 0)
	{
		if(g_Config.m_QmTranslateLibreKey[0] == '\0')
			return Localize("⚠️ LibreTranslate API Key not set");
	}
	else if(str_comp_nocase(g_Config.m_QmTranslateBackend, "llm") == 0)
	{
		if(g_Config.m_QmTranslateLlmKeyZhipu[0] == '\0' &&
			g_Config.m_QmTranslateLlmKeyDeepseek[0] == '\0' &&
			g_Config.m_QmTranslateLlmKeyOpenai[0] == '\0' &&
			g_Config.m_QmTranslateLlmKeyCustom[0] == '\0')
			return Localize("⚠️ LLM API Key not configured");
	}
	return nullptr;
}

CChat::CLine::CLine()
{
	m_TextContainerIndex.Reset();
	m_QuadContainerIndex = -1;
	m_aYOffset[0] = -1.0f;
	m_aYOffset[1] = -1.0f;
	m_TextYOffset = 0.0f;
	m_CutOffProgress = 0.0f;
	CChat::ResetPresentationState(m_Presentation);
	m_ForceVisible = false;
	m_ConsoleSuppressed = false;
	m_ServerMessageClass = QmHudNotifications::EServerMessageClass::None;
}

void CChat::CLine::Reset(CChat &This)
{
	This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	This.Graphics()->DeleteQuadContainer(m_QuadContainerIndex);
	m_QuadContainerIndex = -1;
	m_Initialized = false;
	m_Time = 0;
	m_aText[0] = '\0';
	m_aName[0] = '\0';
	m_aYOffset[0] = -1.0f;
	m_aYOffset[1] = -1.0f;
	m_TextYOffset = 0.0f;
	m_CutOffProgress = 0.0f;
	CChat::ResetPresentationState(m_Presentation);
	m_Friend = false;
	m_ForceVisible = false;
	m_ConsoleSuppressed = false;
	m_ServerMessageClass = QmHudNotifications::EServerMessageClass::None;
	m_TimesRepeated = 0;
	m_vMergedAuthors.clear();
	m_pManagedTeeRenderInfo = nullptr;
	m_pTranslateResponse = nullptr;

	// 递增翻译 ID，标记内容已变更
	// 溢出保护：跳过 0，避免与默认值冲突
	if(m_TranslationId < std::numeric_limits<unsigned int>::max())
		m_TranslationId++;
	else
		m_TranslationId = 1;
}

static float ClampPresentationProgress(float Value)
{
	return std::clamp(Value, 0.0f, 1.0f);
}

CChat::CChat()
{
	m_Mode = MODE_NONE;
	m_LastPresentationUpdateTime = 0;
	m_LargeAreaOpenTick = 0;
	m_LastPresentationShowLargeArea = false;
	m_PendingConsoleLineIndex = -1;
	m_aChatLogLastCleanupDate[0] = '\0';

	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = minimum(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

const CChat::CCommand *CChat::FindServerCommand(const char *pName) const
{
	for(const CCommand &Command : m_vServerCommands)
	{
		if(str_comp_nocase(Command.m_aName, pName) == 0)
			return &Command;
	}
	return nullptr;
}

void CChat::RefreshSlashCommandSuggestions()
{
	const char *pInput = m_Input.GetString();
	if(m_SlashCommandSuggestionsDismissed && str_comp(pInput, m_aSlashCommandSuggestionsDismissedInput) == 0)
	{
		m_vSlashCommandSuggestions.clear();
		return;
	}
	m_SlashCommandSuggestionsDismissed = false;
	m_aSlashCommandSuggestionsDismissedInput[0] = '\0';
	m_vSlashCommandSuggestions = BuildSlashCommandSuggestions(pInput, 8);
}

const char *CChat::LocalizeCommandPreviewText(const char *pText) const
{
	if(pText == nullptr || pText[0] == '\0')
		return pText;

	return Localize(pText);
}

float CChat::CalculateCutOffOffsetX(float Progress)
{
	if(!g_Config.m_QmChatAnimSlideOut)
		return 0.0f;
	return -24.0f * std::clamp(Progress, 0.0f, 1.0f);
}

bool CChat::BuildCommandUsagePreview(const char *pInput, char *pBuf, size_t BufSize) const
{
	if(BufSize == 0)
		return false;

	pBuf[0] = '\0';
	const int PreviewBufSize = (int)BufSize;
	if(pInput == nullptr || pInput[0] != '/' || pInput[1] == '\0')
		return false;

	char aCommand[COMMAND_PREVIEW_TOKEN_LENGTH];
	const char *pAfterCommand = ReadCommandPreviewToken(pInput + 1, aCommand, sizeof(aCommand));
	if(aCommand[0] == '\0')
		return false;

	char aFirstArg[COMMAND_PREVIEW_TOKEN_LENGTH];
	const char *pAfterFirstArg = ReadCommandPreviewToken(pAfterCommand, aFirstArg, sizeof(aFirstArg));

	char aRestArg[MAX_LINE_LENGTH];
	CopyCommandPreviewRest(pAfterCommand, aRestArg, sizeof(aRestArg));

	char aRestAfterFirstArg[MAX_LINE_LENGTH];
	CopyCommandPreviewRest(pAfterFirstArg, aRestAfterFirstArg, sizeof(aRestAfterFirstArg));

	if(CommandPreviewNameIs(aCommand, "points"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Query points for %s"), aRestArg);
		else
			str_format(pBuf, PreviewBufSize, Localize("Query points for %s"), Localize("yourself"));
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "rank"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Query rank for %s"), aRestArg);
		else
			str_format(pBuf, PreviewBufSize, Localize("Query rank for %s"), Localize("yourself"));
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "teamrank") || CommandPreviewNameIs(aCommand, "rankteam"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Query team rank for %s"), aRestArg);
		else
			str_format(pBuf, PreviewBufSize, Localize("Query team rank for %s"), Localize("yourself"));
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "r") || CommandPreviewNameIs(aCommand, "rescue"))
	{
		str_copy(pBuf, Localize("Rescue: auto mode teleports out of freeze; manual mode records a rescue point on landing and teleports when frozen"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "w") || CommandPreviewNameIs(aCommand, "whisper"))
	{
		if(aFirstArg[0] != '\0' && aRestAfterFirstArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Whisper to %s: %s"), aFirstArg, aRestAfterFirstArg);
		else if(aFirstArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Whisper to %s"), aFirstArg);
		else
			str_copy(pBuf, Localize("Whisper: /w player message"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "c") || CommandPreviewNameIs(aCommand, "converse"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Reply to the last whisper target: %s"), aRestArg);
		else
			str_copy(pBuf, Localize("Reply to the last whisper target"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "mapinfo"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Query map info for %s"), aRestArg);
		else
			str_copy(pBuf, Localize("Query current map info"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "team"))
	{
		if(aFirstArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Join team %s"), aFirstArg);
		else
			str_copy(pBuf, Localize("Show your current team"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "lock"))
	{
		if(str_comp(aFirstArg, "0") == 0)
			str_copy(pBuf, Localize("Unlock the team"), BufSize);
		else
			str_copy(pBuf, Localize("Lock the team so other players cannot join directly"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "invite"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Invite %s to the locked team"), aRestArg);
		else
			str_format(pBuf, PreviewBufSize, Localize("Invite %s to the locked team"), Localize("a player"));
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "swap"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Request to swap positions with %s"), aRestArg);
		else
			str_copy(pBuf, Localize("Request a position swap"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "save"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Save the team as %s"), aRestArg);
		else
			str_copy(pBuf, Localize("Save the current team"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "load"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Load save %s"), aRestArg);
		else
			str_copy(pBuf, Localize("Show existing saves"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "settings"))
	{
		if(aFirstArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("Query server setting: %s"), aFirstArg);
		else
			str_copy(pBuf, Localize("Show server settings"), BufSize);
		return true;
	}

	if(CommandPreviewNameIs(aCommand, "help"))
	{
		if(aRestArg[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("%s (/%s %s)"), Localize("Usage"), aCommand, aRestArg);
		else
			str_format(pBuf, PreviewBufSize, Localize("Usage: /%s %s"), aCommand, "");
		return true;
	}

	const CCommand *pCommand = FindServerCommand(aCommand);
	if(pCommand == nullptr)
		return false;

	const char *pHelpText = LocalizeCommandPreviewText(pCommand->m_aHelpText);
	if(pHelpText != nullptr && pHelpText[0] != '\0')
	{
		if(pCommand->m_aParams[0] != '\0')
			str_format(pBuf, PreviewBufSize, Localize("%s (/%s %s)"), pHelpText, pCommand->m_aName, pCommand->m_aParams);
		else
			str_copy(pBuf, pHelpText, BufSize);
		return true;
	}

	if(pCommand->m_aParams[0] != '\0')
	{
		str_format(pBuf, PreviewBufSize, Localize("Usage: /%s %s"), pCommand->m_aName, pCommand->m_aParams);
		return true;
	}

	return false;
}

void CChat::HideArgumentCandidates()
{
	m_vArgumentCandidates.clear();
	m_ArgumentCompletionCachedCursor = std::numeric_limits<size_t>::max();
	m_ArgumentCandidatePopup.m_RectValid = false;
	m_ArgumentCandidatePopup.m_PressedIndex = -1;
	m_ArgumentCandidateLastMousePos.reset();
	m_ArgumentCandidatesRequestedByTab = false;
	m_ArgumentCompletionNextSourceCheck = 0;
	m_ArgumentCompletionSelected = 0;
	m_ArgumentCompletionScroll = 0;
}

void CChat::RefreshArgumentCandidates()
{
	if(m_Mode == MODE_NONE)
	{
		HideArgumentCandidates();
		return;
	}

	QmChatCompletion::SContext Context;
	const char *pInput = m_Input.GetString();
	const size_t CursorOffset = m_Input.GetCursorOffset();
	if(!QmChatCompletion::ParseContext(pInput, CursorOffset, Context) &&
		(!m_ArgumentCandidatesRequestedByTab || !QmChatCompletion::ParsePlayerTabContext(pInput, CursorOffset, Context)))
	{
		m_ArgumentCompletionCachedInput = pInput;
		m_ArgumentCompletionCachedCursor = CursorOffset;
		HideArgumentCandidates();
		return;
	}
	const int64_t Now = time();
	const bool InputUnchanged = m_ArgumentCompletionCachedInput == pInput && m_ArgumentCompletionCachedCursor == CursorOffset;
	if(InputUnchanged && Now < m_ArgumentCompletionNextSourceCheck)
		return;
	m_ArgumentCompletionNextSourceCheck = Now + time_freq() / 4;

	CServerInfo CurrentServerInfo;
	bool IsDdnetMode = false;
	if(Context.m_Provider == QmChatCompletion::EProvider::MAP)
	{
		Client()->GetServerInfo(&CurrentServerInfo);
		IsDdnetMode = str_comp(CurrentServerInfo.m_aCommunityId, IServerBrowser::COMMUNITY_DDNET) == 0;
		if(!IsDdnetMode && ServerBrowser() != nullptr && Client()->ServerAddress() != nullptr)
		{
			const IServerBrowser::CServerEntry *pEntry = ServerBrowser()->Find(*Client()->ServerAddress());
			IsDdnetMode = pEntry != nullptr && pEntry->m_GotInfo && str_comp(pEntry->m_Info.m_aCommunityId, IServerBrowser::COMMUNITY_DDNET) == 0;
		}
	}

	uint64_t SourceSignature = 0;
	if(Context.m_Provider == QmChatCompletion::EProvider::PLAYER)
	{
		for(const auto *pPlayerInfo : GameClient()->m_Snap.m_apInfoByName)
		{
			if(pPlayerInfo == nullptr)
				continue;
			const char *pName = GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aName;
			SourceSignature = SourceSignature * 1099511628211ULL ^ str_quickhash(pName);
		}
	}
	else if(Context.m_Provider == QmChatCompletion::EProvider::MAP)
	{
		SourceSignature = ServerBrowser() != nullptr ? ((uint64_t)ServerBrowser()->NumServers() << 32) ^ ServerBrowser()->LoadingProgression() : 0;
		SourceSignature = SourceSignature * 1099511628211ULL ^ (IsDdnetMode ? 1ULL : 0ULL);
		for(const CVoteOptionClient *pOption = GameClient()->m_Voting.FirstOption(); pOption != nullptr; pOption = pOption->m_pNext)
			SourceSignature = SourceSignature * 1099511628211ULL ^ str_quickhash(pOption->m_aDescription);
		for(const std::string &MapName : Client()->MaplistEntries())
			SourceSignature = SourceSignature * 1099511628211ULL ^ str_quickhash(MapName.c_str());
	}

	if(InputUnchanged && m_ArgumentCompletionSourceSignature == SourceSignature)
		return;

	m_ArgumentCompletionCachedInput = pInput;
	m_ArgumentCompletionCachedCursor = CursorOffset;
	m_ArgumentCompletionSourceSignature = SourceSignature;
	m_ArgumentCompletionContext = Context;
	m_vArgumentCandidates.clear();
	m_ArgumentCandidatePopup.m_RectValid = false;
	if(Context.m_Provider == QmChatCompletion::EProvider::PLAYER)
	{
		for(const auto *pPlayerInfo : GameClient()->m_Snap.m_apInfoByName)
		{
			if(pPlayerInfo == nullptr)
				continue;
			QmChatCompletion::AddMatchingCandidate(m_vArgumentCandidates, GameClient()->m_aClients[pPlayerInfo->m_ClientId].m_aName, Context.m_Query.c_str(), true);
		}
	}
	else if(Context.m_Provider == QmChatCompletion::EProvider::MAP)
	{
		if(ServerBrowser() != nullptr)
		{
			for(int ServerIndex = 0; ServerIndex < ServerBrowser()->NumServers(); ++ServerIndex)
			{
				const CServerInfo *pInfo = ServerBrowser()->Get(ServerIndex);
				if(pInfo == nullptr || pInfo->m_aMap[0] == '\0')
					continue;
				std::string FallbackCategory;
				QmChatCompletion::ExtractMapCategory(pInfo->m_aCommunityType, pInfo->m_aName, FallbackCategory);
				std::string Category;
				QmChatCompletion::ResolveMapCompletionCategory(pInfo->m_aMap, IsDdnetMode, FallbackCategory.c_str(), Category);
				QmChatCompletion::AddMatchingCandidate(m_vArgumentCandidates, pInfo->m_aMap, Context.m_Query.c_str(), false, Category.c_str());
			}
		}
		std::string CurrentCategory;
		QmChatCompletion::ExtractMapCategory(CurrentServerInfo.m_aCommunityType, CurrentServerInfo.m_aName, CurrentCategory);
		for(const CVoteOptionClient *pOption = GameClient()->m_Voting.FirstOption(); pOption != nullptr; pOption = pOption->m_pNext)
		{
			std::string MapName;
			if(QmChatCompletion::ExtractMapNameFromVoteOption(pOption->m_aDescription, MapName))
			{
				std::string Category;
				QmChatCompletion::ResolveMapCompletionCategory(MapName.c_str(), IsDdnetMode, CurrentCategory.c_str(), Category);
				QmChatCompletion::AddMatchingCandidate(m_vArgumentCandidates, MapName.c_str(), Context.m_Query.c_str(), false, Category.c_str());
			}
		}
		for(const std::string &MapName : Client()->MaplistEntries())
		{
			std::string Category;
			QmChatCompletion::ResolveMapCompletionCategory(MapName.c_str(), IsDdnetMode, CurrentCategory.c_str(), Category);
			QmChatCompletion::AddMatchingCandidate(m_vArgumentCandidates, MapName.c_str(), Context.m_Query.c_str(), false, Category.c_str());
		}
	}
	QmChatCompletion::SortCandidates(m_vArgumentCandidates);
	m_vArgumentCandidates.erase(std::unique(m_vArgumentCandidates.begin(), m_vArgumentCandidates.end(), [](const auto &Left, const auto &Right) {
		return str_comp_nocase(Left.m_Value.c_str(), Right.m_Value.c_str()) == 0;
	}),
		m_vArgumentCandidates.end());
	m_ArgumentCompletionSelected = 0;
	m_ArgumentCompletionScroll = 0;
	m_ArgumentCandidatePopup.m_PressedIndex = -1;
	if(m_vArgumentCandidates.empty())
		m_ArgumentCandidatePopup.m_RectValid = false;
}

void CChat::EnsureArgumentCandidateVisible()
{
	if(m_vArgumentCandidates.empty())
		return;
	m_ArgumentCompletionSelected = std::clamp(m_ArgumentCompletionSelected, 0, (int)m_vArgumentCandidates.size() - 1);
	const int VisibleRows = maximum(1, m_ArgumentCandidatePopup.m_VisibleRows);
	if(m_ArgumentCompletionSelected < m_ArgumentCompletionScroll)
		m_ArgumentCompletionScroll = m_ArgumentCompletionSelected;
	else if(m_ArgumentCompletionSelected >= m_ArgumentCompletionScroll + VisibleRows)
		m_ArgumentCompletionScroll = m_ArgumentCompletionSelected - VisibleRows + 1;
	m_ArgumentCompletionScroll = std::clamp(m_ArgumentCompletionScroll, 0, maximum(0, (int)m_vArgumentCandidates.size() - VisibleRows));
}

bool CChat::ApplyArgumentCandidate(int Index)
{
	if(Index < 0 || Index >= (int)m_vArgumentCandidates.size())
		return false;
	char aCompleted[MAX_LINE_LENGTH];
	size_t CursorOffset = 0;
	if(!QmChatCompletion::ApplyCandidate(m_Input.GetString(), m_ArgumentCompletionContext, m_vArgumentCandidates[Index].m_Value.c_str(), aCompleted, sizeof(aCompleted), CursorOffset))
		return false;
	m_Input.Set(aCompleted);
	m_Input.SetCursorOffset(CursorOffset);
	m_Input.Activate(EInputPriority::CHAT);
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_ArgumentCompletionCachedCursor = std::numeric_limits<size_t>::max();
	HideArgumentCandidates();
	return true;
}

int CChat::ArgumentCandidateIndexAt(vec2 MousePos) const
{
	if(!m_ArgumentCandidatePopup.m_RectValid ||
		MousePos.x < m_ArgumentCandidatePopup.m_X || MousePos.x > m_ArgumentCandidatePopup.m_X + m_ArgumentCandidatePopup.m_W ||
		MousePos.y < m_ArgumentCandidatePopup.m_Y || MousePos.y > m_ArgumentCandidatePopup.m_Y + m_ArgumentCandidatePopup.m_H)
		return -1;
	const int Row = (int)((MousePos.y - m_ArgumentCandidatePopup.m_Y) / m_ArgumentCandidatePopup.m_RowHeight);
	const int Index = m_ArgumentCompletionScroll + Row;
	return Row >= 0 && Row < m_ArgumentCandidatePopup.m_VisibleRows && Index < (int)m_vArgumentCandidates.size() ? Index : -1;
}

void CChat::RenderArgumentCandidates(const CUIRect &InputRect, float Width)
{
	if(m_vArgumentCandidates.empty())
	{
		m_ArgumentCandidatePopup.m_RectValid = false;
		return;
	}

	const float FontSize = maximum(7.0f, this->FontSize() * 0.95f);
	const float RowHeight = FontSize + 5.0f;
	const int VisibleRows = std::min({5, (int)m_vArgumentCandidates.size(), maximum(1, (int)((InputRect.y - 54.0f) / RowHeight))});
	const float PopupHeight = RowHeight * VisibleRows;
	const bool HasScrollbar = (int)m_vArgumentCandidates.size() > VisibleRows;
	float ContentWidth = 0.0f;
	for(const QmChatCompletion::SCandidate &Candidate : m_vArgumentCandidates)
	{
		float RowContentWidth = TextRender()->TextWidth(FontSize, Candidate.m_Value.c_str());
		if(!Candidate.m_Detail.empty())
			RowContentWidth += 8.0f + TextRender()->TextWidth(FontSize, Localize(Candidate.m_Detail.c_str())) + 1.0f;
		ContentWidth = maximum(ContentWidth, RowContentWidth);
	}
	const float PopupWidth = QmChatCompletion::CalculateCandidatePopupWidth(Width, ContentWidth, HasScrollbar);
	const CUIRect PopupRect = {InputRect.x, InputRect.y - PopupHeight - 3.0f, PopupWidth, PopupHeight};
	m_ArgumentCandidatePopup.m_RectValid = true;
	m_ArgumentCandidatePopup.m_X = PopupRect.x;
	m_ArgumentCandidatePopup.m_Y = PopupRect.y;
	m_ArgumentCandidatePopup.m_W = PopupRect.w;
	m_ArgumentCandidatePopup.m_H = PopupRect.h;
	m_ArgumentCandidatePopup.m_RowHeight = RowHeight;
	m_ArgumentCandidatePopup.m_VisibleRows = VisibleRows;
	m_ArgumentCompletionSelected = std::clamp(m_ArgumentCompletionSelected, 0, (int)m_vArgumentCandidates.size() - 1);
	m_ArgumentCompletionScroll = std::clamp(m_ArgumentCompletionScroll, 0, maximum(0, (int)m_vArgumentCandidates.size() - VisibleRows));

	PopupRect.Draw(ColorRGBA(0.045f, 0.055f, 0.075f, 0.94f), IGraphics::CORNER_ALL, 4.0f);
	const vec2 MousePos = GetChatMousePos();
	const int HoveredIndex = ArgumentCandidateIndexAt(MousePos);
	const bool MouseMoved = !m_ArgumentCandidateLastMousePos.has_value() ||
				m_ArgumentCandidateLastMousePos->x != MousePos.x || m_ArgumentCandidateLastMousePos->y != MousePos.y;
	m_ArgumentCandidateLastMousePos = MousePos;
	if(MouseMoved && HoveredIndex >= 0)
		m_ArgumentCompletionSelected = HoveredIndex;

	for(int Row = 0; Row < VisibleRows; ++Row)
	{
		const int Index = m_ArgumentCompletionScroll + Row;
		if(Index >= (int)m_vArgumentCandidates.size())
			break;
		const QmChatCompletion::SCandidate &Candidate = m_vArgumentCandidates[Index];
		CUIRect RowRect = {PopupRect.x, PopupRect.y + Row * RowHeight, PopupRect.w, RowHeight};
		if(Index == m_ArgumentCompletionSelected)
			RowRect.Draw(ColorRGBA(0.24f, 0.45f, 0.76f, 0.72f), IGraphics::CORNER_ALL, 3.0f);

		CTextCursor Cursor;
		Cursor.SetPosition(vec2(RowRect.x + 5.0f, RowRect.y + 2.5f));
		Cursor.m_FontSize = FontSize;
		float DetailWidth = 0.0f;
		const float RightPadding = HasScrollbar ? 9.0f : 5.0f;
		if(!Candidate.m_Detail.empty())
		{
			const char *pDetail = Localize(Candidate.m_Detail.c_str());
			DetailWidth = minimum(TextRender()->TextWidth(FontSize, pDetail) + 1.0f, maximum(0.0f, RowRect.w - 5.0f - RightPadding));
			CTextCursor DetailCursor;
			DetailCursor.SetPosition(vec2(RowRect.x + RowRect.w - RightPadding - DetailWidth, RowRect.y + 2.5f));
			DetailCursor.m_FontSize = FontSize;
			DetailCursor.m_LineWidth = DetailWidth;
			DetailCursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END;
			TextRender()->TextColor(1.0f, 0.82f, 0.34f, 1.0f);
			TextRender()->TextEx(&DetailCursor, pDetail);
		}
		Cursor.m_LineWidth = maximum(0.0f, RowRect.w - 5.0f - RightPadding - (DetailWidth > 0.0f ? DetailWidth + 8.0f : 0.0f));
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END;
		TextRender()->TextColor(0.90f, 0.93f, 1.0f, 0.96f);
		if(Candidate.m_MatchOffset >= 0 && Candidate.m_MatchLength > 0)
		{
			const std::string Prefix = Candidate.m_Value.substr(0, Candidate.m_MatchOffset);
			const std::string Match = Candidate.m_Value.substr(Candidate.m_MatchOffset, Candidate.m_MatchLength);
			const std::string Suffix = Candidate.m_Value.substr(Candidate.m_MatchOffset + Candidate.m_MatchLength);
			TextRender()->TextEx(&Cursor, Prefix.c_str());
			TextRender()->TextColor(1.0f, 0.82f, 0.34f, 1.0f);
			TextRender()->TextEx(&Cursor, Match.c_str());
			TextRender()->TextColor(0.90f, 0.93f, 1.0f, 0.96f);
			TextRender()->TextEx(&Cursor, Suffix.c_str());
		}
		else
			TextRender()->TextEx(&Cursor, Candidate.m_Value.c_str());
	}

	if((int)m_vArgumentCandidates.size() > VisibleRows)
	{
		const float RailWidth = 2.0f;
		const float HandleHeight = maximum(8.0f, PopupRect.h * VisibleRows / (float)m_vArgumentCandidates.size());
		const int MaxScroll = (int)m_vArgumentCandidates.size() - VisibleRows;
		const float HandleY = PopupRect.y + (PopupRect.h - HandleHeight) * m_ArgumentCompletionScroll / maximum(1, MaxScroll);
		CUIRect Rail = {PopupRect.x + PopupRect.w - RailWidth - 1.0f, PopupRect.y + 2.0f, RailWidth, PopupRect.h - 4.0f};
		Rail.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.14f), IGraphics::CORNER_ALL, 1.0f);
		CUIRect Handle = {Rail.x, HandleY, RailWidth, HandleHeight};
		Handle.Draw(ColorRGBA(0.75f, 0.84f, 1.0f, 0.66f), IGraphics::CORNER_ALL, 1.0f);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vServerCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vServerCommands.emplace_back(pName, pParams, pHelpText);
	m_ServerCommandsNeedSorting = true;
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vServerCommands.erase(std::remove_if(m_vServerCommands.begin(), m_vServerCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vServerCommands.end());
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized)
			continue;
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		// recalculate sizes
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
		Line.m_CutOffProgress = 0.0f;
		Line.m_Presentation.m_RenderYInitialized = false;
	}
}

void CChat::ClearLines()
{
	FlushPendingConsoleLine(true);
	for(auto &Line : m_aLines)
		Line.Reset(*this);
	m_BacklogCurLine = 0;
	m_ScrollbarDragging = false;
	m_ScrollbarDragOffset = 0.0f;
	m_LastMousePos.reset();
	m_MouseIsPress = false;
	m_MousePress = vec2(0.0f, 0.0f);
	m_MouseRelease = vec2(0.0f, 0.0f);
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
	m_LastPresentationUpdateTime = 0;
	m_LargeAreaOpenTick = 0;
	m_LastPresentationShowLargeArea = false;
}

int CChat::GetLineIndex(const CLine *pLine) const
{
	if(pLine == nullptr)
		return -1;

	// 计算指针在数组中的偏移量
	const CLine *pBegin = m_aLines;
	const CLine *pEnd = pBegin + MAX_LINES;

	if(pLine < pBegin || pLine >= pEnd)
		return -1; // 指针不在数组范围内

	return static_cast<int>(pLine - pBegin);
}

CChat::CLine *CChat::GetLineByIndex(int Index)
{
	if(Index < 0 || Index >= MAX_LINES)
		return nullptr;

	return &m_aLines[Index];
}

int CChat::CountInitializedLines() const
{
	int Count = 0;
	for(const CLine &Line : m_aLines)
	{
		if(Line.m_Initialized)
			++Count;
	}
	return Count;
}

int CChat::CountVisibleLinesFrom(int BacklogLine) const
{
	const bool FocusModeActive = g_Config.m_QmFocusMode != 0;
	const bool FocusHideChat = FocusModeActive && g_Config.m_QmFocusModeHideChat;
	const bool FocusHideSystemInfoMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemInfoMessages;
	const bool FocusHideSystemPromptMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemMessages;
	const bool FocusHideEcho = FocusModeActive && g_Config.m_QmFocusModeHideEcho;

	int Count = 0;
	for(int i = BacklogLine; i < MAX_LINES; ++i)
	{
		const CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		const bool ServerMessageIsBasicInfo = Line.m_ServerMessageClass == QmHudNotifications::EServerMessageClass::BasicInfo;
		if(ShouldRenderFocusFilteredChatLine(FocusHideChat, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, FocusHideEcho, Line.m_ClientId, Line.m_ForceVisible, ServerMessageIsBasicInfo))
			++Count;
	}
	return Count;
}

void CChat::UpdatePresentationStates(int64_t Now, float DeltaSeconds, bool ShowLargeArea, bool ExtraAnimations)
{
	if(ShowLargeArea && !m_LastPresentationShowLargeArea)
		m_LargeAreaOpenTick = Now;
	else if(!ShowLargeArea)
		m_LargeAreaOpenTick = 0;
	m_LastPresentationShowLargeArea = ShowLargeArea;

	int RecallIndex = 0;
	for(int i = 0; i < MAX_LINES; ++i)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;

		const float RecallDelaySeconds = ShowLargeArea ? RecallIndex++ * CHAT_RECALL_STAGGER_SECONDS : 0.0f;
		UpdateLinePresentation(
			Line.m_Presentation,
			Line.m_Time,
			Now,
			DeltaSeconds,
			ShowLargeArea,
			Line.m_ForceVisible,
			m_LargeAreaOpenTick,
			RecallDelaySeconds,
			ExtraAnimations);
	}
}

void CChat::InvalidateLineTranslation(CLine &Line)
{
	++Line.m_TranslationId;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();
	HideArgumentCandidates();
	m_ArgumentCompletionCachedInput.clear();
	m_ArgumentCompletionCachedCursor = std::numeric_limits<size_t>::max();
	m_ArgumentCompletionSourceSignature = 0;

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = nullptr;
	m_PendingChatCounter = 0;
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_aSavedInputText[0] = '\0';
	m_SavedInputPending = false;
	m_ServerSupportsCommandInfo = false;
	m_ServerCommandsNeedSorting = false;
	m_aCurrentInputText[0] = '\0';
	m_vSlashCommandSuggestions.clear();
	DisableMode();
	m_vServerCommands.clear();

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	FlushPendingConsoleLine(true);
	m_Show = false;
	HideArgumentCandidates();
}

void CChat::OnStateChange(int NewState, int OldState)
{
	FlushPendingConsoleLine(true);
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	CChat *pChat = (CChat *)pUserData;
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		pChat->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		pChat->EnableMode(1);
	else
		pChat->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0])
	{
		pChat->m_Input.Set(pResult->GetString(1));
	}
	else if(g_Config.m_ClChatReset)
	{
		if(g_Config.m_QmChatSaveDraft && pChat->m_SavedInputPending)
		{
			pChat->m_Input.Set(pChat->m_aSavedInputText);
		}
		else
		{
			pChat->m_Input.Clear();
		}
	}

	if(!g_Config.m_QmChatSaveDraft)
	{
		pChat->m_SavedInputPending = false;
		pChat->m_aSavedInputText[0] = '\0';
	}
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::SaveDraft()
{
	if(!g_Config.m_QmChatSaveDraft)
	{
		m_SavedInputPending = false;
		m_aSavedInputText[0] = '\0';
		return;
	}

	if(m_Input.GetString()[0] != '\0')
	{
		str_copy(m_aSavedInputText, m_Input.GetString(), sizeof(m_aSavedInputText));
		m_SavedInputPending = true;
	}
	else
	{
		m_SavedInputPending = false;
		m_aSavedInputText[0] = '\0';
	}
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::Echo(const char *pString)
{
	const bool FocusHideEcho = g_Config.m_QmFocusMode != 0 && g_Config.m_QmFocusModeHideEcho;
	const unsigned EchoColor = g_Config.m_ClMessageClientColor;
	if(!FocusHideEcho && GameClient()->m_QmHudNotifications.QueueEcho(pString, EchoColor))
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "— %s", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat/client", aBuf, color_cast<ColorRGBA>(ColorHSLA(EchoColor)));
		return;
	}
	AddLine(CLIENT_MSG, 0, pString);
}

void CChat::Echo(const char *pString, bool ForceVisible)
{
	const bool FocusHideEcho = g_Config.m_QmFocusMode != 0 && g_Config.m_QmFocusModeHideEcho && !ForceVisible;
	const unsigned EchoColor = g_Config.m_ClMessageClientColor;
	if(!FocusHideEcho && GameClient()->m_QmHudNotifications.QueueEcho(pString, EchoColor))
	{
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "— %s", pString);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat/client", aBuf, color_cast<ColorRGBA>(ColorHSLA(EchoColor)));
		return;
	}
	AddLine(CLIENT_MSG, 0, pString, ForceVisible);
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

bool CChat::OnInput(const IInput::CEvent &Event)
{
	const bool ChatInputActive = m_Mode != MODE_NONE;
	if(!ChatInputActive)
		return false;

	const bool LanguageMenuOpen = m_LanguageMenuOpen || Ui()->IsPopupOpen(&m_LanguagePopupContext);
	const bool ChatLineMenuOpen = Ui()->IsPopupOpen(&m_ChatLinePopupContext);
	const bool AnyChatPopupOpen = LanguageMenuOpen || ChatLineMenuOpen;
	const bool IsWheelEvent = Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN;
	RefreshArgumentCandidates();
	if(!AnyChatPopupOpen && m_ArgumentCandidatePopup.m_RectValid && !m_vArgumentCandidates.empty())
	{
		const vec2 MousePos = GetChatMousePos();
		const int MouseIndex = ArgumentCandidateIndexAt(MousePos);
		const bool InsideCandidates = MouseIndex >= 0 || (m_ArgumentCandidatePopup.m_RectValid &&
									 MousePos.x >= m_ArgumentCandidatePopup.m_X && MousePos.x <= m_ArgumentCandidatePopup.m_X + m_ArgumentCandidatePopup.m_W &&
									 MousePos.y >= m_ArgumentCandidatePopup.m_Y && MousePos.y <= m_ArgumentCandidatePopup.m_Y + m_ArgumentCandidatePopup.m_H);

		if((Event.m_Flags & IInput::FLAG_PRESS) && IsWheelEvent && InsideCandidates)
		{
			const int Direction = Event.m_Key == KEY_MOUSE_WHEEL_UP ? -1 : 1;
			const int MaxScroll = maximum(0, (int)m_vArgumentCandidates.size() - maximum(1, m_ArgumentCandidatePopup.m_VisibleRows));
			m_ArgumentCompletionScroll = std::clamp(m_ArgumentCompletionScroll + Direction, 0, MaxScroll);
			return true;
		}
		if((Event.m_Flags & IInput::FLAG_PRESS) && (Event.m_Key == KEY_UP || Event.m_Key == KEY_DOWN))
		{
			m_ArgumentCandidateLastMousePos = GetChatMousePos();
			const int Direction = Event.m_Key == KEY_UP ? -1 : 1;
			m_ArgumentCompletionSelected = (m_ArgumentCompletionSelected + Direction + (int)m_vArgumentCandidates.size()) % (int)m_vArgumentCandidates.size();
			EnsureArgumentCandidateVisible();
			return true;
		}
		if((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_TAB)
		{
			m_ArgumentCandidateLastMousePos = GetChatMousePos();
			if(Input()->ShiftIsPressed())
			{
				m_ArgumentCompletionSelected = (m_ArgumentCompletionSelected - 1 + (int)m_vArgumentCandidates.size()) % (int)m_vArgumentCandidates.size();
				EnsureArgumentCandidateVisible();
			}
			return ApplyArgumentCandidate(m_ArgumentCompletionSelected);
		}
		if((Event.m_Flags & IInput::FLAG_PRESS) && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
		{
			m_ArgumentCandidateLastMousePos = GetChatMousePos();
			return ApplyArgumentCandidate(m_ArgumentCompletionSelected);
		}

		if(Event.m_Key == KEY_MOUSE_1)
		{
			if((Event.m_Flags & IInput::FLAG_PRESS) && MouseIndex >= 0)
			{
				m_ArgumentCompletionSelected = MouseIndex;
				m_ArgumentCandidatePopup.m_PressedIndex = MouseIndex;
				CLineInput::SMouseSelection *pMouseSelection = m_Input.GetMouseSelection();
				if(pMouseSelection != nullptr)
					pMouseSelection->m_Selecting = false;
				return true;
			}
			if((Event.m_Flags & IInput::FLAG_RELEASE) && m_ArgumentCandidatePopup.m_PressedIndex >= 0)
			{
				const int PressedIndex = m_ArgumentCandidatePopup.m_PressedIndex;
				m_ArgumentCandidatePopup.m_PressedIndex = -1;
				if(MouseIndex == PressedIndex)
					ApplyArgumentCandidate(PressedIndex);
				m_Input.Activate(EInputPriority::CHAT);
				return true;
			}
		}
	}
	if(!AnyChatPopupOpen && (Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_TAB && !m_ArgumentCandidatePopup.m_RectValid)
	{
		if(!m_vArgumentCandidates.empty())
			return true;
		QmChatCompletion::SContext TabContext;
		if(QmChatCompletion::ParsePlayerTabContext(m_Input.GetString(), m_Input.GetCursorOffset(), TabContext))
		{
			m_ArgumentCandidatesRequestedByTab = true;
			m_ArgumentCompletionCachedCursor = std::numeric_limits<size_t>::max();
			RefreshArgumentCandidates();
			if(!m_vArgumentCandidates.empty())
				return true;
			m_ArgumentCandidatesRequestedByTab = false;
		}
	}
	if(!AnyChatPopupOpen && (Event.m_Flags & IInput::FLAG_PRESS) && IsWheelEvent)
	{
		const float Height = 300.0f;
		const float Width = Height * Graphics()->ScreenAspect();
		const bool ChatAnchoredRight = true;
		const bool ChatScrollbarOnRight = ChatAnchoredRight;
		const CUIRect ChatRect = {0.0f, 50.0f, std::min(Width, std::max(190.0f, g_Config.m_ClChatWidth + 32.0f)), 250.0f};
		float HistoryBottom = Height - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));
		HistoryBottom -= FontSize() * (8.0f / 6.0f);
		const float HeightLimit = GameClient()->m_Scoreboard.IsActive() ? 180.0f : (m_PrevShowChat ? 50.0f : 200.0f);
		const vec2 MousePos = GetChatMousePos();
		const bool InsideHistory = MousePos.x >= ChatRect.x && MousePos.x <= ChatRect.x + ChatRect.w && MousePos.y >= HeightLimit && MousePos.y <= HistoryBottom;
		const bool InsideTranslateButton =
			m_TranslateButton.m_RectValid &&
			MousePos.x >= m_TranslateButton.m_X &&
			MousePos.x <= m_TranslateButton.m_X + m_TranslateButton.m_W &&
			MousePos.y >= m_TranslateButton.m_Y &&
			MousePos.y <= m_TranslateButton.m_Y + m_TranslateButton.m_H;
		if(InsideHistory && !InsideTranslateButton && !m_ScrollbarDragging)
		{
			const int TotalLines = CountInitializedLines();
			const int Direction = Event.m_Key == KEY_MOUSE_WHEEL_UP ? 1 : -1;
			m_BacklogCurLine = ClampBacklogLine(m_BacklogCurLine + Direction, TotalLines, 1);
			RebuildChat();
			return true;
		}
	}

	// ===== 翻译按钮处理（优先级高于输入框）=====
	if(!AnyChatPopupOpen && m_TranslateButton.m_RectValid)
	{
		const vec2 MousePos = GetChatMousePos();
		const bool InsideButton =
			MousePos.x >= m_TranslateButton.m_X &&
			MousePos.x <= m_TranslateButton.m_X + m_TranslateButton.m_W &&
			MousePos.y >= m_TranslateButton.m_Y &&
			MousePos.y <= m_TranslateButton.m_Y + m_TranslateButton.m_H;

		// 左键处理：翻译可见聊天；没有可翻译行时打开语言菜单
		if(Event.m_Key == KEY_MOUSE_1)
		{
			if(Event.m_Flags & IInput::FLAG_PRESS)
			{
				m_TranslateButton.m_IsPressed = InsideButton;
				if(InsideButton)
				{
					// 重置输入框的鼠标选择状态
					CLineInput::SMouseSelection *pMouseSel = m_Input.GetMouseSelection();
					if(pMouseSel)
					{
						pMouseSel->m_Selecting = false;
						pMouseSel->m_PressMouse = vec2(0, 0);
						pMouseSel->m_ReleaseMouse = vec2(0, 0);
					}
					return true;
				}
			}
			else if(Event.m_Flags & IInput::FLAG_RELEASE)
			{
				const bool Activate = m_TranslateButton.m_IsPressed && InsideButton;
				m_TranslateButton.m_IsPressed = false;
				if(Activate)
				{
					if(!TranslateVisibleChatLines())
						OpenLanguageMenu();
					return true;
				}
			}
		}

		// 右键处理：切换自动翻译
		if(Event.m_Key == KEY_MOUSE_2)
		{
			if((Event.m_Flags & IInput::FLAG_PRESS) && InsideButton)
			{
				ToggleAutoTranslate();
				return true;
			}
		}
	}

	// 聊天弹窗打开时，键盘确认/取消只作用于弹窗，不能穿透到聊天提交/关闭。
	if(AnyChatPopupOpen && (Event.m_Flags & IInput::FLAG_PRESS) && (Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(LanguageMenuOpen)
			CloseLanguageMenu();
		if(ChatLineMenuOpen)
			CloseChatLineMenu();
		return true;
	}

	// ESC 键处理：优先关闭弹出菜单
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		if(m_LanguageMenuOpen || Ui()->IsPopupOpen(&m_LanguagePopupContext))
		{
			CloseLanguageMenu();
			return true;
		}
		if(Ui()->IsPopupOpen(&m_ChatLinePopupContext))
		{
			CloseChatLineMenu();
			return true;
		}

		m_vSlashCommandSuggestions.clear();
		DisableMode();
		GameClient()->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			SaveDraft();
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
		else if(!g_Config.m_QmChatSaveDraft)
		{
			m_SavedInputPending = false;
			m_aSavedInputText[0] = '\0';
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_ServerCommandsNeedSorting)
		{
			std::sort(m_vServerCommands.begin(), m_vServerCommands.end());
			m_ServerCommandsNeedSorting = false;
		}

		if(GameClient()->m_BindChat.ChatDoBinds(m_Input.GetString()))
			; // Do nothing as bindchat was executed
		else if(GameClient()->m_TClient.ChatDoSpecId(m_Input.GetString()))
			; // Do nothing as specid was executed
		else
			SendChatQueued(m_Input.GetString());
		m_SavedInputPending = false;
		m_aSavedInputText[0] = '\0';
		m_pHistoryEntry = nullptr;
		m_vSlashCommandSuggestions.clear();
		DisableMode();
		GameClient()->OnRelease();
		m_Input.Clear();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		m_SlashCommandSuggestionsDismissed = true;
		str_copy(m_aSlashCommandSuggestionsDismissedInput, m_Input.GetString(), sizeof(m_aSlashCommandSuggestionsDismissedInput));
		m_vSlashCommandSuggestions.clear();
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : GameClient()->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = GameClient()->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != nullptr)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &Player1, const CRateablePlayer &Player2) -> bool {
					return Player1.m_Score < Player2.m_Score;
				});
		}

		if(m_aCompletionBuffer[0] == '/' && !m_vServerCommands.empty())
		{
			CCommand *pCompletionCommand = nullptr;

			const size_t NumCommands = m_vServerCommands.size();

			if(ShiftPressed && m_CompletionUsed)
				m_CompletionChosen--;
			else if(!ShiftPressed)
				m_CompletionChosen++;
			m_CompletionChosen = (m_CompletionChosen + 2 * NumCommands) % (2 * NumCommands);

			m_CompletionUsed = true;

			const char *pCommandStart = m_aCompletionBuffer + 1;
			for(size_t i = 0; i < 2 * NumCommands; ++i)
			{
				int SearchType;
				int Index;

				if(ShiftPressed)
				{
					SearchType = ((m_CompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen - i + NumCommands) % NumCommands;
				}
				else
				{
					SearchType = ((m_CompletionChosen + i) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen + i) % NumCommands;
				}

				auto &Command = m_vServerCommands[Index];

				if(str_startswith_nocase(Command.m_aName, pCommandStart))
				{
					pCompletionCommand = &Command;
					m_CompletionChosen = Index + SearchType * NumCommands;
					break;
				}
			}

			// insert the command
			if(pCompletionCommand)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// add the command
				str_append(aBuf, "/");
				str_append(aBuf, pCompletionCommand->m_aName);

				// add separator
				const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
				str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName) + 1;
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
		else
		{
			// find next possible name
			const char *pCompletionString = nullptr;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &GameClient()->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].m_ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if(m_Input.GetString()[0] == '/' && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
		RefreshSlashCommandSuggestions();
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
			{
				m_pHistoryEntry = pTest;
			}
		}
		else
		{
			m_pHistoryEntry = m_History.Last();
		}

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(m_Mode == MODE_NONE)
	{
		if(Team)
			m_Mode = MODE_TEAM;
		else
			m_Mode = MODE_ALL;

		Input()->Clear();
		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_Input.Activate(EInputPriority::CHAT);
	}
}

void CChat::DisableMode()
{
	CloseLanguageMenu();
	CloseChatLineMenu();
	HideArgumentCandidates();

	if(m_Mode != MODE_NONE)
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		auto &Re = GameClient()->m_TClient.m_RegexChatIgnore;
		if(Re.error().empty() && Re.test(pMsg->m_pMessage))
			return;

		if(pMsg->m_ClientId == SERVER_MSG && g_Config.m_ClShowChatSystem)
		{
			const auto PrintSuppressedServerMessage = [this, pMsg]() {
				if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
					StoreSave(pMsg->m_pMessage);
				char aBuf[1024];
				str_copy(aBuf, pMsg->m_pMessage);
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat/server", aBuf, color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor)));
			};
			const bool FocusModeActive = g_Config.m_QmFocusMode != 0;
			const bool FocusHideSystemInfoMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemInfoMessages;
			const bool FocusHideSystemPromptMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemMessages;
			QmHudNotifications::SServerMessageAnalysis ServerMessageAnalysis;
			const bool ServerMessageHandled = GameClient()->m_QmHudNotifications.HandleServerChat(pMsg->m_pMessage, g_Config.m_QmHudNotificationsSystem != 0, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, &ServerMessageAnalysis);
			if(ServerMessageHandled && QmHudNotifications::ShouldSuppressServerMessageChat(ServerMessageAnalysis, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages))
			{
				PrintSuppressedServerMessage();
				return;
			}
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage, false, ServerMessageAnalysis.m_Class);
		}
		else
		{
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
		}

		SaveChatLogLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pMsg->m_ClientId == SERVER_MSG)
		{
			StoreSave(pMsg->m_pMessage);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vServerCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

static constexpr const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

// TODO: remove this in a few releases (in 2027 or later)
//       it got deprecated by CGameClient::StoreSave
void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		Client()->GetCurrentMap(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

bool CChat::EnsureChatLogFolder() const
{
	if(!Storage()->CreateFolder("qmclient", IStorage::TYPE_SAVE) && !Storage()->FolderExists("qmclient", IStorage::TYPE_SAVE))
	{
		log_error("chat", "Failed to create chat log root folder");
		return false;
	}
	if(!Storage()->CreateFolder(QM_CHAT_LOG_DIR, IStorage::TYPE_SAVE) && !Storage()->FolderExists(QM_CHAT_LOG_DIR, IStorage::TYPE_SAVE))
	{
		log_error("chat", "Failed to create chat log folder '%s'", QM_CHAT_LOG_DIR);
		return false;
	}
	return true;
}

void CChat::CleanupOldChatLogs(const char *pToday)
{
	if(g_Config.m_QmChatLogKeepDays <= 0 || str_comp(m_aChatLogLastCleanupDate, pToday) == 0)
		return;

	time_t TodayDate = 0;
	if(!timestamp_from_str(pToday, "%Y-%m-%d", &TodayDate))
		return;

	SChatLogCleanupData Data;
	Data.m_pStorage = Storage();
	Data.m_CutoffDate = TodayDate - (time_t)maximum(g_Config.m_QmChatLogKeepDays - 1, 0) * 24 * 60 * 60;
	Storage()->ListDirectory(IStorage::TYPE_SAVE, QM_CHAT_LOG_DIR, ChatLogCleanupCallback, &Data);
	str_copy(m_aChatLogLastCleanupDate, pToday);
}

void CChat::SaveChatLogLine(int ClientId, int Team, const char *pLine)
{
	if(!g_Config.m_QmChatLogAutoSave || Client()->State() == IClient::STATE_DEMOPLAYBACK || pLine == nullptr || pLine[0] == '\0')
		return;
	if(!EnsureChatLogFolder())
		return;

	char aDate[11];
	str_timestamp_format(aDate, sizeof(aDate), "%Y-%m-%d");
	CleanupOldChatLogs(aDate);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), FORMAT_SPACE);

	char aName[MAX_NAME_LENGTH];
	if(ClientId == SERVER_MSG)
	{
		str_copy(aName, "server");
	}
	else if(ClientId == CLIENT_MSG)
	{
		str_copy(aName, "client");
	}
	else if(ClientId >= 0 && ClientId < MAX_CLIENTS && GameClient()->m_aClients[ClientId].m_aName[0] != '\0')
	{
		GameClient()->FormatStreamerName(ClientId, aName, sizeof(aName));
	}
	else
	{
		str_format(aName, sizeof(aName), "client %d", ClientId);
	}
	str_sanitize_cc(aName);

	char aText[MAX_LINE_LENGTH];
	str_copy(aText, pLine);
	str_sanitize_cc(aText);

	char aFilename[IO_MAX_PATH_LENGTH];
	str_format(aFilename, sizeof(aFilename), "%s/%s%s%s", QM_CHAT_LOG_DIR, QM_CHAT_LOG_PREFIX, aDate, QM_CHAT_LOG_EXTENSION);
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("chat", "Failed to open chat log '%s'", aFilename);
		return;
	}

	char aLine[512];
	if(ClientId == SERVER_MSG || ClientId == CLIENT_MSG)
		str_format(aLine, sizeof(aLine), "[%s] [%s] %s", aTimestamp, ChatLogKind(ClientId, Team), aText);
	else
		str_format(aLine, sizeof(aLine), "[%s] [%s] %s: %s", aTimestamp, ChatLogKind(ClientId, Team), aName, aText);

	io_write(File, aLine, str_length(aLine));
	io_write_newline(File);
	io_close(File);
}

void CChat::PrintBlockedMessageToConsole(int ClientId, int Team, const char *pLine)
{
	char aName[64] = "";
	bool Highlighted = false;
	const char *pFrom = "chat/all";
	ColorRGBA ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));

	if(ClientId == SERVER_MSG)
	{
		str_copy(aName, "*** ");
		pFrom = "chat/server";
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	}
	else if(ClientId == CLIENT_MSG)
	{
		str_copy(aName, "— ");
		pFrom = "chat/client";
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	}
	else
	{
		const CGameClient::CClientData &LineAuthor = GameClient()->m_aClients[ClientId];
		char aDisplayName[MAX_NAME_LENGTH];
		GameClient()->FormatStreamerName(ClientId, aDisplayName, sizeof(aDisplayName));

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK && !GameClient()->IsLocalClientId(ClientId))
		{
			for(int LocalId : GameClient()->m_aLocalIds)
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
		}
		else if(Client()->State() == IClient::STATE_DEMOPLAYBACK && ClientId != GameClient()->m_Snap.m_LocalClientId)
		{
			const int LocalId = GameClient()->m_Snap.m_LocalClientId;
			Highlighted = LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
		}

		if(Team == TEAM_WHISPER_SEND || Team == TEAM_WHISPER_RECV)
		{
			str_copy(aName, Team == TEAM_WHISPER_SEND ? "→" : "←");
			if(LineAuthor.m_Active)
			{
				str_append(aName, " ");
				str_append(aName, aDisplayName);
			}
			Highlighted = Team == TEAM_WHISPER_RECV;
			pFrom = "chat/whisper";
		}
		else
		{
			str_copy(aName, aDisplayName);
			if(Team == 1)
				pFrom = "chat/team";
		}

		const bool Friend = LineAuthor.m_Active && LineAuthor.m_Friend;
		if(Highlighted)
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Friend && g_Config.m_ClMessageFriend)
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
		else if(Team == 1)
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	}

	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "%s%s%s", aName, ClientId >= 0 ? ": " : "", pLine);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor);
}

ColorRGBA CChat::PlayerNameColor(int ClientId, int NameColor, bool TeamMessage) const
{
	if(ClientId >= 0 && g_Config.m_TcWarList && g_Config.m_TcWarListChat && GameClient()->m_WarList.GetAnyWar(ClientId))
		return GameClient()->m_WarList.GetPriorityColor(ClientId);
	if(TeamMessage)
		return CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
	if(NameColor == TEAM_RED)
		return ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
	if(NameColor == TEAM_BLUE)
		return ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
	if(NameColor == TEAM_SPECTATORS)
		return ColorRGBA(0.75f, 0.5f, 0.75f, 1.0f);
	if(ClientId >= 0 && g_Config.m_ClChatTeamColors && GameClient()->m_Teams.Team(ClientId))
		return GameClient()->GetDDTeamColor(GameClient()->m_Teams.Team(ClientId), 0.75f);
	return ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);
}

void CChat::AddMergedAuthor(CLine &Line, int ClientId)
{
	for(const SMergedAuthor &Author : Line.m_vMergedAuthors)
	{
		if(Author.m_ClientId == ClientId)
			return;
	}

	SMergedAuthor Author;
	Author.m_ClientId = ClientId;
	GameClient()->FormatStreamerName(ClientId, Author.m_aName, sizeof(Author.m_aName));
	str_copy(Author.m_aPlayerName, GameClient()->m_aClients[ClientId].m_aName);

	int NameColor = -2;
	const CGameClient::CClientData &LineAuthor = GameClient()->m_aClients[ClientId];
	if(LineAuthor.m_Active)
	{
		if(LineAuthor.m_Team == TEAM_SPECTATORS)
			NameColor = TEAM_SPECTATORS;
		if(GameClient()->IsTeamPlay())
		{
			if(LineAuthor.m_Team == TEAM_RED)
				NameColor = TEAM_RED;
			else if(LineAuthor.m_Team == TEAM_BLUE)
				NameColor = TEAM_BLUE;
		}
	}
	Author.m_NameColor = PlayerNameColor(ClientId, NameColor, false);
	Line.m_vMergedAuthors.push_back(Author);
	RebuildMergedAuthorName(Line);
}

void CChat::RebuildMergedAuthorName(CLine &Line)
{
	Line.m_aName[0] = '\0';
	for(size_t i = 0; i < Line.m_vMergedAuthors.size(); ++i)
	{
		if(i > 0)
			str_append(Line.m_aName, ",", sizeof(Line.m_aName));
		str_append(Line.m_aName, Line.m_vMergedAuthors[i].m_aName, sizeof(Line.m_aName));
	}
}

void CChat::PrintLineToConsole(const CLine &Line) const
{
	if(Line.m_ConsoleSuppressed)
		return;

	ColorRGBA ChatLogColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	if(Line.m_Highlighted)
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
	else if(Line.m_Friend && g_Config.m_ClMessageFriend)
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
	else if(Line.m_Team)
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
	else if(Line.m_ClientId == SERVER_MSG)
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
	else if(Line.m_ClientId == CLIENT_MSG)
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
	else
		ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));

	const char *pFrom;
	if(Line.m_Whisper)
		pFrom = "chat/whisper";
	else if(Line.m_Team)
		pFrom = "chat/team";
	else if(Line.m_ClientId == SERVER_MSG)
		pFrom = "chat/server";
	else if(Line.m_ClientId == CLIENT_MSG)
		pFrom = "chat/client";
	else
		pFrom = "chat/all";

	char aBuf[4096] = "";
	const bool Merged = Line.m_TimesRepeated > 0 && !Line.m_vMergedAuthors.empty();
	if(!Merged)
	{
		str_format(aBuf, sizeof(aBuf), "%s%s%s", Line.m_aName, Line.m_ClientId >= 0 ? ": " : "", Line.m_aText);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor);
		return;
	}

	std::vector<CGameConsole::SColorSpan> vColorSpans;
	vColorSpans.reserve(Line.m_vMergedAuthors.size());
	for(size_t i = 0; i < Line.m_vMergedAuthors.size(); ++i)
	{
		if(i > 0)
			str_append(aBuf, ",", sizeof(aBuf));
		const size_t StartByte = str_length(aBuf);
		str_append(aBuf, Line.m_vMergedAuthors[i].m_aName, sizeof(aBuf));
		const size_t EndByte = str_length(aBuf);
		const int StartChar = (int)str_utf8_offset_bytes_to_chars(aBuf, StartByte);
		const int EndChar = (int)str_utf8_offset_bytes_to_chars(aBuf, EndByte);
		vColorSpans.push_back({StartChar, EndChar - StartChar, Line.m_vMergedAuthors[i].m_NameColor});
	}
	char aCount[16];
	str_format(aCount, sizeof(aCount), " [%d]: ", Line.m_TimesRepeated + 1);
	str_append(aBuf, aCount, sizeof(aBuf));
	str_append(aBuf, Line.m_aText, sizeof(aBuf));
	GameClient()->m_GameConsole.PrintLineWithColorSpans(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor, vColorSpans.data(), vColorSpans.size());
}

void CChat::FlushPendingConsoleLine(bool Force)
{
	if(m_PendingConsoleLineIndex < 0 || m_PendingConsoleLineIndex >= MAX_LINES)
		return;
	CLine &Line = m_aLines[m_PendingConsoleLineIndex];
	if(!Line.m_Initialized)
	{
		m_PendingConsoleLineIndex = -1;
		return;
	}
	const int64_t Now = time();
	if(!Force && g_Config.m_QmMessageMerge && Now >= Line.m_Time && Now - Line.m_Time <= time_freq() * 2)
		return;
	PrintLineToConsole(Line);
	m_PendingConsoleLineIndex = -1;
}

void CChat::AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible)
{
	AddLine(ClientId, Team, pLine, ForceVisible, std::nullopt);
}

void CChat::AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible, std::optional<QmHudNotifications::EServerMessageClass> KnownServerMessageClass)
{
	if(ClientId >= 0 && !GameClient()->LiveTeamFilterAllowsClient(ClientId))
		return;

	if(*pLine == 0 ||
		(ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_aName[0] == '\0' || // unknown client
					  GameClient()->m_aClients[ClientId].m_ChatIgnore ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_QmWarListBlockEnemyChat && GameClient()->m_WarList.IsEnemy(ClientId)) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && GameClient()->m_aClients[ClientId].m_Foe))))
		return;

	// TClient
	if(ClientId == CLIENT_MSG && !g_Config.m_TcShowChatClient)
		return;

	char aFilteredLine[MAX_LINE_LENGTH];
	const char *pFilteredLine = pLine;
	std::vector<std::string> BlockedWords;
	bool BlockWordsConsolePrinted = false;
	const EBlockWordsAction BlockWordsAction = static_cast<EBlockWordsAction>(g_Config.m_QmBlockWordsAction);
	bool IsLocalBlockWordsClient = GameClient()->IsLocalClientId(ClientId);
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		IsLocalBlockWordsClient = ClientId == GameClient()->m_Snap.m_LocalClientId;
	const bool CanHideBlockWordsMessage = ShouldHideBlockWordsMessage(BlockWordsAction, true, ClientId, IsLocalBlockWordsClient, Team);
	if(g_Config.m_QmBlockWordsEnabled && g_Config.m_QmBlockWordsList[0] != '\0' &&
		(BlockWordsAction == EBlockWordsAction::REPLACE || CanHideBlockWordsMessage))
	{
		std::string Text = pLine;
		std::vector<std::string> *pMatched = g_Config.m_QmBlockWordsShowConsole ? &BlockedWords : nullptr;
		if(ApplyBlockWords(Text, pMatched))
		{
			FlushPendingConsoleLine(true);
			if(g_Config.m_QmBlockWordsShowConsole && !BlockedWords.empty())
			{
				std::string Joined;
				for(size_t i = 0; i < BlockedWords.size(); ++i)
				{
					if(i > 0)
						Joined.append(", ");
					Joined.append(BlockedWords[i]);
				}
				char aBuf[512];
				str_format(aBuf, sizeof(aBuf), "屏蔽词: %s", Joined.c_str());
				const ColorRGBA LogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmBlockWordsConsoleColor));
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "chat/blocklist", aBuf, LogColor);
			}
			PrintBlockedMessageToConsole(ClientId, Team, pLine);
			BlockWordsConsolePrinted = true;
			if(CanHideBlockWordsMessage)
			{
				return;
			}
			str_copy(aFilteredLine, Text.c_str(), sizeof(aFilteredLine));
			pFilteredLine = aFilteredLine;
		}
	}
	pLine = pFilteredLine;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = nullptr;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == nullptr)
		{
			pEnd = pStrOld;
		}

		if(++Length >= MAX_LINE_LENGTH)
		{
			*(const_cast<char *>(pStr)) = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*(const_cast<char *>(pEnd)) = '\0';

	if(*pLine == 0)
		return;

	bool Highlighted = false;

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));

	CLine &PreviousLine = m_aLines[m_CurrentLine];
	const int64_t Now = time();
	FlushPendingConsoleLine(false);

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	if(g_Config.m_QmMessageMerge &&
		PreviousLine.m_Initialized &&
		(PreviousLine.m_ConsoleSuppressed || m_PendingConsoleLineIndex == m_CurrentLine) &&
		PreviousLine.m_CustomColor == CustomColor &&
		PreviousLine.m_ForceVisible == ForceVisible &&
		PreviousLine.m_ConsoleSuppressed == BlockWordsConsolePrinted &&
		CanMergePlayerMessages(PreviousLine.m_ClientId, PreviousLine.m_TeamNumber, PreviousLine.m_aText, PreviousLine.m_Time, ClientId, Team, pLine, Now))
	{
		const int PreviousTeam = PreviousLine.m_TeamNumber;
		PreviousLine.m_TimesRepeated++;
		AddMergedAuthor(PreviousLine, ClientId);
		if(PreviousTeam != Team)
		{
			PreviousLine.m_Team = false;
			PreviousLine.m_TeamNumber = 0;
		}
		if(PreviousLine.m_vMergedAuthors.size() > 1)
		{
			PreviousLine.m_Friend = false;
			PreviousLine.m_pManagedTeeRenderInfo = nullptr;
		}
		PreviousLine.m_ConsoleSuppressed |= BlockWordsConsolePrinted;
		TextRender()->DeleteTextContainer(PreviousLine.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(PreviousLine.m_QuadContainerIndex);
		PreviousLine.m_Time = Now;
		PreviousLine.m_aYOffset[0] = -1.0f;
		PreviousLine.m_aYOffset[1] = -1.0f;
		PreviousLine.m_CutOffProgress = 0.0f;
		BeginLinePresentation(PreviousLine.m_Presentation, PreviousLine.m_Time, true);

		CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		str_copy(ClientData.m_aChatBubbleText, pLine, sizeof(ClientData.m_aChatBubbleText));
		ClientData.m_ChatBubbleStartTick = Now;
		ClientData.m_ChatBubbleExpireTick = Now + time_freq() * g_Config.m_QmChatBubbleDuration;
		return;
	}

	FlushPendingConsoleLine(true);

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;
	if(m_BacklogCurLine > 0)
		m_BacklogCurLine = ClampBacklogLine(m_BacklogCurLine + 1, CountInitializedLines() + 1, 1);

	CLine &CurrentLine = m_aLines[m_CurrentLine];
	CurrentLine.Reset(*this);
	CurrentLine.m_Initialized = true;
	CurrentLine.m_Time = Now;
	BeginLinePresentation(CurrentLine.m_Presentation, CurrentLine.m_Time, false);
	CurrentLine.m_aYOffset[0] = -1.0f;
	CurrentLine.m_aYOffset[1] = -1.0f;
	CurrentLine.m_ClientId = ClientId;
	CurrentLine.m_TeamNumber = Team;
	CurrentLine.m_Team = Team == 1;
	CurrentLine.m_Whisper = Team >= 2;
	CurrentLine.m_NameColor = -2;
	CurrentLine.m_CustomColor = CustomColor;
	CurrentLine.m_ForceVisible = ForceVisible;
	CurrentLine.m_ConsoleSuppressed = BlockWordsConsolePrinted;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && ClientId != GameClient()->m_aLocalIds[1])
		{
			for(int LocalId : GameClient()->m_aLocalIds)
			{
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
			}
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}
	CurrentLine.m_Highlighted = Highlighted;

	str_copy(CurrentLine.m_aText, pLine);
	CurrentLine.m_ServerMessageClass = ResolveLineServerMessageClass(ClientId, CurrentLine.m_aText, KnownServerMessageClass);

	if(CurrentLine.m_ClientId == SERVER_MSG)
	{
		str_copy(CurrentLine.m_aName, MessageNamePrefixForClientId(CurrentLine.m_ClientId, g_Config.m_QmChatHideSystemPrefix != 0));
	}
	else if(CurrentLine.m_ClientId == CLIENT_MSG)
	{
		str_copy(CurrentLine.m_aName, MessageNamePrefixForClientId(CurrentLine.m_ClientId));
	}
	else
	{
		const auto &LineAuthor = GameClient()->m_aClients[CurrentLine.m_ClientId];
		char aDisplayName[MAX_NAME_LENGTH];
		GameClient()->FormatStreamerName(CurrentLine.m_ClientId, aDisplayName, sizeof(aDisplayName));

		if(LineAuthor.m_Active)
		{
			if(LineAuthor.m_Team == TEAM_SPECTATORS)
				CurrentLine.m_NameColor = TEAM_SPECTATORS;

			if(GameClient()->IsTeamPlay())
			{
				if(LineAuthor.m_Team == TEAM_RED)
					CurrentLine.m_NameColor = TEAM_RED;
				else if(LineAuthor.m_Team == TEAM_BLUE)
					CurrentLine.m_NameColor = TEAM_BLUE;
			}
		}

		if(Team == TEAM_WHISPER_SEND)
		{
			str_copy(CurrentLine.m_aName, "→");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, aDisplayName);
			}
			CurrentLine.m_NameColor = TEAM_BLUE;
			CurrentLine.m_Highlighted = false;
			Highlighted = false;
		}
		else if(Team == TEAM_WHISPER_RECV)
		{
			str_copy(CurrentLine.m_aName, "←");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, aDisplayName);
			}
			CurrentLine.m_NameColor = TEAM_RED;
			CurrentLine.m_Highlighted = true;
			Highlighted = true;
		}
		else
		{
			str_copy(CurrentLine.m_aName, aDisplayName);
		}

		if(LineAuthor.m_Active)
		{
			CurrentLine.m_Friend = LineAuthor.m_Friend;
			CurrentLine.m_pManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(LineAuthor);
		}

		if(Team < TEAM_WHISPER_SEND)
			AddMergedAuthor(CurrentLine, ClientId);
	}

	if(g_Config.m_QmMessageMerge && ClientId >= 0 && Team < TEAM_WHISPER_SEND && !CurrentLine.m_ConsoleSuppressed)
		m_PendingConsoleLineIndex = m_CurrentLine;
	else
		PrintLineToConsole(CurrentLine);

	// play sound
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", CurrentLine.m_aName, CurrentLine.m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = CurrentLine.m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
			}
#endif
			if(PlaySound)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}

	// Set chat bubble for player
	if(ClientId >= 0 && ClientId < MAX_CLIENTS)
	{
		CGameClient::CClientData &ClientData = GameClient()->m_aClients[ClientId];
		str_copy(ClientData.m_aChatBubbleText, pLine, sizeof(ClientData.m_aChatBubbleText));
		const int64_t BubbleStartTick = time();
		ClientData.m_ChatBubbleStartTick = BubbleStartTick;
		ClientData.m_ChatBubbleExpireTick = BubbleStartTick + time_freq() * g_Config.m_QmChatBubbleDuration;
	}

	// TClient
	GameClient()->m_Translate.AutoTranslate(CurrentLine);
}

void CChat::OnPrepareLines(float y)
{
	float x = 5.0f;
	float FontSize = this->FontSize();
	const bool FocusModeActive = g_Config.m_QmFocusMode != 0;
	const bool FocusHideChat = FocusModeActive && g_Config.m_QmFocusModeHideChat;
	const bool FocusHideSystemInfoMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemInfoMessages;
	const bool FocusHideSystemPromptMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemMessages;
	const bool FocusHideEcho = FocusModeActive && g_Config.m_QmFocusModeHideEcho;

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive();
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat;
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	float LineWidth = (IsScoreBoardOpen ? maximum(85.0f, (FontSize * 85.0f / 6.0f)) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	const float HeightLimit =
		IsScoreBoardOpen ?
			CHAT_HEIGHT_MIN + 130.0f :
			(ShowLargeArea ? CHAT_HEIGHT_MIN : CHAT_HEIGHT_FULL);

	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	for(int i = m_BacklogCurLine; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		const bool ServerMessageIsBasicInfo = Line.m_ServerMessageClass == QmHudNotifications::EServerMessageClass::BasicInfo;
		if(!ShouldRenderFocusFilteredChatLine(FocusHideChat, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, FocusHideEcho, Line.m_ClientId, Line.m_ForceVisible, ServerMessageIsBasicInfo))
		{
			continue;
		}
		if(!ShowLargeArea && !Line.m_ForceVisible && Line.m_Presentation.m_State == EPresentationState::COLLAPSED)
		{
			continue;
		}

		if(Line.m_TextContainerIndex.Valid() && !ForceRecreate)
		{
			// 已有容器也必须消耗相同的垂直预算，
			// 否则只有“首次创建”时受高度限制，后续帧又会溢出。
			if(Line.m_aYOffset[OffsetType] >= 0.0f)
			{
				y -= Line.m_aYOffset[OffsetType] * ClampPresentationProgress(Line.m_Presentation.m_LayoutVisibility);

				if(y < HeightLimit)
					break;
			}
			continue;
		}

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		const bool MergedPlayerMessages = Line.m_TimesRepeated > 0 && !Line.m_vMergedAuthors.empty();
		const bool MultipleAuthors = Line.m_vMergedAuthors.size() > 1;

		char aClientId[16] = "";
		if(!MultipleAuthors && g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0' && !GameClient()->ShouldHideStreamerIdentity(Line.m_ClientId))
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load *** *** ***' to continue";
			}
		}

		const CColoredParts ColoredParts(pText, Line.m_ClientId == CLIENT_MSG);
		if(!ColoredParts.Colors().empty() && ColoredParts.Colors()[0].m_Index == 0)
			Line.m_CustomColor = ColoredParts.Colors()[0].m_Color;
		pText = ColoredParts.Text();

		const char *pTranslatedError = nullptr;
		const char *pTranslatedText = nullptr;
		const char *pTranslatedLanguage = nullptr;
		if(Line.m_pTranslateResponse != nullptr && Line.m_pTranslateResponse->m_Text[0])
		{
			// If hidden and there is translated text
			if(pText != Line.m_aText)
			{
				pTranslatedError = Localize("Translated text hidden due to streamer mode");
			}
			else if(Line.m_pTranslateResponse->m_Error)
			{
				pTranslatedError = Line.m_pTranslateResponse->m_Text;
			}
			else
			{
				pTranslatedText = Line.m_pTranslateResponse->m_Text;
				if(Line.m_pTranslateResponse->m_Language[0] != '\0')
					pTranslatedLanguage = Line.m_pTranslateResponse->m_Language;
			}
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			CTextCursor MeasureCursor;
			MeasureCursor.SetPosition(vec2(TextBegin, 0.0f));
			MeasureCursor.m_FontSize = FontSize;
			MeasureCursor.m_Flags = 0;
			MeasureCursor.m_LineWidth = LineWidth;

			if(!MultipleAuthors && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				MeasureCursor.m_X += RealMsgPaddingTee;

				if(Line.m_Friend && g_Config.m_ClMessageFriend)
				{
					TextRender()->TextEx(&MeasureCursor, "♥ ");
				}
			}

			TextRender()->TextEx(&MeasureCursor, aClientId);
			if(MergedPlayerMessages)
			{
				for(size_t i = 0; i < Line.m_vMergedAuthors.size(); ++i)
				{
					if(i > 0)
						TextRender()->TextEx(&MeasureCursor, ",");
					TextRender()->TextEx(&MeasureCursor, Line.m_vMergedAuthors[i].m_aName);
				}
			}
			else
			{
				TextRender()->TextEx(&MeasureCursor, Line.m_aName);
			}
			if(Line.m_TimesRepeated > 0)
				TextRender()->TextEx(&MeasureCursor, aCount);

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&MeasureCursor, ": ");
			}

			CTextCursor AppendCursor = MeasureCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = MeasureCursor.m_X;
				AppendCursor.m_LineWidth -= MeasureCursor.m_LongestLineWidth;
			}

			if(pTranslatedText)
			{
				TextRender()->TextEx(&AppendCursor, pTranslatedText);
				if(pTranslatedLanguage)
				{
					TextRender()->TextEx(&AppendCursor, " [");
					TextRender()->TextEx(&AppendCursor, pTranslatedLanguage);
					TextRender()->TextEx(&AppendCursor, "]");
				}
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pText);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else if(pTranslatedError)
			{
				TextRender()->TextEx(&AppendCursor, pText);
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pTranslatedError);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else
			{
				TextRender()->TextEx(&AppendCursor, pText);
			}

			Line.m_aYOffset[OffsetType] = AppendCursor.Height() + RealMsgPaddingY;
		}

		const float LineHeight = Line.m_aYOffset[OffsetType];
		const float LayoutVisibility = ClampPresentationProgress(Line.m_Presentation.m_LayoutVisibility);
		const float LayoutBottom = y;
		y -= LineHeight * LayoutVisibility;
		// 超出 HUD 的聊天高度预算：停止准备更旧消息。
		if(y < HeightLimit)
			break;
		const float TargetY = LayoutBottom - LineHeight;

		// the position the text was created
		Line.m_TextYOffset = TargetY + RealMsgPaddingY / 2.0f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		// reset the cursor
		CTextCursor LineCursor;
		LineCursor.SetPosition(vec2(TextBegin, Line.m_TextYOffset));
		LineCursor.m_FontSize = FontSize;
		LineCursor.m_LineWidth = LineWidth;

		// Message is from valid player
		if(!MultipleAuthors && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			LineCursor.m_X += RealMsgPaddingTee;

			if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendHeartColor)));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, "♥ ");
			}
		}

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else
			NameColor = PlayerNameColor(Line.m_ClientId, Line.m_NameColor, Line.m_Team);

		if(MergedPlayerMessages)
		{
			TextRender()->TextColor(Line.m_vMergedAuthors.front().m_NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aClientId);
			for(size_t i = 0; i < Line.m_vMergedAuthors.size(); ++i)
			{
				TextRender()->TextColor(Line.m_vMergedAuthors[i].m_NameColor);
				if(i > 0)
					TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, ",");
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, Line.m_vMergedAuthors[i].m_aName);
			}
			NameColor = Line.m_vMergedAuthors.back().m_NameColor;
		}
		else
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aClientId);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, Line.m_aName);
		}

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aCount);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, ": ");
		}

		ColorRGBA Color;
		const char *pGradient = nullptr;
		if(Line.m_CustomColor)
		{
			Color = *Line.m_CustomColor;
		}
		else if(Line.m_ClientId == SERVER_MSG)
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			pGradient = g_Config.m_ClMessageSystemGradient;
		}
		else if(Line.m_ClientId == CLIENT_MSG)
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			pGradient = g_Config.m_ClMessageClientGradient;
		}
		else if(Line.m_Highlighted)
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
			pGradient = g_Config.m_ClMessageHighlightGradient;
		}
		else if(Line.m_Friend && g_Config.m_ClMessageFriend)
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
			pGradient = g_Config.m_ClMessageFriendGradient;
		}
		else if(Line.m_Team)
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			pGradient = g_Config.m_ClMessageTeamGradient;
		}
		else // regular message
		{
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
			pGradient = g_Config.m_ClMessageGradient;
		}
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = LineCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LineCursor.m_X;
			AppendCursor.m_LineWidth -= LineCursor.m_LongestLineWidth;
		}

		if(pTranslatedText)
		{
			if(pGradient != nullptr && Line.m_CustomColor == std::nullopt && ColoredParts.Colors().empty())
				CMessageGradient::AddTextSplits(AppendCursor, pTranslatedText, pGradient, Color);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedText);
			AppendCursor.m_vColorSplits.clear();
			if(pTranslatedLanguage)
			{
				ColorRGBA ColorLang = Color;
				ColorLang.r *= 0.8f;
				ColorLang.g *= 0.8f;
				ColorLang.b *= 0.8f;
				TextRender()->TextColor(ColorLang);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, " [");
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedLanguage);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "]");
			}
			ColorRGBA ColorSub = Color;
			ColorSub.r *= 0.7f;
			ColorSub.g *= 0.7f;
			ColorSub.b *= 0.7f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else if(pTranslatedError)
		{
			if(pGradient != nullptr && Line.m_CustomColor == std::nullopt && ColoredParts.Colors().empty())
				CMessageGradient::AddTextSplits(AppendCursor, pText, pGradient, Color);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_vColorSplits.clear();
			ColorRGBA ColorSub = Color;
			ColorSub.r = 0.7f;
			ColorSub.g = 0.6f;
			ColorSub.b = 0.6f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedError);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else
		{
			if(pGradient != nullptr && Line.m_CustomColor == std::nullopt && ColoredParts.Colors().empty())
				CMessageGradient::AddTextSplits(AppendCursor, pText, pGradient, Color);
			ColoredParts.AddSplitsToCursor(AppendCursor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_vColorSplits.clear();
		}

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			float FullWidth = RealMsgPaddingX * 1.5f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				FullWidth += LineCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
			}
			else
			{
				FullWidth += maximum(LineCursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
			}
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, TargetY, FullWidth, LineHeight, MessageRounding(), IGraphics::CORNER_ALL);
		}

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CChat::OnRender()
{
	FlushPendingConsoleLine(false);
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const bool FocusModeActive = g_Config.m_QmFocusMode != 0;
	const bool FocusHideChat = FocusModeActive && g_Config.m_QmFocusModeHideChat;
	const bool FocusHideSystemInfoMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemInfoMessages;
	const bool FocusHideSystemPromptMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemMessages;
	const bool FocusHideEcho = FocusModeActive && g_Config.m_QmFocusModeHideEcho;
	const bool HasForceVisibleLine = std::any_of(std::begin(m_aLines), std::end(m_aLines), [](const CLine &Line) { return Line.m_Initialized && Line.m_ForceVisible; });
	if(!ShouldRenderAnyFocusFilteredChat(FocusHideChat, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, FocusHideEcho, HasForceVisibleLine))
	{
		m_ArgumentCandidatePopup.m_RectValid = false;
		return;
	}

	const bool HudEditorPreview = GameClient()->m_HudEditor.IsActive();
	const bool InputActive = m_Mode != MODE_NONE;
	const bool ChatPopupOpen = m_LanguageMenuOpen || Ui()->IsPopupOpen(&m_LanguagePopupContext) || Ui()->IsPopupOpen(&m_ChatLinePopupContext);
	if(InputActive && !ChatPopupOpen)
		RefreshArgumentCandidates();
	else
		m_ArgumentCandidatePopup.m_RectValid = false;
	const bool ShowLargeArea =
		m_Show ||
		(InputActive && g_Config.m_ClShowChat == 1) ||
		g_Config.m_ClShowChat == 2;
	const bool ExtraAnimations = g_Config.m_QmExtraAnimations != 0 && GameClient()->UiRuntimeV2()->Enabled();
	int64_t Now = time();
	if(m_LastPresentationUpdateTime == 0 || Now < m_LastPresentationUpdateTime)
		m_LastPresentationUpdateTime = Now;
	float DeltaSeconds = std::clamp((Now - m_LastPresentationUpdateTime) / (float)time_freq(), 0.0f, CHAT_PRESENTATION_MAX_DELTA_SECONDS);
	const float ChatFadeDurationSeconds = g_Config.m_QmChatAnimFadeDurationMs / 1000.0f;
	(void)ChatFadeDurationSeconds;
	m_LastPresentationUpdateTime = Now;
	if(HudEditorPreview)
		DeltaSeconds = 0.0f;
	else
		UpdatePresentationStates(Now, DeltaSeconds, ShowLargeArea, ExtraAnimations);

	// send pending chat messages
	if(m_PendingChatCounter > 0 && m_LastChatSend + time_freq() < time())
	{
		CHistoryEntry *pEntry = m_History.Last();
		for(int i = m_PendingChatCounter - 1; pEntry; --i, pEntry = m_History.Prev(pEntry))
		{
			if(i == 0)
			{
				SendChat(pEntry->m_Team, pEntry->m_aText);
				break;
			}
		}
		--m_PendingChatCounter;
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	const bool ChatAnchoredRight = true;
	const bool ChatScrollbarOnRight = ChatAnchoredRight;
	const CUIRect ChatRect = {0.0f, 50.0f, std::min(Width, std::max(190.0f, g_Config.m_ClChatWidth + 32.0f)), 250.0f};
	const auto HudEditorScope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::Chat, ChatRect);

	float x = 5.0f;
	float BoundsTop = Height;
	float BoundsBottom = 0.0f;
	bool HasBounds = false;
	auto ExtendBounds = [&](float X, float Y, float W, float H) {
		if(W <= 0.0f || H <= 0.0f)
			return;
		const float Bottom = Y + H;
		if(!HasBounds)
		{
			BoundsTop = Y;
			BoundsBottom = Bottom;
			HasBounds = true;
			return;
		}
		BoundsTop = minimum(BoundsTop, Y);
		BoundsBottom = maximum(BoundsBottom, Bottom);
	};

	// TClient
	float y = 300.0f - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));
	// float y = 300.0f - 20.0f * FontSize() / 6.0f;

	float ScaledFontSize = FontSize() * (8.0f / 6.0f);
	const float CommandPreviewFontSize = ScaledFontSize * 0.5f;
	const float TranslateButtonSize = maximum(16.0f, ScaledFontSize * 1.35f);
	const float TranslateButtonGap = 4.0f;
	const float InputLineWidth = std::max(Width - 190.0f, 190.0f);
	const char *pInputModeLabel = m_Mode == MODE_ALL ? Localize("All") : (m_Mode == MODE_TEAM ? Localize("Team") : Localize("Chat"));
	const float InputPrefixWidth = TextRender()->TextWidth(ScaledFontSize, pInputModeLabel) + TextRender()->TextWidth(ScaledFontSize, ": ");
	const float CommandPreviewMaxWidth = maximum(1.0f, InputLineWidth - InputPrefixWidth - TranslateButtonSize - TranslateButtonGap);
	char aCommandPreview[MAX_LINE_LENGTH];
	const bool HasCommandPreview = m_Mode != MODE_NONE && BuildCommandUsagePreview(m_Input.GetString(), aCommandPreview, sizeof(aCommandPreview));
	CUIRect InputBlockRect = {};
	bool InputBlockRectValid = false;
	if(HasCommandPreview)
	{
		const STextBoundingBox PreviewBoundingBox = TextRender()->TextBoundingBox(CommandPreviewFontSize, aCommandPreview, -1, CommandPreviewMaxWidth);
		y -= PreviewBoundingBox.m_H + 4.0f;
	}

	if(InputActive)
	{
		// render chat input
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(x, y));
		InputCursor.SetPosition(vec2(x + TranslateButtonSize + TranslateButtonGap, y));
		InputCursor.m_FontSize = ScaledFontSize;
		InputCursor.m_LineWidth = InputLineWidth;

		// TClient
		InputCursor.m_LineWidth = InputLineWidth;

		TextRender()->TextEx(&InputCursor, pInputModeLabel);

		TextRender()->TextEx(&InputCursor, ": ");

		// 计算翻译按钮大小并调整输入框宽度
		const float MessageMaxWidth = InputCursor.m_LineWidth - (InputCursor.m_X - InputCursor.m_StartX) - TranslateButtonSize - TranslateButtonGap;
		const float InputContentHeight = 2.25f * InputCursor.m_FontSize;
		const float InputClipPaddingTop = maximum(1.0f, InputCursor.m_FontSize * 0.18f);
		const float InputClipPaddingBottom = maximum(1.0f, InputCursor.m_FontSize * 0.10f);
		const CUIRect InputContentRect = {InputCursor.m_X, InputCursor.m_Y, MessageMaxWidth, InputContentHeight};
		const CUIRect InputClippingRect = {InputContentRect.x, InputContentRect.y - InputClipPaddingTop, InputContentRect.w, InputContentRect.h + InputClipPaddingTop + InputClipPaddingBottom};
		InputBlockRect = {x, InputContentRect.y, ChatRect.w - x, InputContentRect.h};
		InputBlockRectValid = true;
		ExtendBounds(x, InputContentRect.y, ChatRect.w - x, InputContentRect.h);
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		Graphics()->ClipEnable((int)(InputClippingRect.x * XScale), (int)(InputClippingRect.y * YScale), (int)(InputClippingRect.w * XScale), (int)(InputClippingRect.h * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();

		m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
		const CUIRect InputCursorRect = {InputContentRect.x, InputContentRect.y + InputClipPaddingTop - ScrollOffset, 0.0f, 0.0f};
		const bool WasChanged = m_Input.WasChanged();
		const bool WasCursorChanged = m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		const STextBoundingBox BoundingBox = m_Input.Render(&InputCursorRect, InputCursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);

		Graphics()->ClipDisable();

		// Scroll up or down to keep the caret inside the content rect.
		const float CaretPositionY = m_Input.GetCaretPosition().y - InputClipPaddingTop - ScrollOffsetChange;
		if(CaretPositionY < InputContentRect.y)
			ScrollOffsetChange -= InputContentRect.y - CaretPositionY;
		else if(CaretPositionY + InputCursor.m_FontSize > InputContentRect.y + InputContentRect.h)
			ScrollOffsetChange += CaretPositionY + InputCursor.m_FontSize - (InputContentRect.y + InputContentRect.h);

		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, InputContentRect.h, BoundingBox.m_H);

		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollOffsetChange);

		// Autocompletion hint
		if(m_Input.GetString()[0] == '/' && m_Input.GetString()[1] != '\0' && !m_vServerCommands.empty())
		{
			for(const auto &Command : m_vServerCommands)
			{
				if(str_startswith_nocase(Command.m_aName, m_Input.GetString() + 1))
				{
					InputCursor.m_X = InputCursor.m_X + TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
					InputCursor.m_Y = m_Input.GetCaretPosition().y;
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
					TextRender()->TextEx(&InputCursor, Command.m_aName + str_length(m_Input.GetString() + 1));
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					break;
				}
			}
		}

		if(HasCommandPreview)
		{
			CTextCursor PreviewCursor;
			const float PreviewY = InputContentRect.y + minimum(BoundingBox.m_H, InputContentRect.h) + 2.0f;
			PreviewCursor.SetPosition(vec2(InputContentRect.x, PreviewY));
			PreviewCursor.m_FontSize = CommandPreviewFontSize;
			PreviewCursor.m_LineWidth = MessageMaxWidth;
			PreviewCursor.m_Flags = TEXTFLAG_RENDER;
			TextRender()->TextColor(0.72f, 0.88f, 1.0f, 0.78f);
			TextRender()->TextEx(&PreviewCursor, aCommandPreview);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			ExtendBounds(PreviewCursor.m_StartX, PreviewCursor.m_StartY, MessageMaxWidth, PreviewCursor.Height());
		}

		if(!ChatPopupOpen)
			RenderArgumentCandidates(InputContentRect, MessageMaxWidth);
		else
			m_ArgumentCandidatePopup.m_RectValid = false;
		if(m_ArgumentCandidatePopup.m_RectValid)
		{
			ExtendBounds(m_ArgumentCandidatePopup.m_X, m_ArgumentCandidatePopup.m_Y, m_ArgumentCandidatePopup.m_W, m_ArgumentCandidatePopup.m_H);
			y = minimum(y, m_ArgumentCandidatePopup.m_Y);
		}

		// 渲染翻译按钮
		CUIRect TranslateButtonRect = {InputContentRect.x + InputContentRect.w + TranslateButtonGap, InputContentRect.y, TranslateButtonSize, maximum(InputCursor.m_FontSize + 4.0f, 16.0f)};
		RenderTranslateButton(TranslateButtonRect);
	}
	else
	{
		m_TranslateButton.m_RectValid = false;
		m_ArgumentCandidatePopup.m_RectValid = false;
	}

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
	{
		GameClient()->m_HudEditor.EndTransform(HudEditorScope);
		return;
	}

	y -= ScaledFontSize;

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive();
	const float HeightLimit = IsScoreBoardOpen ? CHAT_HEIGHT_MIN + 130.0f : (ShowLargeArea ? CHAT_HEIGHT_MIN : CHAT_HEIGHT_FULL);
	int OffsetType = IsScoreBoardOpen ? 1 : 0;
	const ColorRGBA ConfigBackgroundColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true));
	const ColorRGBA BackgroundBaseColor = g_Config.m_ClChatOld ? ConfigBackgroundColor : ConfigBackgroundColor.Multiply(ColorRGBA(0.78f, 0.86f, 1.0f, 1.0f));
	const ColorRGBA DefaultTextColor = TextRender()->DefaultTextColor();
	const ColorRGBA DefaultTextOutlineColor = TextRender()->DefaultTextOutlineColor();
	const CAnimState *pIdleState = CAnimState::GetIdle();
	const int TeeSize = MessageTeeSize();

	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	const float RowHeight = FontSize() + RealMsgPaddingY;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	const float HistoryBottom = y;
	const float HistoryHeight = maximum(0.0f, HistoryBottom - HeightLimit);
	const int TotalVisibleLines = CountVisibleLinesFrom(0);
	const int VisibleLineCapacity = maximum(1, (int)std::floor(HistoryHeight / maximum(RowHeight, 1.0f)));
	const int MaxScroll = maximum(0, TotalVisibleLines - VisibleLineCapacity);
	if(!InputActive)
		m_BacklogCurLine = 0;
	m_BacklogCurLine = ClampBacklogLine(m_BacklogCurLine, TotalVisibleLines, VisibleLineCapacity);

	const bool ShowChatScrollbar = InputActive && MaxScroll > 0 && HistoryHeight > 0.0f;
	CUIRect ScrollbarRect = {ChatScrollbarOnRight ? ChatRect.w - CHAT_SCROLLBAR_WIDTH - CHAT_SCROLLBAR_MARGIN : CHAT_SCROLLBAR_MARGIN, HeightLimit, CHAT_SCROLLBAR_WIDTH, HistoryHeight};
	float ScrollbarHandleY = ScrollbarRect.y;
	float ScrollbarHandleH = ScrollbarRect.h;
	if(ShowChatScrollbar)
	{
		const float VisibleRatio = std::clamp(VisibleLineCapacity / (float)maximum(TotalVisibleLines, 1), 0.08f, 1.0f);
		ScrollbarHandleH = std::clamp(ScrollbarRect.h * VisibleRatio, 12.0f, ScrollbarRect.h);
		const float TrackRange = maximum(1.0f, ScrollbarRect.h - ScrollbarHandleH);
		ScrollbarHandleY = ScrollbarRect.y + TrackRange * BacklogLineToScrollbarValue(m_BacklogCurLine, MaxScroll);
		vec2 MousePos = GetChatMousePos();
		if(HudEditorScope.m_Applied && ChatRect.w > 0.0f)
		{
			const float Scale = HudEditorScope.m_TargetRect.w / ChatRect.w;
			MousePos.x = (MousePos.x - HudEditorScope.m_TargetRect.x) / Scale;
			MousePos.y = (MousePos.y - HudEditorScope.m_TargetRect.y) / Scale;
		}
		const bool InsideRail =
			MousePos.x >= ScrollbarRect.x &&
			MousePos.x <= ScrollbarRect.x + ScrollbarRect.w &&
			MousePos.y >= ScrollbarRect.y &&
			MousePos.y <= ScrollbarRect.y + ScrollbarRect.h;
		const bool InsideHandle =
			MousePos.x >= ScrollbarRect.x &&
			MousePos.x <= ScrollbarRect.x + ScrollbarRect.w &&
			MousePos.y >= ScrollbarHandleY &&
			MousePos.y <= ScrollbarHandleY + ScrollbarHandleH;

		if(Input()->KeyPress(KEY_MOUSE_1) && InsideRail)
		{
			m_ScrollbarDragging = true;
			m_ScrollbarDragOffset = InsideHandle ? std::clamp(MousePos.y - ScrollbarHandleY, 0.0f, ScrollbarHandleH) : ScrollbarHandleH * 0.5f;
		}
		if(!Input()->KeyIsPressed(KEY_MOUSE_1))
			m_ScrollbarDragging = false;
		if(m_ScrollbarDragging)
		{
			const float HandleTop = std::clamp(MousePos.y - m_ScrollbarDragOffset, ScrollbarRect.y, ScrollbarRect.y + TrackRange);
			const float RelativeTop = (HandleTop - ScrollbarRect.y) / TrackRange;
			const int NewBacklogCurLine = ScrollbarValueToBacklogLine(RelativeTop, MaxScroll);
			if(NewBacklogCurLine != m_BacklogCurLine)
			{
				m_BacklogCurLine = NewBacklogCurLine;
				RebuildChat();
			}
			ScrollbarHandleY = ScrollbarRect.y + TrackRange * BacklogLineToScrollbarValue(m_BacklogCurLine, MaxScroll);
		}
	}
	else
	{
		m_ScrollbarDragging = false;
	}

	const bool LanguageMenuOpen = m_LanguageMenuOpen || Ui()->IsPopupOpen(&m_LanguagePopupContext);
	const bool ChatLineMenuOpen = Ui()->IsPopupOpen(&m_ChatLinePopupContext);
	vec2 MousePos = GetChatMousePos();
	if(HudEditorScope.m_Applied && ChatRect.w > 0.0f)
	{
		const float Scale = HudEditorScope.m_TargetRect.w / ChatRect.w;
		MousePos.x = (MousePos.x - HudEditorScope.m_TargetRect.x) / Scale;
		MousePos.y = (MousePos.y - HudEditorScope.m_TargetRect.y) / Scale;
	}
	const bool MouseDown = Input()->KeyIsPressed(KEY_MOUSE_1);
	const bool InsideInputBlock =
		InputBlockRectValid &&
		MousePos.x >= InputBlockRect.x &&
		MousePos.x <= InputBlockRect.x + InputBlockRect.w &&
		MousePos.y >= InputBlockRect.y &&
		MousePos.y <= InputBlockRect.y + InputBlockRect.h;
	const bool InsideTranslateButton =
		m_TranslateButton.m_RectValid &&
		MousePos.x >= m_TranslateButton.m_X &&
		MousePos.x <= m_TranslateButton.m_X + m_TranslateButton.m_W &&
		MousePos.y >= m_TranslateButton.m_Y &&
		MousePos.y <= m_TranslateButton.m_Y + m_TranslateButton.m_H;
	const bool InsideArgumentCandidates =
		m_ArgumentCandidatePopup.m_RectValid &&
		MousePos.x >= m_ArgumentCandidatePopup.m_X &&
		MousePos.x <= m_ArgumentCandidatePopup.m_X + m_ArgumentCandidatePopup.m_W &&
		MousePos.y >= m_ArgumentCandidatePopup.m_Y &&
		MousePos.y <= m_ArgumentCandidatePopup.m_Y + m_ArgumentCandidatePopup.m_H;
	const bool InsideScrollbar =
		ShowChatScrollbar &&
		MousePos.x >= ScrollbarRect.x &&
		MousePos.x <= ScrollbarRect.x + ScrollbarRect.w &&
		MousePos.y >= ScrollbarRect.y &&
		MousePos.y <= ScrollbarRect.y + ScrollbarRect.h;
	const bool ChatCopyActive = m_Mode != MODE_NONE && !LanguageMenuOpen && !ChatLineMenuOpen && !InsideInputBlock && !InsideTranslateButton && !InsideArgumentCandidates && !InsideScrollbar && !m_ScrollbarDragging;
	const bool CopyClickReleased = m_MouseIsPress && !MouseDown && IsCopyClickDrag(m_MousePress, MousePos);
	const bool ChatLineMenuRequested = ChatCopyActive && Input()->KeyPress(KEY_MOUSE_2);
	if(ChatCopyActive)
	{
		if(!m_MouseIsPress && MouseDown)
		{
			m_MouseIsPress = true;
			m_MousePress = MousePos;
			m_MouseRelease = MousePos;
		}
		else if(m_MouseIsPress && MouseDown)
		{
			m_MouseRelease = MousePos;
		}
		else if(m_MouseIsPress)
		{
			m_MouseIsPress = false;
			m_MouseRelease = MousePos;
		}
	}
	else if(!MouseDown)
	{
		m_MouseIsPress = false;
	}

	OnPrepareLines(y);

	bool RenderedAnyLines = false;
	const CLine *pClickedLine = nullptr;
	const CLine *pMenuLine = nullptr;

	for(int i = m_BacklogCurLine; i < MAX_LINES; i++)
	{
		const int LineIndex = ((m_CurrentLine - i) + MAX_LINES) % MAX_LINES;
		CLine &Line = m_aLines[LineIndex];
		if(!Line.m_Initialized)
			break;
		const bool ServerMessageIsBasicInfo = Line.m_ServerMessageClass == QmHudNotifications::EServerMessageClass::BasicInfo;
		if(!ShouldRenderFocusFilteredChatLine(FocusHideChat, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, FocusHideEcho, Line.m_ClientId, Line.m_ForceVisible, ServerMessageIsBasicInfo))
		{
			continue;
		}
		if(!ShowLargeArea && !Line.m_ForceVisible && Line.m_Presentation.m_State == EPresentationState::COLLAPSED)
		{
			continue;
		}

		const bool LineHeightValid = Line.m_aYOffset[OffsetType] >= 0.0f;
		const float LineHeight = LineHeightValid ? Line.m_aYOffset[OffsetType] : FontSize() + RealMsgPaddingY;
		const float LayoutVisibility = ClampPresentationProgress(Line.m_Presentation.m_LayoutVisibility);
		const float LayoutBottom = y;
		y -= LineHeight * LayoutVisibility;
		if(y < HeightLimit)
			break;
		Line.m_Presentation.m_TargetY = LayoutBottom - LineHeight;

		// Don't abort the full render pass on a single malformed line.
		if(!LineHeightValid)
		{
			Line.m_CutOffProgress = 0.0f;
			continue;
		}

		if(!Line.m_Presentation.m_RenderYInitialized || HudEditorPreview || !ExtraAnimations)
		{
			Line.m_Presentation.m_RenderY = Line.m_Presentation.m_TargetY;
			Line.m_Presentation.m_RenderYInitialized = true;
		}
		else
			Line.m_Presentation.m_RenderY = SmoothPresentationY(Line.m_Presentation.m_RenderY, Line.m_Presentation.m_TargetY, DeltaSeconds);

		const float RenderY = Line.m_Presentation.m_RenderY + Line.m_Presentation.m_RenderOffsetY;
		Line.m_CutOffProgress = 0.0f;
		const float AnimAlpha = ClampPresentationProgress(Line.m_Presentation.m_RenderAlpha);
		const float AnimOffsetX = Line.m_Presentation.m_RenderOffsetX + CalculateCutOffOffsetX(Line.m_CutOffProgress);
		const float AnimOffsetY = (RenderY + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset;

		if(AnimAlpha <= 0.001f)
			continue;

		// Fully transparent lines must not receive mouse interaction.
		if(AnimAlpha <= 0.001f)
			continue;

		const CUIRect RenderedTextRect = {x + AnimOffsetX, RenderY, ChatRect.w - x, LineHeight};
		if(CopyClickReleased && MousePos.x >= RenderedTextRect.x && MousePos.x <= RenderedTextRect.x + RenderedTextRect.w &&
			MousePos.y >= RenderedTextRect.y && MousePos.y <= RenderedTextRect.y + RenderedTextRect.h)
		{
			pClickedLine = &Line;
		}

		if(ChatLineMenuRequested && MousePos.x >= RenderedTextRect.x && MousePos.x <= RenderedTextRect.x + RenderedTextRect.w &&
			MousePos.y >= RenderedTextRect.y && MousePos.y <= RenderedTextRect.y + RenderedTextRect.h)
		{
			pMenuLine = &Line;
		}

		// Draw backgrounds for messages in one batch
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				const float QuadScale = Line.m_Presentation.m_RenderScale;
				const float QuadY = Line.m_TextYOffset - RealMsgPaddingY / 2.0f;
				const float QuadOffsetX = AnimOffsetX + x * (1.0f - QuadScale);
				const float QuadOffsetY = AnimOffsetY + QuadY * (1.0f - QuadScale);
				Graphics()->SetColor(BackgroundBaseColor.WithMultipliedAlpha(AnimAlpha));
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, QuadOffsetX, QuadOffsetY, QuadScale, QuadScale);
			}
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			RenderedAnyLines = true;
			ExtendBounds(x + AnimOffsetX, RenderY, ChatRect.w - x, LineHeight);
			if(Line.m_vMergedAuthors.size() <= 1 && !g_Config.m_ClChatOld && Line.m_pManagedTeeRenderInfo != nullptr)
			{
				CTeeRenderInfo &TeeRenderInfo = Line.m_pManagedTeeRenderInfo->TeeRenderInfo();
				TeeRenderInfo.m_Size = TeeSize * Line.m_Presentation.m_RenderScale;

				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + AnimOffsetX + (RealMsgPaddingX + TeeSize) / 2.0f, RenderY + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, AnimAlpha);
			}

			const ColorRGBA TextColor = DefaultTextColor.WithMultipliedAlpha(AnimAlpha);
			const ColorRGBA TextOutlineColor = DefaultTextOutlineColor.WithMultipliedAlpha(AnimAlpha);
			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, AnimOffsetX, AnimOffsetY);
		}
	}

	if(CopyClickReleased && pClickedLine != nullptr && pClickedLine->m_aText[0] != '\0')
	{
		Input()->SetClipboardText(pClickedLine->m_aText);
	}
	if(ChatLineMenuRequested && pMenuLine != nullptr && pMenuLine->m_aText[0] != '\0')
	{
		OpenChatLineMenu(*pMenuLine, MousePos);
	}

	if(ShowChatScrollbar)
	{
		Graphics()->TextureClear();
		Graphics()->DrawRect(ScrollbarRect.x, ScrollbarRect.y, ScrollbarRect.w, ScrollbarRect.h, ColorRGBA(1.0f, 1.0f, 1.0f, 0.18f * CHAT_SCROLLBAR_ALPHA_SCALE), IGraphics::CORNER_ALL, ScrollbarRect.w * 0.5f);
		const ColorRGBA HandleColor = m_ScrollbarDragging ? ColorRGBA(0.85f, 0.85f, 0.85f, 0.95f * CHAT_SCROLLBAR_ALPHA_SCALE) : ColorRGBA(0.62f, 0.62f, 0.62f, 0.82f * CHAT_SCROLLBAR_ALPHA_SCALE);
		Graphics()->DrawRect(ScrollbarRect.x, ScrollbarHandleY, ScrollbarRect.w, ScrollbarHandleH, HandleColor, IGraphics::CORNER_ALL, ScrollbarRect.w * 0.5f);
		ExtendBounds(ScrollbarRect.x, ScrollbarRect.y, ScrollbarRect.w, ScrollbarRect.h);
	}

	if(HudEditorPreview && !RenderedAnyLines)
	{
		struct SPreviewLine
		{
			const char *m_pPrefix;
			const char *m_pMessage;
			ColorRGBA m_TextColor;
		};

		static const SPreviewLine s_aPreviewLines[] = {
			{"Server", "Welcome to QmClient", ColorRGBA(0.72f, 0.82f, 1.0f, 0.92f)},
			{"Teammate", "Ready?", ColorRGBA(0.72f, 1.0f, 0.72f, 0.92f)},
			{"Friend", "Let's go!", ColorRGBA(1.0f, 0.92f, 0.72f, 0.92f)},
		};

		float PreviewY = 300.0f - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f)) - ScaledFontSize;
		PreviewY -= RowHeight * (float)std::size(s_aPreviewLines);

		for(const SPreviewLine &Line : s_aPreviewLines)
		{
			char aPreviewText[256];
			str_format(aPreviewText, sizeof(aPreviewText), "%s: %s", Line.m_pPrefix, Line.m_pMessage);
			const float TextWidth = TextRender()->TextWidth(FontSize(), aPreviewText, -1, -1.0f);
			const float PreviewWidth = minimum(ChatRect.w - x, TextWidth + RealMsgPaddingX * 1.5f + (g_Config.m_ClChatOld ? 0.0f : MessageTeeSize() + 2.0f));

			if(!g_Config.m_ClChatOld)
				Graphics()->DrawRect(x, PreviewY, PreviewWidth, RowHeight, BackgroundBaseColor, IGraphics::CORNER_ALL, MessageRounding());

			TextRender()->TextColor(Line.m_TextColor);
			TextRender()->Text(x + (g_Config.m_ClChatOld ? 0.0f : RealMsgPaddingX), PreviewY + RealMsgPaddingY * 0.5f, FontSize(), aPreviewText, -1.0f);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			ExtendBounds(x, PreviewY, PreviewWidth, RowHeight);
			PreviewY += RowHeight;
		}
	}

	if(HasBounds)
	{
		const float BoundsHeight = maximum(0.0f, BoundsBottom - BoundsTop);
		GameClient()->m_HudEditor.UpdateVisibleRect(EHudEditorElement::Chat, {x, BoundsTop, ChatRect.w - x, BoundsHeight});
	}

	GameClient()->m_HudEditor.EndTransform(HudEditorScope);

	// 渲染聊天相关弹窗。
	if(m_Mode != MODE_NONE && (Ui()->IsPopupOpen(&m_LanguagePopupContext) || Ui()->IsPopupOpen(&m_ChatLinePopupContext)))
	{
		Ui()->StartCheck();
		Ui()->Update();
		Ui()->MapScreen();
		Ui()->RenderPopupMenus();
		Ui()->FinishCheck();
		Ui()->ClearHotkeys();
		m_LanguageMenuOpen = Ui()->IsPopupOpen(&m_LanguagePopupContext);
		Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	}
	else
	{
		m_LanguageMenuOpen = false;
	}

	// 渲染鼠标光标（当聊天框激活时）
	if(m_Mode != MODE_NONE)
	{
		const vec2 UiMousePos = Ui()->UpdatedMousePos() * vec2(Ui()->Screen()->w, Ui()->Screen()->h) / vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
		const vec2 UiToChatScale(Width / Ui()->Screen()->w, Height / Ui()->Screen()->h);
		RenderTools()->RenderCursor(UiMousePos * UiToChatScale, 12.0f);
	}
}

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

static bool ShouldSyncDummyCommandToOther(const char *pLine)
{
	return g_Config.m_ClDummyCopyMoves && CChat::ShouldSyncDummyCommand(pLine);
}

void CChat::SendChat(int Team, const char *pLine)
{
#if defined(CONF_QM_LIVE_CLIENT)
	if(GameClient()->LivePresentationMode() == CGameClient::EQmLivePresentationMode::LIVE_OBSERVER && ShouldBlockLiveDirectorChatCommand(pLine))
		return;
#endif

	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;
	if(GameClient()->m_FastPractice.ConsumePracticeChatCommand(Team, pLine))
		return;

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		GameClient()->TClientComponent().TryRemoveLocalSaveForLoadCommand(pLine);

		if(Client()->DummyConnected() && ShouldSyncDummyCommandToOther(pLine))
			SendChatOnConn(!g_Config.m_ClDummy, Team, pLine);

		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
	GameClient()->TClientComponent().TryRemoveLocalSaveForLoadCommand(pLine);

	if(Client()->DummyConnected() && ShouldSyncDummyCommandToOther(pLine))
		SendChatOnConn(!g_Config.m_ClDummy, Team, pLine);
}

void CChat::SendChatOnConn(int Conn, int Team, const char *pLine, bool AllowWhitespaceOnly, bool HandleLocalSaveForLoadCommand)
{
#if defined(CONF_QM_LIVE_CLIENT)
	if(GameClient()->LivePresentationMode() == CGameClient::EQmLivePresentationMode::LIVE_OBSERVER && ShouldBlockLiveDirectorChatCommand(pLine))
		return;
#endif

	if(pLine == nullptr || pLine[0] == '\0')
		return;

	// don't send empty messages
	if(!AllowWhitespaceOnly && *str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	if(Conn != IClient::CONN_DUMMY)
		Conn = IClient::CONN_MAIN;

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsg(Conn, &Msg7, MSGFLAG_VITAL, true);
	}
	else
	{
		// send chat message
		CNetMsg_Cl_Say Msg;
		Msg.m_Team = Team;
		Msg.m_pMessage = pLine;
		Client()->SendPackMsg(Conn, &Msg, MSGFLAG_VITAL);
	}

	if(HandleLocalSaveForLoadCommand)
		GameClient()->TClientComponent().TryRemoveLocalSaveForLoadCommand(pLine);
}

void CChat::SendChatQueued(int Team, const char *pLine, bool AllowOutgoingTranslation)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	// 自动出站翻译
	if(AllowOutgoingTranslation && GameClient()->m_Translate.ShouldAutoTranslateOutgoing(pLine))
	{
		GameClient()->m_Translate.StartAutoOutgoingTranslate(Team, pLine);
		return;
	}

	bool AddEntry = false;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(Team, pLine);
		AddEntry = true;
	}
	else if(m_PendingChatCounter < 3)
	{
		++m_PendingChatCounter;
		AddEntry = true;
	}

	if(AddEntry)
	{
		const int Length = str_length(pLine);
		CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
		pEntry->m_Team = Team;
		str_copy(pEntry->m_aText, pLine, Length + 1);
	}
}

void CChat::SendChatQueued(const char *pLine)
{
	SendChatQueued(m_Mode == MODE_ALL ? 0 : 1, pLine, true);
}

// ===== 翻译按钮相关方法 =====

vec2 CChat::GetChatMousePos() const
{
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	const vec2 WindowSize(maximum(1.0f, (float)Graphics()->WindowWidth()), maximum(1.0f, (float)Graphics()->WindowHeight()));
	const vec2 UiMousePos = Ui()->UpdatedMousePos() * vec2(Ui()->Screen()->w, Ui()->Screen()->h) / WindowSize;
	const vec2 UiToChatScale(Width / Ui()->Screen()->w, Height / Ui()->Screen()->h);
	return UiMousePos * UiToChatScale;
}

void CChat::RenderTranslateButton(const CUIRect &ButtonRect)
{
	using namespace FontIcons;

	m_TranslateButton.m_X = ButtonRect.x;
	m_TranslateButton.m_Y = ButtonRect.y;
	m_TranslateButton.m_W = ButtonRect.w;
	m_TranslateButton.m_H = ButtonRect.h;
	m_TranslateButton.m_RectValid = true;

	const vec2 MousePos = GetChatMousePos();
	const bool Hovered = ButtonRect.Inside(MousePos);

	const bool IsOpen = m_LanguageMenuOpen;
	m_TranslateButton.m_AutoTranslateEnabled = g_Config.m_QmTranslateAutoOutgoing != 0;
	const bool IsEnabled = m_TranslateButton.m_AutoTranslateEnabled;

	ColorRGBA ButtonColor;
	if(IsEnabled)
	{
		ButtonColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmTranslateBtnColorEnabled, true));
	}
	else if(IsOpen)
	{
		ButtonColor = ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f);
	}
	else if(Hovered)
	{
		ButtonColor = ColorRGBA(0.28f, 0.28f, 0.28f, 0.90f);
	}
	else
	{
		ButtonColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmTranslateBtnColorDisabled, true));
	}
	const float ButtonRounding = maximum(6.0f, ButtonRect.h * 0.28f);

	ButtonRect.Draw(ButtonColor, IGraphics::CORNER_ALL, ButtonRounding);

	CUIRect IconRect;
	ButtonRect.Margin(1.0f, &IconRect);
	const float IconSize = IconRect.h * CUi::ms_FontmodHeight;

	if(!m_TranslateButton.m_IconUiElementInit)
	{
		m_TranslateButton.m_IconUiElement.Init(Ui(), 1);
		m_TranslateButton.m_IconUiElementInit = true;
	}

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH |
				     ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING |
				     ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING |
				     ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT |
				     ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.95f);
	Ui()->DoLabelStreamed(*m_TranslateButton.m_IconUiElement.Rect(0), &IconRect, FONT_ICON_LANGUAGE, IconSize, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(Hovered)
	{
		const char *pTooltip = IsEnabled ? Localize("Right-click to disable auto-translate") : Localize("Right-click to enable auto-translate");
		GameClient()->m_Tooltips.DoToolTip(&m_TranslateButton, &ButtonRect, pTooltip);
	}
}

bool CChat::TranslateVisibleChatLines()
{
	const bool FocusModeActive = g_Config.m_QmFocusMode != 0;
	const bool FocusHideChat = FocusModeActive && g_Config.m_QmFocusModeHideChat;
	const bool FocusHideSystemInfoMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemInfoMessages;
	const bool FocusHideSystemPromptMessages = FocusModeActive && g_Config.m_QmFocusModeHideSystemMessages;
	const bool FocusHideEcho = FocusModeActive && g_Config.m_QmFocusModeHideEcho;
	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive();
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	const int OffsetType = IsScoreBoardOpen ? 1 : 0;

	int aLineIndices[MAX_LINES];
	int NumLineIndices = 0;
	for(int i = m_BacklogCurLine; i < MAX_LINES; i++)
	{
		const int LineIndex = ((m_CurrentLine - i) + MAX_LINES) % MAX_LINES;
		CLine &Line = m_aLines[LineIndex];
		if(!Line.m_Initialized)
			break;
		const bool ServerMessageIsBasicInfo = Line.m_ServerMessageClass == QmHudNotifications::EServerMessageClass::BasicInfo;
		if(!ShouldRenderFocusFilteredChatLine(FocusHideChat, FocusHideSystemInfoMessages, FocusHideSystemPromptMessages, FocusHideEcho, Line.m_ClientId, Line.m_ForceVisible, ServerMessageIsBasicInfo))
			continue;
		if(!ShowLargeArea && !Line.m_ForceVisible && Line.m_Presentation.m_State == EPresentationState::COLLAPSED)
			continue;
		if(!ShowLargeArea && Line.m_Presentation.m_LayoutVisibility <= 0.001f && Line.m_Presentation.m_RenderAlpha <= 0.001f)
			continue;
		if(Line.m_aYOffset[OffsetType] < 0.0f)
			continue;
		if(Line.m_CutOffProgress >= 1.0f)
			continue;
		if(!IsManualVisibleTranslateCandidate(Line.m_ClientId, Line.m_aText[0] != '\0', Line.m_pTranslateResponse != nullptr, GameClient()->m_aLocalIds, std::size(GameClient()->m_aLocalIds)))
			continue;

		aLineIndices[NumLineIndices++] = LineIndex;
	}
	for(int i = 0; i < NumLineIndices; i++)
		GameClient()->m_Translate.Translate(m_aLines[aLineIndices[i]]);
	return NumLineIndices > 0;
}

void CChat::ToggleAutoTranslate()
{
	m_TranslateButton.m_AutoTranslateEnabled = !m_TranslateButton.m_AutoTranslateEnabled;
	g_Config.m_QmTranslateAutoOutgoing = m_TranslateButton.m_AutoTranslateEnabled ? 1 : 0;
}

void CChat::OpenLanguageMenu()
{
	if(m_LanguageMenuOpen || Ui()->IsPopupOpen(&m_LanguagePopupContext))
	{
		CloseLanguageMenu();
		return;
	}

	m_LanguageMenuOpen = true;
	m_LanguagePopupContext.m_pChat = this;
	m_LanguagePopupContext.m_OpenTime = time();
	m_LanguagePopupContext.m_AnimationProgress = 1.0f;

	constexpr float MenuWidth = 240.0f;
	constexpr float TitleHeight = 16.0f;
	constexpr float ToggleHeight = 16.0f;
	constexpr float DropdownLabelHeight = 11.0f;
	constexpr float DropdownHeight = 18.0f;
	constexpr float SectionSpacing = 4.0f;
	constexpr float ContentMargin = 3.0f;
	// Matches the popup border and margin trimmed by CUi::RenderPopupMenus.
	constexpr float PopupChromeHeight = 10.0f;
	const bool HasWarning = ChatTranslateBackendWarning() != nullptr;
	const float ContentHeight =
		TitleHeight +
		SectionSpacing +
		ToggleHeight +
		SectionSpacing +
		ToggleHeight +
		SectionSpacing +
		DropdownLabelHeight + DropdownHeight +
		SectionSpacing +
		DropdownLabelHeight + DropdownHeight +
		SectionSpacing +
		DropdownLabelHeight + DropdownHeight +
		(HasWarning ? (SectionSpacing + ToggleHeight) : 0.0f) +
		ContentMargin * 2.0f;
	const float MenuHeight = ContentHeight + PopupChromeHeight;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	const vec2 ChatToUiScale(Ui()->Screen()->w / Width, Ui()->Screen()->h / Height);
	vec2 MenuPos = vec2(m_TranslateButton.m_X + m_TranslateButton.m_W, m_TranslateButton.m_Y) * ChatToUiScale;
	MenuPos.x -= MenuWidth;
	MenuPos.y -= MenuHeight;
	MenuPos.x = std::clamp(MenuPos.x, 0.0f, maximum(0.0f, Ui()->Screen()->w - MenuWidth));
	MenuPos.y = std::clamp(MenuPos.y, 0.0f, maximum(0.0f, Ui()->Screen()->h - MenuHeight));

	Ui()->DoPopupMenu(&m_LanguagePopupContext, MenuPos.x, MenuPos.y, MenuWidth, MenuHeight, &m_LanguagePopupContext, PopupLanguageMenu);
}

void CChat::CloseLanguageMenu()
{
	if(Ui()->IsPopupOpen(&m_LanguagePopupContext))
		Ui()->ClosePopupMenu(&m_LanguagePopupContext, true);
	m_LanguageMenuOpen = false;
	m_TranslateButton.m_IsPressed = false;
}

void CChat::OpenChatLineMenu(const CLine &Line, vec2 ChatMousePos)
{
	CloseLanguageMenu();

	m_ChatLinePopupContext.m_pChat = this;
	m_ChatLinePopupContext.m_ClientId = Line.m_ClientId;
	m_ChatLinePopupContext.m_TeamNumber = Line.m_TeamNumber;
	str_copy(m_ChatLinePopupContext.m_aText, Line.m_aText);
	m_ChatLinePopupContext.m_PlayerLine = Line.m_vMergedAuthors.size() <= 1 && Line.m_ClientId >= 0 && Line.m_ClientId < MAX_CLIENTS;
	m_ChatLinePopupContext.m_LocalPlayer = m_ChatLinePopupContext.m_PlayerLine && GameClient()->IsLocalClientId(Line.m_ClientId);
	if(m_ChatLinePopupContext.m_PlayerLine)
	{
		str_copy(m_ChatLinePopupContext.m_aPlayerName, GameClient()->m_aClients[Line.m_ClientId].m_aName);
		GameClient()->FormatStreamerName(Line.m_ClientId, m_ChatLinePopupContext.m_aName, sizeof(m_ChatLinePopupContext.m_aName));
	}
	else
	{
		m_ChatLinePopupContext.m_aPlayerName[0] = '\0';
		str_copy(m_ChatLinePopupContext.m_aName, Line.m_aName);
	}

	constexpr float MenuWidth = 188.0f;
	constexpr float MenuHeight = 266.0f;
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	const vec2 ChatToUiScale(Ui()->Screen()->w / Width, Ui()->Screen()->h / Height);
	vec2 MenuPos = ChatMousePos * ChatToUiScale;
	MenuPos.x = std::clamp(MenuPos.x, 0.0f, maximum(0.0f, Ui()->Screen()->w - MenuWidth));
	MenuPos.y = std::clamp(MenuPos.y, 0.0f, maximum(0.0f, Ui()->Screen()->h - MenuHeight));

	Ui()->DoPopupMenu(&m_ChatLinePopupContext, MenuPos.x, MenuPos.y, MenuWidth, MenuHeight, &m_ChatLinePopupContext, PopupChatLineMenu);
}

void CChat::CloseChatLineMenu()
{
	if(Ui()->IsPopupOpen(&m_ChatLinePopupContext))
		Ui()->ClosePopupMenu(&m_ChatLinePopupContext, true);
}

void CChat::AddTextToBlockWords(const char *pText)
{
	if(AppendBlockWordToList(g_Config.m_QmBlockWordsList, sizeof(g_Config.m_QmBlockWordsList), pText))
		g_Config.m_QmBlockWordsEnabled = 1;
}

void CChat::ReplyToChatLine(const CChatLinePopupContext &Context)
{
	if(!Context.m_PlayerLine || Context.m_aName[0] == '\0')
		return;

	if(Context.m_TeamNumber == TEAM_WHISPER_SEND || Context.m_TeamNumber == TEAM_WHISPER_RECV)
	{
		EnableMode(0);
		char aReply[MAX_LINE_LENGTH];
		BuildWhisperCommand(aReply, sizeof(aReply), Context.m_aName, "");
		m_Input.Set(aReply);
		m_Input.SetCursorOffset(m_Input.GetLength());
		m_Input.SelectNothing();
		return;
	}

	EnableMode(Context.m_TeamNumber == 1 ? 1 : 0);
	char aReply[MAX_LINE_LENGTH];
	str_format(aReply, sizeof(aReply), "%s: ", Context.m_aName);
	m_Input.Set(aReply);
	m_Input.SetCursorOffset(m_Input.GetLength());
	m_Input.SelectNothing();
}

void CChat::RepeatChatLine(const CChatLinePopupContext &Context)
{
	if(Context.m_aText[0] == '\0')
		return;

	if(Context.m_TeamNumber == TEAM_WHISPER_SEND || Context.m_TeamNumber == TEAM_WHISPER_RECV)
	{
		if(!Context.m_PlayerLine || Context.m_aName[0] == '\0')
			return;

		char aLine[MAX_LINE_LENGTH];
		if(BuildWhisperCommand(aLine, sizeof(aLine), Context.m_aName, Context.m_aText))
			SendChat(0, aLine);
		return;
	}

	SendChat(Context.m_TeamNumber == 1 ? 1 : 0, Context.m_aText);
}

void CChat::SpectateChatLine(const CChatLinePopupContext &Context)
{
	if(!Context.m_PlayerLine || Context.m_ClientId < 0 || Context.m_ClientId >= MAX_CLIENTS || Context.m_aPlayerName[0] == '\0')
		return;

	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		GameClient()->m_Spectator.Spectate(Context.m_ClientId);
		return;
	}

	char aCommand[2 * MAX_NAME_LENGTH + 32];
	if(BuildSpectateCommand(aCommand, sizeof(aCommand), Context.m_aPlayerName))
		Console()->ExecuteLine(aCommand);
}

CUi::EPopupMenuFunctionResult CChat::PopupChatLineMenu(void *pContext, CUIRect View, bool Active)
{
	CChatLinePopupContext *pPopupContext = static_cast<CChatLinePopupContext *>(pContext);
	CChat *pChat = pPopupContext->m_pChat;
	CUi *pUi = pChat->Ui();

	View.Margin(5.0f, &View);

	CUIRect Header, Divider;
	View.HSplitTop(40.0f, &Header, &View);
	View.HSplitTop(4.0f, &Divider, &View);

	Header.Draw(ColorRGBA(0.08f, 0.11f, 0.14f, 0.82f), IGraphics::CORNER_ALL, 5.0f);
	Header.Margin(5.0f, &Header);

	CUIRect NameRow, PreviewRow;
	Header.HSplitTop(14.0f, &NameRow, &PreviewRow);
	char aName[96];
	if(pPopupContext->m_aName[0] != '\0')
		str_copy(aName, pPopupContext->m_aName);
	else
		str_copy(aName, Localize("Chat"));

	SLabelProperties LabelProps;
	LabelProps.m_MaxWidth = NameRow.w;
	LabelProps.m_EllipsisAtEnd = true;
	LabelProps.SetColor(ColorRGBA(0.92f, 0.98f, 1.0f, 0.95f));
	pUi->DoLabel(&NameRow, aName, 8.0f, TEXTALIGN_ML, LabelProps);

	LabelProps.m_MaxWidth = PreviewRow.w;
	LabelProps.SetColor(ColorRGBA(0.72f, 0.82f, 0.88f, 0.78f));
	pUi->DoLabel(&PreviewRow, pPopupContext->m_aText, 7.0f, TEXTALIGN_ML, LabelProps);

	Divider.HMargin(1.5f, &Divider);
	Divider.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.13f), IGraphics::CORNER_ALL, 1.0f);
	View.HSplitTop(4.0f, nullptr, &View);

	constexpr float ButtonHeight = 24.0f;
	constexpr float ButtonSpacing = 3.5f;
	constexpr float FontSize = 9.5f;
	constexpr float IconSize = 9.5f;
	constexpr float IconWidth = 21.0f;

	auto DoEntry = [&](CButtonContainer *pButton, const char *pIcon, const char *pText, bool Enabled, ColorRGBA AccentColor) {
		CUIRect Button, IconRect, LabelRect;
		View.HSplitTop(ButtonHeight, &Button, &View);
		View.HSplitTop(ButtonSpacing, nullptr, &View);
		const CUIRect ButtonHitRect = Button;

		const bool Hovered = Active && Enabled && pUi->MouseHovered(&Button);
		const ColorRGBA BackgroundColor = Enabled ?
							  (Hovered ? ColorRGBA(0.18f, 0.25f, 0.30f, 0.96f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)) :
							  ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f);
		Button.Draw(BackgroundColor, IGraphics::CORNER_ALL, 5.0f);

		Button.VMargin(6.0f, &Button);
		Button.VSplitLeft(IconWidth, &IconRect, &LabelRect);

		pChat->TextRender()->TextColor(Enabled ? AccentColor : ColorRGBA(0.55f, 0.60f, 0.64f, 0.45f));
		pUi->DoLabel(&IconRect, pIcon, IconSize, TEXTALIGN_MC);
		pChat->TextRender()->TextColor(Enabled ? ColorRGBA(0.93f, 0.96f, 0.98f, 0.96f) : ColorRGBA(0.62f, 0.67f, 0.70f, 0.45f));
		pUi->DoLabel(&LabelRect, pText, FontSize, TEXTALIGN_ML);
		pChat->TextRender()->TextColor(pChat->TextRender()->DefaultTextColor());

		return Active && Enabled && pUi->DoButtonLogic(pButton, 0, &ButtonHitRect, BUTTONFLAG_LEFT);
	};

	if(DoEntry(&pPopupContext->m_CopyButton, FontIcons::FONT_ICON_COPY, Localize("Copy"), pPopupContext->m_aText[0] != '\0', ColorRGBA(0.74f, 0.88f, 1.0f, 1.0f)))
	{
		pChat->Input()->SetClipboardText(pPopupContext->m_aText);
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(DoEntry(&pPopupContext->m_AddOneButton, FontIcons::FONT_ICON_ARROWS_ROTATE, Localize("Add one"), pPopupContext->m_aText[0] != '\0', ColorRGBA(0.70f, 0.95f, 0.78f, 1.0f)))
	{
		pChat->RepeatChatLine(*pPopupContext);
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(DoEntry(&pPopupContext->m_ReplyButton, FontIcons::FONT_ICON_COMMENT, Localize("Reply"), pPopupContext->m_PlayerLine && pPopupContext->m_aName[0] != '\0', ColorRGBA(0.88f, 0.78f, 1.0f, 1.0f)))
	{
		pChat->ReplyToChatLine(*pPopupContext);
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(DoEntry(&pPopupContext->m_SpectateButton, FontIcons::FONT_ICON_EYE, Localize("Spectate"), pPopupContext->m_PlayerLine && pPopupContext->m_aPlayerName[0] != '\0', ColorRGBA(0.72f, 0.86f, 1.0f, 1.0f)))
	{
		pChat->SpectateChatLine(*pPopupContext);
		return CUi::POPUP_CLOSE_CURRENT;
	}

	View.HSplitTop(2.0f, &Divider, &View);
	Divider.HMargin(0.75f, &Divider);
	Divider.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.09f), IGraphics::CORNER_ALL, 1.0f);
	View.HSplitTop(3.0f, nullptr, &View);

	if(DoEntry(&pPopupContext->m_MutePlayerButton, FontIcons::FONT_ICON_BAN, Localize("Mute player"), pPopupContext->m_PlayerLine && !pPopupContext->m_LocalPlayer, ColorRGBA(1.0f, 0.50f, 0.52f, 1.0f)))
	{
		pChat->GameClient()->m_aClients[pPopupContext->m_ClientId].m_ChatIgnore = true;
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(DoEntry(&pPopupContext->m_AddBlockedWordButton, FontIcons::FONT_ICON_COMMENT_SLASH, Localize("Add to blocked words"), pPopupContext->m_aText[0] != '\0', ColorRGBA(1.0f, 0.67f, 0.45f, 1.0f)))
	{
		pChat->AddTextToBlockWords(pPopupContext->m_aText);
		return CUi::POPUP_CLOSE_CURRENT;
	}
	if(DoEntry(&pPopupContext->m_CopyNameButton, FontIcons::FONT_ICON_USER, Localize("Copy name"), pPopupContext->m_PlayerLine && pPopupContext->m_aName[0] != '\0', ColorRGBA(0.78f, 0.88f, 0.95f, 1.0f)))
	{
		pChat->Input()->SetClipboardText(pPopupContext->m_aName);
		return CUi::POPUP_CLOSE_CURRENT;
	}

	return CUi::POPUP_KEEP_OPEN;
}

CUi::EPopupMenuFunctionResult CChat::PopupLanguageMenu(void *pContext, CUIRect View, bool Active)
{
	CLanguagePopupContext *pPopupContext = static_cast<CLanguagePopupContext *>(pContext);
	CChat *pChat = pPopupContext->m_pChat;
	CUi *pUi = pChat->Ui();
	pPopupContext->InitLabelUiElements(pUi);

	const float Margin = 3.0f;
	View.Margin(Margin, &View);

	const float FontSize = 7.5f;
	const float TitleHeight = 16.0f;
	const float ToggleHeight = 16.0f;
	const float DropdownLabelHeight = 11.0f;
	const float DropdownHeight = 18.0f;
	const float SectionSpacing = 4.0f;

	ColorRGBA OptionSelectedColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmTranslateMenuOptionSelected, true));
	ColorRGBA OptionNormalColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmTranslateMenuOptionNormal, true));

	// 标题
	CUIRect TitleRect;
	View.HSplitTop(TitleHeight, &TitleRect, &View);
	static CButtonContainer s_CloseButton;
	CUIRect CloseButton;
	TitleRect.VSplitRight(22.0f, &TitleRect, &CloseButton);
	if(pUi->DoButton_FontIcon(&s_CloseButton, FontIcons::FONT_ICON_XMARK, 0, &CloseButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL))
		return CUi::POPUP_CLOSE_CURRENT;
	DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_TITLE], TitleRect, Localize("Translation Settings"), FontSize, TEXTALIGN_MC);
	View.HSplitTop(SectionSpacing, nullptr, &View);

	// 自动入站翻译开关
	{
		CUIRect ToggleRect;
		View.HSplitTop(ToggleHeight, &ToggleRect, &View);

		const bool InboundEnabled = g_Config.m_QmTranslateAuto != 0;
		const ColorRGBA ToggleColor = InboundEnabled ? OptionSelectedColor : OptionNormalColor;
		ToggleRect.Draw(ToggleColor, IGraphics::CORNER_ALL, 4.0f);

		static int s_InboundToggleId = 0;
		if(Active && pUi->DoButtonLogic(&s_InboundToggleId, 0, &ToggleRect, BUTTONFLAG_LEFT))
		{
			g_Config.m_QmTranslateAuto = InboundEnabled ? 0 : 1;
			return CUi::POPUP_KEEP_OPEN;
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Auto-translate incoming messages"), InboundEnabled ? Localize("On") : Localize("Off"));
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_INBOUND_TOGGLE], ToggleRect, aBuf, FontSize, TEXTALIGN_MC);
	}
	View.HSplitTop(SectionSpacing, nullptr, &View);

	// 自动出站翻译开关
	{
		CUIRect ToggleRect;
		View.HSplitTop(ToggleHeight, &ToggleRect, &View);

		const bool OutboundEnabled = g_Config.m_QmTranslateAutoOutgoing != 0;
		const ColorRGBA ToggleColor = OutboundEnabled ? OptionSelectedColor : OptionNormalColor;
		ToggleRect.Draw(ToggleColor, IGraphics::CORNER_ALL, 4.0f);

		static int s_OutboundToggleId = 0;
		if(Active && pUi->DoButtonLogic(&s_OutboundToggleId, 0, &ToggleRect, BUTTONFLAG_LEFT))
		{
			g_Config.m_QmTranslateAutoOutgoing = OutboundEnabled ? 0 : 1;
			return CUi::POPUP_KEEP_OPEN;
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Auto-translate outgoing messages"), OutboundEnabled ? Localize("On") : Localize("Off"));
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_OUTBOUND_TOGGLE], ToggleRect, aBuf, FontSize, TEXTALIGN_MC);
	}
	View.HSplitTop(SectionSpacing, nullptr, &View);

	// 语言/后端名称数组（用于 DoDropDown）
	static const char *s_apLangNames[] = {"中文", "English", "日本語", "한국어", "繁體中文", "Русский", "Deutsch", "Français", "Español", "Português"};
	static const char *s_apLangCodes[] = {"zh", "en", "ja", "ko", "zh-TW", "ru", "de", "fr", "es", "pt"};
	const char *apBackendNames[] = {Localize("LLM API"), Localize("Tencent Cloud"), Localize("LibreTranslate"), Localize("FTAPI")};
	static const char *s_apBackendCodes[] = {"llm", "tencentcloud", "libretranslate", "ftapi"};

	auto FindIndex = [](const char *pValue, const char **apCodes, int Count) -> int {
		for(int i = 0; i < Count; ++i)
			if(str_comp(pValue, apCodes[i]) == 0)
				return i;
		return 0;
	};

	// 入站语言标签 + 下拉框
	{
		CUIRect LabelRect, DropdownRect;
		View.HSplitTop(DropdownLabelHeight, &LabelRect, &View);
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_INBOUND_LANG], LabelRect, Localize("Incoming language"), FontSize, TEXTALIGN_ML);
		View.HSplitTop(DropdownHeight, &DropdownRect, &View);

		const int OldSel = FindIndex(g_Config.m_QmTranslateTarget, s_apLangCodes, std::size(s_apLangCodes));
		const int NewSel = pUi->DoDropDown(&DropdownRect, OldSel, s_apLangNames, std::size(s_apLangNames), pPopupContext->m_InboundLangDropDownState, Active);
		if(NewSel != OldSel)
			str_copy(g_Config.m_QmTranslateTarget, s_apLangCodes[NewSel], sizeof(g_Config.m_QmTranslateTarget));
	}
	View.HSplitTop(SectionSpacing, nullptr, &View);

	// 出站语言标签 + 下拉框
	{
		CUIRect LabelRect, DropdownRect;
		View.HSplitTop(DropdownLabelHeight, &LabelRect, &View);
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_OUTBOUND_LANG], LabelRect, Localize("Outgoing language"), FontSize, TEXTALIGN_ML);
		View.HSplitTop(DropdownHeight, &DropdownRect, &View);

		const int OldSel = FindIndex(g_Config.m_QmTranslateOutgoingTarget, s_apLangCodes, std::size(s_apLangCodes));
		const int NewSel = pUi->DoDropDown(&DropdownRect, OldSel, s_apLangNames, std::size(s_apLangNames), pPopupContext->m_OutboundLangDropDownState, Active);
		if(NewSel != OldSel)
			str_copy(g_Config.m_QmTranslateOutgoingTarget, s_apLangCodes[NewSel], sizeof(g_Config.m_QmTranslateOutgoingTarget));
	}
	View.HSplitTop(SectionSpacing, nullptr, &View);

	// 翻译后端标签 + 下拉框
	{
		CUIRect LabelRect, DropdownRect;
		View.HSplitTop(DropdownLabelHeight, &LabelRect, &View);
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_BACKEND], LabelRect, Localize("Translation service"), FontSize, TEXTALIGN_ML);
		View.HSplitTop(DropdownHeight, &DropdownRect, &View);

		const int OldSel = FindIndex(g_Config.m_QmTranslateBackend, s_apBackendCodes, std::size(s_apBackendCodes));
		const int NewSel = pUi->DoDropDown(&DropdownRect, OldSel, apBackendNames, std::size(apBackendNames), pPopupContext->m_BackendDropDownState, Active);
		if(NewSel != OldSel)
			str_copy(g_Config.m_QmTranslateBackend, s_apBackendCodes[NewSel], sizeof(g_Config.m_QmTranslateBackend));
	}

	// 后端未配置警告
	const char *pConfigWarning = ChatTranslateBackendWarning();
	if(pConfigWarning != nullptr)
	{
		View.HSplitTop(SectionSpacing, nullptr, &View);
		CUIRect WarningRect, WarningLabelRect;
		View.HSplitTop(ToggleHeight, &WarningRect, &View);
		WarningRect.Draw(ColorRGBA(0.7f, 0.3f, 0.3f, 0.6f), IGraphics::CORNER_ALL, 4.0f);
		WarningRect.VMargin(4.0f, &WarningLabelRect);
		DoCachedChatPopupLabel(pUi, pPopupContext->m_aLabelUiElements[CLanguagePopupContext::LABEL_WARNING], WarningLabelRect, pConfigWarning, FontSize, TEXTALIGN_ML);
	}

	return CUi::POPUP_KEEP_OPEN;
}

bool CChat::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(m_Mode == MODE_NONE)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}
