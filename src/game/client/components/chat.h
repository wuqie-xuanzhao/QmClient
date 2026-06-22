/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_CHAT_H
#define GAME_CLIENT_COMPONENTS_CHAT_H
#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/shared/ringbuffer.h>

#include <generated/protocol7.h>

#include <game/client/component.h>
#include <game/client/components/qmclient/hud_notifications/hud_notifications.h>
#include <game/client/lineinput.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

class CTranslateResponse
{
public:
	bool m_Error = false;
	char m_Text[1024] = "";
	char m_Language[16] = "";
};

constexpr auto SAVES_FILE = "ddnet-saves.txt";

class CChat : public CComponent
{
	static constexpr float CHAT_HEIGHT_FULL = 200.0f;
	static constexpr float CHAT_HEIGHT_MIN = 50.0f;
	static constexpr float CHAT_FONTSIZE_WIDTH_RATIO = 2.5f;

	static constexpr float CHAT_ANIM_SLIDE_OUT_OFFSET = 60.0f; // 被挤出可见区域时的水平偏移量
	static constexpr float CHAT_VISIBLE_SECONDS_NO_FOCUS = 16.0f; // 聊天折叠时保留消息时长（秒）

	enum
	{
		MAX_LINES = 64,
		MAX_LINE_LENGTH = 256
	};

	CLineInputBuffered<MAX_LINE_LENGTH> m_Input;
	class CLine
	{
	public:
		CLine();
		void Reset(CChat &This);

		bool m_Initialized;
		int64_t m_Time;
		float m_aYOffset[2];
		int m_ClientId;
		int m_TeamNumber;
		bool m_Team;
		bool m_Whisper;
		int m_NameColor;
		char m_aName[64];
		char m_aText[MAX_LINE_LENGTH];
		bool m_Friend;
		bool m_Highlighted;
		bool m_ForceVisible;
		QmHudNotifications::EServerMessageClass m_ServerMessageClass;
		std::optional<ColorRGBA> m_CustomColor;

		STextContainerIndex m_TextContainerIndex;
		int m_QuadContainerIndex;

		std::shared_ptr<CManagedTeeRenderInfo> m_pManagedTeeRenderInfo;

		float m_TextYOffset;
		float m_CutOffProgress;

		int m_TimesRepeated;

		// 翻译标识符（每次内容变更时递增）
		unsigned int m_TranslationId = 0;

		std::shared_ptr<CTranslateResponse> m_pTranslateResponse;
	};

	bool m_PrevScoreBoardShowed;
	bool m_PrevShowChat;
	int64_t m_LastAnimUpdateTime;

	CLine m_aLines[MAX_LINES];
	int m_CurrentLine;
	int m_BacklogCurLine;
	bool m_ScrollbarDragging;
	float m_ScrollbarDragOffset;
	std::optional<vec2> m_LastMousePos;
	bool m_MouseIsPress;
	vec2 m_MousePress;
	vec2 m_MouseRelease;

	enum
	{
		// client IDs for special messages
		CLIENT_MSG = -2,
		SERVER_MSG = -1,
	};

	enum
	{
		MODE_NONE = 0,
		MODE_ALL,
		MODE_TEAM,
	};

	enum
	{
		CHAT_SERVER = 0,
		CHAT_HIGHLIGHT,
		CHAT_CLIENT,
		CHAT_NUM,
	};

	int m_Mode;
	bool m_Show;
	bool m_CompletionUsed;
	int m_CompletionChosen;
	char m_aCompletionBuffer[MAX_LINE_LENGTH];
	int m_PlaceholderOffset;
	int m_PlaceholderLength;
	static char ms_aDisplayText[MAX_LINE_LENGTH];
	class CRateablePlayer
	{
	public:
		int m_ClientId;
		int m_Score;
	};
	CRateablePlayer m_aPlayerCompletionList[MAX_CLIENTS];
	int m_PlayerCompletionListLength;

	struct CCommand
	{
		char m_aName[IConsole::TEMPCMD_NAME_LENGTH];
		char m_aParams[IConsole::TEMPCMD_PARAMS_LENGTH];
		char m_aHelpText[IConsole::TEMPCMD_HELP_LENGTH];

