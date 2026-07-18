/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_SETTINGSPAGELAYOUT_H
#define GAME_CLIENT_QMUI_SETTINGSPAGELAYOUT_H

#include "UiTokens.h"

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cstdint>

struct SSettingsPageLayoutFrame
{
	CUIRect m_PageRect;
	CUIRect m_UnreservedScrollViewport;
	CUIRect m_ScrollViewport;
	CUIRect m_ContentViewport;
	CUIRect m_SubTabRect;
	CUIRect m_aColumns[2];
	float m_CardGap = 0.0f;
	bool m_TwoColumns = false;
};

struct SSettingsShellLayoutFrame
{
	CUIRect m_ShellRect;
	CUIRect m_ContentPanelRect;
	CUIRect m_ContentRect;
	CUIRect m_TabBarRect;
	CUIRect m_RestartBarRect;
	CUIRect m_ScrollViewport;
	CUIRect m_aColumns[2];
	float m_TabBarWidth = 0.0f;
	float m_UiScale = 1.0f;
	float m_CardGap = 0.0f;
	bool m_TwoColumns = false;
};

struct SSettingsSubTabLayoutFrame
{
	CUIRect m_TabBarRect;
	CUIRect m_ContentRect;
};

struct SSettingsContentMetrics
{
	float m_UiScale = 1.0f;
	float m_LineHeight = 20.0f;
	float m_BodySize = 12.0f;
	float m_SmallSize = 10.0f;
	float m_HeadlineSize = 14.0f;
	float m_LineSpacing = 5.0f;
	float m_RowStep = 25.0f;
	float m_InputHeight = 20.0f;
	float m_ButtonHeight = 20.0f;
	float m_SectionGap = 10.0f;
	float m_BadgeHeight = 16.0f;
	float m_ListRowHeight = 20.0f;
	float m_LabelWidth = 120.0f;
	float m_CardGap = ui_token::settings::CARD_GAP;
};

inline uint64_t ResolveSettingsCardDefinitionsRevision(uint64_t DisplayCycle, uint64_t TextGeneration, float ContentWidth, uint64_t LayoutRevision = 0)
{
	const uint64_t QuantizedWidth = (uint64_t)std::max(0, (int)(ContentWidth * 100.0f + 0.5f));
	uint64_t Revision = 1469598103934665603ULL;
	const auto Mix = [&Revision](uint64_t Value) {
		Revision ^= Value;
		Revision *= 1099511628211ULL;
	};
	Mix(DisplayCycle);
	Mix(TextGeneration);
	Mix(QuantizedWidth);
	Mix(LayoutRevision);
	return Revision;
}

struct SSettingsConfigRowMetrics
{
	float m_ControlLineHeight = 0.0f;
	float m_ControlBlockHeight = 0.0f;
	float m_HelpGap = 0.0f;
	float m_RowHeight = 0.0f;
};

struct SSettingsRadioRowLayout
{
	CUIRect m_LabelRect;
	CUIRect m_ButtonsRect;
	float m_Height = 0.0f;
	bool m_Stacked = false;
};

inline float ResolveSettingsUiScale(const float ContentWidth)
{
	// 在窄窗口保留紧凑基线，并平滑过渡到 800px 处的标准缩放，避免跨阈值跳变。
	const float CompactBaseline = 0.78f + std::clamp((ContentWidth - 680.0f) / 120.0f, 0.0f, 1.0f) * 0.07f;
	return std::clamp(std::max(ContentWidth / 1000.0f, CompactBaseline), 0.78f, 1.0f);
}

