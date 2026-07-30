// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "chat_completion.h"

#include <base/system.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace QmChatCompletion
{
	namespace
	{
		struct SPinyinEntry
		{
			int m_Codepoint;
			const char *m_pReading;
		};

		struct SOfficialDdnetMapCategory
		{
			const char *m_pMapName;
			const char *m_pCategory;
		};

		static constexpr SPinyinEntry s_aPinyinEntries[] = {
#include "chat_completion_pinyin.inc"
		};

		static constexpr SOfficialDdnetMapCategory s_aOfficialDdnetMapCategories[] = {
#include "chat_completion_ddnet_maps.inc"
		};

		const char *PinyinReading(int Codepoint)
		{
			const auto It = std::lower_bound(std::begin(s_aPinyinEntries), std::end(s_aPinyinEntries), Codepoint, [](const SPinyinEntry &Entry, int Value) {
				return Entry.m_Codepoint < Value;
			});
			return It != std::end(s_aPinyinEntries) && It->m_Codepoint == Codepoint ? It->m_pReading : nullptr;
		}

		void BuildPinyinKeys(const char *pValue, std::string &Full, std::string &Initials)
		{
			Full.clear();
			Initials.clear();
			const char *pCursor = pValue;
			while(*pCursor != '\0')
			{
				const int Codepoint = str_utf8_decode(&pCursor);
				if(Codepoint < 0)
					continue;
				if(Codepoint < 128)
				{
					if(std::isalnum(Codepoint))
					{
						const char Lower = (char)std::tolower(Codepoint);
						Full.push_back(Lower);
						Initials.push_back(Lower);
					}
					continue;
				}

				const char *pReading = PinyinReading(Codepoint);
				if(pReading != nullptr)
				{
					Full.append(pReading);
					Initials.push_back(pReading[0]);
				}
			}
		}

		EProvider ProviderForCommand(const char *pCommand)
		{
			struct SCommandProvider
			{
				const char *m_pCommand;
				EProvider m_Provider;
			};
			static constexpr SCommandProvider s_aCommandProviders[] = {
				{"w", EProvider::PLAYER},
				{"whisper", EProvider::PLAYER},
				{"map", EProvider::MAP},
				{"mapinfo", EProvider::MAP},
			};
			for(const SCommandProvider &CommandProvider : s_aCommandProviders)
			{
				if(str_comp_nocase(pCommand, CommandProvider.m_pCommand) == 0)
					return CommandProvider.m_Provider;
			}
			return EProvider::NONE;
		}

		const char *MapCategoryKeyFromText(const char *pText)
		{
			if(pText == nullptr || pText[0] == '\0')
				return nullptr;
			if(str_find_nocase(pText, "DDmaX"))
			{
				if(str_find_nocase(pText, "Easy"))
					return "DDmaX Easy";
				if(str_find_nocase(pText, "Next"))
					return "DDmaX Next";
				if(str_find_nocase(pText, "Pro"))
					return "DDmaX Pro";
				if(str_find_nocase(pText, "Nut"))
					return "DDmaX Nut";
				return "DDmaX";
			}
			static constexpr const char *s_apCategories[] = {"Oldschool", "Novice", "Moderate", "Brutal", "Insane", "Dummy", "Solo", "Race", "Fun", "Event"};
			for(const char *pCategory : s_apCategories)
				if(str_find_nocase(pText, pCategory))
					return pCategory;
			return nullptr;
		}
	}

	bool ParseContext(const char *pInput, size_t CursorOffset, SContext &Context)
	{
		Context = {};
		if(pInput == nullptr || pInput[0] != '/')
			return false;

		const size_t InputLength = str_length(pInput);
		CursorOffset = std::min(CursorOffset, InputLength);
		size_t CommandEnd = 1;
		while(CommandEnd < InputLength && pInput[CommandEnd] != ' ' && pInput[CommandEnd] != '\t')
			++CommandEnd;
		if(CommandEnd == 1 || CursorOffset < CommandEnd)
			return false;

		char aCommand[64];
		str_truncate(aCommand, sizeof(aCommand), pInput + 1, CommandEnd - 1);
		Context.m_Provider = ProviderForCommand(aCommand);
		if(Context.m_Provider == EProvider::NONE)
			return false;

		size_t ArgumentStart = CommandEnd;
		while(ArgumentStart < InputLength && (pInput[ArgumentStart] == ' ' || pInput[ArgumentStart] == '\t'))
			++ArgumentStart;
		Context.m_NeedsArgumentSeparator = ArgumentStart == CommandEnd;
		if(CursorOffset < ArgumentStart)
			return false;

		if(ArgumentStart < InputLength && pInput[ArgumentStart] == '"')
		{
			size_t ContentEnd = ArgumentStart + 1;
			bool Escaped = false;
			while(ContentEnd < InputLength)
			{
				const char Character = pInput[ContentEnd];
				if(Character == '"' && !Escaped)
					break;
				Escaped = Character == '\\' && !Escaped;
				if(Character != '\\')
					Escaped = false;
				++ContentEnd;
			}
			if(ContentEnd < InputLength || ContentEnd < CursorOffset)
				return false;
			Context.m_ReplaceStart = ArgumentStart;
			Context.m_ReplaceEnd = ContentEnd < InputLength ? ContentEnd + 1 : ContentEnd;
			const size_t QueryEnd = std::min(CursorOffset, ContentEnd);
			Context.m_Query.assign(pInput + ArgumentStart + 1, QueryEnd - ArgumentStart - 1);
			return true;
		}

		size_t ArgumentEnd = ArgumentStart;
		while(ArgumentEnd < InputLength && pInput[ArgumentEnd] != ' ' && pInput[ArgumentEnd] != '\t')
			++ArgumentEnd;
		if(ArgumentEnd < InputLength || CursorOffset > ArgumentEnd)
			return false;
		Context.m_ReplaceStart = ArgumentStart;
		Context.m_ReplaceEnd = ArgumentEnd;
		Context.m_Query.assign(pInput + ArgumentStart, CursorOffset - ArgumentStart);
		return true;
	}

	bool ApplyCandidate(const char *pInput, const SContext &Context, const char *pCandidate, char *pOutput, size_t OutputSize, size_t &CursorOffset)
	{
		if(pInput == nullptr || pCandidate == nullptr || pOutput == nullptr || OutputSize == 0 || Context.m_ReplaceStart > Context.m_ReplaceEnd || Context.m_ReplaceEnd > str_length(pInput))
			return false;

		std::string Result(pInput, Context.m_ReplaceStart);
		if(Context.m_NeedsArgumentSeparator)
			Result.push_back(' ');
		const bool Quote = Context.m_QuoteCandidate && (str_find(pCandidate, " ") != nullptr || str_find(pCandidate, "\"") != nullptr || str_find(pCandidate, "\\") != nullptr);
		if(Quote)
			Result.push_back('"');
		for(const char *pCharacter = pCandidate; *pCharacter != '\0'; ++pCharacter)
		{
			if(Quote && (*pCharacter == '"' || *pCharacter == '\\'))
				Result.push_back('\\');
			Result.push_back(*pCharacter);
		}
		if(Quote)
			Result.push_back('"');

		const char *pSuffix = pInput + Context.m_ReplaceEnd;
		CursorOffset = Result.size();
		if(Context.m_AppendColon)
		{
			Result.push_back(':');
			if(pSuffix[0] == '\0' || pSuffix[0] != ' ')
				Result.push_back(' ');
			CursorOffset = Result.size();
		}
		else if(pSuffix[0] == '\0')
		{
			Result.push_back(' ');
			CursorOffset = Result.size();
		}
		Result.append(pSuffix);
		if(Result.size() >= OutputSize)
			return false;
		str_copy(pOutput, Result.c_str(), OutputSize);
		return true;
	}

	bool ParsePlayerTabContext(const char *pInput, size_t CursorOffset, SContext &Context)
	{
		Context = {};
		if(pInput == nullptr || pInput[0] == '/')
			return false;
		const size_t InputLength = str_length(pInput);
		CursorOffset = std::min(CursorOffset, InputLength);
		size_t Start = CursorOffset;
		while(Start > 0 && pInput[Start - 1] != ' ' && pInput[Start - 1] != '\t')
			--Start;
		size_t End = CursorOffset;
		while(End < InputLength && pInput[End] != ' ' && pInput[End] != '\t')
			++End;
		Context.m_Provider = EProvider::PLAYER;
		Context.m_Query.assign(pInput + Start, CursorOffset - Start);
		Context.m_ReplaceStart = Start;
		Context.m_ReplaceEnd = End;
		Context.m_AppendColon = Start == 0;
		Context.m_QuoteCandidate = false;
		return true;
	}

	bool ExtractMapNameFromVoteOption(const char *pDescription, std::string &MapName)
	{
		MapName.clear();
		if(pDescription == nullptr)
			return false;
		const char *pMapPrefix = str_find_nocase(pDescription, "Map:");
		if(pMapPrefix != nullptr)
		{
			pMapPrefix += 4;
			while(*pMapPrefix == ' ')
				++pMapPrefix;
			MapName = pMapPrefix;
			return !MapName.empty();
		}
		const char *pBy = str_find_nocase(pDescription, " by ");
		if(pBy != nullptr && (str_find(pDescription, "★") != nullptr || str_find(pDescription, "✰") != nullptr))
		{
			MapName.assign(pDescription, pBy - pDescription);
			return !MapName.empty();
		}
		return false;
	}

	bool ExtractMapCategory(const char *pCommunityType, const char *pServerName, std::string &Category)
	{
		Category.clear();
		const char *pCategory = MapCategoryKeyFromText(pCommunityType);
		if(pCategory == nullptr)
			pCategory = MapCategoryKeyFromText(pServerName);
		if(pCategory == nullptr)
			return false;
		Category = pCategory;
		return true;
	}

	bool FindOfficialDdnetMapCategory(const char *pMapName, std::string &Category)
	{
		Category.clear();
		if(pMapName == nullptr || pMapName[0] == '\0')
			return false;

		static const std::vector<const SOfficialDdnetMapCategory *> s_vpSortedCategories = []() {
			std::vector<const SOfficialDdnetMapCategory *> vpCategories;
			vpCategories.reserve(std::size(s_aOfficialDdnetMapCategories));
			for(const SOfficialDdnetMapCategory &Entry : s_aOfficialDdnetMapCategories)
				vpCategories.push_back(&Entry);
			std::sort(vpCategories.begin(), vpCategories.end(), [](const auto *pLeft, const auto *pRight) {
				return str_comp(pLeft->m_pMapName, pRight->m_pMapName) < 0;
			});
			return vpCategories;
		}();

		const auto It = std::lower_bound(s_vpSortedCategories.begin(), s_vpSortedCategories.end(), pMapName, [](const auto *pEntry, const char *pName) {
			return str_comp(pEntry->m_pMapName, pName) < 0;
		});
		if(It != s_vpSortedCategories.end() && str_comp((*It)->m_pMapName, pMapName) == 0)
		{
			Category = (*It)->m_pCategory;
			return true;
		}

		const char *pCaseInsensitiveCategory = nullptr;
		for(const SOfficialDdnetMapCategory &Entry : s_aOfficialDdnetMapCategories)
		{
			if(str_utf8_comp_nocase(Entry.m_pMapName, pMapName) != 0)
				continue;
			if(pCaseInsensitiveCategory != nullptr && str_comp(pCaseInsensitiveCategory, Entry.m_pCategory) != 0)
				return false;
			pCaseInsensitiveCategory = Entry.m_pCategory;
		}
		if(pCaseInsensitiveCategory == nullptr)
			return false;
		Category = pCaseInsensitiveCategory;
		return true;
	}

	void ResolveMapCompletionCategory(const char *pMapName, bool IsDdnetMode, const char *pFallbackCategory, std::string &Category)
	{
		if(IsDdnetMode)
		{
			if(!FindOfficialDdnetMapCategory(pMapName, Category))
				Category = "Other";
			return;
		}
		Category = pFallbackCategory != nullptr ? pFallbackCategory : "";
	}

	float CalculateCandidatePopupWidth(float MaximumWidth, float ContentWidth, bool HasScrollbar)
	{
		const float HorizontalPadding = 10.0f;
		const float ScrollbarSpace = HasScrollbar ? 4.0f : 0.0f;
		return std::min(std::max(80.0f, ContentWidth + HorizontalPadding + ScrollbarSpace), std::max(80.0f, MaximumWidth));
	}

	void AddMatchingCandidate(std::vector<SCandidate> &vCandidates, const char *pValue, const char *pQuery, bool MatchPinyin, const char *pDetail)
	{
		if(pValue == nullptr || pValue[0] == '\0' || pQuery == nullptr)
			return;

		SCandidate Candidate;
		Candidate.m_Value = pValue;
		if(pDetail != nullptr)
			Candidate.m_Detail = pDetail;
		Candidate.m_MatchLength = str_length(pQuery);
		const char *pDirectMatch = pQuery[0] == '\0' ? nullptr : str_utf8_find_nocase(pValue, pQuery);
		if(pQuery[0] == '\0' || pDirectMatch != nullptr)
		{
			Candidate.m_MatchOffset = pQuery[0] == '\0' ? -1 : (int)(pDirectMatch - pValue);
			Candidate.m_Rank = Candidate.m_MatchOffset <= 0 ? 0 : 1;
			vCandidates.push_back(std::move(Candidate));
			return;
		}
		if(!MatchPinyin)
			return;

		std::string Full;
		std::string Initials;
		BuildPinyinKeys(pValue, Full, Initials);
		const char *pFullMatch = str_find_nocase(Full.c_str(), pQuery);
		const char *pInitialMatch = str_find_nocase(Initials.c_str(), pQuery);
		if(pFullMatch == nullptr && pInitialMatch == nullptr)
			return;
		const bool Prefix = (pFullMatch == Full.c_str()) || (pInitialMatch == Initials.c_str());
		Candidate.m_Rank = Prefix ? 2 : 3;
		Candidate.m_MatchOffset = -1;
		vCandidates.push_back(std::move(Candidate));
	}

	void SortCandidates(std::vector<SCandidate> &vCandidates)
	{
		std::stable_sort(vCandidates.begin(), vCandidates.end(), [](const SCandidate &Left, const SCandidate &Right) {
			if(Left.m_Rank != Right.m_Rank)
				return Left.m_Rank < Right.m_Rank;
			const int ValueComparison = str_comp_nocase(Left.m_Value.c_str(), Right.m_Value.c_str());
			if(ValueComparison != 0)
				return ValueComparison < 0;
			return !Left.m_Detail.empty() && Right.m_Detail.empty();
		});
	}
}
