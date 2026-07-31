// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <game/client/QmUi/QmLayout.h>
#include <game/client/components/qmclient/afk_presentation.h>
#include <game/client/components/qmclient/input_overlay.h>
#include <game/client/components/qmclient/scoreboard_team_modes.h>
#include <game/client/components/scoreboard.h>

#include <gtest/gtest.h>

#include <vector>

TEST(QmInputOverlayLayout, MouseClassificationRequiresMouseOnlyInputs)
{
	EXPECT_TRUE(QmInputOverlay::IsMouseOnlyLayout(false, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, false));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(false, false));
}

TEST(QmInputOverlayLayout, MouseSizeDoesNotMoveKeyboardOrMouseAnchor)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.1f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	EXPECT_FLOAT_EQ(Keyboard.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(Keyboard.m_MaxX, 216.0f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX, LargeMouse.m_MinX);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX - Keyboard.m_MaxX, 17.5f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MaxX - SmallMouse.m_MinX, 28.5f);
	EXPECT_FLOAT_EQ(LargeMouse.m_MaxX - LargeMouse.m_MinX, 142.5f);
}

TEST(QmInputOverlayLayout, VisibleBoundsUseIndependentContentScales)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.25f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	const auto SmallBounds = QmInputOverlay::UnionBounds(Keyboard, SmallMouse);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinY, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxX, 304.75f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxY, 150.0f);

	const auto LargeBounds = QmInputOverlay::UnionBounds(Keyboard, LargeMouse);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxX, 376.0f);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxY, 210.5f);
}

TEST(QmAfkPresentation, ServerAndEscMenuStatesRemainAvailableForNonOpacityIndicators)
{
	EXPECT_TRUE(IsQmAfkForPresentation(true, false, false, 7, 3));
	EXPECT_TRUE(IsQmAfkForPresentation(false, true, true, 3, 3));

	EXPECT_FALSE(IsQmAfkForPresentation(false, true, false, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, 4, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, false, true, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, -1, -1));
}

TEST(QmScoreboardTeamModes, AggregationRequiresDisplayInfoAndCombinesKnownMembers)
{
	SQmScoreboardTeamModeState State;
	AccumulateQmScoreboardTeamModeState(State, false, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_FALSE(State.m_Known);
	EXPECT_EQ(State.m_Flags, 0);
	SQmScoreboardTeamModeState KnownEmptyState;
	AccumulateQmScoreboardTeamModeState(KnownEmptyState, true, 0);
	EXPECT_TRUE(KnownEmptyState.m_Known);
	EXPECT_EQ(KnownEmptyState.m_Flags, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_SOLO);
	EXPECT_TRUE(State.m_Known);
	EXPECT_TRUE(State.Practice());
	EXPECT_FALSE(State.Team0Mode());
	EXPECT_FALSE(State.Locked());
	EXPECT_EQ(State.m_Flags & CHARACTERFLAG_SOLO, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_TEAM0_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_TRUE(State.Practice());
	EXPECT_TRUE(State.Team0Mode());
	EXPECT_TRUE(State.Locked());
}

TEST(UiV2Layout, RowPaddingGapAndPosition)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 5.0f;
	ContainerStyle.m_Padding = {10.0f, 10.0f, 10.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::START;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 200.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 20.0f);

	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 65.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_H, 20.0f);
}

TEST(UiV2Layout, RowFlexDistribution)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 10.0f;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 230.0f, 40.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(2.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Px(30.0f);
	vChildren[2].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 60.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 120.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_W, 30.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 200.0f);
}

TEST(UiV2Layout, ColumnJustifyCenter)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 10.0f;
	ContainerStyle.m_JustifyContent = EUiAlign::CENTER;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 80.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 25.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 55.0f);
}

TEST(UiV2Layout, AlignStretchExpandsCrossAxis)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Padding = {0.0f, 10.0f, 0.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 100.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(1);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Auto();

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 80.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
}

TEST(UiV2Layout, ApplyConstraintsMinMaxPercent)
{
	CUiV2LayoutEngine Engine;
	SUiStyle Style;
	Style.m_Width = SUiLength::Percent(0.5f);
	Style.m_Height = SUiLength::Px(100.0f);
	Style.m_MinWidth = SUiLength::Px(120.0f);
	Style.m_MaxWidth = SUiLength::Px(180.0f);
	Style.m_MaxHeight = SUiLength::Px(70.0f);

	SUiLayoutBox Parent{0.0f, 0.0f, 300.0f, 300.0f};
	const SUiLayoutBox Box = Engine.ApplyConstraints(Style, Parent);

	EXPECT_FLOAT_EQ(Box.m_W, 150.0f);
	EXPECT_FLOAT_EQ(Box.m_H, 70.0f);
}

TEST(UiV2Layout, ScoreboardTeamColumnsWithGap)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 7.5f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 850.0f, 385.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 421.25f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 428.75f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 421.25f);
}

TEST(UiV2Layout, ScoreboardThreeColumnsEqualWidth)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 900.0f, 320.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 600.0f);
}