inline SSettingsContentMetrics ResolveSettingsContentMetrics(const float ContentWidth)
{
	SSettingsContentMetrics Metrics;
	Metrics.m_UiScale = ResolveSettingsUiScale(ContentWidth);
	Metrics.m_LineHeight = std::clamp(ui_token::settings::ROW_HEIGHT * Metrics.m_UiScale, 16.0f, ui_token::settings::ROW_HEIGHT);
	Metrics.m_BodySize = std::clamp(ui_token::font::BODY * Metrics.m_UiScale, 10.0f, ui_token::font::BODY);
	Metrics.m_SmallSize = std::clamp(ui_token::font::SMALL * Metrics.m_UiScale, 9.0f, ui_token::font::SMALL);
	Metrics.m_HeadlineSize = std::clamp(ui_token::font::HEADLINE * Metrics.m_UiScale, 12.0f, ui_token::font::HEADLINE);
	Metrics.m_LineSpacing = std::clamp(ui_token::settings::ROW_GAP * Metrics.m_UiScale, 3.0f, ui_token::settings::ROW_GAP);
	Metrics.m_RowStep = Metrics.m_LineHeight + Metrics.m_LineSpacing;
	Metrics.m_InputHeight = Metrics.m_LineHeight;
	Metrics.m_ButtonHeight = Metrics.m_LineHeight;
	Metrics.m_SectionGap = 2.0f * Metrics.m_LineSpacing;
	Metrics.m_BadgeHeight = std::max(12.0f, Metrics.m_LineHeight - Metrics.m_LineSpacing);
	Metrics.m_ListRowHeight = Metrics.m_RowStep;
	Metrics.m_CardGap = ui_token::settings::CARD_GAP * Metrics.m_UiScale;
	const float MinimumControlWidth = 160.0f * Metrics.m_UiScale;
	const float Gap = 8.0f * Metrics.m_UiScale;
	const float PreferredLabelWidth = ContentWidth * 0.32f;
	Metrics.m_LabelWidth = std::clamp(PreferredLabelWidth, 96.0f * Metrics.m_UiScale, std::max(96.0f * Metrics.m_UiScale, ContentWidth - MinimumControlWidth - Gap));
	return Metrics;
}

inline float ResolveSettingsCardLabelWidth(const float CardContentWidth, const SSettingsContentMetrics &Metrics)
{
	const float Scale = Metrics.m_UiScale > 0.0f ? Metrics.m_UiScale : 1.0f;
	const float MinimumControlWidth = 160.0f * Scale;
	const float Gap = 8.0f * Scale;
	const float MinimumLabelWidth = 96.0f * Scale;
	const float PreferredLabelWidth = std::max(0.0f, CardContentWidth) * 0.32f;
	return std::clamp(PreferredLabelWidth, MinimumLabelWidth, std::max(MinimumLabelWidth, CardContentWidth - MinimumControlWidth - Gap));
}

inline SSettingsConfigRowMetrics ResolveSettingsConfigRowMetrics(const bool Compact, const bool Stacked, const float LineHeight, const float RowSpacing, const float HelpHeight, const float ColorControlHeight, const float VerticalPadding)
{
	SSettingsConfigRowMetrics Metrics;
	Metrics.m_ControlLineHeight = std::max(std::max(0.0f, LineHeight), std::max(0.0f, ColorControlHeight));
	Metrics.m_ControlBlockHeight = Stacked ? std::max(0.0f, LineHeight) + std::max(0.0f, RowSpacing) + Metrics.m_ControlLineHeight : Metrics.m_ControlLineHeight;
	Metrics.m_HelpGap = Compact ? 0.0f : 2.0f;
	Metrics.m_RowHeight = std::max(0.0f, VerticalPadding) * 2.0f + Metrics.m_ControlBlockHeight;
	if(!Compact)
		Metrics.m_RowHeight += Metrics.m_HelpGap + std::max(0.0f, HelpHeight);
	return Metrics;
}

inline float ResolveSettingsGridHeight(const int ItemCount, const int ItemsPerRow, const float RowHeight, const float RowSpacing)
{
	if(ItemCount <= 0 || ItemsPerRow <= 0)
		return 0.0f;
	const int Rows = (ItemCount + ItemsPerRow - 1) / ItemsPerRow;
	return Rows * std::max(0.0f, RowHeight) + std::max(0, Rows - 1) * std::max(0.0f, RowSpacing);
}

