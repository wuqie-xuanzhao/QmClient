#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_LAYOUT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NAMEPLATE_LAYOUT_H

#include <algorithm>
#include <array>
#include <cstddef>

enum class ENameplateCoreRow
{
	NAME,
	CLAN,
	HOOK,
	COORDS,
	KEYS,
	NUM_ROWS
};

static constexpr size_t kNameplateCoreRowCount = static_cast<size_t>(ENameplateCoreRow::NUM_ROWS);

inline std::array<float, kNameplateCoreRowCount> QmNameplateCoreRowBaselines(const std::array<float, kNameplateCoreRowCount> &aHeights)
{
	std::array<float, kNameplateCoreRowCount> aBaselines{};
	float Position = 0.0f;
	for(size_t RowIndex = 0; RowIndex < aHeights.size(); ++RowIndex)
	{
		aBaselines[RowIndex] = Position;
		Position -= aHeights[RowIndex];
	}
	// 方向键只使用强弱钩默认大小的间距（24 + 5），不随其开关或大小变化。
	aBaselines[static_cast<size_t>(ENameplateCoreRow::KEYS)] = aBaselines[static_cast<size_t>(ENameplateCoreRow::HOOK)] - 29.0f - aHeights[static_cast<size_t>(ENameplateCoreRow::COORDS)];
	return aBaselines;
}

inline float QmNameplatePreviewContentSpan(std::array<float, kNameplateCoreRowCount> aHeights)
{
	// 为两项保留最大尺寸空间：强弱钩为 38 + 5，方向键为 38 * 1.5。
	// 开关和缩放只改变对应图标，不能带动预览框及另一项整体移动。
	aHeights[static_cast<size_t>(ENameplateCoreRow::HOOK)] = 43.0f;
	aHeights[static_cast<size_t>(ENameplateCoreRow::KEYS)] = 57.0f;
	const auto aBaselines = QmNameplateCoreRowBaselines(aHeights);
	float Span = 0.0f;
	for(size_t RowIndex = 0; RowIndex < aHeights.size(); ++RowIndex)
		Span = std::max(Span, -aBaselines[RowIndex] + aHeights[RowIndex]);
	return Span;
}

#endif
