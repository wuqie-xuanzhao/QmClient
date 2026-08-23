// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_lyrics_source_apple_music.h"

#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/http.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace QmLyrics
{

	namespace
	{
		constexpr const char *APPLE_MUSIC_BROWSE_URL = "https://music.apple.com/us/browse";
		constexpr const char *APPLE_MUSIC_ASSETS_BASE = "https://music.apple.com/assets/index";
		constexpr const char *APPLE_MUSIC_API_BASE = "https://amp-api.music.apple.com/v1";
		constexpr const char *APPLE_MUSIC_USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/110.0.0.0 Safari/537.36";

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage != nullptr ? pMessage : "", ErrSize);
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

		void AppendQueryParam(std::string *pOut, std::string_view Key, std::string_view Value, bool First)
		{
			pOut->push_back(First ? '?' : '&');
			pOut->append(Key);
			pOut->push_back('=');
			UrlEncode(Value, pOut);
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int JsonInt(const json_value *pVal, int Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return (int)pVal->u.integer;
			if(pVal->type == json_double)
				return (int)pVal->u.dbl;
			return Default;
		}

		bool ParseJsonObjectRoot(const char *pBody, size_t BodyLen, json_value **ppRoot, char *pErr, size_t ErrSize)
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

		const json_value *FirstArrayItem(const json_value *pArray)
		{
			if(pArray == nullptr || pArray->type != json_array || pArray->u.array.length == 0)
				return nullptr;
			return pArray->u.array.values[0];
		}

		bool ContainsTimedTtmlMarkers(std::string_view Text)
		{
			return Text.find("begin=") != std::string_view::npos && Text.find("end=") != std::string_view::npos;
		}

		bool IsAppleMusicIndexSuffix(std::string_view Suffix)
		{
			if(Suffix.empty())
				return false;
			for(char C : Suffix)
			{
				if(C == '/' || C == '\\' || C == '<' || C == '>' || C == '"' || C == '\'' ||
					C == ' ' || C == '\t' || C == '\r' || C == '\n')
				{
					return false;
				}
			}
			return true;
		}

		std::string BuildAppleMusicSearchTerm(const SSourceQuery &Query)
		{
			std::string Term;
			if(!Query.m_Artist.empty())
				Term.append(Query.m_Artist);
			if(!Query.m_Title.empty())
			{
				if(!Term.empty())
					Term.push_back(' ');
				Term.append(Query.m_Title);
			}
			return Term;
		}

		bool ReadResponseBody(IHttpRequest *pReq, std::vector<unsigned char> *pOut)
		{
			if(pReq == nullptr || pOut == nullptr || pReq->State() != EHttpState::DONE || pReq->StatusCode() < 200 || pReq->StatusCode() >= 300)
				return false;
			unsigned char *pBody = nullptr;
			size_t BodyLen = 0;
			pReq->Result(&pBody, &BodyLen);
			if(pBody == nullptr || BodyLen == 0)
				return false;
			pOut->assign(pBody, pBody + BodyLen);
			return true;
		}

		void AddBaseHeaders(IHttpRequest *pRequest)
		{
			pRequest->HeaderString("User-Agent", APPLE_MUSIC_USER_AGENT);
			pRequest->HeaderString("Accept", "application/json");
			pRequest->HeaderString("Origin", "https://music.apple.com");
			pRequest->HeaderString("Referer", "https://music.apple.com/");
		}

		void AddLongHeader(IHttpRequest *pRequest, const char *pName, const std::string &Value)
		{
			if(Value.empty())
				return;
			std::string Header = pName;
			Header.append(": ");
			Header.append(Value);
			pRequest->Header(Header.c_str());
		}

	} // anonymous namespace

	std::string BuildAppleMusicBrowseUrl()
	{
		return APPLE_MUSIC_BROWSE_URL;
	}

	std::string BuildAppleMusicIndexJsUrl(std::string_view BrowseHtml)
	{
		size_t SearchPos = 0;
		while(SearchPos < BrowseHtml.size())
		{
			const size_t IndexPos = BrowseHtml.find("index", SearchPos);
			if(IndexPos == std::string_view::npos)
				return {};

			const size_t JsPos = BrowseHtml.find(".js", IndexPos + 5);
			if(JsPos != std::string_view::npos && JsPos + 3 < BrowseHtml.size() &&
				(BrowseHtml[JsPos + 3] == '"' || BrowseHtml[JsPos + 3] == '\''))
			{
				const std::string_view Suffix = BrowseHtml.substr(IndexPos + 5, JsPos - (IndexPos + 5));
				if(!IsAppleMusicIndexSuffix(Suffix))
				{
					SearchPos = IndexPos + 5;
					continue;
				}
				std::string Url = APPLE_MUSIC_ASSETS_BASE;
				Url.append(Suffix);
				Url.append(".js");
				return Url;
			}
			SearchPos = IndexPos + 5;
		}
		return {};
	}

	std::string ParseAppleMusicAccessToken(std::string_view JsText)
	{
		const size_t TokenStart = JsText.find("eyJh");
		if(TokenStart == std::string_view::npos)
			return {};
		size_t TokenEnd = TokenStart;
		while(TokenEnd < JsText.size() && JsText[TokenEnd] != '"' && JsText[TokenEnd] != '\'' &&
			JsText[TokenEnd] != '\\' && JsText[TokenEnd] != '\r' && JsText[TokenEnd] != '\n' &&
			JsText[TokenEnd] != '\t' && JsText[TokenEnd] != ' ')
		{
			++TokenEnd;
		}
		return std::string(JsText.substr(TokenStart, TokenEnd - TokenStart));
	}

	bool ParseAppleMusicStorefrontResponse(const char *pBody, size_t BodyLen, std::string *pStorefront, std::string *pLanguage, char *pErr, size_t ErrSize)
	{
		if(pStorefront == nullptr || pLanguage == nullptr)
			return false;
		pStorefront->clear();
		pLanguage->clear();
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';

		json_value *pRoot = nullptr;
		if(!ParseJsonObjectRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;

		const json_value &Root = *pRoot;
		const json_value *pItem = FirstArrayItem(&Root["data"]);
		if(pItem == nullptr || pItem->type != json_object)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing storefront data");
			return false;
		}

		const char *pId = JsonString(&(*pItem)["id"]);
		const json_value *pAttributes = &(*pItem)["attributes"];
		const char *pDefaultLanguageTag = pAttributes != nullptr && pAttributes->type == json_object ? JsonString(&(*pAttributes)["defaultLanguageTag"]) : "";
		if(pId[0] == '\0' || pDefaultLanguageTag[0] == '\0')
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing storefront fields");
			return false;
		}

		*pStorefront = pId;
		*pLanguage = pDefaultLanguageTag;
		json_value_free(pRoot);
		return true;
	}

	std::string BuildAppleMusicSearchUrl(const std::string &Storefront, const std::string &Language, const SSourceQuery &Query)
	{
		std::string Url = APPLE_MUSIC_API_BASE;
		Url.append("/catalog/");
		Url.append(Storefront);
		Url.append("/search");
		bool First = true;
		AppendQueryParam(&Url, "term", BuildAppleMusicSearchTerm(Query), First);
		First = false;
		AppendQueryParam(&Url, "types", "songs", First);
		AppendQueryParam(&Url, "limit", "1", false);
		AppendQueryParam(&Url, "l", Language, false);
		return Url;
	}

	bool ParseAppleMusicSearchResponse(const char *pBody, size_t BodyLen, const SSourceQuery &Query, SAppleMusicSearchHit *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
			return false;
		*pOut = {};
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';

		json_value *pRoot = nullptr;
		if(!ParseJsonObjectRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;

		const json_value &Root = *pRoot;
		const json_value *pSongs = &Root["results"]["songs"];
		const json_value *pSong = pSongs != nullptr && pSongs->type == json_object ? FirstArrayItem(&(*pSongs)["data"]) : nullptr;
		if(pSong == nullptr || pSong->type != json_object)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing songs");
			return false;
		}

		const char *pId = JsonString(&(*pSong)["id"]);
		const json_value *pAttributes = &(*pSong)["attributes"];
		if(pId[0] == '\0' || pAttributes == nullptr || pAttributes->type != json_object)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing song fields");
			return false;
		}

		SAppleMusicSearchHit Hit;
		Hit.m_Id = pId;
		Hit.m_Reference = "https://music.apple.com/song/";
		Hit.m_Reference.append(Hit.m_Id);
		Hit.m_Metadata.m_Title = JsonString(&(*pAttributes)["name"]);
		Hit.m_Metadata.m_Artist = JsonString(&(*pAttributes)["artistName"]);
		Hit.m_Metadata.m_Album = JsonString(&(*pAttributes)["albumName"]);
		Hit.m_Metadata.m_DurationSec = JsonInt(&(*pAttributes)["durationInMillis"]) / 1000;
		Hit.m_Score = Score(Query, Hit.m_Metadata);
		*pOut = std::move(Hit);
		json_value_free(pRoot);
		return true;
	}

	std::string BuildAppleMusicLyricsUrl(const std::string &Storefront, const std::string &Language, const std::string &SongId)
	{
		std::string Url = APPLE_MUSIC_API_BASE;
		Url.append("/catalog/");
		Url.append(Storefront);
		Url.append("/songs/");
		Url.append(SongId);
		Url.append("?include[songs]=lyrics,syllable-lyrics&l=");
		UrlEncode(Language, &Url);
		return Url;
	}

	bool ParseAppleMusicLyricsResponse(const char *pBody, size_t BodyLen, const SAppleMusicSearchHit &Hit, SSourceCandidate *pOut, char *pErr, size_t ErrSize)
	{
		if(pOut == nullptr)
			return false;
		*pOut = {};
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';

		json_value *pRoot = nullptr;
		if(!ParseJsonObjectRoot(pBody, BodyLen, &pRoot, pErr, ErrSize))
			return false;

		const json_value &Root = *pRoot;
		const json_value *pSong = FirstArrayItem(&Root["data"]);
		if(pSong == nullptr || pSong->type != json_object)
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing lyrics song");
			return false;
		}

		const json_value *pRelationships = &(*pSong)["relationships"];
		const json_value *pSyllableLyrics = pRelationships != nullptr && pRelationships->type == json_object ? &(*pRelationships)["syllable-lyrics"] : nullptr;
		const json_value *pSyllableLyric = pSyllableLyrics != nullptr && pSyllableLyrics->type == json_object ? FirstArrayItem(&(*pSyllableLyrics)["data"]) : nullptr;
		const json_value *pAttributes = pSyllableLyric != nullptr && pSyllableLyric->type == json_object ? &(*pSyllableLyric)["attributes"] : nullptr;
		const char *pTtml = pAttributes != nullptr && pAttributes->type == json_object ? JsonString(&(*pAttributes)["ttml"]) : "";
		if(pTtml[0] == '\0' || !ContainsTimedTtmlMarkers(pTtml))
		{
			json_value_free(pRoot);
			SetError(pErr, ErrSize, "missing timed syllable ttml");
			return false;
		}

		SSourceCandidate Candidate;
		Candidate.m_RawText = pTtml;
		Candidate.m_FormatHint = EFormat::TTML;
		Candidate.m_Metadata = Hit.m_Metadata;
		Candidate.m_SourceId = "apple-music";
		Candidate.m_SourceScore = std::clamp(Hit.m_Score / 100.0f, 0.0f, 1.0f);
		*pOut = std::move(Candidate);
		json_value_free(pRoot);
		return true;
	}

	struct CLyricsSourceAppleMusic::SImpl
	{
		enum class EStage
		{
			IDLE,
			BROWSE,
			INDEX_JS,
			STOREFRONT,
			SEARCH,
			LYRIC,
		};

		IHttp *m_pHttp = nullptr;
		int m_TimeoutMs = 8000;
		const char *m_pMediaUserTokenConfig = nullptr;
		std::string m_MediaUserToken;
		std::string m_AccessToken;
		std::string m_Storefront;
		std::string m_Language;
		bool m_Initialized = false;
		EStage m_Stage = EStage::IDLE;
		std::shared_ptr<IHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_Query;
		SAppleMusicSearchHit m_Hit;
	};

	namespace
	{
		void CompleteAppleMusicEmpty(CLyricsSourceAppleMusic::SImpl *pImpl)
		{
			FSourceDoneCallback Done = std::move(pImpl->m_Done);
			pImpl->m_Error = nullptr;
			pImpl->m_Stage = CLyricsSourceAppleMusic::SImpl::EStage::IDLE;
			pImpl->m_pRequest.reset();
			pImpl->m_Hit = {};
			if(Done)
				Done({});
		}

		void DispatchAppleMusicRequest(CLyricsSourceAppleMusic::SImpl *pImpl, const std::string &Url, CLyricsSourceAppleMusic::SImpl::EStage Stage)
		{
			pImpl->m_Stage = Stage;
			pImpl->m_pRequest = HttpGet(Url.c_str());
			pImpl->m_pRequest->Timeout(CTimeout{5000, pImpl->m_TimeoutMs, 0, 0});
			pImpl->m_pRequest->LogProgress(HTTPLOG::FAILURE);
			AddBaseHeaders(pImpl->m_pRequest.get());
			if(Stage != CLyricsSourceAppleMusic::SImpl::EStage::BROWSE && Stage != CLyricsSourceAppleMusic::SImpl::EStage::INDEX_JS)
			{
				AddLongHeader(pImpl->m_pRequest.get(), "Authorization", std::string("Bearer ") + pImpl->m_AccessToken);
				AddLongHeader(pImpl->m_pRequest.get(), "media-user-token", pImpl->m_MediaUserToken);
				if(!pImpl->m_Language.empty())
					AddLongHeader(pImpl->m_pRequest.get(), "Accept-Language", pImpl->m_Language + ",en;q=0.9");
			}
			pImpl->m_pHttp->Run(pImpl->m_pRequest);
		}

		void DispatchAppleMusicSearch(CLyricsSourceAppleMusic::SImpl *pImpl)
		{
			DispatchAppleMusicRequest(pImpl, BuildAppleMusicSearchUrl(pImpl->m_Storefront, pImpl->m_Language, pImpl->m_Query), CLyricsSourceAppleMusic::SImpl::EStage::SEARCH);
		}

		std::string ReadMediaUserToken(const char *pMediaUserTokenConfig)
		{
			return pMediaUserTokenConfig != nullptr ? pMediaUserTokenConfig : "";
		}

	} // anonymous namespace

	CLyricsSourceAppleMusic::CLyricsSourceAppleMusic(IHttp *pHttp, int TimeoutMs, const char *pMediaUserToken) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
		m_pImpl->m_pMediaUserTokenConfig = pMediaUserToken;
	}

	CLyricsSourceAppleMusic::~CLyricsSourceAppleMusic()
	{
		Cancel();
	}

	void CLyricsSourceAppleMusic::Cancel()
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

	void CLyricsSourceAppleMusic::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
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

		const std::string MediaUserToken = ReadMediaUserToken(m_pImpl->m_pMediaUserTokenConfig);
		if(MediaUserToken.empty())
		{
			if(Done)
				Done({});
			return;
		}
		if(MediaUserToken != m_pImpl->m_MediaUserToken)
		{
			m_pImpl->m_MediaUserToken = MediaUserToken;
			m_pImpl->m_AccessToken.clear();
			m_pImpl->m_Storefront.clear();
			m_pImpl->m_Language.clear();
			m_pImpl->m_Initialized = false;
		}

		m_pImpl->m_Query = Query;
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		if(m_pImpl->m_Initialized)
			DispatchAppleMusicSearch(m_pImpl.get());
		else
			DispatchAppleMusicRequest(m_pImpl.get(), BuildAppleMusicBrowseUrl(), SImpl::EStage::BROWSE);
	}

	void CLyricsSourceAppleMusic::Tick()
	{
		if(m_pImpl->m_Stage == SImpl::EStage::IDLE || !m_pImpl->m_pRequest || !m_pImpl->m_pRequest->Done())
			return;

		std::vector<unsigned char> vBody;
		const bool Ok = ReadResponseBody(m_pImpl->m_pRequest.get(), &vBody);
		m_pImpl->m_pRequest.reset();
		if(!Ok)
		{
			CompleteAppleMusicEmpty(m_pImpl.get());
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::BROWSE)
		{
			const std::string IndexJsUrl = BuildAppleMusicIndexJsUrl(std::string_view((const char *)vBody.data(), vBody.size()));
			if(IndexJsUrl.empty())
			{
				CompleteAppleMusicEmpty(m_pImpl.get());
				return;
			}
			DispatchAppleMusicRequest(m_pImpl.get(), IndexJsUrl, SImpl::EStage::INDEX_JS);
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::INDEX_JS)
		{
			m_pImpl->m_AccessToken = ParseAppleMusicAccessToken(std::string_view((const char *)vBody.data(), vBody.size()));
			if(m_pImpl->m_AccessToken.empty())
			{
				CompleteAppleMusicEmpty(m_pImpl.get());
				return;
			}
			DispatchAppleMusicRequest(m_pImpl.get(), std::string(APPLE_MUSIC_API_BASE) + "/me/storefront", SImpl::EStage::STOREFRONT);
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::STOREFRONT)
		{
			if(!ParseAppleMusicStorefrontResponse((const char *)vBody.data(), vBody.size(), &m_pImpl->m_Storefront, &m_pImpl->m_Language))
			{
				CompleteAppleMusicEmpty(m_pImpl.get());
				return;
			}
			m_pImpl->m_Initialized = true;
			DispatchAppleMusicSearch(m_pImpl.get());
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::SEARCH)
		{
			SAppleMusicSearchHit Hit;
			if(!ParseAppleMusicSearchResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Query, &Hit))
			{
				CompleteAppleMusicEmpty(m_pImpl.get());
				return;
			}
			m_pImpl->m_Hit = std::move(Hit);
			DispatchAppleMusicRequest(m_pImpl.get(), BuildAppleMusicLyricsUrl(m_pImpl->m_Storefront, m_pImpl->m_Language, m_pImpl->m_Hit.m_Id), SImpl::EStage::LYRIC);
			return;
		}

		if(m_pImpl->m_Stage == SImpl::EStage::LYRIC)
		{
			SSourceCandidate Candidate;
			if(ParseAppleMusicLyricsResponse((const char *)vBody.data(), vBody.size(), m_pImpl->m_Hit, &Candidate))
			{
				FSourceDoneCallback Done = std::move(m_pImpl->m_Done);
				m_pImpl->m_Error = nullptr;
				m_pImpl->m_Stage = SImpl::EStage::IDLE;
				if(Done)
					Done({std::move(Candidate)});
				return;
			}
		}

		CompleteAppleMusicEmpty(m_pImpl.get());
	}

} // namespace QmLyrics