inline float ResolveSettingsRowsHeight(const int RowCount, const float RowHeight, const float RowSpacing)
{
	if(RowCount <= 0)
		return 0.0f;
	return RowCount * std::max(0.0f, RowHeight) + std::max(0, RowCount - 1) * std::max(0.0f, RowSpacing);
}

inline float ResolveSettingsHslaRowsHeight(const SSettingsContentMetrics &Metrics, const bool Alpha)
{
	return ResolveSettingsRowsHeight(Alpha ? 4 : 3, Metrics.m_LineHeight, Metrics.m_LineSpacing);
}

struct SSettingsTeeCustomColorsLayout
{
	CUIRect m_BodyGroup{};
	CUIRect m_BodyTitle{};
	CUIRect m_BodyControls{};
	CUIRect m_FeetGroup{};
	CUIRect m_FeetTitle{};
	CUIRect m_FeetControls{};
	float m_Height = 0.0f;
};

inline SSettingsTeeCustomColorsLayout ResolveSettingsTeeCustomColorsLayout(const CUIRect &View, const bool Enabled, const SSettingsContentMetrics &Metrics)
{
	SSettingsTeeCustomColorsLayout Layout;
	const float Spacing = std::max(0.0f, Metrics.m_LineSpacing);
	Layout.m_Height = Spacing * 2.0f;
	if(!Enabled)
		return Layout;

	const float ControlsHeight = ResolveSettingsHslaRowsHeight(Metrics, false);
	const float GroupHeight = Spacing * 2.0f + Metrics.m_LineHeight + Spacing + ControlsHeight;
	Layout.m_BodyGroup = {View.x, View.y + Spacing, View.w, GroupHeight};
	Layout.m_FeetGroup = {View.x, Layout.m_BodyGroup.y + Layout.m_BodyGroup.h + Metrics.m_SectionGap, View.w, GroupHeight};
	const auto ResolveGroup = [&](const CUIRect &Group, CUIRect &Title, CUIRect &Controls) {
		CUIRect Inner;
		Group.Margin(Spacing, &Inner);
		Inner.HSplitTop(Metrics.m_LineHeight, &Title, &Inner);
		Inner.HSplitTop(Spacing, nullptr, &Inner);
		Inner.HSplitTop(ControlsHeight, &Controls, nullptr);
	};
	ResolveGroup(Layout.m_BodyGroup, Layout.m_BodyTitle, Layout.m_BodyControls);
	ResolveGroup(Layout.m_FeetGroup, Layout.m_FeetTitle, Layout.m_FeetControls);
	Layout.m_Height = Layout.m_FeetGroup.y + Layout.m_FeetGroup.h - View.y + Spacing;
	return Layout;
}

struct SSettingsColorRowLayout
{
	CUIRect m_RowRect{};
	CUIRect m_LabelRect{};
	CUIRect m_ColorButtonRect{};
	CUIRect m_ResetButtonRect{};
	float m_ConsumedHeight = 0.0f;
};

