// 请抬头享受阳光｜日子很好 我很我---------致咩子
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
		int aLeftParts[4] = {};
		int aRightParts[4] = {};
		int LeftPartCount = 0;
		int RightPartCount = 0;
		OutComparison = 0;

		while(pLeftCursor[0] != '\0' && LeftPartCount < 4)
		{
			if(!ReadNextVersionPart(pLeftCursor, aLeftParts[LeftPartCount]))
				return false;
			++LeftPartCount;
		}
		while(pRightCursor[0] != '\0' && RightPartCount < 4)
		{
			if(!ReadNextVersionPart(pRightCursor, aRightParts[RightPartCount]))
				return false;
			++RightPartCount;
		}
		if(pLeftCursor[0] != '\0' || pRightCursor[0] != '\0' || LeftPartCount != RightPartCount)
			return false;

		for(int Index = 0; Index < LeftPartCount; ++Index)
		{
			if(aLeftParts[Index] != aRightParts[Index])
			{
				OutComparison = aLeftParts[Index] < aRightParts[Index] ? -1 : 1;
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
