#include <game/client/components/qmclient/qm_realtime.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>

TEST(QmRealtime, ParsesStateAndBroadcast)
{
	SQmRealtimeMessage Message;
	const char *pState = "{\"type\":\"state\",\"data\":{\"online_users\":12}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pState, std::strlen(pState), Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::STATE);
	EXPECT_TRUE(Message.m_HasOnlineUsers);
	EXPECT_EQ(Message.m_OnlineUsers, 12);
	const char *pBroadcast = "{\"type\":\"broadcast\",\"data\":{\"markdown\":\"hello\",\"version\":2}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pBroadcast, std::strlen(pBroadcast), Message));
	EXPECT_TRUE(Message.m_HasBroadcast);
	EXPECT_EQ(Message.m_BroadcastVersion, 2);
}

TEST(QmRealtime, RejectsOversizedBroadcastPayload)
{
	std::string Markdown(64 * 1024 + 1, 'x');
	const std::string Data = "{\"type\":\"broadcast\",\"data\":{\"markdown\":\"" + Markdown + "\"}}";
	SQmRealtimeMessage Message;
	ASSERT_TRUE(ParseQmRealtimeMessage(Data.data(), Data.size(), Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::BROADCAST);
	EXPECT_FALSE(Message.m_HasBroadcast);
}

TEST(QmRealtime, RejectsMalformedMessages)
{
	SQmRealtimeMessage Message;
	EXPECT_FALSE(ParseQmRealtimeMessage(nullptr, 0, Message));
	EXPECT_FALSE(ParseQmRealtimeMessage("[]", 2, Message));
	EXPECT_FALSE(ParseQmRealtimeMessage("{\"type\":5}", 10, Message));
}

TEST(QmRealtime, ParsesAnonymousEmoticonLaunch)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"emoticon\",\"data\":{\"player_id\":3,\"player_name\":\"tee\",\"emoticon\":5,\"launch\":true,\"super_launch\":false}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_TRUE(Message.m_EmoticonPayloadValid);
	EXPECT_EQ(Message.m_EmoticonPlayerId, 3);
	EXPECT_EQ(Message.m_EmoticonId, 5);
	EXPECT_TRUE(Message.m_EmoticonLaunch);
	EXPECT_FALSE(Message.m_EmoticonSuperLaunch);
	EXPECT_EQ(Message.m_EmoticonPlayerName, "tee");
}

TEST(QmRealtime, ParsesAnonymousEmoticonServerAddress)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"emoticon\",\"data\":{\"player_id\":3,\"player_name\":\"tee\",\"emoticon\":5,\"server_address\":\"[::1]:8303\"}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_EQ(Message.m_EmoticonServerAddress, "[::1]:8303");
}

TEST(QmRealtime, AcceptsLaunchModeAlias)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"emoticon\",\"data\":{\"player_id\":3,\"emoticon\":5,\"launch_mode\":true}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_TRUE(Message.m_EmoticonLaunch);
}

TEST(QmRealtime, ParsesSuperLaunchAlongsideLaunchMode)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"emoticon\",\"data\":{\"player_id\":3,\"emoticon\":5,\"launch_mode\":true,\"super_launch\":true}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_TRUE(Message.m_EmoticonPayloadValid);
	EXPECT_TRUE(Message.m_EmoticonLaunch);
	EXPECT_TRUE(Message.m_EmoticonSuperLaunch);
}

TEST(QmRealtime, ParsesServiceEventsWithoutTreatingThemAsUnknown)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"titles\",\"data\":{\"server_time\":123}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::TITLES);
	EXPECT_TRUE(Message.m_HasRealtimeData);

	const char *pError = "{\"type\":\"error\",\"data\":{\"code\":\"busy\"}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pError, std::strlen(pError), Message));
	EXPECT_EQ(Message.m_Event, EQmRealtimeEvent::ERROR);
	EXPECT_TRUE(Message.m_HasRealtimeData);
}

TEST(QmRealtime, ParsesServiceDataForLocalStateApplication)
{
	SQmRealtimeMessage Message;
	const char *pData = R"({"type":"title_profile","data":{"server_time":1700000000,"playtime_seconds":42,"title":"Dream","bound_name":"Player","style":"exotic_rainbow"}})";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_TRUE(Message.m_HasServerTime);
	EXPECT_EQ(Message.m_ServerTime, 1700000000);
	EXPECT_TRUE(Message.m_HasPlaytimeSeconds);
	EXPECT_EQ(Message.m_PlaytimeSeconds, 42);
	EXPECT_TRUE(Message.m_HasTitleProfile);
	EXPECT_EQ(Message.m_TitleText, "Dream");
	EXPECT_EQ(Message.m_TitleBoundName, "Player");
	EXPECT_EQ(Message.m_TitleStyle, "exotic_rainbow");
}

TEST(QmRealtime, PreservesAnonymousEmoticonIdentityAndSequence)
{
	SQmRealtimeMessage Message;
	const char *pData = "{\"type\":\"emoticon\",\"data\":{\"client_id\":\"abc\",\"sequence\":42,\"player_id\":3,\"emoticon\":5,\"launch_mode\":true}}";
	ASSERT_TRUE(ParseQmRealtimeMessage(pData, std::strlen(pData), Message));
	EXPECT_TRUE(Message.m_HasRealtimeData);
	EXPECT_EQ(Message.m_EmoticonClientId, "abc");
	EXPECT_EQ(Message.m_EmoticonSequence, 42U);
}
