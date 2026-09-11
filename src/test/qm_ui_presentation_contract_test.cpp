#include <gtest/gtest.h>
#include <test/test.h>

#include <string>

TEST(QmImePresentationContract, PopupUsesContinuousRedirectablePresentationState)
{
	const std::string ManagerSource = ReadTestSourceFile("src/game/client/qm_ime_manager.cpp");
	const std::string PopupSource = ReadTestSourceFile("src/game/client/qm_ime_candidate_popup.cpp");
	const std::string PopupHeader = ReadTestSourceFile("src/game/client/qm_ime_candidate_popup.h");
	EXPECT_NE(ManagerSource.find("State.m_Visible = HasComposition && CandidateCount > 0;"), std::string::npos);
	EXPECT_NE(PopupHeader.find("SPresentationTargets"), std::string::npos);
	EXPECT_NE(PopupSource.find("SImePresentationTarget"), std::string::npos);
	EXPECT_EQ(PopupSource.find("ResolveImePresentationStateValue"), std::string::npos);
	EXPECT_NE(PopupSource.find("ResolveUiPresentationStateValue"), std::string::npos);
	EXPECT_NE(PopupSource.find("SetUiPresentationStateValue"), std::string::npos);
	EXPECT_NE(PopupSource.find("TargetPresentation.m_CandidateAlpha"), std::string::npos);
	EXPECT_NE(PopupSource.find("if(!m_Presentation.m_Initialized)"), std::string::npos);
	EXPECT_NE(PopupSource.find("m_Presentation.m_Initialized = true;"), std::string::npos);
	EXPECT_EQ(PopupSource.find("Presence.m_FreshEnter"), std::string::npos);
	EXPECT_NE(PopupSource.find("const float Alpha = minimum(Presence.m_Alpha, PresentationAlpha);"), std::string::npos);
	EXPECT_NE(PopupSource.find("const float CandidateDrawAlpha = Alpha * CandidateAlpha;"), std::string::npos);
	EXPECT_NE(PopupSource.find("WithAlpha(Ime.m_SelectedBg, CandidateDrawAlpha)"), std::string::npos);
	EXPECT_NE(PopupSource.find("TargetPresentation.m_Radius = PanelHeight * 0.5f;"), std::string::npos);
	EXPECT_NE(PopupSource.find("IME_CONTENT_TIME_SCALE = 0.40f"), std::string::npos);
	EXPECT_NE(PopupSource.find("BuildCandidateViewport"), std::string::npos);
}

TEST(QmUiPresentationContract, OverlaysUsePresentationState)
{
	const std::string OverlaySource = ReadTestSourceFile("src/game/client/QmUi/UiOverlays.h");
	EXPECT_NE(OverlaySource.find("ResolveUiPresentationStateValue"), std::string::npos);
	EXPECT_NE(OverlaySource.find("SetUiPresentationStateValue"), std::string::npos);
	EXPECT_EQ(OverlaySource.find("->SetValue("), std::string::npos);
}
