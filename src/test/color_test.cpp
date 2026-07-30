// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <base/color.h>

#include <game/client/components/qmclient/tee_color_code.h>

#include <gtest/gtest.h>

TEST(Color, HslToRgbToHslConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSLA OldHsl = ColorHSLA(PackedColor);
		ColorRGBA ConvertedRgb = color_cast<ColorRGBA>(OldHsl);
		ColorHSLA NewHsl = color_cast<ColorHSLA>(ConvertedRgb);

		if(OldHsl.s == 0.0f || OldHsl.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else if(OldHsl.l == 0.0f || OldHsl.l == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsl.h, 1.0f), std::fmod(NewHsl.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsl.s, NewHsl.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
	}
}

TEST(Color, RgbToHslToRgbConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorRGBA OldRgb = ColorRGBA(PackedColor);
		ColorHSLA ConvertedHsl = color_cast<ColorHSLA>(OldRgb);
		ColorRGBA NewRgb = color_cast<ColorRGBA>(ConvertedHsl);

		ASSERT_NEAR(OldRgb.r, NewRgb.r, 0.000001f);
		ASSERT_NEAR(OldRgb.g, NewRgb.g, 0.000001f);
		ASSERT_NEAR(OldRgb.b, NewRgb.b, 0.000001f);
	}
}

TEST(Color, HslToHsvToHslConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSLA OldHsl = ColorHSLA(PackedColor);
		ColorHSVA ConvertedHsv = color_cast<ColorHSVA>(OldHsl);
		ColorHSLA NewHsl = color_cast<ColorHSLA>(ConvertedHsv);

		if(OldHsl.s == 0.0f || OldHsl.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else if(OldHsl.l == 0.0f || OldHsl.l == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsl.h, 1.0f), std::fmod(NewHsl.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsl.s, NewHsl.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsl.l, NewHsl.l);
		}
	}
}

TEST(Color, HsvToHslToHsvConv)
{
	for(unsigned PackedColor = 0; PackedColor <= 0xFFFFFF; PackedColor += 0xA)
	{
		ColorHSVA OldHsv = ColorHSVA(PackedColor);
		ColorHSLA ConvertedHsl = color_cast<ColorHSLA>(OldHsv);
		ColorHSVA NewHsv = color_cast<ColorHSVA>(ConvertedHsl);

		if(OldHsv.s == 0.0f || OldHsv.s == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
		else if(OldHsv.v == 0.0f || OldHsv.v == 1.0f)
		{
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
		else
		{
			ASSERT_NEAR(std::fmod(OldHsv.h, 1.0f), std::fmod(NewHsv.h, 1.0f), 0.001f);
			ASSERT_NEAR(OldHsv.s, NewHsv.s, 0.0001f);
			ASSERT_FLOAT_EQ(OldHsv.v, NewHsv.v);
		}
	}
}

// Any color_cast should keep the same alpha value
TEST(Color, ConvKeepsAlpha)
{
	const int Max = 100;
	for(int i = 0; i <= Max; i++)
	{
		const float Alpha = i / (float)Max;
		ASSERT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorRGBA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSLA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSLA>(ColorHSVA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSVA>(ColorRGBA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
		ASSERT_FLOAT_EQ(color_cast<ColorHSVA>(ColorHSLA(0.1f, 0.2f, 0.3f, Alpha)).a, Alpha);
	}
}

TEST(Color, QmTeeColorCodeAcceptsRgbAndShortRgb)
{
	const std::optional<unsigned> White = QmParseTeeColorCode("#ffffff");
	ASSERT_TRUE(White.has_value());
	EXPECT_STREQ(QmFormatTeeColorCode(*White).data(), "#FFFFFF");

	const std::optional<unsigned> Red = QmParseTeeColorCode("#F00");
	ASSERT_TRUE(Red.has_value());
	EXPECT_STREQ(QmFormatTeeColorCode(*Red).data(), "#FF0000");
}

TEST(Color, QmTeeColorCodeRejectsUnsupportedFormats)
{
	EXPECT_GE(QM_TEE_COLOR_CODE_INPUT_SIZE, sizeof("#FFFFFFFF"));
	EXPECT_FALSE(QmParseTeeColorCode(nullptr).has_value());
	EXPECT_FALSE(QmParseTeeColorCode("FFFFFF").has_value());
	EXPECT_FALSE(QmParseTeeColorCode("$FFFFFF").has_value());
	EXPECT_FALSE(QmParseTeeColorCode("0xFFFFFF").has_value());
	EXPECT_FALSE(QmParseTeeColorCode("#FFFF").has_value());
	EXPECT_FALSE(QmParseTeeColorCode("#FFFFFFFF").has_value());
	EXPECT_FALSE(QmParseTeeColorCode("#GGGGGG").has_value());
}

TEST(Color, QmTeeColorCodeUsesRenderedRgbInsteadOfPackedHslBytes)
{
	EXPECT_STREQ(QmFormatTeeColorCode(65408).data(), "#FF8080");
}

TEST(Color, QmTeeColorCodeClampsToClassicTeeMinimumLightness)
{
	const std::optional<unsigned> Black = QmParseTeeColorCode("#000000");
	ASSERT_TRUE(Black.has_value());
	EXPECT_STREQ(QmFormatTeeColorCode(*Black).data(), "#808080");
}

TEST(Color, QmTeeColorCodeInputsBindClassicBodyAndFeetColors)
{
	const std::string Settings = ReadTestSourceFile("src/game/client/components/menus_settings.cpp");
	const std::string Settings7 = ReadTestSourceFile("src/game/client/components/menus_settings7.cpp");

	EXPECT_NE(Settings.find("static CLineInputBuffered<QM_TEE_COLOR_CODE_INPUT_SIZE> s_aaTeeColorCodeInputs[NUM_DUMMIES][2]"), std::string::npos);
	EXPECT_NE(Settings.find("CLineInput &ColorCodeInput = s_aaTeeColorCodeInputs[m_Dummy][i]"), std::string::npos);
	EXPECT_NE(Settings.find("QmFormatTeeColorCode(*apColors[i])"), std::string::npos);
	EXPECT_NE(Settings.find("if(!ColorCodeInput.IsActive())"), std::string::npos);
	EXPECT_EQ(Settings7.find("QmParseTeeColorCode"), std::string::npos);

	const size_t ParsePosition = Settings.find("QmParseTeeColorCode(ColorCodeInput.GetString())");
	ASSERT_NE(ParsePosition, std::string::npos);
	const size_t AssignmentPosition = Settings.find("*apColors[i] = *Color", ParsePosition);
	const size_t SendPosition = Settings.find("SetNeedSendInfo()", ParsePosition);
	const size_t SlidersPosition = Settings.find("if(RenderHslaScrollbars", ParsePosition);
	ASSERT_NE(AssignmentPosition, std::string::npos);
	ASSERT_NE(SendPosition, std::string::npos);
	ASSERT_NE(SlidersPosition, std::string::npos);
	EXPECT_LT(ParsePosition, AssignmentPosition);
	EXPECT_LT(AssignmentPosition, SendPosition);
	EXPECT_LT(SendPosition, SlidersPosition);
}
