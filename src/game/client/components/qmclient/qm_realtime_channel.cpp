#include "qm_realtime_channel.h"

#include <algorithm>

void CQmRealtimeChannel::SetAddress(const char *pAddress)
{
	const std::string Address = pAddress != nullptr ? pAddress : "";
	if(Address == m_Address)
		return;
	m_Address = Address;
	m_Outgoing.clear();
	m_RetryCount = 0;
	m_NextRetryAt = 0.0;
	m_State = EQmRealtimeConnectionState::DISCONNECTED;
}

void CQmRealtimeChannel::BeginConnect(const double Now)
{
	if(m_Address.empty() || m_State == EQmRealtimeConnectionState::CONNECTED || m_State == EQmRealtimeConnectionState::CONNECTING)
		return;
	if(m_State == EQmRealtimeConnectionState::BACKOFF && Now < m_NextRetryAt)
		return;
	m_State = EQmRealtimeConnectionState::CONNECTING;
}

void CQmRealtimeChannel::MarkConnected()
{
	m_State = EQmRealtimeConnectionState::CONNECTED;
	m_RetryCount = 0;
	m_NextRetryAt = 0.0;
}

void CQmRealtimeChannel::MarkDisconnected(const double Now)
{
	m_State = EQmRealtimeConnectionState::BACKOFF;
	m_RetryCount = std::min(m_RetryCount + 1, 30);
	const double Delay = std::min(MAX_RETRY_DELAY, INITIAL_RETRY_DELAY * (1 << std::min(m_RetryCount - 1, 6)));
	m_NextRetryAt = Now + Delay;
}

bool CQmRealtimeChannel::ShouldConnect(const double Now) const
{
	return !m_Address.empty() && (m_State == EQmRealtimeConnectionState::DISCONNECTED || (m_State == EQmRealtimeConnectionState::BACKOFF && Now >= m_NextRetryAt));
}

bool CQmRealtimeChannel::QueueOutgoing(const char *pData, const size_t Size)
{
	if(pData == nullptr || Size == 0 || Size > MAX_MESSAGE_SIZE || m_Address.empty())
		return false;
	if(m_Outgoing.size() >= MAX_OUTGOING_MESSAGES)
		m_Outgoing.pop_front();
	m_Outgoing.emplace_back(pData, Size);
	return true;
}

bool CQmRealtimeChannel::PopOutgoing(std::string &Data)
{
	if(m_Outgoing.empty())
		return false;
	Data = std::move(m_Outgoing.front());
	m_Outgoing.pop_front();
	return true;
}
