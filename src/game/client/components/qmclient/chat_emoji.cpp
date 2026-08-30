#include "chat_emoji.h"

#include <engine/storage.h>

void CQmChatEmoji::EnsureTextureLoaded(EQmChatEmoji Emoji) const
{
	if(!QmChatEmojiIsKnown(Emoji))
		return;
	const std::size_t TextureIndex = static_cast<std::size_t>(Emoji) - 1;
	if(!QmChatEmojiShouldLoadTexture(Emoji, m_aLoadAttempted[TextureIndex]))
		return;

	// 原图较大，首次遇到对应表情时再上传纹理，避免启动即常驻整套资源。
	m_aLoadAttempted[TextureIndex] = true;
	const char *pTexturePath = QmChatEmojiTexturePath(Emoji);
	if(pTexturePath != nullptr)
		m_aTextures[TextureIndex] = Graphics()->LoadTexture(pTexturePath, IStorage::TYPE_ALL);
}

void CQmChatEmoji::OnShutdown()
{
	for(IGraphics::CTextureHandle &Texture : m_aTextures)
	{
		if(Texture.IsValid())
			Graphics()->UnloadTexture(&Texture);
	}
	m_aLoadAttempted.fill(false);
}

bool CQmChatEmoji::CanRender(EQmChatEmoji Emoji) const
{
	if(!QmChatEmojiIsKnown(Emoji))
		return false;
	EnsureTextureLoaded(Emoji);
	const std::size_t TextureIndex = static_cast<std::size_t>(Emoji) - 1;
	return QmChatEmojiShouldRenderImage(Emoji, m_aTextures[TextureIndex].IsValid());
}

void CQmChatEmoji::Render(EQmChatEmoji Emoji, float X, float Y, float Width, float Height, float Alpha) const
{
	if(!CanRender(Emoji) || Width <= 0.0f || Height <= 0.0f || Alpha <= 0.0f)
		return;

	const std::size_t TextureIndex = static_cast<std::size_t>(Emoji) - 1;
	Graphics()->TextureSet(m_aTextures[TextureIndex]);
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, std::clamp(Alpha, 0.0f, 1.0f)));
	IGraphics::CQuadItem Quad(X + Width * 0.5f, Y + Height * 0.5f, Width, Height);
	Graphics()->QuadsDraw(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->TextureClear();
	Graphics()->SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
}
