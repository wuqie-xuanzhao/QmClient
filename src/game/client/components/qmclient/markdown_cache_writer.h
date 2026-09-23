#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MARKDOWN_CACHE_WRITER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MARKDOWN_CACHE_WRITER_H

#include <base/system.h>

#include <engine/shared/jobs.h>
#include <engine/shared/jsonwriter.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

// 每个缓存文件使用独立实例；界面立即更新，磁盘只需保留最新完整快照。
class CQmMarkdownCacheWriter
{
	struct SRequest
	{
		std::string m_Path;
		int m_CacheVersion;
		int m_Version;
		std::string m_Markdown;
	};
	struct SState
	{
		std::mutex m_Mutex;
		std::optional<SRequest> m_Pending;
		bool m_Running = false;
	};
	class CWriteJob : public IJob
	{
		std::shared_ptr<SState> m_pState;
		void Run() override
		{
			while(true)
			{
				SRequest Request;
				{
					const std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
					if(!m_pState->m_Pending)
					{
						m_pState->m_Running = false;
						return;
					}
					Request = std::move(*m_pState->m_Pending);
					m_pState->m_Pending.reset();
				}
				// 序列化和文件操作不持有入队锁，也不访问组件、配置或 Storage。
				CJsonStringWriter Writer;
				Writer.BeginObject();
				Writer.WriteAttribute("cache_version");
				Writer.WriteIntValue(Request.m_CacheVersion);
				Writer.WriteAttribute("version");
				Writer.WriteIntValue(Request.m_Version);
				Writer.WriteAttribute("markdown");
				Writer.WriteStrValue(Request.m_Markdown.c_str());
				Writer.EndObject();
				const std::string Output = Writer.GetOutputString();
				fs_makedir_rec_for(Request.m_Path.c_str());
				IOHANDLE File = io_open(Request.m_Path.c_str(), IOFLAG_WRITE);
				if(File)
				{
					io_write(File, Output.c_str(), Output.size());
					io_close(File);
				}
			}
		}

	public:
		explicit CWriteJob(std::shared_ptr<SState> pState) :
			m_pState(std::move(pState)) {}
	};
	std::shared_ptr<SState> m_pState = std::make_shared<SState>();

public:
	std::shared_ptr<IJob> Enqueue(std::string Path, int CacheVersion, int Version, std::string Markdown)
	{
		const std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
		m_pState->m_Pending = SRequest{std::move(Path), CacheVersion, Version, std::move(Markdown)};
		if(m_pState->m_Running)
			return nullptr;
		m_pState->m_Running = true;
		return std::make_shared<CWriteJob>(m_pState);
	}
};

#endif
