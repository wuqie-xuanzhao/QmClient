#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_LOCAL_SAVE_DISPLAY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_LOCAL_SAVE_DISPLAY_H

#include <base/system.h>

#include <engine/shared/jobs.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace qm_local_saves
{
	struct SLocalSaveDisplayEntry
	{
		std::string m_Time;
		std::string m_Players;
		std::string m_Map;
		std::string m_Code;
		std::string m_RawLine;
	};

	inline void TrimDisplayField(std::string &Field)
	{
		while(!Field.empty() && str_isspace(Field.front()))
			Field.erase(Field.begin());
		while(!Field.empty() && str_isspace(Field.back()))
			Field.pop_back();
	}

	inline std::array<std::string, 4> ParseSaveCsvFields(const char *pLine)
	{
		std::array<std::string, 4> aFields;
		int FieldIndex = 0;
		bool InQuotes = false;

		for(int CharIndex = 0; pLine[CharIndex] != '\0' && FieldIndex < (int)aFields.size(); ++CharIndex)
		{
			if(pLine[CharIndex] == '"')
			{
				if(InQuotes && pLine[CharIndex + 1] == '"')
				{
					aFields[FieldIndex].push_back('"');
					++CharIndex;
				}
				else
				{
					InQuotes = !InQuotes;
				}
			}
			else if(pLine[CharIndex] == ',' && !InQuotes)
			{
				++FieldIndex;
			}
			else
			{
				aFields[FieldIndex].push_back(pLine[CharIndex]);
			}
		}

		for(std::string &Field : aFields)
			TrimDisplayField(Field);
		return aFields;
	}

	inline std::vector<SLocalSaveDisplayEntry> ParseEntries(const char *pFileContent)
	{
		std::vector<SLocalSaveDisplayEntry> vEntries;
		const char *pCursor = pFileContent;
		char aLine[2048];
		bool FirstLine = true;
		while((pCursor = str_next_token(pCursor, "\n", aLine, sizeof(aLine))))
		{
			str_utf8_trim_right(aLine);
			if(aLine[0] == '\0')
				continue;
			if(FirstLine)
			{
				FirstLine = false;
				if(str_startswith(aLine, "Time"))
					continue;
			}

			std::array<std::string, 4> aFields = ParseSaveCsvFields(aLine);
			SLocalSaveDisplayEntry Entry;
			Entry.m_Time = aFields[0];
			Entry.m_Players = aFields[1];
			Entry.m_Map = aFields[2];
			Entry.m_Code = aFields[3];
			Entry.m_RawLine = aLine;
			vEntries.push_back(std::move(Entry));
		}

		return vEntries;
	}
}

// 任务只拥有路径和解析结果；菜单释放后仍可安全完成，不访问客户端或 Storage。
class CQmLocalSaveDisplayCache
{
	class CLoadJob : public IJob
	{
		std::string m_Path;
		void Run() override
		{
			IOHANDLE File = io_open(m_Path.c_str(), IOFLAG_READ);
			if(!File)
				return;
			m_FileExists = true;
			char *pContent = io_read_all_str(File);
			io_close(File);
			if(!pContent)
				return;
			m_vEntries = qm_local_saves::ParseEntries(pContent);
			free(pContent);
		}

	public:
		bool m_FileExists = false;
		std::vector<qm_local_saves::SLocalSaveDisplayEntry> m_vEntries;
		explicit CLoadJob(const char *pPath) :
			m_Path(pPath) {}
	};

	std::shared_ptr<CLoadJob> m_pJob;
	std::vector<qm_local_saves::SLocalSaveDisplayEntry> m_vEntries;
	int64_t m_LastRequest = 0;
	bool m_Requested = false;
	bool m_Ready = false;
	bool m_FileExists = false;

public:
	// 主线程只发布已完成的结果；有任务在途时保留旧快照且不重复排队。
	std::shared_ptr<IJob> Refresh(const char *pPath, int64_t Now, int64_t Interval)
	{
		if(m_pJob)
		{
			if(m_pJob->State() != IJob::STATE_DONE)
				return nullptr;
			m_vEntries.swap(m_pJob->m_vEntries);
			m_FileExists = m_pJob->m_FileExists;
			m_Ready = true;
			m_pJob.reset();
		}
		if(m_Requested && Now - m_LastRequest <= Interval)
			return nullptr;
		m_pJob = std::make_shared<CLoadJob>(pPath);
		m_LastRequest = Now;
		m_Requested = true;
		return m_pJob;
	}

	bool Ready() const { return m_Ready; }
	bool FileExists() const { return m_FileExists; }
	const std::vector<qm_local_saves::SLocalSaveDisplayEntry> &Entries() const { return m_vEntries; }
	void Reset()
	{
		m_pJob.reset();
		m_vEntries.clear();
		m_LastRequest = 0;
		m_Requested = false;
		m_Ready = false;
		m_FileExists = false;
	}
};

#endif
