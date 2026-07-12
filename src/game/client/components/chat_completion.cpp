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

		static constexpr SPinyinEntry s_aPinyinEntries[] = {
#include "chat_completion_pinyin.inc"
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
		const bool Quote = str_find(pCandidate, " ") != nullptr || str_find(pCandidate, "\"") != nullptr || str_find(pCandidate, "\\") != nullptr;
		if(Quote)
			Result.push_back('"');
		for(const char *pCharacter = pCandidate; *pCharacter != '\0'; ++pCharacter)
		{
			if(*pCharacter == '"' || *pCharacter == '\\')
				Result.push_back('\\');
			Result.push_back(*pCharacter);
		}
		if(Quote)
			Result.push_back('"');

		const char *pSuffix = pInput + Context.m_ReplaceEnd;
		CursorOffset = Result.size();
		if(pSuffix[0] == '\0')
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

	void AddMatchingCandidate(std::vector<SCandidate> &vCandidates, const char *pValue, const char *pQuery, bool MatchPinyin)
	{
		if(pValue == nullptr || pValue[0] == '\0' || pQuery == nullptr)
			return;

		SCandidate Candidate;
		Candidate.m_Value = pValue;
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
			return str_comp_nocase(Left.m_Value.c_str(), Right.m_Value.c_str()) < 0;
		});
	}
}
