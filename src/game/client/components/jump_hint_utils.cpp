/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.                */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "jump_hint_utils.h"

void DecodeEscapedNewlines(const char *pInput, char *pOutput, size_t OutputSize)
{
	if(OutputSize == 0)
		return;

	size_t OutPos = 0;
	for(size_t InPos = 0; pInput != nullptr && pInput[InPos] != '\0' && OutPos + 1 < OutputSize; ++InPos)
	{
		if(pInput[InPos] == '\\' && pInput[InPos + 1] == 'n')
		{
			pOutput[OutPos++] = '\n';
			++InPos;
		}
		else
		{
			pOutput[OutPos++] = pInput[InPos];
		}
	}
	pOutput[OutPos] = '\0';
}

void EncodeEscapedNewlines(const char *pInput, char *pOutput, size_t OutputSize)
{
	if(OutputSize == 0)
		return;

	size_t OutPos = 0;
	for(size_t InPos = 0; pInput != nullptr && pInput[InPos] != '\0' && OutPos + 1 < OutputSize; ++InPos)
	{
		if(pInput[InPos] == '\r')
			continue;
		if(pInput[InPos] == '\n')
		{
			if(OutPos + 2 >= OutputSize)
				break;
			pOutput[OutPos++] = '\\';
			pOutput[OutPos++] = 'n';
		}
		else
		{
			pOutput[OutPos++] = pInput[InPos];
		}
	}
	pOutput[OutPos] = '\0';
}
