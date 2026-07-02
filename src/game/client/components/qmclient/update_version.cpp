#include "update_version.h"

#include <base/str.h>

#include <climits>

namespace
{

	void NormalizeQmClientVersion(const char *pStr, char *pBuf, size_t BufSize)
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

	bool ReadNextVersionPart(const char *&pCursor, int &OutValue)
	{
		OutValue = 0;
		if(!pCursor || pCursor[0] == '\0')
			return false;

		if(pCursor[0] == '.')
			pCursor++;
		if(pCursor[0] == '\0')
			return false;

		bool HasDigit = false;
		while(*pCursor >= '0' && *pCursor <= '9')
		{
			HasDigit = true;
			const int Digit = *pCursor - '0';
			if(OutValue > (INT_MAX - Digit) / 10)
				return false;
			OutValue = OutValue * 10 + Digit;
			pCursor++;
		}

		if(!HasDigit)
			return false;
		if(*pCursor != '\0' && *pCursor != '.')
			return false;
		return true;
	}

	bool CompareQmClientVersions(const char *pLeft, const char *pRight, int &OutComparison)
	{
		const char *pLeftCursor = pLeft;
		const char *pRightCursor = pRight;
		OutComparison = 0;

		for(int Index = 0; Index < 4; ++Index)
		{
			int LeftPart = 0;
			int RightPart = 0;
			const bool LeftOk = ReadNextVersionPart(pLeftCursor, LeftPart);
			const bool RightOk = ReadNextVersionPart(pRightCursor, RightPart);

			if(!LeftOk && !RightOk)
				return true;
			if(LeftOk != RightOk)
				return false;
			if(LeftPart != RightPart)
			{
				OutComparison = LeftPart < RightPart ? -1 : 1;
				return true;
			}
		}

		return true;
	}

} // namespace

bool IsQmClientRemoteVersionNewer(const char *pRemoteVersion, const char *pLocalVersion)
{
	char aRemote[64];
	char aLocal[64];
	NormalizeQmClientVersion(pRemoteVersion, aRemote, sizeof(aRemote));
	NormalizeQmClientVersion(pLocalVersion, aLocal, sizeof(aLocal));

	if(aRemote[0] == '\0' || aLocal[0] == '\0')
		return false;

	int Comparison = 0;
	if(!CompareQmClientVersions(aRemote, aLocal, Comparison))
		return false;

	return Comparison > 0;
}
