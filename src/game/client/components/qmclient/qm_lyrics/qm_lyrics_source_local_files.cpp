#include "qm_lyrics_source_local_files.h"

#include "qm_lyrics_parser_lrc.h"
#include "qm_lyrics_parser_ttml.h"

#include <base/str.h>
#include <base/system.h>

#include <engine/engine.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <utility>

#if defined(CONF_VIDEORECORDER) && __has_include(<libavformat/avformat.h>) && __has_include(<libavutil/dict.h>)
#define QM_LYRICS_HAS_FFMPEG 1
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}
#else
#define QM_LYRICS_HAS_FFMPEG 0
#endif

namespace QmLyrics
{

	namespace
	{
		constexpr const char *LOCAL_MEDIA_INDEX = "qmclient/lyrics/files-index.json";
		constexpr const char *LOCAL_MEDIA_INDEX_DIR = "qmclient/lyrics";

		struct SLocalMusicScanResult
		{
			std::vector<SLocalMediaFileEntry> m_vIndex;
			std::vector<SSourceCandidate> m_vCandidates;
		};

		bool IsSlash(char C)
		{
			return C == '/' || C == '\\';
		}

		std::string FileNameOnly(std::string_view Path)
		{
			const size_t Slash = Path.find_last_of("/\\");
			return std::string(Slash == std::string_view::npos ? Path : Path.substr(Slash + 1));
		}

		std::string FileNameWithoutExtension(std::string_view Path)
		{
			std::string Name = FileNameOnly(Path);
			const size_t Dot = Name.find_last_of('.');
			if(Dot != std::string::npos)
				Name.resize(Dot);
			return Name;
		}

		std::string ReplaceOrAppendExtension(std::string_view Path, const char *pExtension)
		{
			const size_t Slash = Path.find_last_of("/\\");
			const size_t Dot = Path.find_last_of('.');
			if(Dot != std::string_view::npos && (Slash == std::string_view::npos || Dot > Slash))
			{
				std::string Result(Path.substr(0, Dot));
				Result.append(pExtension);
				return Result;
			}
			std::string Result(Path);
			Result.append(pExtension);
			return Result;
		}

		bool HasExtension(std::string_view Path, const char *pExtension)
		{
			const size_t ExtLen = str_length(pExtension);
			if(Path.size() < ExtLen)
				return false;
			const std::string Tail(Path.substr(Path.size() - ExtLen));
			return str_comp_nocase(Tail.c_str(), pExtension) == 0;
		}

		std::string TrimCopy(std::string_view Text)
		{
			std::string Work(Text);
			const char *pTrimmed = str_utf8_skip_whitespaces(Work.c_str());
			if(pTrimmed != Work.c_str())
				Work.erase(0, pTrimmed - Work.c_str());
			str_utf8_trim_right(Work.data());
			Work.resize(str_length(Work.c_str()));
			return Work;
		}

		bool HasAnyContent(std::string_view Text)
		{
			return !TrimCopy(Text).empty();
		}

		void AppendUnique(std::vector<std::string> *pvPaths, std::string Path)
		{
			Path = TrimCopy(Path);
			if(Path.empty())
				return;
			if(std::find(pvPaths->begin(), pvPaths->end(), Path) == pvPaths->end())
				pvPaths->push_back(std::move(Path));
		}

		std::string JoinPath(std::string_view Dir, std::string_view Name)
		{
			if(Dir.empty())
				return std::string(Name);
			std::string Out(Dir);
			if(!IsSlash(Out.back()))
				Out.push_back('/');
			Out.append(Name);
			return Out;
		}

		int64_t FileSize(std::string_view Path)
		{
			const std::string Work(Path);
			IOHANDLE File = io_open(Work.c_str(), IOFLAG_READ);
			if(!File)
				return 0;
			const int64_t Size = io_length(File);
			io_close(File);
			return Size > 0 ? Size : 0;
		}

		int64_t FileModified(std::string_view Path)
		{
			const std::string Work(Path);
			time_t Created = 0;
			time_t Modified = 0;
			if(fs_file_time(Work.c_str(), &Created, &Modified) != 0)
				return 0;
			return (int64_t)Modified;
		}

