#include <base/system.h>

#include <engine/shared/network.h>

#include <gtest/gtest.h>

#include <chrono>

using namespace std::chrono_literals;

namespace
{

	void InitNetBase()
	{
		static bool s_Initialized = false;
		if(!s_Initialized)
		{
			CNetBase::Init();
			s_Initialized = true;
		}
	}

	unsigned char *PackTestChunk(CNetPacketConstruct *pPacket, int Flags, int DataSize, const unsigned char *pData, bool Sixup, int Sequence = 17)
	{
		CNetChunkHeader Header;
		Header.m_Flags = Flags;
		Header.m_Size = DataSize;
		Header.m_Sequence = (Flags & NET_CHUNKFLAG_VITAL) ? Sequence : -1;
		unsigned char *pChunkData = Header.Pack(pPacket->m_aChunkData + pPacket->m_DataSize, Sixup ? 6 : 4);
		mem_copy(pChunkData, pData, DataSize);
		pPacket->m_DataSize = (int)(pChunkData + DataSize - pPacket->m_aChunkData);
		pPacket->m_NumChunks++;
		return pChunkData;
	}

	CNetPacketConstruct BuildTestPacket(bool Sixup)
	{
		CNetPacketConstruct Packet;
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_Flags = 0;
		Packet.m_Ack = 234;
		const unsigned char aChunk1[] = {'h', 'e', 'l', 'l', 'o'};
		const unsigned char aChunk2[] = {'s', 'n', 'a', 'p'};
		PackTestChunk(&Packet, NET_CHUNKFLAG_VITAL, sizeof(aChunk1), aChunk1, Sixup);
		PackTestChunk(&Packet, 0, sizeof(aChunk2), aChunk2, Sixup);
		return Packet;
	}

	void ExpectPacketRoundtrip(const CNetPacketConstruct &Original, SECURITY_TOKEN SecurityToken, bool Sixup)
	{
		CNetPacketConstruct Packet = Original;
		unsigned char aBuffer[NET_MAX_PACKETSIZE];
		const int PackedSize = CNetBase::PackPacket(aBuffer, sizeof(aBuffer), &Packet, SecurityToken, Sixup);
		ASSERT_GT(PackedSize, 0);

		CNetPacketConstruct Unpacked;
		bool UnpackedSixup = Sixup;
		SECURITY_TOKEN UnpackedToken = NET_SECURITY_TOKEN_UNKNOWN;
		SECURITY_TOKEN ResponseToken = NET_SECURITY_TOKEN_UNKNOWN;
		ASSERT_EQ(CNetBase::UnpackPacket(aBuffer, PackedSize, &Unpacked, UnpackedSixup, &UnpackedToken, &ResponseToken), 0);
		EXPECT_EQ(UnpackedSixup, Sixup);
		EXPECT_EQ(Unpacked.m_Flags & ~NET_PACKETFLAG_COMPRESSION, Original.m_Flags);
		EXPECT_EQ(Unpacked.m_Ack, Original.m_Ack);
		EXPECT_EQ(Unpacked.m_NumChunks, Original.m_NumChunks);
		EXPECT_EQ(Unpacked.m_DataSize, Original.m_DataSize);
		EXPECT_EQ(mem_comp(Unpacked.m_aChunkData, Original.m_aChunkData, Original.m_DataSize), 0);
		if(Sixup)
			EXPECT_EQ(UnpackedToken, SecurityToken);
	}

	NETSOCKET BindUdpSocket(int Port)
	{
		NETADDR BindAddr = {};
		BindAddr.type = NETTYPE_IPV4;
		BindAddr.port = Port;
		return net_udp_create(BindAddr);
	}

} // namespace

TEST(Net, Ipv4AndIpv6Work)
{
	NETADDR Bindaddr = {};
	NETSOCKET Socket1;
	NETSOCKET Socket2;

	Bindaddr.type = NETTYPE_IPV4 | NETTYPE_IPV6;
	Socket2 = net_udp_create(Bindaddr);
	do
	{
		Bindaddr.port = secure_rand_below(65535 - 1024) + 1024;
	} while(!(Socket1 = net_udp_create(Bindaddr)));

	NETADDR LocalhostV4;
	NETADDR LocalhostV6;
	NETADDR TargetV4;
	NETADDR TargetV6;
	ASSERT_FALSE(net_addr_from_str(&LocalhostV4, "127.0.0.1"));
	ASSERT_FALSE(net_addr_from_str(&LocalhostV6, "[::1]"));
	TargetV4 = LocalhostV4;
	TargetV6 = LocalhostV6;
	TargetV4.port = Bindaddr.port;
	TargetV6.port = Bindaddr.port;

	NETADDR Addr;
	unsigned char *pData;

	EXPECT_EQ(net_udp_send(Socket2, &TargetV4, "abc", 3), 3);

	EXPECT_EQ(net_socket_read_wait(Socket1, 10s), 1);
	ASSERT_EQ(net_udp_recv(Socket1, &Addr, &pData), 3);
	Addr.port = 0;
	EXPECT_EQ(Addr, LocalhostV4);
	EXPECT_EQ(mem_comp(pData, "abc", 3), 0);

	EXPECT_EQ(net_udp_send(Socket2, &TargetV6, "def", 3), 3);

	EXPECT_EQ(net_socket_read_wait(Socket1, 10s), 1);
	ASSERT_EQ(net_udp_recv(Socket1, &Addr, &pData), 3);
	Addr.port = 0;
	EXPECT_EQ(Addr, LocalhostV6);
	EXPECT_EQ(mem_comp(pData, "def", 3), 0);

	net_udp_close(Socket1);
	net_udp_close(Socket2);
}

