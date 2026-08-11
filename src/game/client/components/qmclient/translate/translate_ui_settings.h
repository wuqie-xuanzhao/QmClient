// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_TRANSLATE_TRANSLATE_UI_SETTINGS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_TRANSLATE_TRANSLATE_UI_SETTINGS_H

#include <engine/config.h>

class CMenus;
class CUIRect;

namespace NTranslateUiSettings
{
	constexpr unsigned COLOR_ALPHA_MASK = 0xFF000000u;

	constexpr bool RestoreLegacyColorAlpha(unsigned &Color, const unsigned DefaultColor, const EColorInputAlphaMode InputAlphaMode = EColorInputAlphaMode::PACKED)
	{
		// 这五个配置在启用 CFGFLAG_COLALPHA 前只能持久化 24 位 RGB，最高字节为零。
		// 因此保留显式 alpha 和带非零 alpha 的新十进制 packed 配置，只迁移旧 RGB。
		if(InputAlphaMode == EColorInputAlphaMode::EXPLICIT || (InputAlphaMode == EColorInputAlphaMode::PACKED && (Color & COLOR_ALPHA_MASK) != 0))
			return false;
		Color = (Color & ~COLOR_ALPHA_MASK) | (DefaultColor & COLOR_ALPHA_MASK);
		return true;
	}

	constexpr bool MigrateLegacyColorAlphas(bool &Migrated, unsigned &DisabledColor, unsigned &EnabledColor, unsigned &MenuBackgroundColor, unsigned &SelectedOptionColor, unsigned &NormalOptionColor,
		const unsigned DisabledDefault, const unsigned EnabledDefault, const unsigned MenuBackgroundDefault, const unsigned SelectedOptionDefault, const unsigned NormalOptionDefault,
		const EColorInputAlphaMode DisabledInputAlphaMode = EColorInputAlphaMode::PACKED, const EColorInputAlphaMode EnabledInputAlphaMode = EColorInputAlphaMode::PACKED, const EColorInputAlphaMode MenuBackgroundInputAlphaMode = EColorInputAlphaMode::PACKED, const EColorInputAlphaMode SelectedOptionInputAlphaMode = EColorInputAlphaMode::PACKED, const EColorInputAlphaMode NormalOptionInputAlphaMode = EColorInputAlphaMode::PACKED)
	{
		if(Migrated)
			return false;
		RestoreLegacyColorAlpha(DisabledColor, DisabledDefault, DisabledInputAlphaMode);
		RestoreLegacyColorAlpha(EnabledColor, EnabledDefault, EnabledInputAlphaMode);
		RestoreLegacyColorAlpha(MenuBackgroundColor, MenuBackgroundDefault, MenuBackgroundInputAlphaMode);
		RestoreLegacyColorAlpha(SelectedOptionColor, SelectedOptionDefault, SelectedOptionInputAlphaMode);
		RestoreLegacyColorAlpha(NormalOptionColor, NormalOptionDefault, NormalOptionInputAlphaMode);
		Migrated = true;
		return true;
	}

	void RenderTranslateUiModule(CMenus *pMenus, CUIRect &CardContent, float LineHeight, float BodySize, float LineSpacing);
}

#endif