		void EscapeJsonString(std::string_view In, std::string *pOut)
		{
			pOut->push_back('"');
			for(char C : In)
			{
				switch(C)
				{
				case '"': pOut->append("\\\""); break;
				case '\\': pOut->append("\\\\"); break;
				case '\b': pOut->append("\\b"); break;
				case '\f': pOut->append("\\f"); break;
				case '\n': pOut->append("\\n"); break;
				case '\r': pOut->append("\\r"); break;
				case '\t': pOut->append("\\t"); break;
				default:
					if((unsigned char)C < 0x20)
					{
						char aBuf[8];
						str_format(aBuf, sizeof(aBuf), "\\u%04x", (unsigned)(unsigned char)C);
						pOut->append(aBuf);
					}
					else
					{
						pOut->push_back(C);
					}
				}
			}
			pOut->push_back('"');
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int64_t JsonInt64(const json_value *pVal, int64_t Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return pVal->u.integer;
			if(pVal->type == json_double)
				return (int64_t)pVal->u.dbl;
			return Default;
		}

		bool EnsureLocalMediaIndexDir(IStorage *pStorage)
		{
			if(pStorage == nullptr)
				return false;
			pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			return pStorage->CreateFolder(LOCAL_MEDIA_INDEX_DIR, IStorage::TYPE_SAVE) || pStorage->FolderExists(LOCAL_MEDIA_INDEX_DIR, IStorage::TYPE_SAVE);
		}

		void FillMetadataFromParsedTrack(const SLyricsTrack &Track, SMatchCandidate *pMetadata)
		{
			if(pMetadata == nullptr)
				return;
			if(Track.m_Title.has_value())
				pMetadata->m_Title = *Track.m_Title;
			if(Track.m_Artist.has_value())
				pMetadata->m_Artist = *Track.m_Artist;
			if(Track.m_Album.has_value())
				pMetadata->m_Album = *Track.m_Album;
			int64_t EndMs = 0;
			for(const SLyricsLine &Line : Track.m_vLines)
				EndMs = std::max(EndMs, Line.m_EndMs);
			if(EndMs > 0)
				pMetadata->m_DurationSec = (int)(EndMs / 1000);
		}

		void TryFillMetadata(std::string_view Text, ELocalLyricsFileKind Kind, SMatchCandidate *pMetadata)
		{
			SLyricsTrack Track;
			char aErr[128];
			bool Parsed = false;
			if(Kind == ELocalLyricsFileKind::TTML)
				Parsed = ParseTtml(Text.data(), Text.size(), &Track, aErr, sizeof(aErr));
			else
				Parsed = ParseLrc(Text.data(), Text.size(), &Track, aErr, sizeof(aErr));
			if(Parsed)
				FillMetadataFromParsedTrack(Track, pMetadata);
		}

		bool ReadLocalTextFile(IStorage *pStorage, const char *pPath, std::string *pOut)
		{
			if(pStorage == nullptr || pPath == nullptr || pPath[0] == '\0' || pOut == nullptr)
				return false;
			char *pText = pStorage->ReadFileStr(pPath, IStorage::TYPE_ALL_OR_ABSOLUTE);
			if(pText == nullptr)
				return false;
			pOut->assign(pText);
			free(pText);
			return true;
		}

		EFormat GuessLyricsFormat(std::string_view Text)
		{
			const std::string Trimmed = TrimCopy(Text);
			if(Trimmed.find("<tt") != std::string::npos || Trimmed.find("<tt:tt") != std::string::npos)
				return EFormat::TTML;
			return EFormat::LRC_ENHANCED;
		}

		SMatchCandidate MatchCandidateFromLocalMedia(const SLocalMediaFileEntry &Entry)
		{
			SMatchCandidate Candidate;
			Candidate.m_Title = Entry.m_Title.empty() ? FileNameWithoutExtension(Entry.m_FileName) : Entry.m_Title;
			Candidate.m_Artist = Entry.m_Artist;
			Candidate.m_Album = Entry.m_Album;
			Candidate.m_FileName = Entry.m_FileName;
			Candidate.m_DurationSec = Entry.m_DurationSec;
			return Candidate;
		}

#if QM_LYRICS_HAS_FFMPEG
		std::string MetadataValue(AVDictionary *pDictionary, const char *const *ppKeys, int NumKeys)
		{
			if(pDictionary == nullptr)
				return "";
			const AVDictionaryEntry *pEntry = nullptr;
			while((pEntry = av_dict_iterate(pDictionary, pEntry)) != nullptr)
			{
				for(int i = 0; i < NumKeys; ++i)
				{
					if(str_comp_nocase(pEntry->key, ppKeys[i]) == 0 && pEntry->value != nullptr)
						return pEntry->value;
				}
			}
			return "";
		}

		std::string FindMetadataValue(AVFormatContext *pContext, const char *const *ppKeys, int NumKeys)
		{
			if(pContext == nullptr)
				return "";
			std::string Value = MetadataValue(pContext->metadata, ppKeys, NumKeys);
			if(!Value.empty())
				return Value;
			for(unsigned int i = 0; i < pContext->nb_streams; ++i)
			{
				if(pContext->streams[i] == nullptr)
					continue;
				Value = MetadataValue(pContext->streams[i]->metadata, ppKeys, NumKeys);
				if(!Value.empty())
					return Value;
			}
			for(unsigned int i = 0; i < pContext->nb_chapters; ++i)
			{
				if(pContext->chapters[i] == nullptr)
					continue;
				Value = MetadataValue(pContext->chapters[i]->metadata, ppKeys, NumKeys);
				if(!Value.empty())
					return Value;
			}
			return "";
		}
#endif

		bool ExtractLocalMusicMetadata(const char *pPath, SLocalMediaFileEntry *pEntry)
		{
			if(pPath == nullptr || pPath[0] == '\0' || pEntry == nullptr)
				return false;
			pEntry->m_Path = pPath;
			pEntry->m_FileName = FileNameOnly(pPath);
			pEntry->m_FileSize = FileSize(pPath);
			pEntry->m_Modified = FileModified(pPath);
			pEntry->m_Title = FileNameWithoutExtension(pPath);

#if QM_LYRICS_HAS_FFMPEG
			AVFormatContext *pContext = nullptr;
			if(avformat_open_input(&pContext, pPath, nullptr, nullptr) < 0 || pContext == nullptr)
			{
				if(pContext != nullptr)
					avformat_close_input(&pContext);
				return false;
			}

			(void)avformat_find_stream_info(pContext, nullptr);
			const char *apTitleKeys[] = {"title", "TIT2"};
			const char *apArtistKeys[] = {"artist", "album_artist", "albumartist", "TPE1", "TPE2"};
			const char *apAlbumKeys[] = {"album", "TALB"};
			const char *apLyricsKeys[] = {
				"lyrics",
				"unsyncedlyrics",
				"unsynchronised lyrics",
				"unsynchronized lyrics",
				"unsynchronisedlyrics",
				"unsynchronizedlyrics",
				"USLT",
				"SYLT",
			};
			const std::string Title = TrimCopy(FindMetadataValue(pContext, apTitleKeys, std::size(apTitleKeys)));
			const std::string Artist = TrimCopy(FindMetadataValue(pContext, apArtistKeys, std::size(apArtistKeys)));
			const std::string Album = TrimCopy(FindMetadataValue(pContext, apAlbumKeys, std::size(apAlbumKeys)));
			const std::string Lyrics = FindMetadataValue(pContext, apLyricsKeys, std::size(apLyricsKeys));
			if(!Title.empty())
				pEntry->m_Title = Title;
			pEntry->m_Artist = Artist;
			pEntry->m_Album = Album;
			if(HasAnyContent(Lyrics))
				pEntry->m_EmbeddedLyrics = Lyrics;
			if(pContext->duration > 0)
				pEntry->m_DurationSec = (int)((pContext->duration + AV_TIME_BASE / 2) / AV_TIME_BASE);
			avformat_close_input(&pContext);
			return true;
#else
			return false;
#endif
		}

		std::string ResolveMediaRoot(IStorage *pStorage, std::string_view Root)
		{
			const std::string Trimmed = TrimCopy(Root);
			if(Trimmed.empty())
				return "";
			if(!fs_is_relative_path(Trimmed.c_str()) || pStorage == nullptr)
				return Trimmed;
			char aPath[IO_MAX_PATH_LENGTH];
			pStorage->GetCompletePath(IStorage::TYPE_SAVE, Trimmed.c_str(), aPath, sizeof(aPath));
			return aPath;
		}

		void ScanDirectoryRecursive(const std::string &Dir, int Depth, std::vector<std::string> *pvFiles);

		struct SDirectoryListData
		{
			std::string m_Dir;
			std::vector<std::string> *m_pvFiles = nullptr;
			int m_Depth = 0;
		};

		int DirectoryListCallback(const char *pName, int IsDir, int, void *pUser)
		{
			SDirectoryListData *pData = static_cast<SDirectoryListData *>(pUser);
			if(pData == nullptr || pData->m_pvFiles == nullptr || pName == nullptr ||
				str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0)
				return 0;
			const std::string Path = JoinPath(pData->m_Dir, pName);
			if(IsDir)
			{
				if(pData->m_Depth < 64)
					ScanDirectoryRecursive(Path, pData->m_Depth + 1, pData->m_pvFiles);
			}
			else if(IsLocalMusicFileExtension(Path))
			{
				pData->m_pvFiles->push_back(Path);
			}
			return 0;
		}

		void ScanDirectoryRecursive(const std::string &Dir, int Depth, std::vector<std::string> *pvFiles)
		{
			if(pvFiles == nullptr || Dir.empty())
				return;
			SDirectoryListData Data;
			Data.m_Dir = Dir;
			Data.m_pvFiles = pvFiles;
			Data.m_Depth = Depth;
			fs_listdir(Dir.c_str(), DirectoryListCallback, 0, &Data);
		}

		std::vector<std::string> FindLocalMusicFiles(IStorage *pStorage, const std::vector<std::string> &vRoots)
		{
			std::vector<std::string> vFiles;
			for(const std::string &RootRaw : vRoots)
			{
				const std::string Root = ResolveMediaRoot(pStorage, RootRaw);
				if(Root.empty())
					continue;
				if(fs_is_file(Root.c_str()))
				{
					if(IsLocalMusicFileExtension(Root))
						AppendUnique(&vFiles, Root);
				}
				else if(fs_is_dir(Root.c_str()))
				{
					ScanDirectoryRecursive(Root, 0, &vFiles);
				}
			}
			return vFiles;
		}

		SLocalMusicScanResult BuildLocalMusicScanResult(IStorage *pStorage, const SSourceQuery &Query, const std::vector<std::string> &vRoots, const std::vector<SLocalMediaFileEntry> &vOldIndex, const IJob *pAbortJob)
		{
			std::unordered_map<std::string, SLocalMediaFileEntry> mOldEntries;
			mOldEntries.reserve(vOldIndex.size());
			for(const SLocalMediaFileEntry &Entry : vOldIndex)
				mOldEntries[Entry.m_Path] = Entry;

			SLocalMusicScanResult Result;
			std::vector<std::string> vFiles = FindLocalMusicFiles(pStorage, vRoots);
			for(const std::string &File : vFiles)
			{
				if(pAbortJob != nullptr && pAbortJob->State() == IJob::STATE_ABORTED)
					break;
				const int64_t Size = FileSize(File);
				const int64_t Modified = FileModified(File);
				const auto OldIt = mOldEntries.find(File);
				if(OldIt != mOldEntries.end() && OldIt->second.m_FileSize == Size && OldIt->second.m_Modified == Modified)
				{
					Result.m_vIndex.push_back(OldIt->second);
					continue;
				}

				SLocalMediaFileEntry Entry;
				if(ExtractLocalMusicMetadata(File.c_str(), &Entry))
				{
					Entry.m_FileSize = Size;
					Entry.m_Modified = Modified;
					Result.m_vIndex.push_back(std::move(Entry));
				}
			}
			Result.m_vCandidates = BuildLocalMusicCandidatesFromIndex(Query, Result.m_vIndex);
			return Result;
		}

		class CLocalMusicScanJob : public IJob
		{
		public:
			CLocalMusicScanJob(IStorage *pStorage, SSourceQuery Query, std::vector<std::string> vRoots, std::vector<SLocalMediaFileEntry> vOldIndex, int Generation) :
				m_pStorage(pStorage),
				m_Query(std::move(Query)),
				m_vRoots(std::move(vRoots)),
				m_vOldIndex(std::move(vOldIndex)),
				m_Generation(Generation)
			{
				Abortable(true);
			}

			int Generation() const { return m_Generation; }
			const SLocalMusicScanResult &Result() const { return m_Result; }

		protected:
			void Run() override
			{
				m_Result = BuildLocalMusicScanResult(m_pStorage, m_Query, m_vRoots, m_vOldIndex, this);
			}

		private:
			IStorage *m_pStorage = nullptr;
			SSourceQuery m_Query;
			std::vector<std::string> m_vRoots;
			std::vector<SLocalMediaFileEntry> m_vOldIndex;
			SLocalMusicScanResult m_Result;
			int m_Generation = 0;
		};

	} // anonymous namespace

