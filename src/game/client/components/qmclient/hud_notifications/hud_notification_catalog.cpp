#include "hud_notification_catalog.h"

#include <iterator>

namespace QmHudNotifications
{
	namespace
	{
		const SMessageMetadata s_aMessageMetadata[] = {
			{EServerMessageRoute::None, EServerMessageClass::None, EServerMessageDomain::Unknown, false, ""},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "You will receive whispers"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "You will not receive any further whispers"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "You will now see all tees on this server, no matter the distance"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "You will no longer see all tees on this server"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::SwapRescue, false, "Rescue is not enabled on this server and you're not in a team with /practice turned on. Note that you can't earn a rank with practice enabled."},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "Unknown emote. Use /emote to see available emotes."},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "Your timeout code has been set. 0.7 clients can not reclaim their tees on timeout; however, a 0.6 client can claim your tee"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Team, false, "Team save already in progress"},
		};

		const SMessageMetadata s_aDynamicMessageMetadata[] = {
			{EServerMessageRoute::None, EServerMessageClass::None, EServerMessageDomain::Unknown, false, ""},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Team, false, "'%s' joined team %s"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::SwapRescue, false, "You have requested to swap with %s. Use /cancelswap to cancel the request."},
		};

		static_assert(std::size(s_aMessageMetadata) == static_cast<size_t>(EMessageKey::Count), "EMessageKey metadata table out of sync");
		static_assert(std::size(s_aDynamicMessageMetadata) == static_cast<size_t>(EDynamicMessageKey::Count), "EDynamicMessageKey metadata table out of sync");
	}

	const SMessageMetadata *FindMessageMetadata(EMessageKey Key)
	{
		const int Index = static_cast<int>(Key);
		if(Index < 0 || Index >= static_cast<int>(std::size(s_aMessageMetadata)))
			return nullptr;
		return &s_aMessageMetadata[Index];
	}

	const char *CanonicalMessageText(EMessageKey Key)
	{
		const auto *pMeta = FindMessageMetadata(Key);
		return pMeta != nullptr ? pMeta->m_pCanonicalText : "";
	}

	const SMessageMetadata *FindMessageMetadata(EDynamicMessageKey Key)
	{
		const int Index = static_cast<int>(Key);
		if(Index < 0 || Index >= static_cast<int>(std::size(s_aDynamicMessageMetadata)))
			return nullptr;
		return &s_aDynamicMessageMetadata[Index];
	}
} // namespace QmHudNotifications