		CCommand() = default;
		CCommand(const char *pName, const char *pParams, const char *pHelpText)
		{
			str_copy(m_aName, pName);
			str_copy(m_aParams, pParams);
			str_copy(m_aHelpText, pHelpText);
		}

		bool operator<(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) < 0; }
		bool operator<=(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) <= 0; }
		bool operator==(const CCommand &Other) const { return str_comp(m_aName, Other.m_aName) == 0; }
	};

	std::vector<CCommand> m_vServerCommands;
	bool m_ServerCommandsNeedSorting;

	struct CHistoryEntry
	{
		int m_Team;
		char m_aText[1];
	};
	CHistoryEntry *m_pHistoryEntry;
	CStaticRingBuffer<CHistoryEntry, 64 * 1024, CRingBufferBase::FLAG_RECYCLE> m_History;
	int m_PendingChatCounter;
	int64_t m_LastChatSend;
	int64_t m_aLastSoundPlayed[CHAT_NUM];
	bool m_IsInputCensored;
	char m_aCurrentInputText[MAX_LINE_LENGTH];
	bool m_EditingNewLine;
	char m_aSavedInputText[MAX_LINE_LENGTH];
	bool m_SavedInputPending;
	char m_aChatLogLastCleanupDate[11];

	bool m_ServerSupportsCommandInfo;
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConChat(IConsole::IResult *pResult, void *pUserData);
	static void ConShowChat(IConsole::IResult *pResult, void *pUserData);
	static void ConEcho(IConsole::IResult *pResult, void *pUserData);
	static void ConClearChat(IConsole::IResult *pResult, void *pUserData);

	static void ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	bool LineShouldHighlight(const char *pLine, const char *pName);
	void StoreSave(const char *pText);
	bool EnsureChatLogFolder() const;
	void CleanupOldChatLogs(const char *pToday);
	void SaveChatLogLine(int ClientId, int Team, const char *pLine);
	const CCommand *FindServerCommand(const char *pName) const;
	const char *LocalizeCommandPreviewText(const char *pText) const;
	bool BuildCommandUsagePreview(const char *pInput, char *pBuf, size_t BufSize) const;
	void SendChatQueued(int Team, const char *pLine, bool AllowOutgoingTranslation);
	int CountInitializedLines() const;
	int CountVisibleLinesFrom(int BacklogLine) const;

	static float EaseInQuad(float t);
	static float CalculateCutOffAlpha(float CutOffT);
	static float CalculateCutOffOffsetX(float CutOffT);

	friend class CBindChat;
	friend class CTranslate;
	friend class CTClient;
	friend class CPieMenu;

	// 翻译按钮状态
	struct STranslateButtonState
	{
		bool m_IsPressed = false;
		bool m_RectValid = false;
		bool m_IconUiElementInit = false;
		float m_X = 0.0f;
		float m_Y = 0.0f;
		float m_W = 0.0f;
		float m_H = 0.0f;
		bool m_AutoTranslateEnabled = false;
		CUIElement m_IconUiElement;
	};
	STranslateButtonState m_TranslateButton;

	// 翻译菜单下拉框展开状态
	enum class ETranslateDropdown : int
	{
		NONE = 0,
		INBOUND_LANG,
		OUTBOUND_LANG,
		BACKEND,
	};

	// 语言菜单
	class CLanguagePopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;

		// DoDropDown 状态（使用游戏自带下拉框组件）
		CUi::SDropDownState m_InboundLangDropDownState;
		CUi::SDropDownState m_OutboundLangDropDownState;
		CUi::SDropDownState m_BackendDropDownState;

		enum
		{
			LABEL_TITLE = 0,
			LABEL_INBOUND_TOGGLE,
			LABEL_OUTBOUND_TOGGLE,
			LABEL_INBOUND_LANG,
			LABEL_OUTBOUND_LANG,
			LABEL_BACKEND,
			LABEL_WARNING,
			LABEL_COUNT,
		};
		CUIElement m_aLabelUiElements[LABEL_COUNT];
		bool m_LabelUiElementsInit = false;

		// 菜单动画状态
		int64_t m_OpenTime = 0;
		float m_AnimationProgress = 1.0f;

		void InitLabelUiElements(CUi *pUi)
		{
			if(m_LabelUiElementsInit)
				return;
			for(CUIElement &LabelUiElement : m_aLabelUiElements)
				LabelUiElement.Init(pUi, 1);
			m_LabelUiElementsInit = true;
		}
	};
	CLanguagePopupContext m_LanguagePopupContext;
	bool m_LanguageMenuOpen = false;

	class CChatLinePopupContext : public SPopupMenuId
	{
	public:
		CChat *m_pChat = nullptr;
		int m_ClientId = CLIENT_MSG;
		int m_TeamNumber = 0;
		char m_aName[64] = "";
		char m_aPlayerName[MAX_NAME_LENGTH] = "";
		char m_aText[MAX_LINE_LENGTH] = "";
		bool m_PlayerLine = false;
		bool m_LocalPlayer = false;

		CButtonContainer m_CopyButton;
		CButtonContainer m_AddOneButton;
		CButtonContainer m_ReplyButton;
		CButtonContainer m_SpectateButton;
		CButtonContainer m_MutePlayerButton;
		CButtonContainer m_AddBlockedWordButton;
		CButtonContainer m_CopyNameButton;
	};
	CChatLinePopupContext m_ChatLinePopupContext;