	const char *LocalLyricsFileExtension(ELocalLyricsFileKind Kind)
	{
		switch(Kind)
		{
		case ELocalLyricsFileKind::LRC: return ".lrc";
		case ELocalLyricsFileKind::ESLRC: return ".eslrc";
		case ELocalLyricsFileKind::TTML: return ".ttml";
		}
		return ".lrc";
	}

	const char *LocalLyricsSourceId(ELocalLyricsFileKind Kind)
	{
		switch(Kind)
		{
		case ELocalLyricsFileKind::LRC: return "local-lrc";
		case ELocalLyricsFileKind::ESLRC: return "local-eslrc";
		case ELocalLyricsFileKind::TTML: return "local-ttml";
		}
		return "local-lrc";
	}

	EFormat LocalLyricsFormatHint(ELocalLyricsFileKind Kind)
	{
		switch(Kind)
		{
		case ELocalLyricsFileKind::LRC: return EFormat::LRC_ENHANCED;
		case ELocalLyricsFileKind::ESLRC: return EFormat::ESLRC;
		case ELocalLyricsFileKind::TTML: return EFormat::TTML;
		}
		return EFormat::LRC_ENHANCED;
	}

	std::vector<std::string> BuildLocalLyricsCandidatePaths(const SSourceQuery &Query, ELocalLyricsFileKind Kind)
	{
		std::vector<std::string> vPaths;
		const char *pExtension = LocalLyricsFileExtension(Kind);
		if(!Query.m_LinkedFileName.empty())
		{
			if(HasExtension(Query.m_LinkedFileName, pExtension))
				AppendUnique(&vPaths, Query.m_LinkedFileName);
			AppendUnique(&vPaths, ReplaceOrAppendExtension(Query.m_LinkedFileName, pExtension));

			const std::string NameOnly = FileNameOnly(Query.m_LinkedFileName);
			if(NameOnly != Query.m_LinkedFileName)
			{
				if(HasExtension(NameOnly, pExtension))
					AppendUnique(&vPaths, NameOnly);
				AppendUnique(&vPaths, ReplaceOrAppendExtension(NameOnly, pExtension));
			}
		}

		if(!Query.m_Title.empty())
		{
			AppendUnique(&vPaths, ReplaceOrAppendExtension(Query.m_Title, pExtension));
			if(!Query.m_Artist.empty())
			{
				std::string ArtistTitle = Query.m_Artist;
				ArtistTitle.append(" - ");
				ArtistTitle.append(Query.m_Title);
				AppendUnique(&vPaths, ReplaceOrAppendExtension(ArtistTitle, pExtension));

				std::string TitleArtist = Query.m_Title;
				TitleArtist.append(" - ");
				TitleArtist.append(Query.m_Artist);
				AppendUnique(&vPaths, ReplaceOrAppendExtension(TitleArtist, pExtension));
			}
		}

		vPaths.erase(std::remove_if(vPaths.begin(), vPaths.end(), [](const std::string &Path) {
			return Path.empty() || IsSlash(Path.back());
		}),
			vPaths.end());
		return vPaths;
	}

