#include "qm_lyrics_source_amll_ttml_db.h"

#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/http.h>
#include <engine/shared/http.h>
#include <engine/storage.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

namespace QmLyrics
{

	namespace
	{

		constexpr const char *AMLL_TTML_DB_DEFAULT_BASE_URL = "https://raw.githubusercontent.com/amll-dev/amll-ttml-db/refs/heads/main";
		constexpr const char *AMLL_TTML_DB_INDEX_SUFFIX = "metadata/raw-lyrics-index.jsonl";
		constexpr const char *AMLL_TTML_DB_QUERY_PREFIX = "raw-lyrics";
		constexpr const char *AMLL_TTML_DB_DIR = "qmclient/lyrics";
		constexpr const char *AMLL_TTML_DB_INDEX_PATH = "qmclient/lyrics/amll-ttml-db-index.jsonl";
		constexpr const char *AMLL_TTML_DB_LAST_UPDATED_PATH = "qmclient/lyrics/amll-ttml-db-last-updated.txt";
		constexpr int64_t AMLL_TTML_DB_INDEX_TTL_SEC = 24 * 60 * 60;

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage != nullptr ? pMessage : "", ErrSize);
		}

		std::string NormalizeBaseUrl(const char *pBaseUrl)
		{
			std::string Url = (pBaseUrl != nullptr && pBaseUrl[0] != '\0') ? pBaseUrl : AMLL_TTML_DB_DEFAULT_BASE_URL;
			while(!Url.empty() && Url.back() == '/')
				Url.pop_back();
			return Url;
		}

		void UrlEncodePathSegment(std::string_view In, std::string *pOut)
		{
			for(unsigned char C : In)
			{
				const bool Unreserved = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
							(C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~' || C == '/';
				if(Unreserved)
					pOut->push_back((char)C);
				else
				{
					char aBuf[4];
					str_format(aBuf, sizeof(aBuf), "%%%02X", (int)C);
					pOut->append(aBuf);
				}
			}
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int64_t JsonInt(const json_value *pVal, int64_t Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return pVal->u.integer;
			if(pVal->type == json_double)
				return (int64_t)pVal->u.dbl;
			if(pVal->type == json_string)
			{
				char *pEnd = nullptr;
				const int64_t Value = std::strtoll(pVal->u.string.ptr, &pEnd, 10);
				return pEnd != pVal->u.string.ptr ? Value : Default;
			}
			return Default;
		}

		bool ParseJsonRoot(const char *pBody, size_t BodyLen, json_value **ppRoot, char *pErr, size_t ErrSize)
		{
			if(ppRoot == nullptr)
				return false;
			*ppRoot = nullptr;
			if(pBody == nullptr || BodyLen == 0)
			{
				SetError(pErr, ErrSize, "empty input");
				return false;
			}
			json_settings Settings{};
			char aJsonErr[json_error_max];
			*ppRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
			if(*ppRoot == nullptr)
			{
				SetError(pErr, ErrSize, aJsonErr);
				return false;
			}
			if((*ppRoot)->type != json_object)
			{
				json_value_free(*ppRoot);
				*ppRoot = nullptr;
				SetError(pErr, ErrSize, "root is not object");
				return false;
			}
			return true;
		}

		std::string ReadFirstString(const json_value &Values)
		{
			if(Values.type != json_array || Values.u.array.length == 0 || Values.u.array.values[0] == nullptr)
				return {};
			return JsonString(Values.u.array.values[0]);
		}

		std::string JoinStringValues(const json_value &Values)
		{
			if(Values.type != json_array)
				return {};
			std::string Out;
			for(unsigned int i = 0; i < Values.u.array.length; ++i)
			{
				const json_value *pItem = Values.u.array.values[i];
				const char *pValue = JsonString(pItem);
				if(pValue[0] == '\0')
					continue;
				if(!Out.empty())
					Out.push_back('/');
				Out.append(pValue);
			}
			return Out;
		}

		bool EnsureIndexDir(IStorage *pStorage)
		{
			if(pStorage == nullptr)
				return false;
			pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
			return pStorage->CreateFolder(AMLL_TTML_DB_DIR, IStorage::TYPE_SAVE) || pStorage->FolderExists(AMLL_TTML_DB_DIR, IStorage::TYPE_SAVE);
		}

		bool WriteFileText(IStorage *pStorage, const char *pPath, std::string_view Text)
		{
			if(pStorage == nullptr || pPath == nullptr || !EnsureIndexDir(pStorage))
				return false;
			IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
			if(!File)
				return false;
			const bool Ok = io_write(File, Text.data(), (unsigned)Text.size()) == Text.size();
			io_close(File);
			return Ok;
		}

		std::string ReadFileText(IStorage *pStorage, const char *pPath)
		{
			if(pStorage == nullptr || pPath == nullptr)
				return {};
			char *pText = pStorage->ReadFileStr(pPath, IStorage::TYPE_SAVE);
			if(pText == nullptr)
				return {};
			std::string Text = pText;
			free(pText);
			return Text;
		}

		bool IsIndexFresh(IStorage *pStorage)
		{
			if(pStorage == nullptr || !pStorage->FileExists(AMLL_TTML_DB_INDEX_PATH, IStorage::TYPE_SAVE) ||
				!pStorage->FileExists(AMLL_TTML_DB_LAST_UPDATED_PATH, IStorage::TYPE_SAVE))
				return false;
			const std::string LastUpdatedText = ReadFileText(pStorage, AMLL_TTML_DB_LAST_UPDATED_PATH);
			char *pEnd = nullptr;
			const int64_t LastUpdated = std::strtoll(LastUpdatedText.c_str(), &pEnd, 10);
			if(pEnd == LastUpdatedText.c_str() || LastUpdated <= 0)
				return false;
			return time_timestamp() - LastUpdated <= AMLL_TTML_DB_INDEX_TTL_SEC;
		}

		bool SaveIndex(IStorage *pStorage, std::string_view Text)
		{
			if(!WriteFileText(pStorage, AMLL_TTML_DB_INDEX_PATH, Text))
				return false;
			char aNow[32];
			str_format(aNow, sizeof(aNow), "%lld", (long long)time_timestamp());
			return WriteFileText(pStorage, AMLL_TTML_DB_LAST_UPDATED_PATH, aNow);
		}

		bool ReadResponseBody(CHttpRequest *pReq, std::vector<unsigned char> *pOut)
		{
			if(pReq == nullptr || pReq->State() != EHttpState::DONE || pReq->StatusCode() != 200)
				return false;
			unsigned char *pBody = nullptr;
			size_t BodyLen = 0;
			pReq->Result(&pBody, &BodyLen);
			if(pBody == nullptr || BodyLen == 0)
				return false;
			pOut->assign(pBody, pBody + BodyLen);
			return true;
		}

		void PrepareRequest(CHttpRequest *pReq, int TimeoutMs)
		{
			pReq->Timeout(CTimeout{5000, TimeoutMs > 0 ? TimeoutMs : 8000, 0, 0});
			pReq->LogProgress(HTTPLOG::FAILURE);
			pReq->AllowInsecureProtocol(false);
			pReq->HeaderString("User-Agent", "QmClient (https://github.com/Q1menG)");
		}

	} // anonymous namespace

	std::string BuildAmllTtmlDbIndexUrl(const char *pBaseUrl)
	{
		std::string Url = NormalizeBaseUrl(pBaseUrl);
		Url.push_back('/');
		Url.append(AMLL_TTML_DB_INDEX_SUFFIX);
		return Url;
	}

	std::string BuildAmllTtmlDbLyricUrl(const std::string &RawLyricFile, const char *pBaseUrl)
	{
		std::string Url = NormalizeBaseUrl(pBaseUrl);
		Url.push_back('/');
		Url.append(AMLL_TTML_DB_QUERY_PREFIX);
		Url.push_back('/');
		UrlEncodePathSegment(RawLyricFile, &Url);
		return Url;
	}

	bool ParseAmllTtmlDbIndexLine(const char *pLine, size_t LineLen, const SSourceQuery &Query, SAmllTtmlDbIndexHit *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		json_value *pRoot = nullptr;
		if(!ParseJsonRoot(pLine, LineLen, &pRoot, pErr, ErrSize))
			return false;

		SAmllTtmlDbIndexHit Hit;
		Hit.m_RawLyricFile = JsonString(&(*pRoot)["rawLyricFile"]);
		const json_value &Metadata = (*pRoot)["metadata"];
		if(Metadata.type == json_array)
		{
			for(unsigned int i = 0; i < Metadata.u.array.length; ++i)
			{
				const json_value *pMeta = Metadata.u.array.values[i];
				if(pMeta == nullptr || pMeta->type != json_array || pMeta->u.array.length != 2)
					continue;
				const char *pKey = JsonString(pMeta->u.array.values[0]);
				const json_value *pValues = pMeta->u.array.values[1];
				if(pValues == nullptr)
					continue;
				if(str_comp(pKey, "musicName") == 0)
					Hit.m_Metadata.m_Title = ReadFirstString(*pValues);
				else if(str_comp(pKey, "artists") == 0)
					Hit.m_Metadata.m_Artist = JoinStringValues(*pValues);
				else if(str_comp(pKey, "album") == 0)
					Hit.m_Metadata.m_Album = ReadFirstString(*pValues);
				else if(str_comp(pKey, "duration") == 0 || str_comp(pKey, "durationSec") == 0)
					Hit.m_Metadata.m_DurationSec = (int)JsonInt(pValues->type == json_array && pValues->u.array.length > 0 ? pValues->u.array.values[0] : pValues);
			}
		}
		json_value_free(pRoot);

		if(Hit.m_RawLyricFile.empty())
		{
			SetError(pErr, ErrSize, "missing rawLyricFile");
			return false;
		}
		Hit.m_Score = Score(Query, Hit.m_Metadata);
		*pOut = std::move(Hit);
		return true;
	}

	bool FindAmllTtmlDbBestMatch(std::string_view IndexText, const SSourceQuery &Query, SAmllTtmlDbIndexHit *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		float BestScore = -1.0f;
		SAmllTtmlDbIndexHit Best;
		size_t Pos = 0;
		while(Pos < IndexText.size())
		{
			size_t End = IndexText.find('\n', Pos);
			if(End == std::string_view::npos)
				End = IndexText.size();
			std::string_view Line = IndexText.substr(Pos, End - Pos);
			while(!Line.empty() && (Line.back() == '\r' || Line.back() == ' ' || Line.back() == '\t'))
				Line.remove_suffix(1);
			while(!Line.empty() && (Line.front() == ' ' || Line.front() == '\t'))
				Line.remove_prefix(1);
			if(!Line.empty())
			{
				SAmllTtmlDbIndexHit Hit;
				if(ParseAmllTtmlDbIndexLine(Line.data(), Line.size(), Query, &Hit) && Hit.m_Score > BestScore)
				{
					BestScore = Hit.m_Score;
					Best = std::move(Hit);
				}
			}
			Pos = End + 1;
		}
		if(Best.m_RawLyricFile.empty())
		{
			SetError(pErr, ErrSize, "no amll match");
			return false;
		}
		*pOut = std::move(Best);
		return true;
	}

	bool ParseAmllTtmlDbLyricResponse(const char *pBody, size_t BodyLen, const SAmllTtmlDbIndexHit &Hit, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
		{
			SetError(pErr, ErrSize, "null output");
			return false;
		}
		if(pBody == nullptr || BodyLen == 0)
		{
			SetError(pErr, ErrSize, "empty lyric");
			return false;
		}
		std::string_view Text(pBody, BodyLen);
		if(Text.find("<tt") == std::string_view::npos || Text.find("begin=") == std::string_view::npos || Text.find("end=") == std::string_view::npos)
		{
			SetError(pErr, ErrSize, "not timed ttml");
			return false;
		}

		SSourceCandidate Candidate;
		Candidate.m_RawText.assign(Text);
		Candidate.m_FormatHint = EFormat::TTML;
		Candidate.m_Metadata = Hit.m_Metadata;
		Candidate.m_SourceId = "amll-ttml-db";
		Candidate.m_SourceScore = std::clamp(Hit.m_Score / 100.0f, 0.0f, 1.0f);
		*pOut = std::move(Candidate);
		return true;
	}

	struct CLyricsSourceAmllTtmlDb::SImpl
	{
		enum class EStage
		{
			IDLE,
			INDEX,
			LYRIC,
		};

		IHttp *m_pHttp = nullptr;
		IStorage *m_pStorage = nullptr;
		int m_TimeoutMs = 8000;
		std::string m_BaseUrl = AMLL_TTML_DB_DEFAULT_BASE_URL;
		EStage m_Stage = EStage::IDLE;
		std::shared_ptr<CHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_Query;
		SAmllTtmlDbIndexHit m_Hit;
	};

	CLyricsSourceAmllTtmlDb::CLyricsSourceAmllTtmlDb(IHttp *pHttp, IStorage *pStorage, int TimeoutMs, const char *pBaseUrl) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_pStorage = pStorage;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
		m_pImpl->m_BaseUrl = NormalizeBaseUrl(pBaseUrl);
	}

	CLyricsSourceAmllTtmlDb::~CLyricsSourceAmllTtmlDb()
	{
		Cancel();
	}

	void CLyricsSourceAmllTtmlDb::Cancel()
	{
		if(m_pImpl->m_pRequest)
		{
			m_pImpl->m_pRequest->Abort();
			m_pImpl->m_pRequest.reset();
		}
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		m_pImpl->m_Hit = {};
	}

	void CLyricsSourceAmllTtmlDb::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		if(Query.m_Title.empty())
		{
			if(Done)
				Done({});
			return;
		}
		if(m_pImpl->m_pHttp == nullptr)
		{
			if(Error)
				Error("no IHttp");
			return;
		}
		m_pImpl->m_Query = Query;
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);

		if(IsIndexFresh(m_pImpl->m_pStorage))
		{
			const std::string IndexText = ReadFileText(m_pImpl->m_pStorage, AMLL_TTML_DB_INDEX_PATH);
			if(!IndexText.empty() && FindAmllTtmlDbBestMatch(IndexText, Query, &m_pImpl->m_Hit))
			{
				m_pImpl->m_Stage = SImpl::EStage::LYRIC;
				m_pImpl->m_pRequest = std::make_shared<CHttpRequest>(BuildAmllTtmlDbLyricUrl(m_pImpl->m_Hit.m_RawLyricFile, m_pImpl->m_BaseUrl.c_str()).c_str());
				PrepareRequest(m_pImpl->m_pRequest.get(), m_pImpl->m_TimeoutMs);
				m_pImpl->m_pHttp->Run(m_pImpl->m_pRequest);
				return;
			}
			if(!IndexText.empty())
			{
				FSourceDoneCallback DoneCallback = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(DoneCallback)
					DoneCallback({});
				return;
			}
		}

		m_pImpl->m_Stage = SImpl::EStage::INDEX;
		m_pImpl->m_pRequest = std::make_shared<CHttpRequest>(BuildAmllTtmlDbIndexUrl(m_pImpl->m_BaseUrl.c_str()).c_str());
		PrepareRequest(m_pImpl->m_pRequest.get(), m_pImpl->m_TimeoutMs);
		m_pImpl->m_pHttp->Run(m_pImpl->m_pRequest);
	}

	void CLyricsSourceAmllTtmlDb::Tick()
	{
		if(m_pImpl->m_Stage == SImpl::EStage::IDLE || !m_pImpl->m_pRequest || !m_pImpl->m_pRequest->Done())
			return;

		std::vector<unsigned char> vBody;
		const bool Ok = ReadResponseBody(m_pImpl->m_pRequest.get(), &vBody);
		m_pImpl->m_pRequest.reset();
		if(!Ok)
		{
			FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
			m_pImpl->m_Error = nullptr;
			m_pImpl->m_Stage = SImpl::EStage::IDLE;
			if(Done)
				Done({});
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::INDEX)
		{
			const std::string IndexText((const char *)vBody.data(), vBody.size());
			SaveIndex(m_pImpl->m_pStorage, IndexText);
			if(FindAmllTtmlDbBestMatch(IndexText, m_pImpl->m_Query, &m_pImpl->m_Hit))
			{
				m_pImpl->m_Stage = SImpl::EStage::LYRIC;
				m_pImpl->m_pRequest = std::make_shared<CHttpRequest>(BuildAmllTtmlDbLyricUrl(m_pImpl->m_Hit.m_RawLyricFile, m_pImpl->m_BaseUrl.c_str()).c_str());
				PrepareRequest(m_pImpl->m_pRequest.get(), m_pImpl->m_TimeoutMs);
				m_pImpl->m_pHttp->Run(m_pImpl->m_pRequest);
				return;
			}
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::LYRIC)
		{
			SSourceCandidate Candidate;
			if(ParseAmllTtmlDbLyricResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Hit, &Candidate))
			{
				FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(Done)
					Done({std::move(Candidate)});
				return;
			}
		}

		FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
		if(Done)
			Done({});
	}

} // namespace QmLyrics
