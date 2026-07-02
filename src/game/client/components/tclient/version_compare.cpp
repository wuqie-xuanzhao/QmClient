#include "version_compare.h"

#include <base/str.h>

namespace
{

	void NormalizeVersion(const char *pStr, char *pBuf, size_t BufSize)
	{
		if(!pBuf || BufSize == 0)
			return;
		pBuf[0] = '\0';
		if(!pStr)
			return;

		pStr = str_skip_whitespaces_const(pStr);
		if(pStr[0] == 'v' || pStr[0] == 'V')
			pStr++;

		str_copy(pBuf, pStr, BufSize);
		int End = str_length(pBuf);
		while(End > 0 && str_isspace(pBuf[End - 1]))
		{
			pBuf[End - 1] = '\0';
			End--;
		}
	}

	const char *SkipDot(const char *pStr)
	{
		return pStr && pStr[0] == '.' ? pStr + 1 : pStr;
	}

	bool ParseNextComponent(const char *&pVersion, int &OutValue)
	{
		OutValue = 0;
		if(!pVersion || pVersion[0] == '\0')
			return false;

		const char *pCursor = pVersion;
		pCursor = SkipDot(pCursor);
		if(!pCursor || pCursor[0] == '\0')
		{
			pVersion = pCursor;
			return false;
		}

		int Value = 0;
		bool ParsedDigit = false;
		while(*pCursor >= '0' && *pCursor <= '9')
		{
			ParsedDigit = true;
			Value = Value * 10 + (*pCursor - '0');
			pCursor++;
		}

		if(!ParsedDigit)
			return false;

		if(*pCursor != '\0' && *pCursor != '.')
			return false;

		OutValue = Value;
		pVersion = pCursor;
		return true;
	}

	int CompareNormalizedVersions(const char *pLeft, const char *pRight)
	{
		const char *pLeftCursor = pLeft;
		const char *pRightCursor = pRight;

		for(int Index = 0; Index < 4; ++Index)
		{
			int LeftValue = 0;
			int RightValue = 0;
			const bool LeftHasValue = ParseNextComponent(pLeftCursor, LeftValue);
			const bool RightHasValue = ParseNextComponent(pRightCursor, RightValue);

			if(!LeftHasValue && !RightHasValue)
				return 0;
			if(LeftValue != RightValue)
				return LeftValue < RightValue ? -1 : 1;
		}

		return 0;
	}

} // namespace

bool IsRemoteVersionNewer(const char *pRemoteVersion, const char *pLocalVersion)
{
	char aRemote[64];
	char aLocal[64];
	NormalizeVersion(pRemoteVersion, aRemote, sizeof(aRemote));
	NormalizeVersion(pLocalVersion, aLocal, sizeof(aLocal));

	if(aRemote[0] == '\0' || aLocal[0] == '\0')
		return false;

	return CompareNormalizedVersions(aRemote, aLocal) > 0;
}