inline SSettingsColorRowLayout ResolveSettingsColorRowLayout(const CUIRect &View, const SSettingsContentMetrics &Metrics, const bool ReserveCheckboxIndent)
{
	SSettingsColorRowLayout Layout;
	const float RowHeight = std::max(0.0f, Metrics.m_ButtonHeight);
	const float ColorButtonWidth = std::min(RowHeight, std::max(0.0f, View.w * 0.20f));
	const float StandardHorizontalGap = std::clamp(ui_token::settings::ROW_GAP * Metrics.m_UiScale, 3.0f, ui_token::settings::ROW_GAP);
	const float Gap = std::min(StandardHorizontalGap, std::max(0.0f, (View.w - ColorButtonWidth) * 0.10f));
	Layout.m_RowRect = {View.x, View.y, View.w, RowHeight};
	Layout.m_ConsumedHeight = RowHeight + std::max(0.0f, Metrics.m_LineSpacing);

	const float MaximumResetWidth = std::max(0.0f, View.w - ColorButtonWidth - Gap * 2.0f);
	const float MinimumResetWidth = std::min(60.0f * Metrics.m_UiScale, MaximumResetWidth);
	const float PreferredResetWidth = 72.0f * Metrics.m_UiScale;
	const float ResetWidth = std::clamp(std::min(PreferredResetWidth, View.w * 0.30f), MinimumResetWidth, MaximumResetWidth);
	CUIRect Remaining = Layout.m_RowRect;
	Remaining.VSplitRight(ResetWidth, &Remaining, &Layout.m_ResetButtonRect);
	Remaining.VSplitRight(Gap, &Remaining, nullptr);
	Remaining.VSplitRight(ColorButtonWidth, &Remaining, &Layout.m_ColorButtonRect);
	Remaining.VSplitRight(Gap, &Layout.m_LabelRect, nullptr);
	if(ReserveCheckboxIndent)
	{
		const float Indent = std::min(Layout.m_LabelRect.w, RowHeight + Gap);
		Layout.m_LabelRect.VSplitLeft(Indent, nullptr, &Layout.m_LabelRect);
	}
	return Layout;
}

inline SSettingsRadioRowLayout ResolveSettingsRadioRowLayout(const CUIRect &View, const int OptionCount, const SSettingsContentMetrics &Metrics)
{
	SSettingsRadioRowLayout Layout;
	if(OptionCount <= 0 || View.w <= 0.0f)
		return Layout;

	const float Gap = Metrics.m_LineSpacing;
	const float LabelWidth = ResolveSettingsCardLabelWidth(View.w, Metrics);
	const float MinimumOptionWidth = std::max(48.0f * Metrics.m_UiScale, Metrics.m_ButtonHeight * 2.4f);
	const float RequiredInlineWidth = LabelWidth + Gap + MinimumOptionWidth * OptionCount;
	Layout.m_Stacked = View.w < RequiredInlineWidth;
	if(Layout.m_Stacked)
	{
		Layout.m_LabelRect = {View.x, View.y, View.w, Metrics.m_LineHeight};
		Layout.m_ButtonsRect = {View.x, View.y + Metrics.m_LineHeight + Gap, View.w, Metrics.m_ButtonHeight};
		Layout.m_Height = Metrics.m_LineHeight + Gap + Metrics.m_ButtonHeight;
	}
	else
	{
		Layout.m_LabelRect = {View.x, View.y, LabelWidth, Metrics.m_LineHeight};
		Layout.m_ButtonsRect = {View.x + LabelWidth + Gap, View.y, std::max(0.0f, View.w - LabelWidth - Gap), Metrics.m_ButtonHeight};
		Layout.m_Height = std::max(Metrics.m_LineHeight, Metrics.m_ButtonHeight);
	}
	return Layout;
}

inline float ResolveSettingsInlineRowMinimumWidth(const float FixedControlsWidth, const float Gap, const int GapCount)
{
	return std::max(0.0f, FixedControlsWidth) + std::max(0.0f, Gap) * std::max(0, GapCount);
}

inline float ResolveSettingsCheckboxFontSize(const float BodySize, const float RequestedFontSize, const float RowHeight, const float BoxHeight, const float FontmodHeight)
{
	// 显式字号属于页面 metrics 契约，使用整行高度限幅；默认路径保留按图标内框限幅的旧行为。
	const float AvailableHeight = RequestedFontSize > 0.0f ? RowHeight : BoxHeight;
	return std::min(std::max(0.0f, BodySize), std::max(0.0f, AvailableHeight) * std::max(0.0f, FontmodHeight));
}

inline float ResolveAppearanceChatMessagesHeight(const SSettingsContentMetrics &Metrics)
{
	constexpr int MessageGradientCount = 6;
	const float MessageGradientHeight = 2.0f * Metrics.m_LineHeight + 2.0f * Metrics.m_LineSpacing;
	const float ColorPickerHeight = Metrics.m_LineHeight + Metrics.m_LineSpacing;
	return MessageGradientCount * MessageGradientHeight + ColorPickerHeight;
}

