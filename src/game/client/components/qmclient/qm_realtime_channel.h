#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_CHANNEL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_REALTIME_CHANNEL_H

#include <cstddef>
#include <deque>
#include <string>

enum class EQmRealtimeConnectionState
{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	BACKOFF,
};

class CQmRealtimeChannel
{
	static constexpr size_t MAX_OUTGOING_MESSAGES = 64;
	static constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024;
	static constexpr double INITIAL_RETRY_DELAY = 1.0;
	static constexpr double MAX_RETRY_DELAY = 60.0;

	EQmRealtimeConnectionState m_State = EQmRealtimeConnectionState::DISCONNECTED;
	std::string m_Address;
	std::deque<std::string> m_Outgoing;
	int m_RetryCount = 0;
	double m_NextRetryAt = 0.0;

public:
	void SetAddress(const char *pAddress);
	const std::string &Address() const { return m_Address; }

	void BeginConnect(double Now);
	void MarkConnected();
	void MarkDisconnected(double Now);
	bool ShouldConnect(double Now) const;
	EQmRealtimeConnectionState State() const { return m_State; }
	int RetryCount() const { return m_RetryCount; }

	bool QueueOutgoing(const char *pData, size_t Size);
	bool PopOutgoing(std::string &Data);
	void ClearOutgoing() { m_Outgoing.clear(); }
	size_t OutgoingCount() const { return m_Outgoing.size(); }
};

#endif
