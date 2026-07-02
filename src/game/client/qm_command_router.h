/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_COMMAND_ROUTER_H
#define GAME_CLIENT_QM_COMMAND_ROUTER_H

#include <base/system.h>

#include <engine/console.h>

#include <generated/protocol.h>

#include <cstdint>

class CGameClient;

enum class EQmCommandTarget
{
	ACTIVE = 0,
	MAIN,
	DUMMY,
};

enum class EQmDummyInputCommand
{
	LEFT = 0,
	RIGHT,
	JUMP,
	HOOK,
	FIRE,
	WEAPON1,
	WEAPON2,
	WEAPON3,
	WEAPON4,
	WEAPON5,
	NEXT_WEAPON,
	PREV_WEAPON,
};

struct SQmDummyCommand
{
	class CQmCommandRouter *m_pRouter = nullptr;
	EQmDummyInputCommand m_Command = EQmDummyInputCommand::LEFT;
};

namespace qm_dummy_command
{
	inline void UpdateInputCounter(int &Value, int State)
	{
		if((Value & 1) != (State != 0 ? 1 : 0))
			Value++;
		Value &= INPUT_STATE_MASK;
	}

	inline bool HasHeldInput(const CNetObj_PlayerInput &Input)
	{
		return Input.m_Direction != 0 ||
		       Input.m_Jump != 0 ||
		       Input.m_Hook != 0 ||
		       (Input.m_Fire & 1) != 0 ||
		       (Input.m_NextWeapon & 1) != 0 ||
		       (Input.m_PrevWeapon & 1) != 0;
	}

	inline int InactiveConn(int ActiveConn)
	{
		return ActiveConn ^ 1;
	}

	inline void BuildSlashCommand(char *pBuf, int BufSize, const char *pCommand, const char *pArgs)
	{
		if(pBuf == nullptr || BufSize <= 0)
			return;

		if(pCommand == nullptr || pCommand[0] == '\0')
		{
			pBuf[0] = '\0';
			return;
		}

		if(pArgs != nullptr && pArgs[0] != '\0')
			str_format(pBuf, BufSize, "/%s %s", pCommand, pArgs);
		else
			str_format(pBuf, BufSize, "/%s", pCommand);
	}
} // namespace qm_dummy_command

class CQmCommandRouter
{
public:
	void Init(CGameClient *pGameClient);
	void OnConsoleInit();
	void OnDummySwap();
	void ResetDummyInputState();
	bool HasManualDummyInput() const;

private:
	static void ConDummyInput(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySay(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyPause(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySpec(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyLock(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySave(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyLoad(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyRescue(IConsole::IResult *pResult, void *pUserData);

	int ConnForTarget(EQmCommandTarget Target) const;
	bool EnsureConnAvailable(int Conn, bool Verbose);
	void ReportDummyUnavailable();
	bool HasHeldDummyInputState() const;
	void ReleaseDummyInput(CNetObj_PlayerInput &Input) const;
	void PrepareDummyInput(CNetObj_PlayerInput &Input, int TargetConn) const;
	void ApplyHeldDummyInput(CNetObj_PlayerInput &Input) const;
	void ApplyDummyInput(EQmDummyInputCommand Command, int State);
	void SendDummyChat(int Team, const char *pLine);
	void SendDummySlashCommand(const char *pCommand, const char *pArgs);

	CGameClient *m_pGameClient = nullptr;
	int m_DummyLeft = 0;
	int m_DummyRight = 0;
	int m_DummyJump = 0;
	int m_DummyHook = 0;
	int m_DummyFire = 0;
	int m_DummyNextWeapon = 0;
	int m_DummyPrevWeapon = 0;
	int64_t m_LastDummyUnavailableLogTime = 0;

	SQmDummyCommand m_DummyLeftCommand{this, EQmDummyInputCommand::LEFT};
	SQmDummyCommand m_DummyRightCommand{this, EQmDummyInputCommand::RIGHT};
	SQmDummyCommand m_DummyJumpCommand{this, EQmDummyInputCommand::JUMP};
	SQmDummyCommand m_DummyHookCommand{this, EQmDummyInputCommand::HOOK};
	SQmDummyCommand m_DummyFireCommand{this, EQmDummyInputCommand::FIRE};
	SQmDummyCommand m_DummyWeapon1Command{this, EQmDummyInputCommand::WEAPON1};
	SQmDummyCommand m_DummyWeapon2Command{this, EQmDummyInputCommand::WEAPON2};
	SQmDummyCommand m_DummyWeapon3Command{this, EQmDummyInputCommand::WEAPON3};
	SQmDummyCommand m_DummyWeapon4Command{this, EQmDummyInputCommand::WEAPON4};
	SQmDummyCommand m_DummyWeapon5Command{this, EQmDummyInputCommand::WEAPON5};
	SQmDummyCommand m_DummyNextWeaponCommand{this, EQmDummyInputCommand::NEXT_WEAPON};
	SQmDummyCommand m_DummyPrevWeaponCommand{this, EQmDummyInputCommand::PREV_WEAPON};
};

#endif
