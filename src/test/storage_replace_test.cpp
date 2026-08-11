#include "test.h"

#include <engine/config.h>
#include <engine/shared/config.h>
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

TEST(ConfigMigration, KeepsTClientFilesAndSharedDdnetConfigInPlace)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	ASSERT_TRUE(pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		WriteStorageFile(pStorage.get(), s_aConfigDomains[Domain].m_aConfigPath, "canonical\n");
	WriteStorageFile(pStorage.get(), "settings_ddnet.cfg", "bind x say shared\n");
	WriteStorageFile(pStorage.get(), "settings_tclient.cfg", "tc_fast_input 1\n");
	WriteStorageFile(pStorage.get(), "tclient_chatbinds.cfg", "chatbind 1 say untouched\n");
	WriteStorageFile(pStorage.get(), "tclient_profiles.cfg", "profile untouched\n");
	WriteStorageFile(pStorage.get(), "tclient_warlist.cfg", "warlist untouched\n");

	ASSERT_TRUE(QmFinalizeConfigMigration(pStorage.get()));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "settings_ddnet.cfg"), "bind x say shared\n");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "settings_tclient.cfg"), "tc_fast_input 1\n");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "tclient_chatbinds.cfg"), "chatbind 1 say untouched\n");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "tclient_profiles.cfg"), "profile untouched\n");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "tclient_warlist.cfg"), "warlist untouched\n");
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "qmclient/migration_backup_v2/settings_ddnet.cfg"), "bind x say shared\n");
	EXPECT_FALSE(pStorage->FileExists("qmclient/migration_backup_v2/settings_tclient.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->FileExists("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		EXPECT_TRUE(pStorage->RemoveFile(s_aConfigDomains[Domain].m_aConfigPath, IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("settings_tclient.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("tclient_chatbinds.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("tclient_profiles.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("tclient_warlist.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v2/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v2", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient", IStorage::TYPE_SAVE));
}

TEST(ConfigMigration, RestoresOnlyMissingSharedDdnetConfigFromV1Backup)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	ASSERT_TRUE(pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE));
	ASSERT_TRUE(pStorage->CreateFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		WriteStorageFile(pStorage.get(), s_aConfigDomains[Domain].m_aConfigPath, "canonical\n");
	WriteStorageFile(pStorage.get(), "qmclient/migration_backup_v1/settings_ddnet.cfg", "bind x say restored\n");

	ASSERT_TRUE(QmFinalizeConfigMigration(pStorage.get()));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "settings_ddnet.cfg"), "bind x say restored\n");

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		EXPECT_TRUE(pStorage->RemoveFile(s_aConfigDomains[Domain].m_aConfigPath, IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v1/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v2/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v2", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient", IStorage::TYPE_SAVE));
}

TEST(ConfigMigration, RestoresEmptySharedDdnetConfigFromV1Backup)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	ASSERT_TRUE(pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE));
	ASSERT_TRUE(pStorage->CreateFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		WriteStorageFile(pStorage.get(), s_aConfigDomains[Domain].m_aConfigPath, "canonical\n");
	WriteStorageFile(pStorage.get(), "settings_ddnet.cfg", "");
	WriteStorageFile(pStorage.get(), "qmclient/migration_backup_v1/settings_ddnet.cfg", "bind x say restored\n");

	ASSERT_TRUE(QmFinalizeConfigMigration(pStorage.get()));
	EXPECT_EQ(ReadStorageFile(pStorage.get(), "settings_ddnet.cfg"), "bind x say restored\n");

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		EXPECT_TRUE(pStorage->RemoveFile(s_aConfigDomains[Domain].m_aConfigPath, IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v1/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v2/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v2", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient", IStorage::TYPE_SAVE));
}

TEST(ConfigMigration, DoesNotRestoreEmptyV1Backup)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	ASSERT_TRUE(pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE));
	ASSERT_TRUE(pStorage->CreateFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		WriteStorageFile(pStorage.get(), s_aConfigDomains[Domain].m_aConfigPath, "canonical\n");
	WriteStorageFile(pStorage.get(), "qmclient/migration_backup_v1/settings_ddnet.cfg", "");

	ASSERT_TRUE(QmFinalizeConfigMigration(pStorage.get()));
	EXPECT_FALSE(pStorage->FileExists("settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->FileExists("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));

	for(ConfigDomain Domain = ConfigDomain::START; Domain < ConfigDomain::NUM; ++Domain)
		EXPECT_TRUE(pStorage->RemoveFile(s_aConfigDomains[Domain].m_aConfigPath, IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/migration_backup_v1/settings_ddnet.cfg", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFile("qmclient/config_migration_v2.done", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v1", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient/migration_backup_v2", IStorage::TYPE_SAVE));
	EXPECT_TRUE(pStorage->RemoveFolder("qmclient", IStorage::TYPE_SAVE));
}
