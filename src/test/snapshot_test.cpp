#include <base/mem.h>

#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <gtest/gtest.h>

TEST(Snapshot, CrcOneInt)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder::New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 4;
	Flag.m_Y = 0;
	Flag.m_Team = 0;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 4);
}

TEST(Snapshot, CrcTwoInts)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder::New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 1;
	Flag.m_Y = 1;
	Flag.m_Team = 0;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 2);
}

TEST(Snapshot, CrcBiggerInts)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder::New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 99999999;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 100000001);
}

TEST(Snapshot, CrcOverflow)
{
	rust::Box<CSnapshotBuilder> pBuilder = CSnapshotBuilder::New();
	pBuilder->Init(false);

	CNetObj_Flag Flag;
	Flag.m_X = 0xFFFFFFFF;
	Flag.m_Y = 1;
	Flag.m_Team = 1;
	ASSERT_TRUE(pBuilder->NewItem(NETOBJTYPE_FLAG, 0, Flag.AsSlice()));

	CSnapshotBuffer Buffer;
	pBuilder->Finish(Buffer);
	ASSERT_EQ(Buffer.AsSnapshot()->Crc(), 1);
}

TEST(Snapshot, StorageGet)
{
	CSnapshotStorage Storage;
	const char aData[8] = {};
	Storage.Add(10, 1000, 1, aData, 0, nullptr);
	Storage.Add(20, 2000, 2, aData, 0, nullptr);
	Storage.Add(30, 3000, 3, aData, 0, nullptr);
	Storage.Add(40, 4000, 4, aData, 0, nullptr);

	int64_t Tagtime = -1;
	EXPECT_EQ(Storage.Get(40, &Tagtime, nullptr, nullptr), 4);
	EXPECT_EQ(Tagtime, 4000);
	EXPECT_EQ(Storage.Get(10, &Tagtime, nullptr, nullptr), 1);
	EXPECT_EQ(Tagtime, 1000);
	EXPECT_EQ(Storage.Get(30, &Tagtime, nullptr, nullptr), 3);
	EXPECT_EQ(Tagtime, 3000);
	EXPECT_EQ(Storage.Get(50, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(5, nullptr, nullptr, nullptr), -1);
	EXPECT_EQ(Storage.Get(25, nullptr, nullptr, nullptr), -1);
}
