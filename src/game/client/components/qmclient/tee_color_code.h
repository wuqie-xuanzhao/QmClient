// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_TEE_COLOR_CODE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_TEE_COLOR_CODE_H

#include <base/color.h>
#include <base/system.h>

#include <array>
#include <cstddef>
#include <optional>

constexpr std::size_t QM_TEE_COLOR_CODE_INPUT_SIZE = 16;

inline std::optional<unsigned> QmParseTeeColorCode(const char *pColorCode)
{
	if(pColorCode == nullptr || pColorCode[0] != '#')
		return std::nullopt;

	const int Length = str_length(pColorCode + 1);
	if(Length != 3 && Length != 6)
		return std::nullopt;

	const std::optional<ColorRGBA> RgbColor = color_parse<ColorRGBA>(pColorCode + 1);
	if(!RgbColor.has_value())
		return std::nullopt;

	return color_cast<ColorHSLA>(*RgbColor).Pack(ColorHSLA::DARKEST_LGT, false);
}

inline std::array<char, 8> QmFormatTeeColorCode(unsigned PackedColor)
{
	const ColorRGBA RgbColor = color_cast<ColorRGBA>(ColorHSLA(PackedColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
	std::array<char, 8> aColorCode;
	str_format(aColorCode.data(), aColorCode.size(), "#%06X", RgbColor.Pack(false));
	return aColorCode;
}

#endif
