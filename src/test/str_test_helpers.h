#pragma once

#include <gtest/gtest.h>

#include <base/str.h>

using TStringArgumentFunction = void (*)(char *pStr);

template<TStringArgumentFunction Func>
static void TestInplace(const char *pInput, const char *pOutput)
{
	char aBuf[512];
	str_copy(aBuf, pInput);
	Func(aBuf);
	EXPECT_STREQ(aBuf, pOutput);
}
