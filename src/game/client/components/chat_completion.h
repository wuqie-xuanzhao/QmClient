// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_CHAT_COMPLETION_H
#define GAME_CLIENT_COMPONENTS_CHAT_COMPLETION_H

#include <cstddef>
#include <string>
#include <vector>

namespace QmChatCompletion
{
	enum class EProvider
	{
		NONE,
		PLAYER,
		MAP,
	};

	struct SContext
	{
		EProvider m_Provider = EProvider::NONE;
		std::string m_Query;
		size_t m_ReplaceStart = 0;
		size_t m_ReplaceEnd = 0;
		bool m_NeedsArgumentSeparator = false;
		bool m_AppendColon = false;
		bool m_QuoteCandidate = true;
	};

	struct SCandidate
	{
		std::string m_Value;
		std::string m_Detail;
		int m_Rank = 0;
		int m_MatchOffset = -1;
		int m_MatchLength = 0;
	};

	bool ParseContext(const char *pInput, size_t CursorOffset, SContext &Context);
	bool ParsePlayerTabContext(const char *pInput, size_t CursorOffset, SContext &Context);
	bool ApplyCandidate(const char *pInput, const SContext &Context, const char *pCandidate, char *pOutput, size_t OutputSize, size_t &CursorOffset);
	bool ExtractMapNameFromVoteOption(const char *pDescription, std::string &MapName);
	bool ExtractMapCategory(const char *pCommunityType, const char *pServerName, std::string &Category);
	void AddMatchingCandidate(std::vector<SCandidate> &vCandidates, const char *pValue, const char *pQuery, bool MatchPinyin = false, const char *pDetail = nullptr);
	void SortCandidates(std::vector<SCandidate> &vCandidates);
}

#endif
