// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_KEYWORD_REPLY_RULES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_KEYWORD_REPLY_RULES_H

#include <base/system.h>

namespace QmKeywordReplyRules
{
	struct SEditorChanges
	{
		bool m_Added = false;
		bool m_Removed = false;
		bool m_TriggerText = false;
		bool m_ReplyText = false;
		bool m_Rename = false;
		bool m_Regex = false;

		bool Any() const
		{
			return m_Added || m_Removed || m_TriggerText || m_ReplyText || m_Rename || m_Regex;
		}

		bool ShouldCommit(bool RenderOnly) const
		{
			return Any() && !RenderOnly;
		}
	};

	inline bool EditorConfigChanged(bool Initialized, const char *pCachedConfig, const char *pConfig)
	{
		return !Initialized || str_comp(pCachedConfig != nullptr ? pCachedConfig : "", pConfig != nullptr ? pConfig : "") != 0;
	}

	inline void EncodeForConfig(const char *pRules, char *pOut, size_t OutSize)
	{
		if(!pOut || OutSize == 0)
			return;

		pOut[0] = '\0';
		if(!pRules)
			return;

		size_t OutPos = 0;
		for(const char *pCursor = pRules; *pCursor && OutPos + 1 < OutSize; ++pCursor)
		{
			if(*pCursor == '\\')
			{
				if(OutPos + 2 >= OutSize)
					break;
				pOut[OutPos++] = '\\';
				pOut[OutPos++] = '\\';
			}
			else if(*pCursor == '\r')
			{
				if(pCursor[1] == '\n')
					continue;
				if(OutPos + 2 >= OutSize)
					break;
				pOut[OutPos++] = '\\';
				pOut[OutPos++] = 'n';
			}
			else if(*pCursor == '\n')
			{
				if(OutPos + 2 >= OutSize)
					break;
				pOut[OutPos++] = '\\';
				pOut[OutPos++] = 'n';
			}
			else
			{
				pOut[OutPos++] = *pCursor;
			}
		}
		pOut[OutPos] = '\0';
	}

	inline void DecodeFromConfig(const char *pRules, char *pOut, size_t OutSize)
	{
		if(!pOut || OutSize == 0)
			return;

		pOut[0] = '\0';
		if(!pRules)
			return;

		size_t OutPos = 0;
		for(const char *pCursor = pRules; *pCursor && OutPos + 1 < OutSize; ++pCursor)
		{
			if(pCursor[0] == '\\' && pCursor[1] == '\\')
			{
				pOut[OutPos++] = '\\';
				++pCursor;
			}
			else if(pCursor[0] == '\\' && pCursor[1] == 'n')
			{
				pOut[OutPos++] = '\n';
				++pCursor;
			}
			else
			{
				pOut[OutPos++] = *pCursor;
			}
		}
		pOut[OutPos] = '\0';
	}

} // namespace QmKeywordReplyRules

#endif