TEST(UiV2Layout, ScoreboardSoundMuteVerticalButtons)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 4.0f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 22.0f, 230.0f};

	std::vector<SUiLayoutChild> vChildren(9);
	for(SUiLayoutChild &Child : vChildren)
	{
		Child.m_Style.m_Height = SUiLength::Px(22.0f);
	}

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 22.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[8].m_Box.m_Y, 208.0f);
}

TEST(QmScoreboardRender, BlurTargetUsesQuarterResolutionAndRoundsUp)
{
	EXPECT_EQ(ScoreboardBlurTargetDimension(1920), 480);
	EXPECT_EQ(ScoreboardBlurTargetDimension(1080), 270);
	EXPECT_EQ(ScoreboardBlurTargetDimension(1081), 271);
	EXPECT_EQ(ScoreboardBlurTargetDimension(1), 1);
	EXPECT_EQ(ScoreboardBlurTargetDimension(0), 0);
}

TEST(QmScoreboardRender, DenseRowsKeepClientBrandWhileReducingSecondaryDetail)
{
	const SScoreboardRowRenderDetail Disabled = ResolveScoreboardRowRenderDetail(false, 5.0f);
	EXPECT_TRUE(Disabled.m_FullTee);
	EXPECT_TRUE(Disabled.m_ShowClientBrand);
	EXPECT_TRUE(Disabled.m_ShowClan);
	EXPECT_TRUE(Disabled.m_ShowCountry);

	const SScoreboardRowRenderDetail Regular = ResolveScoreboardRowRenderDetail(true, 13.5f);
	EXPECT_TRUE(Regular.m_FullTee);
	EXPECT_TRUE(Regular.m_ShowClientBrand);
	EXPECT_TRUE(Regular.m_ShowClan);
	EXPECT_TRUE(Regular.m_ShowCountry);

	const SScoreboardRowRenderDetail Dense = ResolveScoreboardRowRenderDetail(true, 10.0f);
	EXPECT_FALSE(Dense.m_FullTee);
	EXPECT_TRUE(Dense.m_ShowClientBrand);
	EXPECT_FALSE(Dense.m_ShowClan);
	EXPECT_FALSE(Dense.m_ShowCountry);
}

TEST(QmScoreboardRender, DdTeamLabelUsesBelowRowLayoutRegardlessOfColumnCount)
{
	const SScoreboardTeamLabelLayout SingleColumn = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 8.0f, 8.0f, 0.0f, true);
	const SScoreboardTeamLabelLayout MultiColumn = ResolveScoreboardTeamLabelLayout(220.0f, 40.0f, 30.0f, 2.5f, 8.0f, 0.0f, true);

	EXPECT_FLOAT_EQ(SingleColumn.m_X, 25.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_X, 225.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_RowSpacing, 8.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_RowSpacing, 8.0f);

	// A DDTeam that continues in the next column must not reserve or render a duplicate label.
	const SScoreboardTeamLabelLayout ContinuedTeam = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 2.5f, 8.0f, SCOREBOARD_TEAM_MODE_ICON_SIZE, false);
	EXPECT_FLOAT_EQ(ContinuedTeam.m_RowSpacing, 2.5f);
}

TEST(QmScoreboardRender, DdTeamModeIconsUseNativeHudSizeAndCenteredLabelLayout)
{
	const SScoreboardTeamLabelLayout Layout = ResolveScoreboardTeamLabelLayout(
		220.0f,
		40.0f,
		25.0f,
		2.5f,
		8.0f,
		SCOREBOARD_TEAM_MODE_ICON_SIZE,
		true);

	EXPECT_FLOAT_EQ(SCOREBOARD_TEAM_MODE_ICON_SIZE, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_RowSpacing, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_Y, 67.0f);
	EXPECT_FLOAT_EQ(Layout.m_IconY, 65.0f);
}

TEST(QmScoreboardRender, DdTeamLabelSpacingFitsDenseColumnsWithoutOverlap)
{
	constexpr float AvailableRowsHeight = 333.0f;
	constexpr int RowsPerColumn = 12;
	constexpr float PreferredLineHeight = 25.0f;
	constexpr float PreferredSpacing = 2.5f;
	constexpr float PreferredTeamFontSize = 8.0f;
	const float ScaleWithoutTeams = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, 0, 0, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const float Scale = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, RowsPerColumn, RowsPerColumn, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const SScoreboardTeamLabelLayout TeamEnd = ResolveScoreboardTeamLabelLayout(
		0.0f,
		0.0f,
		PreferredLineHeight * Scale,
		PreferredSpacing * Scale,
		PreferredTeamFontSize * Scale,
		SCOREBOARD_TEAM_MODE_ICON_SIZE,
		true);

	EXPECT_FLOAT_EQ(ScaleWithoutTeams, 1.0f);
	EXPECT_LT(Scale, 1.0f);
	EXPECT_FLOAT_EQ(TeamEnd.m_RowSpacing, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	EXPECT_LE(RowsPerColumn * (PreferredLineHeight * Scale + TeamEnd.m_RowSpacing), AvailableRowsHeight + 0.001f);
}