inline float ResolveQmHudCoordsHeight(const SSettingsContentMetrics &Metrics)
{
	// 六个复选框、一个数值输入和一个颜色选择器都会消费一行及其底部间距。
	return 8.0f * Metrics.m_RowStep;
}

inline float ResolveQmHudNotificationsHeight(const SSettingsContentMetrics &Metrics, const bool Advanced, const bool CategoryFilters)
{
	// 基础区域：两个开关、两个数值输入和高级选项开关。
	float Height = 5.0f * Metrics.m_RowStep;
	if(!Advanced)
		return Height;

	// 高级区域固定消费十行；分类过滤启用后再显示四个分类开关。
	Height += (10.0f + (CategoryFilters ? 4.0f : 0.0f)) * Metrics.m_RowStep;
	// 说明文字使用 Small 语义，并保留与其他行相同的安全间距。
	return Height + Metrics.m_SmallSize + Metrics.m_LineSpacing;
}

inline float ResolveQmHudPlayerStatsHeight(const SSettingsContentMetrics &Metrics, const bool MapProgress, const bool EmbeddedProgress)
{
	if(!MapProgress)
		return 3.0f * Metrics.m_RowStep;
	return (EmbeddedProgress ? 5.0f : 10.0f) * Metrics.m_RowStep;
}

inline float ResolveQmHudInputOverlayHeight(const SSettingsContentMetrics &Metrics, const bool Enabled)
{
	if(!Enabled)
		return Metrics.m_RowStep;
	return 5.0f * Metrics.m_RowStep + 2.0f * (Metrics.m_SmallSize + Metrics.m_LineSpacing);
}

inline float ResolveQmHudDummyMiniViewHeight(const SSettingsContentMetrics &Metrics, const bool Expanded)
{
	const float PreviewGap = Metrics.m_LineHeight * 0.8f + Metrics.m_LineSpacing;
	return (Expanded ? 4.0f : 1.0f) * Metrics.m_RowStep + PreviewGap;
}

inline float ResolveQmHudVoiceHeight(const SSettingsContentMetrics &Metrics, const bool Enabled, const bool Advanced, const bool ShowStatus, const int NoiseSuppressMode, const bool VadEnabled, const bool StereoEnabled)
{
	if(!Enabled)
		return Metrics.m_RowStep;

	// 常规区域：启用、房间密码、静音、麦克风音量、VAD 和高级选项。
	float Height = 6.0f * Metrics.m_RowStep;
	if(!Advanced)
		return Height;

	// 高级固定区域：状态开关、服务器/设备/编码/降噪、AGC、播放、立体声、半径和房间范围。
	Height += 11.0f * Metrics.m_RowStep + Metrics.m_LineSpacing * 1.15f;
	if(NoiseSuppressMode != 0)
		Height += Metrics.m_RowStep;
#if !defined(CONF_RNNOISE)
	if(NoiseSuppressMode == 2)
		Height += Metrics.m_LineHeight * 0.78f + Metrics.m_LineSpacing * 0.75f;
#endif
	if(VadEnabled)
		Height += 2.0f * Metrics.m_RowStep;
	if(StereoEnabled)
		Height += Metrics.m_RowStep;
	if(ShowStatus)
	{
		// 标题、提示、十个稳定状态行以及状态区尾间距。
		Height += Metrics.m_LineHeight * 1.46f + Metrics.m_LineSpacing * 0.75f;
		Height += 10.0f * (Metrics.m_LineHeight + Metrics.m_LineSpacing * 0.75f);
		Height += Metrics.m_LineSpacing * 0.5f;
	}
	return Height;
}

