// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef ENGINE_SHARED_QM_REMOVED_CONFIG_H
#define ENGINE_SHARED_QM_REMOVED_CONFIG_H

#include <base/str.h>

#include <string>
#include <string_view>

namespace QmRemovedConfig
{
	inline std::string ReadToken(std::string_view &Command)
	{
		while(!Command.empty() && str_isspace(Command.front()))
			Command.remove_prefix(1);
		const bool Quoted = !Command.empty() && Command.front() == '"';
		if(Quoted)
			Command.remove_prefix(1);
		size_t Length = 0;
		while(Length < Command.size() && (Quoted ? Command[Length] != '"' : !str_isspace(Command[Length]) && Command[Length] != ';' && Command[Length] != '#'))
			++Length;
		std::string Token(Command.substr(0, Length));
		Command.remove_prefix(Length);
		if(Quoted && !Command.empty())
			Command.remove_prefix(1);
		return Token;
	}

	inline bool IsFocusCommand(std::string_view Command)
	{
		std::string Name = ReadToken(Command);
		if(str_comp_nocase(Name.c_str(), "toggle") == 0 || str_comp_nocase(Name.c_str(), "+toggle") == 0 ||
			str_comp_nocase(Name.c_str(), "+toggle_restore") == 0 || str_comp_nocase(Name.c_str(), "reset") == 0)
			Name = ReadToken(Command);
		const char *pSuffix = str_startswith_nocase(Name.c_str(), "qm_focus_mode");
		return pSuffix != nullptr && (*pSuffix == '\0' || *pSuffix == '_');
	}

	inline std::string CleanFocusCommands(std::string_view Commands)
	{
		const std::string_view Original = Commands;
		const bool HasMultiCommandPrefix = Commands.substr(0, 3) == "mc;";
		if(HasMultiCommandPrefix)
			Commands.remove_prefix(3);
		std::string Cleaned;
		bool Removed = false;
		bool InQuotes = false;
		size_t Start = 0;
		const auto AppendCommand = [&](size_t End) {
			const std::string_view Part = Commands.substr(Start, End - Start);
			if(IsFocusCommand(Part))
				Removed = true;
			else
			{
				std::string_view Remaining = Part;
				if(!ReadToken(Remaining).empty())
				{
					if(!Cleaned.empty())
						Cleaned += ';';
					Cleaned.append(Part);
				}
			}
		};
		// 与控制台一致：引号内的分号属于参数，反斜杠只在紧邻引号时影响命令分段。
		for(size_t Index = 0; Index < Commands.size(); ++Index)
		{
			if(Commands[Index] == '\\' && Index + 1 < Commands.size() && Commands[Index + 1] == '"')
				++Index;
			else if(Commands[Index] == '"')
				InQuotes = !InQuotes;
			else if(!InQuotes && Commands[Index] == '#')
				break;
			else if(!InQuotes && Commands[Index] == ';')
			{
				AppendCommand(Index);
				Start = Index + 1;
			}
		}
		AppendCommand(Commands.size());
		if(!Removed)
			return std::string(Original);
		if(HasMultiCommandPrefix && !Cleaned.empty())
			Cleaned.insert(0, "mc;");
		return Cleaned;
	}
}

#endif
