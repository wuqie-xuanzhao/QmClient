#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LOCAL_FILES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_LOCAL_FILES_H

#include "qm_lyrics_source.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class IEngine;
class IStorage;

namespace QmLyrics
{

	enum class ELocalLyricsFileKind
	{
		LRC,
		ESLRC,
		TTML,
	};

	const char *LocalLyricsFileExtension(ELocalLyricsFileKind Kind);
	const char *LocalLyricsSourceId(ELocalLyricsFileKind Kind);
	EFormat LocalLyricsFormatHint(ELocalLyricsFileKind Kind);

	std::vector<std::string> BuildLocalLyricsCandidatePaths(const SSourceQuery &Query, ELocalLyricsFileKind Kind);
	bool LoadLocalLyricsCandidate(IStorage *pStorage, const char *pPath, ELocalLyricsFileKind Kind, SSourceCandidate *pOut, char *pErr = nullptr, size_t ErrSize = 0);

	struct SLocalMediaFileEntry
	{
		std::string m_Path;
		std::string m_FileName;
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album;
		std::string m_EmbeddedLyrics;
		int m_DurationSec = 0;
		int64_t m_FileSize = 0;
		int64_t m_Modified = 0;
	};

	std::vector<std::string> SplitLocalMediaFolders(std::string_view Folders);
	bool IsLocalMusicFileExtension(std::string_view Path);
	std::string LocalMediaIndexToJson(const std::vector<SLocalMediaFileEntry> &vEntries);
	bool LocalMediaIndexFromJson(std::string_view Json, std::vector<SLocalMediaFileEntry> *pvEntries, char *pErr = nullptr, size_t ErrSize = 0);
	bool LoadLocalMediaIndex(IStorage *pStorage, std::vector<SLocalMediaFileEntry> *pvEntries);
	bool SaveLocalMediaIndex(IStorage *pStorage, const std::vector<SLocalMediaFileEntry> &vEntries);
	std::vector<SSourceCandidate> BuildLocalMusicCandidatesFromIndex(const SSourceQuery &Query, const std::vector<SLocalMediaFileEntry> &vEntries);

	class CLyricsSourceLocalMusicFile : public IQmLyricsSource
	{
	public:
		CLyricsSourceLocalMusicFile(IStorage *pStorage, IEngine *pEngine, const char *pLocalMediaFolders);
		~CLyricsSourceLocalMusicFile() override;

		const char *Id() const override;
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

		bool BusyForTests() const;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};

	class CLyricsSourceLocalLyricsFile : public IQmLyricsSource
	{
	public:
		CLyricsSourceLocalLyricsFile(IStorage *pStorage, ELocalLyricsFileKind Kind);
		~CLyricsSourceLocalLyricsFile() override;

		const char *Id() const override;
		void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) override;
		void Tick() override;
		void Cancel() override;

		bool BusyForTests() const;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace QmLyrics

#endif