inline float ResolveQmHudLyricsPreviewHeight(const int FontSize, const int LineCount)
{
	constexpr float PreviewPaddingY = 7.0f;
	constexpr float PreviewLineGap = 5.0f;
	const int ClampedLines = std::clamp(LineCount, 1, 2);
	return std::max(42.0f, PreviewPaddingY * 2.0f + std::max(0, FontSize) * ClampedLines + PreviewLineGap * (ClampedLines - 1));
}

inline float ResolveQmHudLyricsHeight(const SSettingsContentMetrics &Metrics, const int PreviewFontSize, const int PreviewLineCount)
{
	return 48.0f * Metrics.m_RowStep + ResolveQmHudLyricsPreviewHeight(PreviewFontSize, PreviewLineCount) + Metrics.m_LineSpacing;
}

inline float ResolveQmHudBackground3DHeight(const SSettingsContentMetrics &Metrics, const float ContentWidth, const bool Enabled, const bool CustomColor, const bool Glow, const bool Trail, const bool Pulse, const bool Twinkle)
{
	if(!Enabled)
		return Metrics.m_RowStep;
	const SSettingsRadioRowLayout Radio = ResolveSettingsRadioRowLayout({0.0f, 0.0f, ContentWidth, Metrics.m_LineHeight * 2.0f + Metrics.m_LineSpacing}, 2, Metrics);
	float Height = 18.0f * Metrics.m_RowStep + Radio.m_Height + Metrics.m_LineSpacing;
	Height += CustomColor ? Metrics.m_RowStep : 0.0f;
	Height += Glow ? 2.0f * Metrics.m_RowStep : 0.0f;
	Height += Trail ? 2.0f * Metrics.m_RowStep : 0.0f;
	Height += Pulse ? 2.0f * Metrics.m_RowStep : 0.0f;
	Height += Twinkle ? Metrics.m_RowStep : 0.0f;
	return Height;
}

inline uint64_t ResolveQmHudVoiceRevision(const bool Enabled, const bool Advanced, const bool ShowStatus, const int NoiseSuppressMode, const bool VadEnabled, const bool StereoEnabled)
{
	return (Enabled ? 1u : 0u) |
	       (Advanced ? 1u << 1 : 0u) |
	       (ShowStatus ? 1u << 2 : 0u) |
	       ((uint64_t)std::clamp(NoiseSuppressMode, 0, 3) << 3) |
	       (VadEnabled ? 1u << 5 : 0u) |
	       (StereoEnabled ? 1u << 6 : 0u);
}

inline uint64_t ResolveQmHudBackground3DRevision(const bool Enabled, const bool CustomColor, const bool Glow, const bool Trail, const bool Pulse, const bool Twinkle)
{
	return (Enabled ? 1u : 0u) |
	       (CustomColor ? 1u << 1 : 0u) |
	       (Glow ? 1u << 2 : 0u) |
	       (Trail ? 1u << 3 : 0u) |
	       (Pulse ? 1u << 4 : 0u) |
	       (Twinkle ? 1u << 5 : 0u);
}

inline float ResolveAppearanceLaserColorsHeight(const SSettingsContentMetrics &Metrics)
{
	constexpr int ColorPickerCount = 10;
	constexpr float EntitySectionGap = 10.0f;
	const float HeadingHeight = Metrics.m_LineHeight + 2.0f * Metrics.m_LineSpacing;
	const float ColorPickerHeight = Metrics.m_ButtonHeight + Metrics.m_LineSpacing;
	return 2.0f * HeadingHeight + 8.0f * Metrics.m_LineSpacing + ColorPickerCount * ColorPickerHeight + EntitySectionGap + 2.0f * Metrics.m_LineHeight;
}

inline float ResolveAppearanceLaserEnhancedHeight(const SSettingsContentMetrics &Metrics, const bool Enhanced)
{
	const int RowCount = Enhanced ? 7 : 5;
	const int SpacingCount = Enhanced ? 6 : 4;
	return RowCount * Metrics.m_LineHeight + SpacingCount * Metrics.m_LineSpacing;
}