	bool LoadLocalLyricsCandidate(IStorage *pStorage, const char *pPath, ELocalLyricsFileKind Kind, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pOut == nullptr)
			return false;
		std::string Text;
		if(!ReadLocalTextFile(pStorage, pPath, &Text))
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "local lyrics file not found", ErrSize);
			return false;
		}
		if(Text.empty())
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "local lyrics file is empty", ErrSize);
			return false;
		}

		SSourceCandidate Candidate;
		Candidate.m_RawText = std::move(Text);
		Candidate.m_FormatHint = LocalLyricsFormatHint(Kind);
		Candidate.m_SourceId = LocalLyricsSourceId(Kind);
		Candidate.m_SourceScore = 1.0f;
		Candidate.m_Metadata.m_FileName = FileNameOnly(pPath);
		TryFillMetadata(Candidate.m_RawText, Kind, &Candidate.m_Metadata);
		*pOut = std::move(Candidate);
		return true;
	}

	std::vector<std::string> SplitLocalMediaFolders(std::string_view Folders)
	{
		std::vector<std::string> vFolders;
		size_t Start = 0;
		for(size_t i = 0; i <= Folders.size(); ++i)
		{
			const bool End = i == Folders.size();
			const char C = End ? '\0' : Folders[i];
			if(End || C == '|' || C == '\n' || C == '\r' || C == ';')
			{
				AppendUnique(&vFolders, std::string(Folders.substr(Start, i - Start)));
				Start = i + 1;
			}
		}
		return vFolders;
	}

	bool IsLocalMusicFileExtension(std::string_view Path)
	{
		static const char *s_apExtensions[] = {
			".mp3",
			".aac",
			".m4a",
			".ogg",
			".opus",
			".wma",
			".amr",
			".flac",
			".alac",
			".ape",
			".wv",
			".tak",
			".wav",
			".aiff",
			".aif",
			".pcm",
			".cda",
			".dsf",
			".dff",
			".au",
			".snd",
			".mid",
			".midi",
			".mod",
			".xm",
			".it",
			".s3m",
		};
		for(const char *pExtension : s_apExtensions)
		{
			if(HasExtension(Path, pExtension))
				return true;
		}
		return false;
	}

	std::string LocalMediaIndexToJson(const std::vector<SLocalMediaFileEntry> &vEntries)
	{
		std::string Out;
		Out.reserve(vEntries.size() * 192);
		Out.append("{\"version\":1,\"entries\":[");
		bool First = true;
		for(const SLocalMediaFileEntry &Entry : vEntries)
		{
			if(Entry.m_Path.empty())
				continue;
			if(!First)
				Out.push_back(',');
			First = false;
			Out.push_back('{');
			Out.append("\"path\":");
			EscapeJsonString(Entry.m_Path, &Out);
			Out.append(",\"file\":");
			EscapeJsonString(Entry.m_FileName, &Out);
			Out.append(",\"title\":");
			EscapeJsonString(Entry.m_Title, &Out);
			Out.append(",\"artist\":");
			EscapeJsonString(Entry.m_Artist, &Out);
			Out.append(",\"album\":");
			EscapeJsonString(Entry.m_Album, &Out);
			Out.append(",\"lyrics\":");
			EscapeJsonString(Entry.m_EmbeddedLyrics, &Out);
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), ",\"duration\":%d,\"size\":%lld,\"modified\":%lld",
				Entry.m_DurationSec,
				(long long)Entry.m_FileSize,
				(long long)Entry.m_Modified);
			Out.append(aBuf);
			Out.push_back('}');
		}
		Out.append("]}");
		return Out;
	}

	bool LocalMediaIndexFromJson(std::string_view Json, std::vector<SLocalMediaFileEntry> *pvEntries, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pvEntries == nullptr)
			return false;
		if(Json.empty())
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "empty input", ErrSize);
			return false;
		}

		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, Json.data(), Json.size(), aJsonErr);
		if(pRoot == nullptr)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, aJsonErr, ErrSize);
			return false;
		}
		if(pRoot->type != json_object)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "root not object", ErrSize);
			return false;
		}

		std::vector<SLocalMediaFileEntry> vNewEntries;
		const json_value &Entries = (*pRoot)["entries"];
		if(Entries.type != json_array)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "entries missing/not array", ErrSize);
			return false;
		}

		vNewEntries.reserve(Entries.u.array.length);
		for(unsigned int i = 0; i < Entries.u.array.length; ++i)
		{
			const json_value *pItem = Entries.u.array.values[i];
			if(pItem == nullptr || pItem->type != json_object)
				continue;
			SLocalMediaFileEntry Entry;
			Entry.m_Path = JsonString(&(*pItem)["path"]);
			Entry.m_FileName = JsonString(&(*pItem)["file"]);
			Entry.m_Title = JsonString(&(*pItem)["title"]);
			Entry.m_Artist = JsonString(&(*pItem)["artist"]);
			Entry.m_Album = JsonString(&(*pItem)["album"]);
			Entry.m_EmbeddedLyrics = JsonString(&(*pItem)["lyrics"]);
			Entry.m_DurationSec = (int)JsonInt64(&(*pItem)["duration"]);
			Entry.m_FileSize = JsonInt64(&(*pItem)["size"]);
			Entry.m_Modified = JsonInt64(&(*pItem)["modified"]);
			if(Entry.m_Path.empty())
				continue;
			if(Entry.m_FileName.empty())
				Entry.m_FileName = FileNameOnly(Entry.m_Path);
			vNewEntries.push_back(std::move(Entry));
		}

		json_value_free(pRoot);
		*pvEntries = std::move(vNewEntries);
		return true;
	}

	bool LoadLocalMediaIndex(IStorage *pStorage, std::vector<SLocalMediaFileEntry> *pvEntries)
	{
		if(pStorage == nullptr || pvEntries == nullptr)
			return false;
		char *pJson = pStorage->ReadFileStr(LOCAL_MEDIA_INDEX, IStorage::TYPE_SAVE);
		if(pJson == nullptr)
			return false;
		char aErr[128];
		const bool Ok = LocalMediaIndexFromJson(pJson, pvEntries, aErr, sizeof(aErr));
		free(pJson);
		return Ok;
	}

	bool SaveLocalMediaIndex(IStorage *pStorage, const std::vector<SLocalMediaFileEntry> &vEntries)
	{
		if(!EnsureLocalMediaIndexDir(pStorage))
			return false;
		IOHANDLE File = pStorage->OpenFile(LOCAL_MEDIA_INDEX, IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(!File)
			return false;
		const std::string Json = LocalMediaIndexToJson(vEntries);
		const bool Ok = io_write(File, Json.data(), (unsigned)Json.size()) == Json.size();
		io_close(File);
		return Ok;
	}

	std::vector<SSourceCandidate> BuildLocalMusicCandidatesFromIndex(const SSourceQuery &Query, const std::vector<SLocalMediaFileEntry> &vEntries)
	{
		float BestScore = 0.0f;
		const SLocalMediaFileEntry *pBest = nullptr;
		for(const SLocalMediaFileEntry &Entry : vEntries)
		{
			if(!HasAnyContent(Entry.m_EmbeddedLyrics))
				continue;
			const SMatchCandidate Metadata = MatchCandidateFromLocalMedia(Entry);
			const float ScoreValue = Score(Query, Metadata);
			if(ScoreValue > BestScore)
			{
				BestScore = ScoreValue;
				pBest = &Entry;
			}
		}
		if(pBest == nullptr)
			return {};

		SSourceCandidate Candidate;
		Candidate.m_RawText = pBest->m_EmbeddedLyrics;
		Candidate.m_FormatHint = GuessLyricsFormat(pBest->m_EmbeddedLyrics);
		Candidate.m_Metadata = MatchCandidateFromLocalMedia(*pBest);
		Candidate.m_SourceId = "local-music-file";
		Candidate.m_SourceScore = BestScore / 100.0f;
		return {std::move(Candidate)};
	}

	struct CLyricsSourceLocalMusicFile::SImpl
	{
		IStorage *m_pStorage = nullptr;
		IEngine *m_pEngine = nullptr;
		const char *m_pLocalMediaFolders = nullptr;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		std::shared_ptr<CLocalMusicScanJob> m_pJob;
		std::vector<SLocalMediaFileEntry> m_vIndex;
		std::vector<SSourceCandidate> m_vPendingCandidates;
		int m_Generation = 0;
		bool m_IndexLoaded = false;
		bool m_PendingDone = false;
	};

	CLyricsSourceLocalMusicFile::CLyricsSourceLocalMusicFile(IStorage *pStorage, IEngine *pEngine, const char *pLocalMediaFolders) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pStorage = pStorage;
		m_pImpl->m_pEngine = pEngine;
		m_pImpl->m_pLocalMediaFolders = pLocalMediaFolders;
	}

	CLyricsSourceLocalMusicFile::~CLyricsSourceLocalMusicFile() = default;

	const char *CLyricsSourceLocalMusicFile::Id() const
	{
		return "local-music-file";
	}

	void CLyricsSourceLocalMusicFile::Cancel()
	{
		if(m_pImpl->m_pJob)
			m_pImpl->m_pJob->Abort();
		m_pImpl->m_pJob.reset();
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_vPendingCandidates.clear();
		m_pImpl->m_PendingDone = false;
		++m_pImpl->m_Generation;
	}

	bool CLyricsSourceLocalMusicFile::BusyForTests() const
	{
		return m_pImpl->m_PendingDone || (m_pImpl->m_pJob && !m_pImpl->m_pJob->Done());
	}

	void CLyricsSourceLocalMusicFile::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		m_pImpl->m_PendingDone = true;
		if(!m_pImpl->m_IndexLoaded)
		{
			LoadLocalMediaIndex(m_pImpl->m_pStorage, &m_pImpl->m_vIndex);
			m_pImpl->m_IndexLoaded = true;
		}

		std::vector<std::string> vRoots = SplitLocalMediaFolders(m_pImpl->m_pLocalMediaFolders != nullptr ? m_pImpl->m_pLocalMediaFolders : "");
		if(!Query.m_LinkedFileName.empty() && IsLocalMusicFileExtension(Query.m_LinkedFileName))
			AppendUnique(&vRoots, Query.m_LinkedFileName);

		if(vRoots.empty())
		{
			m_pImpl->m_vPendingCandidates.clear();
			return;
		}

		const int Generation = ++m_pImpl->m_Generation;
		if(m_pImpl->m_pEngine != nullptr)
		{
			m_pImpl->m_pJob = std::make_shared<CLocalMusicScanJob>(m_pImpl->m_pStorage, Query, std::move(vRoots), m_pImpl->m_vIndex, Generation);
			m_pImpl->m_pEngine->AddJob(m_pImpl->m_pJob);
		}
		else
		{
			const SLocalMusicScanResult Result = BuildLocalMusicScanResult(m_pImpl->m_pStorage, Query, vRoots, m_pImpl->m_vIndex, nullptr);
			m_pImpl->m_vIndex = Result.m_vIndex;
			m_pImpl->m_vPendingCandidates = Result.m_vCandidates;
			SaveLocalMediaIndex(m_pImpl->m_pStorage, m_pImpl->m_vIndex);
		}
	}

	void CLyricsSourceLocalMusicFile::Tick()
	{
		if(m_pImpl->m_pJob && m_pImpl->m_pJob->Done())
		{
			std::shared_ptr<CLocalMusicScanJob> pJob = std::move(m_pImpl->m_pJob);
			if(pJob->State() != IJob::STATE_ABORTED && pJob->Generation() == m_pImpl->m_Generation)
			{
				const SLocalMusicScanResult &Result = pJob->Result();
				m_pImpl->m_vIndex = Result.m_vIndex;
				m_pImpl->m_vPendingCandidates = Result.m_vCandidates;
				SaveLocalMediaIndex(m_pImpl->m_pStorage, m_pImpl->m_vIndex);
			}
		}
		if(!m_pImpl->m_PendingDone || m_pImpl->m_pJob)
			return;
		FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
		std::vector<SSourceCandidate> vCandidates = std::move(m_pImpl->m_vPendingCandidates);
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_PendingDone = false;
		if(Done)
			Done(std::move(vCandidates));
	}

	struct CLyricsSourceLocalLyricsFile::SImpl
	{
		IStorage *m_pStorage = nullptr;
		ELocalLyricsFileKind m_Kind = ELocalLyricsFileKind::LRC;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		std::vector<SSourceCandidate> m_vPendingCandidates;
		bool m_PendingDone = false;
	};

	CLyricsSourceLocalLyricsFile::CLyricsSourceLocalLyricsFile(IStorage *pStorage, ELocalLyricsFileKind Kind) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pStorage = pStorage;
		m_pImpl->m_Kind = Kind;
	}

	CLyricsSourceLocalLyricsFile::~CLyricsSourceLocalLyricsFile() = default;

	const char *CLyricsSourceLocalLyricsFile::Id() const
	{
		return LocalLyricsSourceId(m_pImpl->m_Kind);
	}

	void CLyricsSourceLocalLyricsFile::Cancel()
	{
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_vPendingCandidates.clear();
		m_pImpl->m_PendingDone = false;
	}

	bool CLyricsSourceLocalLyricsFile::BusyForTests() const
	{
		return m_pImpl->m_PendingDone;
	}

	void CLyricsSourceLocalLyricsFile::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		m_pImpl->m_PendingDone = true;
		const std::vector<std::string> vPaths = BuildLocalLyricsCandidatePaths(Query, m_pImpl->m_Kind);
		for(const std::string &Path : vPaths)
		{
			SSourceCandidate Candidate;
			char aErr[128];
			if(LoadLocalLyricsCandidate(m_pImpl->m_pStorage, Path.c_str(), m_pImpl->m_Kind, &Candidate, aErr, sizeof(aErr)))
			{
				m_pImpl->m_vPendingCandidates.push_back(std::move(Candidate));
				break;
			}
		}
	}

	void CLyricsSourceLocalLyricsFile::Tick()
	{
		if(!m_pImpl->m_PendingDone)
			return;
		FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
		std::vector<SSourceCandidate> vCandidates = std::move(m_pImpl->m_vPendingCandidates);
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_PendingDone = false;
		if(Done)
			Done(std::move(vCandidates));
	}

} // namespace QmLyrics
