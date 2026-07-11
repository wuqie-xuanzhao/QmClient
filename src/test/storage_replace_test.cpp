#include "test.h"

#include <engine/storage.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{
	void WriteStorageFile(IStorage *pStorage, const char *pFilename, const char *pContent)
	{
		IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		ASSERT_TRUE(File);
		ASSERT_EQ(io_write(File, pContent, str_length(pContent)), str_length(pContent));
		ASSERT_FALSE(io_close(File));
	}

	std::string ReadStorageFile(IStorage *pStorage, const char *pFilename)
	{
		IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
		EXPECT_TRUE(File);
		if(!File)
			return {};
		const int64_t Length = io_length(File);
		std::string Content((size_t)Length, '\0');
		EXPECT_EQ(io_read(File, Content.data(), Content.size()), Length);
		EXPECT_FALSE(io_close(File));
		return Content;
	}
}

TEST(StorageReplace, PromotesTempFileAndRemovesBackup)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteStorageFile(pStorage.get(), "real.txt", "old");
	WriteStorageFile(pStorage.get(), "temp.txt", "new");

	char aBackup[IO_MAX_PATH_LENGTH];
	ASSERT_TRUE(IStorage::ReplaceFileSafely(pStorage.get(), "temp.txt", "real.txt", aBackup, sizeof(aBackup)));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "real.txt"), "new");
	EXPECT_FALSE(pStorage->FileExists("temp.txt", IStorage::TYPE_SAVE));
	EXPECT_FALSE(pStorage->FileExists(aBackup, IStorage::TYPE_SAVE));
}

TEST(StorageReplace, RestoresPreviousFileWhenTempPromotionFails)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteStorageFile(pStorage.get(), "real.txt", "old");

	char aBackup[IO_MAX_PATH_LENGTH];
	EXPECT_FALSE(IStorage::ReplaceFileSafely(pStorage.get(), "missing.txt", "real.txt", aBackup, sizeof(aBackup)));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "real.txt"), "old");
	EXPECT_FALSE(pStorage->FileExists(aBackup, IStorage::TYPE_SAVE));
}

TEST(StorageReplace, ExistingBackupDoesNotDamagePreviousFile)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	WriteStorageFile(pStorage.get(), "real.txt", "old");
	WriteStorageFile(pStorage.get(), "temp.txt", "new");
	WriteStorageFile(pStorage.get(), "temp.txt.backup", "occupied");

	char aBackup[IO_MAX_PATH_LENGTH];
	EXPECT_FALSE(IStorage::ReplaceFileSafely(pStorage.get(), "temp.txt", "real.txt", aBackup, sizeof(aBackup)));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "real.txt"), "old");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "temp.txt"), "new");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "temp.txt.backup"), "occupied");
}