inline float ResolveDDNetDemoRows(const bool RaceGhostEnabled, const bool SaveGhostEnabled)
{
	return 5.0f + (RaceGhostEnabled ? 3.0f + (SaveGhostEnabled ? 1.0f : 0.0f) : 0.0f);
}

inline float ResolveDDNetGameplayRows(const bool TextEntitiesEnabled, const bool AntiPingEnabled)
{
	return 9.0f + (TextEntitiesEnabled ? 1.0f : 0.0f) + (AntiPingEnabled ? 3.0f : 0.0f);
}

inline SSettingsSubTabLayoutFrame ResolveSettingsSubTabLayout(const CUIRect &MainView, const float UiScale = 1.0f)
{
	const float Scale = UiScale > 0.0f ? UiScale : 1.0f;
	const float TabHeight = std::min(ui_token::settings::SUB_TAB_HEIGHT * Scale, std::max(0.0f, MainView.h));
	const float Gap = std::min(ui_token::settings::SUB_TAB_GAP * Scale, std::max(0.0f, MainView.h - TabHeight));
	SSettingsSubTabLayoutFrame Frame;
	Frame.m_TabBarRect = {MainView.x, MainView.y, MainView.w, TabHeight};
	Frame.m_ContentRect = {MainView.x, MainView.y + TabHeight + Gap, MainView.w, std::max(0.0f, MainView.h - TabHeight - Gap)};
	return Frame;
}

