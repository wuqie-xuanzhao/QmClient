#include "qm_lyrics_source_kugou.h"

#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/external/zlib/zlib.h>
#include <engine/http.h>
#include <engine/shared/http.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace QmLyrics
{

	namespace
	{

		constexpr unsigned char KRC_XOR_KEY[16] = {
			0x40, 0x47, 0x61, 0x77, 0x5E, 0x32, 0x74, 0x47,
			0x51, 0x36, 0x31, 0x2D, 0xCE, 0xD2, 0x6E, 0x69};

		constexpr int64_t LAST_LINE_TAIL_MS = 5000;

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage, ErrSize);
		}

		void UrlEncode(std::string_view In, std::string *pOut)
		{
			for(unsigned char C : In)
			{
				const bool Unreserved = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
							(C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~';
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
			return Default;
		}

		double JsonDouble(const json_value *pVal, double Default = 0.0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_double)
				return pVal->u.dbl;
			if(pVal->type == json_integer)
				return (double)pVal->u.integer;
			return Default;
		}

		// Base64 decode（标准字符表 A-Za-z0-9+/= ）
		bool Base64Decode(std::string_view In, std::vector<unsigned char> *pOut)
		{
			static unsigned char s_aDecode[256];
			static bool s_Init = false;
			if(!s_Init)
			{
				std::memset(s_aDecode, 0xFF, sizeof(s_aDecode));
				const char *pAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				for(unsigned char i = 0; i < 64; ++i)
					s_aDecode[(unsigned char)pAlphabet[i]] = i;
				s_Init = true;
			}
			pOut->clear();
			pOut->reserve(In.size() * 3 / 4);
			unsigned int Acc = 0;
			int Bits = 0;
			for(char C : In)
			{
				if(C == '=' || C == '\n' || C == '\r' || C == ' ')
					continue;
				const unsigned char V = s_aDecode[(unsigned char)C];
				if(V == 0xFF)
					return false;
				Acc = (Acc << 6) | V;
				Bits += 6;
				if(Bits >= 8)
				{
					Bits -= 8;
					pOut->push_back((unsigned char)((Acc >> Bits) & 0xFF));
				}
			}
			return true;
		}

	} // anonymous namespace

	std::string BuildKugouSearchUrl(const SMatchQuery &Query)
	{
		// mobilecdn.kugou.com 是 http://（其 https 证书 CN/SAN 不匹配会被 schannel 拒绝）。
		// 调用方必须用 AllowInsecureProtocol(true) 放行。
		std::string Url = "http://mobilecdn.kugou.com/api/v3/search/song?format=json&pagesize=5&page=1&showtype=1&keyword=";
		std::string Keyword = Query.m_Title;
		if(!Query.m_Artist.empty())
		{
			if(!Keyword.empty())
				Keyword.push_back(' ');
			Keyword.append(Query.m_Artist);
		}
		UrlEncode(Keyword, &Url);
		return Url;
	}

	std::string BuildKugouLyricSearchUrl(const std::string &FileHash, int DurationMs)
	{
		std::string Url = "https://lyrics.kugou.com/search?ver=1&man=yes&client=pc&keyword=&hash=";
		UrlEncode(FileHash, &Url);
		if(DurationMs > 0)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "&duration=%d", DurationMs);
			Url.append(aBuf);
		}
		return Url;
	}

	std::string BuildKugouLyricDownloadUrl(const std::string &Id, const std::string &AccessKey)
	{
		std::string Url = "https://lyrics.kugou.com/download?ver=1&client=pc&fmt=krc&charset=utf8&id=";
		UrlEncode(Id, &Url);
		Url.append("&accesskey=");
		UrlEncode(AccessKey, &Url);
		return Url;
	}

	bool ParseKugouSearchResponse(const char *pBody, size_t BodyLen,
		const SMatchQuery &Query,
		SKugouSearchHit *pOut,
		char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
		if(pRoot == nullptr)
		{
			SetError(pErr, ErrSize, aJsonErr);
			return false;
		}
		const json_value &Data = (*pRoot)["data"];
		// songsearch.kugou.com/song_search_v2 用 data.lists；
		// mobilecdn.kugou.com/api/v3/search/song 用 data.info；都兼容。
		const json_value &Lists = Data["lists"];
		const json_value &Info = Data["info"];
		const json_value &Items = Lists.type == json_array ? Lists : Info;
		if(Items.type != json_array || Items.u.array.length == 0)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "no candidates[]");
			return false;
		}
		float BestScore = -1.0f;
		const json_value *pBest = nullptr;
		for(unsigned int i = 0; i < Items.u.array.length; ++i)
		{
			const json_value *pItem = Items.u.array.values[i];
			if(pItem == nullptr || pItem->type != json_object)
				continue;
			SMatchCandidate C;
			// 两个端点字段名不同，都试
			const char *pTitle = JsonString(&(*pItem)["SongName"]);
			if(pTitle[0] == '\0')
				pTitle = JsonString(&(*pItem)["songname"]);
			const char *pArtist = JsonString(&(*pItem)["SingerName"]);
			if(pArtist[0] == '\0')
				pArtist = JsonString(&(*pItem)["singername"]);
			C.m_Title = pTitle;
			C.m_Artist = pArtist;
			int64_t Dur = JsonInt(&(*pItem)["Duration"]);
			if(Dur == 0)
				Dur = JsonInt(&(*pItem)["duration"]);
			C.m_DurationSec = (int)Dur;
			const float S = Score(Query, C);
			if(S > BestScore)
			{
				BestScore = S;
				pBest = pItem;
			}
		}
		if(pBest == nullptr)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "no candidates");
			return false;
		}
		pOut->m_FileHash = JsonString(&(*pBest)["FileHash"]);
		if(pOut->m_FileHash.empty())
			pOut->m_FileHash = JsonString(&(*pBest)["hash"]);
		// 注意：songsearch 的 Duration 单位是秒
		int64_t DurSec = JsonInt(&(*pBest)["Duration"]);
		if(DurSec == 0)
			DurSec = JsonInt(&(*pBest)["duration"]);
		pOut->m_DurationMs = (int)(DurSec * 1000);
		pOut->m_Title = JsonString(&(*pBest)["SongName"]);
		if(pOut->m_Title.empty())
			pOut->m_Title = JsonString(&(*pBest)["songname"]);
		pOut->m_Artist = JsonString(&(*pBest)["SingerName"]);
		if(pOut->m_Artist.empty())
			pOut->m_Artist = JsonString(&(*pBest)["singername"]);
		json_value_free(pRoot);
		if(pOut->m_FileHash.empty())
		{
			SetError(pErr, ErrSize, "no FileHash");
			return false;
		}
		return true;
	}

	bool ParseKugouLyricSearchResponse(const char *pBody, size_t BodyLen,
		SKugouLyricCandidate *pOut,
		char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
		if(pRoot == nullptr)
		{
			SetError(pErr, ErrSize, aJsonErr);
			return false;
		}
		const json_value &Cands = (*pRoot)["candidates"];
		if(Cands.type != json_array || Cands.u.array.length == 0)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "no candidates");
			return false;
		}
		double BestScore = -1.0;
		const json_value *pBest = nullptr;
		for(unsigned int i = 0; i < Cands.u.array.length; ++i)
		{
			const json_value *pItem = Cands.u.array.values[i];
			const double S = JsonDouble(&(*pItem)["score"]);
			if(S > BestScore)
			{
				BestScore = S;
				pBest = pItem;
			}
		}
		if(pBest == nullptr)
		{
			json_value_free(pRoot);
			return false;
		}
		pOut->m_Id = JsonString(&(*pBest)["id"]);
		pOut->m_AccessKey = JsonString(&(*pBest)["accesskey"]);
		json_value_free(pRoot);
		return !pOut->m_Id.empty() && !pOut->m_AccessKey.empty();
	}

	bool ParseKugouLyricDownloadResponse(const char *pBody, size_t BodyLen,
		std::string *pOutKrcText,
		char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
		if(pRoot == nullptr)
		{
			SetError(pErr, ErrSize, aJsonErr);
			return false;
		}
		const char *pContent = JsonString(&(*pRoot)["content"]);
		std::string Base64(pContent != nullptr ? pContent : "");
		json_value_free(pRoot);
		if(Base64.empty())
		{
			SetError(pErr, ErrSize, "no content");
			return false;
		}
		std::vector<unsigned char> vDecoded;
		if(!Base64Decode(Base64, &vDecoded) || vDecoded.size() < 4)
		{
			SetError(pErr, ErrSize, "base64 decode failed");
			return false;
		}
		// 头 4 字节 "krc1" 丢弃
		if(vDecoded[0] != 'k' || vDecoded[1] != 'r' || vDecoded[2] != 'c' || vDecoded[3] != '1')
		{
			SetError(pErr, ErrSize, "bad krc magic");
			return false;
		}
		// XOR
		std::vector<unsigned char> vXored(vDecoded.size() - 4);
		for(size_t i = 0; i < vXored.size(); ++i)
			vXored[i] = vDecoded[i + 4] ^ KRC_XOR_KEY[i % 16];
		// zlib inflate
		std::vector<unsigned char> vInflated;
		vInflated.resize(vXored.size() * 8 + 1024);
		z_stream Stream{};
		Stream.next_in = vXored.data();
		Stream.avail_in = (uInt)vXored.size();
		Stream.next_out = vInflated.data();
		Stream.avail_out = (uInt)vInflated.size();
		if(inflateInit(&Stream) != Z_OK)
		{
			SetError(pErr, ErrSize, "inflateInit failed");
			return false;
		}
		int Ret = inflate(&Stream, Z_FINISH);
		while(Ret == Z_BUF_ERROR || Ret == Z_OK)
		{
			const size_t Used = vInflated.size() - Stream.avail_out;
			vInflated.resize(vInflated.size() * 2);
			Stream.next_out = vInflated.data() + Used;
			Stream.avail_out = (uInt)(vInflated.size() - Used);
			Ret = inflate(&Stream, Z_FINISH);
		}
		const size_t Total = vInflated.size() - Stream.avail_out;
		inflateEnd(&Stream);
		if(Ret != Z_STREAM_END)
		{
			SetError(pErr, ErrSize, "inflate failed");
			return false;
		}
		pOutKrcText->assign((const char *)vInflated.data(), Total);
		// 跳过可能的 UTF-8 BOM
		if(pOutKrcText->size() >= 3 && (unsigned char)(*pOutKrcText)[0] == 0xEF &&
			(unsigned char)(*pOutKrcText)[1] == 0xBB && (unsigned char)(*pOutKrcText)[2] == 0xBF)
			pOutKrcText->erase(0, 3);
		return true;
	}

	bool ParseKrcText(const char *pText, size_t TextSize, SLyricsTrack *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr || pText == nullptr || TextSize == 0)
		{
			SetError(pErr, ErrSize, "bad input");
			return false;
		}
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		pOut->m_vLines.clear();
		pOut->m_Format = EFormat::LRC_ENHANCED;

		const char *p = pText;
		const char *pEnd = pText + TextSize;
		while(p < pEnd)
		{
			const char *pLineStart = p;
			while(p < pEnd && *p != '\n')
				++p;
			std::string_view Line(pLineStart, (size_t)(p - pLineStart));
			if(p < pEnd)
				++p;
			while(!Line.empty() && (Line.back() == '\r' || Line.back() == ' '))
				Line.remove_suffix(1);
			while(!Line.empty() && Line.front() == ' ')
				Line.remove_prefix(1);
			// 跳过元数据 [ti:xx]
			if(Line.size() < 4 || Line.front() != '[')
				continue;
			// 必须是 [num,num]...
			// 找 ']'
			const size_t BracketEnd = Line.find(']');
			if(BracketEnd == std::string_view::npos)
				continue;
			std::string_view Inside = Line.substr(1, BracketEnd - 1);
			const size_t Comma = Inside.find(',');
			if(Comma == std::string_view::npos)
				continue;
			// 解析数字
			int64_t LineStartMs = 0, LineDurMs = 0;
			bool BadInt = false;
			auto ParseI64 = [&BadInt](std::string_view S) -> int64_t {
				int64_t V = 0;
				for(char C : S)
				{
					if(C < '0' || C > '9')
					{
						BadInt = true;
						return 0;
					}
					V = V * 10 + (C - '0');
				}
				return V;
			};
			LineStartMs = ParseI64(Inside.substr(0, Comma));
			LineDurMs = ParseI64(Inside.substr(Comma + 1));
			if(BadInt)
				continue;

			SLyricsLine L;
			L.m_StartMs = LineStartMs;
			L.m_EndMs = LineStartMs + LineDurMs;

			std::string_view Body = Line.substr(BracketEnd + 1);
			std::string PlainText;
			std::vector<SLyricsWord> vWords;
			// Body 形如：<offset,dur,0>词<offset,dur,0>词...
			size_t i = 0;
			while(i < Body.size())
			{
				if(Body[i] != '<')
				{
					PlainText.push_back(Body[i]);
					++i;
					continue;
				}
				const size_t GtPos = Body.find('>', i);
				if(GtPos == std::string_view::npos)
					break;
				std::string_view Tag = Body.substr(i + 1, GtPos - i - 1);
				const size_t C1 = Tag.find(',');
				if(C1 == std::string_view::npos)
				{
					i = GtPos + 1;
					continue;
				}
				const size_t C2 = Tag.find(',', C1 + 1);
				if(C2 == std::string_view::npos)
				{
					i = GtPos + 1;
					continue;
				}
				BadInt = false;
				const int64_t WordOffMs = ParseI64(Tag.substr(0, C1));
				const int64_t WordDurMs = ParseI64(Tag.substr(C1 + 1, C2 - C1 - 1));
				if(BadInt)
				{
					i = GtPos + 1;
					continue;
				}
				// 找下一个 '<' 或行尾
				const size_t Next = Body.find('<', GtPos + 1);
				const size_t TextEnd = Next == std::string_view::npos ? Body.size() : Next;
				std::string_view WordText = Body.substr(GtPos + 1, TextEnd - GtPos - 1);
				SLyricsWord W;
				W.m_StartMs = LineStartMs + WordOffMs;
				W.m_EndMs = W.m_StartMs + WordDurMs;
				W.m_Text = std::string(WordText);
				vWords.push_back(std::move(W));
				PlainText.append(WordText);
				i = TextEnd;
			}
			L.m_RawText = std::move(PlainText);
			L.m_vWords = std::move(vWords);
			if(!L.m_RawText.empty())
				pOut->m_vLines.push_back(std::move(L));
		}
		if(pOut->m_vLines.empty())
		{
			SetError(pErr, ErrSize, "no lines");
			return false;
		}
		std::sort(pOut->m_vLines.begin(), pOut->m_vLines.end(), [](const SLyricsLine &A, const SLyricsLine &B) {
			return A.m_StartMs < B.m_StartMs;
		});
		// 用下一行 StartMs 修正 EndMs
		for(size_t i = 0; i + 1 < pOut->m_vLines.size(); ++i)
		{
			if(pOut->m_vLines[i].m_EndMs < pOut->m_vLines[i + 1].m_StartMs)
				pOut->m_vLines[i].m_EndMs = pOut->m_vLines[i + 1].m_StartMs;
		}
		if(!pOut->m_vLines.empty() && pOut->m_vLines.back().m_EndMs <= pOut->m_vLines.back().m_StartMs)
			pOut->m_vLines.back().m_EndMs = pOut->m_vLines.back().m_StartMs + LAST_LINE_TAIL_MS;
		return true;
	}

	struct CLyricsSourceKugou::SImpl
	{
		enum class EStage
		{
			IDLE,
			SEARCH,
			LYRIC_SEARCH,
			DOWNLOAD,
		};

		IHttp *m_pHttp = nullptr;
		int m_TimeoutMs = 8000;
		EStage m_Stage = EStage::IDLE;
		std::shared_ptr<CHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_Query;
		SKugouSearchHit m_Hit;
		SKugouLyricCandidate m_Candidate;
	};

	namespace
	{

		void DispatchKugouRequest(CLyricsSourceKugou::SImpl *pImpl, const std::string &Url)
		{
			pImpl->m_pRequest = std::make_shared<CHttpRequest>(Url.c_str());
			pImpl->m_pRequest->Timeout(CTimeout{5000, pImpl->m_TimeoutMs, 0, 0});
			pImpl->m_pRequest->LogProgress(HTTPLOG::FAILURE);
			pImpl->m_pRequest->AllowInsecureProtocol(true);
			pImpl->m_pRequest->HeaderString("User-Agent", "QmClient (https://github.com/Q1menG)");
			pImpl->m_pHttp->Run(pImpl->m_pRequest);
		}

		bool ReadKugouResponseBody(CHttpRequest *pReq, std::vector<unsigned char> *pOut)
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

	} // anonymous namespace

	CLyricsSourceKugou::CLyricsSourceKugou(IHttp *pHttp, int TimeoutMs) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
	}

	CLyricsSourceKugou::~CLyricsSourceKugou()
	{
		Cancel();
	}

	void CLyricsSourceKugou::Cancel()
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
		m_pImpl->m_Candidate = {};
	}

	void CLyricsSourceKugou::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		if(m_pImpl->m_pHttp == nullptr)
		{
			if(Error)
				Error("no IHttp");
			return;
		}
		if(Query.m_Title.empty())
		{
			if(Done)
				Done({});
			return;
		}
		m_pImpl->m_Query = Query;
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		m_pImpl->m_Stage = SImpl::EStage::SEARCH;
		DispatchKugouRequest(m_pImpl.get(), BuildKugouSearchUrl(Query));
	}

	void CLyricsSourceKugou::Tick()
	{
		if(m_pImpl->m_Stage == SImpl::EStage::IDLE || !m_pImpl->m_pRequest || !m_pImpl->m_pRequest->Done())
			return;

		std::vector<unsigned char> vBody;
		const bool Ok = ReadKugouResponseBody(m_pImpl->m_pRequest.get(), &vBody);
		m_pImpl->m_pRequest.reset();
		if(!Ok)
		{
			FSourceErrorCallback Error = std::move(m_pImpl->m_Error);
			FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
			m_pImpl->m_Stage = SImpl::EStage::IDLE;
			if(Done)
				Done({});
			else if(Error)
				Error("request failed");
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::SEARCH)
		{
			SKugouSearchHit Hit;
			if(ParseKugouSearchResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Query, &Hit))
			{
				m_pImpl->m_Hit = std::move(Hit);
				const int DurMs = m_pImpl->m_Hit.m_DurationMs > 0 ? m_pImpl->m_Hit.m_DurationMs : m_pImpl->m_Query.m_DurationSec * 1000;
				m_pImpl->m_Stage = SImpl::EStage::LYRIC_SEARCH;
				DispatchKugouRequest(m_pImpl.get(), BuildKugouLyricSearchUrl(m_pImpl->m_Hit.m_FileHash, DurMs));
				return;
			}
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::LYRIC_SEARCH)
		{
			SKugouLyricCandidate Candidate;
			if(ParseKugouLyricSearchResponse((const char *)vBody.data(), vBody.size(), &Candidate))
			{
				m_pImpl->m_Candidate = std::move(Candidate);
				m_pImpl->m_Stage = SImpl::EStage::DOWNLOAD;
				DispatchKugouRequest(m_pImpl.get(), BuildKugouLyricDownloadUrl(m_pImpl->m_Candidate.m_Id, m_pImpl->m_Candidate.m_AccessKey));
				return;
			}
		}
		else if(m_pImpl->m_Stage == SImpl::EStage::DOWNLOAD)
		{
			std::string KrcText;
			if(ParseKugouLyricDownloadResponse((const char *)vBody.data(), vBody.size(), &KrcText) && !KrcText.empty())
			{
				SSourceCandidate Candidate;
				Candidate.m_RawText = std::move(KrcText);
				Candidate.m_FormatHint = EFormat::KRC;
				Candidate.m_Metadata.m_Title = m_pImpl->m_Hit.m_Title;
				Candidate.m_Metadata.m_Artist = m_pImpl->m_Hit.m_Artist;
				Candidate.m_Metadata.m_DurationSec = m_pImpl->m_Hit.m_DurationMs > 0 ? m_pImpl->m_Hit.m_DurationMs / 1000 : m_pImpl->m_Query.m_DurationSec;
				Candidate.m_SourceId = Id();
				Candidate.m_SourceScore = 1.0f;

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
