#include <game/client/components/qmclient/emoticon_projectile.h>

#include <gtest/gtest.h>

#include <array>

namespace
{
	std::array<unsigned char, 16> OpaquePixel()
	{
		return {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
	}
}

TEST(QmEmoticonProjectile, TransparentMaskDoesNotCollide)
{
	std::array<unsigned char, 16> Pixels{};
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(Pixels.data(), 2, 2);
	EXPECT_FALSE(Mask.Overlaps(vec2(16.0f, 16.0f), 32.0f, 0.0f, [](int, int) { return true; }));
}

TEST(QmEmoticonProjectile, OpaqueMaskCollidesOnlyWithSolidTile)
{
	const auto Pixels = OpaquePixel();
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(Pixels.data(), 2, 2);
	EXPECT_TRUE(Mask.Overlaps(vec2(16.0f, 16.0f), 32.0f, 0.0f, [](int X, int Y) { return X == 0 && Y == 0; }));
	EXPECT_FALSE(Mask.Overlaps(vec2(16.0f, 16.0f), 32.0f, 0.0f, [](int, int) { return false; }));
}

TEST(QmEmoticonProjectile, ProjectileBouncesAndExpires)
{
	const auto Pixels = OpaquePixel();
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(Pixels.data(), 2, 2);
	CEmoticonProjectile Projectile;
	Projectile.Init(vec2(16.0f, 16.0f), vec2(100.0f, 0.0f), 0, 0.25f);
	const vec2 Before = Projectile.m_Pos;
	Projectile.Update((float)CEmoticonProjectile::STEP, Mask, [](int X, int) { return X >= 0; });
	EXPECT_TRUE(Projectile.m_Active);
	EXPECT_EQ(Projectile.m_Pos, Before);
	EXPECT_LT(Projectile.m_Vel.x, 0.0f);
	Projectile.Update(4.0f, Mask, [](int, int) { return false; });
	EXPECT_FALSE(Projectile.m_Active);
}

TEST(QmEmoticonProjectile, PlayerCollisionExcludesOwner)
{
	const auto Pixels = OpaquePixel();
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(Pixels.data(), 2, 2);
	const QmEmoticon::SPlayerBox Boxes[] = {
		{7, vec2(16.0f, 16.0f), 16.0f},
		{8, vec2(16.0f, 16.0f), 16.0f},
	};
	EXPECT_TRUE(QmEmoticon::OverlapsPlayerBoxes(Mask, vec2(16.0f, 16.0f), 32.0f, 0.0f, 7, Boxes, std::size(Boxes)));
	EXPECT_FALSE(QmEmoticon::OverlapsPlayerBoxes(Mask, vec2(16.0f, 16.0f), 32.0f, 0.0f, 7, Boxes, 1));
}
