#ifndef ENGINE_CLIENT_GAME_PING_H
#define ENGINE_CLIENT_GAME_PING_H

#include <engine/shared/uuid_manager.h>

#include <algorithm>
#include <cstdint>

struct SManualPingProbe
{
	int64_t m_StartTime = -1;

	void Reset()
	{
		m_StartTime = -1;
	}

	bool Begin(int64_t Now)
	{
		if(m_StartTime >= 0)
			return false;

		m_StartTime = Now;
		return true;
	}

	bool HandleTimeout(int64_t Now, int64_t Frequency)
	{
		if(m_StartTime < 0 || Now - m_StartTime < 2 * Frequency)
			return false;
		m_StartTime = -1;
		return true;
	}

	bool HandlePong(int64_t Now, int64_t Frequency, float &RttMs)
	{
		if(m_StartTime < 0)
			return false;

		RttMs = std::max<int64_t>(0, Now - m_StartTime) * 1000 / (float)Frequency;
		m_StartTime = -1;
		return true;
	}
};

struct SGamePingProbe
{
	int64_t m_StartTime = -1;
	int64_t m_NextTime = -1;
	int m_RttMs = -1;
	CUuid m_Uuid = UUID_ZEROED;
	bool m_Legacy = false;

	void Reset()
	{
		m_StartTime = -1;
		m_NextTime = -1;
		m_RttMs = -1;
		m_Uuid = UUID_ZEROED;
		m_Legacy = false;
	}

	void Begin(const CUuid &Uuid, int64_t Now, int64_t Frequency)
	{
		m_StartTime = Now;
		m_NextTime = Now + Frequency;
		m_Uuid = Uuid;
		m_Legacy = false;
	}

	void BeginLegacy(int64_t Now, int64_t Frequency)
	{
		m_StartTime = Now;
		m_NextTime = Now + Frequency;
		m_Uuid = UUID_ZEROED;
		m_Legacy = true;
	}

	bool HandlePong(const CUuid &Uuid, int64_t Now, int64_t Frequency)
	{
		if(m_StartTime < 0 || m_Legacy || Uuid != m_Uuid)
			return false;

		m_RttMs = (int)std::max<int64_t>(0, (Now - m_StartTime) * 1000 / Frequency);
		m_StartTime = -1;
		m_NextTime = Now + Frequency;
		m_Uuid = UUID_ZEROED;
		m_Legacy = false;
		return true;
	}

	bool HandleLegacyPong(int64_t Now, int64_t Frequency)
	{
		if(m_StartTime < 0 || !m_Legacy)
			return false;
		m_RttMs = (int)std::max<int64_t>(0, (Now - m_StartTime) * 1000 / Frequency);
		m_StartTime = -1;
		m_NextTime = Now + Frequency;
		m_Legacy = false;
		return true;
	}

	bool HandleTimeout(int64_t Now, int64_t Frequency)
	{
		if(m_StartTime < 0 || Now - m_StartTime < 2 * Frequency)
			return false;

		m_StartTime = -1;
		m_NextTime = Now + Frequency;
		m_RttMs = -1;
		m_Uuid = UUID_ZEROED;
		m_Legacy = false;
		return true;
	}
};

#endif // ENGINE_CLIENT_GAME_PING_H
