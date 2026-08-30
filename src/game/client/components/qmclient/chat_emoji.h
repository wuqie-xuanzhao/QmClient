#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_CHAT_EMOJI_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_CHAT_EMOJI_H

#include <base/system.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <algorithm>
#include <array>
#include <cstddef>

enum class EQmChatEmoji
{
	NONE = 0,
	LOVE,
	NO,
	OPPOSE,
	AWKWARD,
	KNEEL,
	HEHE,
	INSULT,
	CUTE,
	ANGRY,
	DEAD,
	AGREE,
	SURRENDER,
	SMELL,
	QUESTION,
	SHOCKED,
	SUPPORT,
	COUNT,
};

constexpr std::size_t QM_CHAT_EMOJI_COUNT = static_cast<std::size_t>(EQmChatEmoji::COUNT) - 1;

struct SQmChatEmojiDefinition
{
	const char *m_pText;
	EQmChatEmoji m_Emoji;
	const char *m_pTexturePath;
};

inline constexpr std::array<SQmChatEmojiDefinition, QM_CHAT_EMOJI_COUNT> QM_CHAT_EMOJI_DEFINITIONS = {{
	{":ax", EQmChatEmoji::LOVE, "qmclient/chat_emojis/love.png"},
	{":bx", EQmChatEmoji::NO, "qmclient/chat_emojis/no.png"},
	{":fd", EQmChatEmoji::OPPOSE, "qmclient/chat_emojis/oppose.png"},
	{":gg", EQmChatEmoji::AWKWARD, "qmclient/chat_emojis/awkward.png"},
	{":gx", EQmChatEmoji::KNEEL, "qmclient/chat_emojis/kneel.png"},
	{":hh", EQmChatEmoji::HEHE, "qmclient/chat_emojis/hehe.png"},
	{":mr", EQmChatEmoji::INSULT, "qmclient/chat_emojis/insult.png"},
	{":mm", EQmChatEmoji::CUTE, "qmclient/chat_emojis/cute.png"},
	{":sq", EQmChatEmoji::ANGRY, "qmclient/chat_emojis/angry.png"},
	{":sd", EQmChatEmoji::DEAD, "qmclient/chat_emojis/dead.png"},
	{":ty", EQmChatEmoji::AGREE, "qmclient/chat_emojis/agree.png"},
	{":tx", EQmChatEmoji::SURRENDER, "qmclient/chat_emojis/surrender.png"},
	{":wd", EQmChatEmoji::SMELL, "qmclient/chat_emojis/smell.png"},
	{":wh", EQmChatEmoji::QUESTION, "qmclient/chat_emojis/question.png"},
	{":zj", EQmChatEmoji::SHOCKED, "qmclient/chat_emojis/shocked.png"},
	{":zc", EQmChatEmoji::SUPPORT, "qmclient/chat_emojis/support.png"},
}};

inline EQmChatEmoji QmChatEmojiFromText(const char *pText)
{
	if(pText == nullptr)
		return EQmChatEmoji::NONE;
	for(const SQmChatEmojiDefinition &Definition : QM_CHAT_EMOJI_DEFINITIONS)
	{
		if(str_comp(pText, Definition.m_pText) == 0)
			return Definition.m_Emoji;
	}
	return EQmChatEmoji::NONE;
}

inline const char *QmChatEmojiTexturePath(EQmChatEmoji Emoji)
{
	for(const SQmChatEmojiDefinition &Definition : QM_CHAT_EMOJI_DEFINITIONS)
	{
		if(Definition.m_Emoji == Emoji)
			return Definition.m_pTexturePath;
	}
	return nullptr;
}

constexpr bool QmChatEmojiIsKnown(EQmChatEmoji Emoji)
{
	const std::size_t Value = static_cast<std::size_t>(Emoji);
	return Value > static_cast<std::size_t>(EQmChatEmoji::NONE) && Value < static_cast<std::size_t>(EQmChatEmoji::COUNT);
}

constexpr bool QmChatEmojiShouldRenderImage(EQmChatEmoji Emoji, bool TextureAvailable)
{
	return QmChatEmojiIsKnown(Emoji) && TextureAvailable;
}

constexpr bool QmChatEmojiShouldTranslate(EQmChatEmoji Emoji)
{
	return !QmChatEmojiIsKnown(Emoji);
}

constexpr bool QmChatEmojiShouldLoadTexture(EQmChatEmoji Emoji, bool LoadAttempted)
{
	return QmChatEmojiIsKnown(Emoji) && !LoadAttempted;
}

inline float QmChatEmojiChatDisplaySize(float FontSize)
{
	return std::clamp(FontSize * 3.0f, 18.0f, 30.0f);
}

inline float QmChatEmojiBubbleDisplaySize(float FontSize)
{
	return std::clamp(FontSize * 3.0f, 48.0f, 96.0f);
}

class CQmChatEmoji : public CComponent
{
	mutable std::array<IGraphics::CTextureHandle, QM_CHAT_EMOJI_COUNT> m_aTextures;
	mutable std::array<bool, QM_CHAT_EMOJI_COUNT> m_aLoadAttempted{};

	void EnsureTextureLoaded(EQmChatEmoji Emoji) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnShutdown() override;

	bool CanRender(EQmChatEmoji Emoji) const;
	void Render(EQmChatEmoji Emoji, float X, float Y, float Width, float Height, float Alpha) const;
};

#endif
