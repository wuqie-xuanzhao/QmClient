/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information.                */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_JUMP_HINT_UTILS_H
#define GAME_CLIENT_COMPONENTS_JUMP_HINT_UTILS_H

#include <base/str.h>

#include <cstddef>

// Shared helpers for the position jump hint HUD element (CHud::RenderJumpHint
// and CHudEditor's text editor). Kept in a tiny dedicated module so the
// default text, legacy color constants and the \n escape codec live in one
// place instead of being duplicated across hud.cpp and hud_editor.cpp.

// Default speed-table text. Uses literal "\n" (backslash + n) as in-config
// newline encoding; callers decode it via DecodeEscapedNewlines before draw.
constexpr const char *JUMP_HINT_DEFAULT_TEXT = "三格边缘跳:\\n左起跳: .34|.31|.16\\n左二段跳: .41|.28|.25|.13\\n右起跳: .63|.66|.81\\n右二段跳: .56|.69|.72|.84";

// 首次升级时关闭三跳提示，仅替换原封不动的旧英文默认文案。
// 持久化迁移标记，保留用户随后重新开启或编辑文本的选择。
inline void MigrateJumpHintDefaults(int &Migrated, int &Enabled, char *pText, size_t TextSize)
{
	if(Migrated)
		return;
	constexpr const char *pLegacyText = "3 Tiles Edge Jump:\\nLeft Jump: .34|.31|.16\\nLeft Double Jump: .41|.28|.25|.13\\nRight Jump: .63|.66|.81\\nRight Double Jump: .56|.69|.72|.84";
	if(str_comp(pText, pLegacyText) == 0)
		str_copy(pText, JUMP_HINT_DEFAULT_TEXT, TextSize);
	Enabled = 0;
	Migrated = 1;
}

// Default text color (HSLA packed int, as used by MACRO_CONFIG_COL).
constexpr int JUMP_HINT_DEFAULT_COLOR = 255;
// Legacy sentinel value that used to mean "black"; mapped back to the default
// color to preserve the pre-migration visual for affected configs.
constexpr int JUMP_HINT_LEGACY_BLACK_COLOR = 256;

// Turn literal "\n" (backslash + n) sequences into real '\n'.
// Always NUL-terminates the output. No-op when OutputSize is 0.
void DecodeEscapedNewlines(const char *pInput, char *pOutput, size_t OutputSize);

// Turn real '\n' into literal "\n" (backslash + n); '\r' is dropped.
// Always NUL-terminates the output. No-op when OutputSize is 0.
void EncodeEscapedNewlines(const char *pInput, char *pOutput, size_t OutputSize);

#endif