public:
	CChat();
	int Sizeof() const override { return sizeof(*this); }

	static constexpr float MESSAGE_TEE_PADDING_RIGHT = 0.5f;

	static int ClampBacklogLine(int Line, int TotalLines, int VisibleLines)
	{
		const int MaxScroll = std::max(0, TotalLines - VisibleLines);
		return std::clamp(Line, 0, MaxScroll);
	}
	static int ScrollbarValueToBacklogLine(float Value, int MaxScroll)
	{
		const float ClampedValue = std::clamp(Value, 0.0f, 1.0f);
		return std::clamp((int)std::round((1.0f - ClampedValue) * MaxScroll), 0, MaxScroll);
	}
	static float BacklogLineToScrollbarValue(int Line, int MaxScroll)
	{
		if(MaxScroll <= 0)
			return 1.0f;
		return 1.0f - std::clamp(Line, 0, MaxScroll) / (float)MaxScroll;
	}
	static bool IsCopyClickDrag(vec2 Press, vec2 Release)
	{
		return length(Release - Press) <= 5.0f;
	}
	static bool ShouldBlockLiveDirectorChatCommand(const char *pLine)
	{
		if(pLine == nullptr)
			return false;

		const char *pCommand = str_utf8_skip_whitespaces(pLine);
		const char *pRest = str_startswith_nocase(pCommand, "/pause");
		if(pRest == nullptr)
			return false;

		return pRest[0] == '\0' || str_utf8_skip_whitespaces(pRest) != pRest;
	}
	static bool AppendBlockWordToList(char *pList, size_t ListSize, const char *pWord)
	{
		if(pList == nullptr || pWord == nullptr || pWord[0] == '\0' || ListSize == 0)
			return false;

		const size_t ListLength = str_length(pList);
		const size_t SeparatorLength = ListLength > 0 ? 1 : 0;
		const size_t WordLength = str_length(pWord);
		if(ListLength + SeparatorLength + WordLength >= ListSize)
			return false;

		if(SeparatorLength > 0)
			str_append(pList, ";", ListSize);

		const size_t WordStart = str_length(pList);
		str_append(pList, pWord, ListSize);
		return (size_t)str_length(pList) > WordStart;
	}
	static bool BuildWhisperCommand(char *pBuf, size_t BufSize, const char *pName, const char *pMessage)
	{
		if(pBuf == nullptr || pName == nullptr || pName[0] == '\0' || pMessage == nullptr || BufSize == 0)
			return false;

		str_copy(pBuf, "/w \"", BufSize);
		char *pDst = pBuf + str_length(pBuf);
		str_escape(&pDst, pName, pBuf + BufSize);
		str_append(pBuf, "\" ", BufSize);
		str_append(pBuf, pMessage, BufSize);
		return true;
	}
	static bool BuildSpectateCommand(char *pBuf, size_t BufSize, const char *pName)
	{
		if(pBuf == nullptr || pName == nullptr || pName[0] == '\0' || BufSize == 0)
			return false;

		str_copy(pBuf, "say /spec \"", BufSize);
		char *pDst = pBuf + str_length(pBuf);
		str_escape(&pDst, pName, pBuf + BufSize);
		str_append(pBuf, "\"", BufSize);
		return true;
	}
	static const char *MessageNamePrefixForClientId(int ClientId)
	{
		if(ClientId == CLIENT_MSG)
			return "— ";
		return "";
	}
	static QmHudNotifications::EServerMessageClass ResolveLineServerMessageClass(int ClientId, const char *pLine, std::optional<QmHudNotifications::EServerMessageClass> KnownServerMessageClass = std::nullopt)
	{
		if(ClientId != SERVER_MSG)
			return QmHudNotifications::EServerMessageClass::None;
		if(KnownServerMessageClass.has_value())
			return *KnownServerMessageClass;
		return QmHudNotifications::ServerMessageClass(pLine, QmHudNotifications::ESoloPrompt::None);
	}

	bool IsActive() const { return m_Mode != MODE_NONE; }
	const char *GetInputText() const { return m_Input.GetString(); }
	void AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible = false);
	void AddLine(int ClientId, int Team, const char *pLine, bool ForceVisible, std::optional<QmHudNotifications::EServerMessageClass> KnownServerMessageClass);
	void EnableMode(int Team);
	void DisableMode();
	void SaveDraft();
	void RegisterCommand(const char *pName, const char *pParams, const char *pHelpText);
	void UnregisterCommand(const char *pName);
	void Echo(const char *pString);
	void Echo(const char *pString, bool ForceVisible);

	void OnWindowResize() override;
	void OnConsoleInit() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;
	void OnPrepareLines(float y);
	void Reset();
	void OnRelease() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnInit() override;

	void RebuildChat();
	void ClearLines();

	void EnsureCoherentFontSize() const;
	void EnsureCoherentWidth() const;

	float FontSize() const { return g_Config.m_ClChatFontSize / 10.0f; }
	float MessagePaddingX() const { return FontSize() * (5 / 6.f); }
	float MessagePaddingY() const { return FontSize() * (1 / 6.f); }
	float MessageTeeSize() const { return FontSize() * (7 / 6.f); }
	float MessageRounding() const { return FontSize() * (1 / 2.f); }

	// 翻译按钮相关方法
	vec2 GetChatMousePos() const;
	void RenderTranslateButton(const CUIRect &ButtonRect);
	void ToggleAutoTranslate();
	void OpenLanguageMenu();
	void CloseLanguageMenu();
	bool IsLanguageMenuOpen() const { return m_LanguageMenuOpen; }
	bool TranslateVisibleChatLines();
	static bool IsManualVisibleTranslateCandidate(int ClientId, bool HasText, bool HasTranslateResponse, const int *pLocalIds, size_t NumLocalIds)
	{
		if(!HasText || HasTranslateResponse)
			return false;
		if(ClientId < 0)
			return false;
		for(size_t i = 0; i < NumLocalIds; ++i)
		{
			if(pLocalIds[i] >= 0 && ClientId == pLocalIds[i])
				return false;
		}
		return true;
	}
	static CUi::EPopupMenuFunctionResult PopupLanguageMenu(void *pContext, CUIRect View, bool Active);
	void OpenChatLineMenu(const CLine &Line, vec2 ChatMousePos);
	void CloseChatLineMenu();
	static CUi::EPopupMenuFunctionResult PopupChatLineMenu(void *pContext, CUIRect View, bool Active);
	void AddTextToBlockWords(const char *pText);
	void ReplyToChatLine(const CChatLinePopupContext &Context);
	void RepeatChatLine(const CChatLinePopupContext &Context);
	void SpectateChatLine(const CChatLinePopupContext &Context);

	// 聊天行索引辅助方法（用于翻译任务的内存安全）
	int GetLineIndex(const CLine *pLine) const;
	CLine *GetLineByIndex(int Index);
	void InvalidateLineTranslation(CLine &Line);

	// ----- send functions -----

	// Sends a chat message to the server.
	//
	// @param Team MODE_ALL=0 MODE_TEAM=1
	// @param pLine the chat message
	void SendChat(int Team, const char *pLine);
	// Sends a chat message using the specified connection (main/dummy).
	void SendChatOnConn(int Conn, int Team, const char *pLine);

	// Sends a chat message to the server.
	//
	// It uses a queue with a maximum of 3 entries
	// that ensures there is a minimum delay of one second
	// between sent messages.
	//
	// It uses team or public chat depending on m_Mode.
	void SendChatQueued(const char *pLine);
};
#endif