TEST(Net, PackPacketKeepsLegacyRoundtrip)
{
	InitNetBase();

	ExpectPacketRoundtrip(BuildTestPacket(false), NET_SECURITY_TOKEN_UNSUPPORTED, false);
	ExpectPacketRoundtrip(BuildTestPacket(true), 0x1234567, true);
}

TEST(Net, PackPacketRejectsTooSmallBuffer)
{
	InitNetBase();

	CNetPacketConstruct Packet;
	mem_zero(&Packet, sizeof(Packet));
	Packet.m_Flags = NET_PACKETFLAG_CONTROL;
	Packet.m_Ack = 1;
	Packet.m_DataSize = 1;
	Packet.m_aChunkData[0] = NET_CTRLMSG_KEEPALIVE;

	unsigned char aBuffer[2];
	EXPECT_EQ(CNetBase::PackPacket(aBuffer, sizeof(aBuffer), &Packet, NET_SECURITY_TOKEN_UNSUPPORTED), -1);
}

TEST(Net, PacketChunkUnpackerSkipsOldVitalChunk)
{
	CNetPacketConstruct Packet;
	mem_zero(&Packet, sizeof(Packet));
	const unsigned char aOldVital[] = {'o', 'l', 'd'};
	const unsigned char aNextChunk[] = {'n', 'e', 'x', 't'};
	PackTestChunk(&Packet, NET_CHUNKFLAG_VITAL, sizeof(aOldVital), aOldVital, false, 0);
	PackTestChunk(&Packet, 0, sizeof(aNextChunk), aNextChunk, false);

	NETADDR Addr = {};
	Addr.type = NETTYPE_IPV4;
	CNetConnection Connection;
	Connection.DirectInit(Addr, NET_SECURITY_TOKEN_UNSUPPORTED, NET_TOKEN_NONE, false);

	CPacketChunkUnpacker Unpacker;
	Unpacker.FeedPacket(Addr, Packet, &Connection, 0);

	CNetChunk Chunk;
	ASSERT_TRUE(Unpacker.UnpackNextChunk(&Chunk));
	EXPECT_EQ(Chunk.m_DataSize, (int)sizeof(aNextChunk));
	EXPECT_EQ(mem_comp(Chunk.m_pData, aNextChunk, sizeof(aNextChunk)), 0);
	EXPECT_FALSE(Unpacker.UnpackNextChunk(&Chunk));
}

TEST(Net, PacketChunkUnpackerRejectsMissingDeclaredChunk)
{
	CNetPacketConstruct Packet;
	mem_zero(&Packet, sizeof(Packet));
	const unsigned char aChunk[] = {'o', 'n', 'e'};
	PackTestChunk(&Packet, 0, sizeof(aChunk), aChunk, false);
	Packet.m_NumChunks++;

	NETADDR Addr = {};
	Addr.type = NETTYPE_IPV4;
	CNetConnection Connection;
	Connection.DirectInit(Addr, NET_SECURITY_TOKEN_UNSUPPORTED, NET_TOKEN_NONE, false);

	CPacketChunkUnpacker Unpacker;
	Unpacker.FeedPacket(Addr, Packet, &Connection, 0);

	CNetChunk Chunk;
	ASSERT_TRUE(Unpacker.UnpackNextChunk(&Chunk));
	EXPECT_EQ(Chunk.m_DataSize, (int)sizeof(aChunk));
	EXPECT_EQ(mem_comp(Chunk.m_pData, aChunk, sizeof(aChunk)), 0);
	EXPECT_FALSE(Unpacker.UnpackNextChunk(&Chunk));
}