inline SSettingsPageLayoutFrame ResolveSettingsPageLayout(const CUIRect &PageRect, const bool HasSubTabs, const float UiScale = 1.0f)
{
	const float Scale = UiScale > 0.0f ? UiScale : 1.0f;
	const float Inset = ui_token::settings::PAGE_INSET * Scale;
	SSettingsPageLayoutFrame Frame{};
	Frame.m_PageRect = PageRect;
	float ContentTop = PageRect.y;
	float ContentHeight = std::max(0.0f, PageRect.h);
	if(HasSubTabs)
	{
		const SSettingsSubTabLayoutFrame SubTab = ResolveSettingsSubTabLayout(PageRect, Scale);
		Frame.m_SubTabRect = SubTab.m_TabBarRect;
		ContentTop = SubTab.m_ContentRect.y;
		ContentHeight = SubTab.m_ContentRect.h;
	}
	Frame.m_ScrollViewport = {
		PageRect.x + Inset,
		ContentTop + Inset,
		std::max(0.0f, PageRect.w - Inset * 2.0f),
		std::max(0.0f, ContentHeight - Inset * 2.0f),
	};
	Frame.m_UnreservedScrollViewport = Frame.m_ScrollViewport;
	Frame.m_ContentViewport = Frame.m_ScrollViewport;
	Frame.m_CardGap = ui_token::settings::CARD_GAP * Scale;
	Frame.m_TwoColumns = Frame.m_ContentViewport.w >= ui_token::settings::TWO_COLUMN_MIN_WIDTH;
	if(Frame.m_TwoColumns)
	{
		const float ColumnWidth = std::max(0.0f, (Frame.m_ContentViewport.w - Frame.m_CardGap) * 0.5f);
		Frame.m_aColumns[0] = {Frame.m_ContentViewport.x, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
		Frame.m_aColumns[1] = {Frame.m_aColumns[0].x + ColumnWidth + Frame.m_CardGap, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
	}
	else
		Frame.m_aColumns[0] = Frame.m_ContentViewport;
	return Frame;
}

// 滚动条占用空间后，基于有效 viewport 重新计算内容列，避免卡片落入滚动条轨道。
inline SSettingsPageLayoutFrame ResolveSettingsPageLayoutForScrollViewport(const SSettingsPageLayoutFrame &Layout, const CUIRect &ScrollViewport, const float UiScale = 1.0f)
{
	(void)UiScale;
	SSettingsPageLayoutFrame Frame = Layout;
	Frame.m_ScrollViewport = ScrollViewport;
	Frame.m_ContentViewport = ScrollViewport;
	Frame.m_TwoColumns = Frame.m_ContentViewport.w >= ui_token::settings::TWO_COLUMN_MIN_WIDTH;
	Frame.m_aColumns[0] = {};
	Frame.m_aColumns[1] = {};
	if(Frame.m_TwoColumns)
	{
		const float ColumnWidth = std::max(0.0f, (Frame.m_ContentViewport.w - Frame.m_CardGap) * 0.5f);
		Frame.m_aColumns[0] = {Frame.m_ContentViewport.x, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
		Frame.m_aColumns[1] = {Frame.m_aColumns[0].x + ColumnWidth + Frame.m_CardGap, Frame.m_ContentViewport.y, ColumnWidth, Frame.m_ContentViewport.h};
	}
	else
		Frame.m_aColumns[0] = Frame.m_ContentViewport;
	return Frame;
}

inline SSettingsShellLayoutFrame ResolveSettingsShellLayout(const CUIRect &AvailableRect, const float BottomReservedHeight = 0.0f)
{
	constexpr float ShellGap = 10.0f;
	constexpr float PanelMargin = 10.0f;
	constexpr float RestartBarHeight = 20.0f;
	SSettingsShellLayoutFrame Frame;
	CUIRect ShellAvailable = AvailableRect;
	ShellAvailable.h = std::max(0.0f, ShellAvailable.h - std::max(0.0f, BottomReservedHeight));
	Frame.m_TabBarWidth = std::clamp(AvailableRect.w * 0.16f, 132.0f, 168.0f);
	const float AvailablePanelWidth = std::max(0.0f, ShellAvailable.w - Frame.m_TabBarWidth - ShellGap);
	const float MaxPanelWidth = ui_token::settings::MAX_CONTENT_WIDTH + PanelMargin * 2.0f;
	const float PanelWidth = std::min(AvailablePanelWidth, MaxPanelWidth);
	const float ShellWidth = PanelWidth + ShellGap + Frame.m_TabBarWidth;
	ShellAvailable.VMargin(std::max(0.0f, (ShellAvailable.w - ShellWidth) * 0.5f), &Frame.m_ShellRect);
	Frame.m_ShellRect.VSplitRight(Frame.m_TabBarWidth, &Frame.m_ContentPanelRect, &Frame.m_TabBarRect);
	Frame.m_ContentPanelRect.VSplitRight(ShellGap, &Frame.m_ContentPanelRect, nullptr);
	Frame.m_ContentRect = Frame.m_ContentPanelRect;
	Frame.m_ContentRect.Margin(PanelMargin, &Frame.m_ContentRect);
	if(BottomReservedHeight >= RestartBarHeight)
		Frame.m_RestartBarRect = {Frame.m_ContentPanelRect.x, AvailableRect.y + AvailableRect.h - RestartBarHeight, Frame.m_ContentPanelRect.w, RestartBarHeight};

	const SSettingsContentMetrics Metrics = ResolveSettingsContentMetrics(Frame.m_ContentRect.w);
	const SSettingsPageLayoutFrame Page = ResolveSettingsPageLayout(Frame.m_ContentRect, false, Metrics.m_UiScale);
	Frame.m_UiScale = Metrics.m_UiScale;
	Frame.m_ScrollViewport = Page.m_ScrollViewport;
	Frame.m_ScrollViewport.w = std::max(0.0f, Frame.m_ScrollViewport.w - ui_token::settings::OUTER_SCROLLBAR_SLOT);
	const SSettingsPageLayoutFrame EffectivePage = ResolveSettingsPageLayoutForScrollViewport(Page, Frame.m_ScrollViewport, Metrics.m_UiScale);
	Frame.m_aColumns[0] = EffectivePage.m_aColumns[0];
	Frame.m_aColumns[1] = EffectivePage.m_aColumns[1];
	Frame.m_CardGap = Page.m_CardGap;
	Frame.m_TwoColumns = EffectivePage.m_TwoColumns;
	return Frame;
}

#endif
