#include "swap_countdown_message.h"

#include <base/math.h>
#include <base/str.h>

namespace
{
	bool ExtractLeadingName(const char *pText, const char *pMarker, char *pOut, int OutSize)
	{
		if(OutSize <= 0)
			return false;
		pOut[0] = '\0';
		if(pText == nullptr || pMarker == nullptr)
			return false;

		const char *pMarkerPos = str_find_nocase(pText, pMarker);
		if(pMarkerPos == nullptr || pMarkerPos == pText)
			return false;

		const int NameLength = minimum<int>(pMarkerPos - pText, OutSize - 1);
		str_truncate(pOut, OutSize, pText, NameLength);
		return pOut[0] != '\0';
	}

	bool ExtractWrappedName(const char *pText, const char *pPrefix, const char *pSuffix, char *pOut, int OutSize)
	{
		if(OutSize <= 0)
			return false;
		pOut[0] = '\0';
		if(pText == nullptr || pPrefix == nullptr || pSuffix == nullptr)
			return false;

		const char *pNameStart = str_startswith_nocase(pText, pPrefix);
		const char *pNameEnd = pNameStart == nullptr ? nullptr : str_find_nocase(pNameStart, pSuffix);
		if(pNameStart == nullptr || pNameEnd == nullptr || pNameEnd == pNameStart)
			return false;

		const int NameLength = minimum<int>(pNameEnd - pNameStart, OutSize - 1);
		str_truncate(pOut, OutSize, pNameStart, NameLength);
		return pOut[0] != '\0';
	}
}

bool ParseSwapCountdownMessage(const char *pText, ESwapCountdownMessageAction &Action, ESwapCountdownMessageDirection &Direction, char *pCounterpart, int CounterpartSize)
{
	Action = ESwapCountdownMessageAction::None;
	Direction = ESwapCountdownMessageDirection::Incoming;
	if(CounterpartSize > 0)
		pCounterpart[0] = '\0';

	if(pText == nullptr)
		return false;

	if(str_startswith_nocase(pText, "You have requested to swap with "))
	{
		Action = ESwapCountdownMessageAction::Start;
		Direction = ESwapCountdownMessageDirection::Outgoing;
		return ExtractWrappedName(pText, "You have requested to swap with ", ". Use /cancelswap to cancel the request.", pCounterpart, CounterpartSize);
	}
	if(str_startswith(pText, "你已向 "))
	{
		Action = ESwapCountdownMessageAction::Start;
		Direction = ESwapCountdownMessageDirection::Outgoing;
		return ExtractWrappedName(pText, "你已向 ", " 发出交换请求", pCounterpart, CounterpartSize);
	}
	if(str_find_nocase(pText, "has requested to swap with you"))
	{
		Action = ESwapCountdownMessageAction::Start;
		return ExtractLeadingName(pText, " has requested to swap with you", pCounterpart, CounterpartSize);
	}
	if(str_find_nocase(pText, "请求与你交换位置"))
	{
		Action = ESwapCountdownMessageAction::Start;
		return ExtractLeadingName(pText, " 请求与你交换位置", pCounterpart, CounterpartSize);
	}
	if(str_find_nocase(pText, "has canceled swap with you"))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		return ExtractLeadingName(pText, " has canceled swap with you", pCounterpart, CounterpartSize);
	}
	if(str_startswith_nocase(pText, "You have canceled swap with "))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		Direction = ESwapCountdownMessageDirection::Outgoing;
		return ExtractWrappedName(pText, "You have canceled swap with ", ".", pCounterpart, CounterpartSize);
	}
	if(str_startswith(pText, "你已取消与 "))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		Direction = ESwapCountdownMessageDirection::Outgoing;
		return ExtractWrappedName(pText, "你已取消与 ", " 的交换", pCounterpart, CounterpartSize);
	}
	if(str_find_nocase(pText, "已取消与你的交换"))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		return ExtractLeadingName(pText, " 已取消与你的交换", pCounterpart, CounterpartSize);
	}
	if(str_find_nocase(pText, "has swapped with") || str_find_nocase(pText, "已完成交换"))
	{
		Action = ESwapCountdownMessageAction::Complete;
		return true;
	}

	return false;
}
