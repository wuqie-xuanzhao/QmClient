#include "qmclient_source_contract_test.h"

#include <gtest/gtest.h>

TEST(QmIconLoaderContract, RejectsIncompleteOrDuplicateKnownManifestEntries)
{
	const std::string Source = ReadRepoFile("src/game/client/qm_icon_manager.cpp");
	const size_t LoadManifest = Source.find("bool CQmIconManager::LoadManifest(");
	const size_t PixelAlignedRect = Source.find("CUIRect CQmIconManager::PixelAlignedRect", LoadManifest);
	ASSERT_NE(LoadManifest, std::string::npos);
	ASSERT_NE(PixelAlignedRect, std::string::npos);
	const std::string Loader = Source.substr(LoadManifest, PixelAlignedRect - LoadManifest);
	EXPECT_NE(Loader.find("bool InvalidKnownEntry = false"), std::string::npos);
	EXPECT_NE(Loader.find("if(Entry.m_Valid)"), std::string::npos);
	EXPECT_NE(Loader.find("LoadedIconCount != static_cast<int>(EQmIcon::COUNT)"), std::string::npos);
	EXPECT_NE(Loader.find("QmIconTextureCanCommit(Texture.IsValid(), Texture.IsNullTexture())"), std::string::npos);
	EXPECT_NE(Loader.find("Atlas.m_LoadedIconCount = LoadedIconCount"), std::string::npos);
	EXPECT_NE(Loader.find("Atlas.m_Type = Msdf ? CQmIconAtlas::EType::MSDF : CQmIconAtlas::EType::ALPHA"), std::string::npos);

	const std::string Header = ReadRepoFile("src/game/client/qm_icon_manager.h");
	EXPECT_NE(Header.find("m_LoadedIconCount == static_cast<int>(EQmIcon::COUNT)"), std::string::npos);
	EXPECT_NE(Header.find("void Swap(CQmIconAtlas &Other)"), std::string::npos);
}

TEST(QmIconLoaderContract, ReloadCommitsCandidateOnlyAfterCompleteLoad)
{
	const std::string Source = ReadRepoFile("src/game/client/qm_icon_manager.cpp");
	const size_t Reload = Source.find("bool CQmIconManager::Reload()");
	const size_t LoadManifestForScale = Source.find("bool CQmIconManager::LoadManifestForScale", Reload);
	ASSERT_NE(Reload, std::string::npos);
	ASSERT_NE(LoadManifestForScale, std::string::npos);
	const std::string ReloadBody = Source.substr(Reload, LoadManifestForScale - Reload);
	EXPECT_NE(ReloadBody.find("CQmIconAtlas Candidate"), std::string::npos);
	EXPECT_NE(ReloadBody.find("if(!Success)"), std::string::npos);
	EXPECT_NE(ReloadBody.find("m_Atlas.Swap(Candidate);"), std::string::npos);
	EXPECT_NE(ReloadBody.find("QmIconAtlasCanRetainOnReloadFailure(IsReady(), m_Atlas.Type(), MsdfSupported)"), std::string::npos);
	EXPECT_NE(ReloadBody.find("if(!RetainedResidentAtlas)"), std::string::npos);
	EXPECT_NE(ReadRepoFile("src/game/client/qm_icon_manager.h").find("void Swap(CQmIconAtlas &Other)"), std::string::npos);
}

TEST(QmIconLoaderContract, MsdfRetryDoesNotReloadResidentAlphaAtlas)
{
	const std::string Source = ReadRepoFile("src/game/client/qm_icon_manager.cpp");
	const size_t Retry = Source.find("bool CQmIconManager::RetryMsdfAtlas()");
	const size_t LoadManifestForScale = Source.find("bool CQmIconManager::LoadManifestForScale", Retry);
	ASSERT_NE(Retry, std::string::npos);
	ASSERT_NE(LoadManifestForScale, std::string::npos);
	const std::string RetryBody = Source.substr(Retry, LoadManifestForScale - Retry);
	EXPECT_NE(RetryBody.find("LoadMsdfManifest(Candidate)"), std::string::npos);
	EXPECT_EQ(RetryBody.find("LoadManifestForScale"), std::string::npos);
	EXPECT_NE(RetryBody.find("m_Atlas.Swap(Candidate);"), std::string::npos);
}
