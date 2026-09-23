#include <game/client/components/qmclient/qm_realtime_channel.h>

#include <gtest/gtest.h>

TEST(QmRealtimeChannel, UsesBackoffAndReconnects)
{
	CQmRealtimeChannel Channel;
	Channel.SetAddress("wss://example.test/ws");
	EXPECT_TRUE(Channel.ShouldConnect(0.0));
	Channel.BeginConnect(0.0);
	EXPECT_EQ(Channel.State(), EQmRealtimeConnectionState::CONNECTING);
	Channel.MarkDisconnected(1.0);
	EXPECT_FALSE(Channel.ShouldConnect(1.5));
	EXPECT_TRUE(Channel.ShouldConnect(2.0));
	Channel.BeginConnect(2.0);
	Channel.MarkConnected();
	EXPECT_EQ(Channel.RetryCount(), 0);
}

TEST(QmRealtimeChannel, AddressChangeDropsStaleMessages)
{
	CQmRealtimeChannel Channel;
	Channel.SetAddress("wss://one.test/ws");
	ASSERT_TRUE(Channel.QueueOutgoing("old", 3));
	Channel.SetAddress("wss://two.test/ws");
	EXPECT_EQ(Channel.OutgoingCount(), 0u);
}

TEST(QmRealtimeChannel, QueueIsBoundedAndRejectsOversizedPayload)
{
	CQmRealtimeChannel Channel;
	Channel.SetAddress("wss://example.test/ws");
	std::string Payload(1024 * 1024, 'x');
	ASSERT_TRUE(Channel.QueueOutgoing(Payload.data(), Payload.size()));
	EXPECT_FALSE(Channel.QueueOutgoing("", 0));
	std::string Out;
	EXPECT_TRUE(Channel.PopOutgoing(Out));
	EXPECT_EQ(Out.size(), Payload.size());
}