TEST(Net, KcpHeaderRejectsInvalidPackets)
{
	unsigned char aPacket[NET_KCP_HEADER_SIZE + 1] = {'Q', 'K', 'C', 'P', 1, 0, 0, 0, 1, 0};
	uint32_t Conv = 0;
	const unsigned char *pPayload = nullptr;
	int PayloadSize = 0;

	EXPECT_FALSE(CNetKcpSession::UnpackHeader(nullptr, sizeof(aPacket), &Conv, &pPayload, &PayloadSize));
	EXPECT_FALSE(CNetKcpSession::UnpackHeader(aPacket, NET_KCP_HEADER_SIZE, &Conv, &pPayload, &PayloadSize));
	EXPECT_TRUE(CNetKcpSession::UnpackHeader(aPacket, sizeof(aPacket), &Conv, &pPayload, &PayloadSize));
	EXPECT_EQ(Conv, 1u);
	EXPECT_EQ(pPayload, aPacket + NET_KCP_HEADER_SIZE);
	EXPECT_EQ(PayloadSize, 1);

	aPacket[0] = 'X';
	EXPECT_FALSE(CNetKcpSession::UnpackHeader(aPacket, sizeof(aPacket), &Conv, &pPayload, &PayloadSize));
	aPacket[0] = 'Q';
	aPacket[4] = 2;
	EXPECT_FALSE(CNetKcpSession::UnpackHeader(aPacket, sizeof(aPacket), &Conv, &pPayload, &PayloadSize));
	aPacket[4] = 1;
	aPacket[8] = 0;
	EXPECT_FALSE(CNetKcpSession::UnpackHeader(aPacket, sizeof(aPacket), &Conv, &pPayload, &PayloadSize));
}

TEST(Net, KcpSessionSendsOverUdpAndRoundtripsPacket)
{
	InitNetBase();

	NETSOCKET Socket1 = nullptr;
	NETSOCKET Socket2 = nullptr;
	int Port1 = 0;
	int Port2 = 0;
	for(int Attempt = 0; Attempt < 100 && (!Socket1 || !Socket2); ++Attempt)
	{
		if(Socket1)
		{
			net_udp_close(Socket1);
			Socket1 = nullptr;
		}
		if(Socket2)
		{
			net_udp_close(Socket2);
			Socket2 = nullptr;
		}
		Port1 = secure_rand_below(65535 - 1024) + 1024;
		Port2 = secure_rand_below(65535 - 1024) + 1024;
		if(Port1 == Port2)
			continue;
		Socket1 = BindUdpSocket(Port1);
		Socket2 = BindUdpSocket(Port2);
	}
	ASSERT_NE(Socket1, nullptr);
	ASSERT_NE(Socket2, nullptr);

	NETADDR Addr1;
	NETADDR Addr2;
	ASSERT_FALSE(net_addr_from_str(&Addr1, "127.0.0.1"));
	ASSERT_FALSE(net_addr_from_str(&Addr2, "127.0.0.1"));
	Addr1.port = Port1;
	Addr2.port = Port2;

	const uint32_t Conv = 0x1234567u;
	CNetKcpSession Sender;
	CNetKcpSession Receiver;
	ASSERT_TRUE(Sender.Init(Socket1, Addr2, Conv));
	ASSERT_TRUE(Receiver.Init(Socket2, Addr1, Conv));

	CNetPacketConstruct Packet;
	mem_zero(&Packet, sizeof(Packet));
	Packet.m_Flags = 0;
	Packet.m_Ack = 42;
	const unsigned char aPayload[] = {'k', 'c', 'p'};
	PackTestChunk(&Packet, 0, sizeof(aPayload), aPayload, false);
	ASSERT_EQ(Sender.SendPacket(&Packet, NET_SECURITY_TOKEN_UNSUPPORTED, false), 0);
	Sender.Flush();

	NETADDR From;
	unsigned char *pUdpData = nullptr;
	ASSERT_EQ(net_socket_read_wait(Socket2, 10s), 1);
	const int UdpBytes = net_udp_recv(Socket2, &From, &pUdpData);
	ASSERT_GT(UdpBytes, NET_KCP_HEADER_SIZE);
	EXPECT_TRUE(CNetKcpSession::IsKcpPacket(pUdpData, UdpBytes));
	ASSERT_TRUE(Receiver.Input(From, pUdpData, UdpBytes, false));

	unsigned char aPacked[NET_MAX_PACKETSIZE];
	const int PackedSize = Receiver.Recv(aPacked, sizeof(aPacked));
	ASSERT_GT(PackedSize, 0);

	CNetPacketConstruct Unpacked;
	bool Sixup = false;
	SECURITY_TOKEN Token = NET_SECURITY_TOKEN_UNKNOWN;
	SECURITY_TOKEN ResponseToken = NET_SECURITY_TOKEN_UNKNOWN;
	ASSERT_EQ(CNetBase::UnpackPacket(aPacked, PackedSize, &Unpacked, Sixup, &Token, &ResponseToken), 0);
	EXPECT_EQ(Unpacked.m_Ack, Packet.m_Ack);
	EXPECT_EQ(Unpacked.m_NumChunks, Packet.m_NumChunks);
	EXPECT_EQ(Unpacked.m_DataSize, Packet.m_DataSize);
	EXPECT_EQ(mem_comp(Unpacked.m_aChunkData, Packet.m_aChunkData, Packet.m_DataSize), 0);

	net_udp_close(Socket1);
	net_udp_close(Socket2);
}
