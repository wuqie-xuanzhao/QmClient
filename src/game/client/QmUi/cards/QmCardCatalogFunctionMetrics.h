#ifndef GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGFUNCTIONMETRICS_H
#define GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGFUNCTIONMETRICS_H

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/localization.h>
#include <engine/textrender.h>

#include <game/localization.h>

#include <algorithm>
#include <array>
#include <cstddef>

// 函数分类卡片的行数/高度口径：定义在卡片目录下，菜单渲染层（menus_qmclient.cpp）
// 与卡片模块（QmCardCatalogFunction.cpp）共用同一份计算，避免两处行数漂移。
namespace qm_card_catalog
{
	// 「梦境特性」卡片的普通开关行：普通行来自这张表，另加「新版 IME」与「赞助提醒」两个特殊行。
	// 渲染（menus_qmclient.cpp）与卡片高度测量都取自同一张表，
	// 避免增删开关后高度测量与实际行数再次错位而留下空白。
	struct SQmMiniFeatureRow
	{
		const void *m_pId;
		const char *m_pTextId;
		int *m_pValue;
	};

	inline const std::array<SQmMiniFeatureRow, 15> &QmMiniFeatureRows()
	{
		// 行文案以 Localizable 标注：表在卡片目录里，渲染侧只做 Localize(m_pTextId)，
		// 不加标注翻译提取脚本就看不到这些 source key，语言文件会整行退回英文。
		// 本地差异：远程此表含 g_Config.m_QmProcessHighPriority（"High process priority"），
		// 该项为本地既定不吸收（本地合同测试断言其不存在），故整行删除且表长由 16 改为 15。
		static const std::array<SQmMiniFeatureRow, 15> s_aRows = {{
			{&g_Config.m_QmClientShowBadge, Localizable("Show Qm badge"), &g_Config.m_QmClientShowBadge},
			{&g_Config.m_QmAutoUpdate, Localizable("Automatic updates"), &g_Config.m_QmAutoUpdate},
			{&g_Config.m_QmShowOutdatedVersionWarning, Localizable("Show outdated version warning"), &g_Config.m_QmShowOutdatedVersionWarning},
			{&g_Config.m_QmBetterScoreboard, Localizable("Better scoreboard"), &g_Config.m_QmBetterScoreboard},
			{&g_Config.m_QmScoreboardPoints, Localizable("Scoreboard point check"), &g_Config.m_QmScoreboardPoints},
			{&g_Config.m_QmScoreboardOnDeath, Localizable("Show scoreboard after death"), &g_Config.m_QmScoreboardOnDeath},
			{&g_Config.m_QmHideJoinServerInfo, Localizable("Hide server information on join"), &g_Config.m_QmHideJoinServerInfo},
			{&g_Config.m_QmMessageMerge, Localizable("Message merging"), &g_Config.m_QmMessageMerge},
			{&g_Config.m_QmNewUi, Localizable("New UI"), &g_Config.m_QmNewUi},
			{&g_Config.m_QmShortServerNames, Localizable("Short server names"), &g_Config.m_QmShortServerNames},
			{&g_Config.m_QmImeAutoManage, Localizable("Auto manage IME while typing"), &g_Config.m_QmImeAutoManage},
			{&g_Config.m_QmRepeatEnabled, Localizable("Enable repeat"), &g_Config.m_QmRepeatEnabled},
			{&g_Config.m_QmRandomEmoteOnHit, Localizable("Random emoticon"), &g_Config.m_QmRandomEmoteOnHit},
			{&g_Config.m_QmComboPopup, Localizable("Combo"), &g_Config.m_QmComboPopup},
			{&g_Config.m_QmSayNoPop, Localizable("Hide input emoticon"), &g_Config.m_QmSayNoPop},
		}};
		return s_aRows;
	}

	// 「新版 IME」需要独立文案、「赞助提醒」需要关闭时的灵动岛提示，两者单独渲染，
	// 因而卡片总行数为普通行加这两行。
	inline constexpr size_t QmMiniFeatureSpecialRowCount = 2;

	// 多行输入框（词条过滤 / 关键词回复）按文本换行数估算高度。
	inline float CalcQiaFenInputHeight(ITextRender *pTextRender, const char *pText, const float Width, const float TextFontSize, const float LineSpacing, const float MinHeight)
	{
		if(pText == nullptr || pText[0] == '\0')
			return MinHeight;
		const float Height = pTextRender->TextBoundingBox(TextFontSize, pText, -1, std::max(1.0f, Width)).m_H;
		return std::max(MinHeight, Height + LineSpacing);
	}

	// 地图上传卡片的三行说明文案与其换行高度。
	inline std::array<const char *, 3> QmMapUploadInstructions()
	{
		return {Localize("Upload .map files. Server and admin passwords: s."),
			Localize("Press F2, log in, then use: change_map map_name"),
			Localize("After uploading again, use: reload or hot_reload")};
	}

	inline float QmMapUploadHelpLineHeight(ITextRender *pTextRender, const char *pText, const float Width, const float BodySize, const float LineHeight)
	{
		return std::max(LineHeight, pTextRender->TextBoundingBox(BodySize, pText, -1, std::max(1.0f, Width)).m_H + BodySize * 0.25f);
	}
} // namespace qm_card_catalog

#endif // GAME_CLIENT_QMUI_CARDS_QMCARDCATALOGFUNCTIONMETRICS_H
