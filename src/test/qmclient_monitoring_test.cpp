#define CONF_TEST 1
#include <engine/client/gpu_upload_limiter.h>

#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/components/qmclient/monitoring/monitoring.h>
#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/components/qmclient/settings_perf_windows.h>
#include <game/client/components/qmclient/settings_resource_preview.h>
#include <game/client/components/settings_resource_jobs.h>
#include <game/client/frame_scheduler.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

	std::string ReadRepoFile(const char *pPath)
	{
		return ReadTestSourceFile(pPath);
	}

	bool ContainsAll(const std::string &Source, std::initializer_list<const char *> Needles)
	{
		for(const char *pNeedle : Needles)
		{
			if(Source.find(pNeedle) == std::string::npos)
				return false;
		}
		return true;
	}

	bool ContainsAny(const std::string &Source, std::initializer_list<const char *> Needles)
	{
		for(const char *pNeedle : Needles)
		{
			if(Source.find(pNeedle) != std::string::npos)
				return true;
		}
		return false;
	}

	std::string Trim(const std::string &Text)
	{
		const size_t Begin = Text.find_first_not_of(" \t\r\n");
		if(Begin == std::string::npos)
			return {};

		const size_t End = Text.find_last_not_of(" \t\r\n");
		return Text.substr(Begin, End - Begin + 1);
	}

	std::vector<std::string> SplitLines(const std::string &Source)
	{
		std::vector<std::string> vLines;
		std::stringstream Stream(Source);
		std::string Line;
		while(std::getline(Stream, Line))
			vLines.push_back(Line);
		return vLines;
	}

	void AppendUniqueCandidate(std::vector<std::string> &vCandidates, const std::string &Candidate)
	{
		if(Candidate.empty())
			return;

		for(const std::string &Existing : vCandidates)
		{
			if(Existing == Candidate)
				return;
		}
		vCandidates.push_back(Candidate);
	}

	std::vector<std::string> ExtractQuotedCandidates(const std::string &Line)
	{
		std::vector<std::string> vFound;
		size_t Pos = 0;
		while((Pos = Line.find('"', Pos)) != std::string::npos)
		{
			const size_t End = Line.find('"', Pos + 1);
			if(End == std::string::npos)
				break;
			AppendUniqueCandidate(vFound, Line.substr(Pos + 1, End - Pos - 1));
			Pos = End + 1;
		}
		return vFound;
	}

	std::string ExtractVariableAssignment(const std::string &Line)
	{
		for(const char *pVar : {"pTitle", "pText", "pLabel"})
		{
			const size_t VarPos = Line.find(pVar);
			if(VarPos == std::string::npos)
				continue;

			const size_t EqualPos = Line.find('=', VarPos + 1);
			if(EqualPos == std::string::npos)
				continue;

			return Trim(Line.substr(EqualPos + 1));
		}
		return {};
	}

	[[maybe_unused]] std::vector<std::string> CollectStableTextCandidates(const std::string &Source)
	{
		const std::vector<std::string> vLines = SplitLines(Source);
		std::vector<std::string> vCandidates;
		for(size_t LineIndex = 0; LineIndex < vLines.size(); ++LineIndex)
		{
			const std::string &Line = vLines[LineIndex];
			if(!ContainsAny(Line,
				   {
					   "Ui()->DoLabel(",
					   "DoButton_Menu(",
					   "DoButton_CheckBox",
					   "DoButton_CheckBoxAutoVMarginAndSet",
					   "Ui()->DoScrollbarOption",
					   "DoSettingsLabel",
					   "DoSettingsMenuLabel",
					   "DoSettingsButton_",
					   "DoSettingsScrollbarOption",
					   "SettingsTextElement",
					   "QmNewFeatureLabel",
					   "RainbowColor",
					   "DoModuleHeadline",
					   "pTitle",
					   "pText",
					   "pLabel",
				   }))
			{
				continue;
			}

			for(const std::string &Quoted : ExtractQuotedCandidates(Line))
				AppendUniqueCandidate(vCandidates, Quoted);

			const size_t BlockBegin = LineIndex > 2 ? LineIndex - 2 : 0;
			const size_t BlockEnd = std::min(LineIndex + 3, vLines.size());
			for(size_t NearbyLine = BlockBegin; NearbyLine < BlockEnd; ++NearbyLine)
			{
				for(const std::string &Quoted : ExtractQuotedCandidates(vLines[NearbyLine]))
					AppendUniqueCandidate(vCandidates, Quoted);

				const std::string Assignment = ExtractVariableAssignment(vLines[NearbyLine]);
				if(!Assignment.empty())
					AppendUniqueCandidate(vCandidates, Assignment);
			}

			if(Line.find("QmNewFeatureLabel") != std::string::npos)
				AppendUniqueCandidate(vCandidates, "QmNewFeatureLabel");
			if(Line.find("RainbowColor") != std::string::npos)
				AppendUniqueCandidate(vCandidates, "RainbowColor");
			if(Line.find("DoModuleHeadline") != std::string::npos)
				AppendUniqueCandidate(vCandidates, "DoModuleHeadline");
		}
		return vCandidates;
	}

	struct SStableTextCandidate
	{
		int m_Line = 0;
		std::string m_Text;
		std::string m_SourceLine;
	};

	struct SStableTextRawAllow
	{
		const char *m_pFile;
		int m_Line;
		const char *m_pReason;
	};

	bool IsStableTextAllowedReason(const char *pReason)
	{
		for(const char *pAllowed : {
			    "dynamic-value",
			    "user-generated",
			    "localized-list-data",
			    "stateful-new-label",
			    "animated-style",
			    "icon-only",
			    "status-message",
			    "search-result",
			    "input-text",
		    })
		{
			if(str_comp(pReason, pAllowed) == 0)
				return true;
		}
		return false;
	}

	bool IsDynamicStableTextCandidateLine(const std::string &Line)
	{
		return ContainsAny(Line, {
						 "aBuf",
						 "pSkinContainer->Name()",
						 "Language.m_Name",
						 "m_aStatusMessage",
						 "FONT_ICON_",
						 "pValue",
						 "DoEditBox",
						 "Input",
						 "Search",
						 "m_aName",
						 "m_aClan",
						 "Profile.",
						 "Client.",
						 "pEntry->",
						 "pType->",
						 "pVar->",
						 "SourceName(",
						 "pText,",
						 "pLabel,",
						 "pTitle,",
					 });
	}

	bool IsStableTextCandidatePayloadIgnored(const std::string &Text)
	{
		return Text == "%" || Text == "ms" || Text == "ms (off)" || Text == "s" || Text == " min" || Text == " seconds" || Text == " seconds (never)" || Text == "X" || Text == "RainbowColor";
	}

	bool IsPooledStableTextLine(const std::string &Line)
	{
		return ContainsAny(Line, {
						 "DoSettingsMenuLabel(",
						 "DoSettingsButton_Menu(",
						 "DoSettingsButton_CheckBox(",
						 "DoSettingsButton_CheckBoxAutoVMarginAndSet(",
						 "DoButton_Menu(",
						 "DoButton_CheckBox(",
						 "DoButton_CheckBox_Common(",
						 "DoButton_CheckBoxAutoVMarginAndSet(",
						 "DoSettingsScrollbarOption(",
						 "SettingsTextElement(",
						 "DoSettingsLabel(",
						 "DoQmSettingsLabel(",
						 "DoQmSettingsCheckbox(",
						 "DoQmSettingsMenuButton(",
					 });
	}

	std::vector<SStableTextCandidate> CollectRawStableTextCandidatesWithLines(const std::string &Source)
	{
		const std::vector<std::string> vLines = SplitLines(Source);
		std::vector<SStableTextCandidate> vCandidates;
		for(size_t LineIndex = 0; LineIndex < vLines.size(); ++LineIndex)
		{
			const std::string TrimmedLine = Trim(vLines[LineIndex]);
			if(TrimmedLine.empty() || TrimmedLine.rfind("//", 0) == 0)
				continue;
			if(TrimmedLine.find("TextRender()->TextColor(") != std::string::npos)
				continue;
			if(IsPooledStableTextLine(TrimmedLine) || IsDynamicStableTextCandidateLine(TrimmedLine))
				continue;
			if(!ContainsAny(TrimmedLine, {
							     "Ui()->DoLabel(",
							     "DoButton_Menu(",
							     "DoButton_CheckBox",
							     "Ui()->DoScrollbarOption",
							     "QmNewFeatureLabel",
							     "RainbowColor",
							     "DoModuleHeadline",
						     }))
			{
				continue;
			}

			std::vector<std::string> vTexts = ExtractQuotedCandidates(TrimmedLine);
			if(TrimmedLine.find("QmNewFeatureLabel") != std::string::npos)
				AppendUniqueCandidate(vTexts, "QmNewFeatureLabel");
			if(TrimmedLine.find("RainbowColor") != std::string::npos)
				AppendUniqueCandidate(vTexts, "RainbowColor");
			if(TrimmedLine.find("DoModuleHeadline") != std::string::npos)
				AppendUniqueCandidate(vTexts, "DoModuleHeadline");
			if(vTexts.empty() && ContainsAny(TrimmedLine, {"pTitle", "pText", "pLabel"}))
				AppendUniqueCandidate(vTexts, TrimmedLine);

			for(const std::string &Text : vTexts)
			{
				if(Text.empty() || IsStableTextCandidatePayloadIgnored(Text))
					continue;
				vCandidates.push_back({(int)LineIndex + 1, Text, TrimmedLine});
			}
		}
		return vCandidates;
	}

	bool IsStableTextCandidateAllowed(const SStableTextCandidate &Candidate, const char *pFile, const std::vector<SStableTextRawAllow> &vAllowlist)
	{
		for(const SStableTextRawAllow &Allow : vAllowlist)
		{
			EXPECT_TRUE(IsStableTextAllowedReason(Allow.m_pReason)) << Allow.m_pReason;
			if(str_comp(Allow.m_pFile, pFile) == 0 && Allow.m_Line == Candidate.m_Line)
				return true;
		}
		return false;
	}

	std::vector<SStableTextCandidate> FilterCandidatesNotCoveredByMenuPoolOrAllowlist(const char *pFile, const std::vector<SStableTextCandidate> &vCandidates, const std::vector<SStableTextRawAllow> &vAllowlist)
	{
		std::vector<SStableTextCandidate> vUnexpected;
		for(const SStableTextCandidate &Candidate : vCandidates)
		{
			if(!IsStableTextCandidateAllowed(Candidate, pFile, vAllowlist))
				vUnexpected.push_back(Candidate);
		}
		return vUnexpected;
	}

	std::string JoinCandidates(const std::vector<SStableTextCandidate> &vCandidates)
	{
		std::ostringstream Out;
		for(const SStableTextCandidate &Candidate : vCandidates)
			Out << "\nline " << Candidate.m_Line << ": " << Candidate.m_Text << " :: " << Candidate.m_SourceLine;
		return Out.str();
	}

	std::string ExtractSourceFunctionBody(const std::string &Source, const char *pSignature)
	{
		const size_t SignaturePos = Source.find(pSignature);
		if(SignaturePos == std::string::npos)
			return {};

		const size_t BodyStart = Source.find('{', SignaturePos);
		if(BodyStart == std::string::npos)
			return {};

		int Depth = 0;
		for(size_t i = BodyStart; i < Source.size(); ++i)
		{
			if(Source[i] == '{')
				++Depth;
			else if(Source[i] == '}')
			{
				--Depth;
				if(Depth == 0)
					return Source.substr(BodyStart, i - BodyStart + 1);
			}
		}
		return {};
	}

	std::string ExtractSourceBlock(const std::string &Source, const char *pBeginMarker, const char *pEndMarker)
	{
		const size_t Begin = Source.find(pBeginMarker);
		if(Begin == std::string::npos)
			return {};
		const size_t End = Source.find(pEndMarker, Begin);
		if(End == std::string::npos)
			return Source.substr(Begin);
		return Source.substr(Begin, End - Begin);
	}

	size_t CountSubstring(const std::string &Haystack, const std::string &Needle)
	{
		if(Needle.empty())
			return 0;

		size_t Count = 0;
		size_t Pos = 0;
		while((Pos = Haystack.find(Needle, Pos)) != std::string::npos)
		{
			++Count;
			Pos += Needle.size();
		}
		return Count;
	}

} // namespace

TEST(QmMonitoringHelpers, ConnectionGradeTracksDisconnectedState)
{
	SQmNetworkMetrics Net;
	Net.m_Connected = false;
	EXPECT_EQ(QmDetermineConnectionGrade(Net), EQmConnectionGrade::DISCONNECTED);
}

TEST(QmMonitoringHelpers, ConnectionGradeUsesThresholdTable)
{
	SQmNetworkMetrics Net;
	Net.m_Connected = true;
	Net.m_SnapshotLatencyMs = 40.0f;
	Net.m_PredictionLatencyMs = 50.0f;
	Net.m_JitterMs = 5.0f;
	Net.m_PacketLossPct = 0.0f;
	EXPECT_EQ(QmDetermineConnectionGrade(Net), EQmConnectionGrade::NORMAL);

	Net.m_PredictionLatencyMs = 110.0f;
	EXPECT_EQ(QmDetermineConnectionGrade(Net), EQmConnectionGrade::ELEVATED);

	Net.m_PredictionLatencyMs = 210.0f;
	EXPECT_EQ(QmDetermineConnectionGrade(Net), EQmConnectionGrade::SEVERE);
}

TEST(QmMonitoringHelpers, PrimaryCausePrefersDominantMetric)
{
	SQmNetworkMetrics Net;
	SQmPerformanceMetrics Perf;

	Net.m_Connected = false;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::DISCONNECTED), EQmDiagnosticCause::NONE);

	Net.m_Connected = true;
	Net.m_SnapshotLatencyMs = 120.0f;
	Net.m_PredictionLatencyMs = 40.0f;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::ELEVATED), EQmDiagnosticCause::DOWNSTREAM);

	Net.m_SnapshotLatencyMs = 20.0f;
	Net.m_PredictionLatencyMs = 95.0f;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::ELEVATED), EQmDiagnosticCause::UPSTREAM);

	Net.m_PredictionLatencyMs = 30.0f;
	Net.m_JitterMs = 28.0f;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::SEVERE), EQmDiagnosticCause::JITTER);

	Net.m_JitterMs = 6.0f;
	Net.m_PacketLossPct = 8.0f;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::SEVERE), EQmDiagnosticCause::PACKET_LOSS);

	Net.m_PacketLossPct = 0.0f;
	Net.m_ConnectionProblems = true;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::SEVERE), EQmDiagnosticCause::DOWNSTREAM);

	Net.m_PacketLossPct = 2.0f;
	Net.m_ConnectionProblems = true;
	Net.m_SnapshotLatencyMs = 130.0f;
	Net.m_PredictionLatencyMs = 20.0f;
	EXPECT_EQ(QmDeterminePrimaryCause(Net, Perf, EQmConnectionGrade::ELEVATED), EQmDiagnosticCause::PACKET_LOSS);
}

TEST(QmMonitoringHelpers, DiagnosticLossRateUsesSendDeltaAndResends)
{
	EXPECT_NEAR(QmComputeDiagnosticPacketLossPct(60, 20), 33.3333f, 0.001f);
	EXPECT_FLOAT_EQ(QmComputeDiagnosticPacketLossPct(60, 20), 33.333332f);
	EXPECT_FLOAT_EQ(QmComputeDiagnosticPacketLossPct(0, 0), 0.0f);
	EXPECT_FLOAT_EQ(QmComputeDiagnosticPacketLossPct(0, 3), 100.0f);
}

TEST(QmMonitoringHelpers, RollbackAmountUsesNegativeGameTimeMargin)
{
	EXPECT_FLOAT_EQ(QmComputeRollbackMs(-18.0f), 18.0f);
	EXPECT_FLOAT_EQ(QmComputeRollbackMs(6.0f), 0.0f);
}

TEST(QmMonitoringHelpers, PeakSelectionPrefersLatestMatchingPeak)
{
	std::array<float, 8> aHistory = {41.0f, 39.0f, 41.0f, 24.0f, 24.0f, 18.0f, 24.0f, 17.0f};
	EXPECT_EQ(QmFindLatestPeakIndex(aHistory, 0, 8), 2);
	EXPECT_EQ(QmFindLatestAbsolutePeakIndex(aHistory, 0, 8), 2);
}

TEST(QmMonitoringHelpers, SignedPeakSelectionUsesLatestAbsolutePeak)
{
	std::array<float, 8> aHistory = {-7.0f, 5.0f, -9.0f, 3.0f, 9.0f, 4.0f, 8.0f, 2.0f};
	EXPECT_EQ(QmFindLatestAbsolutePeakIndex(aHistory, 0, 8), 4);
}

TEST(QmMonitoringHelpers, UiScaleGrowsOnHighResolutionScreens)
{
	EXPECT_FLOAT_EQ(QmComputeMonitoringUiScale(800.0f, 600.0f), 0.65f);
	const float Expected1600x900 = std::sqrt((1600.0f / 1920.0f) * (900.0f / 1080.0f));
	EXPECT_FLOAT_EQ(QmComputeMonitoringUiScale(1600.0f, 900.0f), Expected1600x900);
	EXPECT_FLOAT_EQ(QmComputeMonitoringUiScale(3840.0f, 2160.0f), 1.8f);
}

TEST(QmMonitoringHelpers, PanelOpacityClampsPercentToUnitRange)
{
	EXPECT_FLOAT_EQ(QmComputeMonitoringPanelOpacity(-20), 0.0f);
	EXPECT_FLOAT_EQ(QmComputeMonitoringPanelOpacity(35), 0.35f);
	EXPECT_FLOAT_EQ(QmComputeMonitoringPanelOpacity(140), 1.0f);
}

TEST(QmMonitoringHelpers, ProcessCpuUsageNormalizesAcrossLogicalCpus)
{
	EXPECT_FLOAT_EQ(QmNormalizeProcessCpuUsagePct(-1.0f, 8), -1.0f);
	EXPECT_FLOAT_EQ(QmNormalizeProcessCpuUsagePct(114.0f, 8), 14.25f);
	EXPECT_FLOAT_EQ(QmNormalizeProcessCpuUsagePct(1600.0f, 16), 100.0f);
	EXPECT_FLOAT_EQ(QmNormalizeProcessCpuUsagePct(114.0f, 0), 100.0f);
}

TEST(QmMonitoringHelpers, TotalCpuUsageComputesBusyDelta)
{
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 1000, 125, 1100), 75.0f);
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 1000, 200, 1100), 0.0f);
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 1000, 100, 1100), 100.0f);
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 0, 125, 1100), -1.0f);
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 1000, 90, 1100), -1.0f);
	EXPECT_FLOAT_EQ(QmComputeTotalCpuUsagePct(100, 1000, 125, 990), -1.0f);
}

TEST(QmMonitoringHelpers, CpuRatioValueShowsProcessAndTotalCpu)
{
	char aBuf[32];
	FormatCpuRatioValue(aBuf, sizeof(aBuf), -1.0f, 35.0f);
	EXPECT_STREQ(aBuf, "--");
	FormatCpuRatioValue(aBuf, sizeof(aBuf), 12.4f, -1.0f);
	EXPECT_STREQ(aBuf, "12%");
	FormatCpuRatioValue(aBuf, sizeof(aBuf), 12.4f, 35.6f);
	EXPECT_STREQ(aBuf, "12%/36%");
}

TEST(QmMonitoringHelpers, TrafficStatsMatchOfficialDebugMath)
{
	const auto Stats = QmComputeTrafficStats(10, 1000, 14, 1320);
	EXPECT_EQ(Stats.m_Packets, 4u);
	EXPECT_EQ(Stats.m_PayloadBytes, 320u);
	EXPECT_EQ(Stats.m_OverheadBytes, 168u);
	EXPECT_EQ(Stats.m_TotalBytes, 488u);
	EXPECT_EQ(Stats.m_AveragePayloadBytes, 80u);
	EXPECT_FLOAT_EQ(Stats.m_RateKibPerSec, 3.8125f);
}

TEST(QmMonitoringHelpers, HudLayoutPlacesPanelLeftOfGraphColumn)
{
	const SQmMonitoringHudLayout Layout = QmComputeMonitoringHudLayout(1600.0f, 900.0f, 1184.0f, 16.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.w, 768.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.h, 594.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.x, 400.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.y, 32.0f);
	EXPECT_FLOAT_EQ(Layout.m_ContentRect.x, 410.0f);
	EXPECT_FLOAT_EQ(Layout.m_ContentRect.y, 42.0f);
}

TEST(QmMonitoringHelpers, BodyLayoutPreservesMetricsBudgetOnCompactPanels)
{
	const SQmMonitoringBodyLayout Layout = QmComputeMonitoringBodyLayout(260.0f, 1.0f);
	EXPECT_NEAR(Layout.m_MainGraphHeight, 63.1f, 0.1f);
	EXPECT_NEAR(Layout.m_FpsGraphHeight, 39.8f, 0.1f);
	EXPECT_NEAR(Layout.m_PrimaryCardsHeight, 37.2f, 0.1f);
	EXPECT_NEAR(Layout.m_MetricsExtraHeight, 31.9f, 0.1f);
	EXPECT_GT(Layout.m_PrimaryCardsHeight, 30.0f);
}

TEST(QmMonitoringHelpers, HudLayoutUsesLargerPanelOn4kScreens)
{
	const SQmMonitoringHudLayout Layout = QmComputeMonitoringHudLayout(3840.0f, 2160.0f, 2842.0f, 38.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.w, 1843.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.h, 1405.0f);
	EXPECT_FLOAT_EQ(Layout.m_ContentRect.x, 983.0f);
	EXPECT_FLOAT_EQ(Layout.m_ContentRect.y, 98.0f);
}

TEST(QmMonitoringHelpers, HudLayoutClampsPanelInsideScreenBounds)
{
	const SQmMonitoringHudLayout Layout = QmComputeMonitoringHudLayout(360.0f, 240.0f, 120.0f, 16.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.x, 0.0f);
	EXPECT_FLOAT_EQ(Layout.m_PanelRect.y, 0.0f);
	EXPECT_LE(Layout.m_PanelRect.x + Layout.m_PanelRect.w, 360.0f);
	EXPECT_LE(Layout.m_PanelRect.y + Layout.m_PanelRect.h, 240.0f);
}

TEST(QmMonitoringHelpers, DeviceMetricsDefaultToUnavailable)
{
	SQmPerformanceMetrics Perf;
	EXPECT_FALSE(Perf.m_DeviceSampleAvailable);
	EXPECT_FLOAT_EQ(Perf.m_GpuUtilPct, -1.0f);
	EXPECT_FLOAT_EQ(Perf.m_GpuDedicatedVramMb, -1.0f);
	EXPECT_FLOAT_EQ(Perf.m_GpuDedicatedVramBudgetMb, -1.0f);
	EXPECT_FLOAT_EQ(Perf.m_GpuSharedVramMb, -1.0f);
	EXPECT_FLOAT_EQ(Perf.m_DiskReadMbPerSec, -1.0f);
}

TEST(QmMonitoringHelpers, TeeSkinListFrameTelemetryExposesRowsFields)
{
	std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	EXPECT_NE(Source.find("event=list_frame page=settings:tee"), std::string::npos);
	EXPECT_NE(Source.find("rows_total=%d rows_visible=%d rows_rendered=%d rows_iterated=%d rows_skipped=%d"), std::string::npos);
	EXPECT_NE(Source.find("first_visible_index=%d last_visible_index=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, JumpHintLegacyConfigMigratesAllFieldsToQmConfig)
{
	std::ifstream TClientFile(TestSourcePath("src/engine/shared/config_variables_tclient.h"));
	ASSERT_TRUE(TClientFile.good());
	std::stringstream TClientBuffer;
	TClientBuffer << TClientFile.rdbuf();
	const std::string TClientSource = TClientBuffer.str();

	std::ifstream QmFile(TestSourcePath("src/engine/shared/config_variables_qmclient.h"));
	ASSERT_TRUE(QmFile.good());
	std::stringstream QmBuffer;
	QmBuffer << QmFile.rdbuf();
	const std::string QmSource = QmBuffer.str();

	std::ifstream GameClientFile(TestSourcePath("src/game/client/gameclient.cpp"));
	ASSERT_TRUE(GameClientFile.good());
	std::stringstream GameClientBuffer;
	GameClientBuffer << GameClientFile.rdbuf();
	const std::string GameClientSource = GameClientBuffer.str();

	// The jump-hint UI moved from tc_jump_hint* to qm_jump_hint*. This test
	// guards the migration shape so we do not silently drop old users' enabled
	// state, color, position, or font size when only the text key is migrated.
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_INT(TcJumpHintLegacy, tc_jump_hint,"), std::string::npos);
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_STR(TcJumpHintTextLegacy, tc_jump_hint_text,"), std::string::npos);
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_COL(TcJumpHintColorLegacy, tc_jump_hint_color,"), std::string::npos);
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_INT(TcJumpHintXLegacy, tc_jump_hint_x,"), std::string::npos);
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_INT(TcJumpHintYLegacy, tc_jump_hint_y,"), std::string::npos);
	EXPECT_NE(TClientSource.find("MACRO_CONFIG_INT(TcJumpHintSizeLegacy, tc_jump_hint_size,"), std::string::npos);
	// Legacy keys are read-only migration inputs. Keeping CFGFLAG_SAVE on them
	// would re-save old tc_jump_hint* values and let them overwrite qm_jump_hint*
	// again whenever the user resets the new value back to its default.
	EXPECT_EQ(TClientSource.find("TcJumpHintLegacy, tc_jump_hint, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TcJumpHintTextLegacy, tc_jump_hint_text, 512, \"\", CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TcJumpHintColorLegacy, tc_jump_hint_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TcJumpHintXLegacy, tc_jump_hint_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TcJumpHintYLegacy, tc_jump_hint_y, 5, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(TClientSource.find("TcJumpHintSizeLegacy, tc_jump_hint_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_INT(QmJumpHint, qm_jump_hint,"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_STR(QmJumpHintText, qm_jump_hint_text,"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_COL(QmJumpHintColor, qm_jump_hint_color,"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_INT(QmJumpHintX, qm_jump_hint_x,"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_INT(QmJumpHintY, qm_jump_hint_y,"), std::string::npos);
	EXPECT_NE(QmSource.find("MACRO_CONFIG_INT(QmJumpHintSize, qm_jump_hint_size,"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateInt(g_Config.m_QmJumpHint, g_Config.m_TcJumpHintLegacy"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateStr(g_Config.m_QmJumpHintText, sizeof(g_Config.m_QmJumpHintText), g_Config.m_TcJumpHintTextLegacy"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateCol(g_Config.m_QmJumpHintColor, g_Config.m_TcJumpHintColorLegacy"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateInt(g_Config.m_QmJumpHintX, g_Config.m_TcJumpHintXLegacy"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateInt(g_Config.m_QmJumpHintY, g_Config.m_TcJumpHintYLegacy"), std::string::npos);
	EXPECT_NE(GameClientSource.find("MigrateInt(g_Config.m_QmJumpHintSize, g_Config.m_TcJumpHintSizeLegacy"), std::string::npos);
}

TEST(QmMonitoringHelpers, JumpHintSettingsCardExposesAllQmFields)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmFunctionJumpHintContent(");
	ASSERT_FALSE(Body.empty());

	// Jump hint migrated from tc_jump_hint* to qm_jump_hint*. The settings page
	// must expose the whole qm_* surface, not just the enable toggle, otherwise
	// users can render the HUD element but cannot adjust its color, position, or
	// size without using console commands.
	EXPECT_NE(Body.find("RenderQmFunctionCheckbox(&g_Config.m_QmJumpHint"), std::string::npos);
	EXPECT_NE(Body.find("&g_Config.m_QmJumpHintColor"), std::string::npos);
	EXPECT_NE(Body.find("&g_Config.m_QmJumpHintX"), std::string::npos);
	EXPECT_NE(Body.find("&g_Config.m_QmJumpHintY"), std::string::npos);
	EXPECT_NE(Body.find("&g_Config.m_QmJumpHintSize"), std::string::npos);
	EXPECT_NE(Body.find("RenderQmSettingsSliderWithValueInput(pInputId, ControlColumn, pValue"), std::string::npos);
	EXPECT_NE(Body.find("&s_QmJumpHintXInputId, &g_Config.m_QmJumpHintX"), std::string::npos);
	EXPECT_NE(Body.find("&s_QmJumpHintYInputId, &g_Config.m_QmJumpHintY"), std::string::npos);
	EXPECT_NE(Body.find("&s_QmJumpHintSizeInputId, &g_Config.m_QmJumpHintSize"), std::string::npos);
	EXPECT_EQ(Body.find("m_TcJumpHint"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiRuntimeTelemetryExposesSettingsContext)
{
	std::ifstream HeaderFile(TestSourcePath("src/game/client/QmUi/QmRt.h"));
	ASSERT_TRUE(HeaderFile.good());
	std::stringstream HeaderBuffer;
	HeaderBuffer << HeaderFile.rdbuf();
	const std::string Header = HeaderBuffer.str();

	std::ifstream SourceFile(TestSourcePath("src/game/client/QmUi/QmRt.cpp"));
	ASSERT_TRUE(SourceFile.good());
	std::stringstream SourceBuffer;
	SourceBuffer << SourceFile.rdbuf();
	const std::string Source = SourceBuffer.str();

	EXPECT_NE(Header.find("void SetPerfContext(const char *pPage, const char *pOperation);"), std::string::npos);
	EXPECT_EQ(Header.find("float m_LayoutMs = 0.0f;"), std::string::npos);
	EXPECT_NE(Header.find("int m_ActiveAnimCount = 0;"), std::string::npos);
	EXPECT_NE(Header.find("int m_QueuedAnimCount = 0;"), std::string::npos);
	EXPECT_NE(Source.find("active_anims=%d queued_anims=%d"), std::string::npos);
	EXPECT_EQ(Source.find("layout_ms=%.3f"), std::string::npos);
	EXPECT_NE(Source.find("QmPerfLogPayload(\"perf/ui_runtime\""), std::string::npos);
	EXPECT_NE(Source.find("QmPerfLogStage(\"perf/ui_runtime\", pStage, DurationMs, Force, pClient, pPage, nullptr, pExtra);"), std::string::npos);
	EXPECT_NE(Source.find("LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\\0' ? m_aPerfPage : nullptr, \"ui_runtime_total\", RenderTimer.ElapsedMs(), false, aExtra);"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientPerfTelemetryUsesLiveClientContext)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/components/qmclient/menus_qmclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("void LogQmPerfStage(IClient *pClient, const char *pStage, double DurationMs, bool Force = false, const char *pExtra = nullptr)"), std::string::npos);
		EXPECT_NE(Source.find("QmPerfLogStage(\"perf/qmclient\", pStage, DurationMs, Force, pClient, nullptr, nullptr, pExtra);"), std::string::npos);
		EXPECT_EQ(CountSubstring(Source, "LogQmPerfStage("), CountSubstring(Source, "LogQmPerfStage(Client(),") + 1);
		EXPECT_NE(Source.find("QmPerfLogPayload(\"perf/qmclient\", aPayload, Client(), CurrentQmUiPerfPage());"), std::string::npos);
		EXPECT_EQ(Source.find("QmPerfLogPayload(\"perf/qmclient\", aPayload);"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/gameclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("m_UiRuntimeV2.SetPerfContext(pPerfPage, m_Menus.CurrentQmUiPerfOperation());"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, SectionQuadBatchingDoesNotCrossTextOrClipBoundaries)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/ui.h"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("class CUiScopedQuadBatch"), std::string::npos);
		EXPECT_NE(Source.find("void BeginQuadBatch() const;"), std::string::npos);
		EXPECT_NE(Source.find("void FlushQuadBatch() const;"), std::string::npos);
		EXPECT_NE(Source.find("void EndQuadBatch() const;"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/ui.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("CUiScopedQuadBatch::CUiScopedQuadBatch"), std::string::npos);
		EXPECT_NE(Source.find("CUiScopedQuadBatch::~CUiScopedQuadBatch"), std::string::npos);
		EXPECT_NE(Source.find("void CUi::FlushQuadBatch() const"), std::string::npos);
		EXPECT_EQ(Source.find("RenderQuadContainerAsSpriteMultiple(m_QuadBatchContainerIndex"), std::string::npos);
		EXPECT_NE(Source.find("TextRender()->RenderTextContainer"), std::string::npos);
		EXPECT_NE(Source.find("void CUi::RenderLabelTextContainerAligned"), std::string::npos);
		EXPECT_NE(Source.find("FlushQuadBatch();\n\tTextRender()->RenderTextContainer"), std::string::npos);
		EXPECT_NE(Source.find("void CUi::ClipEnable(const CUIRect *pRect)\n{\n\tFlushQuadBatch();"), std::string::npos);
		EXPECT_NE(Source.find("void CUi::ClipDisable()\n{\n\tFlushQuadBatch();"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/tclient/menus_tclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("CUiScopedQuadBatch QuadBatchScope(Ui());"), std::string::npos);
	}
}

TEST(UiQuadBatch, PureColorFlushUsesUntexturedQuadContainerPath)
{
	std::ifstream File(TestSourcePath("src/game/client/ui.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	const std::string Body = ExtractSourceFunctionBody(Source, "void CUi::FlushQuadBatch() const");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("UiQuadBatchHasPendingSubmission"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->TextureClear();"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->SetColor(m_QuadBatchColor);"), std::string::npos);
	EXPECT_EQ(Body.find("RenderQuadContainerAsSpriteMultiple"), std::string::npos);
	EXPECT_NE(Body.find("m_vQuadBatchSprites"), std::string::npos);
	EXPECT_NE(Body.find("RenderQuadContainerEx(m_QuadBatchContainerIndex, 0, -1"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);"), std::string::npos);
	EXPECT_NE(Body.find("m_vQuadBatchSprites.clear();"), std::string::npos);
	EXPECT_NE(Body.find("m_QuadBatchContainerIndex = -1;"), std::string::npos);
}

TEST(QmMonitoringHelpers, UiQuadBatchFlushGuardsInvalidRectsAndKeepsSubmissionPlan)
{
	EXPECT_FALSE(UiBatchableRectHasPositiveSize(0.0f, 10.0f));
	EXPECT_FALSE(UiBatchableRectHasPositiveSize(10.0f, 0.0f));
	EXPECT_FALSE(UiBatchableRectHasPositiveSize(-1.0f, 10.0f));
	EXPECT_FALSE(UiBatchableRectHasPositiveSize(10.0f, -1.0f));
	EXPECT_TRUE(UiBatchableRectHasPositiveSize(1.0f, 1.0f));
	EXPECT_TRUE(UiBatchableRectHasPositiveSize(200.0f, 24.0f));

	EXPECT_FALSE(UiQuadBatchHasPendingSubmission(-1, 0));
	EXPECT_FALSE(UiQuadBatchHasPendingSubmission(-1, 3));
	EXPECT_FALSE(UiQuadBatchHasPendingSubmission(2, 0));
	EXPECT_TRUE(UiQuadBatchHasPendingSubmission(0, 1));
	EXPECT_TRUE(UiQuadBatchHasPendingSubmission(5, 4));

	const ColorRGBA Red(1.0f, 0.0f, 0.0f, 1.0f);
	const ColorRGBA Blue(0.0f, 0.0f, 1.0f, 1.0f);

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(false, -1, Red, -1, Blue);
		EXPECT_TRUE(Plan.m_LeavesBatchUntouched);
		EXPECT_FALSE(Plan.m_RenderImmediately);
		EXPECT_FALSE(Plan.m_FlushBeforeQueue);
		EXPECT_FALSE(Plan.m_QueueSprite);
	}

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(false, -1, Red, 7, Blue);
		EXPECT_FALSE(Plan.m_LeavesBatchUntouched);
		EXPECT_TRUE(Plan.m_RenderImmediately);
		EXPECT_FALSE(Plan.m_FlushBeforeQueue);
		EXPECT_FALSE(Plan.m_QueueSprite);
	}

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(true, -1, Red, 7, Blue);
		EXPECT_FALSE(Plan.m_LeavesBatchUntouched);
		EXPECT_FALSE(Plan.m_RenderImmediately);
		EXPECT_FALSE(Plan.m_FlushBeforeQueue);
		EXPECT_TRUE(Plan.m_QueueSprite);
	}

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(true, 7, Red, 7, Red);
		EXPECT_FALSE(Plan.m_LeavesBatchUntouched);
		EXPECT_FALSE(Plan.m_RenderImmediately);
		EXPECT_FALSE(Plan.m_FlushBeforeQueue);
		EXPECT_TRUE(Plan.m_QueueSprite);
	}

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(true, 7, Red, 8, Red);
		EXPECT_FALSE(Plan.m_LeavesBatchUntouched);
		EXPECT_FALSE(Plan.m_RenderImmediately);
		EXPECT_TRUE(Plan.m_FlushBeforeQueue);
		EXPECT_TRUE(Plan.m_QueueSprite);
	}

	{
		const SUiQuadBatchSubmissionPlan Plan = UiPlanQuadBatchSubmission(true, 7, Red, 7, Blue);
		EXPECT_FALSE(Plan.m_LeavesBatchUntouched);
		EXPECT_FALSE(Plan.m_RenderImmediately);
		EXPECT_TRUE(Plan.m_FlushBeforeQueue);
		EXPECT_TRUE(Plan.m_QueueSprite);
	}
}

TEST(QmMonitoringHelpers, SettingsPerfWindowAccumulatesFpsAndFrameStats)
{
	CQmSettingsPerfWindowTracker Tracker;
	Tracker.StartFixedFrameWindow("settings_open", "online", "settings:tee", "none", 30, false);

	Tracker.RecordFrame(0.010f, 12.0, false);
	Tracker.RecordFrame(0.020f, 30.0, false);
	Tracker.RecordFrame(0.0f, 99.0, false);
	Tracker.RecordFrame(-1.0f, 99.0, false);

	EXPECT_TRUE(Tracker.HasActiveWindow());
	const SQmSettingsPerfWindowSummary Summary = Tracker.FinishActiveWindow();

	EXPECT_STREQ(Summary.m_aOperation, "settings_open");
	EXPECT_STREQ(Summary.m_aContext, "online");
	EXPECT_STREQ(Summary.m_aPage, "settings:tee");
	EXPECT_EQ(Summary.m_SampleFrames, 2);
	EXPECT_NEAR(Summary.m_SampleSeconds, 0.030f, 0.0001f);
	EXPECT_NEAR(Summary.m_FpsAvg, 66.666f, 0.01f);
	EXPECT_NEAR(Summary.m_FpsMin, 50.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FpsMax, 100.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FpsOnePctLow, 50.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FrameMsAvg, 15.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FrameMsP95, 20.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FrameMsP99, 20.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FrameMsMax, 20.0f, 0.01f);
	EXPECT_NEAR(Summary.m_MenuMsMax, 30.0f, 0.01f);
	EXPECT_FALSE(Summary.m_CapLimited);
	EXPECT_FALSE(Tracker.HasActiveWindow());
}

TEST(QmMonitoringHelpers, SettingsPerfWindowLogsOnePercentLowFps)
{
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/settings_perf_windows.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");

	EXPECT_NE(Header.find("m_FpsOnePctLow"), std::string::npos);
	EXPECT_EQ(Header.find("m_FpsOnePctLow = m_Summary.m_FrameMsP99 > 0.0f ? 1000.0f / m_Summary.m_FrameMsP99 : 0.0f;"), std::string::npos);
	EXPECT_NE(Header.find("OnePercentLowFpsFromFrameMs"), std::string::npos);
	EXPECT_NE(Menus.find("fps_1pct_low=%.3f"), std::string::npos);
	EXPECT_NE(Menus.find("fps_1pct_source=real_sampled"), std::string::npos);
	EXPECT_NE(Menus.find("window_start_frame=%"), std::string::npos);
	EXPECT_NE(Menus.find("window_end_frame=%"), std::string::npos);
	EXPECT_NE(Menus.find("Summary.m_FpsOnePctLow"), std::string::npos);
}

// Intent: catch regression of the stable-text key mismatch root cause.
// Bug: plan-collection replay uses MenuTextSettingsContentView(Screen), visible render uses real MainView;
// sub-pixel Rect.w differences × 0.1-pixel bucket granularity (round_to_int(width * 10)) produced 8424
// key mismatches in the 2026-06-19 fresh log (qm_perf_2026-06-19_21-51-10_summary.json).
// Fix: MaxWidthBucket uses 4-pixel granularity (round_to_int(width / 4.0f) * 4) to tolerate sub-pixel drift;
// FontSize and UiScaleBucket keep 0.1 granularity (those values are stable across plan vs visible paths).
TEST(QmMonitoringHelpers, SettingsTextStyleKeyUsesFourPixelMaxWidthBucket)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");

	const size_t AssignPos = Menus.find("m_MaxWidthBucket =");
	ASSERT_NE(AssignPos, std::string::npos) << "BuildMenuTextStyleKey must assign m_MaxWidthBucket";
	const std::string Tail = Menus.substr(AssignPos, 200);

	EXPECT_NE(Tail.find("round_to_int(MaxWidth / 4.0f) * 4"), std::string::npos)
		<< "MaxWidthBucket must use 4-pixel granularity";
	EXPECT_EQ(Tail.find("MenuTextBucket(MaxWidth)"), std::string::npos)
		<< "MaxWidthBucket must not use 0.1-pixel MenuTextBucket anymore";
}

// Intent: catch regression of the stable-text double-bookkeeping bug.
// Bug: CScopedMenuTextVisibleGuard constructor unconditionally cleared per-frame counters; outer shell guard
// (menus.cpp RenderMenuShell) and inner content guard (menus_settings.cpp RenderSettings) each constructed
// one. Inner constructor wiped outer's accumulation; both destructors logged when candidates > 0, producing
// 2x duplicate settings_text_usage payloads (6/18 hit-rate audit found 2:269 duplication histogram).
// Fix: guard uses stack semantics — only stack bottom (m_Previous == false) clears counters on construct
// and flushes log on destruct; nested guards inherit parent's counters and add their own on top.
TEST(QmMonitoringHelpers, SettingsTextVisibleGuardUsesStackSemantics)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");

	const size_t CtorPos = Menus.find("CScopedMenuTextVisibleGuard::CScopedMenuTextVisibleGuard");
	ASSERT_NE(CtorPos, std::string::npos);
	const std::string CtorTail = Menus.substr(CtorPos, 1500);

	EXPECT_NE(CtorTail.find("if(!m_Previous)"), std::string::npos)
		<< "Constructor must gate counter reset on stack-bottom (m_Previous == false)";

	const size_t DtorPos = Menus.find("CScopedMenuTextVisibleGuard::~CScopedMenuTextVisibleGuard");
	ASSERT_NE(DtorPos, std::string::npos);
	const std::string DtorTail = Menus.substr(DtorPos, 800);

	EXPECT_NE(DtorTail.find("if(!m_Previous && m_pMenus->m_MenuTextStableCandidatesThisFrame > 0)"), std::string::npos)
		<< "Destructor must gate log on stack-bottom (m_Previous == false)";
}

TEST(QmMonitoringHelpers, RealOnePctLowUsesFrameSamples)
{
	CQmSettingsPerfWindowTracker Tracker;
	Tracker.StartFixedFrameWindow("settings_tab_switch", "offline", "settings:assets", "entity_bg", 100, false, 1000);
	SQmSettingsPerfWindowFrameResult Result;
	for(int i = 0; i < 99; ++i)
		Result = Tracker.RecordFrame(0.001f, 1.0, false, 1001 + i);
	Result = Tracker.RecordFrame(0.100f, 100.0, false, 1100);
	ASSERT_TRUE(Result.m_ShouldFlush);
	const SQmSettingsPerfWindowSummary Summary = Result.m_Summary;

	EXPECT_EQ(Summary.m_WindowStartFrame, 1000);
	EXPECT_EQ(Summary.m_WindowEndFrame, 1100);
	EXPECT_NEAR(Summary.m_FrameMsP99, 1.0f, 0.01f);
	EXPECT_NEAR(Summary.m_FpsOnePctLow, 10.0f, 0.01f);
	EXPECT_NE(Summary.m_FpsOnePctLow, Summary.m_FrameMsP99 > 0.0f ? 1000.0f / Summary.m_FrameMsP99 : 0.0f);
}

TEST(QmMonitoringHelpers, SettingsPerfWindowEndsAfterFixedFrameBudget)
{
	CQmSettingsPerfWindowTracker Tracker;
	Tracker.StartFixedFrameWindow("settings_tab_switch", "offline", "settings:tclient", "0", 3, true);

	EXPECT_FALSE(Tracker.RecordFrame(0.010f, 2.0, false).m_ShouldFlush);
	EXPECT_FALSE(Tracker.RecordFrame(0.010f, 3.0, false).m_ShouldFlush);
	const SQmSettingsPerfWindowFrameResult Result = Tracker.RecordFrame(0.010f, 4.0, false);

	ASSERT_TRUE(Result.m_ShouldFlush);
	EXPECT_EQ(Result.m_Summary.m_SampleFrames, 3);
	EXPECT_STREQ(Result.m_Summary.m_aOperation, "settings_tab_switch");
	EXPECT_STREQ(Result.m_Summary.m_aContext, "offline");
	EXPECT_STREQ(Result.m_Summary.m_aPage, "settings:tclient");
	EXPECT_STREQ(Result.m_Summary.m_aTab, "0");
	EXPECT_TRUE(Result.m_Summary.m_CapLimited);
	EXPECT_FALSE(Tracker.HasActiveWindow());
}

TEST(QmMonitoringHelpers, SettingsPerfScrollWindowEndsAfterIdleTimeout)
{
	CQmSettingsPerfWindowTracker Tracker;
	Tracker.StartScrollWindow("settings_tee_scroll", "offline", "settings:tee", "none", 0.250f, false);

	EXPECT_FALSE(Tracker.RecordFrame(0.016f, 5.0, true).m_ShouldFlush);
	EXPECT_FALSE(Tracker.RecordFrame(0.100f, 6.0, false).m_ShouldFlush);
	EXPECT_FALSE(Tracker.RecordFrame(0.149f, 7.0, false).m_ShouldFlush);
	const SQmSettingsPerfWindowFrameResult Result = Tracker.RecordFrame(0.001f, 8.0, false);

	ASSERT_TRUE(Result.m_ShouldFlush);
	EXPECT_EQ(Result.m_Summary.m_SampleFrames, 4);
	EXPECT_STREQ(Result.m_Summary.m_aOperation, "settings_tee_scroll");
	EXPECT_NEAR(Result.m_Summary.m_MenuMsMax, 8.0f, 0.01f);
	EXPECT_FALSE(Tracker.HasActiveWindow());
}

TEST(QmMonitoringHelpers, SettingsPerfWindowStartFlushesInterruptedWindow)
{
	CQmSettingsPerfWindowTracker Tracker;
	Tracker.StartFixedFrameWindow("settings_open", "offline", "settings:tee", "none", 30, false);
	Tracker.RecordFrame(0.016f, 7.0, false);
	Tracker.RecordFrame(0.017f, 8.0, false);

	const SQmSettingsPerfWindowFrameResult Interrupted = Tracker.StartScrollWindow("settings_tee_scroll", "offline", "settings:tee", "none", 0.250f, false);

	ASSERT_TRUE(Interrupted.m_ShouldFlush);
	EXPECT_STREQ(Interrupted.m_Summary.m_aOperation, "settings_open");
	EXPECT_EQ(Interrupted.m_Summary.m_SampleFrames, 2);
	EXPECT_TRUE(Tracker.HasActiveWindow());
	EXPECT_STREQ(Tracker.ActiveOperation(), "settings_tee_scroll");
}

TEST(QmMonitoringHelpers, SettingsOpenWindowIsProtectedFromStalePreviousSettingsState)
{
	const std::string MenusSource = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const std::string SettingsSource = ReadTestSourceFile("src/game/client/components/menus_settings.cpp");

	EXPECT_NE(MenusSource.find("OldPage != NewPage && NewPage == PAGE_SETTINGS"), std::string::npos);
	EXPECT_NE(MenusSource.find("m_SettingsPerfLastPage = -1;"), std::string::npos);
	EXPECT_NE(SettingsSource.find("if(m_SettingsPerfLastPage != -1)"), std::string::npos);
	EXPECT_NE(SettingsSource.find("StartSettingsPerfFixedWindow(\"settings_tab_switch\""), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsPerfWindowFlushesActiveSamplesOnShutdown)
{
	const std::string MenusSource = ReadTestSourceFile("src/game/client/components/menus.cpp");

	EXPECT_NE(MenusSource.find("void CMenus::OnShutdown()"), std::string::npos);
	EXPECT_NE(MenusSource.find("if(m_SettingsPerfWindowTracker.HasActiveWindow())"), std::string::npos);
	EXPECT_NE(MenusSource.find("const SQmSettingsPerfWindowSummary Summary = m_SettingsPerfWindowTracker.FinishActiveWindow();"), std::string::npos);
	EXPECT_NE(MenusSource.find("LogSettingsPerfWindowSummary(Summary);"), std::string::npos);
}

namespace
{

	template<typename TPredicate>
	bool WaitUntil(TPredicate Predicate, std::chrono::milliseconds Timeout = std::chrono::milliseconds(200))
	{
		const auto Deadline = std::chrono::steady_clock::now() + Timeout;
		while(std::chrono::steady_clock::now() < Deadline)
		{
			if(Predicate())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return Predicate();
	}

} // namespace

TEST(QmMonitoringHelpers, DevicePerfSnapshotCacheReturnsConsistentVersionedSnapshot)
{
	CQmDevicePerfSnapshotCache Cache;

	SQmDevicePerfSample First;
	First.m_GpuUtilPct = 11.0f;
	First.m_GpuDedicatedVramMb = 101.0f;
	First.m_GpuDedicatedVramBudgetMb = 2048.0f;
	First.m_Available = true;
	const SQmDevicePerfSnapshot FirstSnapshot = Cache.Publish(First);

	SQmDevicePerfSample Second;
	Second.m_GpuUtilPct = 27.5f;
	Second.m_GpuDedicatedVramMb = 205.0f;
	Second.m_GpuDedicatedVramBudgetMb = 4096.0f;
	Second.m_GpuSharedVramMb = 17.0f;
	Second.m_DiskReadMbPerSec = 3.5f;
	Second.m_Available = true;
	const SQmDevicePerfSnapshot Published = Cache.Publish(Second);

	const SQmDevicePerfSnapshot Read = Cache.Snapshot();
	EXPECT_GT(Published.m_Version, FirstSnapshot.m_Version);
	EXPECT_EQ(Read.m_Version, Published.m_Version);
	EXPECT_FLOAT_EQ(Read.m_Sample.m_GpuUtilPct, Second.m_GpuUtilPct);
	EXPECT_FLOAT_EQ(Read.m_Sample.m_GpuDedicatedVramMb, Second.m_GpuDedicatedVramMb);
	EXPECT_FLOAT_EQ(Read.m_Sample.m_GpuDedicatedVramBudgetMb, Second.m_GpuDedicatedVramBudgetMb);
	EXPECT_FLOAT_EQ(Read.m_Sample.m_GpuSharedVramMb, Second.m_GpuSharedVramMb);
	EXPECT_FLOAT_EQ(Read.m_Sample.m_DiskReadMbPerSec, Second.m_DiskReadMbPerSec);
	EXPECT_EQ(Read.m_Sample.m_Available, Second.m_Available);
}

TEST(QmMonitoringHelpers, DevicePerfSnapshotCacheKeepsSampleAndVersionConsistentAcrossThreads)
{
	CQmDevicePerfSnapshotCache Cache;
	std::atomic<bool> Stop{false};
	std::atomic<int> MismatchCount{0};

	std::thread Writer([&]() {
		for(uint64_t Version = 1; Version <= 2000; ++Version)
		{
			SQmDevicePerfSample Sample;
			Sample.m_GpuUtilPct = (float)Version;
			Sample.m_GpuDedicatedVramMb = (float)Version * 2.0f;
			Sample.m_GpuDedicatedVramBudgetMb = (float)Version * 4.0f;
			Sample.m_Available = true;
			Cache.Publish(Sample);
		}
		Stop.store(true, std::memory_order_release);
	});

	std::thread Reader([&]() {
		while(!Stop.load(std::memory_order_acquire))
		{
			const SQmDevicePerfSnapshot Snapshot = Cache.Snapshot();
			if(Snapshot.m_Version == 0)
				continue;
			if(Snapshot.m_Sample.m_GpuUtilPct != (float)Snapshot.m_Version ||
				Snapshot.m_Sample.m_GpuDedicatedVramMb != (float)Snapshot.m_Version * 2.0f ||
				Snapshot.m_Sample.m_GpuDedicatedVramBudgetMb != (float)Snapshot.m_Version * 4.0f)
			{
				MismatchCount.fetch_add(1, std::memory_order_relaxed);
			}
		}
	});

	Writer.join();
	Reader.join();
	EXPECT_EQ(MismatchCount.load(std::memory_order_relaxed), 0);
}

TEST(QmMonitoringHelpers, DevicePerfSamplerStateStopsWorkerOnDisableAndCanRestart)
{
	std::atomic<int> SampleCalls{0};
	auto SampleFn = [&SampleCalls]() {
		SQmDevicePerfSample Sample;
		Sample.m_GpuUtilPct = (float)SampleCalls.fetch_add(1, std::memory_order_relaxed) + 1.0f;
		Sample.m_Available = true;
		return Sample;
	};

	CQmAsyncDevicePerfSampler Sampler(SampleFn, std::chrono::milliseconds(5));
	QmUpdateDevicePerfSamplerState(Sampler, false);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	EXPECT_EQ(SampleCalls.load(std::memory_order_relaxed), 0);

	QmUpdateDevicePerfSamplerState(Sampler, true);
	ASSERT_TRUE(WaitUntil([&]() { return SampleCalls.load(std::memory_order_relaxed) >= 2; }));

	QmUpdateDevicePerfSamplerState(Sampler, false);
	const int CallsAfterDisable = SampleCalls.load(std::memory_order_relaxed);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	EXPECT_EQ(SampleCalls.load(std::memory_order_relaxed), CallsAfterDisable);
	const SQmDevicePerfSnapshot ClearedSnapshot = Sampler.Snapshot();
	EXPECT_EQ(ClearedSnapshot.m_Version, 0u);
	EXPECT_FALSE(ClearedSnapshot.m_Sample.m_Available);
	EXPECT_FLOAT_EQ(ClearedSnapshot.m_Sample.m_GpuUtilPct, -1.0f);

	QmUpdateDevicePerfSamplerState(Sampler, true);
	ASSERT_TRUE(WaitUntil([&]() { return SampleCalls.load(std::memory_order_relaxed) > CallsAfterDisable; }));
	Sampler.Stop();
}

TEST(QmMonitoringHelpers, DiskReadRateUsesMegabytesPerSecond)
{
	EXPECT_FLOAT_EQ(QmComputeDiskReadMbPerSec(0, 0, 1024 * 1024, 1000000000ull), -1.0f);
	EXPECT_FLOAT_EQ(QmComputeDiskReadMbPerSec(0, 1000000000ull, 1024 * 1024, 2000000000ull), 1.0f);
	EXPECT_FLOAT_EQ(QmComputeDiskReadMbPerSec(1024, 2000000000ull, 1024, 3000000000ull), 0.0f);
	EXPECT_FLOAT_EQ(QmComputeDiskReadMbPerSec(2048, 5000000000ull, 1024, 4000000000ull), -1.0f);
	EXPECT_FLOAT_EQ(QmComputeDiskReadMbPerSec(1024, 1000000000ull, 2048, 1000000000ull), -1.0f);
}

TEST(QmMonitoringHelpers, PerfConfigDefaultsUseLowThresholdWithoutJsonToggle)
{
	std::ifstream File(TestSourcePath("src/engine/shared/config_variables_qmclient.h"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("MACRO_CONFIG_INT(QmPerfDebugThresholdMs, qm_perf_debug_threshold_ms, 4, 1, 1000"), std::string::npos);
	EXPECT_EQ(Source.find("MACRO_CONFIG_INT(QmPerfJson, qm_perf_json, 0, 0, 1"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfDurationGateUsesConfiguredThreshold)
{
	const int OldThreshold = g_Config.m_QmPerfDebugThresholdMs;
	g_Config.m_QmPerfDebugThresholdMs = 4;

	EXPECT_FALSE(QmPerfShouldLogDuration(3.999));
	EXPECT_TRUE(QmPerfShouldLogDuration(4.0));
	EXPECT_TRUE(QmPerfShouldLogDuration(0.0, true));

	g_Config.m_QmPerfDebugThresholdMs = OldThreshold;
}

TEST(QmMonitoringHelpers, ProcessHighPriorityConfigExistsAndDefaultsOff)
{
	std::ifstream File(TestSourcePath("src/engine/shared/config_variables_qmclient.h"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("MACRO_CONFIG_INT(QmProcessHighPriority, qm_process_high_priority, 0, 0, 1"), std::string::npos);
	EXPECT_NE(Source.find("MACRO_CONFIG_INT(QmAssetsPreviewBudgetMbOverride, qm_assets_preview_budget_mb_override, 0, 0, 16384"), std::string::npos);
	EXPECT_NE(Source.find("MACRO_CONFIG_INT(QmAssetsPreviewBudgetPercent, qm_assets_preview_budget_percent, 8, 0, 100"), std::string::npos);
}

TEST(QmMonitoringHelpers, WindowsStartupPriorityHookIsOptionalAndGuarded)
{
	std::ifstream File(TestSourcePath("src/engine/client/client.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("#if defined(CONF_FAMILY_WINDOWS)"), std::string::npos);
	EXPECT_NE(Source.find("if(g_Config.m_QmProcessHighPriority)"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmProcessHighPriority ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS"), std::string::npos);
	EXPECT_NE(Source.find("SetPriorityClass(GetCurrentProcess(), PriorityClass)"), std::string::npos);
	EXPECT_NE(Source.find("m_pConsole->Chain(\"qm_process_high_priority\", ConchainProcessHighPriority, this);"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfLoggingAlwaysEmitsJsonPayload)
{
	std::ifstream File(TestSourcePath("src/game/client/components/qmclient/perf_logging.h"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_EQ(Source.find("if(g_Config.m_QmPerfJson == 0)"), std::string::npos);
	EXPECT_NE(Source.find("str_copy(aJson, \"{\", sizeof(aJson));"), std::string::npos);
	EXPECT_NE(Source.find("dbg_msg(pSystem, \"%s\", aJson);"), std::string::npos);
	EXPECT_NE(Source.find("if(!QmPerfShouldLogDuration(DurationMs, Force))"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfPayloadJsonFieldsPreserveSpaceContainingValues)
{
	char aJson[1024];
	bool First = true;
	str_copy(aJson, "{", sizeof(aJson));
	QmPerfAppendPayloadJsonFields(aJson, sizeof(aJson), First, "event=source_request skin=My Skin Name priority=visible first_visible_skin=Another Skin");
	str_append(aJson, "}", sizeof(aJson));

	EXPECT_NE(str_find(aJson, "\"skin\":\"My Skin Name\""), nullptr);
	EXPECT_NE(str_find(aJson, "\"priority\":\"visible\""), nullptr);
	EXPECT_NE(str_find(aJson, "\"first_visible_skin\":\"Another Skin\""), nullptr);
}

TEST(QmMonitoringHelpers, RuntimePerfCallsitesUseSharedLoggingHelpers)
{
	for(const char *pPath : {
		    "src/game/client/components/countryflags.cpp",
		    "src/game/client/components/menus.cpp",
		    "src/game/client/components/menus_settings_assets.cpp",
		    "src/game/client/components/section_loader.cpp",
		    "src/game/client/components/qmclient/menus_qmclient.cpp",
		    "src/game/client/components/tclient/menus_tclient.cpp",
	    })
	{
		std::ifstream File(TestSourcePath(pPath));
		ASSERT_TRUE(File.good()) << pPath;
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();
		EXPECT_EQ(Source.find("dbg_msg(\"perf/"), std::string::npos) << pPath;
	}
}

TEST(QmMonitoringHelpers, QmClientModuleDragGhostUsesPressAnchor)
{
	std::ifstream File(TestSourcePath("src/game/client/components/qmclient/menus_qmclient.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PressBlockPos = Source.find("if(!InteractionBlocked && Ui()->MouseButtonClicked(0) && OverHeader)");
	ASSERT_NE(PressBlockPos, std::string::npos);
	const size_t DragGuardPos = Source.find("if(s_DragState.m_pDragging != pModule)", PressBlockPos);
	ASSERT_NE(DragGuardPos, std::string::npos);
	const std::string PressBlock = Source.substr(PressBlockPos, DragGuardPos - PressBlockPos);

	EXPECT_NE(PressBlock.find("s_DragState.m_pDragging = pModule;"), std::string::npos);
	EXPECT_NE(PressBlock.find("s_DragState.m_GrabOffset = vec2(Ui()->MouseX() - CardRect.x, Ui()->MouseY() - CardRect.y);"), std::string::npos);
	EXPECT_NE(PressBlock.find("s_DragState.m_DraggedWidth = CardRect.w;"), std::string::npos);
	EXPECT_NE(PressBlock.find("s_DragState.m_DraggedHeight = CardRect.h;"), std::string::npos);
	EXPECT_NE(PressBlock.find("s_DragState.m_HasDragAnchor = true;"), std::string::npos);
	EXPECT_EQ(Source.find("if(!InteractionBlocked && s_DragState.m_pPressed == pModule && Ui()->MouseButton(0) && s_DragState.m_pDragging == nullptr)"), std::string::npos);
	EXPECT_NE(Source.find("if(s_DragState.m_pDragging == nullptr || !s_DragState.m_HasDragAnchor)"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuTextPoolReplacesSettingsOnlyBoundary)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_TRUE(ContainsAll(Header, {"m_MenuTextPool", "SMenuTextPoolEntry", "SMenuTextStyleKey", "MenuTextElement(", "DoMenuLabelStreamed(", "PrebuildSettingsMenuTextPool("}));
	EXPECT_TRUE(ContainsAll(Source, {"SettingsTextElement", "MenuTextElement", "MENU_TEXT_SCOPE_SETTINGS"}));
	EXPECT_EQ(Header.find("std::unordered_map<std::string, SSettingsTextPoolEntry> m_SettingsTextPool"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsStableTextMissAndStaleBlockVisibleBuild)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_TRUE(ContainsAll(Source, {"event=settings_text_miss", "event=settings_text_stale", "m_MenuTextPoolVisibleGuard", "StableTextMiss", "StableTextStale"}));
	EXPECT_TRUE(ContainsAll(Source, {"event=settings_text_usage", "m_MenuTextStableCandidatesThisFrame", "m_MenuTextStableHitsThisFrame", "m_MenuTextStableReusedThisFrame"}));
	EXPECT_TRUE(ContainsAll(Source, {"MenuTextPoolSizeForTesting", "CScopedMenuTextVisibleGuard"}));
	EXPECT_EQ(Source.find("context-checkbox-common-"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Source, {
						"scope=%s page=%s tab=%d subtab=%d key=%s reason=%s plan_status=%s operation=%s frame=%",
						"%s:%d:%d:%d:%s:fs%d:al%d:mw%d:us%d:hd%d:cm%d",
						"StyleKey.m_Align",
						"StyleKey.m_MaxWidthBucket",
						"StyleKey.m_HiDpiScaleBucket",
						"StyleKey.m_CompactMode",
					}));
	EXPECT_TRUE(ContainsAll(Source, {
						"const char *CMenus::SettingsPerfStableTextScope(int Page) const",
						"str_comp(SettingsPerfActiveOperation(), \"ingame_esc_open\") == 0",
						"(void)Page;",
						"return str_comp(pActivePage, aPage) == 0 ? \"target_settings\" : \"settings\";",
						"SettingsPerfStableTextScope(Page)",
					}));
	EXPECT_TRUE(ContainsAll(Source, {"LogSettingsTextPoolCoverageGap(Client(), \"settings_text_miss\", Scope, SettingsPerfStableTextScope(Page), Page, Tab, Subtab", "LogSettingsTextPoolCoverageGap(Client(), \"settings_text_stale\", Scope, SettingsPerfStableTextScope(Page), Page, Tab, Subtab"}));
	EXPECT_TRUE(ContainsAll(Source, {"case ESettingsInvalidationReason::DPI_CHANGED: return \"dpi\";", "case ESettingsInvalidationReason::UI_SCALE_CHANGED: return \"ui_scale\";"}));

	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	EXPECT_TRUE(ContainsAll(Header, {
						"void DoSettingsMenuLabel(int Page, int Tab, int Subtab",
						"int DoSettingsButton_Menu(int Page, int Tab, int Subtab, CButtonContainer *pBC",
						"int DoSettingsButton_CheckBox(int Page, int Tab, int Subtab",
						"bool DoSettingsScrollbarOption(int Page, int Tab, int Subtab",
					}));
	EXPECT_NE(Source.find("CUIElement &TextElement = MenuTextElement(MENU_TEXT_SCOPE_SETTINGS, Page, Tab, Subtab, pTextId, StyleKey);"), std::string::npos);
	EXPECT_NE(Source.find("dbg_assert(pBC != nullptr, \"settings menu button requires a stable button container\")"), std::string::npos);
	EXPECT_EQ(Header.find("CButtonContainer *pBC = nullptr"), std::string::npos);
	EXPECT_EQ(Source.find("s_FallbackButton"), std::string::npos);
	EXPECT_NE(Source.find("DoButton_Menu(pBC, pText, Checked, pRect, Flags, nullptr, Corners, Rounding, FontFactor, Color, &TextElement)"), std::string::npos);
	EXPECT_EQ(Source.find("reason=%s\", pReason != nullptr ? pReason : \"unknown\""), std::string::npos);

	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	EXPECT_TRUE(ContainsAll(TClient, {
						 "DoSettingsButton_Menu(SETTINGS_TCLIENT, TCLIENT_TAB_BINDWHEEL, TCLIENT_TAB_BINDWHEEL, &s_OverrideButton",
						 "DoSettingsButton_Menu(SETTINGS_TCLIENT, TCLIENT_TAB_BINDWHEEL, TCLIENT_TAB_BINDWHEEL, &s_AddButton",
						 "DoSettingsButton_Menu(SETTINGS_TCLIENT, TCLIENT_TAB_BINDWHEEL, TCLIENT_TAB_BINDWHEEL, &s_RemoveButton",
					 }));
}

TEST(QmMonitoringHelpers, MenuTextPoolStaleRefreshDoesNotReinitRegisteredElement)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const size_t StaleBranch = Source.find("else if(It->second.m_Generation != m_MenuTextPoolGeneration)");
	ASSERT_NE(StaleBranch, std::string::npos);
	const size_t ReturnElement = Source.find("return It->second.m_Element;", StaleBranch);
	ASSERT_NE(ReturnElement, std::string::npos);
	const std::string Body = Source.substr(StaleBranch, ReturnElement - StaleBranch);

	EXPECT_NE(Body.find("Ui()->ResetUIElement(It->second.m_Element);"), std::string::npos);
	EXPECT_EQ(Body.find("It->second.m_Element.Init(Ui(), 1);"), std::string::npos);
}

TEST(QmMonitoringHelpers, StableSettingsHelpersRequireExplicitTextIds)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_EQ(Source.find("ctx-scrollbar-"), std::string::npos);
	EXPECT_EQ(Source.find("ctx-checkbox-"), std::string::npos);
	EXPECT_EQ(Source.find("ctx-menu-label-"), std::string::npos);
	EXPECT_EQ(Source.find("ctx-menu-button-"), std::string::npos);
	EXPECT_EQ(Source.find("ctx-label-"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientStableTextCandidateAuditIsEmptyExceptAllowlist)
{
	const char *pFile = "src/game/client/components/tclient/menus_tclient.cpp";
	const std::string Source = ReadRepoFile(pFile);
	const std::vector<SStableTextCandidate> vCandidates = CollectRawStableTextCandidatesWithLines(Source);
	const std::vector<SStableTextRawAllow> vAllowlist = {
		{pFile, 3464, "user-generated"},
		{pFile, 3803, "dynamic-value"},
		{pFile, 3805, "localized-list-data"},
		{pFile, 3933, "localized-list-data"},
		{pFile, 3994, "localized-list-data"},
		{pFile, 4106, "user-generated"},
		{pFile, 4118, "user-generated"},
		{pFile, 4180, "localized-list-data"},
		{pFile, 4190, "dynamic-value"},
		{pFile, 4222, "localized-list-data"},
		{pFile, 4229, "dynamic-value"},
		{pFile, 4283, "localized-list-data"},
		{pFile, 4744, "dynamic-value"},
		{pFile, 4747, "dynamic-value"},
		{pFile, 4750, "dynamic-value"},
		{pFile, 4755, "user-generated"},
		{pFile, 4757, "user-generated"},
		{pFile, 5126, "dynamic-value"},
		{pFile, 5459, "localized-list-data"},
		{pFile, 5496, "localized-list-data"},
		{pFile, 5512, "localized-list-data"},
	};
	const std::vector<SStableTextCandidate> vUnexpected = FilterCandidatesNotCoveredByMenuPoolOrAllowlist(pFile, vCandidates, vAllowlist);
	EXPECT_TRUE(vUnexpected.empty()) << JoinCandidates(vUnexpected);
}

TEST(QmMonitoringHelpers, QmClientStableTextCandidateAuditIsEmptyExceptAllowlist)
{
	const char *pFile = "src/game/client/components/qmclient/menus_qmclient.cpp";
	const std::string Source = ReadRepoFile(pFile);
	const std::vector<SStableTextCandidate> vCandidates = CollectRawStableTextCandidatesWithLines(Source);
	const std::vector<SStableTextRawAllow> vAllowlist = {
		{pFile, 765, "stateful-new-label"},
		{pFile, 757, "stateful-new-label"},
		{pFile, 942, "animated-style"},
		{pFile, 943, "animated-style"},
		{pFile, 950, "animated-style"},
		{pFile, 951, "animated-style"},
		{pFile, 954, "animated-style"},
		{pFile, 958, "animated-style"},
		{pFile, 959, "animated-style"},
		{pFile, 961, "animated-style"},
		{pFile, 962, "animated-style"},
		{pFile, 963, "animated-style"},
		{pFile, 966, "animated-style"},
		{pFile, 967, "animated-style"},
		{pFile, 968, "animated-style"},
		{pFile, 969, "animated-style"},
		{pFile, 970, "animated-style"},
		{pFile, 976, "animated-style"},
		{pFile, 985, "animated-style"},
		{pFile, 993, "animated-style"},
		{pFile, 1989, "dynamic-value"},
		{pFile, 2155, "icon-only"},
		{pFile, 2482, "animated-style"},
		{pFile, 2483, "status-message"},
		{pFile, 2490, "animated-style"},
		{pFile, 2491, "status-message"},
		{pFile, 3461, "localized-list-data"},
		{pFile, 4315, "localized-list-data"},
		{pFile, 4677, "status-message"},
		{pFile, 4681, "user-generated"},
		{pFile, 4689, "user-generated"},
		{pFile, 4954, "status-message"},
		{pFile, 5185, "stateful-new-label"},
		{pFile, 5181, "stateful-new-label"},
		{pFile, 5193, "stateful-new-label"},
		{pFile, 5771, "status-message"},
		{pFile, 5767, "status-message"},
		{pFile, 5779, "status-message"},
		{pFile, 6259, "status-message"},
		{pFile, 6260, "status-message"},
		{pFile, 952, "animated-style"},
		{pFile, 953, "animated-style"},
		{pFile, 960, "animated-style"},
		{pFile, 964, "animated-style"},
		{pFile, 971, "animated-style"},
		{pFile, 972, "animated-style"},
		{pFile, 974, "animated-style"},
		{pFile, 978, "animated-style"},
		{pFile, 981, "animated-style"},
		{pFile, 982, "animated-style"},
		{pFile, 995, "animated-style"},
		{pFile, 2501, "animated-style"},
		{pFile, 2502, "animated-style"},
		{pFile, 4700, "localized-list-data"},
		{pFile, 5204, "stateful-new-label"},
		{pFile, 5790, "status-message"},
		{pFile, 949, "animated-style"},
		{pFile, 957, "animated-style"},
		{pFile, 965, "animated-style"},
		{pFile, 975, "animated-style"},
		{pFile, 977, "animated-style"},
		{pFile, 979, "animated-style"},
		{pFile, 980, "animated-style"},
		{pFile, 992, "animated-style"},
		{pFile, 2611, "animated-style"},
		{pFile, 2612, "user-generated"},
		{pFile, 7204, "stateful-new-label"},
		{pFile, 983, "dynamic-value"},
		{pFile, 990, "dynamic-value"},
		{pFile, 994, "dynamic-value"},
		{pFile, 998, "dynamic-value"},
		{pFile, 999, "dynamic-value"},
		{pFile, 1001, "dynamic-value"},
		{pFile, 1002, "dynamic-value"},
		{pFile, 984, "dynamic-value"},
		{pFile, 986, "dynamic-value"},
		{pFile, 987, "dynamic-value"},
		{pFile, 989, "dynamic-value"},
		{pFile, 996, "dynamic-value"},
		{pFile, 1000, "dynamic-value"},
		{pFile, 1003, "dynamic-value"},
		{pFile, 1004, "dynamic-value"},
		{pFile, 1045, "dynamic-value"},
		{pFile, 1046, "dynamic-value"},
		{pFile, 1053, "dynamic-value"},
		{pFile, 1057, "dynamic-value"},
		{pFile, 1061, "dynamic-value"},
		{pFile, 1062, "dynamic-value"},
		{pFile, 1064, "dynamic-value"},
		{pFile, 1065, "dynamic-value"},
		{pFile, 1047, "dynamic-value"},
		{pFile, 1048, "dynamic-value"},
		{pFile, 1049, "dynamic-value"},
		{pFile, 1050, "dynamic-value"},
		{pFile, 1054, "dynamic-value"},
		{pFile, 1056, "dynamic-value"},
		{pFile, 1058, "dynamic-value"},
		{pFile, 1060, "dynamic-value"},
		{pFile, 1063, "dynamic-value"},
		{pFile, 1066, "dynamic-value"},
		{pFile, 1067, "dynamic-value"},
		{pFile, 1068, "dynamic-value"},
		{pFile, 1069, "dynamic-value"},
		{pFile, 1306, "dynamic-value"},
		{pFile, 1307, "dynamic-value"},
		{pFile, 1313, "dynamic-value"},
		{pFile, 1317, "dynamic-value"},
		{pFile, 1321, "dynamic-value"},
		{pFile, 1322, "dynamic-value"},
		{pFile, 1324, "dynamic-value"},
		{pFile, 1325, "dynamic-value"},
		{pFile, 1308, "dynamic-value"},
		{pFile, 1309, "dynamic-value"},
		{pFile, 1315, "dynamic-value"},
		{pFile, 1319, "dynamic-value"},
		{pFile, 1323, "dynamic-value"},
		{pFile, 1326, "dynamic-value"},
		{pFile, 1327, "dynamic-value"},
		{pFile, 1331, "dynamic-value"},
		{pFile, 1332, "dynamic-value"},
		{pFile, 1333, "dynamic-value"},
		{pFile, 1337, "dynamic-value"},
		{pFile, 1338, "dynamic-value"},
		{pFile, 1344, "dynamic-value"},
		{pFile, 1345, "dynamic-value"},
		{pFile, 1346, "dynamic-value"},
		{pFile, 1347, "dynamic-value"},
		{pFile, 1341, "dynamic-value"},
		{pFile, 1342, "dynamic-value"},
		{pFile, 1348, "dynamic-value"},
		{pFile, 1349, "dynamic-value"},
		{pFile, 1350, "dynamic-value"},
		{pFile, 1352, "dynamic-value"},
		{pFile, 1356, "dynamic-value"},
		{pFile, 1357, "dynamic-value"},
		{pFile, 1359, "dynamic-value"},
		{pFile, 1360, "dynamic-value"},
	};
	const std::vector<SStableTextCandidate> vUnexpected = FilterCandidatesNotCoveredByMenuPoolOrAllowlist(pFile, vCandidates, vAllowlist);
	EXPECT_TRUE(vUnexpected.empty()) << JoinCandidates(vUnexpected);
	EXPECT_FALSE(IsPooledStableTextLine("Ui()->DoLabel(&TitleRect, QmNewFeatureLabel(pTitle, pNewFeatureId, aTitle, sizeof(aTitle)), LgHeadlineSizeNew, TEXTALIGN_ML);"));
	EXPECT_FALSE(IsPooledStableTextLine("Ui()->DoLabel(&Label, RainbowColor(), LgBodySize, TEXTALIGN_ML);"));
	EXPECT_FALSE(IsPooledStableTextLine("RenderQmModuleHeadline(View, pTitle, pTip, true);"));
}

TEST(QmMonitoringHelpers, BaseSettingsStableTextCandidateAuditIsEmptyExceptAllowlist)
{
	const char *pFile = "src/game/client/components/menus_settings.cpp";
	const std::string Source = ReadRepoFile(pFile);
	const std::vector<SStableTextCandidate> vCandidates = CollectRawStableTextCandidatesWithLines(Source);
	const std::vector<SStableTextRawAllow> vAllowlist = {
		{pFile, 472, "animated-style"},
		{pFile, 1446, "input-text"},
		{pFile, 1759, "dynamic-value"},
		{pFile, 1790, "localized-list-data"},
		{pFile, 1935, "localized-list-data"},
		{pFile, 1971, "localized-list-data"},
		{pFile, 1980, "localized-list-data"},
		{pFile, 1989, "localized-list-data"},
		{pFile, 2680, "search-result"},
		{pFile, 3590, "localized-list-data"},
		{pFile, 3644, "input-text"},
		{pFile, 3648, "input-text"},
		{pFile, 3838, "input-text"},
		{pFile, 4135, "localized-list-data"},
		{pFile, 4804, "status-message"},
		{pFile, 4809, "status-message"},
		{pFile, 6427, "input-text"},
		{pFile, 474, "localized-list-data"},
		{pFile, 1761, "dynamic-value"},
		{pFile, 1973, "localized-list-data"},
		{pFile, 1982, "localized-list-data"},
		{pFile, 1991, "localized-list-data"},
		{pFile, 4890, "status-message"},
		{pFile, 4895, "status-message"},
		{pFile, 6513, "input-text"},
	};
	const std::vector<SStableTextCandidate> vUnexpected = FilterCandidatesNotCoveredByMenuPoolOrAllowlist(pFile, vCandidates, vAllowlist);
	EXPECT_TRUE(vUnexpected.empty()) << JoinCandidates(vUnexpected);
}

TEST(QmMonitoringHelpers, SettingsStaticLabelsUseTextElementCache)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_TEE, -1, \"tee-name-label\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_TEE, -1, \"tee-clan-label\")"), std::string::npos);
		// DDNet 标题（demo/ghost/gameplay/background/miscellaneous）已迁移到 card deck（BeginSettingsCardDeckCard），不再用独立 SettingsTextElement title
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_DDNET, -1, \"ddnet-run-on-join-label\")"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_DDNET, -1, &s_UseCurrentMapId, \"Use current map as background\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_DDNET, -1, &g_Config.m_ClBackgroundShowTilesLayers, \"Show tiles layers from BG map\""), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/tclient/menus_tclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("ConfigureSettingsCardSection(S, Localizable(\"Visual: Font & Cursor\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-visual-font-cursor-title\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-custom-font-label\")"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/qmclient/menus_qmclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, m_QmClientSettingsTab, pTitle, TitleStyleKey)"), std::string::npos);
		EXPECT_NE(Source.find("void CMenus::RenderQmVisualLabel("), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_VISUAL"), std::string::npos);
		EXPECT_EQ(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, m_QmClientSettingsTab, pValue)"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, TClientSettingsCardsUseSharedBoxAndAlignedFirstSection)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Body.find("auto DrawSectionBox = "), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->RenderBatchableRect(&Section, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);"), std::string::npos);
	EXPECT_NE(Body.find("DrawTClientCacheSectionBox(BoxRect);"), std::string::npos);
	EXPECT_NE(Body.find("InsetTClientCacheSectionContent(MeasuredColumn);"), std::string::npos);
	EXPECT_NE(Body.find("CUIRect BoxRect = LayoutSection(MeasuredColumn, false);"), std::string::npos);
	EXPECT_NE(Body.find("BoxRect.x = Col.x;"), std::string::npos);
	EXPECT_NE(Body.find("BoxRect.w = Col.w;"), std::string::npos);
	EXPECT_NE(Body.find("InsetTClientCacheSectionContent(ContentColumn);"), std::string::npos);
	EXPECT_NE(Body.find("Col.y = ContentColumn.y;"), std::string::npos);
	EXPECT_EQ(Source.find("ConfigureSplitCachedStaticLayer"), std::string::npos);
	EXPECT_NE(Source.find("RenderSettingsCardSection"), std::string::npos);
	EXPECT_NE(Source.find("ConfigureSettingsCardSection"), std::string::npos);
	EXPECT_NE(Source.find("Section.m_pStableCardId = pStableCardId;"), std::string::npos);
	EXPECT_NE(Header.find("void ConfigureSettingsCardSection(SSettingsSection &Section, const char *pTitle, const char *pStableCardId"), std::string::npos);

	const size_t ThemeSection = Source.find("SSettingsSection CMenus::BuildTClientThemeCacheSection()");
	ASSERT_NE(ThemeSection, std::string::npos);
	const size_t ThemeSectionEnd = Source.find("SSettingsSection CMenus::BuildTClientAutoReplyCacheSection()", ThemeSection);
	ASSERT_NE(ThemeSectionEnd, std::string::npos);
	const std::string ThemeBody = Source.substr(ThemeSection, ThemeSectionEnd - ThemeSection);
	EXPECT_NE(ThemeBody.find("ConfigureSettingsCardSection(S, Localizable(\"Visual: Font & Cursor\"), \"tclient:visual-font-cursor\", [this](CUIRect &Col, bool Render) -> float { return LayoutTClientThemeCacheSection(Col, Render); }, Margin);"), std::string::npos);

	const size_t AutoReplySection = Source.find("SSettingsSection CMenus::BuildTClientAutoReplyCacheSection()");
	ASSERT_NE(AutoReplySection, std::string::npos);
	const size_t AutoReplySectionEnd = Source.find("SSettingsSection CMenus::BuildTClientPetCacheSection()", AutoReplySection);
	ASSERT_NE(AutoReplySectionEnd, std::string::npos);
	const std::string AutoReplyBody = Source.substr(AutoReplySection, AutoReplySectionEnd - AutoReplySection);
	EXPECT_NE(AutoReplyBody.find("ConfigureSettingsCardSection(S, Localizable(\"Auto reply\"), \"tclient:auto-reply\", [this](CUIRect &Col, bool Render) -> float { return LayoutTClientAutoReplyCacheSection(Col, Render); }, MarginBetweenSections);"), std::string::npos);

	const size_t PetSection = Source.find("SSettingsSection CMenus::BuildTClientPetCacheSection()");
	ASSERT_NE(PetSection, std::string::npos);
	const size_t PetSectionEnd = Source.find("SSettingsSection CMenus::BuildTClientHudCacheSection()", PetSection);
	ASSERT_NE(PetSectionEnd, std::string::npos);
	const std::string PetBody = Source.substr(PetSection, PetSectionEnd - PetSection);
	EXPECT_NE(PetBody.find("ConfigureSettingsCardSection(S, \"Pet\", \"tclient:pet\", [this](CUIRect &Col, bool Render) -> float { return LayoutTClientPetCacheSection(Col, Render); }, MarginBetweenSections);"), std::string::npos);

	const size_t HudSection = Source.find("SSettingsSection CMenus::BuildTClientHudCacheSection()");
	ASSERT_NE(HudSection, std::string::npos);
	const size_t HudSectionEnd = Source.find("void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)", HudSection);
	ASSERT_NE(HudSectionEnd, std::string::npos);
	const std::string HudBody = Source.substr(HudSection, HudSectionEnd - HudSection);
	EXPECT_NE(HudBody.find("ConfigureSettingsCardSection(S, \"HUD\", \"tclient:hud\", [this](CUIRect &Col, bool Render) -> float { return LayoutTClientHudCacheSection(Col, Render); }, Margin);"), std::string::npos);

	const size_t ThemeLayout = Source.find("float CMenus::LayoutTClientThemeCacheSection(CUIRect &CurrentColumn, bool Render)");
	ASSERT_NE(ThemeLayout, std::string::npos);
	const size_t ThemeLayoutEnd = Source.find("float CMenus::LayoutTClientAutoReplyCacheSection", ThemeLayout);
	ASSERT_NE(ThemeLayoutEnd, std::string::npos);
	const std::string ThemeLayoutBody = Source.substr(ThemeLayout, ThemeLayoutEnd - ThemeLayout);
	EXPECT_NE(ThemeLayoutBody.find("CurrentColumn.HSplitTop(Margin, nullptr, &CurrentColumn);"), std::string::npos);
	EXPECT_EQ(ThemeLayoutBody.find("CurrentColumn.HSplitTop(MarginBetweenSections, nullptr, &CurrentColumn);"), std::string::npos);

	const size_t AutoReplyLayout = Source.find("float CMenus::LayoutTClientAutoReplyCacheSection(CUIRect &CurrentColumn, bool Render)");
	ASSERT_NE(AutoReplyLayout, std::string::npos);
	const size_t AutoReplyLayoutEnd = Source.find("float CMenus::LayoutTClientPetCacheSection", AutoReplyLayout);
	ASSERT_NE(AutoReplyLayoutEnd, std::string::npos);
	const std::string AutoReplyLayoutBody = Source.substr(AutoReplyLayout, AutoReplyLayoutEnd - AutoReplyLayout);
	EXPECT_NE(AutoReplyLayoutBody.find("const float SavedY = CurrentColumn.y;"), std::string::npos);
	EXPECT_NE(AutoReplyLayoutBody.find("return CurrentColumn.y - SavedY;"), std::string::npos);
	EXPECT_EQ(AutoReplyLayoutBody.find("return CurrentColumn.y - BoxRect.y;"), std::string::npos);

	const size_t PetLayout = Source.find("float CMenus::LayoutTClientPetCacheSection(CUIRect &CurrentColumn, bool Render)");
	ASSERT_NE(PetLayout, std::string::npos);
	const size_t PetLayoutEnd = Source.find("float CMenus::LayoutTClientHudCacheSection", PetLayout);
	ASSERT_NE(PetLayoutEnd, std::string::npos);
	const std::string PetLayoutBody = Source.substr(PetLayout, PetLayoutEnd - PetLayout);
	EXPECT_NE(PetLayoutBody.find("const float SavedY = CurrentColumn.y;"), std::string::npos);
	EXPECT_NE(PetLayoutBody.find("return CurrentColumn.y - SavedY;"), std::string::npos);
	EXPECT_EQ(PetLayoutBody.find("return CurrentColumn.y - BoxRect.y;"), std::string::npos);

	const size_t HudLayout = Source.find("float CMenus::LayoutTClientHudCacheSection(CUIRect &CurrentColumn, bool Render)");
	ASSERT_NE(HudLayout, std::string::npos);
	const size_t HudLayoutEnd = Source.find("SSettingsSection CMenus::BuildTClientThemeCacheSection", HudLayout);
	ASSERT_NE(HudLayoutEnd, std::string::npos);
	const std::string HudLayoutBody = Source.substr(HudLayout, HudLayoutEnd - HudLayout);
	EXPECT_NE(HudLayoutBody.find("const float SavedY = CurrentColumn.y;"), std::string::npos);
	EXPECT_NE(HudLayoutBody.find("return CurrentColumn.y - SavedY;"), std::string::npos);
	EXPECT_EQ(HudLayoutBody.find("return CurrentColumn.y - BoxRect.y;"), std::string::npos);

	const size_t DrawBoxRect = Source.find("CUIRect CMenus::TClientCacheSectionBoxRect(CUIRect BoxRect) const");
	ASSERT_NE(DrawBoxRect, std::string::npos);
	const size_t DrawBoxRectEnd = Source.find("void CMenus::InsetTClientCacheSectionContent", DrawBoxRect);
	ASSERT_NE(DrawBoxRectEnd, std::string::npos);
	const std::string DrawBoxRectBody = Source.substr(DrawBoxRect, DrawBoxRectEnd - DrawBoxRect);
	EXPECT_NE(DrawBoxRectBody.find("BoxRect.h += Padding;"), std::string::npos);
	EXPECT_NE(DrawBoxRectBody.find("BoxRect.y -= Padding * 0.5f;"), std::string::npos);
	EXPECT_EQ(DrawBoxRectBody.find("BoxRect.w += Padding;"), std::string::npos);
	EXPECT_EQ(DrawBoxRectBody.find("BoxRect.x -= Padding * 0.5f;"), std::string::npos);

	const size_t DrawBox = Source.find("void CMenus::DrawTClientCacheSectionBox(CUIRect BoxRect)");
	ASSERT_NE(DrawBox, std::string::npos);
	const size_t DrawBoxEnd = Source.find("float CMenus::RenderSettingsCardSection", DrawBox);
	ASSERT_NE(DrawBoxEnd, std::string::npos);
	const std::string DrawBoxBody = Source.substr(DrawBox, DrawBoxEnd - DrawBox);
	EXPECT_EQ(DrawBoxBody.find("CUi::ms_DarkButtonColorFunction.GetColor(false, false)"), std::string::npos);
	EXPECT_EQ(Source.find("ColorRGBA TClientCacheSectionBackgroundColor()"), std::string::npos);
	EXPECT_EQ(Source.find("return ColorRGBA(0.08f, 0.085f, 0.09f, 0.92f);"), std::string::npos);
	EXPECT_NE(DrawBoxBody.find("RenderQmSettingsGlassCard(TClientCacheSectionBoxRect(BoxRect), QmSettingsCardStyle(1.0f));"), std::string::npos);
	EXPECT_EQ(DrawBoxBody.find("Ui()->RenderBatchableRect(&BoxRect"), std::string::npos);
	EXPECT_EQ(DrawBoxBody.find("BoxRect.w += Padding;"), std::string::npos);
	EXPECT_EQ(DrawBoxBody.find("BoxRect.x -= Padding * 0.5f;"), std::string::npos);
	EXPECT_EQ(DrawBoxBody.find("BoxRect = TClientCacheSectionBoxRect(BoxRect);"), std::string::npos);

	const size_t InsetHelper = Source.find("void CMenus::InsetTClientCacheSectionContent(CUIRect &ContentRect) const");
	ASSERT_NE(InsetHelper, std::string::npos);
	const size_t InsetHelperEnd = Source.find("void CMenus::DrawTClientCacheSectionBox", InsetHelper);
	ASSERT_NE(InsetHelperEnd, std::string::npos);
	const std::string InsetHelperBody = Source.substr(InsetHelper, InsetHelperEnd - InsetHelper);
	EXPECT_NE(InsetHelperBody.find("ContentRect.VSplitLeft(Margin, nullptr, &ContentRect);"), std::string::npos);
	EXPECT_NE(InsetHelperBody.find("ContentRect.VSplitRight(Margin, &ContentRect, nullptr);"), std::string::npos);

	const std::string BindChatBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)");
	ASSERT_FALSE(BindChatBody.empty());
	EXPECT_EQ(BindChatBody.find("Background.w += Padding;"), std::string::npos);
	EXPECT_EQ(BindChatBody.find("Background.x -= Padding * 0.5f;"), std::string::npos);
	EXPECT_EQ(BindChatBody.find("Background.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);"), std::string::npos);
	EXPECT_NE(BindChatBody.find("DrawTClientCacheSectionBox(Section);"), std::string::npos);
	EXPECT_NE(BindChatBody.find("s_ScrollRegion.AddRect(TClientCacheSectionBoxRect(Section))"), std::string::npos);
	EXPECT_NE(BindChatBody.find("InsetTClientCacheSectionContent(ContentColumn);"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientSettingsCardDeckDragRuntimeUsesCtrlHeaderGate)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");

	EXPECT_NE(Header.find("std::vector<std::string> m_vTClientLeftCardOrder;"), std::string::npos);
	EXPECT_NE(Header.find("std::vector<std::string> m_vTClientRightCardOrder;"), std::string::npos);
	EXPECT_NE(Header.find("SSettingsCardDeckDragState m_TClientSettingsCardDragState;"), std::string::npos);
	EXPECT_NE(Source.find("RegisterSettingsCardDeckItem(Item);"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckCanStartDrag({&Item, false, HitRegion})"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckBeginPress(DragState, Item);"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckTryPromotePress(DragState);"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckClearPress(DragState);"), std::string::npos);
	EXPECT_NE(Source.find("DragState.m_DropColumn = Column;"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckDropColumnForMouseX(LeftView, RightView, Ui()->MouseX(), m_TClientSettingsCardDragState.m_DropColumn)"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckMoveBetweenColumns(vSourceOrder, vTargetOrder, DragState.m_Item.m_pStableId, DropIndex)"), std::string::npos);
	EXPECT_NE(Source.find("CommitSettingsCardDeckDragDrop(pOrder, Column, DropIndex, pDeckCoordinator);"), std::string::npos);
	EXPECT_NE(Header.find("bool CommitSettingsCardDeckDragDrop(std::vector<std::string> *pOrder, ESettingsCardDeckColumn DropColumn, int DropIndex, settings_card_deck::CDeck *pDeckCoordinator = nullptr);"), std::string::npos);
	EXPECT_NE(Source.find("m_TClientSettingsCardDeckOrderDirty = true;"), std::string::npos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/QmCardRegistry.h>"), std::string::npos);
	EXPECT_NE(Source.find("LoadTClientOrderFromGlobalCardModel(g_Config.m_QmGlobalCardOrder, m_vTClientLeftCardOrder, m_vTClientRightCardOrder)"), std::string::npos);
	EXPECT_NE(Source.find("Model.LoadExplicit(pConfig, qm_card_registry::BuildDefaultEntries())"), std::string::npos);
	EXPECT_EQ(Source.find("const char *pEntry = pConfig;\n\t\tchar aToken[160];\n\t\twhile((pEntry = str_next_token(pEntry, \";\", aToken"), std::string::npos);
	EXPECT_NE(Source.find("LoadTClientOrderFromLegacyCardOrder(g_Config.m_QmSettingsCardOrder, m_vTClientLeftCardOrder, m_vTClientRightCardOrder);"), std::string::npos);
	EXPECT_NE(Source.find("const bool HasGlobalTClientOrder = LoadTClientOrderFromGlobalCardModel(g_Config.m_QmGlobalCardOrder, m_vTClientLeftCardOrder, m_vTClientRightCardOrder);"), std::string::npos);
	EXPECT_NE(Source.find("if(!HasGlobalTClientOrder)"), std::string::npos);
	EXPECT_NE(Source.find("if(g_Config.m_QmGlobalCardOrder[0] == '\\0')"), std::string::npos);
	EXPECT_NE(Source.find("else\n\t\t\t\t{\n\t\t\t\t\tm_vTClientLeftCardOrder.clear();\n\t\t\t\t\tm_vTClientRightCardOrder.clear();\n\t\t\t\t}"), std::string::npos);
	EXPECT_NE(Source.find("const bool IsTClientMainOrder = pOrder == &m_vTClientLeftCardOrder || pOrder == &m_vTClientRightCardOrder;"), std::string::npos);
	EXPECT_NE(Source.find("if(IsTClientMainOrder)"), std::string::npos);
	EXPECT_NE(Source.find("SerializeMergedTClientGlobalCardOrder(g_Config.m_QmGlobalCardOrder, m_vTClientLeftCardOrder, m_vTClientRightCardOrder"), std::string::npos);
	EXPECT_NE(Source.find("str_copy(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(g_Config.m_QmGlobalCardOrder));"), std::string::npos);
	EXPECT_EQ(Source.find("str_copy(g_Config.m_QmSettingsCardOrder, aSerialized"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckApplyOrder(vSections, vOrder);"), std::string::npos);
	EXPECT_NE(Source.find("WrapSettingsCardDeckSections(vLeftSections, ESettingsCardDeckColumn::LEFT, m_vTClientLeftCardOrder);"), std::string::npos);
	EXPECT_NE(Source.find("WrapSettingsCardDeckSections(vRightSections, ESettingsCardDeckColumn::RIGHT, m_vTClientRightCardOrder);"), std::string::npos);
	EXPECT_NE(Source.find("const bool TClientSettingsCardDeckOrderDirtyAtFrameStart = !PrewarmOnly && m_TClientSettingsCardDeckOrderDirty;"), std::string::npos);
	EXPECT_NE(Source.find("if(TClientSettingsCardDeckOrderDirtyAtFrameStart)\n\t\tm_TClientSettingsCardDeckOrderDirty = false;"), std::string::npos);
	EXPECT_NE(Source.find("if(TClientSettingsCardDeckOrderDirtyAtFrameStart)\n\t\ts_VisualFontLoader.InvalidateCache(ESettingsCacheDirtyReason::CONFIG);"), std::string::npos);
	EXPECT_NE(Source.find("if(TClientSettingsCardDeckOrderDirtyAtFrameStart)\n\t\t\ts_RightSectionLoader.InvalidateCache(ESettingsCacheDirtyReason::CONFIG);"), std::string::npos);
	EXPECT_EQ(Source.find("if(!PrewarmOnly && m_TClientSettingsCardDeckOrderDirty)\n\t\tm_TClientSettingsCardDeckOrderDirty = false;"), std::string::npos);
	EXPECT_NE(Source.find("s_VisualFontLoader.Register(std::move(vLeftSections));"), std::string::npos);
	EXPECT_NE(Source.find("s_RightSectionLoader.Register(std::move(vRightSections));"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckIsDraggingItem(m_TClientSettingsCardDragState, Item)"), std::string::npos);
	EXPECT_NE(Source.find("m_TClientSettingsCardDragState.m_DropColumn == ColumnId"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckDropIndicatorRect(Item, m_TClientSettingsCardDragState.m_DropIndex"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckDropIndexForColumnItems("), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckProxyRect(m_TClientSettingsCardDragState.m_Item, Ui()->MouseX(), Ui()->MouseY())"), std::string::npos);
	EXPECT_NE(Source.find("if(m_TClientSettingsCardDragState.m_Active && !Ui()->MouseButton(0) && Ui()->LastMouseButton(0))"), std::string::npos);
	EXPECT_NE(Source.find("std::vector<std::string> *pOrder = m_TClientSettingsCardDragState.m_Item.m_Column == ESettingsCardDeckColumn::LEFT ? &m_vTClientLeftCardOrder : &m_vTClientRightCardOrder;"), std::string::npos);
	EXPECT_NE(Source.find("if(m_TClientSettingsCardDragState.m_Active && !Ui()->MouseButton(0) && !Ui()->LastMouseButton(0))"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckAutoScrollDelta(Ui()->MouseY(), Viewport.y, Viewport.y + Viewport.h"), std::string::npos);
	EXPECT_EQ(Source.find("SettingsCardDeckCanStartDrag({&Item, true"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsCardDeckGlobalOrderUsesSharedCardModel)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string TClientSource = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::LoadSettingsCardDeckOrdersFromGlobalConfig()");
	const std::string SerializeBody = ExtractSourceFunctionBody(Source, "void CMenus::SerializeMergedSettingsCardDeckOrdersToGlobalConfig()");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(SerializeBody.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/QmCardRegistry.h>"), std::string::npos);
	EXPECT_NE(Body.find("qm_card_order::CModel Model;"), std::string::npos);
	EXPECT_NE(Body.find("qm_card_registry::BuildDefaultEntries()"), std::string::npos);
	EXPECT_NE(Body.find("Model.LoadExplicit(g_Config.m_QmGlobalCardOrder, vDefaults)"), std::string::npos);
	EXPECT_NE(Body.find("Model.StableIdOrder(\"deck:\", DeckId.c_str(), 1)"), std::string::npos);
	EXPECT_NE(Body.find("Model.StableIdOrder(\"deck:\", DeckId.c_str(), 2)"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsCardDeckColumnPrefs.clear();"), std::string::npos);
	EXPECT_NE(Body.find("std::unordered_map<std::string, int> &ColumnPrefs = m_SettingsCardDeckColumnPrefs[DeckId];"), std::string::npos);
	EXPECT_NE(Body.find("ColumnPrefs[StableId] = 0x10;"), std::string::npos);
	EXPECT_NE(Body.find("ColumnPrefs[StableId] = 0x10 | 0x1;"), std::string::npos);
	EXPECT_NE(SerializeBody.find("m_SettingsCardDeckColumnPrefs"), std::string::npos);
	EXPECT_NE(SerializeBody.find("ColumnOrder[Column]++"), std::string::npos);
	EXPECT_NE(SerializeBody.find("vEntries.push_back({vOrder[i].c_str(), DeckId.c_str(), Column, ColumnOrder[Column]++});"), std::string::npos);
	EXPECT_NE(Source.find("if(PrefIt == pColumnPrefs->end() || (PrefIt->second & 0x10) == 0)"), std::string::npos);
	EXPECT_NE(Source.find("const int CurrentColumnPref = SettingsCardDeckColumnPref(pColumnPrefs, pGlobalStableId);"), std::string::npos);
	EXPECT_NE(Source.find("if(Deck.m_TwoColumns && (CurrentColumnPref & 0x10) != 0)"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckDropColumnForMouseX(Deck.m_aBaseColumns[0], Deck.m_aBaseColumns[1], Ui()->MouseX(), m_TClientSettingsCardDragState.m_DropColumn)"), std::string::npos);
	EXPECT_NE(Source.find("else if(Ui()->LastMouseButton(0))\n\t{\n\t\tm_TClientSettingsCardDragState.m_DropColumn = Deck.m_TwoColumns ?"), std::string::npos);
	EXPECT_NE(TClientSource.find("PrefIt->second = 0x10 | (DropColumn == ESettingsCardDeckColumn::RIGHT ? 1 : 0);"), std::string::npos);
	EXPECT_EQ(SerializeBody.find("vEntries.push_back({vOrder[i].c_str(), DeckId.c_str(), 1, (int)i});"), std::string::npos);
	EXPECT_EQ(Source.find("SettingsCardDeckParseGlobalColumn"), std::string::npos);
	EXPECT_EQ(Body.find("const char *pEntry = g_Config.m_QmGlobalCardOrder;\n\tchar aToken[160];\n\twhile((pEntry = str_next_token(pEntry, \";\", aToken"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientSettingsCardDeckCoversEveryBoxedSection)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::vector<const char *> vStableIds = {
		"tclient:visual-font-cursor",
		"tclient:visual-nameplates",
		"tclient:visual-effects",
		"tclient:input",
		"tclient:anti-latency-tools",
		"tclient:improved-anti-ping",
		"tclient:execute-on-join",
		"tclient:voting",
		"tclient:auto-reply",
		"tclient:player-indicator",
		"tclient:pet",
		"tclient:hud",
		"tclient:tee-status-bar",
		"tclient:tile-outlines",
		"tclient:ghost-tools",
		"tclient:rainbow",
		"tclient:tee-trails",
		"tclient:background-draw",
		"tclient:finish-name",
	};
	for(const char *pStableId : vStableIds)
	{
		EXPECT_NE(Source.find(pStableId), std::string::npos) << pStableId;
	}
	EXPECT_EQ(Source.find("if(SectionMeta.m_pStableCardId == nullptr || SectionMeta.m_pStableCardId[0] == '\\0')\n\t\t\t\tcontinue;"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientFocusModeSectionLabelsUseDisplayTextNotTranslationKeys)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmVisualFocusModeContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float ColumnGap, float LabelWidth)");
	ASSERT_FALSE(Body.empty());
	const std::string &FocusModeBody = Body;

	EXPECT_NE(FocusModeBody.find("auto RenderSection = [&](CUIRect &Target, const char *pTextId, const char *pLabel)"), std::string::npos);
	EXPECT_EQ(FocusModeBody.find("Localize(pTextId)"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderQmVisualLabel(pTextId, &Row, Localize(pLabel), BodySize * 0.82f);"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderSection(LeftColumn, \"qmclient-focus-section-interface\", \"Interface\");"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderSection(LeftColumn, \"qmclient-focus-section-players\", \"Players\");"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderSection(LeftColumn, \"qmclient-focus-section-visuals\", \"Visuals\");"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderSection(RightColumn, \"qmclient-focus-section-audio\", \"Audio\");"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("RenderSection(RightColumn, \"qmclient-focus-section-chat\", \"Chat\");"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsTextColdStartAvoidsGlobalLanguageCacheAndCachesCheckboxLabels)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_EQ(Source.find("PrepareLanguagePageCache(MainView.w);"), std::string::npos);
		EXPECT_EQ(Source.find("if(g_Config.m_UiSettingsPage == SETTINGS_LANGUAGE)"), std::string::npos);
		EXPECT_NE(Source.find("PrepareLanguagePageCache(List.w, true);"), std::string::npos);
		EXPECT_EQ(Source.find("PrepareLanguagePageCache(MainView.w, false)"), std::string::npos);
		EXPECT_NE(Source.find("PrepareLanguagePageCache(Content.w, false);"), std::string::npos);
		EXPECT_NE(Source.find("SettingsWarmupConsumeBudget(m_SettingsFrameBudget, ESettingsWarmupCost::TEXT_CONTAINER)"), std::string::npos);
		EXPECT_NE(Source.find("const bool TextChanged = RectEl.m_Text != Language.m_Name.c_str();"), std::string::npos);
		EXPECT_NE(Source.find("const bool SizeChanged = RectEl.m_Width != Label.w || RectEl.m_Height != Label.h;"), std::string::npos);
		EXPECT_NE(Source.find("const bool NeedsTextContainer = !RectEl.m_UITextContainer.Valid() || ColorChanged || TextChanged || SizeChanged;"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_GRAPHICS"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_SOUND"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsButton_CheckBox(SETTINGS_DDNET"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("int CMenus::DoSettingsButton_CheckBox(int Page, int Tab, const void *pId, const char *pTextId, const char *pText, int Checked, const CUIRect *pRect)"), std::string::npos);
		EXPECT_NE(Source.find("int CMenus::DoSettingsButton_CheckBox(int Page, int Tab, int Subtab, const void *pId, const char *pTextId, const char *pText, int Checked, const CUIRect *pRect)"), std::string::npos);
		EXPECT_NE(Source.find("const SMenuTextStyleKey StyleKey = BuildMenuTextStyleKey(&Label, Box.h * CUi::ms_FontmodHeight, TEXTALIGN_ML, Props);"), std::string::npos);
		EXPECT_NE(Source.find("CUIElement &LabelElement = MenuTextElement(MENU_TEXT_SCOPE_SETTINGS, Page, Tab, Subtab, pTextId, StyleKey);"), std::string::npos);
		EXPECT_NE(Source.find("DoButton_CheckBox_Common_WithLabelElement(pId, pText, Checked ? \"X\" : \"\", pRect, BUTTONFLAG_LEFT, &LabelElement, ProcessInput);"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsLabelStreamed(*pLabelElement, &Label, pText, FontSize, TEXTALIGN_ML, Props);"), std::string::npos);
		EXPECT_NE(Source.find("int CMenus::DoButton_CheckBox_Common(const void *pId, const char *pText, const char *pBoxText, const CUIRect *pRect, const unsigned Flags, const bool ProcessInput)"), std::string::npos);
		EXPECT_EQ(Source.find("context-checkbox-common-"), std::string::npos);
		EXPECT_EQ(Source.find("m_SettingsTextContextPage >= 0 && pText != nullptr && pText[0] != '\\0'"), std::string::npos);
		EXPECT_NE(Source.find("void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText"), std::string::npos);
		EXPECT_NE(Source.find("if(pText == nullptr)"), std::string::npos);
		EXPECT_NE(Source.find("Ui()->DoLabel(&Label, pText, FontSize, TEXTALIGN_ML, Props);"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, SettingsTextPlanPrebuildSeparatesInvisibleWarmupFromVisibleRender)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus.h"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("struct SMenuTextPlanItem"), std::string::npos);
		EXPECT_NE(Source.find("std::string m_TextId;"), std::string::npos);
		EXPECT_NE(Source.find("std::string m_Text;"), std::string::npos);
		EXPECT_NE(Source.find("enum EMenuTextStyleMode"), std::string::npos);
		EXPECT_NE(Source.find("MENU_TEXT_STYLE_DEFAULT"), std::string::npos);
		EXPECT_NE(Source.find("MENU_TEXT_STYLE_RECT"), std::string::npos);
		EXPECT_NE(Source.find("MENU_TEXT_STYLE_EXACT"), std::string::npos);
		EXPECT_NE(Source.find("MENU_TEXT_STYLE_ALLOWLIST_DYNAMIC"), std::string::npos);
		EXPECT_NE(Source.find("EMenuTextStyleMode m_StyleMode"), std::string::npos);
		EXPECT_NE(Source.find("std::string m_AllowlistReason;"), std::string::npos);
		EXPECT_NE(Source.find("std::string m_SourceTag;"), std::string::npos);
		EXPECT_EQ(Source.find("bool m_UseExplicitStyleKey = false;"), std::string::npos);
		EXPECT_NE(Source.find("MENU_TEXT_SCOPE_INGAME"), std::string::npos);
		EXPECT_NE(Source.find("SMenuTextPlanItem AddStableTextDefault("), std::string::npos);
		EXPECT_NE(Source.find("SMenuTextPlanItem AddStableTextLabel("), std::string::npos);
		EXPECT_NE(Source.find("SMenuTextPlanItem AddStableTextCheckbox("), std::string::npos);
		EXPECT_NE(Source.find("SMenuTextPlanItem AddStableTextScrollbar("), std::string::npos);
		EXPECT_NE(Source.find("SMenuTextPlanItem AddStableTextButton("), std::string::npos);
		EXPECT_NE(Source.find("void BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems"), std::string::npos);
		EXPECT_NE(Source.find("void BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems"), std::string::npos);
		EXPECT_NE(Source.find("void BuildIngameMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView);"), std::string::npos);
		EXPECT_NE(Source.find("void BuildBaseSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView);"), std::string::npos);
		EXPECT_NE(Source.find("void BuildSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView);"), std::string::npos);
		EXPECT_NE(Source.find("bool PrebuildSettingsTextPlanItem(const SMenuTextPlanItem &Item, int &RemainingBudget);"), std::string::npos);
		EXPECT_NE(Source.find("int CountMissingSettingsMenuTextPlanItems() const;"), std::string::npos);
		EXPECT_NE(Source.find("int PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride = nullptr);"), std::string::npos);
		EXPECT_EQ(Source.find("void PrebuildVisibleSettingsTextPool(const CUIRect &MainView, int Budget);"), std::string::npos);
		EXPECT_TRUE(ContainsAll(Source, {
							"std::vector<SMenuTextPlanItem> m_vSettingsMenuTextPrebuildPlan;",
							"std::unordered_set<std::string> m_SettingsMenuTextPlannedKeys;",
							"size_t m_SettingsMenuTextPlanCursor",
							"uint64_t m_SettingsMenuTextPlanGeneration",
							"SSettingsMenuTextPrebuildStats m_SettingsMenuTextLastPrebuildStats",
							"m_MenuTextStablePlannedThisFrame",
							"m_MenuTextStableUnplannedThisFrame",
							"std::unordered_set<std::string> m_SettingsMenuTextPlannedDescriptors;",
						}));
		EXPECT_NE(Source.find("bool DoSettingsScrollbarOption(int Page, int Tab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale = &CUi::ms_LinearScrollbarScale, unsigned Flags = 0u, const char *pSuffix = \"\", const char *pMaxText = nullptr);"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();
		const std::string LoadingBody = ExtractSourceFunctionBody(Source, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");

		EXPECT_NE(Source.find("bool CMenus::PrebuildSettingsTextPlanItem(const SMenuTextPlanItem &Item, int &RemainingBudget)"), std::string::npos);
		const std::string PrebuildItemBody = ExtractSourceFunctionBody(Source, "bool CMenus::PrebuildSettingsTextPlanItem(const SMenuTextPlanItem &Item, int &RemainingBudget)");
		EXPECT_NE(Source.find("void CMenus::BuildBaseSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView)"), std::string::npos);
		EXPECT_EQ(Source.find("AddDefaultStyleItem("), std::string::npos);
		EXPECT_NE(Source.find("static constexpr int s_aBaseSettingsPages[]"), std::string::npos);
		EXPECT_NE(Source.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_NE(Source.find("SETTINGS_GENERAL"), std::string::npos);
		EXPECT_NE(Source.find("SETTINGS_DDNET"), std::string::npos);
		EXPECT_EQ(Source.find("AddGeneralItem(\""), std::string::npos);
		EXPECT_EQ(Source.find("AddGeneralCheckbox(\""), std::string::npos);
		EXPECT_EQ(Source.find("AddStableTextDefault(SETTINGS_TEE"), std::string::npos);
		EXPECT_NE(Source.find("void CMenus::BuildIngameMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView)"), std::string::npos);
		EXPECT_NE(Source.find("\"ingame-tab-server-info\""), std::string::npos);
		EXPECT_NE(Source.find("RenderMenubar(TabBar, IClient::STATE_ONLINE);"), std::string::npos);
		EXPECT_NE(Source.find("RenderServerInfo(ContentView);"), std::string::npos);
		EXPECT_NE(Source.find("plan_status=%s"), std::string::npos);
		EXPECT_NE(Source.find("planned=%d unplanned=%d"), std::string::npos);
		EXPECT_NE(Source.find("MenuTextDescriptorKey("), std::string::npos);
		EXPECT_NE(Source.find("const bool HasDescriptor = m_SettingsMenuTextPlannedDescriptors.find(MenuTextDescriptorKey("), std::string::npos);
		EXPECT_NE(Source.find("HasDescriptor ? (KeyPlanned ? \"not_built\" : \"key_mismatch\") : \"missing_descriptor\""), std::string::npos);
		EXPECT_NE(Source.find("Box.Margin(2.0f, &Box);\n\tSLabelProperties Props;"), std::string::npos);
		EXPECT_NE(Source.find("void CMenus::BuildSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView)"), std::string::npos);
		EXPECT_NE(Source.find("CUIRect SettingsMainView = MenuTextSettingsContentView(Screen);"), std::string::npos);
		EXPECT_NE(Source.find("BuildVisibleSettingsMenuTextPlan(vVisibleItems, SettingsMainView);"), std::string::npos);
		EXPECT_NE(Source.find("BuildIngameMenuTextPlan(vItems, Screen);"), std::string::npos);
		EXPECT_NE(Source.find("int CMenus::CountMissingSettingsMenuTextPlanItems()"), std::string::npos);
		EXPECT_NE(Source.find("It->second.m_Generation != m_MenuTextPoolGeneration"), std::string::npos);
		EXPECT_NE(Source.find("int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)"), std::string::npos);
		EXPECT_EQ(Source.find("void CMenus::PrebuildVisibleSettingsTextPool(const CUIRect &MainView, int Budget)"), std::string::npos);
		ASSERT_FALSE(PrebuildItemBody.empty());
		EXPECT_NE(PrebuildItemBody.find("pRect->m_UITextContainer.Valid()"), std::string::npos);
		EXPECT_NE(PrebuildItemBody.find("Entry.m_Built = true;"), std::string::npos);
		EXPECT_NE(PrebuildItemBody.find("Entry.m_Generation = m_MenuTextPoolGeneration;"), std::string::npos);
		EXPECT_NE(Source.find("bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)"), std::string::npos);
		EXPECT_EQ(Source.find("RenderSettingsTClient(ContentView, true);"), std::string::npos);
		EXPECT_EQ(Source.find("RenderSettingsQmClient(ContentView, false, true);"), std::string::npos);
		EXPECT_EQ(Source.find("PrebuildVisibleSettingsTextPool(ContentView"), std::string::npos);
		ASSERT_FALSE(LoadingBody.empty());
		EXPECT_NE(LoadingBody.find("m_vSettingsMenuTextPrebuildPlan"), std::string::npos);
		EXPECT_NE(LoadingBody.find("m_SettingsMenuTextPlanCursor"), std::string::npos);
		EXPECT_EQ(LoadingBody.find("BuildSettingsMenuTextPlan(vItems);"), std::string::npos);
		EXPECT_EQ(LoadingBody.find("PrebuildSettingsTClientTextPool("), std::string::npos);
		EXPECT_EQ(LoadingBody.find("PrebuildSettingsQmClientTextPool("), std::string::npos);
		EXPECT_EQ(LoadingBody.find("const int LastPage = SettingsCanonicalPage(m_SettingsRuntimeMetadata.m_LastPage);"), std::string::npos);
	}
	{
		const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
		const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
		const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
		const std::string ScrollRegionHeader = ReadRepoFile("src/game/client/ui_scrollregion.h");
		const std::string TClientPlanBody = ExtractSourceFunctionBody(TClient, "void CMenus::BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
		const std::string QmClientPlanBody = ExtractSourceFunctionBody(QmClient, "void CMenus::BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
		const std::string TClientSettingsBody = ExtractSourceFunctionBody(TClient, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
		EXPECT_NE(TClient.find("void CMenus::BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems"), std::string::npos);
		EXPECT_NE(QmClient.find("void CMenus::BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems"), std::string::npos);
		ASSERT_FALSE(TClientPlanBody.empty());
		ASSERT_FALSE(QmClientPlanBody.empty());
		EXPECT_NE(TClientPlanBody.find("g_Config.m_UiSettingsPage = SETTINGS_TCLIENT;"), std::string::npos);
		EXPECT_NE(QmClientPlanBody.find("g_Config.m_UiSettingsPage = SETTINGS_QMCLIENT;"), std::string::npos);
		EXPECT_NE(TClientPlanBody.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_NE(QmClientPlanBody.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_EQ(TClientPlanBody.find("RenderSettingsTClient(MainView, true);"), std::string::npos);
		EXPECT_EQ(QmClientPlanBody.find("RenderSettingsQmClient(MainView, false, true);"), std::string::npos);
		EXPECT_NE(TClient.find("m_MenuTextPlanCollecting = true;"), std::string::npos);
		EXPECT_NE(QmClient.find("m_MenuTextPlanCollecting = true;"), std::string::npos);
		EXPECT_NE(TClient.find("m_pMenuTextPlanCollection = &vItems;"), std::string::npos);
		EXPECT_NE(QmClient.find("m_pMenuTextPlanCollection = &vItems;"), std::string::npos);
		EXPECT_NE(TClient.find("m_MenuTextPlanCollecting = PreviousCollecting;"), std::string::npos);
		EXPECT_NE(QmClient.find("m_MenuTextPlanCollecting = PreviousCollecting;"), std::string::npos);
		ASSERT_FALSE(TClientSettingsBody.empty());
		EXPECT_NE(ScrollRegionHeader.find("void SetContentHeightForNextFrame(float ContentHeight);"), std::string::npos);
		EXPECT_EQ(TClientSettingsBody.find("LogSettingsStage(\"tclient_settings_right_prewarm\", RightColumnTimer);\n\t\t\treturn;"), std::string::npos);
		EXPECT_NE(TClientSettingsBody.find("s_ScrollRegion.SetContentHeightForNextFrame("), std::string::npos);
		EXPECT_NE(TClientSettingsBody.find("FinishSettingsScrollRegion(s_ScrollRegion, ScrollFrame, &ScrollRegion, SETTINGS_TCLIENT);"), std::string::npos);
		EXPECT_LT(TClientSettingsBody.find("s_ScrollRegion.SetContentHeightForNextFrame("), TClientSettingsBody.find("FinishSettingsScrollRegion(s_ScrollRegion, ScrollFrame, &ScrollRegion, SETTINGS_TCLIENT);"));
		EXPECT_EQ(Settings.find("DoButton_CheckBox(&g_Config.m_ClAutoDemoRecord"), std::string::npos);
		EXPECT_EQ(Settings.find("DoButton_CheckBox(&g_Config.m_ClAutoScreenshot"), std::string::npos);
		EXPECT_EQ(Settings.find("DoButton_CheckBox(&g_Config.m_ClAutoStatboardScreenshot"), std::string::npos);
		EXPECT_EQ(Settings.find("DoButton_CheckBox(&g_Config.m_ClAutoCSV"), std::string::npos);
		EXPECT_NE(Settings.find("const bool CollectingMenuTextPlan = m_MenuTextPlanCollecting;"), std::string::npos);
		EXPECT_NE(Settings.find("const bool SettingsPerfEnabled = PerfDebugEnabled() && !CollectingMenuTextPlan;"), std::string::npos);
		EXPECT_NE(Settings.find("if(!CollectingMenuTextPlan && g_Config.m_UiSettingsPage != SETTINGS_ASSETS"), std::string::npos);
		EXPECT_NE(Settings.find("if(!CollectingMenuTextPlan)"), std::string::npos);
		EXPECT_NE(Settings.find("if(!s_SettingsTransitionInitialized)"), std::string::npos);
		EXPECT_NE(Settings.find("if(!CollectingMenuTextPlan && m_SettingsPerfLastPage == g_Config.m_UiSettingsPage)"), std::string::npos);
		EXPECT_NE(Settings.find("m_SettingsPerfLastPage = g_Config.m_UiSettingsPage;"), std::string::npos);
		EXPECT_NE(Settings.find("\"general-auto-demo-record\""), std::string::npos);
		EXPECT_NE(Settings.find("\"general-auto-screenshot\""), std::string::npos);
		EXPECT_NE(Settings.find("\"general-auto-statboard-screenshot\""), std::string::npos);
		EXPECT_NE(Settings.find("\"general-auto-csv\""), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/gameclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();
		const std::string Body = ExtractSourceFunctionBody(Source, "void CGameClient::PrewarmSettingsRuntimeCachesDuringLoading(const char *pLoadingCaption, const char *pLoadingMessage)");

		ASSERT_FALSE(Body.empty());
		EXPECT_EQ(Body.find("return;"), std::string::npos);
		EXPECT_NE(Body.find("m_Menus.PrewarmSettingsPages();"), std::string::npos);
		EXPECT_NE(Body.find("m_Menus.PrewarmSettingsTextPoolForLoading("), std::string::npos);
		EXPECT_NE(Body.find("SettingsLoadingPrewarmAdvance("), std::string::npos);
		EXPECT_EQ(Body.find("while(SettingsLoadingPrewarmShouldKeepPumping"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, SettingsTextPlanCoversHighValueTClientAndQmClientStaticLabels)
{
	{
		const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
		const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");

		EXPECT_NE(Settings.find("\"deck:general-game\""), std::string::npos);
		EXPECT_NE(Settings.find("\"deck:general-language\""), std::string::npos);
		EXPECT_NE(Settings.find("\"deck:general-client\""), std::string::npos);
		EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL, -1, &g_Config.m_ClDyncam, \"general-dynamic-camera\""), std::string::npos);
		EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL, -1, &g_Config.m_ClAutoswitchWeapons, \"general-switch-weapon-pickup\""), std::string::npos);
		EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL, -1, &g_Config.m_ClAutoswitchWeaponsOutOfAmmo, \"general-switch-weapon-out-of-ammo\""), std::string::npos);
		EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL, -1, &g_Config.m_ClSkipStartMenu, \"general-skip-main-menu\""), std::string::npos);
		EXPECT_NE(Settings.find("DoNumericField(\"general-refresh-rate\""), std::string::npos);
		EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_GENERAL, -1, &s_LowerRefreshRate, \"general-lower-refresh-rate\""), std::string::npos);
		EXPECT_NE(Settings.find("DoOpenButton(s_SettingsButtonId, \"general-settings-file\""), std::string::npos);

		EXPECT_NE(Menus.find("static constexpr int s_aBaseSettingsPages[]"), std::string::npos);
		EXPECT_NE(Menus.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_EQ(Menus.find("AddGeneralItem(\""), std::string::npos);
		EXPECT_EQ(Menus.find("AddGeneralCheckbox(\""), std::string::npos);
		EXPECT_EQ(Menus.find("AddGeneralScrollbar(\""), std::string::npos);
		EXPECT_EQ(Menus.find("AddGeneralMenuButton(\""), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/tclient/menus_tclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_EQ(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, nullptr"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-hammer-mode\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-cursor-scale\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-prediction-margin\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-pet-size\""), std::string::npos);
		const std::string PlanBody = ExtractSourceFunctionBody(Source, "void CMenus::BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
		ASSERT_FALSE(PlanBody.empty());
		EXPECT_NE(PlanBody.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_EQ(PlanBody.find("RenderSettingsTClient(MainView, true);"), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(\"tclient-wheel-animate-ms\""), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(\"tclient-pet-alpha\""), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(\"tclient-indicator-offset\""), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(\"tclient-outline-width\""), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(\"tclient-bg-draw-fade-time\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-auto-reply-title\""), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-player-indicator-title\""), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-tee-status-bar-title\")"), std::string::npos);
		EXPECT_EQ(Source.find("else if(Tab == TCLIENT_TAB_BINDCHAT)"), std::string::npos);
		EXPECT_EQ(Source.find("for(const auto &[pTitle, vBindDefaults] : CBindChat::BIND_DEFAULTS)"), std::string::npos);
		EXPECT_EQ(Source.find("AddItem(BindDefault.m_pTitle, Localize(BindDefault.m_pTitle), 210.0f, LineSize, FontSize);"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/qmclient/menus_qmclient.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("void CMenus::RenderQmVisualChatBubbleContent("), std::string::npos);
		EXPECT_NE(Source.find("RenderValue(\"qmclient-chat-bubble-duration\", \"Duration\""), std::string::npos);
		EXPECT_NE(Source.find("RenderValue(\"qmclient-chat-bubble-opacity\", \"Bubble opacity\""), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, m_QmClientSettingsTab, \"qmclient-display-mode\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, \"qmclient-translation-service\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, \"qmclient-target-language\")"), std::string::npos);
		EXPECT_NE(Source.find("SettingsTextElement(SETTINGS_QMCLIENT, QMCLIENT_SETTINGS_TAB_FUNCTION, \"qmclient-llm-provider\")"), std::string::npos);
		const std::string RenderBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
		ASSERT_FALSE(RenderBody.empty());
		EXPECT_NE(RenderBody.find("auto DoQmSettingsCheckboxAuto ="), std::string::npos);
		EXPECT_NE(RenderBody.find("const int OriginalValue = *pValue;"), std::string::npos);
		EXPECT_NE(RenderBody.find("if(PrewarmOnly || Ui()->RenderOnly())"), std::string::npos);
		EXPECT_NE(RenderBody.find("*pValue = OriginalValue;"), std::string::npos);
		EXPECT_NE(RenderBody.find("const char *pTextId"), std::string::npos);
		EXPECT_EQ(RenderBody.find("return DoQmSettingsCheckbox(pId, pText, pText"), std::string::npos);
		EXPECT_EQ(RenderBody.find("DoQmSettingsCheckboxAuto(&g_Config.m_QmFootParticles, Localize(\"Local particle effects\")"), std::string::npos);
		EXPECT_EQ(RenderBody.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
		const size_t DogfoodBranch = RenderBody.find("if(g_Config.m_DbgQmUiDogfood != 0)");
		ASSERT_NE(DogfoodBranch, std::string::npos);
		const size_t DogfoodPrewarmGate = RenderBody.find("if(PrewarmOnly)\n\t\t\treturn;", DogfoodBranch);
		const size_t DogfoodContext = RenderBody.find("IUiContext Ctx;", DogfoodBranch);
		ASSERT_NE(DogfoodPrewarmGate, std::string::npos);
		ASSERT_NE(DogfoodContext, std::string::npos);
		EXPECT_LT(DogfoodPrewarmGate, DogfoodContext);
		const std::string PlanBody = ExtractSourceFunctionBody(Source, "void CMenus::BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
		ASSERT_FALSE(PlanBody.empty());
		EXPECT_NE(PlanBody.find("RenderSettings(MainView);"), std::string::npos);
		EXPECT_EQ(PlanBody.find("RenderSettingsQmClient(MainView, false, true);"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, SettingsStableTextRegistryCoversVisibleWrappers)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");

	EXPECT_NE(Menus.find("AddStableTextLabel("), std::string::npos);
	EXPECT_NE(Menus.find("AddStableTextCheckbox("), std::string::npos);
	EXPECT_NE(Menus.find("AddStableTextScrollbar("), std::string::npos);
	EXPECT_NE(Menus.find("AddStableTextButton("), std::string::npos);
	EXPECT_NE(Menus.find("CollectMenuTextPlanItem("), std::string::npos);
	EXPECT_NE(Menus.find("m_MenuTextPlanCollecting"), std::string::npos);
	const std::string TClientPlanBody = ExtractSourceFunctionBody(TClient, "void CMenus::BuildTClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
	const std::string QmClientPlanBody = ExtractSourceFunctionBody(QmClient, "void CMenus::BuildQmClientSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView, int Tab)");
	ASSERT_FALSE(TClientPlanBody.empty());
	ASSERT_FALSE(QmClientPlanBody.empty());
	EXPECT_NE(TClientPlanBody.find("RenderSettings(MainView);"), std::string::npos);
	EXPECT_NE(QmClientPlanBody.find("RenderSettings(MainView);"), std::string::npos);
	EXPECT_EQ(TClientPlanBody.find("RenderSettingsTClient(MainView, true);"), std::string::npos);
	EXPECT_EQ(QmClientPlanBody.find("RenderSettingsQmClient(MainView, false, true);"), std::string::npos);
	EXPECT_NE(TClient.find("const bool PreviousCollecting = m_MenuTextPlanCollecting;"), std::string::npos);
	EXPECT_NE(QmClient.find("const bool PreviousCollecting = m_MenuTextPlanCollecting;"), std::string::npos);
	EXPECT_NE(TClient.find("m_MenuTextPlanCollecting = true;"), std::string::npos);
	EXPECT_NE(QmClient.find("m_MenuTextPlanCollecting = true;"), std::string::npos);
	EXPECT_NE(TClient.find("m_MenuTextPlanCollecting = PreviousCollecting;"), std::string::npos);
	EXPECT_NE(QmClient.find("m_MenuTextPlanCollecting = PreviousCollecting;"), std::string::npos);
	EXPECT_EQ(TClient.find("auto AddItem = [&]"), std::string::npos);
	EXPECT_EQ(QmClient.find("auto AddItem = [&]"), std::string::npos);
	EXPECT_EQ(Menus.find("Item.m_UseExplicitStyleKey"), std::string::npos);
	EXPECT_NE(Menus.find("CMenus::SMenuTextStyleKey CMenus::SettingsMenuTextPlanStyleKey(const SMenuTextPlanItem &Item) const"), std::string::npos);
	EXPECT_NE(Menus.find("switch(Item.m_StyleMode)"), std::string::npos);

	const std::vector<std::string> vRequiredBaseIds = {
		"\"deck:general-game\"",
		"\"deck:general-language\"",
		"\"deck:general-client\"",
		"\"tee-name-label\"",
		"\"tee-clan-label\"",
		"\"ddnet-run-on-join-label\"",
		"\"Save the best demo of each race\"",
		"\"Enable replays\"",
		"\"Enable ghost\"",
		"\"Show text entities\"",
		"\"Show others\"",
		"\"Show others (own team only)\"",
		"\"Show background quads\"",
		"\"Predict events (experimental)\"",
		"\"AntiPing (latency compensation)\"",
		"\"Use current map as background\"",
		"\"Show tiles layers from BG map\"",
	};
	for(const std::string &Id : vRequiredBaseIds)
	{
		EXPECT_NE(Settings.find(Id), std::string::npos) << Id;
	}
	EXPECT_NE(Menus.find("static constexpr int s_aBaseSettingsPages[]"), std::string::npos);
	EXPECT_NE(Menus.find("RenderSettings(MainView);"), std::string::npos);
	EXPECT_EQ(Menus.find("AddStableTextDefault(SETTINGS_TEE"), std::string::npos);
	EXPECT_EQ(Menus.find("AddGeneralCheckbox(\""), std::string::npos);

	const std::vector<std::string> vRequiredTClientIds = {
		"\"tclient-outline-width\"",
		"\"tclient-statusbar-local-time-title\"",
		"\"tclient-statusbar-colors-title\"",
		"\"tclient-statusbar-empty-preview\"",
		"\"tclient-statusbar-scheme-label\"",
		"\"tclient-statusbar-show\"",
		"\"tclient-statusbar-show-labels\"",
		"\"tclient-statusbar-height\"",
		"\"tclient-statusbar-12-hour-clock\"",
		"\"tclient-statusbar-seconds\"",
		"\"tclient-statusbar-alpha\"",
		"\"tclient-statusbar-text-alpha\"",
		"\"tclient-statusbar-apply-scheme\"",
		"\"tclient-statusbar-add-item\"",
		"\"tclient-statusbar-remove-item\"",
	};
	for(const std::string &Id : vRequiredTClientIds)
	{
		EXPECT_NE(TClient.find(Id), std::string::npos) << Id;
	}
	EXPECT_EQ(TClient.find("AddStableTextScrollbar(SETTINGS_TCLIENT"), std::string::npos);
	EXPECT_EQ(TClient.find("AddStableTextCheckbox(SETTINGS_TCLIENT"), std::string::npos);
	EXPECT_EQ(TClient.find("AddStableTextButton(SETTINGS_TCLIENT"), std::string::npos);

	const std::vector<std::string> vRequiredQmClientIds = {
		"\"qmclient-chat-bubble-duration\"",
		"\"qmclient-chat-bubble-opacity\"",
		"\"qmclient-display-mode\"",
		"\"qmclient-translation-service\"",
		"\"qmclient-target-language\"",
		"\"qmclient-llm-provider\"",
	};
	for(const std::string &Id : vRequiredQmClientIds)
	{
		EXPECT_NE(QmClient.find(Id), std::string::npos) << Id;
	}
	EXPECT_EQ(QmClient.find("AddStableText"), std::string::npos);

	const std::vector<std::string> vRequiredIngameIds = {
		"\"ingame-tab-game\"",
		"\"ingame-tab-players\"",
		"\"ingame-tab-server-info\"",
		"\"ingame-tab-browser\"",
		"\"ingame-tab-ghost\"",
		"\"ingame-tab-call-vote\"",
		"\"ingame-server-info-title\"",
		"\"ingame-server-info-address-label\"",
		"\"ingame-server-info-ping-label\"",
		"\"ingame-server-info-version-label\"",
		"\"ingame-server-info-password-label\"",
		"\"ingame-server-info-community-label\"",
		"\"ingame-game-info-title\"",
		"\"ingame-game-info-type-label\"",
		"\"ingame-game-info-map-label\"",
		"\"ingame-game-info-players-label\"",
		"\"ingame-server-info-motd-title\"",
	};
	for(const std::string &Id : vRequiredIngameIds)
	{
		EXPECT_TRUE(Menus.find(Id) != std::string::npos || Ingame.find(Id) != std::string::npos) << Id;
	}
	EXPECT_NE(Menus.find("DoIngameMenuTab("), std::string::npos);
	EXPECT_NE(Menus.find("BuildIngameMenuTextPlan(vItems, Screen);"), std::string::npos);
	EXPECT_NE(Menus.find("RenderMenubar(TabBar, IClient::STATE_ONLINE);"), std::string::npos);
	EXPECT_NE(Menus.find("RenderServerInfo(ContentView);"), std::string::npos);
	EXPECT_EQ(Menus.find("AddIngameTab("), std::string::npos);
	EXPECT_NE(Menus.find("return DoMenuTabV2(pButtonContainer, pText, Checked != 0, pRect, Corners, nullptr, nullptr, nullptr, nullptr, &TextElement);"), std::string::npos);
	EXPECT_EQ(Menus.find("DoMenuTabV2(&s_ServerInfoButton, Localize(\"Server info\")"), std::string::npos);
	EXPECT_NE(Ingame.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-server-info-title\""), std::string::npos);
	EXPECT_NE(Ingame.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-game-info-title\""), std::string::npos);
	EXPECT_NE(Ingame.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-server-info-motd-title\""), std::string::npos);
	EXPECT_EQ(Ingame.find("DoIngameMenuLabel(PAGE_SERVER_INFO, \"ingame-server-info-name\""), std::string::npos);
	EXPECT_EQ(Ingame.find("DoIngameMenuLabel(PAGE_SERVER_INFO, \"ingame-game-info-map\", &Label, aBuf"), std::string::npos);
	const size_t OnlineBranch = Menus.find("case IClient::STATE_ONLINE:");
	ASSERT_NE(OnlineBranch, std::string::npos);
	const size_t IngameGuard = Menus.find("TextVisibleGuard.emplace(this);", OnlineBranch);
	const size_t IngameContent = Menus.find("if(m_GamePage == PAGE_GAME)", OnlineBranch);
	const size_t IngameSettings = Menus.find("RenderSettings(MainView);", IngameContent);
	const size_t IngameMenubar = Menus.find("RenderMenubar(TabBar, ClientState);", OnlineBranch);
	ASSERT_NE(IngameGuard, std::string::npos);
	ASSERT_NE(IngameContent, std::string::npos);
	ASSERT_NE(IngameSettings, std::string::npos);
	ASSERT_NE(IngameMenubar, std::string::npos);
	EXPECT_LT(IngameGuard, IngameContent);
	EXPECT_LT(IngameGuard, IngameSettings);
	EXPECT_LT(IngameGuard, IngameMenubar);
	const size_t OfflineBranch = Menus.find("case IClient::STATE_OFFLINE:");
	ASSERT_NE(OfflineBranch, std::string::npos);
	const size_t OfflineGuard = Menus.find("std::optional<CScopedMenuTextVisibleGuard> TextVisibleGuard;", OfflineBranch);
	ASSERT_NE(OfflineGuard, std::string::npos);
	const size_t OfflineContent = Menus.find("CPerfTimer ContentTimer;", OfflineGuard);
	ASSERT_NE(OfflineContent, std::string::npos);
	const std::string OfflineGuardBody = Menus.substr(OfflineGuard, OfflineContent - OfflineGuard);
	EXPECT_NE(OfflineGuardBody.find("if(m_MenuPage == PAGE_SETTINGS)"), std::string::npos);
	EXPECT_EQ(OfflineGuardBody.find("if(m_GamePage != PAGE_SETTINGS)"), std::string::npos);
	EXPECT_EQ(Menus.find("Props.m_MaxWidth = Width;"), std::string::npos);
	EXPECT_EQ(Menus.find("CUIRect{0.0f, 0.0f, Width, Height - 4.0f}"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsStableTextPlanKeysMatchVisibleWrappers)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");

	const std::string ScrollbarBody = ExtractSourceFunctionBody(Menus, "CMenus::SMenuTextPlanItem CMenus::AddStableTextScrollbar(int Page, int Tab, int Subtab, const char *pTextId, const char *pText, const CUIRect &Rect, unsigned Flags, const char *pSourceTag) const");
	ASSERT_FALSE(ScrollbarBody.empty());
	EXPECT_NE(ScrollbarBody.find("ui_widget::SliderInputFieldLabelRect("), std::string::npos);
	EXPECT_NE(ScrollbarBody.find("BuildMenuTextStyleKey("), std::string::npos);

	const std::string ScrollbarOptionBody = ExtractSourceFunctionBody(Menus, "bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, int Subtab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)");
	const std::string NumericLabelBridge = ExtractSourceFunctionBody(Menus, "bool CMenus::PrepareSettingsNumericFieldLabel(");
	ASSERT_FALSE(ScrollbarOptionBody.empty());
	ASSERT_FALSE(NumericLabelBridge.empty());
	EXPECT_NE(ScrollbarOptionBody.find("PrepareSettingsNumericFieldLabel("), std::string::npos);
	EXPECT_NE(NumericLabelBridge.find("ui_widget::SliderInputFieldLabelRect("), std::string::npos);
	EXPECT_NE(NumericLabelBridge.find("BuildMenuTextStyleKey("), std::string::npos);
	EXPECT_NE(NumericLabelBridge.find("CollectMenuTextPlanItem(MENU_TEXT_SCOPE_SETTINGS"), std::string::npos);
	EXPECT_EQ(ScrollbarOptionBody.find("DoSettingsMenuLabel(Page, Tab, Subtab, pTextId, &Label, pStr, FontSize, TEXTALIGN_ML, {}, (int)Label.w);"), std::string::npos);
	const std::string SplitScrollbarBody = ExtractSourceFunctionBody(Menus, "void CMenus::SplitSettingsScrollbarRects(const CUIRect &Rect, unsigned Flags, CUIRect *pLabelRect, CUIRect *pValueRect, CUIRect *pScrollBarRect) const");
	ASSERT_FALSE(SplitScrollbarBody.empty());
	EXPECT_NE(SplitScrollbarBody.find("const float ValueWidth = std::clamp(Rect.w * 0.12f, 42.0f, 68.0f);"), std::string::npos);
	EXPECT_NE(SplitScrollbarBody.find("const float LabelWidth = std::clamp(Rect.w * 0.25f, 108.0f, 180.0f);"), std::string::npos);
	EXPECT_NE(SplitScrollbarBody.find("Controls.VSplitRight(ValueWidth, &ScrollBar, &ValueText);"), std::string::npos);
	EXPECT_NE(SplitScrollbarBody.find("ScrollBar.VMargin(minimum(10.0f, Rect.w * 0.025f), &ScrollBar);"), std::string::npos);

	EXPECT_EQ(Header.find("BuildSettingsScrollbarTextStyle("), std::string::npos);
	EXPECT_NE(Settings.find("DoAppearanceNumericField(APPEARANCE_TAB_HUD, \"appearance-freeze-bars-alpha-inside-freeze\""), std::string::npos);
	EXPECT_NE(Header.find("SMenuTextStyleKey BuildSettingsShellTitleTextStyle(const CUIRect &Rect, CUIRect *pOutLabel = nullptr) const;"), std::string::npos);
	EXPECT_NE(Menus.find("BuildSettingsShellTitleTextStyle("), std::string::npos);
	EXPECT_NE(Menus.find("settings-shell-title"), std::string::npos);
	EXPECT_NE(Settings.find("DoDDNetNumericField(\"ddnet-default-zoom\""), std::string::npos);
	EXPECT_NE(Settings.find("DoSettingsButton_CheckBox(SETTINGS_DDNET, -1, &g_Config.m_ClRaceGhost, \"Enable ghost\""), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsStableTextPrebuildCompletesTargetPlan)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string CountBody = ExtractSourceFunctionBody(Menus, "int CMenus::CountMissingSettingsMenuTextPlanItems()");
	const std::string LogBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrebuildSettingsMenuTextPool(int Budget, const char *pScopeOverride, const char *pOperationOverride)");

	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(CountBody.empty());
	ASSERT_FALSE(LogBody.empty());
	EXPECT_NE(Header.find("int CountMissingSettingsMenuTextPlanItems() const;"), std::string::npos);
	EXPECT_NE(CountBody.find("m_vSettingsMenuTextPrebuildPlan"), std::string::npos);
	EXPECT_EQ(CountBody.find("BuildSettingsMenuTextPlan(vItems);"), std::string::npos);
	EXPECT_NE(PrebuildBody.find("m_SettingsMenuTextLastPrebuildStats.m_Remaining = CountMissingSettingsMenuTextPlanItems();"), std::string::npos);
	EXPECT_NE(LogBody.find("const int RemainingMissing = m_SettingsMenuTextLastPrebuildStats.m_Remaining;"), std::string::npos);
	EXPECT_EQ(LogBody.find("const int RemainingMissing = CountMissingSettingsMenuTextPlanItems();"), std::string::npos);
	EXPECT_EQ(CountBody.find("for(const SMenuTextPlanItem &Item : m_vSettingsMenuTextPrebuildPlan)"), std::string::npos);
	EXPECT_NE(CountBody.find("m_SettingsMenuTextPlanCollectionComplete"), std::string::npos);
	EXPECT_NE(CountBody.find("m_vSettingsMenuTextPrebuildPlan.size() - (int)m_SettingsMenuTextPlanCursor"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameEscStableTextRegistryCoversMenubar)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string PlanBody = ExtractSourceFunctionBody(Menus, "void CMenus::BuildIngameMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView)");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string CollectionBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrepareSettingsMenuTextPlanCollectionUnits(const char *pOperationOverride)");
	ASSERT_FALSE(PlanBody.empty());
	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(CollectionBody.empty());

	EXPECT_NE(Header.find("void PrebuildIngameEscTextPoolBeforeOpen(int Budget);"), std::string::npos);
	EXPECT_NE(Menus.find("PrebuildIngameEscTextPoolBeforeOpen("), std::string::npos);
	EXPECT_EQ(Menus.find("PrebuildIngameEscTextPoolBeforeOpen(96);"), std::string::npos);
	EXPECT_NE(CollectionBody.find("const bool IngameEscOperation = str_comp(pOperation, \"ingame_esc_open\") == 0;"), std::string::npos);
	EXPECT_NE(CollectionBody.find("if(IngameEscOperation)"), std::string::npos);
	EXPECT_NE(Menus.find("BuildIngameMenuTextPlan(m_vSettingsMenuTextPrebuildPlan, Screen);"), std::string::npos);
	EXPECT_NE(PrebuildBody.find("AdvanceSettingsMenuTextPlanCollection("), std::string::npos);
	for(const char *pId : {
		    "\"ingame-tab-game\"",
		    "\"ingame-tab-players\"",
		    "\"ingame-tab-server-info\"",
		    "\"ingame-tab-browser\"",
		    "\"ingame-tab-ghost\"",
		    "\"ingame-tab-call-vote\"",
	    })
	{
		EXPECT_NE(PlanBody.find(pId), std::string::npos) << pId;
	}
	EXPECT_EQ(Menus.find("plan_status\":\"missing_descriptor\""), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsOpenSkipsIngamePlanWhileEscCollectsIncrementally)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string CollectionBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrepareSettingsMenuTextPlanCollectionUnits(const char *pOperationOverride)");
	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(CollectionBody.empty());

	EXPECT_NE(CollectionBody.find("const bool IngameEscOperation = str_comp(pOperation, \"ingame_esc_open\") == 0;"), std::string::npos);
	EXPECT_NE(CollectionBody.find("if(IngameEscOperation)"), std::string::npos);
	EXPECT_NE(CollectionBody.find("MENU_TEXT_PLAN_UNIT_INGAME_ESC"), std::string::npos);
	EXPECT_NE(PrebuildBody.find("AdvanceSettingsMenuTextPlanCollection("), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("BuildSettingsMenuTextPlan(vItems, SettingsMainView);"), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("MENU_TEXT_PLAN_UNIT_INGAME_ESC"), std::string::npos);
}

TEST(QmMonitoringHelpers, LoadingAndEscPrebuildDoNotSynchronouslyBuildFullSettingsPlan)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string EscBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrebuildIngameEscTextPoolBeforeOpen(int Budget)");
	const std::string EnsureBody = ExtractSourceFunctionBody(Menus, "void CMenus::EnsureSettingsMenuTextPlanReadyForVisible()");
	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(EscBody.empty());
	ASSERT_FALSE(EnsureBody.empty());

	EXPECT_NE(Header.find("void BuildVisibleSettingsMenuTextPlan(std::vector<SMenuTextPlanItem> &vItems, CUIRect MainView);"), std::string::npos);
	EXPECT_NE(PrebuildBody.find("AdvanceSettingsMenuTextPlanCollection("), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("BuildSettingsMenuTextPlan(vItems, SettingsMainView);"), std::string::npos);
	EXPECT_EQ(EscBody.find("BuildSettingsMenuTextPlan(m_vSettingsMenuTextPrebuildPlan, SettingsMainView);"), std::string::npos);
	EXPECT_NE(EscBody.find("PrebuildSettingsMenuTextPool(minimum(Budget, maximum(1, AdaptiveBudget.m_TextPrebuildTokens)), \"target_settings\", \"ingame_esc_open\");"), std::string::npos);
	EXPECT_NE(EnsureBody.find("BuildVisibleSettingsMenuTextPlan(vVisibleItems, SettingsMainView);"), std::string::npos);
	EXPECT_EQ(EnsureBody.find("BuildSettingsMenuTextPlan(m_vSettingsMenuTextPrebuildPlan);"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameEscPrewarmsStableTextBeforeVisibleFrame)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	const std::string CollectionBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrepareSettingsMenuTextPlanCollectionUnits(const char *pOperationOverride)");
	const std::string CollectBody = ExtractSourceFunctionBody(Menus, "void CMenus::CollectSettingsMenuTextPlanUnit(const SSettingsMenuTextPlanCollectionUnit &Unit, CUIRect Screen, CUIRect SettingsMainView)");
	ASSERT_FALSE(OnRenderBody.empty());
	ASSERT_FALSE(CollectionBody.empty());
	ASSERT_FALSE(CollectBody.empty());

	const size_t StartWindowPos = OnRenderBody.find("StartSettingsPerfFixedWindow(\"ingame_esc_open\"");
	const size_t SetActivePos = OnRenderBody.find("SetActive(true);", StartWindowPos);
	const size_t RenderPos = OnRenderBody.find("Render();");
	ASSERT_NE(StartWindowPos, std::string::npos);
	ASSERT_NE(SetActivePos, std::string::npos);
	ASSERT_NE(RenderPos, std::string::npos);
	EXPECT_LT(StartWindowPos, SetActivePos);
	EXPECT_LT(SetActivePos, RenderPos);

	EXPECT_NE(Header.find("uint64_t m_IngameEscOpenFrame"), std::string::npos);
	EXPECT_NE(Header.find("bool m_IngameServerInfoBackgroundPrepareRequested"), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("PrebuildIngameEscTextPoolBeforeOpen("), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("PrebuildSettingsMenuTextPool("), std::string::npos);
	EXPECT_NE(OnRenderBody.find("Client()->PerfFrame() > m_IngameEscOpenFrame"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("PrepareIngameServerInfoTextRuntime();"), std::string::npos);
	EXPECT_NE(CollectionBody.find("m_vSettingsMenuTextPlanCollectionUnits.push_back({MENU_TEXT_PLAN_UNIT_INGAME_ESC, -1, -1});"), std::string::npos);
	EXPECT_NE(CollectBody.find("case MENU_TEXT_PLAN_UNIT_INGAME_ESC:"), std::string::npos);
	EXPECT_NE(CollectBody.find("BuildIngameMenuTextPlan(m_vSettingsMenuTextPrebuildPlan, Screen);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsMenuTextPlanCollectionUsesIncrementalCursor)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string EscBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrebuildIngameEscTextPoolBeforeOpen(int Budget)");
	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(EscBody.empty());

	EXPECT_NE(Header.find("struct SSettingsMenuTextPlanCollectionUnit"), std::string::npos);
	EXPECT_NE(Header.find("struct SSettingsMenuTextPlanCollectionStats"), std::string::npos);
	EXPECT_NE(Header.find("std::vector<SSettingsMenuTextPlanCollectionUnit> m_vSettingsMenuTextPlanCollectionUnits;"), std::string::npos);
	EXPECT_NE(Header.find("size_t m_SettingsMenuTextPlanCollectionCursor"), std::string::npos);
	EXPECT_NE(Header.find("uint64_t m_SettingsMenuTextPlanCollectionGeneration"), std::string::npos);
	EXPECT_NE(Header.find("bool m_SettingsMenuTextPlanCollectionDirty"), std::string::npos);
	EXPECT_NE(Header.find("bool m_SettingsMenuTextPlanCollectionComplete"), std::string::npos);
	EXPECT_NE(Header.find("SSettingsMenuTextPlanCollectionStats m_SettingsMenuTextLastCollectionStats"), std::string::npos);
	EXPECT_NE(Header.find("int SettingsTextPlanCollectionRemaining() const"), std::string::npos);

	EXPECT_NE(Menus.find("void CMenus::PrepareSettingsMenuTextPlanCollectionUnits(const char *pOperationOverride)"), std::string::npos);
	EXPECT_NE(Menus.find("bool CMenus::AdvanceSettingsMenuTextPlanCollection(int Budget, const char *pOperationOverride)"), std::string::npos);
	EXPECT_NE(Menus.find("void CMenus::CollectSettingsMenuTextPlanUnit(const SSettingsMenuTextPlanCollectionUnit &Unit, CUIRect Screen, CUIRect SettingsMainView)"), std::string::npos);
	EXPECT_NE(Menus.find("event=settings_text_plan_collection"), std::string::npos);
	EXPECT_NE(Menus.find("units_done=%d units_total=%d remaining=%d budget=%d complete=%d dirty=%d phase=%s scope=%s operation=%s"), std::string::npos);

	EXPECT_NE(PrebuildBody.find("AdvanceSettingsMenuTextPlanCollection("), std::string::npos);
	EXPECT_LT(PrebuildBody.find("AdvanceSettingsMenuTextPlanCollection("), PrebuildBody.find("while(m_SettingsMenuTextPlanCursor < m_vSettingsMenuTextPrebuildPlan.size())"));
	EXPECT_EQ(PrebuildBody.find("BuildSettingsMenuTextPlan(vItems, SettingsMainView);"), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("BuildSettingsMenuTextPlan(vItems);"), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("BuildBaseSettingsMenuTextPlan(vItems, SettingsMainView);"), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("for(int Tab = 0; Tab < NumTClientTextPlanTabs; ++Tab)"), std::string::npos);
	EXPECT_EQ(PrebuildBody.find("for(int Tab = 0; Tab < NUMBER_OF_QMCLIENT_SETTINGS_TABS; ++Tab)"), std::string::npos);

	EXPECT_EQ(EscBody.find("BuildSettingsMenuTextPlan("), std::string::npos);
	EXPECT_EQ(EscBody.find("BuildSettingsMenuTextPlan("), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsPlanCollectionDoesNotSynchronouslyPumpStartupOrEsc)
{
	const std::string GameClient = ReadRepoFile("src/game/client/gameclient.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string PrewarmBody = ExtractSourceFunctionBody(GameClient, "void CGameClient::PrewarmSettingsRuntimeCachesDuringLoading(const char *pLoadingCaption, const char *pLoadingMessage)");
	const std::string RenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	const std::string EscBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrebuildIngameEscTextPoolBeforeOpen(int Budget)");
	ASSERT_FALSE(PrewarmBody.empty());
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(EscBody.empty());

	EXPECT_NE(PrewarmBody.find("constexpr int TEXT_PREWARM_BUDGET_PER_STEP = 8;"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("m_Menus.PrewarmSettingsTextPoolForLoading(TEXT_PREWARM_BUDGET_PER_STEP);"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("while(SettingsLoadingPrewarmShouldKeepPumping"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("MAX_PREWARM_ATTEMPTS = 128"), std::string::npos);

	EXPECT_EQ(RenderBody.find("PrebuildIngameEscTextPoolBeforeOpen(96);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("PrebuildIngameEscTextPoolBeforeOpen(4);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("PrebuildIngameEscTextPoolBeforeOpen(3);"), std::string::npos);
	EXPECT_NE(RenderBody.find("StartSettingsPerfFixedWindow(\"ingame_esc_open\""), std::string::npos);
	EXPECT_EQ(EscBody.find("BuildSettingsMenuTextPlan("), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsPlanCollectionDoesNotEnterVisibleGuard)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsBody = ExtractSourceFunctionBody(Settings, "void CMenus::RenderSettings(CUIRect MainView)");
	ASSERT_FALSE(RenderSettingsBody.empty());

	EXPECT_NE(RenderSettingsBody.find("std::optional<CScopedMenuTextVisibleGuard> TextVisibleGuard;"), std::string::npos);
	const size_t GuardCondition = RenderSettingsBody.find("if(!CollectingMenuTextPlan)");
	const size_t GuardEmplace = RenderSettingsBody.find("TextVisibleGuard.emplace(this);");
	ASSERT_NE(GuardCondition, std::string::npos);
	ASSERT_NE(GuardEmplace, std::string::npos);
	EXPECT_LT(GuardCondition, GuardEmplace);
	EXPECT_EQ(RenderSettingsBody.find("const CScopedMenuTextVisibleGuard TextVisibleGuard(this);"), std::string::npos);
}

TEST(QmMonitoringHelpers, VisibleGuardKeepsPlanMetadataAfterInvalidation)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string InvalidateBody = ExtractSourceFunctionBody(Menus, "void CMenus::InvalidateMenuTextPool(const char *pReason)");
	const std::string EnsureBody = ExtractSourceFunctionBody(Menus, "void CMenus::EnsureSettingsMenuTextPlanReadyForVisible()");
	ASSERT_FALSE(InvalidateBody.empty());
	ASSERT_FALSE(EnsureBody.empty());

	EXPECT_NE(Header.find("void EnsureSettingsMenuTextPlanReadyForVisible();"), std::string::npos);
	EXPECT_NE(InvalidateBody.find("if(!m_MenuTextPoolVisibleGuard)"), std::string::npos);
	EXPECT_NE(InvalidateBody.find("m_SettingsMenuTextPlanMetadataDirty = true;"), std::string::npos);
	EXPECT_NE(EnsureBody.find("std::vector<SMenuTextPlanItem> vVisibleItems;"), std::string::npos);
	EXPECT_NE(EnsureBody.find("BuildVisibleSettingsMenuTextPlan(vVisibleItems, SettingsMainView);"), std::string::npos);
	EXPECT_NE(EnsureBody.find("m_SettingsMenuTextPlannedDescriptors.insert"), std::string::npos);
	EXPECT_NE(EnsureBody.find("m_SettingsMenuTextPlannedKeys.insert"), std::string::npos);
	EXPECT_NE(EnsureBody.find("m_vSettingsMenuTextPrebuildPlan.clear();"), std::string::npos);
	EXPECT_EQ(EnsureBody.find("m_vSettingsMenuTextPlanCollectionUnits.clear();"), std::string::npos);
	EXPECT_EQ(EnsureBody.find("m_SettingsMenuTextPlanCollectionDirty = true;"), std::string::npos);
	EXPECT_NE(Menus.find("EnsureSettingsMenuTextPlanReadyForVisible();\n\tm_pMenus->m_MenuTextPoolVisibleGuard = true;"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoBrowserStartupDoesNotSynchronouslyFetchAllHeaders)
{
	const std::string MenusDemo = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string RenderListBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)");
	const std::string PopulateBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::DemolistPopulate()");
	ASSERT_FALSE(RenderListBody.empty());
	ASSERT_FALSE(PopulateBody.empty());

	EXPECT_NE(RenderListBody.find("DemolistPopulate();"), std::string::npos);
	EXPECT_NE(RenderListBody.find("DemolistOnUpdate(true);"), std::string::npos);
	EXPECT_EQ(PopulateBody.find("FetchAllHeaders();"), std::string::npos);
	EXPECT_EQ(PopulateBody.find("EnsureAllDemoDates();"), std::string::npos);
	EXPECT_NE(MenusDemo.find("AdvanceDemoBrowserMetadata("), std::string::npos);
	EXPECT_NE(MenusDemo.find("event=demo_browser_startup"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoBrowserFetchInfoUsesBoundedProgress)
{
	const std::string MenusDemo = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string ButtonsBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::RenderDemoBrowserButtons(CUIRect ButtonsView, bool WasListboxItemActivated)");
	const std::string SortListBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)");
	const std::string DetailsBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::RenderDemoBrowserDetails(CUIRect DetailsView)");
	const std::string FetchAllBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::FetchAllHeaders()");
	const std::string EnsureDatesBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::EnsureAllDemoDates()");
	ASSERT_FALSE(ButtonsBody.empty());
	ASSERT_FALSE(SortListBody.empty());
	ASSERT_FALSE(DetailsBody.empty());
	ASSERT_FALSE(FetchAllBody.empty());
	ASSERT_FALSE(EnsureDatesBody.empty());

	EXPECT_NE(ButtonsBody.find("g_Config.m_BrDemoFetchInfo"), std::string::npos);
	EXPECT_EQ(ButtonsBody.find("FetchAllHeaders();"), std::string::npos);
	EXPECT_EQ(SortListBody.find("EnsureAllDemoDates();"), std::string::npos);
	EXPECT_EQ(FetchAllBody.find("for(auto &Item : m_vDemos)"), std::string::npos);
	EXPECT_EQ(EnsureDatesBody.find("for(auto &Item : m_vDemos)"), std::string::npos);
	EXPECT_NE(FetchAllBody.find("AdvanceDemoBrowserMetadata(2, g_Config.m_BrDemoSort == SORT_DATE ? 4 : 0, \"fetch_info\");"), std::string::npos);
	EXPECT_NE(EnsureDatesBody.find("AdvanceDemoBrowserMetadata(0, maximum(1, AdaptiveBudget.m_DemoMetadataTokens), \"ensure_dates\");"), std::string::npos);
	EXPECT_EQ(SortListBody.find("if(EnsureDemoDate(*pItem))"), std::string::npos);
	EXPECT_NE(SortListBody.find("if(pItem->m_DateLoaded && pItem->m_DateValid)"), std::string::npos);
	EXPECT_EQ(DetailsBody.find("!FetchHeader(*pItem)"), std::string::npos);
	EXPECT_NE(DetailsBody.find("!pItem->m_InfosLoaded"), std::string::npos);
	EXPECT_NE(Header.find("size_t m_DemoHeaderFetchCursor"), std::string::npos);
	EXPECT_NE(Header.find("size_t m_DemoDateFetchCursor"), std::string::npos);
	EXPECT_NE(Header.find("bool m_DemoHeaderFetchComplete"), std::string::npos);
	EXPECT_NE(Header.find("bool m_DemoDateFetchComplete"), std::string::npos);
	EXPECT_NE(MenusDemo.find("event=demo_browser_header_fetch"), std::string::npos);
	EXPECT_NE(MenusDemo.find("event=demo_browser_date_fetch"), std::string::npos);
	EXPECT_NE(MenusDemo.find("event=demo_browser_preview_load"), std::string::npos);
	EXPECT_NE(MenusDemo.find("metadata_remaining=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoBrowserMetadataPrioritizesVisibleWindow)
{
	const std::string MenusDemo = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string AdvanceBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::AdvanceDemoBrowserMetadata(int HeaderBudget, int DateBudget, const char *pTrigger, int VisibleFirst, int VisibleEnd)");
	const std::string RenderListBody = ExtractSourceFunctionBody(MenusDemo, "void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)");
	ASSERT_FALSE(AdvanceBody.empty());
	ASSERT_FALSE(RenderListBody.empty());

	EXPECT_NE(Header.find("void AdvanceDemoBrowserMetadata(int HeaderBudget, int DateBudget, const char *pTrigger, int VisibleFirst = -1, int VisibleEnd = -1);"), std::string::npos);
	EXPECT_NE(RenderListBody.find("int FirstVisibleIndex = -1;"), std::string::npos);
	EXPECT_NE(RenderListBody.find("EndVisibleIndex = ItemIndex + 1;"), std::string::npos);
	EXPECT_NE(RenderListBody.find("\"list_frame\",\n\t\tFirstVisibleIndex,\n\t\tEndVisibleIndex);"), std::string::npos);
	EXPECT_LT(AdvanceBody.find("for(int i = VisibleFirst; i < VisibleEnd && RemainingBudget > 0; ++i)"), AdvanceBody.find("while(m_DemoHeaderFetchCursor < m_vDemos.size() && RemainingBudget > 0)"));
	EXPECT_LT(AdvanceBody.find("for(int i = VisibleFirst; i < VisibleEnd && RemainingBudget > 0; ++i)", AdvanceBody.find("if(!m_DemoDateFetchComplete")), AdvanceBody.find("while(m_DemoDateFetchCursor < m_vDemos.size() && RemainingBudget > 0)"));
	EXPECT_NE(AdvanceBody.find("std::stable_sort(m_vDemos.begin(), m_vDemos.end());\n\t\t\tDemolistOnUpdate(false);"), std::string::npos);
	EXPECT_NE(AdvanceBody.find("visible_scanned=%d"), std::string::npos);
	EXPECT_NE(AdvanceBody.find("background_scanned=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsAssetsPreviewAdmissionPrefersCombinedVisibleWindow)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("struct SSettingsAssetsVisibleAdmission"), std::string::npos);
	EXPECT_NE(Body.find("SSettingsAssetsVisibleAdmission CombinedVisibleAdmission"), std::string::npos);
	EXPECT_NE(Body.find("visible_first=1"), std::string::npos);
	EXPECT_NE(Body.find("visible_starts=%d prefetch_starts=%d background_starts=%d"), std::string::npos);
	EXPECT_LT(Body.find("SSettingsAssetsVisibleAdmission CombinedVisibleAdmission"), Body.find("StartWorkshopThumb("));
	EXPECT_LT(Body.find("SSettingsAssetsVisibleAdmission CombinedVisibleAdmission"), Body.find("SchedulePreviewRange("));
	EXPECT_EQ(Body.find("StartWorkshopThumb(Asset, SettingsWorkshopThumbShouldStartHighPriority(VisibleDownloadableIndex, FirstVisibleDownloadableIndex, LastVisibleDownloadableIndex));"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsAssetsVisibleReadyPreflightPrecedesDraw)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("enum class EAssetsVisiblePreflightState"), std::string::npos);
	EXPECT_NE(Source.find("struct SSettingsAssetsVisiblePreflight"), std::string::npos);
	EXPECT_NE(Body.find("PrepareAssetsVisibleContentBudgeted("), std::string::npos);
	EXPECT_EQ(Body.find("RenderAssetsVisibleReadySkeleton("), std::string::npos);
	EXPECT_NE(Body.find("stage=assets_visible_preflight"), std::string::npos);
	EXPECT_NE(Body.find("stage=assets_card_geometry"), std::string::npos);
	EXPECT_NE(Body.find("visible_ready=%d geometry_stable=%d thumb_starts_before_visible=%d thumb_starts_during_draw=%d"), std::string::npos);
	EXPECT_NE(Body.find("MaxWorkshopThumbJumpStartsPerFrame"), std::string::npos);
	EXPECT_NE(Body.find("const int WorkshopThumbStartLimitThisFrame = WorkshopListJumpScrollActive ? MaxWorkshopThumbJumpStartsPerFrame : MaxWorkshopThumbStartsPerFrameAdaptive;"), std::string::npos);
	EXPECT_LT(Body.find("PrepareAssetsVisibleContentBudgeted("), Body.find("for(size_t ListIndex = WorkshopVisibleRange.m_FirstItem"));
	const size_t DrawLoop = Body.find("for(size_t ListIndex = WorkshopVisibleRange.m_FirstItem");
	ASSERT_NE(DrawLoop, std::string::npos);
	const size_t DrawLog = Body.find("assets_preview_draw_workshop_cards", DrawLoop);
	ASSERT_NE(DrawLog, std::string::npos);
	const std::string DrawLoopBody = Body.substr(DrawLoop, DrawLog - DrawLoop);
	EXPECT_EQ(DrawLoopBody.find("StartWorkshopThumb("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsUsesAdaptiveBudgetForPreviewAndThumbWork)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("BeginSettingsUiFrameScheduler("), std::string::npos);
	EXPECT_NE(Body.find("AdaptiveBudget.m_VisibleTokens"), std::string::npos);
	EXPECT_NE(Body.find("AdaptiveBudget.m_PrefetchTokens"), std::string::npos);
	EXPECT_NE(Body.find("AdaptiveBudget.m_GpuUploadTokens"), std::string::npos);
	EXPECT_NE(Body.find("const bool AssetsScrollPressure = ResourceFrameContext.m_ScrollActive || ResourceFrameContext.m_JumpScrollActive;"), std::string::npos);
	EXPECT_NE(Body.find("const int AdaptivePrefetchTokens = AssetsScrollPressure ? 0 : AdaptiveBudget.m_PrefetchTokens;"), std::string::npos);
	EXPECT_NE(Body.find("const int AdaptiveBackgroundTokens = AssetsScrollPressure ? 0 : AdaptiveBudget.m_BackgroundTokens;"), std::string::npos);
	EXPECT_NE(Body.find("WorkshopVisibleRange.m_EndItem + (AssetsScrollPressure ? 0 : Columns)"), std::string::npos);
	EXPECT_NE(Menus.find("event=settings_adaptive_budget"), std::string::npos);
	EXPECT_EQ(Body.find("constexpr int MaxWorkshopThumbStartsPerFrame = 16;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsPageSwitchDefersPreviewGpuUploads)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const bool AssetsPageSwitchActive = m_SettingsPageSwitchActive;"), std::string::npos);
	EXPECT_NE(Body.find("const bool AssetsShellOnlyFrame = AssetsTabSwitchFirstFrame || AssetsPageSwitchActive;"), std::string::npos);
	EXPECT_NE(Body.find("const bool AssetsContentWarmupBlocked = AssetsShellOnlyFrame || AssetsScrollPressure;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsUploadBlocked = AssetsContentWarmupBlocked || AssetsDirectScrollUploadBlocked"), std::string::npos);
	EXPECT_NE(Body.find("MaxPreviewUploadsPerFrame = AssetsUploadBlocked ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
	EXPECT_NE(Body.find("MaxWorkshopThumbUploadsPerFrame = AssetsUploadBlocked ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
	EXPECT_EQ(Body.find("const int MaxPreviewUploadsPerFrame = maximum(1, AdaptiveBudget.m_GpuUploadTokens);"), std::string::npos);
	EXPECT_EQ(Body.find("const int MaxWorkshopThumbUploadsPerFrame = maximum(1, AdaptiveBudget.m_GpuUploadTokens);"), std::string::npos);
	EXPECT_EQ(Body.find("m_SettingsFrameBudget.m_MaxGpuUploads = maximum(m_SettingsFrameBudget.m_MaxGpuUploads, AdaptiveBudget.m_GpuUploadTokens);"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsDirectScrollDefersPreviewGpuUploads)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("constexpr int AssetsScrollUploadCooldownFrames = 6;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsDirectScrollUploadBlocked"), std::string::npos);
	EXPECT_NE(Body.find("AssetsUploadBlocked = AssetsContentWarmupBlocked || AssetsDirectScrollUploadBlocked"), std::string::npos);
	EXPECT_NE(Body.find("MaxPreviewUploadsPerFrame = AssetsUploadBlocked ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
	EXPECT_NE(Body.find("const char *pAssetsUploadBlockFrameContext = AssetsDirectScrollUploadBlocked ? \"scroll_cooldown\""), std::string::npos);
	EXPECT_NE(Body.find("pAssetsUploadBlockFrameContext"), std::string::npos);
	EXPECT_NE(Body.find("scroll_upload_cooldown=%d frame_context=%s upload_block=%s"), std::string::npos);
	EXPECT_NE(Body.find("RefreshAssetsScrollUploadCooldownForOffset"), std::string::npos);
	EXPECT_NE(Body.find("RefreshAssetsScrollUploadCooldownForOffset(ListScrollActive, s_ListBox.ScrollOffsetY()"), std::string::npos);
	EXPECT_NE(Body.find("RefreshAssetsScrollUploadCooldownForOffset(WorkshopListScrollActive, s_WorkshopAssetsListBox.ScrollOffsetY()"), std::string::npos);
	EXPECT_NE(Body.find("RefreshAssetsUploadBudget();"), std::string::npos);
	EXPECT_EQ(Body.find("const int MaxPreviewUploadsPerFrame = AssetsPageSwitchActive ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsDirectScrollDefersWorkshopFinalize)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("MaxWorkshopThumbDecodeFinalizesThisFrame = AssetsUploadBlocked ? 0 : MaxWorkshopThumbDecodeFinalizesPerFrame;"), std::string::npos);
	EXPECT_NE(Body.find("MaxWorkshopThumbUploadsPerFrame = AssetsUploadBlocked ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
	EXPECT_NE(Body.find("SettingsResourceFrameStageBudget(FinalizeFrameContext, FinalizePriority, MaxWorkshopThumbDecodeFinalizesThisFrame, 0)"), std::string::npos);
	EXPECT_NE(Body.find("scroll_upload_cooldown=%d"), std::string::npos);
	EXPECT_EQ(Body.find("const int MaxWorkshopThumbUploadsPerFrame = AssetsPageSwitchActive ? 0 : AdaptiveBudget.m_GpuUploadTokens;"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsUiBudgetTelemetryExists)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Assets = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");

	EXPECT_NE(Header.find("struct SSettingsUiBudgetFrame"), std::string::npos);
	EXPECT_NE(Header.find("LogSettingsUiBudget("), std::string::npos);
	const size_t UiBudgetEvent = Menus.find("event=settings_ui_budget");
	ASSERT_NE(UiBudgetEvent, std::string::npos);
	const std::string UiBudgetFormat = Menus.substr(UiBudgetEvent, 512);
	EXPECT_TRUE(ContainsAll(UiBudgetFormat, {"layout_ms=", "text_ms=", "text_new=", "text_reused=", "draw_calls=", "vertices=", "indices=", "heap_allocs=", "visible_widgets="}));
	EXPECT_NE(TClient.find("LogSettingsUiBudget(\"settings:tclient\""), std::string::npos);
	EXPECT_NE(Assets.find("LogSettingsUiBudget(\"settings:assets\""), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsInternalTabSwitchStartsFpsWindow)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("StartSettingsPerfFixedWindow(\"settings_assets_tab_switch\""), std::string::npos);
	EXPECT_NE(Body.find("CurrentQmUiPerfPage()"), std::string::npos);
	EXPECT_NE(Body.find("str_format(aAssetsPerfTab"), std::string::npos);
	EXPECT_NE(Body.find("s_AssetsTabSwitchFirstFrame = 1;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsPerfStagesUseRealFrameId)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");

	EXPECT_NE(Source.find("LogAssetsPerfStageForClient(Client(), pStage, DurationMs, Force, pExtra);"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Source, {
						"LogAssetsFramePerfStage(\"assets_preview_draw_workshop_cards\"",
						"LogAssetsFramePerfStage(\"assets_card_geometry\"",
						"LogAssetsPerfStageForClient(Client(), \"assets_window_focus\"",
						"LogAssetsPerfStageForClient(Client(), \"assets_preview_upload_queue_push\"",
						"LogAssetsPerfStageForClient(Client(), \"assets_workshop_thumb_start_local\"",
						"LogAssetsPerfStageForClient(Client(), \"assets_workshop_thumb_start_remote\"",
					}));
	EXPECT_EQ(Source.find("QmPerfLogStage(\"perf/assets\", pStage, DurationMs, Force, nullptr"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfAnalyzerReportsUiBudgetFields)
{
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Quality = ReadRepoFile("qmclient_scripts/perf/lib/quality.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	EXPECT_NE(Stats.find("export interface SettingsUiBudgetSummary"), std::string::npos);
	EXPECT_NE(Stats.find("settingsUiBudgetSummary(entries"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Stats, {"layout_ms", "text_ms", "draw_calls", "heap_allocs"}));
	EXPECT_NE(Quality.find("settingsUiBudget"), std::string::npos);
	EXPECT_NE(Report.find("Settings UI Budget"), std::string::npos);
	EXPECT_FALSE(ContainsAny(Report, {"APPROXIMATE", "REPORT ONLY", "Draw Calls Est.", "Vertices Est.", "Indices Est.", "占位观测", "不代表本轮已做通用文本渲染优化"}));
	EXPECT_NE(Tests.find("testSettingsUiBudgetFieldsAppearInSummaryAndReport"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfTelemetryOverheadIsAttributedByFrameWindow)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");
	const std::string Body = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	ASSERT_FALSE(Body.empty());

	// Text/glyph telemetry is useful only if its own frame-tail flush can be
	// blamed when it becomes the problem. This prevents low-FPS windows from
	// falling back to attribution=none or hiding profiler overhead inside UI total.
	EXPECT_NE(Body.find("TextRender()->FlushQmTextRuntimeBudgetLog();"), std::string::npos);
	EXPECT_NE(Body.find("LogPerfStage(Client(), \"telemetry_flush\", StageTimer.ElapsedMs());"), std::string::npos);
	EXPECT_NE(Stats.find("telemetry_overhead"), std::string::npos);
	EXPECT_NE(Stats.find("telemetry_flush"), std::string::npos);
	EXPECT_NE(Report.find("Telemetry Flush"), std::string::npos);
	EXPECT_NE(Tests.find("testPerfOverheadIsReportedAsCulprit"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientSettingsTab0StableTextKeysMatchPlan)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");

	EXPECT_NE(Menus.find("BuildTClientSettingsMenuTextPlan(vItems, MainView, m_TClientSettingsTab);"), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-prediction-margin\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-outline-opacity\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-outline-solid-opacity\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-outline-width\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-player-indicator-title\""), std::string::npos);

	EXPECT_EQ(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-prediction-margin\""), std::string::npos);
	EXPECT_EQ(Source.find("SettingsTextElement(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-player-indicator-title\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientTab0StableTextKeysMatchLatestLogSamples)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");

	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-antiping-uncertainty-scale\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsScrollbarOption(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-auto-vote-minimum-time\""), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, \"tclient-auto-reply-title\""), std::string::npos);
	EXPECT_NE(Source.find("\"tclient-antiping-uncertainty-scale\""), std::string::npos);
	EXPECT_NE(Source.find("\"tclient-auto-vote-minimum-time\""), std::string::npos);
	EXPECT_NE(Source.find("\"tclient-auto-reply-title\""), std::string::npos);
	EXPECT_EQ(Source.find("SettingsTextElement(SETTINGS_TCLIENT, m_TClientSettingsTab, \"tclient-auto-reply-title\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientPrewarmDoesNotRunUnboundedInVisibleTargetFrame)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const bool TClientVisibleTargetFrame = !PrewarmOnly"), std::string::npos);
	EXPECT_NE(Body.find("SetProgressiveEnabled(TClientVisibleTargetFrame)"), std::string::npos);
	EXPECT_NE(Body.find("SetMaxSectionsPerFrame(TClientVisibleTargetFrame ?"), std::string::npos);
	EXPECT_NE(Body.find("tclient_settings_left_prewarm_budgeted"), std::string::npos);
	EXPECT_NE(Body.find("tclient_settings_right_prewarm_budgeted"), std::string::npos);
	EXPECT_EQ(Body.find("s_VisualFontLoader.SetProgressiveEnabled(false);"), std::string::npos);
	EXPECT_EQ(Body.find("s_RightSectionLoader.SetProgressiveEnabled(false);"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientSectionMeasuredHeightMatchesRenderedHeight)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("LogTClientSectionHeightConsistency"), std::string::npos);
	EXPECT_NE(Body.find("section_height_measured=%.3f section_height_rendered=%.3f height_delta=%.3f"), std::string::npos);
	EXPECT_NE(Body.find("MeasuredHeight = "), std::string::npos);
	EXPECT_NE(Body.find("RenderedHeight = "), std::string::npos);
	EXPECT_NE(Body.find("absolute(HeightDelta) <= 0.01f"), std::string::npos);
	EXPECT_NE(Body.find("RenderBoxedFullSection"), std::string::npos);
	EXPECT_NE(Body.find("FillCachedStaticLayer"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsWorkshopCardDrawHasSubstageTelemetry)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("assets_preview_draw_workshop_cards_layout_text"), std::string::npos);
	EXPECT_NE(Body.find("assets_preview_draw_workshop_cards_preview_draw"), std::string::npos);
	EXPECT_NE(Body.find("assets_preview_draw_workshop_cards_thumb_scheduling"), std::string::npos);
	EXPECT_NE(Body.find("layout_text_ms=%.3f preview_draw_ms=%.3f thumb_scheduling_ms=%.3f"), std::string::npos);
	EXPECT_NE(Body.find("char aExtra[512];"), std::string::npos);
	EXPECT_NE(Body.find("CardLayoutTextTimer"), std::string::npos);
	EXPECT_NE(Body.find("CardPreviewDrawTimer"), std::string::npos);
	EXPECT_NE(Body.find("ThumbSchedulingTimer"), std::string::npos);
	EXPECT_NE(Body.find("WorkshopCardLayoutTextMs += CardLayoutTextTimer.ElapsedMs();"), std::string::npos);
	EXPECT_NE(Body.find("WorkshopCardPreviewDrawMs += CardPreviewDrawTimer.ElapsedMs();"), std::string::npos);
	EXPECT_NE(Body.find("const double PreflightMs = ThumbSchedulingTimer.ElapsedMs();"), std::string::npos);
	EXPECT_NE(Body.find("WorkshopCardThumbSchedulingMs += PreflightMs;"), std::string::npos);
	const size_t PreparePos = Body.find("auto PrepareAssetsVisibleContentBudgeted =");
	const size_t DrawLoopPos = Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)");
	ASSERT_NE(PreparePos, std::string::npos);
	ASSERT_NE(DrawLoopPos, std::string::npos);
	EXPECT_LT(PreparePos, DrawLoopPos);
	EXPECT_NE(Body.find("UiBudget.m_LayoutMs = WorkshopCardsTimer.ElapsedMs();"), std::string::npos);
	EXPECT_NE(Body.find("UiBudget.m_TextReused = 0;"), std::string::npos);
	EXPECT_EQ(Body.find("WorkshopCardLoopMs * 0.5"), std::string::npos);
	EXPECT_EQ(Body.find("WorkshopCardLoopMs - WorkshopCardLayoutTextMs"), std::string::npos);
	EXPECT_EQ(Body.find("UiBudget.m_LayoutMs = AssetsUiBudgetTimer.ElapsedMs();"), std::string::npos);
	EXPECT_EQ(Body.find("UiBudget.m_TextReused = WorkshopVisibleRange.m_RenderedItems;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchLimitsFirstFrameVisibleThumbStarts)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("constexpr int MaxAssetsTabSwitchVisibleThumbStartsFirstFrame = 2;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsTabSwitchFirstFrame"), std::string::npos);
	EXPECT_NE(Body.find("MaxAssetsTabSwitchVisibleThumbStartsFirstFrame"), std::string::npos);
	EXPECT_NE(Body.find("tab_switch_first_frame=%d"), std::string::npos);
	EXPECT_NE(Body.find("visible_thumb_start_limit=%d"), std::string::npos);
	EXPECT_NE(Body.find("CombinedVisibleAdmission.m_VisibleStarts >= VisibleThumbStartLimitThisFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchKeepsVisibleThumbStartsCappedForCooldown)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("constexpr int AssetsTabSwitchCooldownFrames"), std::string::npos);
	EXPECT_NE(Body.find("s_AssetsTabSwitchCooldownFrames = AssetsTabSwitchCooldownFrames;"), std::string::npos);
	EXPECT_NE(Body.find("const bool AssetsTabSwitchCooldownActive = s_AssetsTabSwitchCooldownFrames > 0;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsTabSwitchCooldownActive ? MaxAssetsTabSwitchVisibleThumbStartsPerFrame : MaxWorkshopThumbStartsPerFrameAdaptive"), std::string::npos);
	EXPECT_NE(Body.find("tab_switch_cooldown_frames=%d"), std::string::npos);
	EXPECT_NE(Body.find("--s_AssetsTabSwitchCooldownFrames;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntityBgInstalledThumbsRespectTabSwitchVisibleCap)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("int &VisibleStartsThisFrame"), std::string::npos);
	EXPECT_NE(Source.find("int MaxVisibleStartsPerFrame"), std::string::npos);
	EXPECT_NE(Source.find("if(HighPriority && VisibleStartsThisFrame >= MaxVisibleStartsPerFrame)"), std::string::npos);
	EXPECT_NE(Source.find("++VisibleStartsThisFrame;"), std::string::npos);
	EXPECT_NE(Body.find("CombinedVisibleAdmission.m_VisibleStarts"), std::string::npos);
	EXPECT_NE(Body.find("VisibleThumbStartLimitThisFrame"), std::string::npos);
	EXPECT_NE(Body.find("CombinedVisibleAdmission.m_VisibleStarts,\n\t\t\t\t\t   VisibleThumbStartLimitThisFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchDoesNotTransformCardGeometry)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("TriggerUiSwitchAnimation(AssetsTabSwitchNode"), std::string::npos);
	EXPECT_NE(Body.find("AssetsTabSwitchFirstFrame"), std::string::npos);
	EXPECT_EQ(Body.find("ApplyUiSwitchOffset(MainView, TransitionStrength, s_AssetsTransitionDirection"), std::string::npos);
	EXPECT_EQ(Body.find("ApplyUiSwitchOffset(MainView"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardShellFirstRenderingExists)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("enum class ESettingsAssetsCardHydrationLayer"), std::string::npos);
	EXPECT_NE(Source.find("SSettingsAssetsCardShell"), std::string::npos);
	EXPECT_NE(Source.find("SSettingsAssetsCardMetadataCacheEntry"), std::string::npos);
	EXPECT_NE(Source.find("SSettingsAssetsCardPreviewState"), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardShell("), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardMetadataCached("), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardPreview("), std::string::npos);
	EXPECT_LT(Body.find("RenderAssetsCardShell("), Body.find("RenderAssetsCardMetadataCached("));
	EXPECT_LT(Body.find("RenderAssetsCardShell("), Body.find("RenderAssetsCardPreview("));
}

TEST(QmMonitoringHelpers, AssetsCardMetadataUsesDedicatedCache)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("struct SSettingsAssetsCardCacheKey"), std::string::npos);
	EXPECT_NE(Source.find("m_AssetId"), std::string::npos);
	EXPECT_NE(Source.find("m_Tab"), std::string::npos);
	EXPECT_NE(Source.find("m_LocaleHash"), std::string::npos);
	EXPECT_NE(Source.find("m_UiScale"), std::string::npos);
	EXPECT_NE(Source.find("m_CardWidth"), std::string::npos);
	EXPECT_NE(Source.find("m_StatusHash"), std::string::npos);
	EXPECT_NE(Source.find("m_Installed"), std::string::npos);
	EXPECT_NE(Source.find("m_DownloadFailed"), std::string::npos);
	EXPECT_NE(Source.find("m_LocalOnly"), std::string::npos);
	EXPECT_NE(Source.find("SETTINGS_ASSETS_CARD_METADATA_CACHE_MAX_ENTRIES"), std::string::npos);
	EXPECT_NE(Source.find("TrimAssetsCardMetadataCacheForInsert("), std::string::npos);
	EXPECT_NE(Source.find("static std::unordered_map<SSettingsAssetsCardCacheKey, SSettingsAssetsCardMetadataCacheEntry"), std::string::npos);
	EXPECT_NE(Source.find("BuildAssetsCardCacheKey("), std::string::npos);
	EXPECT_NE(Source.find("Key.m_StatusHash = str_quickhash"), std::string::npos);
	EXPECT_NE(Source.find("FindAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Source.find("HydrateAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Source.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_EQ(Source.find("RefreshAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Body.find("SSettingsAssetsCardMetadataCacheEntry *pMetadata = FindAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Body.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(Body.find("pMetadata = HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_EQ(Body.find("RefreshAssetsCardMetadata(*pMetadata"), std::string::npos);
	EXPECT_EQ(Source.find("RegisterMenuTextPlanItem(\"assets-card"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsMetadataRequestDoesNotHydrateInRenderPath)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string RequestBody = ExtractSourceFunctionBody(Source, "static void RequestAssetsCardMetadataHydration");
	ASSERT_FALSE(RequestBody.empty());

	EXPECT_NE(Source.find("struct SSettingsAssetsCardMetadataRequest"), std::string::npos);
	EXPECT_NE(Source.find("QueueAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_NE(RequestBody.find("QueueAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(RequestBody.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsMetadataHydratesOnlyThroughBudgetDrain)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "static int DrainAssetsCardMetadataHydrationRequests");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(Body.find("const int AssetsInitialMetadataLayoutTokens = maximum(1, minimum(AdaptiveBudget.m_VisibleTokens, 4));"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsMetadataLayoutTokensThisFrame = AssetsShellOnlyFrame ? AssetsInitialMetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Body.find("DrainAssetsCardMetadataHydrationRequests(AssetsMetadataLayoutTokensThisFrame"), std::string::npos);
	EXPECT_NE(DrainBody.find("while(MetadataLayoutTokens > 0"), std::string::npos);
	EXPECT_NE(DrainBody.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsScrollPressureBlocksContentHydrationWork)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Fast scroll/jump scroll drops frames when preview/decode/upload work is
	// allowed in the same render frame. Metadata gets a tiny visible-only budget
	// so labels do not disappear while holding the scrollbar.
	EXPECT_NE(Body.find("const bool AssetsContentWarmupBlocked = AssetsShellOnlyFrame || AssetsScrollPressure;"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsMetadataLayoutTokensThisFrame = AssetsShellOnlyFrame ? AssetsInitialMetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsPreviewArtifactTokensThisFrame = AssetsContentWarmupBlocked ? 0 : AdaptivePreviewArtifactTokens;"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsTextureUploadTokensThisFrame = AssetsContentWarmupBlocked ? 0 : AdaptiveTextureUploadTokens;"), std::string::npos);
	EXPECT_NE(Body.find("const int MaxPreviewDecodeStartsPerFrame = AssetsContentWarmupBlocked ? 0 : maximum(1, AdaptiveBudget.m_VisibleTokens + AdaptivePrefetchTokens + AdaptiveBackgroundTokens);"), std::string::npos);
	EXPECT_NE(Body.find("const int MaxWorkshopThumbStartsPerFrameAdaptive = AssetsContentWarmupBlocked ? 0 : maximum(1, AdaptiveBudget.m_VisibleTokens + AdaptivePrefetchTokens + AdaptiveBackgroundTokens);"), std::string::npos);
	EXPECT_NE(Body.find("const int VisibleThumbStartLimitThisFrame = AssetsContentWarmupBlocked ? 0 :"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsScrollPressureStillRendersMetadataFallback)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Regression guard: holding the scrollbar must not hide card titles. The
	// immediate fallback uses fixed card geometry, so it remains readable without
	// waiting for streamed text containers to hydrate.
	EXPECT_NE(Body.find("const bool AssetsRenderCardMetadataFallback = !AssetsShellOnlyFrame;"), std::string::npos);
	EXPECT_NE(Body.find("else if(AssetsRenderCardMetadataFallback)"), std::string::npos);
	EXPECT_NE(Body.find("pMetadata != nullptr && AssetsContentWarmupBlocked"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsScrollPressureSkipsPreviewSchedulingAndUploads)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Scroll pressure is the path the user stress-tested: scheduling/decode/upload
	// work must not run in the same frame as a fast list movement.
	EXPECT_NE(Body.find("if(!AssetsContentWarmupBlocked)"), std::string::npos);
	EXPECT_NE(Body.find("PrepareAssetsLocalVisibleContentBudgeted();"), std::string::npos);
	EXPECT_NE(Body.find("PrepareAssetsVisibleContentBudgeted();"), std::string::npos);
	EXPECT_NE(Body.find("ProcessAssetsResourcePreviewJobs(Graphics(), ResourcePreviewTelemetry, ResourcePreviewUploadBudget);"), std::string::npos);
	EXPECT_NE(Body.find("if(!AssetsContentWarmupBlocked && FirstVisibleIndex >= 0)"), std::string::npos);
	EXPECT_LT(Body.find("if(!AssetsContentWarmupBlocked)"), Body.find("ProcessAssetsResourcePreviewJobs(Graphics(), ResourcePreviewTelemetry, ResourcePreviewUploadBudget);"));
}

TEST(QmMonitoringHelpers, AssetsTabSwitchFirstFrameDoesNotStartThumbsOrUploads)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const bool AssetsShellOnlyFrame = AssetsTabSwitchFirstFrame || AssetsPageSwitchActive;"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsTextureUploadTokensThisFrame = AssetsContentWarmupBlocked ? 0 : AdaptiveTextureUploadTokens;"), std::string::npos);
	EXPECT_NE(Body.find("if(!AssetsContentWarmupBlocked)\n\t\t\t\t\t\t\tSchedulePreviewRange("), std::string::npos);
	EXPECT_NE(Body.find("if(!AssetsContentWarmupBlocked && StartWorkshopThumb(Asset, Visible))"), std::string::npos);
	EXPECT_NE(Body.find("if(!AssetsContentWarmupBlocked)"), std::string::npos);
	EXPECT_NE(Body.find("PrepareAssetsVisibleContentBudgeted();"), std::string::npos);
	EXPECT_NE(Body.find("ProcessAssetsResourcePreviewJobs(Graphics(), ResourcePreviewTelemetry, ResourcePreviewUploadBudget);"), std::string::npos);
	EXPECT_LT(Body.find("if(!AssetsContentWarmupBlocked)"), Body.find("ProcessAssetsResourcePreviewJobs(Graphics(), ResourcePreviewTelemetry, ResourcePreviewUploadBudget);"));
	EXPECT_NE(Body.find("metadata_hydrated=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsDrawLoopOnlyUsesReadyContent)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	const size_t DrawLoopStart = Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)");
	ASSERT_NE(DrawLoopStart, std::string::npos);
	const size_t DrawLoopEnd = Body.find("s_WorkshopAssetsListBox.SkipItems((int)CombinedCount - WorkshopVisibleRange.m_EndItem)", DrawLoopStart);
	ASSERT_NE(DrawLoopEnd, std::string::npos);
	const std::string DrawLoopBody = Body.substr(DrawLoopStart, DrawLoopEnd - DrawLoopStart);

	EXPECT_NE(DrawLoopBody.find("FindAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(DrawLoopBody.find("RenderAssetsCardMetadataCached("), std::string::npos);
	EXPECT_NE(DrawLoopBody.find("RenderAssetsCardPreview("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("StartWorkshopThumb("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("SchedulePreviewRange("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("ProcessAssetsResourcePreviewJobs("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("DrainSharedResourcePreviewUploadQueue("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsVisiblePreflightRunsOutsideCardDrawLoop)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	const size_t PreparePos = Body.find("PrepareAssetsVisibleContentBudgeted(");
	const size_t DrawLoopStart = Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)");
	ASSERT_NE(PreparePos, std::string::npos);
	ASSERT_NE(DrawLoopStart, std::string::npos);

	// The preflight is the only place that may schedule visible/near-visible
	// content. If this work drifts back into the card loop, tab switches still
	// stall even though the shell-first layer exists.
	EXPECT_LT(PreparePos, DrawLoopStart);
	EXPECT_EQ(Body.find("RunAssetsVisibleReadyPreflight();"), std::string::npos);
	EXPECT_NE(Body.find("LogAssetsFramePerfStage(\"assets_visible_preflight\""), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardDrawLoopDoesNotStartThumbsPreviewJobsOrUploads)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	const size_t DrawLoopStart = Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)");
	ASSERT_NE(DrawLoopStart, std::string::npos);
	const size_t DrawLoopEnd = Body.find("s_WorkshopAssetsListBox.SkipItems((int)CombinedCount - WorkshopVisibleRange.m_EndItem)", DrawLoopStart);
	ASSERT_NE(DrawLoopEnd, std::string::npos);
	const std::string DrawLoopBody = Body.substr(DrawLoopStart, DrawLoopEnd - DrawLoopStart);

	// Regression guard for the broken previews/default tee issue: the draw loop
	// may only consume ready state. It must not start jobs, uploads, or thumbs.
	EXPECT_EQ(DrawLoopBody.find("StartWorkshopThumb("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("SchedulePreviewRange("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("StartAssetsEntityBgPreviewArtifactJob("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("ProcessAssetsResourcePreviewJobs("), std::string::npos);
	EXPECT_EQ(DrawLoopBody.find("SettingsResourcePreviewConsumeUploadBudget("), std::string::npos);
}

TEST(QmMonitoringHelpers, EntityBgPreviewArtifactBackfillsReadyTexture)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string PreviewSource = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Entity BG needs its own tile/artifact preview, not a permanent placeholder.
	// The artifact job must mark CPU artifact ready and the upload drain must
	// backfill a texture that the card renderer can draw in later frames.
	EXPECT_NE(Source.find("StartAssetsEntityBgPreviewArtifactJob("), std::string::npos);
	EXPECT_NE(Source.find("gs_SettingsAssetsResourcePreviewCache.MarkArtifactReady("), std::string::npos);
	EXPECT_NE(PreviewSource.find("m_TextureReady = true"), std::string::npos);
	EXPECT_NE(Body.find("if(pPipelineState != nullptr && pPipelineState->m_Texture.IsValid())"), std::string::npos);
	const size_t FirstPrepare = std::min(Body.find("auto PrepareAssetsLocalVisibleContentBudgeted ="), Body.find("auto PrepareAssetsVisibleContentBudgeted ="));
	ASSERT_NE(FirstPrepare, std::string::npos);
	EXPECT_LT(FirstPrepare, Body.find("StartAssetsEntityBgPreviewArtifactJob("));
	EXPECT_EQ(Body.substr(Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)"), Body.find("s_WorkshopAssetsListBox.SkipItems((int)CombinedCount - WorkshopVisibleRange.m_EndItem)") - Body.find("s_WorkshopAssetsListBox.SkipItems(WorkshopVisibleRange.m_FirstItem)")).find("StartAssetsEntityBgPreviewArtifactJob("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntityBgPreviewJobUsesStableKeyNotItemPointer)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string StartBody = ExtractSourceFunctionBody(Source, "static bool StartAssetsEntityBgPreviewArtifactJob");
	ASSERT_FALSE(StartBody.empty());

	// Preview jobs may finish after a fast scroll, list rebuild, or tab switch.
	// They must be keyed by stable resource identity rather than retaining a
	// pointer to a card/list item whose storage can move or disappear.
	EXPECT_EQ(Source.find("StartAssetsEntityBgPreviewArtifactJob(const SResourcePreviewKey &PreviewKey, CMenus::SCustomItem *pItem"), std::string::npos);
	EXPECT_NE(Source.find("StartAssetsEntityBgPreviewArtifactJob(const SResourcePreviewKey &PreviewKey, const char *pAssetName"), std::string::npos);
	EXPECT_EQ(StartBody.find("pItem->"), std::string::npos);
	EXPECT_NE(StartBody.find("ResolveEntityBgPreviewArtifactSource(pStorage, pAssetName"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardHydrationSchedulerDefersContentAfterTabSwitch)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("struct SSettingsAssetsCardHydrationScheduler"), std::string::npos);
	EXPECT_NE(Source.find("BeginAssetsCardHydrationFrame("), std::string::npos);
	EXPECT_NE(Source.find("CanHydrateMetadata("), std::string::npos);
	EXPECT_NE(Source.find("CanRenderMetadata("), std::string::npos);
	EXPECT_NE(Source.find("CanHydratePreview("), std::string::npos);
	EXPECT_NE(Source.find("CanRenderPreview("), std::string::npos);
	EXPECT_NE(Source.find("m_TabSwitchShellOnlyFrame"), std::string::npos);
	EXPECT_NE(Body.find("SSettingsAssetsCardHydrationScheduler CardHydrationScheduler = BeginAssetsCardHydrationFrame("), std::string::npos);
	EXPECT_NE(Body.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.CanRenderPreview("), std::string::npos);
	EXPECT_EQ(Body.find("RenderAssetsCardPreview(Shell, PreviewState, true, CardHydrationScheduler.CanHydratePreview("), std::string::npos);
	EXPECT_EQ(Body.find("const bool RenderMetadata = CardHydrationScheduler.CanRenderMetadata(CombinedVisible, MetadataCached) &&"), std::string::npos);
	EXPECT_EQ(Body.find("(MetadataCached || PreviewPipelineScheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::VISIBLE));"), std::string::npos);
	EXPECT_NE(Body.find("tab_switch_shell_only=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardPreviewDrawDoesNotStarveAfterBudget)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("bool CanRenderPreview(bool Visible, bool HasPreviewContent, bool HeavyPreviewDeferred = false)"), std::string::npos);
	EXPECT_NE(Source.find("if(!HasPreviewContent)"), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.CanRenderPreview(CombinedVisible, PreviewReady, PreviewState.m_EntityBgHeavyPreviewDeferred)"), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.CanRenderPreview(CombinedVisible, PreviewReady)"), std::string::npos);
	EXPECT_EQ(Body.find("CardHydrationScheduler.CanHydratePreview(CombinedVisible, PreviewState.m_Texture.IsValid() || PreviewState.m_DrawEntityTileArtifact)"), std::string::npos);
	EXPECT_EQ(Body.find("CardHydrationScheduler.CanHydratePreview(CombinedVisible, PreviewReady)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardPreviewHeavyPathLeavesDrawLoop)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("EnsureAssetsCardPreviewArtifact("), std::string::npos);
	EXPECT_NE(Source.find("RenderAssetsEntityTilePreviewArtifact("), std::string::npos);
	EXPECT_NE(Body.find("EnsureAssetsCardPreviewArtifact("), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardPreview("), std::string::npos);
	EXPECT_NE(Source.find("auto RenderAssetsCardShell = [&](const SSettingsAssetsCardShell &Shell)"), std::string::npos);
	EXPECT_NE(Source.find("ShellRect.Draw(ColorRGBA(0.03f, 0.04f, 0.06f, 0.16f), IGraphics::CORNER_ALL, 10.0f);"), std::string::npos);
	const size_t ShellStart = Source.find("auto RenderAssetsCardShell = [&](const SSettingsAssetsCardShell &Shell)");
	ASSERT_NE(ShellStart, std::string::npos);
	const size_t ShellEnd = Source.find("};", ShellStart);
	ASSERT_NE(ShellEnd, std::string::npos);
	const std::string ShellBody = Source.substr(ShellStart, ShellEnd - ShellStart);
	EXPECT_EQ(ShellBody.find("DrawPreviewFrame(Shell.m_TextureRect);"), std::string::npos);
	EXPECT_EQ(Body.find("static const int COLS = 7, ROWS = 7;"), std::string::npos);
	EXPECT_EQ(Body.find("for(int r = 0; r < ROWS; r++)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntityBgPreviewHeavyPathDeferredDuringTabSwitchCooldown)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("m_EntityBgHeavyPreviewDeferred"), std::string::npos);
	EXPECT_NE(Body.find("const bool DeferEntityBgHeavyPreview = s_CurCustomTab == ASSETS_TAB_ENTITY_BG && AssetsTabSwitchCooldownActive"), std::string::npos);
	EXPECT_NE(Body.find("PreviewState.m_EntityBgHeavyPreviewDeferred = DeferEntityBgHeavyPreview"), std::string::npos);
	EXPECT_NE(Body.find("++EntityBgHeavyPreviewDeferredCount;"), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.CanRenderPreview(CombinedVisible, PreviewReady, PreviewState.m_EntityBgHeavyPreviewDeferred)"), std::string::npos);
	EXPECT_NE(Body.find("entity_bg_preview_deferred=%d"), std::string::npos);
	EXPECT_NE(Body.find("entity_bg_preview_deferred_count=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewPipelineExists)
{
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string CMake = ReadRepoFile("CMakeLists.txt");

	EXPECT_NE(Header.find("struct SResourcePreviewKey"), std::string::npos);
	EXPECT_NE(Header.find("struct SResourcePreviewState"), std::string::npos);
	EXPECT_NE(Header.find("class CSettingsResourcePreviewCache"), std::string::npos);
	EXPECT_NE(Header.find("class CSettingsResourcePreviewScheduler"), std::string::npos);
	EXPECT_NE(Header.find("class CSettingsResourcePreviewJob"), std::string::npos);
	EXPECT_NE(Header.find("enum class ESettingsResourcePreviewPriority"), std::string::npos);
	EXPECT_NE(Header.find("VISIBLE"), std::string::npos);
	EXPECT_NE(Header.find("NEAR_VISIBLE"), std::string::npos);
	EXPECT_NE(Header.find("BACKGROUND"), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewScheduler::BeginFrame"), std::string::npos);
	const std::string BeginFrameBody = ExtractSourceFunctionBody(Source, "void CSettingsResourcePreviewScheduler::BeginFrame(int VisibleBudget, int NearVisibleBudget, int BackgroundBudget, int UploadBudget)");
	ASSERT_FALSE(BeginFrameBody.empty());
	EXPECT_EQ(BeginFrameBody.find("CanStartPreviewJob("), std::string::npos);
	EXPECT_NE(CMake.find("components/qmclient/settings_resource_preview.cpp"), std::string::npos);
	EXPECT_NE(CMake.find("src/game/client/components/qmclient/settings_resource_preview.cpp"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewPipelineHasRealArtifactAndUploadScheduler)
{
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");

	EXPECT_NE(Header.find("struct SResourcePreviewArtifact"), std::string::npos);
	EXPECT_NE(Header.find("class CSettingsResourcePreviewUploadScheduler"), std::string::npos);
	EXPECT_NE(Header.find("EnqueueUpload(const SResourcePreviewKey &Key, CImageInfo &&Image"), std::string::npos);
	EXPECT_NE(Header.find("Drain(SResourcePreviewUploadBudget &Budget, SResourcePreviewTelemetry &Telemetry"), std::string::npos);
	EXPECT_NE(Header.find("m_vUploadQueue"), std::string::npos);
	EXPECT_NE(Header.find("m_ArtifactReady"), std::string::npos);
	EXPECT_NE(Header.find("m_UploadQueueDepth"), std::string::npos);
	EXPECT_NE(Header.find("m_UploadBudgetExhausted"), std::string::npos);

	EXPECT_NE(Source.find("BuildPreviewArtifact("), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewJob::CSettingsResourcePreviewJob(std::string Name, CImageInfo &&Image, int TargetSize)"), std::string::npos);
	EXPECT_NE(Source.find("Result.m_Artifact = BuildPreviewArtifact(std::move(m_InputImage), m_TargetSize);"), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewUploadScheduler::Drain"), std::string::npos);
	EXPECT_NE(Source.find("SettingsResourcePreviewConsumeUploadBudget(Budget"), std::string::npos);
	EXPECT_NE(Source.find("SettingsResourcePreviewCommitUploadBudget(Budget"), std::string::npos);
	EXPECT_NE(Source.find("if(pGraphics == nullptr)"), std::string::npos);
	EXPECT_NE(Source.find("Telemetry.m_UploadQueueDepth = (int)m_vUploadQueue.size();"), std::string::npos);
	EXPECT_NE(Source.find("if(Texture.IsValid())"), std::string::npos);
	EXPECT_NE(Source.find("Cache.MarkUploadFailed(Item.m_Key);"), std::string::npos);
	EXPECT_EQ(Source.find("Result.m_Image = std::move(m_InputImage);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewSchedulerZeroBudgetDoesNotAdmitWork)
{
	CSettingsResourcePreviewScheduler Scheduler;
	Scheduler.BeginFrame(0, 0, 0, 0);

	EXPECT_FALSE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::VISIBLE));
	EXPECT_FALSE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::NEAR_VISIBLE));
	EXPECT_FALSE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::BACKGROUND));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::VISIBLE));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::NEAR_VISIBLE));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::BACKGROUND));
	EXPECT_FALSE(Scheduler.CanUploadPreview());
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewShellOnlyBlocksPreviewJobsAndUploads)
{
	CSettingsResourcePreviewScheduler Scheduler;
	Scheduler.BeginFrame(2, 2, 2, 2);
	Scheduler.SetShellOnlyFrame(true);

	EXPECT_TRUE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::VISIBLE));
	EXPECT_FALSE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::NEAR_VISIBLE));
	EXPECT_FALSE(Scheduler.CanHydrateMetadata(ESettingsResourcePreviewPriority::BACKGROUND));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::VISIBLE));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::NEAR_VISIBLE));
	EXPECT_FALSE(Scheduler.CanStartPreviewJob(ESettingsResourcePreviewPriority::BACKGROUND));
	EXPECT_FALSE(Scheduler.CanUploadPreview());
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewUploadBudgetUsesSharedLimiterBeforeLocalBudget)
{
	CGpuUploadLimiter Limiter;
	Limiter.OnFrameStart(0);
	SSettingsResourceMergeBudget MergeBudget;
	MergeBudget.m_MaxGpuUploads = 1;
	SResourcePreviewUploadBudget PreviewBudget;
	PreviewBudget.m_MaxUploads = 1;
	PreviewBudget.m_pMergeBudget = &MergeBudget;
	PreviewBudget.m_pGpuUploadLimiter = &Limiter;

	EXPECT_FALSE(SettingsResourcePreviewConsumeUploadBudget(PreviewBudget));
	EXPECT_EQ(PreviewBudget.m_UploadsUsed, 0);
	EXPECT_EQ(MergeBudget.m_MaxGpuUploads, 1);
	EXPECT_EQ(Limiter.UploadsThisFrame(), 0);

	Limiter.OnFrameStart(1);
	EXPECT_TRUE(SettingsResourcePreviewConsumeUploadBudget(PreviewBudget));
	EXPECT_EQ(PreviewBudget.m_UploadsUsed, 1);
	EXPECT_EQ(MergeBudget.m_MaxGpuUploads, 0);
	SettingsResourcePreviewCommitUploadBudget(PreviewBudget);
	EXPECT_EQ(Limiter.UploadsThisFrame(), 1);
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewRejectsInvalidUploadImagesBeforeBudget)
{
	uint8_t aPixel[4] = {255, 255, 255, 255};

	CImageInfo MissingData;
	MissingData.m_Width = 1;
	MissingData.m_Height = 1;
	MissingData.m_Format = CImageInfo::FORMAT_RGBA;
	EXPECT_FALSE(SettingsResourcePreviewImageValidForUpload(MissingData));

	CImageInfo ZeroWidth;
	ZeroWidth.m_Width = 0;
	ZeroWidth.m_Height = 1;
	ZeroWidth.m_Format = CImageInfo::FORMAT_RGBA;
	ZeroWidth.m_pData = aPixel;
	EXPECT_FALSE(SettingsResourcePreviewImageValidForUpload(ZeroWidth));

	CImageInfo ZeroHeight;
	ZeroHeight.m_Width = 1;
	ZeroHeight.m_Height = 0;
	ZeroHeight.m_Format = CImageInfo::FORMAT_RGBA;
	ZeroHeight.m_pData = aPixel;
	EXPECT_FALSE(SettingsResourcePreviewImageValidForUpload(ZeroHeight));

	CImageInfo ValidImage;
	ValidImage.m_Width = 1;
	ValidImage.m_Height = 1;
	ValidImage.m_Format = CImageInfo::FORMAT_RGBA;
	ValidImage.m_pData = aPixel;
	EXPECT_TRUE(SettingsResourcePreviewImageValidForUpload(ValidImage));
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewDrainRejectsInvalidImagesBeforeConsumingBudget)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "int CSettingsResourcePreviewUploadScheduler::Drain(SResourcePreviewUploadBudget &Budget, SResourcePreviewTelemetry &Telemetry, CSettingsResourcePreviewCache &Cache, IGraphics *pGraphics)");
	const std::string DrainOneBody = ExtractSourceFunctionBody(Source, "bool CSettingsResourcePreviewUploadScheduler::DrainOne(SResourcePreviewUploadBudget &Budget, SResourcePreviewTelemetry &Telemetry, CSettingsResourcePreviewCache &Cache, IGraphics *pGraphics)");
	ASSERT_FALSE(DrainBody.empty());
	ASSERT_FALSE(DrainOneBody.empty());

	EXPECT_NE(DrainBody.find("DrainOne(Budget, Telemetry, Cache, pGraphics)"), std::string::npos);
	const size_t ValidatePos = DrainOneBody.find("!SettingsResourcePreviewImageValidForUpload(Item.m_Image)");
	const size_t ConsumePos = DrainOneBody.find("SettingsResourcePreviewConsumeUploadBudget(Budget");
	ASSERT_NE(ValidatePos, std::string::npos);
	ASSERT_NE(ConsumePos, std::string::npos);
	EXPECT_LT(ValidatePos, ConsumePos);
	EXPECT_NE(DrainOneBody.find("Cache.MarkUploadFailed(Item.m_Key);"), std::string::npos);
	EXPECT_NE(DrainOneBody.find("Item.m_Finalize(false, IGraphics::CTextureHandle())"), std::string::npos);
	EXPECT_NE(DrainOneBody.find("m_vUploadQueue.push_front(std::move(Item));"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsResourcePreviewUploadBudgetExhaustionRequeuesWithoutFailure)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string DrainOneBody = ExtractSourceFunctionBody(Source, "bool CSettingsResourcePreviewUploadScheduler::DrainOne(SResourcePreviewUploadBudget &Budget, SResourcePreviewTelemetry &Telemetry, CSettingsResourcePreviewCache &Cache, IGraphics *pGraphics)");
	ASSERT_FALSE(DrainOneBody.empty());

	const size_t BudgetExhaustedPos = DrainOneBody.find("!SettingsResourcePreviewConsumeUploadBudget(Budget)");
	const size_t UploadPos = DrainOneBody.find("pGraphics->LoadTextureRawMove");
	ASSERT_NE(BudgetExhaustedPos, std::string::npos);
	ASSERT_NE(UploadPos, std::string::npos);
	ASSERT_LT(BudgetExhaustedPos, UploadPos);
	const std::string BudgetExhaustedBody = DrainOneBody.substr(BudgetExhaustedPos, UploadPos - BudgetExhaustedPos);
	EXPECT_NE(BudgetExhaustedBody.find("m_vUploadQueue.push_front(std::move(Item));"), std::string::npos);
	EXPECT_EQ(BudgetExhaustedBody.find("Item.m_Image.Free()"), std::string::npos);
	EXPECT_EQ(BudgetExhaustedBody.find("Item.m_Finalize(false)"), std::string::npos);
	EXPECT_EQ(BudgetExhaustedBody.find("Cache.MarkPreviewJobDone(Item.m_Key, false)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsSharedUploadSchedulerDoesNotRequeueMovedPreviewImages)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	EXPECT_NE(Source.find("DrainSharedResourcePreviewUploadQueue"), std::string::npos);
	EXPECT_NE(Source.find("while(gs_SettingsAssetsResourcePreviewUploadScheduler.QueueDepth() == 0 && !vReadyQueue.empty()"), std::string::npos);
	EXPECT_NE(Source.find("while(gs_SettingsAssetsResourcePreviewUploadScheduler.QueueDepth() == 0 && !WorkshopState.m_vReadyThumbQueue.empty()"), std::string::npos);
	const size_t LocalEnqueue = Source.find("gs_SettingsAssetsResourcePreviewUploadScheduler.EnqueueUploadToTarget(\n\t\t\t\tPreviewKey");
	ASSERT_NE(LocalEnqueue, std::string::npos);
	const size_t LocalFailure = Source.find("if(!gs_SettingsAssetsResourcePreviewUploadScheduler.DrainOne(PreviewUploadBudget", LocalEnqueue);
	ASSERT_NE(LocalFailure, std::string::npos);
	const size_t LocalEnd = Source.find("char aFinalizeUploadExtra", LocalFailure);
	ASSERT_NE(LocalEnd, std::string::npos);
	const std::string LocalFailureBody = Source.substr(LocalFailure, LocalEnd - LocalFailure);
	EXPECT_EQ(LocalFailureBody.find("vReadyQueue.push_front(Handle)"), std::string::npos);
	EXPECT_EQ(LocalFailureBody.find("ResetCustomItemPreviewState(*pItem)"), std::string::npos);
	EXPECT_NE(LocalFailureBody.find("GPU_UPLOAD_BUDGET"), std::string::npos);

	const size_t WorkshopEnqueue = Source.find("gs_SettingsAssetsResourcePreviewUploadScheduler.EnqueueUploadToTarget(\n\t\t\t\t\tPreviewKey", LocalEnd);
	ASSERT_NE(WorkshopEnqueue, std::string::npos);
	const size_t WorkshopFailure = Source.find("if(!gs_SettingsAssetsResourcePreviewUploadScheduler.DrainOne(WorkshopPreviewUploadBudget", WorkshopEnqueue);
	ASSERT_NE(WorkshopFailure, std::string::npos);
	const size_t WorkshopEnd = Source.find("WorkshopThumbUploadedBytesThisFrame", WorkshopFailure);
	ASSERT_NE(WorkshopEnd, std::string::npos);
	const std::string WorkshopFailureBody = Source.substr(WorkshopFailure, WorkshopEnd - WorkshopFailure);
	EXPECT_EQ(WorkshopFailureBody.find("WorkshopState.m_vReadyThumbQueue.push_front"), std::string::npos);
	EXPECT_EQ(WorkshopFailureBody.find("ResetWorkshopThumbReadyState(*pAsset)"), std::string::npos);
	EXPECT_NE(WorkshopFailureBody.find("GPU_UPLOAD_BUDGET"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsLocalEntityBgStartsPreviewPipelineAndKeepsFallbackVisible)
{
	// Entity Background Image cards are separate from the Entities tab. The artifact
	// pipeline may be pending, but the old map/video fallback must stay visible.
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	const size_t LocalBranch = Body.find("if(!UsesCombinedAssetList(pCurrentCategory))");
	ASSERT_NE(LocalBranch, std::string::npos);
	const size_t LocalBranchEnd = Body.find("if(const SAssetResourceCategory *pCategory = AssetResourceCategoryByTab(s_CurCustomTab); UsesCombinedAssetList(pCategory)", LocalBranch);
	ASSERT_NE(LocalBranchEnd, std::string::npos);
	const std::string LocalBody = Body.substr(LocalBranch, LocalBranchEnd - LocalBranch);

	EXPECT_NE(LocalBody.find("BuildAssetsResourcePreviewKey("), std::string::npos);
	EXPECT_NE(LocalBody.find("SettingsResourcePreviewDrawResult("), std::string::npos);
	EXPECT_NE(LocalBody.find("StartAssetsEntityBgPreviewArtifactJob("), std::string::npos);
	EXPECT_NE(LocalBody.find("RenderAssetsCardPreview(Shell, PreviewState"), std::string::npos);
	EXPECT_NE(LocalBody.find("if(s_CurCustomTab == ASSETS_TAB_ENTITY_BG && !PreviewReady && !AssetsContentWarmupBlocked)"), std::string::npos);
	EXPECT_NE(LocalBody.find("RenderEntityBgFallback(Shell.m_TextureRect)"), std::string::npos);
	EXPECT_NE(LocalBody.find("RenderEntityBgVideoFallback(Shell.m_TextureRect)"), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeListDoesNotExposePartialSkinPreviewUploads)
{
	// The tee list must not render a CSkin while only some sprites have uploaded.
	// Partially uploaded skins looked like broken/default tees in the settings list.
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const CSkin *pSkin = State == CSkins::CSkinContainer::EState::LOADED ? pSkinContainer->Skin().get() : pDefaultSkin;"), std::string::npos);
	EXPECT_EQ(Body.find("const CSkin *pSkin = pSkinContainer->Skin() != nullptr ? pSkinContainer->Skin().get() : pDefaultSkin;"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntitiesPreviewUsesTileArtifactRenderer)
{
	// This is the Entities asset tab, not Entity Background Image. Entities need
	// the tile-layout preview renderer; drawing the source texture as one quad
	// makes the card preview look missing or wrong.
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("bool m_DrawEntityTileArtifact = false;"), std::string::npos);
	EXPECT_NE(Body.find("auto RenderAssetsEntityTilePreviewArtifact = "), std::string::npos);
	EXPECT_NE(Body.find("PreviewState.m_DrawEntityTileArtifact = PreviewState.m_Texture.IsValid();"), std::string::npos);
	EXPECT_NE(Body.find("PreviewState.m_DrawEntityTileArtifact = s_CurCustomTab == ASSETS_TAB_ENTITIES && gs_SettingsAssetsEntityGamePreview && Asset.m_ThumbTexture.IsValid();"), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsEntityTilePreviewArtifact(PreviewFrameRect, PreviewState.m_Texture);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsFrameSchedulerExposesResourceAndTextBudgets)
{
	const std::string Header = ReadRepoFile("src/game/client/components/settings_resource_jobs.h");
	const std::string Source = ReadRepoFile("src/game/client/components/settings_resource_jobs.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");

	EXPECT_NE(Header.find("m_FrameId"), std::string::npos);
	EXPECT_NE(Header.find("m_aOperation"), std::string::npos);
	EXPECT_NE(Header.find("m_aPage"), std::string::npos);
	EXPECT_NE(Header.find("m_aTab"), std::string::npos);
	EXPECT_NE(Header.find("m_Subtab"), std::string::npos);
	EXPECT_NE(Header.find("m_aContext"), std::string::npos);
	EXPECT_NE(Header.find("m_TabSwitchFirstFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_FramePressure"), std::string::npos);
	EXPECT_NE(Header.find("m_ResourceUploadTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_TextContainerTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_GlyphRasterizeTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_GlyphUploadTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_MetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_PreviewArtifactTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_TextureUploadTokens"), std::string::npos);
	EXPECT_NE(Header.find("m_CardDrawTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_ResourceUploadTokens = Output.m_GpuUploadTokens;"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_TextContainerTokens = Output.m_TextPrebuildTokens;"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_GlyphRasterizeTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_GlyphUploadTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_MetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_PreviewArtifactTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_TextureUploadTokens"), std::string::npos);
	EXPECT_NE(Source.find("Output.m_CardDrawTokens"), std::string::npos);
	EXPECT_NE(Menus.find("resource_upload_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("text_container_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("glyph_rasterize_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("glyph_upload_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("paragraph_layout_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("metadata_layout_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("preview_artifact_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("texture_upload_tokens=%d"), std::string::npos);
	EXPECT_NE(Menus.find("card_draw_tokens=%d"), std::string::npos);

	SSettingsAdaptiveBudgetState State;
	SSettingsAdaptiveBudgetInput Input;
	Input.m_FrameMsAverage = 2.0f;
	Input.m_FrameMsP95 = 3.0f;
	Input.m_BackgroundBacklog = 4;
	const SSettingsAdaptiveBudgetOutput Output = SettingsAdaptiveBudgetStep(Input, State);
	EXPECT_EQ(Output.m_ResourceUploadTokens, Output.m_GpuUploadTokens);
	EXPECT_EQ(Output.m_TextContainerTokens, Output.m_TextPrebuildTokens);
	EXPECT_GE(Output.m_GlyphRasterizeTokens, 1);
	EXPECT_GE(Output.m_GlyphUploadTokens, 1);
	EXPECT_GE(Output.m_ParagraphLayoutTokens, 1);
	EXPECT_GE(Output.m_MetadataLayoutTokens, 1);
	EXPECT_GE(Output.m_PreviewArtifactTokens, 1);
	EXPECT_GE(Output.m_TextureUploadTokens, 1);
	EXPECT_GE(Output.m_CardDrawTokens, 1);

	SSettingsAdaptiveBudgetInput PressureInput;
	PressureInput.m_FrameMsAverage = 20.0f;
	PressureInput.m_FrameMsP95 = 20.0f;
	PressureInput.m_TargetFrameMs = 8.333f;
	PressureInput.m_BackgroundBacklog = 4;
	const SSettingsAdaptiveBudgetOutput PressureOutput = SettingsAdaptiveBudgetStep(PressureInput, State);
	EXPECT_EQ(PressureOutput.m_ParagraphLayoutTokens, 0);
	EXPECT_LE(PressureOutput.m_PreviewArtifactTokens, 1);
}

TEST(QmMonitoringHelpers, SettingsSchedulerFeedsTextRuntimeCostIntoAdaptiveBudget)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string PrepareBody = ExtractSourceFunctionBody(Source, "void CMenus::PrepareSettingsAdaptiveBudgetInput(SSettingsAdaptiveBudgetInput &Input)");
	const std::string LogBody = ExtractSourceFunctionBody(Source, "void CMenus::LogSettingsAdaptiveBudget(const char *pSource, const SSettingsAdaptiveBudgetInput &Input, const SSettingsAdaptiveBudgetOutput &Output) const");
	ASSERT_FALSE(PrepareBody.empty());
	ASSERT_FALSE(LogBody.empty());

	EXPECT_NE(Header.find("m_TextContainerCreateMsEwma"), std::string::npos);
	EXPECT_NE(PrepareBody.find("TextRender()->QmTextRuntimeBudgetSnapshot()"), std::string::npos);
	EXPECT_NE(PrepareBody.find("Input.m_TextContainerCreateMsEwma"), std::string::npos);
	EXPECT_NE(LogBody.find("text_create_ewma_ms=%.3f"), std::string::npos);
	EXPECT_NE(LogBody.find("text_scroll_cap=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, UiFrameSchedulerOwnsTextAndResourceBudgets)
{
	const std::string Assets = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Skins = ReadRepoFile("src/game/client/components/skins.cpp");

	EXPECT_NE(Menus.find("BeginSettingsUiFrameScheduler"), std::string::npos);
	EXPECT_NE(Menus.find("CurrentSettingsUiFrameBudget"), std::string::npos);
	EXPECT_NE(Assets.find("CurrentSettingsUiFrameBudget()"), std::string::npos);
	EXPECT_NE(Assets.find("AdaptiveBudget.m_MetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Assets.find("AdaptiveBudget.m_PreviewArtifactTokens"), std::string::npos);
	EXPECT_NE(Assets.find("AdaptiveBudget.m_TextureUploadTokens"), std::string::npos);
	EXPECT_NE(Ingame.find("GameClient()->FrameScheduler()->ComputeBudget(EFrameSchedulerConsumer::IngameServerInfo, TextBudgetInput)"), std::string::npos);
	EXPECT_NE(Ingame.find("m_IngameTextFrameBudget.m_TextContainerTokens"), std::string::npos);
	EXPECT_NE(Ingame.find("m_IngameTextFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(Skins.find("SettingsGpuUploadFrameBudgetForFrame()"), std::string::npos);
	EXPECT_NE(Skins.find("SettingsResourcePreviewConsumeUploadBudget(SkinPreviewUploadBudget"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsAndSkinsUseSharedTextureUploadDrain)
{
	const std::string Assets = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Skins = ReadRepoFile("src/game/client/components/skins.cpp");
	const std::string Preview = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string AssetsBody = ExtractSourceFunctionBody(Assets, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	const std::string SkinProcessBody = ExtractSourceFunctionBody(Skins, "CSkins::ESkinProcessResult CSkins::ProcessSkinContainer(CSkinContainer *pSkinContainer, CSkinLoadingStats &Stats,\n\tint &SkinsProcessedThisFrame, std::chrono::nanoseconds StartTime,\n\tstd::chrono::nanoseconds MaxTime)");
	const std::string SkinDrainBody = ExtractSourceFunctionBody(Skins, "CSkins::ESkinProcessResult CSkins::DrainSettingsSkinPreviewUpload(CSkinContainer *pSkinContainer, CSkinLoadingStats &Stats,\n\tint &SkinsProcessedThisFrame, std::chrono::nanoseconds StartTime,\n\tstd::chrono::nanoseconds MaxTime)");
	ASSERT_FALSE(AssetsBody.empty());
	ASSERT_FALSE(SkinProcessBody.empty());
	ASSERT_FALSE(SkinDrainBody.empty());

	EXPECT_NE(Preview.find("CSettingsResourcePreviewUploadScheduler"), std::string::npos);
	EXPECT_NE(Preview.find("SettingsResourcePreviewImageValidForUpload"), std::string::npos);
	EXPECT_NE(Preview.find("EnqueueUploadToTarget"), std::string::npos);
	EXPECT_NE(Assets.find("gs_SettingsAssetsResourcePreviewUploadScheduler.Drain("), std::string::npos);
	EXPECT_NE(AssetsBody.find("EnqueueUploadToTarget("), std::string::npos);
	EXPECT_EQ(AssetsBody.find("ReplaceCustomItemPreviewTexture("), std::string::npos);
	EXPECT_EQ(AssetsBody.find("ReplaceWorkshopThumbTexture("), std::string::npos);
	EXPECT_NE(Assets.find("ResourcePreviewUploadBudget.m_MaxUploads = ResourcePreviewUploadMergeBudget.m_MaxGpuUploads;"), std::string::npos);
	EXPECT_NE(Assets.find("ResourcePreviewUploadBudget.m_pFrameBudget = SettingsFrameBudget();"), std::string::npos);
	EXPECT_NE(Skins.find("SResourcePreviewUploadBudget SkinPreviewUploadBudget"), std::string::npos);
	EXPECT_NE(SkinDrainBody.find("LoadSkinFinish(pSkinContainer"), std::string::npos);
	EXPECT_NE(SkinDrainBody.find("SettingsResourcePreviewConsumeUploadBudget(SkinPreviewUploadBudget, SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)"), std::string::npos);
	EXPECT_EQ(SkinDrainBody.find("BeginSkinPreviewUpload(pSkinContainer"), std::string::npos);
	EXPECT_EQ(SkinDrainBody.find("UploadNextSkinPreviewSprite(pSkinContainer, SkinPreviewUploadBudget)"), std::string::npos);
	EXPECT_EQ(SkinDrainBody.find("FinishSkinPreviewUpload(pSkinContainer)"), std::string::npos);
	EXPECT_NE(Skins.find("preview_uploads"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameServerInfoUsesStableTextAndSnapshotCache)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfo(CUIRect MainView)");
	const std::string SetGamePageBody = ExtractSourceFunctionBody(Menus, "void CMenus::SetGamePage(int NewPage)");
	const std::string RenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::Render()");
	const std::string ValueBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderIngameServerInfoValueCached(const char *pTextId, unsigned &TextHash, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	const std::string SnapshotDrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainSnapshotTextContainers()");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(SetGamePageBody.empty());
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(ValueBody.empty());
	ASSERT_FALSE(SnapshotDrainBody.empty());

	EXPECT_NE(Header.find("struct SIngameServerInfoTextSnapshot"), std::string::npos);
	EXPECT_NE(Header.find("struct SMenuSnapshotTextKey"), std::string::npos);
	EXPECT_NE(Header.find("m_SnapshotTextCache"), std::string::npos);
	EXPECT_NE(Header.find("m_SnapshotTextPending"), std::string::npos);
	EXPECT_NE(Source.find("RenderIngameServerInfoValueCached("), std::string::npos);
	EXPECT_NE(Source.find("RequestSnapshotTextContainer("), std::string::npos);
	EXPECT_NE(Source.find("DrainSnapshotTextContainers()"), std::string::npos);
	EXPECT_NE(Body.find("DoServerInfoField(\"ingame-server-info-address-label\""), std::string::npos);
	EXPECT_NE(Body.find("RenderIngameServerInfoValueCached(\"ingame-server-info-name-value\""), std::string::npos);
	EXPECT_NE(Body.find("RenderIngameServerInfoValueCached(pValueTextId, ValueHash"), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-server-info-address-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-server-info-ping-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-server-info-version-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-server-info-password-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-game-info-type-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-game-info-map-value\""), std::string::npos);
	EXPECT_NE(Body.find("\"ingame-game-info-players-value\""), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoLabel(&Label, CurrentServerInfo.m_aName"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoLabel(&ValueRect"), std::string::npos);
	EXPECT_NE(Body.find("m_IngameServerInfoTextSnapshot"), std::string::npos);
	EXPECT_NE(SetGamePageBody.find("NewPage == PAGE_SERVER_INFO"), std::string::npos);
	EXPECT_NE(SetGamePageBody.find("StartSettingsPerfFixedWindow(\"ingame_server_info\", \"online\", \"game\", \"server_info\", 30)"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(m_GamePage == PAGE_SETTINGS)\n\t\t\t\tTextVisibleGuard.emplace(this);"), std::string::npos);
	EXPECT_NE(ValueBody.find("RenderSnapshotTextContainer(*pReadyElement"), std::string::npos);
	EXPECT_EQ(ValueBody.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_INGAME, *pReadyElement"), std::string::npos);
	EXPECT_NE(SnapshotDrainBody.find("m_IngameTextFrameBudget.m_TextContainerTokens"), std::string::npos);
	EXPECT_EQ(SnapshotDrainBody.find("m_CurrentSettingsUiFrameBudget.m_TextContainerTokens"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameEscOpenHasConcreteSectionTelemetry)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string RenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::Render()");
	const std::string GameBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderGame(CUIRect MainView)");
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(GameBody.empty());

	EXPECT_EQ(Menus.find("ingame_esc_button_column"), std::string::npos);
	EXPECT_NE(GameBody.find("ingame_esc_button_column"), std::string::npos);
	EXPECT_NE(GameBody.find("MainView.HSplitTop(45.0f + (HasSecondaryButtonBar ? 35.0f : 0.0f), &ButtonBars, &MainView);"), std::string::npos);
	EXPECT_LT(GameBody.find("MainView.HSplitTop(45.0f + (HasSecondaryButtonBar ? 35.0f : 0.0f), &ButtonBars, &MainView);"), GameBody.find("ingame_esc_button_column"));
	const std::string ButtonColumnLog = "LogIngamePerfStage(Client(), \"ingame_esc_button_column\", ButtonColumnTimer.ElapsedMs(), false, aButtonColumnPerfExtra);";
	const size_t ButtonColumnTimerPos = GameBody.find("CPerfTimer ButtonColumnTimer;");
	const size_t ButtonColumnLogPos = GameBody.find(ButtonColumnLog);
	const size_t LastButtonControlPos = GameBody.find("Console()->ExecuteLine(\"toggle_local_console\", IConsole::CLIENT_ID_UNSPECIFIED);");
	const size_t TouchEditingBranchPos = GameBody.find("if(GameClient()->m_TouchControls.IsEditingActive())", LastButtonControlPos);
	const size_t NormalButtonColumnFlushPos = GameBody.find("\n\tLogButtonColumnPerf();", LastButtonControlPos);
	ASSERT_NE(ButtonColumnTimerPos, std::string::npos);
	ASSERT_NE(ButtonColumnLogPos, std::string::npos);
	ASSERT_NE(LastButtonControlPos, std::string::npos);
	ASSERT_NE(TouchEditingBranchPos, std::string::npos);
	ASSERT_NE(NormalButtonColumnFlushPos, std::string::npos);
	EXPECT_EQ(CountSubstring(GameBody, ButtonColumnLog), 1u);
	EXPECT_LT(ButtonColumnTimerPos, ButtonColumnLogPos);
	EXPECT_LT(LastButtonControlPos, NormalButtonColumnFlushPos);
	EXPECT_LT(NormalButtonColumnFlushPos, TouchEditingBranchPos);
	EXPECT_LT(NormalButtonColumnFlushPos, GameBody.find("RenderTouchControlsEditor"));
	EXPECT_LT(NormalButtonColumnFlushPos, GameBody.find("RenderTouchButtonEditor"));
	EXPECT_LT(NormalButtonColumnFlushPos, GameBody.find("RenderConfigSettings"));
	EXPECT_LT(NormalButtonColumnFlushPos, GameBody.find("RenderPreviewSettings"));

	const std::string ButtonColumnRegion = GameBody.substr(ButtonColumnTimerPos, TouchEditingBranchPos - ButtonColumnTimerPos);
	std::istringstream RegionStream(ButtonColumnRegion);
	std::string Line;
	std::string PreviousNonEmptyLine;
	while(std::getline(RegionStream, Line))
	{
		const std::string TrimmedLine = Trim(Line);
		if(TrimmedLine.empty())
			continue;
		if(TrimmedLine.find("return;") != std::string::npos)
		{
			if(PreviousNonEmptyLine != "if(ButtonColumnPerfLogged)")
				EXPECT_EQ(PreviousNonEmptyLine, "LogButtonColumnPerf();");
		}
		PreviousNonEmptyLine = TrimmedLine;
	}

	EXPECT_NE(RenderBody.find("ingame_esc_menu_shell"), std::string::npos);
	EXPECT_NE(RenderBody.find("ingame_esc_tab_content"), std::string::npos);
	EXPECT_NE(RenderBody.find("ingame_server_info_layout"), std::string::npos);
	EXPECT_NE(RenderBody.find("const char *pOperationName = SettingsPerfActiveOperation();"), std::string::npos);
	EXPECT_NE(RenderBody.find("operation=%s context=online page=%s tab=none frame=%"), std::string::npos);
	EXPECT_NE(RenderBody.find("context=online"), std::string::npos);
	EXPECT_NE(RenderBody.find("tab=none"), std::string::npos);
	EXPECT_NE(RenderBody.find("frame=%\" PRIu64"), std::string::npos);
	EXPECT_NE(RenderBody.find("LogPerfStage(Client(), \"ingame_esc_menu_shell\", ShellTimer.ElapsedMs(), false, aEscPerfExtra);"), std::string::npos);
	EXPECT_NE(RenderBody.find("LogPerfStage(Client(), \"ingame_esc_tab_content\", StageTimer.ElapsedMs(), TransitionActive, aEscPerfExtra);"), std::string::npos);
	EXPECT_NE(RenderBody.find("LogPerfStage(Client(), \"ingame_server_info_layout\", StageDurationMs, TransitionActive, aEscPerfExtra);"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerInfoTextPreparedOnlyWhenOpeningServerInfoPage)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	const std::string SetGamePageBody = ExtractSourceFunctionBody(Menus, "void CMenus::SetGamePage(int NewPage)");
	const std::string RenderBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderServerInfo(CUIRect MainView)");
	ASSERT_FALSE(OnRenderBody.empty());
	ASSERT_FALSE(SetGamePageBody.empty());
	ASSERT_FALSE(RenderBody.empty());

	// Esc-opening PAGE_GAME and switching to PAGE_SERVER_INFO must not do the
	// dynamic server-info snapshot/MOTD prepare synchronously in the input path.
	// The prepare is allowed only from the OnRender frame-end background path.
	EXPECT_NE(Header.find("void PrepareIngameServerInfoTextRuntime("), std::string::npos);
	EXPECT_NE(Ingame.find("void CMenus::PrepareIngameServerInfoTextRuntime("), std::string::npos);
	EXPECT_NE(OnRenderBody.find("PrepareIngameServerInfoTextRuntime("), std::string::npos);
	EXPECT_EQ(SetGamePageBody.find("PrepareIngameServerInfoTextRuntime("), std::string::npos);
	EXPECT_NE(SetGamePageBody.find("NewPage == PAGE_SERVER_INFO"), std::string::npos);
	EXPECT_NE(SetGamePageBody.find("m_IngameServerInfoBackgroundPrepareRequested = false;"), std::string::npos);
	EXPECT_EQ(RenderBody.find("PrepareIngameServerInfoTextRuntime("), std::string::npos);
	EXPECT_NE(Ingame.find("event=server_info_text_prepare"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameServerInfoBackgroundPrepareDoesNotDrainMotdParagraph)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	const std::string PrepareBody = ExtractSourceFunctionBody(Ingame, "void CMenus::PrepareIngameServerInfoTextRuntime(const CUIRect *pMainView)");
	ASSERT_FALSE(OnRenderBody.empty());
	ASSERT_FALSE(PrepareBody.empty());

	// Esc-open background prewarm may prepare the small server-info value
	// snapshots and enqueue the MOTD paragraph, but it must not synchronously
	// drain the paragraph. Visible server-info frames must not run paragraph
	// drain either; that work is only allowed through the non-visible background
	// scheduler path.
	EXPECT_NE(OnRenderBody.find("PrepareIngameServerInfoTextRuntime();"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("DrainIngameUiSnapshotTextRuntime();"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("m_IngameServerInfoBackgroundPrepareRequested = !m_SnapshotTextPending.empty() || m_IngameMotdParagraphCache.m_Pending;"), std::string::npos);
	EXPECT_NE(PrepareBody.find("RequestIngameMotdParagraphCache("), std::string::npos);
	EXPECT_EQ(PrepareBody.find("DrainIngameUiTextRuntime("), std::string::npos);
	EXPECT_NE(PrepareBody.find("RequestSnapshotTextContainer("), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerInfoDoesNotShowVisiblePlaceholderOnCacheMiss)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string ValueBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderIngameServerInfoValueCached(const char *pTextId, unsigned &TextHash, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	const std::string SnapshotBody = ExtractSourceFunctionBody(Source, "bool CMenus::RequestSnapshotTextContainer(const char *pScope, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, CUIElement **ppReadyElement)");
	ASSERT_FALSE(ValueBody.empty());
	ASSERT_FALSE(SnapshotBody.empty());

	// Dynamic values may be budgeted, but the visible server-info card should
	// reuse the previous ready snapshot instead of showing a user-visible
	// placeholder/loading state on a cache miss.
	EXPECT_NE(Header.find("CUIElement *m_pLastReadyElement"), std::string::npos);
	EXPECT_NE(SnapshotBody.find("m_pLastReadyElement"), std::string::npos);
	EXPECT_NE(ValueBody.find("RenderSnapshotTextContainer(*pReadyElement"), std::string::npos);
	EXPECT_NE(ValueBody.find("RenderSnapshotTextContainer(*pLastReadyElement"), std::string::npos);
	EXPECT_EQ(ValueBody.find("Loading"), std::string::npos);
	// Dynamic values are content, not fixed UI chrome. A miss may enqueue a
	// snapshot text request, but must not synchronously draw through Ui()->DoLabel
	// because that recreates text containers in the visible server-info frame.
	EXPECT_EQ(ValueBody.find("Ui()->DoLabel"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdParagraphMissDoesNotRecreateInRenderPath)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());
	const std::string RequestBody = ExtractSourceFunctionBody(Source, "bool CMenus::RequestIngameMotdParagraphCache(CUIRect Motd, float FontSize)");
	ASSERT_FALSE(RequestBody.empty());

	EXPECT_NE(Header.find("struct SIngameMotdParagraphCache"), std::string::npos);
	EXPECT_NE(Header.find("m_IngameMotdParagraphCache"), std::string::npos);
	EXPECT_NE(Source.find("RequestIngameMotdParagraphCache("), std::string::npos);
	EXPECT_NE(Body.find("RequestIngameMotdParagraphCache(Motd"), std::string::npos);
	EXPECT_EQ(Source.find("EnsureIngameMotdParagraphCache("), std::string::npos);
	EXPECT_NE(RequestBody.find("if(!m_IngameMotdParagraphCache.m_Pending ||"), std::string::npos);
	EXPECT_EQ(RequestBody.find("SettingsAdaptiveBudgetStep("), std::string::npos);
	EXPECT_EQ(RequestBody.find("TextRender()->RecreateTextContainer("), std::string::npos);
	EXPECT_EQ(Body.find("TextRender()->RecreateTextContainer("), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdParagraphHydratesOnlyThroughBudgetDrain)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame)");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(Header.find("m_Pending"), std::string::npos);
	EXPECT_NE(Header.find("m_PendingFrame"), std::string::npos);
	EXPECT_NE(Source.find("GameClient()->FrameScheduler()->ComputeBudget(EFrameSchedulerConsumer::IngameServerInfo, TextBudgetInput)"), std::string::npos);
	EXPECT_EQ(Source.find("BeginSettingsUiFrameScheduler(\"ingame_server_info_snapshot_text\""), std::string::npos);
	EXPECT_EQ(DrainBody.find("BeginSettingsUiFrameScheduler("), std::string::npos);
	EXPECT_NE(DrainBody.find("ParagraphLayoutTokens <= 0"), std::string::npos);
	EXPECT_NE(DrainBody.find("--m_IngameTextFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(Source.find("paragraph_budget_blocked"), std::string::npos);
	EXPECT_NE(Source.find("paragraph_cache_hit"), std::string::npos);
	EXPECT_NE(Source.find("paragraph_layout_ms"), std::string::npos);
	EXPECT_NE(DrainBody.find("TextRender()->CreateOrAppendTextContainer(m_IngameMotdParagraphCache.m_BuildTextContainerIndex"), std::string::npos);
	EXPECT_EQ(DrainBody.find("TextRender()->RecreateTextContainer(m_MotdTextContainerIndex"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_IngameMotdParagraphCache.m_PendingFrame"), std::string::npos);
	EXPECT_NE(Body.find("RequestIngameMotdParagraphCache(Motd"), std::string::npos);
	EXPECT_EQ(Body.find("DrainIngameMotdParagraphCache(Motd"), std::string::npos);
	EXPECT_NE(Source.find("void CMenus::DrainIngameUiTextRuntime(bool AllowCurrentFrame)"), std::string::npos);
	EXPECT_NE(Header.find("m_PendingRect"), std::string::npos);
	EXPECT_NE(Header.find("m_BuildByteOffset"), std::string::npos);
	EXPECT_NE(Header.find("m_BuildTextContainerIndex"), std::string::npos);
	EXPECT_EQ(Header.find("m_IngameTextAdaptiveBudgetState"), std::string::npos);
	EXPECT_NE(Header.find("SSettingsAdaptiveBudgetOutput m_IngameTextFrameBudget"), std::string::npos);
	EXPECT_NE(Source.find("DrainIngameMotdParagraphCache(m_IngameMotdParagraphCache.m_PendingRect, m_IngameMotdParagraphCache.m_PendingFontSize, AllowCurrentFrame);"), std::string::npos);
	EXPECT_EQ(Source.find("m_IngameTextAdaptiveBudgetState"), std::string::npos);
	EXPECT_EQ(Source.find("m_IngameTextFrameBudget.m_ParagraphLayoutTokens = maximum(1, m_IngameTextFrameBudget.m_ParagraphLayoutTokens)"), std::string::npos);
	EXPECT_EQ(Source.find("m_CurrentSettingsUiFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_IngameTextFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_EQ(Body.find("TextRender()->RecreateTextContainer(m_MotdTextContainerIndex"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdParagraphDrainBuildsInChunks)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame)");
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(Header.find("static constexpr int INGAME_MOTD_PARAGRAPH_CHUNK_BYTES"), std::string::npos);
	EXPECT_NE(Header.find("INGAME_MOTD_PARAGRAPH_CHUNK_BYTES = 24"), std::string::npos);
	EXPECT_NE(Header.find("CTextCursor m_BuildCursor"), std::string::npos);
	EXPECT_NE(Header.find("int m_BuildByteOffset"), std::string::npos);
	EXPECT_NE(DrainBody.find("INGAME_MOTD_PARAGRAPH_CHUNK_BYTES"), std::string::npos);
	EXPECT_NE(DrainBody.find("str_utf8_isstart"), std::string::npos);
	EXPECT_NE(DrainBody.find("TextRender()->CreateOrAppendTextContainer(m_IngameMotdParagraphCache.m_BuildTextContainerIndex"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_IngameMotdParagraphCache.m_BuildByteOffset += ChunkLength"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_MotdTextContainerIndex = m_IngameMotdParagraphCache.m_BuildTextContainerIndex"), std::string::npos);
	EXPECT_EQ(DrainBody.find("TextRender()->RecreateTextContainer(m_IngameMotdParagraphCache.m_PreviousTextContainerIndex"), std::string::npos);
	EXPECT_EQ(DrainBody.find("TextRender()->RecreateTextContainer(m_MotdTextContainerIndex"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdUsesReadyOrPreviousParagraphOnly)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());

	// MOTD can be prepared asynchronously, but visible rendering must not show a
	// loading placeholder or blank range. It should render the current ready
	// paragraph, or a previous same-size paragraph while the new one hydrates.
	EXPECT_NE(Header.find("m_PreviousTextHash"), std::string::npos);
	EXPECT_NE(Header.find("m_PreviousTextContainerIndex"), std::string::npos);
	EXPECT_NE(Source.find("RenderIngameMotdPreviousParagraphCache("), std::string::npos);
	EXPECT_NE(Body.find("const bool RenderedMotdParagraph ="), std::string::npos);
	EXPECT_NE(Body.find("RenderIngameMotdPreviousParagraphCache("), std::string::npos);
	EXPECT_NE(Body.find("RenderIngameMotdFallbackText("), std::string::npos);
	EXPECT_NE(Body.find("server_info_not_ready=1"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Loading"), std::string::npos);
}

TEST(QmMonitoringHelpers, ValueSelectorDisplayUsesSingleLineShrink)
{
	const std::string Header = ReadRepoFile("src/game/client/ui.h");
	const std::string Source = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "SEditResult<int64_t> CUi::DoValueSelectorWithState(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props)");
	const std::string FlagsBody = ExtractSourceFunctionBody(Source, "static int GetFlagsForLabelProperties(const SLabelProperties &LabelProps, const CTextCursor *pReadCursor)");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(FlagsBody.empty());

	EXPECT_NE(Header.find("struct SLabelProperties"), std::string::npos);
	EXPECT_NE(Header.find("bool m_DisallowNewline"), std::string::npos);
	EXPECT_NE(FlagsBody.find("LabelProps.m_DisallowNewline ? TEXTFLAG_DISALLOW_NEWLINE : 0"), std::string::npos);
	EXPECT_NE(Body.find("SLabelProperties ValueLabelProps"), std::string::npos);
	EXPECT_NE(Body.find("ValueLabelProps.m_MaxWidth = pRect->w"), std::string::npos);
	EXPECT_NE(Body.find("ValueLabelProps.m_DisallowNewline = true"), std::string::npos);
	EXPECT_NE(Body.find("ValueLabelProps.m_StopAtEnd = true"), std::string::npos);
	EXPECT_NE(Body.find("ValueLabelProps.m_MinimumFontSize"), std::string::npos);
	EXPECT_NE(Body.find("const char *pDisplayText = m_ActiveValueSelectorState.m_pLastTextId == pId ? m_ActiveValueSelectorState.m_NumberInput.GetDisplayedString() : aBuf;"), std::string::npos);
	EXPECT_NE(Body.find("DoLabel(pRect, pDisplayText, 10.0f, TEXTALIGN_MC, ValueLabelProps);"), std::string::npos);
	EXPECT_NE(Body.find("auto RenderValueSelectorDisplay = [&]()"), std::string::npos);
	EXPECT_NE(Body.find("RenderValueSelectorDisplay();"), std::string::npos);
	EXPECT_LT(Body.find("RenderValueSelectorDisplay();"), Body.find("if(Inside && !MouseButton(0) && !MouseButton(1))"));
	EXPECT_EQ(Body.find("DoEditBox(&m_ActiveValueSelectorState.m_NumberInput"), std::string::npos);
	EXPECT_EQ(Body.find("DoLabel(pRect, aBuf, 10.0f, TEXTALIGN_MC);\n"), std::string::npos);
}

TEST(QmMonitoringHelpers, ValueSelectorCanFormatAndParseSpecialTextValues)
{
	const std::string Header = ReadRepoFile("src/game/client/ui.h");
	const std::string Source = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "SEditResult<int64_t> CUi::DoValueSelectorWithState(const void *pId, const CUIRect *pRect, const char *pLabel, int64_t Current, int64_t Min, int64_t Max, const SValueSelectorProperties &Props)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Header.find("FValueSelectorFormatCallback"), std::string::npos);
	EXPECT_NE(Header.find("FValueSelectorParseCallback"), std::string::npos);
	EXPECT_NE(Header.find("m_pfnFormatValue"), std::string::npos);
	EXPECT_NE(Header.find("m_pfnParseValue"), std::string::npos);
	EXPECT_NE(Body.find("Props.m_pfnFormatValue"), std::string::npos);
	EXPECT_NE(Body.find("Props.m_pfnParseValue"), std::string::npos);
	EXPECT_NE(Body.find("m_ActiveValueSelectorState.m_NumberInput.Set(aEditBuf);"), std::string::npos);
	EXPECT_NE(Body.find("if(Props.m_pfnParseValue != nullptr)\n\t\t\t{"), std::string::npos);
	EXPECT_NE(Body.find("if(Props.m_pfnParseValue(m_ActiveValueSelectorState.m_NumberInput.GetString(), ParsedValue, Base))"), std::string::npos);
	EXPECT_NE(Body.find("else\n\t\t\t\tCurrent = std::clamp(m_ActiveValueSelectorState.m_NumberInput.GetInteger64(Base), Min, Max);"), std::string::npos);
}

TEST(QmMonitoringHelpers, GraphicsRefreshRateInputAcceptsInfinitySymbol)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	const std::string UiFormsSource = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string NumericFieldBody = ExtractSourceFunctionBody(UiFormsSource, "bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options)");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(NumericFieldBody.empty());

	EXPECT_NE(Body.find("int InputMin = -1, int InputMax = -1"), std::string::npos);
	EXPECT_NE(Body.find("Options.m_InputMin = InputMin;"), std::string::npos);
	EXPECT_NE(Body.find("Options.m_InputMax = InputMax;"), std::string::npos);
	EXPECT_NE(Body.find("CUi::SCROLLBAR_OPTION_INFINITE | CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, 0, 10000"), std::string::npos);
	EXPECT_NE(NumericFieldBody.find("NumericFieldTextIsInfinite(pInput->GetString())"), std::string::npos);
	EXPECT_NE(NumericFieldBody.find("if(!ParsedInfinite && (Options.m_InputMin >= 0 || Options.m_InputMax >= 0))"), std::string::npos);
	EXPECT_NE(NumericFieldBody.find("Parsed = NumericFieldTextInputStoredValue("), std::string::npos);
	EXPECT_EQ(Body.find("FormatInfiniteValueSelector"), std::string::npos);
	EXPECT_EQ(Body.find("ParseInfiniteValueSelector"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientSliderValueInputReservesReadableValueWidth)
{
	const std::string Source = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const float ValueWidth = std::clamp((MultiLine ? ValueRect.w : Controls.w) * 0.18f, 42.0f, 80.0f);"), std::string::npos);
	EXPECT_NE(Body.find("const bool HasSlider = MultiLine || Controls.w > ValueWidth + SuffixWidth + 42.0f;"), std::string::npos);
	EXPECT_NE(Body.find("InputField.VMargin(std::min(5.0f, ValueWidth * 0.1f), &InputField);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SkinTransitionDurationLabelUsesSingleLineShrink)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmVisualSkinTransitionContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("SLabelProperties DurationLabelProps"), std::string::npos);
	EXPECT_NE(Body.find("DurationLabelProps.m_DisallowNewline = true"), std::string::npos);
	EXPECT_NE(Body.find("DurationLabelProps.m_StopAtEnd = true"), std::string::npos);
	EXPECT_NE(Body.find("DurationLabelProps.m_MinimumFontSize = 6.0f"), std::string::npos);
	EXPECT_NE(Body.find("RenderQmVisualLabel(\"qmclient-skin-transition-duration\", &LabelColumn, Localize(\"Skin transition duration\"), BodySize, TEXTALIGN_ML, DurationLabelProps);"), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeSkinQueueOmitsCapacityAndUsesSharedIntervalNumericField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Body.find("const bool CompactQueueCapacityControls"), std::string::npos);
	EXPECT_EQ(Body.find("CUIRect QueueEnabledRect;"), std::string::npos);
	EXPECT_EQ(Body.find("QueueDirty ? \"● \""), std::string::npos);
	EXPECT_NE(Body.find("str_format(aQueueLabel, sizeof(aQueueLabel), \"%s (%d)\", Localize(\"Skin queue\"), (int)SkinQueue.size());"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsButton_CheckBox(SETTINGS_TEE, -1, -1, &QueueEnabled, QueueDummy ? \"tee-dummy-skin-queue-enabled\" : \"tee-player-skin-queue-enabled\", aQueueLabel, QueueEnabled, &QueueHeader"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->DoLabel(&QueueListHeaderLabel, aCurrentQueueLabel"), std::string::npos);
	EXPECT_EQ(Body.find("DoSettingsMenuLabel(SETTINGS_TEE, -1, -1, \"tee_queue_list_label\", &QueueListHeaderLabel, Localize(\"Skin queue\")"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Enable skin queue\"), QueueEnabled"), std::string::npos);
	EXPECT_NE(Body.find("CUIRect IntervalRow, IntervalLabel, IntervalControls;"), std::string::npos);
	EXPECT_NE(Body.find("QueueSection.HSplitTop(20.0f, &IntervalRow, &QueueSection);"), std::string::npos);
	EXPECT_NE(Body.find("IntervalRow.VSplitLeft(QueueControlLabelWidth, &IntervalLabel, &IntervalControls);"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Enabled\")"), std::string::npos);
	EXPECT_EQ(Body.find("tee-skin-queue-enabled-heading"), std::string::npos);
	EXPECT_NE(Body.find("IntervalControls.VSplitRight(QueueValueInputWidth + QueueValueUnitWidth, nullptr, &IntervalInputGroup);"), std::string::npos);
	EXPECT_NE(Body.find("IntervalInputGroup.VSplitRight(QueueValueUnitWidth, &IntervalInput, &IntervalUnit);"), std::string::npos);
	EXPECT_NE(Body.find("const float QueueValueInputWidth = 58.0f"), std::string::npos);
	EXPECT_NE(Body.find("const float QueueValueUnitWidth = 18.0f"), std::string::npos);
	EXPECT_NE(Body.find("SLabelProperties QueueControlLabelProps;"), std::string::npos);
	EXPECT_NE(Body.find("QueueControlLabelProps.m_DisallowNewline = true"), std::string::npos);
	EXPECT_NE(Body.find("QueueControlLabelProps.m_MinimumFontSize = 6.0f"), std::string::npos);
	EXPECT_NE(Body.find("static ui_widget::SNumericFieldState s_aQueueIntervalStates[NUM_DUMMIES];"), std::string::npos);
	const size_t InputCtxPos = Body.find("IUiContext TeeSkinQueueIntervalCtx;");
	const size_t InputCtxUiPos = Body.find("TeeSkinQueueIntervalCtx.m_pUi = Ui();", InputCtxPos);
	const size_t InputCtxAnimPos = Body.find("TeeSkinQueueIntervalCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", InputCtxUiPos);
	const size_t InputCtxTreePos = Body.find("TeeSkinQueueIntervalCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", InputCtxAnimPos);
	const size_t InputCtxScopePos = Body.find("TeeSkinQueueIntervalCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee_skin_queue_interval_text_input\");", InputCtxTreePos);
	const size_t InputCtxFrameDtPos = Body.find("TeeSkinQueueIntervalCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", InputCtxScopePos);
	const size_t NumericFieldPos = Body.find("ui_widget::NumericField(TeeSkinQueueIntervalCtx, &s_aQueueIntervalStates[QueueDummy], &QueueInterval, &QueueInterval, 1, 120000, IntervalInput, QueueIntervalOptions);", InputCtxFrameDtPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(InputCtxPos, std::string::npos);
	EXPECT_NE(InputCtxUiPos, std::string::npos);
	EXPECT_NE(InputCtxAnimPos, std::string::npos);
	EXPECT_NE(InputCtxTreePos, std::string::npos);
	EXPECT_NE(InputCtxScopePos, std::string::npos);
	EXPECT_NE(InputCtxFrameDtPos, std::string::npos);
	EXPECT_NE(NumericFieldPos, std::string::npos);
	EXPECT_LT(InputCtxPos, InputCtxUiPos);
	EXPECT_LT(InputCtxUiPos, InputCtxAnimPos);
	EXPECT_LT(InputCtxAnimPos, InputCtxTreePos);
	EXPECT_LT(InputCtxTreePos, InputCtxScopePos);
	EXPECT_LT(InputCtxScopePos, InputCtxFrameDtPos);
	EXPECT_LT(InputCtxFrameDtPos, NumericFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&QueueIntervalInput, &IntervalInput, 10.0f, IGraphics::CORNER_ALL, {}, TEXTALIGN_MC)"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->DoLabel(&IntervalUnit, \"ms\""), std::string::npos);
	EXPECT_EQ(Body.find("DoValueSelectorWithState(&s_aQueueIntervalInputIds[QueueDummy]"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoScrollbarH(&QueueInterval"), std::string::npos);
	EXPECT_EQ(Body.find("IntervalScrollbar"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Queue capacity\")"), std::string::npos);
	EXPECT_EQ(Body.find("static char s_aQueueLengthInputIds[NUM_DUMMIES];"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Length\")"), std::string::npos);
	EXPECT_EQ(Body.find("Localize(\"Queue limit\")"), std::string::npos);
	EXPECT_EQ(Body.find("DoSettingsScrollbarOption(SETTINGS_TEE, -1, -1, \"tee-skin-queue-length\""), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeSkinQueueDragShowsDropLineAndGhostRow)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("static vec2 s_QueueDragGrabOffset = vec2(0.0f, 0.0f);"), std::string::npos);
	EXPECT_NE(Body.find("static CUIRect s_QueueDraggedRect;"), std::string::npos);
	EXPECT_NE(Body.find("CUIRect QueueDropLine;"), std::string::npos);
	EXPECT_NE(Body.find("HasQueueDropLine = true;"), std::string::npos);
	EXPECT_NE(Body.find("QueueDropLine.Draw(ColorRGBA(0.45f, 0.7f, 1.0f, 0.9f), IGraphics::CORNER_ALL, 1.0f);"), std::string::npos);
	EXPECT_NE(Body.find("CUIRect QueueDragGhost = s_QueueDraggedRect;"), std::string::npos);
	EXPECT_NE(Body.find("QueueDragGhost.x = Ui()->MouseX() - s_QueueDragGrabOffset.x;"), std::string::npos);
	EXPECT_NE(Body.find("QueueDragGhost.Draw(ColorRGBA(0.18f, 0.2f, 0.24f, 0.92f), IGraphics::CORNER_ALL, 4.0f);"), std::string::npos);
	EXPECT_NE(Body.find("QueueDragGhostLabel"), std::string::npos);
	EXPECT_NE(Body.find("s_QueueDragGrabOffset = Ui()->MousePos() - vec2(Item.m_Rect.x, Item.m_Rect.y);"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameTextRuntimeSkipsIdleLogLines)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame)");
	ASSERT_FALSE(Body.empty());

	// The perf log can run for millions of frames. Avoid 0-cost text runtime rows
	// that only inflate logs and reports; keep rows when miss/block/layout happens.
	EXPECT_NE(Body.find("if(CacheHit || CacheMiss || BudgetBlocked || ParagraphLayoutMs >= QmPerfThresholdMs())"), std::string::npos);
	EXPECT_NE(Body.find("QmPerfLogPayload(\"perf/text\", aPayload, Client(), \"game\");"), std::string::npos);
}

TEST(QmMonitoringHelpers, RenderPathsDoNotCreateTextContainersSynchronously)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Assets = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string LabelBody = ExtractSourceFunctionBody(Menus, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");
	const std::string DrainBody = ExtractSourceFunctionBody(Menus, "void CMenus::DrainMenuTextContainerBuildRequests()");
	const std::string MotdBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	const std::string AssetsBody = ExtractSourceFunctionBody(Assets, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(LabelBody.empty());
	ASSERT_FALSE(DrainBody.empty());
	ASSERT_FALSE(MotdBody.empty());
	ASSERT_FALSE(AssetsBody.empty());

	const size_t QueueBranch = LabelBody.find("if(NeedsBuild && m_pSettingsTextPrebuildBudget == nullptr)");
	const size_t PrebuildBranch = LabelBody.find("if(m_pSettingsTextPrebuildBudget != nullptr)", QueueBranch);
	ASSERT_NE(QueueBranch, std::string::npos);
	ASSERT_NE(PrebuildBranch, std::string::npos);
	const std::string RenderRequestBranch = LabelBody.substr(QueueBranch, PrebuildBranch - QueueBranch);

	EXPECT_NE(LabelBody.find("QueueMenuTextContainerBuild"), std::string::npos);
	EXPECT_EQ(RenderRequestBranch.find("DrainMenuTextContainerBuild("), std::string::npos);
	EXPECT_NE(Menus.find("if(m_pSettingsTextPrebuildBudget != nullptr)"), std::string::npos);
	EXPECT_NE(Menus.find("DrainMenuTextContainerBuild(Element, pRect, pText, Size, Align, LabelProps, StrLen, pReadCursor, Render, &TextContainerRecreated);"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_CurrentSettingsUiFrameBudget.m_TextContainerTokens > 0"), std::string::npos);
	EXPECT_NE(DrainBody.find("--m_CurrentSettingsUiFrameBudget.m_TextContainerTokens"), std::string::npos);
	EXPECT_NE(DrainBody.find("DrainMenuTextContainerBuild("), std::string::npos);
	EXPECT_EQ(LabelBody.find("Ui()->DoLabelStreamed(*Element.Rect(0)"), std::string::npos);
	EXPECT_EQ(MotdBody.find("TextRender()->RecreateTextContainer("), std::string::npos);
	EXPECT_EQ(AssetsBody.find("TextRender()->RecreateTextContainer("), std::string::npos);
	EXPECT_NE(AssetsBody.find("RequestAssetsCardMetadataHydration"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameServerInfoCardTitlesHaveImmediateFallback)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfo(CUIRect MainView)");
	const std::string MotdBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(MotdBody.empty());

	// Screenshot regression: budgeted ingame stable labels can be queued on cache
	// miss, but server-info card headers must still be visible in the current
	// frame instead of disappearing until the text container drain catches up.
	EXPECT_NE(Header.find("void DoIngameMenuTitleLabel("), std::string::npos);
	EXPECT_NE(Menus.find("void CMenus::DoIngameMenuTitleLabel("), std::string::npos);
	EXPECT_NE(Body.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-server-info-title\""), std::string::npos);
	EXPECT_NE(Body.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-game-info-title\""), std::string::npos);
	EXPECT_NE(MotdBody.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-server-info-motd-title\""), std::string::npos);
	EXPECT_EQ(Body.find("DoIngameMenuTitleLabel(PAGE_SERVER_INFO, \"ingame-server-info-address-label\""), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameMenuTabsHaveImmediateTextFallback)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "int CMenus::DoMenuTabV2(CButtonContainer *pButtonContainer, const char *pText, bool Active, const CUIRect *pRect, int Corners, const ColorRGBA *pCustomDefault, const ColorRGBA *pCustomActive, const ColorRGBA *pCustomHover, const CCommunityIcon *pCommunityIcon, CUIElement *pTextUiElement)");
	ASSERT_FALSE(Body.empty());

	// Screenshot regression: the Ghost / Call vote ingame tabs are critical
	// navigation labels. A budgeted streamed-text cache miss may enqueue the
	// real container, but the current frame must still draw readable text.
	EXPECT_NE(Source.find("\"ingame-tab-ghost\""), std::string::npos);
	EXPECT_NE(Source.find("\"ingame-tab-call-vote\""), std::string::npos);
	EXPECT_NE(Body.find("CUIElement::SUIElementRect *pElementRect = pTextUiElement->Rect(0);"), std::string::npos);
	EXPECT_NE(Body.find("const bool HadReadyContainer = pElementRect->m_UITextContainer.Valid();"), std::string::npos);
	EXPECT_NE(Body.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_INGAME, *pTextUiElement, &Label, pText, LabelFontSize, TEXTALIGN_MC);"), std::string::npos);
	EXPECT_NE(Body.find("if(pTextUiElement != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid())"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->DoLabel(&Label, pText, LabelFontSize, TEXTALIGN_MC);"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameStableLabelsHaveImmediateTextFallback)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::DoIngameMenuLabel(int Page, const char *pTextId, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	ASSERT_FALSE(Body.empty());

	// Server-info fixed labels such as Address/Ping/Version are visible UI
	// chrome, not optional content. They still use the ingame text pool, but a
	// current-frame cache miss must not make the label disappear.
	EXPECT_NE(Ingame.find("DoIngameMenuLabel(PAGE_SERVER_INFO, pLabelTextId"), std::string::npos);
	EXPECT_NE(Body.find("CUIElement::SUIElementRect *pElementRect = Element.Rect(0);"), std::string::npos);
	EXPECT_NE(Body.find("const bool HadReadyContainer = pElementRect->m_UITextContainer.Valid();"), std::string::npos);
	EXPECT_NE(Body.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_INGAME, Element, pRect, pText, Size, Align, LabelProps);"), std::string::npos);
	EXPECT_NE(Body.find("if(&Element != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid() && pRect != nullptr)"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->DoLabel(pRect, pText, Size, Align, LabelProps);"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameServerInfoButtonsUseBudgetedTextPipeline)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderServerInfo(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Server-info chrome includes action buttons as well as labels. Fixed
	// buttons must use the ingame text pool so opening a text-heavy server-info
	// page does not create button text containers synchronously.
	EXPECT_NE(Header.find("DoIngameMenuButton("), std::string::npos);
	EXPECT_NE(Source.find("int CMenus::DoIngameMenuButton("), std::string::npos);
	EXPECT_NE(Body.find("DoIngameMenuButton(PAGE_SERVER_INFO, \"ingame-server-info-copy-button\""), std::string::npos);
	EXPECT_EQ(Body.find("DoButton_Menu(&s_CopyButton, Localize(\"Copy info\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameMenuButtonKeepsNativeCenteredButtonTextLayout)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "int CMenus::DoIngameMenuButton(int Page, const char *pTextId, CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Flags, int Corners, float Rounding)");
	ASSERT_FALSE(Body.empty());

	// Regression guard for ingame ESC buttons: the budgeted text helper must
	// use the same text rect calculation as DoButton_Menu, while keeping the
	// ingame scope and current-frame fallback. A hand-written rect diverges from
	// native centered labels and made Chinese button text appear off-center.
	EXPECT_NE(Source.find("CUIRect MenuButtonTextRect("), std::string::npos);
	EXPECT_NE(Body.find("CUIRect Text = MenuButtonTextRect(pRect, 0.0f, 0.0f);"), std::string::npos);
	EXPECT_NE(Body.find("DoButton_Menu(pButtonContainer, \"\", Checked"), std::string::npos);
	EXPECT_NE(Body.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_INGAME, TextElement"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->DoLabel(&Text, pText"), std::string::npos);
	EXPECT_EQ(Body.find("CUIRect Text = *pRect;"), std::string::npos);
	EXPECT_EQ(Body.find("Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f"), std::string::npos);
	EXPECT_EQ(Body.find("HoverLift"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameMenuButtonDoesNotDependOnUiRuntimeAnimation)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "int CMenus::DoIngameMenuButton(int Page, const char *pTextId, CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Flags, int Corners, float Rounding)");
	ASSERT_FALSE(Body.empty());

	// Regression guard for Esc-open crashes: ingame buttons can render before
	// the QmUi animation runtime is in a stable menu-frame context. Their
	// cached text path should use the native static button text rect and avoid
	// per-button animation runtime lookups.
	EXPECT_EQ(Body.find("GameClient()->UiRuntimeV2()->AnimRuntime()"), std::string::npos);
	EXPECT_EQ(Body.find("ResolveUiAnimValue("), std::string::npos);
	EXPECT_EQ(Body.find("HoverLift"), std::string::npos);
	EXPECT_NE(Body.find("CUIRect Text = MenuButtonTextRect(pRect, 0.0f, 0.0f);"), std::string::npos);
	EXPECT_NE(Body.find("DoButton_Menu(pButtonContainer, \"\", Checked"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardRightControlsStayCompact)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Screenshot regression: resource cards need the title/description to keep
	// priority. The right-side status pill and action icon are secondary
	// controls, so their reserved widths should stay compact.
	EXPECT_NE(Source.find("constexpr float AssetsCardStatusTagFontSize = 6.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetsCardStatusTagMinWidth = 24.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetsCardStatusTagMaxWidth = 36.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetsCardStatusTagHorizontalPadding = 2.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetCardHeaderControlHeight = 18.0f;"), std::string::npos);
	EXPECT_NE(Body.find("TitleRect.VSplitRight(16.0f, &TitleRect, &Shell.m_ActionButtonRect);"), std::string::npos);
	EXPECT_EQ(Source.find("constexpr float AssetsCardStatusTagMaxWidth = 52.0f;"), std::string::npos);
	EXPECT_EQ(Source.find("constexpr float AssetsCardStatusTagMaxWidth = 46.0f;"), std::string::npos);
	EXPECT_EQ(Body.find("TitleRect.VSplitRight(24.0f, &TitleRect, &Shell.m_ActionButtonRect);"), std::string::npos);
	EXPECT_EQ(Body.find("TitleRect.VSplitRight(20.0f, &TitleRect, &Shell.m_ActionButtonRect);"), std::string::npos);
}

TEST(QmMonitoringHelpers, StreamedLabelCachesSingleLineMiddleAlignMetrics)
{
	const std::string Header = ReadRepoFile("src/game/client/ui.h");
	const std::string Source = ReadRepoFile("src/game/client/ui.cpp");
	const std::string CachedBody = ExtractSourceFunctionBody(Source, "void CUi::DoLabel(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor) const");
	const std::string StreamedBody = ExtractSourceFunctionBody(Source, "void CUi::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render, bool *pTextContainerRecreated) const");
	const std::string HelperBody = ExtractSourceFunctionBody(Source, "void CUi::RenderLabelTextContainerAligned(const CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, int Align) const");
	ASSERT_FALSE(CachedBody.empty());
	ASSERT_FALSE(StreamedBody.empty());
	ASSERT_FALSE(HelperBody.empty());

	// Streamed single-line labels still need the same vertical centering metrics
	// as the immediate DoLabel path. Otherwise centered settings buttons and
	// centered preview placeholders drift upward after the text-pool rewrite.
	EXPECT_NE(Header.find("float m_BiggestCharacterHeight;"), std::string::npos);
	EXPECT_NE(Header.find("int m_LineCount;"), std::string::npos);
	EXPECT_NE(CachedBody.find("TextBounds.m_LineCount == 1 ? &TextBounds.m_BiggestCharacterHeight : nullptr"), std::string::npos);
	EXPECT_NE(CachedBody.find("RectEl.m_BiggestCharacterHeight = TextBounds.m_BiggestCharacterHeight;"), std::string::npos);
	EXPECT_NE(CachedBody.find("RectEl.m_LineCount = TextBounds.m_LineCount;"), std::string::npos);
	EXPECT_NE(HelperBody.find("RectEl.m_LineCount == 1 ? &RectEl.m_BiggestCharacterHeight : nullptr"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RectEl.m_FontSize != Size"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RectEl.m_TextAlign != Align"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RectEl.m_LabelMaxWidth != LabelProps.m_MaxWidth"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RectEl.m_LabelFlags != Flags"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("CalcAlignedCursorPos(pRect, vec2(RectEl.m_Cursor.m_LongestLineWidth, RectEl.m_Cursor.Height()), Align);"), std::string::npos);
}

TEST(QmMonitoringHelpers, StreamedLabelRenderUsesCanonicalAlignmentHelper)
{
	const std::string Source = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Header = ReadRepoFile("src/game/client/ui.h");
	const std::string StreamedBody = ExtractSourceFunctionBody(Source, "void CUi::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render, bool *pTextContainerRecreated) const");
	ASSERT_FALSE(StreamedBody.empty());

	EXPECT_NE(Header.find("RenderLabelTextContainerAligned"), std::string::npos);
	EXPECT_NE(Source.find("void CUi::RenderLabelTextContainerAligned"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RenderLabelTextContainerAligned(RectEl, pRect, Align)"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("TextRender()->RenderTextContainer(RectEl.m_UITextContainer"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("pRect->x, pRect->y"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuLabelStreamedDoesNotBypassCachedLabelAlignment)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Menus, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("Ui()->RenderLabelTextContainerAligned(*pElementRect, pRect, Align);"), std::string::npos);
	EXPECT_EQ(Body.find("TextRender()->RenderTextContainer(pElementRect->m_UITextContainer"), std::string::npos);
	EXPECT_EQ(Body.find("pRect->x, pRect->y"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuTextStyleKeyIncludesScaleButIgnoresAnimatedColorState)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string BuildBody = ExtractSourceFunctionBody(Source, "CMenus::SMenuTextStyleKey CMenus::BuildMenuTextStyleKey(const CUIRect *pRect, float FontSize, int Align, const SLabelProperties &LabelProps) const");
	const std::string CacheKeyBody = ExtractSourceFunctionBody(Source, "std::string MenuTextCacheKey(CMenus::EMenuTextScope Scope, int Page, int Tab, int Subtab, const char *pTextId, const CMenus::SMenuTextStyleKey &StyleKey)");
	const std::string MenuNeedsBuildBody = ExtractSourceFunctionBody(Source, "bool CMenus::MenuTextContainerNeedsBuild(CUIElement &Element, const CUIRect *pRect, const char *pText, int StrLen, const CTextCursor *pReadCursor)");
	const std::string MenuStreamedBody = ExtractSourceFunctionBody(Source, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");
	const std::string UiSource = ReadRepoFile("src/game/client/ui.cpp");
	const std::string StreamedBody = ExtractSourceFunctionBody(UiSource, "void CUi::DoLabelStreamed(CUIElement::SUIElementRect &RectEl, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render, bool *pTextContainerRecreated) const");

	EXPECT_NE(Header.find("int m_HiDpiScaleBucket"), std::string::npos);
	EXPECT_EQ(Header.find("int m_TextColorHash"), std::string::npos);
	EXPECT_EQ(Header.find("int m_OutlineColorHash"), std::string::npos);
	EXPECT_NE(Header.find("SMenuTextStyleKey BuildMenuTextStyleKey"), std::string::npos);
	EXPECT_FALSE(BuildBody.empty());
	EXPECT_FALSE(CacheKeyBody.empty());
	EXPECT_FALSE(MenuNeedsBuildBody.empty());
	EXPECT_FALSE(MenuStreamedBody.empty());
	EXPECT_FALSE(StreamedBody.empty());
	EXPECT_NE(BuildBody.find("Graphics()->ScreenHiDPIScale()"), std::string::npos);
	EXPECT_EQ(BuildBody.find("TextRender()->GetTextColor()"), std::string::npos);
	EXPECT_EQ(BuildBody.find("TextRender()->GetTextOutlineColor()"), std::string::npos);
	EXPECT_EQ(CacheKeyBody.find(":tc"), std::string::npos);
	EXPECT_EQ(CacheKeyBody.find(":oc"), std::string::npos);
	EXPECT_EQ(MenuNeedsBuildBody.find("ColorChanged"), std::string::npos);
	EXPECT_NE(MenuStreamedBody.find("pElementRect->m_TextColor = TextRender()->GetTextColor();"), std::string::npos);
	EXPECT_NE(MenuStreamedBody.find("pElementRect->m_TextOutlineColor = TextRender()->GetTextOutlineColor();"), std::string::npos);
	EXPECT_NE(StreamedBody.find("if(ColorChanged)"), std::string::npos);
	EXPECT_NE(StreamedBody.find("RectEl.m_TextColor = TextRender()->GetTextColor();"), std::string::npos);
	EXPECT_EQ(StreamedBody.find("|| ColorChanged ||"), std::string::npos);
	EXPECT_EQ(Source.find("StyleKey.m_UiScaleBucket = 100"), std::string::npos);
	EXPECT_EQ(Source.find("str_quickhash(\"default-text-style\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsTextUsageSeparatesPoolHitFromRenderReadyHit)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string MenuTextElementBody = ExtractSourceFunctionBody(Source, "CUIElement &CMenus::MenuTextElement(EMenuTextScope Scope, int Page, int Tab, int Subtab, const char *pTextId, const SMenuTextStyleKey &StyleKey)");
	const std::string StreamedBody = ExtractSourceFunctionBody(Source, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");
	ASSERT_FALSE(MenuTextElementBody.empty());
	ASSERT_FALSE(StreamedBody.empty());

	EXPECT_NE(Header.find("m_MenuTextStablePoolHitsThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableRenderReadyHitsThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableBuildQueuedThisFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_MenuTextStableFallbackImmediateThisFrame"), std::string::npos);
	EXPECT_NE(Source.find("pool_hit=%d render_ready_hit=%d"), std::string::npos);
	EXPECT_NE(Source.find("build_queued=%d fallback_immediate=%d"), std::string::npos);
	EXPECT_NE(MenuTextElementBody.find("++m_MenuTextStablePoolHitsThisFrame"), std::string::npos);
	EXPECT_NE(StreamedBody.find("++m_MenuTextStableRenderReadyHitsThisFrame"), std::string::npos);
	EXPECT_NE(StreamedBody.find("++m_MenuTextStableBuildQueuedThisFrame"), std::string::npos);
	EXPECT_NE(StreamedBody.find("++m_MenuTextStableFallbackImmediateThisFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameImmediateTextFallbackIsCountedForSchedulerCoverage)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string TabBody = ExtractSourceFunctionBody(Source, "int CMenus::DoMenuTabV2(CButtonContainer *pButtonContainer, const char *pText, bool Active, const CUIRect *pRect, int Corners, const ColorRGBA *pCustomDefault, const ColorRGBA *pCustomActive, const ColorRGBA *pCustomHover, const CCommunityIcon *pCommunityIcon, CUIElement *pTextUiElement)");
	const std::string ButtonBody = ExtractSourceFunctionBody(Source, "int CMenus::DoIngameMenuButton(int Page, const char *pTextId, CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Flags, int Corners, float Rounding)");
	const std::string LabelBody = ExtractSourceFunctionBody(Source, "void CMenus::DoIngameMenuLabel(int Page, const char *pTextId, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	const std::string TitleBody = ExtractSourceFunctionBody(Source, "void CMenus::DoIngameMenuTitleLabel(int Page, const char *pTextId, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	ASSERT_FALSE(TabBody.empty());
	ASSERT_FALSE(ButtonBody.empty());
	ASSERT_FALSE(LabelBody.empty());
	ASSERT_FALSE(TitleBody.empty());

	EXPECT_NE(Header.find("CountMenuTextImmediateFallback()"), std::string::npos);
	EXPECT_NE(Source.find("void CMenus::CountMenuTextImmediateFallback()"), std::string::npos);
	EXPECT_NE(Source.find("scheduler_coverage=%s"), std::string::npos);
	EXPECT_NE(Source.find("FallbackImmediate > 0 ? \"uncovered\" : \"budgeted\""), std::string::npos);
	EXPECT_NE(TabBody.find("if(pTextUiElement != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid())"), std::string::npos);
	EXPECT_NE(ButtonBody.find("if(&TextElement != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid())"), std::string::npos);
	EXPECT_NE(LabelBody.find("if(&Element != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid() && pRect != nullptr)"), std::string::npos);
	EXPECT_NE(TitleBody.find("if(&Element != &m_MenuTextFallbackElement && !HadReadyContainer && !pElementRect->m_UITextContainer.Valid() && pRect != nullptr)"), std::string::npos);
}

TEST(QmMonitoringHelpers, TextRenderingStabilizationHasVisualChecklist)
{
	const std::string Checklist = ReadRepoFile("docs/superpowers/plans/2026-06-18-text-rendering-stabilization-observability-visual-checklist.md");
	EXPECT_NE(Checklist.find("Button text is centered"), std::string::npos);
	EXPECT_NE(Checklist.find("render-ready hit coverage"), std::string::npos);
	EXPECT_NE(Checklist.find("Small-card right tags/buttons keep fixed priority"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameFixedChromeUsesBudgetedTextPipeline)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string GameBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderGame(CUIRect MainView)");
	const std::string CallVoteBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderServerControl(CUIRect MainView)");
	const std::string UnfinishedBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderUnfinishedMaps(CUIRect MainView)");
	const std::string GhostBody = ExtractSourceFunctionBody(Ingame, "void CMenus::RenderGhost(CUIRect MainView)");
	ASSERT_FALSE(GameBody.empty());
	ASSERT_FALSE(CallVoteBody.empty());
	ASSERT_FALSE(UnfinishedBody.empty());
	ASSERT_FALSE(GhostBody.empty());

	// Fixed ingame chrome is visible immediately when opening ESC, so it needs
	// the ingame text pool and its current-frame fallback. Leaving these labels
	// on raw DoButton/DoLabel paths recreates containers during the first visible
	// frame and caused server-info/title regressions in earlier iterations.
	EXPECT_NE(Header.find("DoIngameMenuCheckBox("), std::string::npos);
	EXPECT_NE(Source.find("int CMenus::DoIngameMenuCheckBox("), std::string::npos);
	EXPECT_NE(GameBody.find("DoIngameMenuButton(PAGE_GAME, \"ingame-game-disconnect\""), std::string::npos);
	EXPECT_NE(GameBody.find("DoIngameMenuButton(PAGE_GAME, \"ingame-game-edit-hud\""), std::string::npos);
	EXPECT_NE(GameBody.find("DoIngameMenuCheckBox(PAGE_GAME, \"ingame-game-edit-touch-controls\""), std::string::npos);
	EXPECT_EQ(GameBody.find("DoButton_Menu(&s_DisconnectButton, Localize(\"Disconnect\")"), std::string::npos);
	EXPECT_EQ(GameBody.find("DoButton_CheckBox(&s_TouchControlsEditCheckbox, Localize(\"Edit touch controls\")"), std::string::npos);

	EXPECT_NE(CallVoteBody.find("DoIngameMenuButton(PAGE_CALLVOTE, \"ingame-call-vote-call\""), std::string::npos);
	EXPECT_NE(CallVoteBody.find("DoIngameMenuLabel(PAGE_CALLVOTE, \"ingame-call-vote-reason-label\""), std::string::npos);
	EXPECT_NE(CallVoteBody.find("DoIngameMenuButton(PAGE_CALLVOTE, \"ingame-call-vote-force\""), std::string::npos);
	EXPECT_EQ(CallVoteBody.find("DoButton_Menu(&s_CallVoteButton, Localize(\"Call vote\")"), std::string::npos);
	EXPECT_EQ(CallVoteBody.find("Ui()->DoLabel(&Reason, pLabel, 14.0f"), std::string::npos);

	EXPECT_NE(UnfinishedBody.find("DoIngameMenuTitleLabel(PAGE_CALLVOTE, \"ingame-unfinished-maps-title\""), std::string::npos);
	EXPECT_NE(UnfinishedBody.find("DoIngameMenuCheckBox(PAGE_CALLVOTE, \"ingame-unfinished-auto-start-vote\""), std::string::npos);
	EXPECT_NE(UnfinishedBody.find("DoIngameMenuButton(PAGE_CALLVOTE, \"ingame-unfinished-random-pick\""), std::string::npos);
	EXPECT_EQ(UnfinishedBody.find("DoButton_CheckBox(&s_UnfinishedMapAutoVote, Localize(\"Auto start vote\")"), std::string::npos);

	EXPECT_NE(GhostBody.find("DoIngameMenuButton(PAGE_GHOST, \"ingame-ghost-directory\""), std::string::npos);
	EXPECT_NE(GhostBody.find("DoIngameMenuButton(PAGE_GHOST, ActivateAll ? \"ingame-ghost-activate-all\" : \"ingame-ghost-deactivate-all\""), std::string::npos);
	EXPECT_NE(GhostBody.find("DoIngameMenuButton(PAGE_GHOST, \"ingame-ghost-delete\""), std::string::npos);
	EXPECT_EQ(GhostBody.find("DoButton_Menu(&s_DirectoryButton, Localize(\"Ghosts directory\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsToolbarAndPlaceholdersUseBudgetedTextPipeline)
{
	const std::string Assets = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Assets, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Assets tab switches should not spend the first visible frame creating
	// toolbar or per-card placeholder text containers. Toolbar text uses the
	// settings text pool, while card placeholders stay visual-only until preview
	// content is ready.
	EXPECT_NE(Body.find("DoSettingsButton_Menu(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, &s_AssetsEditorButton"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsButton_Menu(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, &s_ShowWorkshopAssetsId"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsButton_Menu(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, &s_WorkshopSyncId"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsButton_Menu(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, &s_AssetsDirId"), std::string::npos);
	EXPECT_EQ(Body.find("DoButton_Menu(&s_AssetsEditorButton, Localize(\"Assets editor\")"), std::string::npos);
	EXPECT_EQ(Body.find("DoButton_Menu(&s_ShowWorkshopAssetsId, Localize(\"Show Workshop Assets\")"), std::string::npos);
	EXPECT_EQ(Body.find("DoButton_Menu(&s_WorkshopSyncId, Localize(\"Sync Workshop Assets\")"), std::string::npos);
	EXPECT_EQ(Body.find("DoButton_Menu(&s_AssetsDirId, Localize(\"Assets directory\")"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsMenuLabel(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, \"assets-loading-list\""), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsMenuLabel(SETTINGS_ASSETS, s_CurCustomTab, s_CurCustomTab, \"assets-workshop-no-assets\""), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoLabel(&LoadingRect, Localize(\"Loading"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameCriticalTextFallbacksAreLimited)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string StreamedBody = ExtractSourceFunctionBody(Source, "void CMenus::DoMenuLabelStreamed(EMenuTextScope Scope, CUIElement &Element, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps, int StrLen, const CTextCursor *pReadCursor, bool Render)");
	const std::string TabBody = ExtractSourceFunctionBody(Source, "int CMenus::DoMenuTabV2(CButtonContainer *pButtonContainer, const char *pText, bool Active, const CUIRect *pRect, int Corners, const ColorRGBA *pCustomDefault, const ColorRGBA *pCustomActive, const ColorRGBA *pCustomHover, const CCommunityIcon *pCommunityIcon, CUIElement *pTextUiElement)");
	const std::string TitleBody = ExtractSourceFunctionBody(Source, "void CMenus::DoIngameMenuTitleLabel(int Page, const char *pTextId, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	const std::string LabelBody = ExtractSourceFunctionBody(Source, "void CMenus::DoIngameMenuLabel(int Page, const char *pTextId, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	ASSERT_FALSE(StreamedBody.empty());
	ASSERT_FALSE(TabBody.empty());
	ASSERT_FALSE(TitleBody.empty());
	ASSERT_FALSE(LabelBody.empty());

	// Keep immediate fallback scoped to ingame stable UI chrome. Expanding it
	// inside DoMenuLabelStreamed would make ordinary settings text misses
	// synchronous again and undo the text-budget work.
	EXPECT_EQ(StreamedBody.find("!HadReadyContainer"), std::string::npos);
	EXPECT_NE(TabBody.find("!HadReadyContainer"), std::string::npos);
	EXPECT_NE(TitleBody.find("!HadReadyContainer"), std::string::npos);
	EXPECT_NE(LabelBody.find("!HadReadyContainer"), std::string::npos);
}

TEST(QmMonitoringHelpers, StableTextUsageTelemetryIsClassifiedAsStaticStable)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void LogSettingsTextPoolUsage(IClient *pClient, CMenus::EMenuTextScope Scope, const char *pScopeName, int Page, int Tab, int Subtab, const char *pOperation, uint64_t Frame, int Candidates, int Hits, int Reused, int Misses, int Stales, int TextNew, int TextReused, int Planned, int Unplanned, int PoolHits, int RenderReadyHits, int BuildQueued, int FallbackImmediate)");
	ASSERT_FALSE(Body.empty());

	// Static stable text is the only class allowed in the stable-text
	// denominator. Dynamic snapshot values, paragraphs, and card metadata have
	// separate hit-rate counters so a bad server name or resource title cache
	// cannot make the stable descriptor plan look broken.
	EXPECT_NE(Body.find("text_class=static_stable"), std::string::npos);
	EXPECT_EQ(Body.find("dynamic_snapshot"), std::string::npos);
	EXPECT_EQ(Body.find("paragraph"), std::string::npos);
	EXPECT_EQ(Body.find("card_metadata"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameServerInfoDoesNotTouchUninitializedUiElementRects)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderIngameServerInfoValueCached(const char *pTextId, unsigned &TextHash, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Header.find("CUIElement m_IngameServerNameTextElement"), std::string::npos);
	EXPECT_EQ(Body.find("Element.Rect(0)"), std::string::npos);
	EXPECT_EQ(Body.find("DeleteTextContainer"), std::string::npos);
	EXPECT_NE(Body.find("(void)TextHash;"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameTextRuntimeDrainsOutsideRenderFunctions)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string RenderBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfo(CUIRect MainView)");
	const std::string MotdBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	const std::string SnapshotDrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameUiSnapshotTextRuntime()");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameUiTextRuntime(bool AllowCurrentFrame)");
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(MotdBody.empty());
	ASSERT_FALSE(SnapshotDrainBody.empty());
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(Header.find("void DrainIngameUiSnapshotTextRuntime();"), std::string::npos);
	EXPECT_NE(Header.find("void DrainIngameUiTextRuntime(bool AllowCurrentFrame = false);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrainSnapshotTextContainers()"), std::string::npos);
	EXPECT_EQ(MotdBody.find("DrainIngameMotdParagraphCache("), std::string::npos);
	EXPECT_NE(SnapshotDrainBody.find("DrainSnapshotTextContainers()"), std::string::npos);
	EXPECT_NE(DrainBody.find("DrainIngameUiSnapshotTextRuntime()"), std::string::npos);
	EXPECT_NE(DrainBody.find("DrainIngameMotdParagraphCache("), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameEscOpenDefersServerInfoRuntimeDrain)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	ASSERT_FALSE(OnRenderBody.empty());

	EXPECT_NE(OnRenderBody.find("StartSettingsPerfFixedWindow(\"ingame_esc_open\""), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("PrebuildIngameEscTextPoolBeforeOpen("), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("PrebuildSettingsMenuTextPool("), std::string::npos);
	EXPECT_NE(Header.find("uint64_t m_IngameEscOpenFrame"), std::string::npos);
	EXPECT_NE(Header.find("bool m_IngameServerInfoBackgroundPrepareRequested"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("m_IngameEscOpenFrame = Client()->PerfFrame();"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("m_IngameServerInfoBackgroundPrepareRequested = true;"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("Client()->PerfFrame() > m_IngameEscOpenFrame"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("PrepareIngameServerInfoTextRuntime();"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("if(m_GamePage == PAGE_SERVER_INFO)"), std::string::npos);
	const size_t VisibleServerInfoBranch = OnRenderBody.find("if(m_GamePage == PAGE_SERVER_INFO)");
	const size_t BackgroundPrepareBranch = OnRenderBody.find("else if(m_IngameServerInfoBackgroundPrepareRequested", VisibleServerInfoBranch);
	ASSERT_NE(VisibleServerInfoBranch, std::string::npos);
	ASSERT_NE(BackgroundPrepareBranch, std::string::npos);
	const std::string VisibleBranch = OnRenderBody.substr(VisibleServerInfoBranch, BackgroundPrepareBranch - VisibleServerInfoBranch);
	EXPECT_NE(VisibleBranch.find("DrainIngameUiSnapshotTextRuntime();"), std::string::npos);
	EXPECT_EQ(VisibleBranch.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("else if(GameClient()->m_Motd.ServerMotd()[0])"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("DrainIngameUiSnapshotTextRuntime();"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerInfoLayoutAvoidsTextWidthInRenderPath)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfo(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Body.find("TextRender()->TextWidth"), std::string::npos);
	EXPECT_NE(Body.find("const float ServerInfoLabelWidth"), std::string::npos);
	EXPECT_NE(Body.find("pRow->VSplitLeft(ServerInfoLabelWidth"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdRenderUsesOnlyCurrentMatchingParagraphCache)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Header.find("m_LastStableHeight"), std::string::npos);
	EXPECT_NE(Source.find("bool CMenus::IngameMotdParagraphCacheMatches"), std::string::npos);
	EXPECT_NE(Body.find("const bool CacheReady = IngameMotdParagraphCacheMatches(Motd, MotdFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("if(m_IngameMotdParagraphCache.m_Valid && m_MotdTextContainerIndex.Valid())"), std::string::npos);
	EXPECT_NE(Body.find("if(CacheReady && m_MotdTextContainerIndex.Valid())"), std::string::npos);
}

TEST(QmMonitoringHelpers, DynamicSnapshotTextUsesBudgetedDrain)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string SnapshotDrainBody = ExtractSourceFunctionBody(Ingame, "void CMenus::DrainSnapshotTextContainers()");

	EXPECT_NE(Header.find("struct SMenuSnapshotTextKey"), std::string::npos);
	EXPECT_NE(Header.find("RequestSnapshotTextContainer"), std::string::npos);
	EXPECT_NE(Header.find("DrainSnapshotTextContainers"), std::string::npos);
	EXPECT_NE(Header.find("m_SnapshotTextPending"), std::string::npos);
	EXPECT_NE(Menus.find("m_TextContainerTokens"), std::string::npos);
	EXPECT_NE(Menus.find("m_SnapshotTextCache.clear()"), std::string::npos);
	EXPECT_NE(Menus.find("m_SnapshotTextPending.clear()"), std::string::npos);
	EXPECT_NE(Menus.find("PrebuildSettingsTextPlanItem"), std::string::npos);
	EXPECT_NE(Menus.find("DrainMenuTextContainerBuild(Element, &Item.m_Rect"), std::string::npos);
	EXPECT_NE(Ingame.find("while(m_IngameTextFrameBudget.m_TextContainerTokens > 0 && !m_SnapshotTextPending.empty())"), std::string::npos);
	EXPECT_NE(Ingame.find("--m_IngameTextFrameBudget.m_TextContainerTokens"), std::string::npos);
	EXPECT_EQ(Ingame.find("m_CurrentSettingsUiFrameBudget.m_TextContainerTokens > 0 && !m_SnapshotTextPending.empty()"), std::string::npos);
	EXPECT_NE(Ingame.find("RenderIngameServerInfoValueCached"), std::string::npos);
	EXPECT_NE(Ingame.find("RequestSnapshotTextContainer("), std::string::npos);
	ASSERT_FALSE(SnapshotDrainBody.empty());
	// Snapshot cache miss/container creation is reported by the drain as a
	// frame aggregate. It must not be inferred from paragraph misses, otherwise
	// MOTD misses pollute dynamic short-text hit rate.
	EXPECT_NE(SnapshotDrainBody.find("snapshot_cache_miss=%d"), std::string::npos);
	EXPECT_NE(SnapshotDrainBody.find("text_container_new=%d"), std::string::npos);
	EXPECT_NE(SnapshotDrainBody.find("text_container_uploads=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdParagraphUsesBudgetedDrain)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame)");
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(DrainBody.find("m_IngameTextFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(DrainBody.find("m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(DrainBody.find("--m_IngameTextFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_EQ(DrainBody.find("m_CurrentSettingsUiFrameBudget.m_ParagraphLayoutTokens"), std::string::npos);
	EXPECT_NE(DrainBody.find("TextRender()->CreateOrAppendTextContainer(m_IngameMotdParagraphCache.m_BuildTextContainerIndex"), std::string::npos);
	EXPECT_EQ(DrainBody.find("TextRender()->RecreateTextContainer(m_MotdTextContainerIndex"), std::string::npos);
	EXPECT_NE(DrainBody.find("paragraph_cache_hit=%d"), std::string::npos);
	EXPECT_NE(DrainBody.find("paragraph_cache_miss=%d"), std::string::npos);
	EXPECT_EQ(DrainBody.find("static_stable_hit="), std::string::npos);
	EXPECT_EQ(DrainBody.find("snapshot_cache_miss="), std::string::npos);
}

TEST(QmMonitoringHelpers, VisibleIngameRenderDrainsParagraphOnlyAfterPendingFrame)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Ingame = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	const std::string PrepareBody = ExtractSourceFunctionBody(Ingame, "void CMenus::PrepareIngameServerInfoTextRuntime(const CUIRect *pMainView)");
	ASSERT_FALSE(OnRenderBody.empty());
	ASSERT_FALSE(PrepareBody.empty());

	// Long MOTD paragraph layout can be enqueued by prepare, but the visible
	// frame must not use the same-frame force path or drain paragraph work.
	EXPECT_NE(OnRenderBody.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("DrainIngameUiTextRuntime(true);"), std::string::npos);
	EXPECT_EQ(PrepareBody.find("DrainIngameUiTextRuntime(true);"), std::string::npos);
	EXPECT_EQ(PrepareBody.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_NE(PrepareBody.find("RequestIngameMotdParagraphCache("), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdPrepareHydratesParagraphWithoutSameFrameStarvation)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string PrepareBody = ExtractSourceFunctionBody(Source, "void CMenus::PrepareIngameServerInfoTextRuntime(const CUIRect *pMainView)");
	const std::string DrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame)");
	const std::string RuntimeDrainBody = ExtractSourceFunctionBody(Source, "void CMenus::DrainIngameUiTextRuntime(bool AllowCurrentFrame)");
	ASSERT_FALSE(PrepareBody.empty());
	ASSERT_FALSE(DrainBody.empty());
	ASSERT_FALSE(RuntimeDrainBody.empty());

	// Server-info preparation may enqueue work, but it must respect the normal
	// budgeted drain path. Same-frame force would move long paragraph layout
	// back into the page-open frame.
	EXPECT_NE(Header.find("void DrainIngameMotdParagraphCache(CUIRect Motd, float FontSize, bool AllowCurrentFrame = false);"), std::string::npos);
	EXPECT_NE(Header.find("void DrainIngameUiTextRuntime(bool AllowCurrentFrame = false);"), std::string::npos);
	EXPECT_EQ(PrepareBody.find("DrainIngameUiTextRuntime(true);"), std::string::npos);
	EXPECT_EQ(PrepareBody.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_NE(PrepareBody.find("RequestIngameMotdParagraphCache("), std::string::npos);
	EXPECT_NE(DrainBody.find("!AllowCurrentFrame && Frame <= m_IngameMotdParagraphCache.m_PendingFrame"), std::string::npos);
	EXPECT_NE(RuntimeDrainBody.find("DrainIngameMotdParagraphCache(m_IngameMotdParagraphCache.m_PendingRect, m_IngameMotdParagraphCache.m_PendingFontSize, AllowCurrentFrame);"), std::string::npos);
}

TEST(QmMonitoringHelpers, MotdParagraphPendingDrainsAfterVisibleRequest)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	ASSERT_FALSE(OnRenderBody.empty());

	// If MOTD changes while the server-info page is already open, render can
	// only enqueue a paragraph request. The frame-end drain must continue the
	// budgeted paragraph pipeline on later frames, otherwise the announcement
	// area stays blank until the page is reopened.
	EXPECT_NE(OnRenderBody.find("if(IsActive() && Client()->State() == IClient::STATE_ONLINE)"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("if(m_GamePage == PAGE_SERVER_INFO)"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("DrainIngameUiTextRuntime(false);"), std::string::npos);
	EXPECT_EQ(OnRenderBody.find("else if(GameClient()->m_Motd.ServerMotd()[0])"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("Client()->PerfFrame() > m_IngameEscOpenFrame"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("PrepareIngameServerInfoTextRuntime();"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameTextPlanCollectionDoesNotTouchMotdRuntimeCache)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("if(m_MenuTextPlanCollecting)"), std::string::npos);
	EXPECT_LT(Body.find("if(m_MenuTextPlanCollecting)"), Body.find("GameClient()->m_Motd.ServerMotd()"));
	EXPECT_LT(Body.find("if(m_MenuTextPlanCollecting)"), Body.find("CScrollRegion"));
	EXPECT_LT(Body.find("if(m_MenuTextPlanCollecting)"), Body.find("RequestIngameMotdParagraphCache(Motd"));
	EXPECT_EQ(Body.find("DrainIngameMotdParagraphCache(Motd"), std::string::npos);
	EXPECT_NE(Body.find("return;"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameDynamicTextPlanCollectionDoesNotTouchRuntimeElements)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderIngameServerInfoValueCached(const char *pTextId, unsigned &TextHash, const CUIRect *pRect, const char *pText, float Size, int Align, const SLabelProperties &LabelProps)");
	ASSERT_FALSE(Body.empty());

	const size_t CollectingCheck = Body.find("if(m_MenuTextPlanCollecting)");
	const size_t TextHashWrite = Body.find("TextHash = NewHash;");
	const size_t SnapshotRequest = Body.find("RequestSnapshotTextContainer(");
	ASSERT_NE(CollectingCheck, std::string::npos);
	ASSERT_NE(TextHashWrite, std::string::npos);
	ASSERT_NE(SnapshotRequest, std::string::npos);
	EXPECT_LT(CollectingCheck, TextHashWrite);
	EXPECT_LT(CollectingCheck, SnapshotRequest);
	EXPECT_NE(Body.find("CollectMenuTextPlanItem(MENU_TEXT_SCOPE_INGAME"), std::string::npos);
	EXPECT_EQ(Body.find("Element.Rect(0)"), std::string::npos);
}

TEST(QmMonitoringHelpers, BackgroundTextureRenderSkipsBeforeInterfacesAreReady)
{
	const std::string ComponentHeader = ReadRepoFile("src/game/client/component.h");
	const std::string BackgroundSource = ReadRepoFile("src/game/client/components/background.cpp");
	const std::string Body = ExtractSourceFunctionBody(BackgroundSource, "bool CBackground::RenderBackgroundTexture()");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(ComponentHeader.find("bool InterfacesInitialized() const { return m_pClient != nullptr; }"), std::string::npos);
	EXPECT_NE(Body.find("if(!InterfacesInitialized())"), std::string::npos);
	EXPECT_LT(Body.find("if(!InterfacesInitialized())"), Body.find("Graphics()->GetScreen("));
	EXPECT_NE(Body.find("return false;"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuBackgroundRenderSkipsBeforeInterfacesAreReady)
{
	const std::string MenuBackgroundSource = ReadRepoFile("src/game/client/components/menu_background.cpp");
	const std::string Body = ExtractSourceFunctionBody(MenuBackgroundSource, "bool CMenuBackground::Render()");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("if(!InterfacesInitialized())"), std::string::npos);
	EXPECT_LT(Body.find("if(!InterfacesInitialized())"), Body.find("RenderBackgroundTexture()"));
	EXPECT_LT(Body.find("if(!InterfacesInitialized())"), Body.find("Client()->RenderFrameTime()"));
	EXPECT_NE(Body.find("return false;"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuBackgroundKeepsPositionAcrossIdleFrames)
{
	const std::string MenuBackgroundSource = ReadRepoFile("src/game/client/components/menu_background.cpp");
	const std::string RenderBody = ExtractSourceFunctionBody(MenuBackgroundSource, "bool CMenuBackground::Render()");
	const std::string ChangePositionBody = ExtractSourceFunctionBody(MenuBackgroundSource, "void CMenuBackground::ChangePosition(int PositionNumber)");
	const std::string LoadBody = ExtractSourceFunctionBody(MenuBackgroundSource, "void CMenuBackground::LoadMenuBackground(bool HasDayHint, bool HasNightHint)");
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(ChangePositionBody.empty());
	ASSERT_FALSE(LoadBody.empty());

	EXPECT_EQ(RenderBody.find("m_CurrentPosition = -1;"), std::string::npos);
	EXPECT_NE(ChangePositionBody.find("if(NewPosition == m_CurrentPosition)"), std::string::npos);
	EXPECT_NE(ChangePositionBody.find("return;"), std::string::npos);
	EXPECT_NE(LoadBody.find("InvalidateCurrentPosition();"), std::string::npos);
}

TEST(QmMonitoringHelpers, StartMenuHasConcretePerfStagesAndNoPerFrameV2LayoutAllocation)
{
	const std::string StartMenuSource = ReadRepoFile("src/game/client/components/menus_start.cpp");
	const std::string Body = ExtractSourceFunctionBody(StartMenuSource, "void CMenusStart::RenderStartMenuImpl(CUIRect MainView, bool UseV2Layout)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(StartMenuSource.find("LogStartMenuPerfStage"), std::string::npos);
	EXPECT_NE(Body.find("const bool TrackPerf = QmPerfEnabled();"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_logo"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_external_layout"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_external_buttons"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_main_buttons"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_version_console"), std::string::npos);
	EXPECT_NE(Body.find("start_menu_total"), std::string::npos);
	EXPECT_NE(StartMenuSource.find("static std::vector<SUiLayoutChild> s_vExternalButtonChildren(6);"), std::string::npos);
	EXPECT_NE(StartMenuSource.find("static std::vector<SUiLayoutChild> s_vMainButtonChildren(5);"), std::string::npos);
	EXPECT_EQ(StartMenuSource.find("std::vector<SUiLayoutChild> vChildren("), std::string::npos);
	EXPECT_EQ(StartMenuSource.find("CPerfTimer"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuIdleRenderThrottleWaitsForRealIdleStateAndRespectsScreenRefreshRate)
{
	const std::string MenusSource = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(MenusSource, "int CMenus::IdleRenderFrameRate() const");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(MenusHeader.find("std::chrono::nanoseconds m_LastMenuInteractionTime"), std::string::npos);
	EXPECT_NE(MenusSource.find("constexpr int MENU_IDLE_REFRESH_RATE = 60;"), std::string::npos);
	EXPECT_NE(MenusSource.find("constexpr auto MENU_IDLE_INTERACTION_GRACE_TIME = 450ms;"), std::string::npos);
	EXPECT_NE(Body.find("!InterfacesInitialized()"), std::string::npos);
	EXPECT_NE(Body.find("Ui()->ActiveItem() != nullptr"), std::string::npos);
	EXPECT_NE(Body.find("CLineInput::GetActiveInput() != nullptr"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_KeyBinder.IsActive()"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_MenuBackground.IsLoading()"), std::string::npos);
	EXPECT_NE(Body.find("UiRuntimeStats.m_ActiveAnimCount > 0"), std::string::npos);
	EXPECT_NE(Body.find("m_SettingsPerfWindowTracker.HasActiveWindow()"), std::string::npos);
	EXPECT_NE(Body.find("SettingsTextPlanCollectionRemaining() > 0"), std::string::npos);
	EXPECT_NE(Body.find("CountMissingSettingsMenuTextPlanItems() > 0"), std::string::npos);
	EXPECT_NE(Body.find("time_get_nanoseconds() - m_LastMenuInteractionTime < MENU_IDLE_INTERACTION_GRACE_TIME"), std::string::npos);
	EXPECT_NE(Body.find("return maximum(MENU_IDLE_REFRESH_RATE, g_Config.m_GfxScreenRefreshRate);"), std::string::npos);
}

TEST(QmMonitoringHelpers, ClientRenderLoopUsesGameClientIdleThrottleWithOneFrameRatePath)
{
	const std::string ClientSource = ReadRepoFile("src/engine/client/client.cpp");
	const std::string ClientInterface = ReadRepoFile("src/engine/client.h");
	const std::string GameClientHeader = ReadRepoFile("src/game/client/gameclient.h");
	const std::string GameClientSource = ReadRepoFile("src/game/client/gameclient.cpp");
	const std::string Forwarder = ExtractSourceFunctionBody(GameClientSource, "int CGameClient::RenderThrottleRefreshRate() const");
	ASSERT_FALSE(Forwarder.empty());

	EXPECT_NE(ClientInterface.find("virtual int RenderThrottleRefreshRate() const = 0;"), std::string::npos);
	EXPECT_NE(GameClientHeader.find("int RenderThrottleRefreshRate() const override;"), std::string::npos);
	EXPECT_NE(Forwarder.find("return m_Menus.IdleRenderFrameRate();"), std::string::npos);
	EXPECT_NE(ClientSource.find("RequestedRenderThrottleRate = GameClient()->RenderThrottleRefreshRate();"), std::string::npos);
	EXPECT_NE(ClientSource.find("GfxRefreshRate = std::clamp(RequestedRenderThrottleRate, 10, 10000);"), std::string::npos);
	EXPECT_NE(ClientSource.find("IdleRenderThrottleRate = GfxRefreshRate;"), std::string::npos);
	EXPECT_NE(ClientSource.find("int LastIdleRenderThrottleRate = -1;"), std::string::npos);
	EXPECT_NE(ClientSource.find("event=idle_render_throttle rate=%d requested=%d configured=%d vsync=%d"), std::string::npos);
	EXPECT_NE(ClientSource.find("LastIdleRenderThrottleRate = IdleRenderThrottleRate;"), std::string::npos);
	EXPECT_NE(ClientSource.find("fs_makedir_rec_for(aPerfLogCompletePath);"), std::string::npos);
	EXPECT_NE(ClientSource.find("PerfLogfile = io_open(aPerfLogCompletePath, IOFLAG_WRITE);"), std::string::npos);
	EXPECT_NE(ClientSource.find("char aWorkingDir[IO_MAX_PATH_LENGTH];"), std::string::npos);
	EXPECT_NE(ClientSource.find("if(fs_getcwd(aWorkingDir, sizeof(aWorkingDir)))"), std::string::npos);
	EXPECT_NE(ClientSource.find("str_format(aPerfLogCompletePath, sizeof(aPerfLogCompletePath), \"%s/%s\", aWorkingDir, aPerfLogPath);"), std::string::npos);
	EXPECT_NE(ClientSource.find("const int64_t RenderFrameTicks = GfxRefreshRate > 0 ? time_freq() / (int64_t)GfxRefreshRate : 0;"), std::string::npos);
	EXPECT_NE(ClientSource.find("(!GfxRefreshRate || RenderFrameTicks <= Now - LastRenderTime)"), std::string::npos);
	EXPECT_NE(ClientSource.find("int64_t AdditionalTime = GfxRefreshRate ? ((Now - LastRenderTime) - RenderFrameTicks) : 0;"), std::string::npos);
	EXPECT_NE(ClientSource.find("state=%d render_rate=%d throttle=%d"), std::string::npos);
	EXPECT_NE(ClientSource.find("const auto WaitWithNetwork = [&](std::chrono::nanoseconds WaitTime)"), std::string::npos);
	EXPECT_NE(ClientSource.find("else if(IdleRenderThrottleRate > 0)"), std::string::npos);
	EXPECT_NE(ClientSource.find("SleepTimeInNanoSeconds = (std::chrono::nanoseconds(1s) / (int64_t)IdleRenderThrottleRate) - (Now - LastTime);"), std::string::npos);
	EXPECT_EQ(ClientSource.find("time_freq() / (int64_t)g_Config.m_GfxRefreshRate"), std::string::npos);
}

TEST(QmMonitoringHelpers, ConsoleQueuedResultCopyPreservesExternalArguments)
{
	const std::string ConsoleSource = ReadRepoFile("src/engine/shared/console.cpp");
	const std::string Body = ExtractSourceFunctionBody(ConsoleSource, "CConsole::CResult::CResult(const CResult &Other)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(ConsoleSource.find("const char *CConsole::CResult::CopyArgumentPointer"), std::string::npos);
	EXPECT_NE(Body.find("CopyArgumentPointer(Other.m_pArgsStart, Other)"), std::string::npos);
	EXPECT_NE(Body.find("CopyArgumentPointer(Other.m_pCommand, Other)"), std::string::npos);
	EXPECT_NE(Body.find("CopyArgumentPointer(Other.m_apArgs[i], Other)"), std::string::npos);
	EXPECT_EQ(Body.find("m_aStringStorage + (Other.m_apArgs[i] - Other.m_aStringStorage)"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsLoadingPrewarmApiIsPublic)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const size_t StartLoading = Header.find("void StartLoading(int Total);");
	const size_t PrewarmSettingsPages = Header.find("void PrewarmSettingsPages();");
	const size_t IsInit = Header.find("bool IsInit() const");
	ASSERT_NE(StartLoading, std::string::npos);
	ASSERT_NE(PrewarmSettingsPages, std::string::npos);
	ASSERT_NE(IsInit, std::string::npos);

	EXPECT_GT(PrewarmSettingsPages, StartLoading);
	EXPECT_LT(PrewarmSettingsPages, IsInit);
}

TEST(QmMonitoringHelpers, TextRuntimeTelemetryReportsGlyphContainerAndParagraphCosts)
{
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");

	EXPECT_NE(Text.find("glyph_rasterize_ms"), std::string::npos);
	EXPECT_NE(Text.find("text_container_create_ms"), std::string::npos);
	EXPECT_NE(Text.find("text_container_upload_ms"), std::string::npos);
	EXPECT_NE(Stats.find("glyphRasterizeMs"), std::string::npos);
	EXPECT_NE(Stats.find("paragraphLayoutMs"), std::string::npos);
	EXPECT_NE(Stats.find("paragraphBudgetBlocked"), std::string::npos);
	EXPECT_NE(Stats.find("paragraphCacheHit"), std::string::npos);
	EXPECT_NE(Stats.find("paragraphCacheMiss"), std::string::npos);
}

TEST(QmMonitoringHelpers, TextRuntimeCountersDoNotAccumulateWhilePerfDisabled)
{
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Body = ExtractSourceFunctionBody(Text, "void FlushQmTextRuntimeBudgetLog() override");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Text.find("void ResetQmTextRuntimeBudgetCounters(bool ConsumeGlyphStats)"), std::string::npos);
	EXPECT_NE(Body.find("m_pGlyphMap->ConsumeQmPerfGlyphStats"), std::string::npos);
	EXPECT_NE(Body.find("if(!QmPerfEnabled()"), std::string::npos);
	EXPECT_LT(Body.find("m_pGlyphMap->ConsumeQmPerfGlyphStats"), Body.find("if(!QmPerfEnabled()"));
	EXPECT_GT(Body.find("ResetQmTextRuntimeBudgetCounters(false);", Body.find("if(!QmPerfEnabled()")), Body.find("if(!QmPerfEnabled()"));
}

TEST(QmMonitoringHelpers, TextRuntimeSnapshotUpdatesBeforePerfLoggingGate)
{
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Body = ExtractSourceFunctionBody(Text, "void FlushQmTextRuntimeBudgetLog() override");
	ASSERT_FALSE(Body.empty());

	// The adaptive settings scheduler consumes this snapshot during normal
	// gameplay, so it must not depend on the perf log/debug switch being on.
	EXPECT_NE(Body.find("UpdateQmTextRuntimeBudgetSnapshot(GlyphNew, GlyphUploads, GlyphRasterizeMs, GlyphUploadMs);"), std::string::npos);
	EXPECT_NE(Body.find("if(!QmPerfEnabled()"), std::string::npos);
	EXPECT_LT(Body.find("UpdateQmTextRuntimeBudgetSnapshot(GlyphNew, GlyphUploads, GlyphRasterizeMs, GlyphUploadMs);"), Body.find("if(!QmPerfEnabled()"));
}

TEST(QmMonitoringHelpers, TextRuntimeTelemetrySkipsLowCostSingleContainerNoise)
{
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Body = ExtractSourceFunctionBody(Text, "void FlushQmTextRuntimeBudgetLog() override");
	ASSERT_FALSE(Body.empty());

	// A long perf run can create millions of cheap one-off text containers.
	// Keep high-cost/glyph/upload rows for attribution, but do not log every
	// sub-threshold single-container create as its own perf/text line.
	EXPECT_NE(Text.find("bool ShouldLogTextRuntimeBudget("), std::string::npos);
	EXPECT_NE(Text.find("TextContainerWork >= 8"), std::string::npos);
	EXPECT_NE(Body.find("if(!ShouldLogTextRuntimeBudget("), std::string::npos);
	EXPECT_NE(Body.find("ResetQmTextRuntimeBudgetCounters(false);"), std::string::npos);
}

TEST(QmMonitoringHelpers, TextRuntimeTelemetryFlushesOncePerUiFrame)
{
	const std::string Header = ReadRepoFile("src/engine/textrender.h");
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string CreateBody = ExtractSourceFunctionBody(Text, "bool CreateTextContainer(STextContainerIndex &TextContainerIndex, CTextCursor *pCursor, const char *pText, int Length = -1) override");
	const std::string UploadBody = ExtractSourceFunctionBody(Text, "void UploadTextContainer(STextContainerIndex TextContainerIndex) override");
	const std::string OnRenderBody = ExtractSourceFunctionBody(Menus, "void CMenus::OnRender()");
	ASSERT_FALSE(CreateBody.empty());
	ASSERT_FALSE(UploadBody.empty());
	ASSERT_FALSE(OnRenderBody.empty());

	// Regression guard for 2GB perf logs: text render hot paths may only
	// accumulate counters. The UI frame owns the single aggregated flush.
	EXPECT_NE(Header.find("FlushQmTextRuntimeBudgetLog"), std::string::npos);
	EXPECT_EQ(CreateBody.find("LogQmTextRuntimeBudget("), std::string::npos);
	EXPECT_EQ(UploadBody.find("LogQmTextRuntimeBudget("), std::string::npos);
	EXPECT_NE(OnRenderBody.find("TextRender()->FlushQmTextRuntimeBudgetLog()"), std::string::npos);
}

TEST(QmMonitoringHelpers, TextRenderExposesRuntimeBudgetSnapshotForScheduler)
{
	const std::string Header = ReadRepoFile("src/engine/textrender.h");
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");

	EXPECT_NE(Header.find("struct SQmTextRuntimeBudgetSnapshot"), std::string::npos);
	EXPECT_NE(Header.find("virtual SQmTextRuntimeBudgetSnapshot QmTextRuntimeBudgetSnapshot() const"), std::string::npos);
	EXPECT_NE(Text.find("SQmTextRuntimeBudgetSnapshot m_QmLastTextRuntimeBudgetSnapshot"), std::string::npos);
	EXPECT_NE(Text.find("m_QmLastTextRuntimeBudgetSnapshot.m_TextContainerCreateMs"), std::string::npos);
}

TEST(QmMonitoringHelpers, HudSettingsTextHydratesUnderBudget)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string HudBranch = ExtractSourceBlock(Settings, "if(m_AppearanceSettingsTab == APPEARANCE_TAB_HUD)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_CHAT)");
	ASSERT_FALSE(HudBranch.empty());

	EXPECT_NE(Settings.find("APPEARANCE_TAB_HUD"), std::string::npos);
	EXPECT_NE(Settings.find("DoAppearanceNumericField(APPEARANCE_TAB_HUD"), std::string::npos);
	EXPECT_NE(HudBranch.find("DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_APPEARANCE, APPEARANCE_TAB_HUD"), std::string::npos);
	EXPECT_EQ(HudBranch.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_NE(HudBranch.find("DoSettingsLabelStreamed"), std::string::npos);
	EXPECT_EQ(HudBranch.find("Ui()->DoLabel_AutoLineSize"), std::string::npos);
	EXPECT_NE(Menus.find("m_pSettingsTextPrebuildBudget"), std::string::npos);
	EXPECT_NE(Menus.find("BeginSettingsUiFrameScheduler("), std::string::npos);
	EXPECT_NE(Menus.find("DrainMenuTextContainerBuildRequests()"), std::string::npos);
	EXPECT_NE(Menus.find("AdaptiveBudget.m_TextPrebuildTokens"), std::string::npos);
}

TEST(QmMonitoringHelpers, AppearanceHudChatNamePlateCheckboxesUseBudgetedTextPipeline)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string HudBranch = ExtractSourceBlock(Settings, "if(m_AppearanceSettingsTab == APPEARANCE_TAB_HUD)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_CHAT)");
	const std::string ChatBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_CHAT)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)");
	const std::string NamePlateBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_HOOK_COLLISION)");
	const std::string HookCollisionBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_HOOK_COLLISION)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_INFO_MESSAGES)");
	const std::string InfoMessagesBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_INFO_MESSAGES)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_LASER)");
	ASSERT_FALSE(HudBranch.empty());
	ASSERT_FALSE(ChatBranch.empty());
	ASSERT_FALSE(NamePlateBranch.empty());
	ASSERT_FALSE(HookCollisionBranch.empty());
	ASSERT_FALSE(InfoMessagesBranch.empty());

	// These visible Appearance subtabs contain many fixed checkbox labels.
	// They must use the settings text cache/drain wrappers so first entry does
	// not synchronously create text containers for every visible checkbox.
	EXPECT_NE(HudBranch.find("DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_APPEARANCE, APPEARANCE_TAB_HUD"), std::string::npos);
	EXPECT_NE(ChatBranch.find("DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_APPEARANCE, APPEARANCE_TAB_CHAT"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_APPEARANCE, APPEARANCE_TAB_NAME_PLATE"), std::string::npos);
	EXPECT_EQ(HudBranch.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_EQ(ChatBranch.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_EQ(ChatBranch.find("DoButton_CheckBox("), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("DoButton_CheckBox("), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("DoButton_Menu(&s_NameplateResetLayoutButton"), std::string::npos);
	EXPECT_NE(HookCollisionBranch.find("DoSettingsButton_CheckBox(SETTINGS_APPEARANCE, APPEARANCE_TAB_HOOK_COLLISION"), std::string::npos);
	EXPECT_NE(InfoMessagesBranch.find("DoSettingsButton_CheckBox(SETTINGS_APPEARANCE, APPEARANCE_TAB_INFO_MESSAGES"), std::string::npos);
	EXPECT_EQ(HookCollisionBranch.find("DoButton_CheckBox("), std::string::npos);
	EXPECT_EQ(InfoMessagesBranch.find("DoButton_CheckBox("), std::string::npos);
}

TEST(QmMonitoringHelpers, AppearanceNamePlateHookStrengthSizeUsesSingleLineSlider)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string NamePlateBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_HOOK_COLLISION)");
	ASSERT_FALSE(NamePlateBranch.empty());

	EXPECT_NE(NamePlateBranch.find("DoAppearanceNumericField(APPEARANCE_TAB_NAME_PLATE, \"appearance-hook-strength-size\""), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("SCROLLBAR_OPTION_MULTILINE"), std::string::npos);
	EXPECT_NE(ReadRepoFile("src/game/client/components/menus_settings.cpp").find("Localize(\"Glow\")"), std::string::npos);
	EXPECT_NE(ReadRepoFile("src/game/client/components/menus_settings.cpp").find("Localize(\"Glow range\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, AppearanceNamePlateTabUsesCardBackedScrollRegion)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string NamePlateBranch = ExtractSourceBlock(Settings, "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_NAME_PLATE)", "else if(m_AppearanceSettingsTab == APPEARANCE_TAB_HOOK_COLLISION)");
	ASSERT_FALSE(NamePlateBranch.empty());

	EXPECT_EQ(NamePlateBranch.find("DrawTClientCacheSectionBox("), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("s_NamePlateSettingsCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("s_NamePlateMeasuredCardHeight"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static float s_NamePlateMeasuredSettingsCardHeight = 0.0f;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("static float s_NamePlateMeasuredPreviewCardHeight = 0.0f;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("const float QmClientSettingsScrollbarWidth"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("const float QmClientSettingsScrollbarMargin = std::clamp(8.0f * UiScale, 6.0f, 8.0f);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("const float NamePlateContentPaddingY = std::clamp(14.0f * UiScale, 10.0f, 14.0f);"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("const float NamePlateRadioLineHeight = 22.0f;"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("AddCard(5, NamePlateSettingsMinCardHeight"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("AddCard(6, NamePlatePreviewMinCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("const float NamePlateCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlateSettingsCard.h = NamePlateSettingsCardHeight;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlateSettingsShadow.Draw(NamePlateSettingsShadowColor, IGraphics::CORNER_ALL, NamePlateCardCornerRadius);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlateSettingsCard.Draw(NamePlateSettingsGlassColor, IGraphics::CORNER_ALL, NamePlateCardCornerRadius);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlateSettingsTopHighlight.Draw(NamePlateSettingsHighlightColor, IGraphics::CORNER_NONE, 0.0f);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("LeftView.HSplitTop(NamePlateContentPaddingY, nullptr, &LeftView);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("LeftView.HSplitBottom(NamePlateContentPaddingY, &LeftView, nullptr);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewCard.h = NamePlateCardHeight;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewCard.h = NamePlatePreviewCardHeight;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewShadow.Draw(NamePlateSettingsShadowColor, IGraphics::CORNER_ALL, NamePlateCardCornerRadius);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewCard.Draw(NamePlateSettingsGlassColor, IGraphics::CORNER_ALL, NamePlateCardCornerRadius);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("NamePlatePreviewTopHighlight.Draw(NamePlateSettingsHighlightColor, IGraphics::CORNER_NONE, 0.0f);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("RightView.HSplitTop(NamePlateContentPaddingY, nullptr, &RightView);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("RightView.HSplitBottom(NamePlateContentPaddingY, &RightView, nullptr);"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("UpdateMeasuredCardHeight(s_NamePlateMeasuredSettingsCardHeight"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("UpdateMeasuredCardHeight(s_NamePlateMeasuredPreviewCardHeight"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("BeginSettingsScrollRegion(s_NamePlateSettingsScrollRegion, &NamePlateSettingsView"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("60.0f"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("ScrollParams.m_ScrollbarWidth ="), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("ScrollParams.m_ScrollbarMargin = QmClientSettingsScrollbarMargin;"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("ScrollParams.m_ScrollbarWidth = std::clamp(20.0f * UiScale, 18.0f, 20.0f);"), std::string::npos);
	EXPECT_EQ(NamePlateBranch.find("ScrollParams.m_ScrollbarMargin = std::clamp(5.0f * UiScale, 4.0f, 5.0f);"), std::string::npos);
	EXPECT_NE(NamePlateBranch.find("RenderNamePlatePreview"), std::string::npos);
	EXPECT_NE(Settings.find("pAppearanceDeckTab = m_AppearanceSettingsTab == APPEARANCE_TAB_HUD ? \"appearance-hud\""), std::string::npos);
	EXPECT_NE(Settings.find("std::array<CScrollRegion, NUMBER_OF_APPEARANCE_TABS> s_AppearanceSettingsCardScrollRegions"), std::string::npos);
	EXPECT_NE(Settings.find("m_SettingsCardDeck.Render(AppearanceCardCtx, AppearancePage, pAppearanceDeckTab"), std::string::npos);
	EXPECT_NE(Settings.find("s_AppearanceSettingsCardScrollRegions[m_AppearanceSettingsTab]"), std::string::npos);
}

TEST(QmMonitoringHelpers, AppearanceSettingsHeadingsUseBudgetedTextPipeline)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Settings, "void CMenus::RenderSettingsAppearance(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Appearance subtab headings are visible settings chrome. They must go
	// through the settings text cache/drain path instead of direct UI labels,
	// otherwise first-entry HUD/Appearance tabs can create containers in render.
	EXPECT_NE(Body.find("auto DoAppearanceHeading"), std::string::npos);
	EXPECT_NE(Body.find("SettingsTextElement(SETTINGS_APPEARANCE, m_AppearanceSettingsTab"), std::string::npos);
	EXPECT_NE(Body.find("DoSettingsLabelStreamed(HeadingElement"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoLabel_AutoLineSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, ControlsSettingsChromeUsesBudgetedTextPipeline)
{
	const std::string Controls = ReadRepoFile("src/game/client/components/menus_settings_controls.cpp");

	// The Controls settings page has a dense first frame: block headings,
	// option labels, controller labels and bind labels all sit above the fold.
	// They must use the shared settings text cache/drain path so first entry
	// cannot synchronously create a large batch of text containers in render.
	EXPECT_NE(Controls.find("DoSettingsControlsLabel("), std::string::npos);
	EXPECT_NE(Controls.find("DoSettingsControlsCheckBox("), std::string::npos);
	EXPECT_NE(Controls.find("ui_widget::NumericField("), std::string::npos);
	EXPECT_NE(Controls.find("DoSettingsButton_Menu(CMenus::SETTINGS_CONTROLS"), std::string::npos);
	EXPECT_EQ(Controls.find("Ui()->DoLabel("), std::string::npos);
	EXPECT_EQ(Controls.find("Ui()->DoLabel_AutoLineSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeSkinFilterCheckboxesUseBudgetedTextPipeline)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string TeeBody = ExtractSourceFunctionBody(Settings, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(TeeBody.empty());

	// The Tee page is list-heavy, but the fixed skin filter checkboxes are
	// still settings chrome. Keep them on the shared settings text cache/drain
	// path instead of direct checkbox labels.
	EXPECT_NE(TeeBody.find("DoSettingsButton_CheckBox(SETTINGS_TEE"), std::string::npos);
	EXPECT_EQ(TeeBody.find("DoButton_CheckBox(&g_Config.m_ClDownloadSkins"), std::string::npos);
	EXPECT_EQ(TeeBody.find("DoButton_CheckBox(&g_Config.m_ClDownloadCommunitySkins"), std::string::npos);
	EXPECT_EQ(TeeBody.find("DoButton_CheckBox(&g_Config.m_ClVanillaSkinsOnly"), std::string::npos);
	EXPECT_EQ(TeeBody.find("DoButton_CheckBox(&g_Config.m_ClFatSkins"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsRadioMenusUseBudgetedTextPipeline)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Controls = ReadRepoFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	// Settings radio rows are fixed UI chrome: one label plus a small set of
	// fixed buttons. They should use the same text cache/drain helpers as
	// checkboxes and scrollbars instead of the generic direct-label helper.
	EXPECT_NE(Header.find("DoSettingsLine_RadioMenu("), std::string::npos);
	EXPECT_NE(Source.find("bool CMenus::DoSettingsLine_RadioMenu("), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsLabel(Page, Tab, pLabelTextId"), std::string::npos);
	EXPECT_NE(Source.find("DoSettingsButton_Menu(Page, Tab, Subtab"), std::string::npos);
	EXPECT_EQ(Controls.find("DoLine_RadioMenu("), std::string::npos);
	EXPECT_EQ(Settings.find("DoLine_RadioMenu("), std::string::npos);
	EXPECT_EQ(TClient.find("DoLine_RadioMenu("), std::string::npos);
	EXPECT_EQ(QmClient.find("DoLine_RadioMenu("), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientVisibleCheckboxesUseBudgetedTextPipeline)
{
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::vector<std::string> vFunctionBodies = {
		ExtractSourceFunctionBody(TClient, "float CMenus::LayoutTClientAutoReplyCacheSection(CUIRect &CurrentColumn, bool Render)"),
		ExtractSourceFunctionBody(TClient, "float CMenus::LayoutTClientPetCacheSection(CUIRect &CurrentColumn, bool Render)"),
		ExtractSourceFunctionBody(TClient, "float CMenus::LayoutTClientHudCacheSection(CUIRect &CurrentColumn, bool Render)")};
	const std::vector<std::string> vRenderBlocks = {
		ExtractSourceBlock(TClient, "auto LayoutVisualNameplateSection", "auto LayoutVisualEffectsSection"),
		ExtractSourceBlock(TClient, "auto LayoutVisualEffectsSection", "auto LayoutInputSection"),
		ExtractSourceBlock(TClient, "auto LayoutInputSection", "auto LayoutAntiLatencyToolsSection"),
		ExtractSourceBlock(TClient, "auto LayoutAntiLatencyToolsSection", "auto LayoutAntiPingSmoothingSection"),
		ExtractSourceBlock(TClient, "auto LayoutAntiPingSmoothingSection", "auto LayoutAutoExecuteSection"),
		ExtractSourceBlock(TClient, "auto LayoutPetSection", "auto MeasurePetSection"),
		ExtractSourceBlock(TClient, "auto RenderPetInteractiveSection", "auto LayoutAutoReplySection"),
		ExtractSourceBlock(TClient, "auto LayoutAutoReplySection", "auto MeasureAutoReplySection"),
		ExtractSourceBlock(TClient, "auto RenderAutoReplyInteractiveSection", "auto LayoutPlayerIndicatorSection"),
		ExtractSourceBlock(TClient, "auto LayoutPlayerIndicatorSection", "// ---- CSectionLoader")};

	EXPECT_NE(Header.find("DoTClientSettingsButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_NE(Header.find("DoTClientSettingsButton_CheckBox("), std::string::npos);
	EXPECT_NE(Header.find("DoTClientSettingsButton_Menu("), std::string::npos);
	EXPECT_NE(TClient.find("int CMenus::DoTClientSettingsButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_NE(TClient.find("int CMenus::DoTClientSettingsButton_CheckBox("), std::string::npos);
	EXPECT_NE(TClient.find("int CMenus::DoTClientSettingsButton_Menu("), std::string::npos);
	EXPECT_NE(TClient.find("return DoSettingsButton_CheckBoxAutoVMarginAndSet(SETTINGS_TCLIENT, m_TClientSettingsTab"), std::string::npos);
	EXPECT_NE(TClient.find("return DoSettingsButton_CheckBox(SETTINGS_TCLIENT, m_TClientSettingsTab"), std::string::npos);
	EXPECT_NE(TClient.find("return DoSettingsButton_Menu(SETTINGS_TCLIENT, m_TClientSettingsTab"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_CheckBox(&g_Config"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_CheckBox(&m_Dummy"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_CheckBox(&s_CustomColorId"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_CheckBox(&s_TcUiTag"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_LoadButton, Localize(\"Load\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_SaveButton, Localize(\"Save\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_DeleteButton, Localize(\"Delete\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_OverrideButton, Localize(\"Override\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_ProfilesFile, Localize(\"Profiles file\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_ApplyBtn, Localize(\"Apply Changes\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&s_ClearBtn, Localize(\"Clear Changes\")"), std::string::npos);
	EXPECT_EQ(TClient.find("DoButton_Menu(&ResetBtn, Localize(\"Reset\")"), std::string::npos);
	for(const auto &Body : vFunctionBodies)
	{
		ASSERT_FALSE(Body.empty());
		EXPECT_NE(Body.find("DoTClientSettingsButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
		EXPECT_EQ(Body.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	}
	for(const auto &Block : vRenderBlocks)
	{
		ASSERT_FALSE(Block.empty());
		EXPECT_NE(Block.find("DoTClientSettingsButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
		EXPECT_EQ(Block.find("DoButton_CheckBoxAutoVMarginAndSet("), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, AssetsEntityBgPreviewUsesArtifactJob)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Preview = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/components/qmclient/settings_resource_preview.h>"), std::string::npos);
	EXPECT_NE(Preview.find("CSettingsResourcePreviewJob"), std::string::npos);
	EXPECT_NE(Source.find("SResourcePreviewKey"), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewCache"), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewScheduler"), std::string::npos);
	EXPECT_NE(Body.find("SettingsResourcePreviewDrawResult"), std::string::npos);
	EXPECT_NE(Body.find("ESettingsResourcePreviewDrawResult::PLACEHOLDER"), std::string::npos);
	EXPECT_NE(Body.find("preview_jobs_started=%d preview_jobs_done=%d preview_uploads=%d preview_admissions=%d preview_artifact_ms=%.3f metadata_hydrate_ms=%.3f metadata_hydrated=%d placeholder_count=%d ready_texture_count=%d visible_ready_ratio=%.3f"), std::string::npos);
	EXPECT_NE(Body.find("CanStartPreviewJob(ESettingsResourcePreviewPriority::VISIBLE"), std::string::npos);
	EXPECT_NE(Source.find("CSettingsResourcePreviewUploadScheduler gs_SettingsAssetsResourcePreviewUploadScheduler"), std::string::npos);
	EXPECT_NE(Source.find("ProcessAssetsResourcePreviewJobs("), std::string::npos);
	EXPECT_NE(Source.find("StartAssetsEntityBgPreviewArtifactJob("), std::string::npos);
	EXPECT_NE(Source.find("m_vEntityBgPreviewJobs"), std::string::npos);
	EXPECT_NE(Body.find("ProcessAssetsResourcePreviewJobs(Graphics(), ResourcePreviewTelemetry"), std::string::npos);
	EXPECT_NE(Body.find("StartAssetsEntityBgPreviewArtifactJob(PreviewKey"), std::string::npos);
	EXPECT_EQ(Body.find("gs_SettingsAssetsResourcePreviewCache.MarkPreviewJobStarted(PreviewKey);"), std::string::npos);
	EXPECT_NE(Body.find("SettingsResourcePreviewDrawResult(ResourcePreviewState)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntityBgPreviewArtifactJobDoesNotDependOnExistingPreviewImage)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string PreviewHeader = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string PreviewSource = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string StartBody = ExtractSourceFunctionBody(Source, "static bool StartAssetsEntityBgPreviewArtifactJob");
	ASSERT_FALSE(StartBody.empty());

	EXPECT_EQ(StartBody.find("pItem->m_PreviewImage.m_pData == nullptr"), std::string::npos);
	EXPECT_EQ(StartBody.find("m_PreviewImage.DeepCopy()"), std::string::npos);
	EXPECT_NE(StartBody.find("ResolveEntityBgPreviewArtifactSource("), std::string::npos);
	EXPECT_NE(StartBody.find("CSettingsResourcePreviewJob::FromPath("), std::string::npos);
	EXPECT_NE(PreviewHeader.find("static std::shared_ptr<CSettingsResourcePreviewJob> FromPath("), std::string::npos);
	EXPECT_NE(PreviewSource.find("LoadPng("), std::string::npos);
	EXPECT_NE(PreviewSource.find("BuildPreviewArtifactFromPath("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardDrawLoopDoesNotRunHeavyPreview)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t RenderPreviewStart = Source.find("auto RenderAssetsCardPreview = ");
	ASSERT_NE(RenderPreviewStart, std::string::npos);
	const size_t RenderPreviewEnd = Source.find("auto RenderAssetsCardLoadingShells", RenderPreviewStart);
	ASSERT_NE(RenderPreviewEnd, std::string::npos);
	const std::string RenderPreviewBody = Source.substr(RenderPreviewStart, RenderPreviewEnd - RenderPreviewStart);

	EXPECT_NE(RenderPreviewBody.find("SettingsResourcePreviewDrawResult"), std::string::npos);
	EXPECT_NE(RenderPreviewBody.find("DrawResourcePreviewPlaceholder"), std::string::npos);
	EXPECT_EQ(RenderPreviewBody.find("Localize(\"Video Background\")"), std::string::npos);
	EXPECT_EQ(RenderPreviewBody.find("Localize(\"Map Preview\")"), std::string::npos);
	EXPECT_EQ(RenderPreviewBody.find("FONT_ICON_PLAY"), std::string::npos);
	EXPECT_EQ(RenderPreviewBody.find("m_DrawEntityBgVideoFallback"), std::string::npos);
	EXPECT_EQ(RenderPreviewBody.find("m_DrawEntityBgFallback"), std::string::npos);
	EXPECT_EQ(Body.find("PreviewState.m_DrawEntityBgVideoFallback ="), std::string::npos);
	EXPECT_EQ(Body.find("PreviewState.m_DrawEntityBgFallback ="), std::string::npos);
	const size_t MapFallback = Source.find("auto RenderEntityBgFallback = ");
	ASSERT_NE(MapFallback, std::string::npos);
	const size_t VideoFallback = Source.find("auto RenderEntityBgVideoFallback = ", MapFallback);
	ASSERT_NE(VideoFallback, std::string::npos);
	const size_t RenderLoop = Source.find("s_WorkshopAssetsListBox.SkipItems", VideoFallback);
	ASSERT_NE(RenderLoop, std::string::npos);
	const std::string FallbackBody = Source.substr(MapFallback, RenderLoop - MapFallback);
	EXPECT_NE(FallbackBody.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_SETTINGS"), std::string::npos);
	EXPECT_EQ(FallbackBody.find("Ui()->DoLabel(&LabelRect"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardDrawLoopDoesNotRunPreviewOrTextLayout)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("RequestAssetsCardMetadataHydration"), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardMetadataCached"), std::string::npos);
	EXPECT_NE(Body.find("SettingsResourcePreviewDrawResult(ResourcePreviewState)"), std::string::npos);
	EXPECT_EQ(Body.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_EQ(Body.find("RefreshAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Body.find("PreviewState.m_DrawEntityTileArtifact ="), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardMetadataRenderingUsesBudgetedMenuText)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t MetadataStart = Source.find("auto RenderAssetsCardMetadata = ");
	ASSERT_NE(MetadataStart, std::string::npos);
	const size_t MetadataEnd = Source.find("auto RenderEntityBgFallback = ", MetadataStart);
	ASSERT_NE(MetadataEnd, std::string::npos);
	const std::string MetadataBody = Source.substr(MetadataStart, MetadataEnd - MetadataStart);

	EXPECT_NE(MetadataBody.find("DoMenuLabelStreamed(MENU_TEXT_SCOPE_SETTINGS"), std::string::npos);
	EXPECT_EQ(MetadataBody.find("Ui()->DoLabelStreamed("), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsLocalCardsUseSharedMetadataRenderer)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());
	const size_t LocalBranch = Body.find("if(!UsesCombinedAssetList(pCurrentCategory))");
	ASSERT_NE(LocalBranch, std::string::npos);
	const size_t LocalBranchEnd = Body.find("if(const SAssetResourceCategory *pCategory = AssetResourceCategoryByTab(s_CurCustomTab); UsesCombinedAssetList(pCategory)", LocalBranch);
	ASSERT_NE(LocalBranchEnd, std::string::npos);
	const std::string LocalBody = Body.substr(LocalBranch, LocalBranchEnd - LocalBranch);

	EXPECT_NE(LocalBody.find("RenderAssetsCardMetadataCached("), std::string::npos);
	EXPECT_NE(LocalBody.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(LocalBody.find("Ui()->DoLabel(&TitleRect"), std::string::npos);
	EXPECT_EQ(LocalBody.find("Ui()->DoLabel(&HeaderLayout.m_AuthorRect"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsMetadataCacheMissUsesVisibleFallbackOutsideShellOnlyFrame)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// The card shell can be frame-0 only, but the first readable non-shell frame
	// should still paint the current title/author/status instead of staying blank.
	EXPECT_NE(Body.find("RenderAssetsCardMetadataFallback(Ui(), Shell"), std::string::npos);
	EXPECT_NE(Body.find("else if(AssetsRenderCardMetadataFallback)"), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardMetadataCached(Shell, pMetadata, RenderAssetsCardMetadata);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SkinsAndTeeDoNotExposePartialPreviewUploads)
{
	const std::string Skins = ReadRepoFile("src/game/client/components/skins.cpp");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string DrainBody = ExtractSourceFunctionBody(Skins, "CSkins::ESkinProcessResult CSkins::DrainSettingsSkinPreviewUpload(CSkinContainer *pSkinContainer, CSkinLoadingStats &Stats,\n\tint &SkinsProcessedThisFrame, std::chrono::nanoseconds StartTime,\n\tstd::chrono::nanoseconds MaxTime)");
	ASSERT_FALSE(DrainBody.empty());

	EXPECT_NE(DrainBody.find("LoadSkinFinish(pSkinContainer"), std::string::npos);
	EXPECT_EQ(DrainBody.find("BeginSkinPreviewUpload(pSkinContainer"), std::string::npos);
	EXPECT_EQ(DrainBody.find("UploadNextSkinPreviewSprite(pSkinContainer, SkinPreviewUploadBudget)"), std::string::npos);
	EXPECT_EQ(DrainBody.find("FinishSkinPreviewUpload(pSkinContainer)"), std::string::npos);
	EXPECT_NE(DrainBody.find("SettingsResourcePreviewConsumeUploadBudget(SkinPreviewUploadBudget, SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)"), std::string::npos);
	EXPECT_NE(Settings.find("tee_preview_pipeline"), std::string::npos);
	EXPECT_NE(Settings.find("BeginSettingsUiFrameScheduler(EFrameSchedulerConsumer::SettingsText, \"tee\""), std::string::npos);
}

TEST(QmMonitoringHelpers, SkinsTeeUploadBudgetRequeuesInsteadOfFailing)
{
	const std::string Skins = ReadRepoFile("src/game/client/components/skins.cpp");
	const std::string DrainBody = ExtractSourceFunctionBody(Skins, "CSkins::ESkinProcessResult CSkins::DrainSettingsSkinPreviewUpload(CSkinContainer *pSkinContainer, CSkinLoadingStats &Stats,\n\tint &SkinsProcessedThisFrame, std::chrono::nanoseconds StartTime,\n\tstd::chrono::nanoseconds MaxTime)");
	ASSERT_FALSE(DrainBody.empty());

	// When the shared upload budget is exhausted, Tee/skin preview completion must
	// stay queued. Marking it failed here caused default yellow tees/question marks
	// to leak into the list during fast scrolling.
	const size_t BudgetCheck = DrainBody.find("!SettingsResourcePreviewConsumeUploadBudget(SkinPreviewUploadBudget, SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)");
	const size_t LoadFinish = DrainBody.find("LoadSkinFinish(pSkinContainer");
	ASSERT_NE(BudgetCheck, std::string::npos);
	ASSERT_NE(LoadFinish, std::string::npos);
	EXPECT_LT(BudgetCheck, LoadFinish);
	const std::string BudgetBlockedBody = DrainBody.substr(BudgetCheck, LoadFinish - BudgetCheck);
	EXPECT_NE(BudgetBlockedBody.find("return ESkinProcessResult::BREAK_GPU_LIMIT;"), std::string::npos);
	EXPECT_EQ(BudgetBlockedBody.find("SetState(CSkinContainer::EState::ERROR"), std::string::npos);
	EXPECT_EQ(BudgetBlockedBody.find("LoadSkinFinish("), std::string::npos);
}

TEST(QmMonitoringHelpers, SharedPreviewUploadSchedulerRejectsPartialCommit)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");
	const std::string DrainOneBody = ExtractSourceFunctionBody(Source, "bool CSettingsResourcePreviewUploadScheduler::DrainOne(SResourcePreviewUploadBudget &Budget, SResourcePreviewTelemetry &Telemetry, CSettingsResourcePreviewCache &Cache, IGraphics *pGraphics)");
	ASSERT_FALSE(DrainOneBody.empty());

	// Commit/finalize must be reserve -> valid texture -> commit. A failed or
	// budget-blocked upload must not consume budget or expose a half-ready texture.
	EXPECT_EQ(Header.find("m_pTargetTexture"), std::string::npos);
	const size_t ConsumePos = DrainOneBody.find("!SettingsResourcePreviewConsumeUploadBudget(Budget)");
	const size_t UploadPos = DrainOneBody.find("pGraphics->LoadTextureRawMove");
	const size_t ValidPos = DrainOneBody.find("if(Texture.IsValid())");
	const size_t CommitPos = DrainOneBody.find("SettingsResourcePreviewCommitUploadBudget(Budget)");
	const size_t FinalizeTruePos = DrainOneBody.find("Item.m_Finalize(true, Texture)");
	ASSERT_NE(ConsumePos, std::string::npos);
	ASSERT_NE(UploadPos, std::string::npos);
	ASSERT_NE(ValidPos, std::string::npos);
	ASSERT_NE(CommitPos, std::string::npos);
	ASSERT_NE(FinalizeTruePos, std::string::npos);
	EXPECT_LT(ConsumePos, UploadPos);
	EXPECT_LT(UploadPos, ValidPos);
	EXPECT_LT(ValidPos, CommitPos);
	EXPECT_LT(CommitPos, FinalizeTruePos);
	const std::string BudgetBlockedBody = DrainOneBody.substr(ConsumePos, UploadPos - ConsumePos);
	EXPECT_EQ(BudgetBlockedBody.find("Item.m_Finalize(false)"), std::string::npos);
	EXPECT_EQ(BudgetBlockedBody.find("Cache.MarkPreviewJobDone(Item.m_Key, false)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchFirstFrameShellOnly)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("m_TabSwitchShellOnlyFrame"), std::string::npos);
	EXPECT_NE(Body.find("AssetsTabSwitchFirstFrame"), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.m_TabSwitchShellOnlyFrame"), std::string::npos);
	EXPECT_NE(Body.find("PreviewPipelineScheduler.BeginFrame("), std::string::npos);
	EXPECT_NE(Body.find("PreviewPipelineScheduler.SetShellOnlyFrame(AssetsShellOnlyFrame)"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsPreviewArtifactTokensThisFrame = AssetsContentWarmupBlocked ? 0 : AdaptivePreviewArtifactTokens;"), std::string::npos);
	EXPECT_NE(Body.find("tab_switch_shell_only=%d"), std::string::npos);
	EXPECT_EQ(Body.find("if(AssetsTabSwitchFirstFrame)\n\t\t\t\t\t\t++ResourcePreviewTelemetry.m_PlaceholderCount;"), std::string::npos);
	EXPECT_EQ(Body.find("StartWorkshopThumb(Asset, SettingsWorkshopThumbShouldStartHighPriority"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchUsesShellFirstFrame)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string BeginFrameBody = ExtractSourceFunctionBody(Source, "static SSettingsAssetsCardHydrationScheduler BeginAssetsCardHydrationFrame");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(BeginFrameBody.empty());
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("constexpr int AssetsTabSwitchCooldownFrames = 8;"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("Scheduler.m_TabSwitchShellOnlyFrame = AssetsTabSwitchFirstFrame;"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("if(AssetsTabSwitchFirstFrame)"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("Scheduler.m_MetadataBudget = maximum(1, minimum(VisibleCardCount, MetadataLayoutTokens));"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("Scheduler.m_PreviewBudget = 0;"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("return Scheduler;"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("Scheduler.m_PreviewBudget = AssetsTabSwitchCooldownActive ? 0"), std::string::npos);
	EXPECT_NE(Body.find("LogAssetsFramePerfStage(\"assets_tab_switch_shell_first\""), std::string::npos);
	EXPECT_NE(Body.find("operation=settings_assets_tab_switch"), std::string::npos);
	EXPECT_NE(Body.find("tab_switch_shell_only=1"), std::string::npos);
	EXPECT_NE(Body.find("CardHydrationScheduler.m_TabSwitchShellOnlyFrame ? 1 : 0"), std::string::npos);
}

TEST(QmMonitoringHelpers, SkinsUseSharedPreviewUploadBudget)
{
	const std::string Skins = ReadRepoFile("src/game/client/components/skins.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/qmclient/settings_resource_preview.h");

	EXPECT_NE(Header.find("CSettingsResourcePreviewScheduler"), std::string::npos);
	EXPECT_NE(Header.find("CSettingsResourcePreviewUploadScheduler"), std::string::npos);
	EXPECT_NE(Skins.find("#include <game/client/components/qmclient/settings_resource_preview.h>"), std::string::npos);
	EXPECT_NE(Skins.find("SResourcePreviewUploadBudget SkinPreviewUploadBudget"), std::string::npos);
	const std::string DrainBody = ExtractSourceFunctionBody(Skins, "CSkins::ESkinProcessResult CSkins::DrainSettingsSkinPreviewUpload(CSkinContainer *pSkinContainer, CSkinLoadingStats &Stats,\n\tint &SkinsProcessedThisFrame, std::chrono::nanoseconds StartTime,\n\tstd::chrono::nanoseconds MaxTime)");
	ASSERT_FALSE(DrainBody.empty());
	EXPECT_NE(DrainBody.find("SkinPreviewUploadBudget.m_MaxUploads = GameClient()->GpuUploadLimiter()->RemainingUploads();"), std::string::npos);
	EXPECT_EQ(DrainBody.find("UploadNextSkinPreviewSprite(pSkinContainer, SkinPreviewUploadBudget)"), std::string::npos);
	EXPECT_NE(DrainBody.find("SettingsResourcePreviewCommitUploadBudget(SkinPreviewUploadBudget, SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)"), std::string::npos);
	EXPECT_NE(Skins.find("preview_uploads"), std::string::npos);
	EXPECT_NE(DrainBody.find("SettingsResourcePreviewConsumeUploadBudget(SkinPreviewUploadBudget, SETTINGS_SKIN_SOURCE_TEXTURE_UPLOADS)"), std::string::npos);
	EXPECT_NE(DrainBody.find("LoadSkinFinish(pSkinContainer"), std::string::npos);
	EXPECT_NE(Menus.find("SResourcePreviewTelemetry TeePreviewTelemetry"), std::string::npos);
	EXPECT_NE(Menus.find("SettingsResourcePreviewDrawResult(TeeResourcePreviewState)"), std::string::npos);
	EXPECT_NE(Menus.find("tee_preview_admissions=%d tee_ready_textures=%d tee_placeholders=%d"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsMetadataHydrationIsBudgetedAndTimed)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_NE(Source.find("ResourcePreviewTelemetry.m_MetadataHydrateMs += HydrateTimer.ElapsedMs();"), std::string::npos);
	EXPECT_NE(Source.find("static int DrainAssetsCardMetadataHydrationRequests"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsInitialMetadataLayoutTokens = maximum(1, minimum(AdaptiveBudget.m_VisibleTokens, 4));"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsMetadataLayoutTokensThisFrame = AssetsShellOnlyFrame ? AssetsInitialMetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Body.find("DrainAssetsCardMetadataHydrationRequests(AssetsMetadataLayoutTokensThisFrame"), std::string::npos);
	EXPECT_NE(Body.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(Body.find("HydrateAssetsCardMetadataTimed("), std::string::npos);
	EXPECT_EQ(Body.find("HydrateAssetsCardMetadata(\n"), std::string::npos);
}

TEST(QmMonitoringHelpers, TextGlyphContainerTelemetryExists)
{
	const std::string Text = ReadRepoFile("src/engine/client/text.cpp");
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	EXPECT_NE(Text.find("perf/text"), std::string::npos);
	EXPECT_NE(Text.find("event=text_runtime_budget"), std::string::npos);
	EXPECT_NE(Text.find("glyph_new=%d glyph_uploads=%d glyph_rasterize_ms=%.3f glyph_upload_ms=%.3f text_container_new=%d text_container_uploads=%d text_container_create_ms=%.3f text_container_upload_ms=%.3f"), std::string::npos);
	EXPECT_NE(Text.find("m_QmPerfGlyphNew"), std::string::npos);
	EXPECT_NE(Text.find("m_QmPerfTextContainerUploads"), std::string::npos);
	EXPECT_NE(Stats.find("export interface TextRuntimeBudgetSummary"), std::string::npos);
	EXPECT_NE(Stats.find("textRuntimeBudgetSummary(entries"), std::string::npos);
	EXPECT_NE(Report.find("Text Pipeline"), std::string::npos);
	EXPECT_NE(Tests.find("testTextRuntimeBudgetSummary"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfAnalyzerReportsOnePctLowAndPreviewBudget)
{
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Quality = ReadRepoFile("qmclient_scripts/perf/lib/quality.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	EXPECT_NE(Stats.find("export interface PreviewBudgetSummary"), std::string::npos);
	EXPECT_NE(Stats.find("previewBudgetSummary(entries"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Stats, {
					       "preview_jobs_started",
					       "preview_jobs_done",
					       "preview_uploads",
					       "preview_admissions",
					       "visible_ready_ratio",
					       "fpsOnePctLowAvailable",
					       "coldTabSwitchFpsSummaries",
					       "warmTabSwitchFpsSummaries",
				       }));
	EXPECT_TRUE(ContainsAll(Report, {"Cold/Warm Tab Switch", "Preview Budget", "1% Low Target"}));
	EXPECT_NE(Quality.find("previewBudget"), std::string::npos);
	EXPECT_NE(Tests.find("testPreviewBudgetSummaryAndColdWarmTabSwitches"), std::string::npos);
}

TEST(QmMonitoringHelpers, PerfAnalyzerCorrelatesOnePctLowWithTextAndResourceBudgets)
{
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Quality = ReadRepoFile("qmclient_scripts/perf/lib/quality.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	EXPECT_NE(Stats.find("export interface BudgetCorrelationWindow"), std::string::npos);
	EXPECT_NE(Stats.find("export function budgetCorrelationSummary(entries: PerfEntry[]): BudgetCorrelationSummary"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Stats, {"dominantAttribution", "culpritRank"}));
	EXPECT_NE(Quality.find("budgetCorrelation: budgetCorrelationSummary(entries)"), std::string::npos);
	EXPECT_TRUE(ContainsAll(Report, {"Budget Attribution by Window", "Top Culprit", "Text Pipeline", "Preview Budget", "UI Frame Scheduler", "1% Low Target"}));
	EXPECT_TRUE(ContainsAll(Tests, {
					       "testUnifiedFrameSchedulerAndTextPipelineBudgetSummary",
					       "testPerfAnalyzerCorrelatesOnePctLowWithTextAndResourceBudgets",
					       "testBudgetCorrelationSummaryByFpsWindow",
					       "testBudgetCorrelationRanksCulprits",
				       }));
}

TEST(QmMonitoringHelpers, PerfReportDefaultsToStatisticalSections)
{
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	// The perf report should default to statistical summaries and sampled charts
	// instead of dumping the full raw log stream.
	EXPECT_NE(Report.find("Assets Draw Distribution"), std::string::npos);
	EXPECT_NE(Report.find("Text Pipeline"), std::string::npos);
	EXPECT_NE(Report.find("Budget Attribution by Window"), std::string::npos);
	EXPECT_NE(Report.find("statisticalSummary("), std::string::npos);
	EXPECT_NE(Report.find("sampleArrayEvenly("), std::string::npos);
	EXPECT_NE(Tests.find("testReportUsesStatisticalBudgetReportInsteadOfRawBudgetDump"), std::string::npos);
	EXPECT_NE(Tests.find("testReportSamplesLargeEmbeddedChartData"), std::string::npos);
	EXPECT_EQ(Report.find("raw event stream"), std::string::npos);
}

TEST(QmMonitoringHelpers, LowFpsWindowRequiresRealSampledAndTopCulprit)
{
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Quality = ReadRepoFile("qmclient_scripts/perf/lib/quality.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	// A 240Hz gate cannot be satisfied with P99-derived placeholders or an
	// unattributed low-fps window.
	EXPECT_NE(Report.find("Top Culprit"), std::string::npos);
	EXPECT_NE(Report.find("unattributed_spike"), std::string::npos);
	EXPECT_NE(Quality.find("fps_1pct_low_missing_real_sampled"), std::string::npos);
	EXPECT_NE(Quality.find("unattributed_spike"), std::string::npos);
	EXPECT_NE(Quality.find("quality.failed ? 'FAIL'"), std::string::npos);
	EXPECT_NE(Tests.find("testPerfAnalyzerFailsUnattributedLowFpsWindow"), std::string::npos);
	EXPECT_NE(Tests.find("testMissingOnePctLowIsMarkedP99DerivedAndNotTargetPass"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardShellUsesCompactCurrentLabelBadges)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t LayoutStart = Source.find("auto LayoutAssetsCardShell = ");
	ASSERT_NE(LayoutStart, std::string::npos);
	const size_t LayoutEnd = Source.find("auto ComputeAssetPreviewContentSize = ", LayoutStart);
	ASSERT_NE(LayoutEnd, std::string::npos);
	const std::string LayoutBody = Source.substr(LayoutStart, LayoutEnd - LayoutStart);

	// Small resource cards should size the right-side chip from the current
	// label with tight padding instead of reserving the old wide English badge
	// widths. The left title/id lane keeps the remaining width.
	EXPECT_NE(Source.find("AssetsCardStatusTagHorizontalPadding"), std::string::npos);
	EXPECT_NE(Source.find("AssetsCardStatusTagMaxWidth"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetCardHeaderMargin = 3.0f;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr float AssetCardHeaderControlMargin = 1.0f;"), std::string::npos);
	EXPECT_EQ(Source.find("auto LayoutAssetCardHeader = "), std::string::npos);
	EXPECT_EQ(Source.find("const float AssetsCardDownloadedTagWidth = TextRender()->TextWidth"), std::string::npos);
	EXPECT_EQ(Source.find("const float AssetsCardLocalOnlyBadgeWidth = TextRender()->TextWidth"), std::string::npos);
	EXPECT_NE(LayoutBody.find("auto ComputeBadgeWidth = [&](const char *pLabel)"), std::string::npos);
	EXPECT_NE(LayoutBody.find("TextRender()->TextWidth(AssetsCardStatusTagFontSize, pLabel"), std::string::npos);
	EXPECT_NE(LayoutBody.find("const float TitleMinWidth = 12.0f;"), std::string::npos);
	EXPECT_NE(LayoutBody.find("ReserveTrailingRect(TitleRect, ComputeBadgeWidth(pStatusLabel)"), std::string::npos);
	EXPECT_NE(LayoutBody.find("ReserveTrailingRect(TitleRect, ComputeBadgeWidth(Localize(\"Local-only\"))"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardMetadataFallbackUsesStableTextGeometry)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string FallbackBody = ExtractSourceFunctionBody(Source, "static void RenderAssetsCardMetadataFallback");
	const std::string MetadataBody = ExtractSourceFunctionBody(Source, "auto RenderAssetsCardMetadata");
	ASSERT_FALSE(FallbackBody.empty());
	ASSERT_FALSE(MetadataBody.empty());

	// Cache-miss frames must look like cache-hit frames. Larger fallback text
	// caused title/status overlap and visible size changes while metadata hydrated.
	EXPECT_NE(FallbackBody.find("AssetsCardTitleFontSize"), std::string::npos);
	EXPECT_NE(FallbackBody.find("AssetsCardAuthorFontSize"), std::string::npos);
	EXPECT_NE(MetadataBody.find("AssetsCardTitleFontSize"), std::string::npos);
	EXPECT_NE(MetadataBody.find("AssetsCardAuthorFontSize"), std::string::npos);
	EXPECT_NE(FallbackBody.find("SLabelProperties TitleProps"), std::string::npos);
	EXPECT_NE(FallbackBody.find("TitleProps.m_EllipsisAtEnd = true;"), std::string::npos);
	EXPECT_NE(FallbackBody.find("AuthorProps.m_EllipsisAtEnd = true;"), std::string::npos);
	EXPECT_EQ(FallbackBody.find("Shell.m_ActionButtonRect : Shell.m_TitleRect"), std::string::npos);
	EXPECT_NE(FallbackBody.find("Shell.m_StatusTagRect"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardMetadataFallbackRendersStatusTags)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string FallbackBody = ExtractSourceFunctionBody(Source, "static void RenderAssetsCardMetadataFallback");
	const std::string MetadataBody = ExtractSourceFunctionBody(Source, "auto RenderAssetsCardMetadata");
	ASSERT_FALSE(FallbackBody.empty());
	ASSERT_FALSE(MetadataBody.empty());

	// Status tags are part of the fixed card shell. A cache miss must not hide
	// Local/Network/Downloaded badges while metadata hydration catches up.
	EXPECT_NE(FallbackBody.find("Shell.m_HasStatusTag"), std::string::npos);
	EXPECT_NE(FallbackBody.find("Shell.m_StatusTagRect"), std::string::npos);
	EXPECT_NE(FallbackBody.find("pStatusLabel"), std::string::npos);
	EXPECT_NE(FallbackBody.find("pUi->DoLabel(&StatusRect"), std::string::npos);
	EXPECT_NE(MetadataBody.find("Metadata.m_StatusLabel.c_str()"), std::string::npos);
	EXPECT_EQ(FallbackBody.find("(void)pStatusLabel"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsScrollRendersCachedMetadata)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Scroll pressure blocks preview/upload work, not visible metadata drawing.
	// A ready cache entry is cheap O(visible) drawing and must stay visible.
	EXPECT_NE(Body.find("const bool AssetsRenderCardMetadataFallback = !AssetsShellOnlyFrame;"), std::string::npos);
	EXPECT_NE(Body.find("if(pMetadata != nullptr)\n\t\t\t\tRenderAssetsCardMetadataCached"), std::string::npos);
	EXPECT_NE(Body.find("if(pMetadata != nullptr)\n\t\t\t\t\t\tRenderAssetsCardMetadataCached"), std::string::npos);
	EXPECT_EQ(Body.find("if(pMetadata != nullptr && !AssetsContentWarmupBlocked)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsScrollRendersCachedMetadataWithoutStreamedContainerWait)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Holding the scrollbar disables text-container creation. Cached metadata
	// must still be readable by drawing the cached strings through fixed-geometry
	// fallback instead of waiting for DoMenuLabelStreamed containers to hydrate.
	EXPECT_NE(Body.find("pMetadata != nullptr && AssetsContentWarmupBlocked"), std::string::npos);
	EXPECT_NE(Body.find("pMetadata->m_Title.c_str()"), std::string::npos);
	EXPECT_NE(Body.find("pMetadata->m_Author.c_str()"), std::string::npos);
	EXPECT_NE(Body.find("pMetadata->m_StatusLabel.c_str()"), std::string::npos);
	EXPECT_LT(Body.find("pMetadata != nullptr && AssetsContentWarmupBlocked"), Body.find("RenderAssetsCardMetadataCached(Shell, pMetadata, RenderAssetsCardMetadata);"));
}

TEST(QmMonitoringHelpers, AssetsScrollMissStillShowsImmediateMetadataFallback)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	// Fast scrollbar drags block hydration work, but visible card titles must
	// remain readable even when the metadata cache missed for a newly exposed
	// range. Missing metadata may use immediate fixed-geometry fallback; it must
	// not be hidden behind AssetsRenderCardMetadataFallback.
	EXPECT_NE(Body.find("const bool AssetsRenderCardMetadataFallback = !AssetsShellOnlyFrame;"), std::string::npos);
	EXPECT_EQ(Body.find("const bool AssetsRenderCardMetadataFallback = !AssetsContentWarmupBlocked;"), std::string::npos);
	EXPECT_NE(Body.find("else if(AssetsRenderCardMetadataFallback)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsFirstVisibleFrameHasMetadataWarmupBudget)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string BeginFrameBody = ExtractSourceFunctionBody(Source, "static SSettingsAssetsCardHydrationScheduler BeginAssetsCardHydrationFrame");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(BeginFrameBody.empty());
	ASSERT_FALSE(Body.empty());

	// First visible frame should have ready metadata for the first screen, not a
	// long shell-only phase. Preview/upload can stay blocked; metadata gets a
	// small first-frame budget.
	EXPECT_NE(BeginFrameBody.find("if(AssetsTabSwitchFirstFrame)"), std::string::npos);
	EXPECT_NE(BeginFrameBody.find("Scheduler.m_MetadataBudget = maximum(1, minimum(VisibleCardCount, MetadataLayoutTokens));"), std::string::npos);
	EXPECT_NE(Body.find("const int AssetsMetadataLayoutTokensThisFrame = AssetsShellOnlyFrame ? AssetsInitialMetadataLayoutTokens"), std::string::npos);
	EXPECT_NE(Body.find("DrainAssetsCardMetadataHydrationRequests(AssetsMetadataLayoutTokensThisFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabSwitchResetsListScrollToTop)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("s_AssetsResetListScrollOnTabSwitch = true;"), std::string::npos);
	EXPECT_NE(Body.find("if(s_AssetsResetListScrollOnTabSwitch)\n\t\t{\n\t\t\ts_ListBox.ResetScroll();"), std::string::npos);
	EXPECT_NE(Body.find("if(s_AssetsResetListScrollOnTabSwitch)\n\t\t\t{\n\t\t\t\ts_WorkshopAssetsListBox.ResetScroll();"), std::string::npos);
	EXPECT_LT(Body.find("s_AssetsResetListScrollOnTabSwitch = true;"), Body.find("s_WorkshopAssetsListBox.DoStart("));
}

TEST(QmMonitoringHelpers, AssetsStatusLabelsDistinguishLocalNetworkWorkshopDownloaded)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("auto ResolveLocalAssetStatusLabel = "), std::string::npos);
	EXPECT_NE(Source.find("pWorkshopAsset != nullptr ? Localize(\"Downloaded\") : Localize(\"Local\")"), std::string::npos);
	EXPECT_NE(Body.find("ResolveLocalAssetStatusLabel(pLocalItem, ShowLocalOnlyBadge)"), std::string::npos);
	EXPECT_NE(Body.find("ResolveLocalAssetStatusLabel(pItem, ShowLocalOnlyBadge)"), std::string::npos);
	EXPECT_NE(Body.find("Localize(Asset.m_Installed ? \"Downloaded\" : \"Network\")"), std::string::npos);
	EXPECT_EQ(Body.find("Asset.m_Installed ? \"Downloaded\" : \"Not downloaded\""), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsStatusTagKeepsMinimumReadableWidth)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t LayoutStart = Source.find("auto LayoutAssetsCardShell = ");
	ASSERT_NE(LayoutStart, std::string::npos);
	const size_t LayoutEnd = Source.find("auto ComputeAssetPreviewContentSize = ", LayoutStart);
	ASSERT_NE(LayoutEnd, std::string::npos);
	const std::string LayoutBody = Source.substr(LayoutStart, LayoutEnd - LayoutStart);

	// Narrow cards must not squeeze the status tag into the delete icon area.
	// If the tag cannot keep a readable minimum width, it must not reserve a
	// header slot.
	EXPECT_NE(LayoutBody.find("AssetsCardStatusTagMinWidth"), std::string::npos);
	EXPECT_NE(LayoutBody.find("if(MaxWidth < DesiredMinWidth)"), std::string::npos);
	EXPECT_NE(LayoutBody.find("DesiredMinWidth"), std::string::npos);
	EXPECT_NE(LayoutBody.find("ReserveTrailingRect(TitleRect, ComputeBadgeWidth(pStatusLabel), AssetsCardStatusTagMinWidth"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsStatusTagColorsDistinguishReadyFromNetwork)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t RenderStart = Source.find("auto RenderAssetStatusTag = ");
	ASSERT_NE(RenderStart, std::string::npos);
	const size_t RenderEnd = Source.find("auto ComputePreviewDrawRect = ", RenderStart);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderStart, RenderEnd - RenderStart);

	// Ready-to-use local/downloaded assets stay green. Network-only assets use
	// the neutral label color so the availability state remains visible.
	EXPECT_NE(RenderBody.find("AssetsCardStatusReadyColor"), std::string::npos);
	EXPECT_NE(RenderBody.find("AssetsCardStatusNetworkColor"), std::string::npos);
	EXPECT_NE(RenderBody.find("Positive ? AssetsCardStatusReadyColor : AssetsCardStatusNetworkColor"), std::string::npos);
	EXPECT_NE(Source.find("RenderAssetsCardMetadataFallback(Ui(), Shell, pMetadata->m_Title.c_str(), pMetadata->m_Author.c_str(), pMetadata->m_StatusLabel.c_str(), pMetadata->m_Installed || pMetadata->m_LocalOnly"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsStatusTagsUseTightHorizontalPadding)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string FallbackBody = ExtractSourceFunctionBody(Source, "static void RenderAssetsCardMetadataFallback");
	const size_t RenderStart = Source.find("auto RenderAssetStatusTag = ");
	ASSERT_FALSE(FallbackBody.empty());
	ASSERT_NE(RenderStart, std::string::npos);
	const size_t RenderEnd = Source.find("auto ComputePreviewDrawRect = ", RenderStart);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderStart, RenderEnd - RenderStart);

	EXPECT_NE(Source.find("AssetsCardStatusTagHorizontalPadding"), std::string::npos);
	EXPECT_NE(FallbackBody.find("StatusLabelProps.m_MaxWidth = static_cast<int>(StatusRect.w - AssetsCardStatusTagHorizontalPadding * 2.0f);"), std::string::npos);
	EXPECT_NE(RenderBody.find("StatusLabelProps.m_MaxWidth = static_cast<int>(StatusRect.w - AssetsCardStatusTagHorizontalPadding * 2.0f);"), std::string::npos);
	EXPECT_EQ(FallbackBody.find("StatusRect.w - 2.0f"), std::string::npos);
	EXPECT_EQ(RenderBody.find("StatusRect.w - 2.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEntityBgFallbackTextIsCenteredTodo)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const size_t FallbackStart = Source.find("auto RenderEntityBgFallback = ");
	ASSERT_NE(FallbackStart, std::string::npos);
	const size_t FallbackEnd = Source.find("auto RenderEntityBgVideoFallback = ", FallbackStart);
	ASSERT_NE(FallbackEnd, std::string::npos);
	const std::string FallbackBody = Source.substr(FallbackStart, FallbackEnd - FallbackStart);

	EXPECT_NE(FallbackBody.find("Localize(\"Map Preview TODO\")"), std::string::npos);
	EXPECT_NE(FallbackBody.find("TEXTALIGN_MC"), std::string::npos);
	EXPECT_NE(FallbackBody.find("LabelRect = FallbackRect;"), std::string::npos);
	EXPECT_EQ(FallbackBody.find("Localize(\"Map Preview\")"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsPreviewFinalizeBudgetsAreNotHardCappedToOnePerFrame)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");

	// Ready previews/thumbs should appear as soon as each one finishes; the
	// pipeline must not serialize visible finalization to 1 item per frame.
	// Raising the steady-state budget is fine, but scroll-active frames still
	// keep a hard visible-stage cap through SettingsResourceFrameStageBudget(..., 0).
	EXPECT_NE(Source.find("constexpr int MaxPreviewDecodeFinalizesPerFrame = 16;"), std::string::npos);
	EXPECT_NE(Source.find("constexpr int MaxWorkshopThumbDecodeFinalizesPerFrame = 16;"), std::string::npos);
	EXPECT_EQ(Source.find("constexpr int MaxPreviewDecodeFinalizesPerFrame = 1;"), std::string::npos);
	EXPECT_EQ(Source.find("constexpr int MaxWorkshopThumbDecodeFinalizesPerFrame = 1;"), std::string::npos);
	EXPECT_NE(Source.find("SettingsResourceFrameStageBudget(FinalizeFrameContext, FinalizePriority, MaxPreviewDecodeFinalizesPerFrame, 0)"), std::string::npos);
	EXPECT_NE(Source.find("SettingsResourceFrameStageBudget(FinalizeFrameContext, FinalizePriority, MaxWorkshopThumbDecodeFinalizesThisFrame, 0)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardMetadataDoesNotHydrateWithoutBudget)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("FindAssetsCardMetadata("), std::string::npos);
	EXPECT_NE(Source.find("HydrateAssetsCardMetadata("), std::string::npos);
	EXPECT_EQ(Source.find("RefreshAssetsCardMetadata("), std::string::npos);
	EXPECT_EQ(Body.find("if(pMetadata == nullptr && CardHydrationScheduler.CanHydrateMetadata(CombinedVisible))"), std::string::npos);
	EXPECT_NE(Body.find("RequestAssetsCardMetadataHydration("), std::string::npos);
	EXPECT_EQ(Body.find("if(pMetadata == nullptr && RenderMetadata)"), std::string::npos);
	EXPECT_EQ(Body.find("if(pMetadata != nullptr && RenderMetadata)"), std::string::npos);
	EXPECT_EQ(Body.find("GetOrHydrateAssetsCardMetadata("), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoLabel(&ErrorRect, Localize(\"Download failed\"), 9.0f, TEXTALIGN_MC);"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsCardInitialEntryUsesStableShellGeometry)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("AssetsCardShellShowsAuthorRow("), std::string::npos);
	EXPECT_NE(Source.find("AssetsCardListAreaWithStableScrollbar("), std::string::npos);
	EXPECT_NE(Source.find("RenderAssetsCardLoadingShells("), std::string::npos);
	EXPECT_NE(Body.find("const bool ShowAuthorRow = AssetsCardShellShowsAuthorRow(s_CurCustomTab, true"), std::string::npos);
	EXPECT_NE(Body.find("const bool ShowAuthorRow = AssetsCardShellShowsAuthorRow(s_CurCustomTab, false"), std::string::npos);
	EXPECT_NE(Body.find("AssetsCardListAreaWithStableScrollbar(WorkshopListArea, s_WorkshopAssetsListBox.ScrollbarWidthMax(), s_WorkshopAssetsListBox.ScrollbarMargin())"), std::string::npos);
	EXPECT_NE(Body.find("s_WorkshopAssetsListBox.DoStart(WorkshopRowHeight, CombinedCount, Columns, 1, OldCombinedSelected, &WorkshopListArea, false, IGraphics::CORNER_ALL);"), std::string::npos);
	EXPECT_NE(Body.find("RenderAssetsCardLoadingShells("), std::string::npos);
	EXPECT_NE(Body.find("const bool AssetsInitialEntryLoading = m_aAssetLoadStates[s_CurCustomTab] == ASSET_LOAD_STATE_LOADING || WorkshopState.m_pListTask != nullptr;"), std::string::npos);
	EXPECT_NE(Body.find("stage=assets_card_geometry"), std::string::npos);
	EXPECT_NE(Body.find("geometry_changed=%d"), std::string::npos);
	EXPECT_LT(Body.find("if(AssetsInitialEntryLoading)"), Body.find("Ui()->DoLabel(&WorkshopListArea, Localize(\"No assets\"), 12.0f, TEXTALIGN_MC);"));
	EXPECT_EQ(Source.find("ShouldShowAssetCardAuthorRow(!Asset.m_Author.empty(), false)"), std::string::npos);
	EXPECT_EQ(Source.find("ShowAuthorRow = !Asset.m_Author.empty()"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoBrowserUsesAdaptiveMetadataBudget)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderDemoBrowserList(CUIRect ListView, bool &WasListboxItemActivated)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Header.find("SSettingsAdaptiveBudgetState m_DemoBrowserAdaptiveBudgetState"), std::string::npos);
	EXPECT_NE(Body.find("BeginSettingsUiFrameScheduler(EFrameSchedulerConsumer::DemoBrowser, \"demo_browser\""), std::string::npos);
	EXPECT_NE(Body.find("AdaptiveBudget.m_DemoMetadataTokens"), std::string::npos);
	EXPECT_NE(ReadRepoFile("src/game/client/components/menus.cpp").find("event=settings_adaptive_budget"), std::string::npos);
	EXPECT_EQ(Body.find("AdvanceDemoBrowserMetadata(g_Config.m_BrDemoFetchInfo && !BrowsingScreenshots ? 2 : 0, g_Config.m_BrDemoSort == SORT_DATE ? 4 : 0"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsTextPrebuildUsesAdaptiveBudget)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string PrebuildBody = ExtractSourceFunctionBody(Menus, "int CMenus::PrebuildSettingsTextPoolForLoading(int Budget, const char *pOperationOverride)");
	const std::string EscBody = ExtractSourceFunctionBody(Menus, "void CMenus::PrebuildIngameEscTextPoolBeforeOpen(int Budget)");
	ASSERT_FALSE(PrebuildBody.empty());
	ASSERT_FALSE(EscBody.empty());

	EXPECT_EQ(Header.find("SSettingsAdaptiveBudgetState m_SettingsTextAdaptiveBudgetState"), std::string::npos);
	EXPECT_NE(PrebuildBody.find("BeginSettingsUiFrameScheduler(EFrameSchedulerConsumer::SettingsText, \"stable_text_prebuild\""), std::string::npos);
	EXPECT_NE(PrebuildBody.find("AdaptiveBudget.m_TextPrebuildTokens"), std::string::npos);
	EXPECT_NE(Menus.find("event=settings_adaptive_budget"), std::string::npos);
	EXPECT_NE(EscBody.find("BeginSettingsUiFrameScheduler(EFrameSchedulerConsumer::SettingsText, \"stable_text_ingame_esc\""), std::string::npos);
	EXPECT_EQ(EscBody.find("PrebuildSettingsMenuTextPool(minimum(Budget, 4), \"target_settings\", \"ingame_esc_open\");"), std::string::npos);
}

TEST(QmMonitoringHelpers, ReportStableTextSamplesDoNotWrapKeyVertically)
{
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Tests = ReadRepoFile("qmclient_scripts/perf/test.ts");

	EXPECT_NE(Report.find("sample-key-cell"), std::string::npos);
	EXPECT_NE(Report.find("white-space:nowrap"), std::string::npos);
	EXPECT_NE(Report.find("table-layout:fixed"), std::string::npos);
	EXPECT_NE(Report.find("truncateMiddle(sampleField(sample, 'key')"), std::string::npos);
	EXPECT_NE(Tests.find("must-not-wrap-vertically"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsStableTextTargetAcceptanceRequiresFullCoverage)
{
	const std::string Stats = ReadRepoFile("qmclient_scripts/perf/lib/stats.ts");
	const std::string Report = ReadRepoFile("qmclient_scripts/perf/lib/report.ts");
	const std::string Quality = ReadRepoFile("qmclient_scripts/perf/lib/quality.ts");

	EXPECT_NE(Stats.find("acceptanceBlocked: missCount > 0 || staleCount > 0 || prebuildRemainingBeforeTarget > 0 || !planCollectionAvailable || !planCollectionComplete || !utilizationAvailable || !planCoverageAvailable || unplannedVisibleCount > 0 || keyMismatchCount > 0 || textNew > 0"), std::string::npos);
	EXPECT_NE(Stats.find("unplanned visible stable text candidates"), std::string::npos);
	EXPECT_NE(Stats.find("stable text key mismatches"), std::string::npos);
	EXPECT_NE(Stats.find("settings_text_plan_collection"), std::string::npos);
	EXPECT_NE(Stats.find("planCollectionRemainingBeforeTarget"), std::string::npos);
	EXPECT_NE(Stats.find("!planCollectionAvailable || !planCollectionComplete"), std::string::npos);
	EXPECT_NE(Report.find("collection remaining=0 只表示计划收集完成"), std::string::npos);
	EXPECT_NE(Report.find("Plan Collection"), std::string::npos);
	EXPECT_NE(Report.find("Container Remaining"), std::string::npos);
	EXPECT_NE(Report.find("Visible Coverage"), std::string::npos);
	EXPECT_NE(Report.find("Assets Visible-First Admission"), std::string::npos);
	EXPECT_NE(Stats.find("assetsVisibleReadySummary"), std::string::npos);
	EXPECT_NE(Stats.find("thumbStartsDuringDraw > 0"), std::string::npos);
	EXPECT_NE(Stats.find("geometryStable === false"), std::string::npos);
	EXPECT_NE(Report.find("Visible Ready"), std::string::npos);
	EXPECT_NE(Quality.find("stable text coverage blocked settings acceptance"), std::string::npos);
	EXPECT_NE(Quality.find("assets visible-ready preflight missing"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsTextMissLogsAreSampledPerFrameBucket)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Menus, "void LogSettingsTextPoolCoverageGap(IClient *pClient, const char *pEvent, CMenus::EMenuTextScope Scope, const char *pScopeName, int Page, int Tab, int Subtab, const char *pKey, const char *pReason, const char *pPlanStatus, const char *pOperation, uint64_t Frame)");
	ASSERT_FALSE(Body.empty());

	// This guard keeps target coverage useful without returning to multi-GB
	// logs when thousands of visible labels share the same key-mismatch cause.
	EXPECT_NE(Body.find("MaxGapSamplesPerFrameBucket"), std::string::npos);
	EXPECT_NE(Body.find("s_SamplesThisBucket"), std::string::npos);
	EXPECT_NE(Body.find("log_sample_limit"), std::string::npos);
}

TEST(QmMonitoringHelpers, DefaultGateRunsFullAutomatedTests)
{
	const std::string Gate = ReadRepoFile("qmclient_scripts/gate/check_gate.py");
	const std::string Verification = ReadRepoFile("docs/ai-workflow/verification.md");
	const std::string ScriptsOverview = ReadRepoFile("qmclient_scripts/scripts_overview.md");
	ASSERT_FALSE(Gate.empty());

	const size_t DefaultMode = Gate.find("\"default\": {");
	ASSERT_NE(DefaultMode, std::string::npos);
	const size_t FullMode = Gate.find("\"full\": {", DefaultMode);
	ASSERT_NE(FullMode, std::string::npos);
	const std::string DefaultSpec = Gate.substr(DefaultMode, FullMode - DefaultMode);

	// The default gate is the normal pre-submit gate, so it must run the full
	// automated test set. Full mode is reserved for extra heavyweight/noisy
	// checks, not for merely getting Rust tests.
	EXPECT_NE(DefaultSpec.find("\"tests\": {\"cxx\": True, \"rust\": True, \"all\": False}"), std::string::npos);
	EXPECT_EQ(DefaultSpec.find("\"strict_build\""), std::string::npos);
	EXPECT_EQ(DefaultSpec.find("\"dilate\""), std::string::npos);
	EXPECT_NE(DefaultSpec.find("C++ 全量测试和 Rust 全量测试"), std::string::npos);
	EXPECT_NE(Gate.substr(FullMode).find("\"strict_build\""), std::string::npos);
	EXPECT_NE(Gate.substr(FullMode).find("\"dilate\""), std::string::npos);
	EXPECT_NE(Verification.find("C++ 全量测试和 Rust 全量测试"), std::string::npos);
	EXPECT_NE(Verification.find("严格构建与静态分析只属于 full gate"), std::string::npos);
	EXPECT_NE(ScriptsOverview.find("C++ 全量测试和 Rust 全量测试"), std::string::npos);
	EXPECT_NE(ScriptsOverview.find("严格构建与静态分析只属于 full gate"), std::string::npos);
	EXPECT_NE(ScriptsOverview.find("不作为“全量测试”的默认入口"), std::string::npos);
}

TEST(QmMonitoringHelpers, WindowsCmakeWrapperDoesNotPreconfigureBeforeBuild)
{
	const std::string Wrapper = ReadRepoFile("qmclient_scripts/cmake-windows.cmd");
	const std::string Repair = ReadRepoFile("qmclient_scripts/repair_ninja_msvc_prefix.py");
	ASSERT_FALSE(Wrapper.empty());
	ASSERT_FALSE(Repair.empty());

	EXPECT_EQ(Wrapper.find("--prepare-build"), std::string::npos);
	EXPECT_NE(Wrapper.find("if not \"%CMRC%\"==\"0\""), std::string::npos);
	EXPECT_NE(Wrapper.find("type \"%CMOUT%\""), std::string::npos);
	EXPECT_FALSE(ContainsAny(Repair, {
						 "--prepare-build",
						 "def _prepare_build_rules",
						 "subprocess.run(\n        [\"cmake\", \"-S\"",
					 }));
}

TEST(QmMonitoringHelpers, NightlyWorkflowPublishesPdbFreePrerelease)
{
	const std::string Nightly = ReadRepoFile(".github/workflows/nightly.yml");
	ASSERT_FALSE(Nightly.empty());

	EXPECT_NE(Nightly.find("workflow_dispatch:"), std::string::npos);
	EXPECT_NE(Nightly.find("schedule:"), std::string::npos);
	EXPECT_NE(Nightly.find("Check Nightly Changes"), std::string::npos);
	EXPECT_NE(Nightly.find("git ls-remote --tags origin refs/tags/nightly"), std::string::npos);
	EXPECT_NE(Nightly.find("should_build=false"), std::string::npos);
	EXPECT_NE(Nightly.find("github.event_name"), std::string::npos);
	EXPECT_NE(Nightly.find("needs.changes.outputs.should_build == 'true'"), std::string::npos);
	EXPECT_NE(Nightly.find("cmake --build build --target package_default --parallel"), std::string::npos);
	EXPECT_NE(Nightly.find("QmClient-windows.zip"), std::string::npos);
	EXPECT_NE(Nightly.find("QmClient-windows.7z"), std::string::npos);
	EXPECT_NE(Nightly.find("QmClient-ubuntu.tar.xz"), std::string::npos);
	EXPECT_NE(Nightly.find("QmClient-macOS.dmg"), std::string::npos);
	EXPECT_NE(Nightly.find("PDB files must not be published"), std::string::npos);
	EXPECT_NE(Nightly.find("\\.pdb$"), std::string::npos);
	EXPECT_NE(Nightly.find("gh release delete nightly --cleanup-tag --yes || true"), std::string::npos);
	EXPECT_NE(Nightly.find("gh release create nightly"), std::string::npos);
	EXPECT_NE(Nightly.find("--prerelease"), std::string::npos);
	EXPECT_NE(Nightly.find("contents: write"), std::string::npos);
	EXPECT_EQ(Nightly.find("startsWith(github.ref, 'refs/tags/')"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsScrollRegionHelperExists)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string ScrollRegionHeader = ReadRepoFile("src/game/client/ui_scrollregion.h");
	const std::string ScrollRegion = ReadRepoFile("src/game/client/ui_scrollregion.cpp");
	const std::string UiHeader = ReadRepoFile("src/game/client/ui.h");

	EXPECT_NE(Header.find("struct SSettingsScrollRegionFrame"), std::string::npos);
	EXPECT_NE(Header.find("BeginSettingsScrollRegion(CScrollRegion &ScrollRegion"), std::string::npos);
	EXPECT_NE(Header.find("FinishSettingsScrollRegion(CScrollRegion &ScrollRegion"), std::string::npos);
	EXPECT_NE(Header.find("const CUIRect *pEndRect = nullptr"), std::string::npos);
	EXPECT_NE(Menus.find("CMenus::SSettingsScrollRegionFrame CMenus::BeginSettingsScrollRegion(CScrollRegion &ScrollRegion"), std::string::npos);
	EXPECT_NE(Menus.find("void CMenus::FinishSettingsScrollRegion(CScrollRegion &ScrollRegion"), std::string::npos);
	EXPECT_NE(Menus.find("if(pEndRect != nullptr)\n\t\tScrollRegion.AddRect(*pEndRect);"), std::string::npos);
	EXPECT_NE(Menus.find("Frame.m_FinalOffsetY = ScrollRegion.ScrollbarShown() ? ScrollRegion.ContentScrollOffsetY() : 0.0f;"), std::string::npos);
	EXPECT_NE(Menus.find("m_SettingsScrollActive = m_SettingsScrollActive ||"), std::string::npos);
	EXPECT_NE(Menus.find("m_SettingsRuntimeMetadata.m_LastScrollY = Frame.m_FinalOffsetY;"), std::string::npos);
	EXPECT_NE(ScrollRegionHeader.find("float ContentScrollOffsetY() const { return m_Params.m_ScrollHorizontal ? 0.0f : -m_ScrollState.Offset(); }"), std::string::npos);
	const std::string ScrollRegionEnd = ExtractSourceFunctionBody(ScrollRegion, "void CScrollRegion::End()");
	const std::string NoScrollBody = ExtractSourceFunctionBody(ScrollRegion, "void CScrollRegion::MaintainNoScrollSliderActive()");
	const std::string ScrollRegionSlider = ExtractSourceFunctionBody(ScrollRegion, "void CScrollRegion::DoSlider()");
	ASSERT_FALSE(ScrollRegionEnd.empty());
	ASSERT_FALSE(NoScrollBody.empty());
	ASSERT_FALSE(ScrollRegionSlider.empty());
	EXPECT_NE(ScrollRegion.find("bool CScrollRegion::ContentOverflows() const"), std::string::npos);
	EXPECT_NE(ScrollRegion.find("return !m_Params.m_HideScrollbar && ContentOverflows();"), std::string::npos);
	EXPECT_NE(ScrollRegion.find("m_ContentSize 来自上一帧 End/AddRect 的测量结果"), std::string::npos);
	EXPECT_NE(ScrollRegionSlider.find("const float ScrollMax = Metrics.MaxOffset();"), std::string::npos);
	EXPECT_NE(ScrollRegionSlider.find("const bool CanScroll = m_ContentSize > 0.0f && ScrollMax > 0.0f && RailSize > 0.0f;"), std::string::npos);
	EXPECT_NE(UiHeader.find("bool IsActiveItem(const void *pId) const"), std::string::npos);
	EXPECT_NE(ScrollRegionEnd.find("MaintainNoScrollSliderActive();"), std::string::npos);
	EXPECT_NE(NoScrollBody.find("m_ScrollState.ResetForNonScrollableContent(Active);"), std::string::npos);
	EXPECT_EQ(NoScrollBody.find("m_ScrollState.EndThumbDrag();"), std::string::npos);
	EXPECT_NE(NoScrollBody.find("Ui()->IsActiveItem(pId)"), std::string::npos);
	EXPECT_NE(NoScrollBody.find("ScrollRegionShouldKeepNoScrollSliderActive(WasActive, Ui()->MouseButton(0))"), std::string::npos);
	EXPECT_NE(NoScrollBody.find("Ui()->SetActiveItem(pId);"), std::string::npos);
	EXPECT_NE(NoScrollBody.find("Ui()->SetActiveItem(nullptr);"), std::string::npos);
	EXPECT_NE(ScrollRegionSlider.find("MaintainNoScrollSliderActive();"), std::string::npos);
}

TEST(QmMonitoringHelpers, ScrollRegionNoScrollSliderReleasesActiveAfterMouseUp)
{
	EXPECT_FALSE(ScrollRegionShouldKeepNoScrollSliderActive(false, false));
	EXPECT_FALSE(ScrollRegionShouldKeepNoScrollSliderActive(false, true));
	EXPECT_FALSE(ScrollRegionShouldKeepNoScrollSliderActive(true, false));
	EXPECT_TRUE(ScrollRegionShouldKeepNoScrollSliderActive(true, true));
}

TEST(QmMonitoringHelpers, SettingsScrollRegionPagesUseUnifiedHelper)
{
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmScroll = ReadRepoFile("src/game/client/QmUi/QmScroll.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string Controls = ReadRepoFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");

	EXPECT_NE(TClient.find("BeginSettingsScrollRegion(s_ScrollRegion, &MainView, ScrollParams"), std::string::npos);
	EXPECT_NE(TClient.find("FinishSettingsScrollRegion(s_ScrollRegion, ScrollFrame, &ScrollRegion, SETTINGS_TCLIENT"), std::string::npos);
	EXPECT_NE(TClient.find("BeginSettingsScrollRegion(s_ScrollRegion, &ListArea, ScrollParams"), std::string::npos);
	EXPECT_NE(TClient.find("FinishSettingsScrollRegion(s_ScrollRegion, ScrollFrame, &EndPad"), std::string::npos);
	EXPECT_NE(Controls.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy(ScrollRequest, UiScale, 0.0f);"), std::string::npos);
	EXPECT_EQ(Controls.find("ScrollParams.m_ScrollUnit = 6.0f * BUTTON_HEIGHT;"), std::string::npos);
	EXPECT_EQ(Controls.find("ScrollParams.m_ForceShowScrollbar = true;"), std::string::npos);
	EXPECT_EQ(Settings.find("ScrollParams.m_ScrollUnit = LANGUAGE_ROW_HEIGHT;"), std::string::npos);
	const std::string QmOverviewBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientOverview(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(QmOverviewBody.empty());
	EXPECT_NE(QmOverviewBody.find("static CScrollRegion s_QmOverviewSettingsScrollRegion;"), std::string::npos);
	EXPECT_NE(QmOverviewBody.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(QmOverviewBody.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(QmOverviewBody.find("QmResolveScrollPolicy("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("BeginSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("FinishSettingsQmScrollContainer("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("DrawFullWidthCard("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("SQmScrollContainerInput ScrollInput;"), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("KEY_MOUSE_WHEEL_UP"), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("SetHotItem("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("SetActiveItem("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("Ui()->ClipEnable("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("Ui()->ClipDisable();"), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("BeginSettingsScrollRegion("), std::string::npos);
	EXPECT_EQ(QmOverviewBody.find("FinishSettingsScrollRegion("), std::string::npos);
	EXPECT_NE(QmClient.find("static CQmScrollState s_QmScrollState;"), std::string::npos);
	EXPECT_NE(Header.find("struct SSettingsQmScrollFrame"), std::string::npos);
	EXPECT_EQ(Header.find("m_ScrollOffsetChanged"), std::string::npos);
	EXPECT_NE(Header.find("BeginSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer"), std::string::npos);
	EXPECT_NE(Header.find("FinishSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer"), std::string::npos);
	EXPECT_NE(QmClient.find("CQmScrollState &ScrollState = s_QmOverviewSettingsScrollRegion.State();"), std::string::npos);
	EXPECT_NE(QmClient.find("const CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);"), std::string::npos);
	EXPECT_NE(QmClient.find("InputState.m_pScrollParams = PrewarmOnly ? nullptr : &ScrollParams;"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"qmclient-overview\", CMenus::SETTINGS_QMCLIENT}"), std::string::npos);
	EXPECT_NE(QmClient.find("Navigation.m_QmClientTab = CMenus::QMCLIENT_SETTINGS_TAB_OVERVIEW;"), std::string::npos);
	EXPECT_NE(QmClient.find("s_apQmTabNames[QMCLIENT_SETTINGS_TAB_OVERVIEW] = Localize(\"Overview\")"), std::string::npos);
	EXPECT_NE(QmClient.find("InputState.m_MousePressed = !PrewarmOnly"), std::string::npos);
	EXPECT_NE(QmClient.find("Localize(\"Overview\")"), std::string::npos);
	EXPECT_NE(QmClient.find("ReadOnly ? s_GlobalSearchPrewarmDeck : m_SettingsCardDeck;"), std::string::npos);
	const std::string MenusSource = ReadRepoFile("src/game/client/components/menus.cpp");
	EXPECT_NE(MenusSource.find("str_comp(pTab, \"qmclient-overview\")"), std::string::npos);
	EXPECT_NE(QmClient.find("SQmScrollContainerInput ScrollInput;"), std::string::npos);
	const std::string BeginScrollBody = ExtractSourceFunctionBody(QmClient, "CMenus::SSettingsQmScrollFrame CMenus::BeginSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer, CUIRect *pView, float ContentHeight, const SQmSettingsCardStyle &CardStyle, float UiScale, float PreviousOffsetY, bool Enabled)");
	const std::string FinishScrollBody = ExtractSourceFunctionBody(QmClient, "void CMenus::FinishSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer, SSettingsQmScrollFrame &Frame, const CUIRect &EndRect, float *pContentHeight, float *pPreviousOffsetY, bool TrackScrollActive)");
	ASSERT_FALSE(BeginScrollBody.empty());
	ASSERT_FALSE(FinishScrollBody.empty());
	EXPECT_EQ(BeginScrollBody.find("ConsumeHotkey(CUi::HOTKEY_SCROLL_"), std::string::npos);
	EXPECT_EQ(FinishScrollBody.find("ConsumeHotkey(CUi::HOTKEY_SCROLL_"), std::string::npos);
	EXPECT_NE(FinishScrollBody.find("Ui()->RegisterWheelOwner(pWheelOwnerId, EUiWheelOwnerPriority::PAGE, WheelHotRect, WheelEligible);"), std::string::npos);
	EXPECT_NE(FinishScrollBody.find("Ui()->TryConsumeWheel(pWheelOwnerId, &WheelDelta)"), std::string::npos);
	EXPECT_NE(FinishScrollBody.find("!Ui()->UnderlyingScrollBlocked()"), std::string::npos);
	EXPECT_NE(QmScroll.find("Config.m_WheelScale = 10.0f;"), std::string::npos);
	EXPECT_NE(QmScroll.find("SQmScrollConfig QmSettingsScrollConfig(float UiScale, float SmoothScrollTimeSec)"), std::string::npos);
	EXPECT_NE(QmScroll.find("Config.m_WheelScale = 120.0f;"), std::string::npos);
	const std::string QmSettingsParamsBody = ExtractSourceFunctionBody(Menus, "CScrollRegionParams CMenus::QmSettingsScrollRegionParams(float UiScale) const");
	ASSERT_FALSE(QmSettingsParamsBody.empty());
	EXPECT_NE(QmSettingsParamsBody.find("QmScrollRegionParamsForSize(EQmScrollSize::LARGE, UiScale)"), std::string::npos);
	EXPECT_NE(QmSettingsParamsBody.find("QmSettingsScrollConfig(UiScale, 0.0f).m_WheelScale"), std::string::npos);
	EXPECT_EQ(QmSettingsParamsBody.find("m_ForceShowScrollbar = true"), std::string::npos);
	EXPECT_EQ(QmScroll.find("Config.m_WheelScale = 10.0f * UiScale;"), std::string::npos);
	EXPECT_EQ(QmScroll.find("Config.m_WheelScale = 60.0f * UiScale;"), std::string::npos);
	EXPECT_EQ(QmClient.find("m_ScrollOffsetChanged"), std::string::npos);
	EXPECT_NE(QmClient.find("Ui()->ClipEnable(&Frame.m_ClipRect);"), std::string::npos);
	EXPECT_NE(QmClient.find("Ui()->ClipDisable();"), std::string::npos);
	EXPECT_NE(QmClient.find("Frame.m_Frame = ScrollContainer.PreviewFrame(ScrollState, Frame.m_ViewRect, *pContentHeight, Frame.m_Style);"), std::string::npos);
	EXPECT_EQ(QmClient.find("ScrollContainer.PreviewFrame(Frame.m_ClipRect"), std::string::npos);
	EXPECT_NE(QmClient.find("*pContentHeight = maximum(0.0f, std::ceil(EndRect.y + EndRect.h - (Frame.m_ClipRect.y + Frame.m_Offset.y)));"), std::string::npos);
	EXPECT_EQ(QmClient.find("CScrollRegion::HEIGHT_MAGIC_FIX"), std::string::npos);
	const std::string QmWrapperBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClient(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmWrapperBody.empty());
	ASSERT_FALSE(QmMainBody.empty());
	EXPECT_NE(QmMainBody.find("SSettingsQmScrollFrame QmScrollFrame = BeginSettingsQmScrollContainer(s_QmScrollState, s_QmScrollContainer, &MainView, s_QmScrollContentHeight, QmCardStyle, UiScale, s_PrevQmScrollY, !PrewarmOnly);"), std::string::npos);
	EXPECT_NE(QmMainBody.find("FinishSettingsQmScrollContainer(s_QmScrollState, s_QmScrollContainer, QmScrollFrame, ScrollEnd, &s_QmScrollContentHeight, &s_PrevQmScrollY, true);"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SQmScrollContainerInput ScrollInput;"), std::string::npos);
	EXPECT_NE(QmMainBody.find("TriggerUiSwitchAnimation(QmClientTabSwitchNode, 0.0f);"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("ApplyUiSwitchOffset(ContentView, TransitionStrength, s_QmTabTransitionDirection, false,"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("ApplyUiSwitchOffset(ContentView, TransitionStrength, s_QmTabTransitionDirection, false, 0.08f, 24.0f, 120.0f);"), std::string::npos);
	EXPECT_NE(Controls.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(Controls.find("&m_SettingsScrollRegion"), std::string::npos);
	EXPECT_EQ(Controls.find("BeginSettingsScrollRegion(m_SettingsScrollRegion"), std::string::npos);
	EXPECT_EQ(Controls.find("FinishSettingsScrollRegion(m_SettingsScrollRegion"), std::string::npos);
	EXPECT_NE(Settings.find("BeginSettingsScrollRegion(gs_LanguageScrollRegion, &MainView, ScrollParams"), std::string::npos);
	EXPECT_NE(Settings.find("FinishSettingsScrollRegion(gs_LanguageScrollRegion, ScrollFrame, &ScrollRegion, SETTINGS_LANGUAGE"), std::string::npos);
}

TEST(QmMonitoringHelpers, GlobalCardOrderConfigHasMigrationLandingZone)
{
	const std::string QmConfig = ReadRepoFile("src/engine/shared/config_variables_qmclient.h");

	EXPECT_NE(QmConfig.find("MACRO_CONFIG_STR(QmGlobalCardOrder, qm_global_card_order, 8192, \"\", CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_NE(QmConfig.find("MACRO_CONFIG_INT(QmCardOrderMigrated, qm_card_order_migrated, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_NE(QmConfig.find("MACRO_CONFIG_STR(QmSettingsCardOrder, qm_settings_card_order, 2048, \"\", CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_NE(QmConfig.find("MACRO_CONFIG_STR(QmSidebarCardOrder, qm_sidebar_card_order, 2048, \"\", CFGFLAG_CLIENT | CFGFLAG_SAVE"), std::string::npos);
	EXPECT_EQ(QmConfig.find("qm_global_card_order, 2048"), std::string::npos);
}

TEST(QmMonitoringHelpers, CardOrderModelUsesStableIdIndexForFind)
{
	const std::string Model = ReadRepoFile("src/game/client/QmUi/QmCardOrderModel.cpp");
	const std::string FindBody = ExtractSourceFunctionBody(Model, "int CModel::FindByStableId(const char *pStableId) const");
	ASSERT_FALSE(FindBody.empty());

	EXPECT_NE(FindBody.find("return StateIndexForStableId(pStableId);"), std::string::npos);
	EXPECT_EQ(FindBody.find("for(size_t i = 0; i < m_vEntries.size(); ++i)"), std::string::npos);
	EXPECT_NE(Model.find("BuildStateIndex(); // 维护 stableId→index 注册表"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientDropCommitUsesGlobalCardModelAsSourceOfTruth)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	const size_t CommitPos = Body.find("auto CommitDropPreview = [&]() -> bool");
	ASSERT_NE(CommitPos, std::string::npos);
	const size_t UpdatePreviewPos = Body.find("auto UpdateDropPreviewBeforeRender", CommitPos);
	ASSERT_NE(UpdatePreviewPos, std::string::npos);
	const std::string CommitBody = Body.substr(CommitPos, UpdatePreviewPos - CommitPos);

	EXPECT_NE(CommitBody.find("qm_module::MoveQmModuleInModel("), std::string::npos);
	EXPECT_NE(CommitBody.find("const std::vector<SQmModuleEntry> vModelEntries = qm_module::SyncModelToLegacyLayout();"), std::string::npos);
	EXPECT_NE(CommitBody.find("qm_module::SerializeQmLayoutFromModel(aSerialized, sizeof(aSerialized));"), std::string::npos);
	EXPECT_NE(CommitBody.find("qm_module::SerializeMergedGlobalCardOrderFromQmModel("), std::string::npos);
	EXPECT_EQ(CommitBody.find("auto BuildColumnIndices = [&]"), std::string::npos);
	EXPECT_EQ(CommitBody.find("LeftIndices"), std::string::npos);
	EXPECT_EQ(CommitBody.find("RightIndices"), std::string::npos);
	EXPECT_EQ(CommitBody.find("qm_module::LoadQmLayoutIntoModel(g_Config.m_QmSidebarCardOrder"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncRunsGlobalCardOrderMigration)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	const size_t LayoutCachePos = QmMainBody.find("str_copy(s_aQmModuleLayoutConfigCache, g_Config.m_QmSidebarCardOrder, sizeof(s_aQmModuleLayoutConfigCache));");
	const size_t MigrationGuardPos = QmMainBody.find("if(g_Config.m_QmCardOrderMigrated == 0 && qm_module::MigrateQmLayoutToGlobalCardOrder(std::vector<SQmModuleEntry>(s_aQmModuleLayout.begin(), s_aQmModuleLayout.end())))");
	const size_t MigrationPos = QmMainBody.find("qm_module::MigrateQmLayoutToGlobalCardOrder(std::vector<SQmModuleEntry>(s_aQmModuleLayout.begin(), s_aQmModuleLayout.end()))");
	const size_t GlobalCachePos = QmMainBody.find("str_copy(s_aQmGlobalCardOrderConfigCache, g_Config.m_QmGlobalCardOrder, sizeof(s_aQmGlobalCardOrderConfigCache));", MigrationGuardPos);
	EXPECT_NE(LayoutCachePos, std::string::npos);
	EXPECT_NE(MigrationGuardPos, std::string::npos);
	EXPECT_NE(MigrationPos, std::string::npos);
	EXPECT_NE(GlobalCachePos, std::string::npos);
	EXPECT_GT(MigrationGuardPos, LayoutCachePos);
	EXPECT_GT(MigrationPos, LayoutCachePos);
	EXPECT_LT(MigrationPos, GlobalCachePos);
	EXPECT_EQ(QmMainBody.find("MigrateQmLayoutToGlobalCardOrder(s_aQmModuleDefaults"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncPrefersGlobalCardOrder)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	EXPECT_NE(QmMainBody.find("const bool HasGlobalCardOrder = g_Config.m_QmGlobalCardOrder[0] != '\\0';"), std::string::npos);
	EXPECT_NE(QmMainBody.find("LoadQmLayoutModelFromGlobalOrder(g_Config.m_QmGlobalCardOrder, std::vector<SQmModuleEntry>(s_aQmModuleDefaults.begin(), s_aQmModuleDefaults.end()))"), std::string::npos);
	EXPECT_NE(QmMainBody.find("const std::vector<SQmModuleEntry> vModelEntries = qm_module::SyncModelToLegacyLayout();"), std::string::npos);
	EXPECT_NE(QmMainBody.find("s_aQmModuleLayout[i] = vModelEntries[i];"), std::string::npos);
	EXPECT_NE(QmMainBody.find("LoadQmLayoutIntoModel(g_Config.m_QmSidebarCardOrder"), std::string::npos);
	EXPECT_LT(QmMainBody.find("LoadQmLayoutModelFromGlobalOrder(g_Config.m_QmGlobalCardOrder"), QmMainBody.find("if(!UseSmartDefaultsOnly && !ParseQmModuleLayout(g_Config.m_QmSidebarCardOrder))"));
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncTracksGlobalCardOrderChanges)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	EXPECT_NE(QmMainBody.find("static char s_aQmGlobalCardOrderConfigCache[sizeof(g_Config.m_QmGlobalCardOrder)] = {};"), std::string::npos);
	EXPECT_NE(QmMainBody.find("const bool LegacyConfigChanged = str_comp(s_aQmModuleLayoutConfigCache, g_Config.m_QmSidebarCardOrder) != 0;"), std::string::npos);
	EXPECT_NE(QmMainBody.find("const bool GlobalConfigChanged = str_comp(s_aQmGlobalCardOrderConfigCache, g_Config.m_QmGlobalCardOrder) != 0;"), std::string::npos);
	EXPECT_NE(QmMainBody.find("const bool ConfigChanged = !s_QmModuleLayoutInitialized || LegacyConfigChanged || GlobalConfigChanged;"), std::string::npos);
	EXPECT_NE(QmMainBody.find("str_copy(s_aQmGlobalCardOrderConfigCache, g_Config.m_QmGlobalCardOrder, sizeof(s_aQmGlobalCardOrderConfigCache));"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncSerializesOnlyWhenConfigOrModelDirty)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());
	const std::string SyncBody = ExtractSourceBlock(QmMainBody, "auto SyncQmModuleLayout = [&]()", "SyncQmModuleLayout();");
	ASSERT_FALSE(SyncBody.empty());

	EXPECT_NE(SyncBody.find("const bool ModelDirty = qm_module::IsQmLayoutModelDirty();"), std::string::npos);
	EXPECT_NE(SyncBody.find("if(ConfigChanged || ModelDirty)"), std::string::npos);
	EXPECT_NE(SyncBody.find("qm_module::SerializeQmLayoutFromModel(aSerialized, sizeof(aSerialized));"), std::string::npos);
	EXPECT_NE(SyncBody.find("qm_module::ClearQmLayoutModelDirty();"), std::string::npos);
	EXPECT_EQ(SyncBody.find("SerializeQmModuleLayout(aSerialized, sizeof(aSerialized));"), std::string::npos);
	EXPECT_LT(SyncBody.find("const bool ModelDirty = qm_module::IsQmLayoutModelDirty();"), SyncBody.find("if(ConfigChanged || ModelDirty)"));
	EXPECT_LT(SyncBody.find("if(ConfigChanged || ModelDirty)"), SyncBody.find("qm_module::SerializeQmLayoutFromModel(aSerialized, sizeof(aSerialized));"));
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncPersistsDirtyModelToGlobalOrder)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());
	const std::string SyncBody = ExtractSourceBlock(QmMainBody, "auto SyncQmModuleLayout = [&]()", "SyncQmModuleLayout();");
	ASSERT_FALSE(SyncBody.empty());

	const size_t DirtyBranchPos = SyncBody.find("if(ConfigChanged || ModelDirty)");
	ASSERT_NE(DirtyBranchPos, std::string::npos);
	const size_t OldLayoutPersistPos = SyncBody.find("qm_module::SerializeQmLayoutFromModel(aSerialized, sizeof(aSerialized));", DirtyBranchPos);
	const size_t GlobalPersistGuardPos = SyncBody.find("if(g_Config.m_QmGlobalCardOrder[0] != '\\0' || ModelDirty)", DirtyBranchPos);
	const size_t GlobalMergePos = SyncBody.find("qm_module::SerializeMergedGlobalCardOrderFromQmModel(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(aMergedGlobalOrder))", DirtyBranchPos);
	const size_t GlobalCopyPos = SyncBody.find("str_copy(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(g_Config.m_QmGlobalCardOrder));", DirtyBranchPos);
	const size_t GlobalCachePos = SyncBody.find("str_copy(s_aQmGlobalCardOrderConfigCache, g_Config.m_QmGlobalCardOrder, sizeof(s_aQmGlobalCardOrderConfigCache));", DirtyBranchPos);
	const size_t DirtyClearPos = SyncBody.find("qm_module::ClearQmLayoutModelDirty();", DirtyBranchPos);

	EXPECT_NE(OldLayoutPersistPos, std::string::npos);
	EXPECT_NE(GlobalPersistGuardPos, std::string::npos);
	EXPECT_NE(GlobalMergePos, std::string::npos);
	EXPECT_NE(GlobalCopyPos, std::string::npos);
	EXPECT_NE(GlobalCachePos, std::string::npos);
	EXPECT_NE(DirtyClearPos, std::string::npos);
	EXPECT_LT(OldLayoutPersistPos, GlobalPersistGuardPos);
	EXPECT_LT(GlobalPersistGuardPos, GlobalMergePos);
	EXPECT_LT(GlobalMergePos, GlobalCopyPos);
	EXPECT_LT(GlobalCopyPos, GlobalCachePos);
	EXPECT_LT(GlobalCachePos, DirtyClearPos);
}

TEST(QmMonitoringHelpers, QmClientLayoutSyncKeepsDirtyWhenGlobalPersistFails)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());
	const std::string SyncBody = ExtractSourceBlock(QmMainBody, "auto SyncQmModuleLayout = [&]()", "SyncQmModuleLayout();");
	ASSERT_FALSE(SyncBody.empty());

	const size_t DirtyBranchPos = SyncBody.find("if(ConfigChanged || ModelDirty)");
	ASSERT_NE(DirtyBranchPos, std::string::npos);
	const size_t PersistFlagInitPos = SyncBody.find("bool GlobalOrderPersisted = !ModelDirty;", DirtyBranchPos);
	const size_t GlobalMergePos = SyncBody.find("qm_module::SerializeMergedGlobalCardOrderFromQmModel(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(aMergedGlobalOrder))", DirtyBranchPos);
	const size_t PersistFlagSetPos = SyncBody.find("GlobalOrderPersisted = true;", DirtyBranchPos);
	const size_t DirtyClearGuardPos = SyncBody.find("if(GlobalOrderPersisted)", DirtyBranchPos);
	const size_t DirtyClearPos = SyncBody.find("qm_module::ClearQmLayoutModelDirty();", DirtyBranchPos);

	EXPECT_NE(PersistFlagInitPos, std::string::npos);
	EXPECT_NE(GlobalMergePos, std::string::npos);
	EXPECT_NE(PersistFlagSetPos, std::string::npos);
	EXPECT_NE(DirtyClearGuardPos, std::string::npos);
	EXPECT_NE(DirtyClearPos, std::string::npos);
	EXPECT_LT(PersistFlagInitPos, GlobalMergePos);
	EXPECT_LT(GlobalMergePos, PersistFlagSetPos);
	EXPECT_LT(PersistFlagSetPos, DirtyClearGuardPos);
	EXPECT_LT(DirtyClearGuardPos, DirtyClearPos);
}

TEST(QmMonitoringHelpers, SettingsCardOrderPersistenceCommitsGlobalConfigAtomically)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string LoadBody = ExtractSourceFunctionBody(Menus, "void CMenus::LoadSettingsCardOrderModel()");
	const std::string SaveBody = ExtractSourceFunctionBody(Menus, "bool CMenus::SaveSettingsCardOrderModel()");
	ASSERT_FALSE(LoadBody.empty());
	ASSERT_FALSE(SaveBody.empty());

	const char *pTempBuffer = "char aSerialized[sizeof(g_Config.m_QmGlobalCardOrder)]";
	const char *pSerialize = "m_SettingsCardOrderModel.Serialize(aSerialized, sizeof(aSerialized))";
	const char *pCopy = "str_copy(g_Config.m_QmGlobalCardOrder, aSerialized, sizeof(g_Config.m_QmGlobalCardOrder));";
	const size_t LoadTempPos = LoadBody.find(pTempBuffer);
	const size_t LoadSerializePos = LoadBody.find(pSerialize);
	const size_t LoadCopyPos = LoadBody.find(pCopy);
	const size_t LoadDirtyClearPos = LoadBody.find("m_SettingsCardOrderModel.ClearDirty();");
	const size_t LoadMigratedPos = LoadBody.find("g_Config.m_QmCardOrderMigrated = 1;");
	EXPECT_NE(LoadTempPos, std::string::npos);
	EXPECT_NE(LoadSerializePos, std::string::npos);
	EXPECT_NE(LoadCopyPos, std::string::npos);
	EXPECT_NE(LoadDirtyClearPos, std::string::npos);
	EXPECT_NE(LoadMigratedPos, std::string::npos);
	EXPECT_LT(LoadTempPos, LoadSerializePos);
	EXPECT_LT(LoadSerializePos, LoadCopyPos);
	EXPECT_LT(LoadCopyPos, LoadDirtyClearPos);
	EXPECT_LT(LoadDirtyClearPos, LoadMigratedPos);

	const size_t SaveTempPos = SaveBody.find(pTempBuffer);
	const size_t SaveSerializePos = SaveBody.find(pSerialize);
	const size_t SaveCopyPos = SaveBody.find(pCopy);
	const size_t SaveDirtyClearPos = SaveBody.find("m_SettingsCardOrderModel.ClearDirty();");
	EXPECT_NE(SaveTempPos, std::string::npos);
	EXPECT_NE(SaveSerializePos, std::string::npos);
	EXPECT_NE(SaveCopyPos, std::string::npos);
	EXPECT_NE(SaveDirtyClearPos, std::string::npos);
	EXPECT_LT(SaveTempPos, SaveSerializePos);
	EXPECT_LT(SaveSerializePos, SaveCopyPos);
	EXPECT_LT(SaveCopyPos, SaveDirtyClearPos);
}
TEST(QmMonitoringHelpers, QmClientDragPersistsGlobalCardOrder)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	const size_t CommitPos = QmMainBody.find("auto CommitDropPreview = [&]() -> bool");
	ASSERT_NE(CommitPos, std::string::npos);
	const size_t DragPersistPos = QmMainBody.find("qm_module::SerializeQmLayoutFromModel(aSerialized, sizeof(aSerialized));", CommitPos);
	const size_t ModelSyncPos = QmMainBody.find("const std::vector<SQmModuleEntry> vModelEntries = qm_module::SyncModelToLegacyLayout();", CommitPos);
	const size_t GlobalMergePos = QmMainBody.find("if(!qm_module::SerializeMergedGlobalCardOrderFromQmModel(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(aMergedGlobalOrder)))", CommitPos);
	const size_t GlobalMergeAbortPos = QmMainBody.find("return false;", GlobalMergePos);
	const size_t GlobalCopyPos = QmMainBody.find("str_copy(g_Config.m_QmGlobalCardOrder, aMergedGlobalOrder, sizeof(g_Config.m_QmGlobalCardOrder));", CommitPos);
	const size_t GlobalCachePos = QmMainBody.find("str_copy(s_aQmGlobalCardOrderConfigCache, g_Config.m_QmGlobalCardOrder, sizeof(s_aQmGlobalCardOrderConfigCache));", CommitPos);
	const size_t MigratedPos = QmMainBody.find("g_Config.m_QmCardOrderMigrated = 1;", CommitPos);
	EXPECT_NE(DragPersistPos, std::string::npos);
	EXPECT_NE(ModelSyncPos, std::string::npos);
	EXPECT_NE(GlobalMergePos, std::string::npos);
	EXPECT_NE(GlobalMergeAbortPos, std::string::npos);
	EXPECT_NE(GlobalCopyPos, std::string::npos);
	EXPECT_NE(GlobalCachePos, std::string::npos);
	EXPECT_NE(MigratedPos, std::string::npos);
	EXPECT_LT(ModelSyncPos, DragPersistPos);
	EXPECT_LT(DragPersistPos, GlobalMergePos);
	EXPECT_LT(GlobalMergePos, GlobalMergeAbortPos);
	EXPECT_LT(GlobalMergeAbortPos, GlobalCopyPos);
	EXPECT_LT(GlobalMergePos, GlobalCopyPos);
	EXPECT_LT(GlobalCopyPos, GlobalCachePos);
	EXPECT_NE(QmMainBody.find("qm_module::ClearQmLayoutModelDirty();", CommitPos), std::string::npos);
	EXPECT_EQ(QmMainBody.find("QmModuleLayoutModel().Serialize(g_Config.m_QmGlobalCardOrder"), std::string::npos);
}

TEST(QmMonitoringHelpers, GlobalSearchUsesDedicatedSettingsPage)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string RuntimeCache = ReadRepoFile("src/game/client/components/settings_runtime_cache.cpp");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmWrapperBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClient(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const std::string SearchBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsGlobalSearch(CUIRect MainView, bool PrewarmOnly)");
	const std::string SearchContentBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)");
	const std::string SharedBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmWrapperBody.empty());
	ASSERT_FALSE(QmMainBody.empty());
	ASSERT_FALSE(SearchBody.empty());
	ASSERT_FALSE(SearchContentBody.empty());
	ASSERT_FALSE(SharedBody.empty());

	EXPECT_NE(Header.find("SETTINGS_SEARCH"), std::string::npos);
	EXPECT_EQ(Header.find("QMCLIENT_SETTINGS_TAB_SEARCH"), std::string::npos);
	EXPECT_EQ(Header.find("CLineInputBuffered<128> m_QmClientModuleSearchInput;"), std::string::npos);
	EXPECT_NE(Header.find("CLineInputBuffered<128> m_GlobalCardSearchInput;"), std::string::npos);
	EXPECT_EQ(Header.find("m_aQmClientModuleSearchInputs"), std::string::npos);
	EXPECT_NE(Settings.find("case CMenus::SETTINGS_SEARCH: return \"search\";"), std::string::npos);
	EXPECT_NE(Menus.find("m_apSettingsTabs[SETTINGS_SEARCH] = Localize(\"Search\");"), std::string::npos);
	EXPECT_NE(Settings.find("SETTINGS_SEARCH,"), std::string::npos);
	EXPECT_NE(RuntimeCache.find("case CMenus::SETTINGS_SEARCH: return \"search\";"), std::string::npos);
	EXPECT_NE(Settings.find("RenderSettingsGlobalSearch(ContentView, CollectingMenuTextPlan);"), std::string::npos);
	EXPECT_EQ(QmClient.find("case CMenus::QMCLIENT_SETTINGS_TAB_SEARCH: return \"search\";"), std::string::npos);
	EXPECT_EQ(QmClient.find("s_apQmTabNames[QMCLIENT_SETTINGS_TAB_SEARCH] = Localize(\"Search\");"), std::string::npos);
	EXPECT_NE(Header.find("RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly = false);"), std::string::npos);
	EXPECT_NE(Header.find("RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly);"), std::string::npos);
	EXPECT_NE(QmWrapperBody.find("RenderSettingsQmClientContent(MainView, ContributorsPage, PrewarmOnly);"), std::string::npos);
	EXPECT_NE(SearchBody.find("RenderSettingsGlobalSearchContent(MainView, PrewarmOnly);"), std::string::npos);
	EXPECT_EQ(SearchBody.find("RenderSettingsQmClientContent("), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SETTINGS_SEARCH"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchTabActive"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("CLineInputBuffered<128> &ModuleSearchInput = m_GlobalCardSearchInput;"), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("m_QmClientModuleSearchInput"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("IUiContext SearchCtx = SettingsUiContext(\"settings_global_search\", UiScale);"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("ui_widget::InputField(SearchCtx, &ModuleSearchInput, Row, BodySize"), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("Ui()->DoEditBox_Search(&ModuleSearchInput"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("InputCard.m_Spec = {\"deck:global-search-input\""), std::string::npos);
	EXPECT_NE(SearchContentBody.find("ResultsCard.m_Spec = {\"deck:global-search-results\""), std::string::npos);
	EXPECT_NE(SearchContentBody.find("DoSettingsMenuLabel(SETTINGS_SEARCH, -1, -1, \"qmclient-search-no-matching-features\""), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("DoSettingsMenuLabel(SETTINGS_QMCLIENT"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("CollectGlobalSearchResults(pModuleSearch, GlobalSearchResults);"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("CardDeck.Render(SearchCtx, Page, \"global-search\""), std::string::npos);
	EXPECT_NE(SearchContentBody.find("static CScrollRegion s_GlobalSearchScrollRegion;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("const bool ReadOnly = PrewarmOnly || Ui()->RenderOnly();"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("s_GlobalSearchPrewarmOrderModel.LoadMerged(\"\", qm_card_registry::BuildDefaultEntries());"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("ReadOnly ? s_GlobalSearchPrewarmOrderModel : SettingsCardOrderModel();"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("static CSettingsCardDeck s_GlobalSearchPrewarmDeck;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("ReadOnly ? s_GlobalSearchPrewarmDeck : m_SettingsCardDeck;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("if(ReadOnly)\n\t{\n\t\tSearchCtx.m_pAnim = nullptr;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("InputState.m_pScrollParams = ReadOnly ? nullptr : &ScrollParams;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("ReadOnly ? nullptr : &s_GlobalSearchScrollRegion"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("const bool Clicked = !ReadOnly && Ui()->DoButtonLogic"), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("BeginSettingsQmScrollContainer"), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("RenderQmSettingsGlassCard"), std::string::npos);
	EXPECT_EQ(SharedBody.find("GlobalSearchPage"), std::string::npos);
	EXPECT_EQ(SharedBody.find("SearchTabActive"), std::string::npos);
	EXPECT_EQ(SharedBody.find("ShowSearchModuleControls"), std::string::npos);
	EXPECT_EQ(SearchContentBody.find("const std::vector<const SQmGlobalSearchCard *> &SearchVisibleGlobalCards = GlobalSearchResults.m_vVisibleCards;"), std::string::npos);
	EXPECT_EQ(SharedBody.find("SearchVisibleGlobalCards"), std::string::npos);
	EXPECT_EQ(SharedBody.find("SearchVisibleExternalCards"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("const std::vector<const SQmGlobalSearchCard *> &SearchVisibleGlobalCards = GlobalSearchResults.m_vAllVisibleCards;"), std::string::npos);
	EXPECT_NE(SearchContentBody.find("Localize(\"Found %d global cards\")"), std::string::npos);
	EXPECT_EQ(SharedBody.find("g_Config.m_UiSettingsPage == SETTINGS_SEARCH"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("m_aQmClientModuleSearchInputs["), std::string::npos);
	EXPECT_EQ(QmMainBody.find("m_GlobalCardSearchInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientSearchTabUsesGlobalCardRegistry)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string SharedBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(SharedBody.empty());

	EXPECT_NE(QmClient.find("#include <game/client/QmUi/QmCardRegistry.h>"), std::string::npos);
	EXPECT_NE(QmClient.find("struct SQmGlobalSearchCard"), std::string::npos);
	EXPECT_NE(QmClient.find("struct SQmGlobalSearchNavigation"), std::string::npos);
	EXPECT_NE(QmClient.find("BuildGlobalSearchCards("), std::string::npos);
	EXPECT_NE(QmClient.find("MatchesGlobalSearchCard("), std::string::npos);
	EXPECT_NE(QmClient.find("CollectGlobalSearchResults("), std::string::npos);
	EXPECT_NE(QmClient.find("ResolveGlobalSearchNavigation("), std::string::npos);
	EXPECT_NE(QmClient.find("pCard->m_pTitle != nullptr && str_utf8_find_nocase(pCard->m_pTitle, pSearch)"), std::string::npos);
	EXPECT_NE(QmClient.find("pCard->m_pSearchKeywords != nullptr && str_utf8_find_nocase(pCard->m_pSearchKeywords, pSearch)"), std::string::npos);
	EXPECT_NE(SharedBody.find("SQmGlobalSearchResults GlobalSearchResults;"), std::string::npos);
	EXPECT_NE(SharedBody.find("CollectGlobalSearchResults(pModuleSearch, GlobalSearchResults);"), std::string::npos);
	EXPECT_NE(SharedBody.find("GlobalSearchResults.m_vAllVisibleCards"), std::string::npos);
	EXPECT_NE(QmClient.find("const std::vector<qm_card_registry::SCardDefault> &Defaults = qm_card_registry::Defaults();"), std::string::npos);
	EXPECT_EQ(SharedBody.find("qm_card_registry::Defaults()"), std::string::npos);
	EXPECT_EQ(SharedBody.find("EQmModuleId::Info"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientMainPageRemovesLegacyModuleSearchPipeline)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	EXPECT_EQ(QmMainBody.find("HasModuleSearch"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("pModuleSearch"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("ModuleSearchKeywords"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("ModuleMatchesSearch"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("CQmFunctionSnapshotJob"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SQmFunctionSnapshot"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchVisibleModules"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchLeftModules"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchRightModules"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchSingleColumnMode"), std::string::npos);
	EXPECT_EQ(QmMainBody.find("SearchDragBlocked"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientCompactLayoutStillRendersBothModuleColumns)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string QmMainBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmMainBody.empty());

	const size_t LeftRenderPos = QmMainBody.find("RenderColumnModules(VisibleLeftModules, EQmModuleColumn::Left);");
	const size_t RightRenderPos = QmMainBody.find("RenderColumnModules(VisibleRightModules, EQmModuleColumn::Right);");
	ASSERT_NE(LeftRenderPos, std::string::npos);
	ASSERT_NE(RightRenderPos, std::string::npos);
	EXPECT_LT(LeftRenderPos, RightRenderPos);
	EXPECT_EQ(QmMainBody.find("if(!CompactLayout)\n\t\t\tRenderColumnModules(VisibleRightModules, EQmModuleColumn::Right);"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6GlobalSearchUsesPublicDeckOnly)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string SharedBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(SharedBody.empty());

	EXPECT_NE(QmClient.find("std::vector<SQmGlobalSearchCard> m_vCards;"), std::string::npos);
	EXPECT_NE(QmClient.find("std::vector<const SQmGlobalSearchCard *> m_vAllVisibleCards;"), std::string::npos);
	EXPECT_NE(QmClient.find("std::vector<const SQmGlobalSearchCard *> m_vVisibleCards;"), std::string::npos);
	EXPECT_NE(QmClient.find("std::vector<const SQmGlobalSearchCard *> m_vExternalCards;"), std::string::npos);
	EXPECT_NE(QmClient.find("IsQmGlobalSearchCard("), std::string::npos);
	EXPECT_NE(QmClient.find("Out.m_vExternalCards.push_back("), std::string::npos);
	EXPECT_NE(SharedBody.find("const std::vector<const SQmGlobalSearchCard *> &SearchVisibleGlobalCards = GlobalSearchResults.m_vAllVisibleCards;"), std::string::npos);
	EXPECT_NE(QmClient.find("Out.m_vAllVisibleCards.push_back(&GlobalSearchCard);"), std::string::npos);
	EXPECT_NE(SharedBody.find("ResultsCard.m_MeasureEachFrame = true;"), std::string::npos);
	EXPECT_NE(SharedBody.find("ResolveGlobalSearchNavigation(*pCard)"), std::string::npos);
	EXPECT_NE(SharedBody.find("Ui()->DoButtonLogic(pCard->m_pStableId, 0, &ResultRect, BUTTONFLAG_LEFT)"), std::string::npos);
	EXPECT_NE(SharedBody.find("GlobalSearchNavigationLabel(Navigation)"), std::string::npos);
	EXPECT_NE(SharedBody.find("m_SettingsCardDeck.RequestReveal(pCard->m_pStableId);"), std::string::npos);
	EXPECT_EQ(QmClient.find("void CMenus::RenderGlobalSearchResultCard("), std::string::npos);
	EXPECT_EQ(QmClient.find("void CMenus::RenderGlobalSearchResults("), std::string::npos);
	EXPECT_EQ(QmClient.find("s_GlobalSearchCardButtonIndex"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientSearchNavigationTargetsSettingsPages)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const size_t NavigationPos = QmClient.find("SQmGlobalSearchNavigation ResolveGlobalSearchNavigation");
	ASSERT_NE(NavigationPos, std::string::npos);
	const std::string NavigationBody = QmClient.substr(NavigationPos, 4200);
	const std::string CardBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(CardBody.empty());

	EXPECT_NE(NavigationBody.find("str_startswith(pStableId, \"qm:\")"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_SettingsPage = CMenus::SETTINGS_QMCLIENT;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_QmClientTab = CMenus::QMCLIENT_SETTINGS_TAB_FUNCTION;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_QmClientTab = CMenus::QMCLIENT_SETTINGS_TAB_HUD;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_SettingsPage = CMenus::SETTINGS_TCLIENT;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_TClientTab = 0;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_SettingsPage = Route.m_SettingsPage;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_TClientTab = Route.m_TClientTab;"), std::string::npos);
	EXPECT_NE(NavigationBody.find("Navigation.m_AppearanceTab = Route.m_AppearanceTab;"), std::string::npos);
	EXPECT_NE(CardBody.find("if(Navigation.m_AppearanceTab >= 0)"), std::string::npos);
	EXPECT_NE(CardBody.find("m_AppearanceSettingsTab = Navigation.m_AppearanceTab;"), std::string::npos);
	EXPECT_NE(MenusHeader.find("int m_AppearanceSettingsTab = APPEARANCE_TAB_HUD;"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientSearchNavigationUsesTabRouteTable)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t NavigationPos = QmClient.find("SQmGlobalSearchNavigation ResolveGlobalSearchNavigation");
	ASSERT_NE(NavigationPos, std::string::npos);
	const std::string NavigationBody = QmClient.substr(NavigationPos, 3600);

	EXPECT_NE(QmClient.find("struct SQmGlobalSearchTabRoute"), std::string::npos);
	EXPECT_NE(QmClient.find("s_aGlobalSearchTabRoutes[]"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"graphics\", CMenus::SETTINGS_GRAPHICS"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"sound\", CMenus::SETTINGS_SOUND"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"ddnet\", CMenus::SETTINGS_DDNET"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"tclient-bind-wheel\", CMenus::SETTINGS_TCLIENT"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"tclient-status-bar\", CMenus::SETTINGS_TCLIENT"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-hud\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-chat\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-name-plate\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-hook-collision\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-info-messages\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(QmClient.find("{\"appearance-laser\", CMenus::SETTINGS_APPEARANCE"), std::string::npos);
	EXPECT_NE(NavigationBody.find("for(const SQmGlobalSearchTabRoute &Route : s_aGlobalSearchTabRoutes)"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"graphics\")"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"sound\")"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"ddnet\")"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"tclient-bind-wheel\")"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"tclient-status-bar\")"), std::string::npos);
	EXPECT_EQ(NavigationBody.find("str_comp(pTab, \"appearance-"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientSearchNavigationRouteTableCoversRegistryDeckTabs)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t RouteTablePos = QmClient.find("static constexpr SQmGlobalSearchTabRoute s_aGlobalSearchTabRoutes[]");
	ASSERT_NE(RouteTablePos, std::string::npos);
	const size_t RouteTableEnd = QmClient.find("};", RouteTablePos);
	ASSERT_NE(RouteTableEnd, std::string::npos);
	const std::string RouteTable = QmClient.substr(RouteTablePos, RouteTableEnd - RouteTablePos);

	std::set<std::string> DeckTabs;
	for(const qm_card_registry::SCardDefault &Default : qm_card_registry::Defaults())
	{
		if(Default.m_pStableId == nullptr || Default.m_pDefaultTab == nullptr)
			continue;
		if(str_startswith(Default.m_pStableId, "deck:") == nullptr)
			continue;
		DeckTabs.insert(Default.m_pDefaultTab);
	}
	ASSERT_FALSE(DeckTabs.empty());
	for(const std::string &Tab : DeckTabs)
	{
		if(Tab == "qmclient-contributors")
		{
			EXPECT_NE(QmClient.find("if(str_comp(pTab, \"qmclient-contributors\") == 0)"), std::string::npos);
			continue;
		}
		if(Tab == "global-search")
			continue;
		const std::string Needle = "{\"" + Tab + "\",";
		EXPECT_NE(RouteTable.find(Needle), std::string::npos) << Tab;
	}
}

TEST(QmMonitoringHelpers, QmClientFunctionHotspotModulesHaveFirstFrameStages)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsQmClientFunctionDeck(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("function_first_frame_light_path"), std::string::npos);
	EXPECT_NE(Body.find("const bool FunctionFirstFrameLightPath"), std::string::npos);
	EXPECT_NE(Body.find("FunctionFirstFrameLightPath"), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"pie_menu_layout\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"pie_menu_controls\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"block_words_layout\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"block_words_controls\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"gores_layout\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"gores_controls\""), std::string::npos);
	EXPECT_NE(Source.find("LogQmPerfStage(Client(), \"gores_bind_lookup\""), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientFriendEnterTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmFunctionFriendNotifyContent(");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("SettingsUiContext(\"settings_qmclient_friend_enter_text_inputs\", 1.0f)"), std::string::npos);
	EXPECT_NE(Body.find("pInput->SetEmptyText(Localize(pPlaceholder));"), std::string::npos);
	EXPECT_NE(Body.find("RenderText(\"qmclient-friend-notifications-large-text-content\", \"Large text content\", &s_FriendEnterBroadcastText, \"Please use %s as friend name\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, pInput, ControlColumn, Localize(pPlaceholder), BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("s_FriendEnterGreetText"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_FriendEnterBroadcastText"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_FriendEnterGreetText"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientGoresActorChatInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmFunctionGoresActorContent(");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("SettingsUiContext(\"settings_qmclient_gores_actor_text_inputs\", 1.0f)"), std::string::npos);
	EXPECT_NE(Body.find("s_FreezeChatMessageQmClient.SetEmptyText(Localize(\"Leave empty to disable\"));"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_FreezeChatMessageQmClient, ControlColumn, Localize(\"Leave empty to disable\"), BodySize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_FreezeChatMessageQmClient"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientPieMenuRenameQueueUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmFunctionPieMenuContent(");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("SettingsUiContext(\"settings_qmclient_pie_menu_text_inputs\", 1.0f)"), std::string::npos);
	EXPECT_NE(Body.find("s_PieMenuRenameQueue.SetEmptyText(Localize(\"Example: name1|name2|name3\"));"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_PieMenuRenameQueue, ControlColumn, Localize(\"Example: name1|name2|name3\"), BodySize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_PieMenuRenameQueue"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientTranslateSettingsInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmFunctionTranslateContent(");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("SettingsUiContext(\"settings_qmclient_translate_text_inputs\", 1.0f)"), std::string::npos);

	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateEndpoint, ControlCol, \"https://tmt.tencentcloudapi.com/\", BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateEndpoint, ControlCol, \"http://localhost:5000\", BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateRegion, ControlCol, \"ap-guangzhou\", BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateSecretId, ControlCol, Localize(\"Tencent Cloud SecretId\"), BodySize);"), std::string::npos);
	const size_t TcSecretKeyHiddenPos = Body.find("s_TranslateSecretKey.SetHidden(true);");
	const size_t TcSecretKeyTextFieldPos = Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateSecretKey, ControlCol, Localize(\"Tencent Cloud SecretKey\"), BodySize);", TcSecretKeyHiddenPos);
	EXPECT_NE(TcSecretKeyHiddenPos, std::string::npos);
	EXPECT_NE(TcSecretKeyTextFieldPos, std::string::npos);
	EXPECT_LT(TcSecretKeyHiddenPos, TcSecretKeyTextFieldPos);
	const size_t LibreKeyHiddenPos = Body.find("s_TranslateKey.SetHidden(true);");
	const size_t LibreKeyTextFieldPos = Body.find("ui_widget::InputField(TextInputCtx, &s_TranslateKey, ControlCol, \"\", BodySize);", LibreKeyHiddenPos);
	EXPECT_NE(LibreKeyHiddenPos, std::string::npos);
	EXPECT_NE(LibreKeyTextFieldPos, std::string::npos);
	EXPECT_LT(LibreKeyHiddenPos, LibreKeyTextFieldPos);

	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, pActiveKeyInput, ControlCol, pActiveKeyInput->GetEmptyText(), BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, pActiveEndpointInput, ControlCol, pActiveEndpointInput->GetEmptyText(), BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, pActiveModelInput, ControlCol, pActiveModelInput->GetEmptyText(), BodySize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TextInputCtx, &s_CustomPrompt, ControlCol, Localize(\"Leave empty to use default prompt\"), BodySize);"), std::string::npos);
	const size_t LanguageOptionsPos = Body.find("ui_widget::STextFieldOptions LanguageInputOptions;");
	const size_t LanguagePlaceholderPos = Body.find("LanguageInputOptions.m_pPlaceholder = pEmptyText;", LanguageOptionsPos);
	const size_t LanguageFontPos = Body.find("LanguageInputOptions.m_FontSize = BodySize;", LanguagePlaceholderPos);
	const size_t LanguageCornersPos = Body.find("LanguageInputOptions.m_Corners = IGraphics::CORNER_ALL;", LanguageFontPos);
	const size_t LanguageAlignPos = Body.find("LanguageInputOptions.m_TextAlign = TEXTALIGN_MC;", LanguageCornersPos);
	const size_t LanguageTextFieldPos = Body.find("ui_widget::InputField(TextInputCtx, &LineInput, EditRect, LanguageInputOptions);", LanguageAlignPos);
	const size_t LanguageWriteBackPos = Body.find("str_copy(pConfigValue, LineInput.GetString(), ConfigValueSize);", LanguageTextFieldPos);
	EXPECT_NE(LanguageOptionsPos, std::string::npos);
	EXPECT_NE(LanguagePlaceholderPos, std::string::npos);
	EXPECT_NE(LanguageFontPos, std::string::npos);
	EXPECT_NE(LanguageCornersPos, std::string::npos);
	EXPECT_NE(LanguageAlignPos, std::string::npos);
	EXPECT_NE(LanguageTextFieldPos, std::string::npos);
	EXPECT_NE(LanguageWriteBackPos, std::string::npos);
	EXPECT_LT(LanguageOptionsPos, LanguagePlaceholderPos);
	EXPECT_LT(LanguagePlaceholderPos, LanguageFontPos);
	EXPECT_LT(LanguageFontPos, LanguageCornersPos);
	EXPECT_LT(LanguageCornersPos, LanguageAlignPos);
	EXPECT_LT(LanguageAlignPos, LanguageTextFieldPos);
	EXPECT_LT(LanguageTextFieldPos, LanguageWriteBackPos);

	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TranslateEndpoint"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TranslateRegion"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TranslateSecretId"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TranslateSecretKey"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TranslateKey"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(pActiveKeyInput"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(pActiveEndpointInput"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(pActiveModelInput"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_CustomPrompt"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&LineInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientVoiceTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmHudVoiceContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, float UiScale, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext QmClientVoiceTextInputCtx;");
	const size_t UiPos = Body.find("QmClientVoiceTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("QmClientVoiceTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("QmClientVoiceTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("QmClientVoiceTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_qmclient_voice_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("QmClientVoiceTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);

	const size_t TokenInputPos = Body.find("static CLineInput s_VoiceToken(g_Config.m_QmVoiceToken, sizeof(g_Config.m_QmVoiceToken));");
	const size_t TokenEmptyTextPos = Body.find("s_VoiceToken.SetEmptyText(Localize(\"Leave empty to join public room\"));", TokenInputPos);
	const size_t TokenHiddenPos = Body.find("s_VoiceToken.SetHidden(true);", TokenEmptyTextPos);
	const size_t TokenTextFieldPos = Body.find("ui_widget::InputField(QmClientVoiceTextInputCtx, &s_VoiceToken, ControlCol, Localize(\"Leave empty to join public room\"), BodySize);", TokenHiddenPos);
	EXPECT_NE(TokenInputPos, std::string::npos);
	EXPECT_NE(TokenEmptyTextPos, std::string::npos);
	EXPECT_NE(TokenHiddenPos, std::string::npos);
	EXPECT_NE(TokenTextFieldPos, std::string::npos);
	EXPECT_LT(TokenInputPos, TokenEmptyTextPos);
	EXPECT_LT(TokenEmptyTextPos, TokenHiddenPos);
	EXPECT_LT(TokenHiddenPos, TokenTextFieldPos);

	const size_t ServerInputPos = Body.find("static CLineInput s_VoiceServer(g_Config.m_QmVoiceServer, sizeof(g_Config.m_QmVoiceServer));");
	const size_t ServerEmptyTextPos = Body.find("s_VoiceServer.SetEmptyText(\"42.194.185.210:9987\");", ServerInputPos);
	const size_t ServerTextFieldPos = Body.find("ui_widget::InputField(QmClientVoiceTextInputCtx, &s_VoiceServer, ControlCol, \"42.194.185.210:9987\", BodySize);", ServerEmptyTextPos);
	EXPECT_NE(ServerInputPos, std::string::npos);
	EXPECT_NE(ServerEmptyTextPos, std::string::npos);
	EXPECT_NE(ServerTextFieldPos, std::string::npos);
	EXPECT_LT(ServerInputPos, ServerEmptyTextPos);
	EXPECT_LT(ServerEmptyTextPos, ServerTextFieldPos);

	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_VoiceToken, &ControlCol, LgBodySize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_VoiceServer, &ControlCol, LgBodySize"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientKeywordReplyRuleInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "CUIRect Card35Start = Column;", "case EQmModuleId::PieMenu:");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext QmClientKeywordReplyTextInputCtx;");
	const size_t UiPos = Body.find("QmClientKeywordReplyTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("QmClientKeywordReplyTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("QmClientKeywordReplyTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("QmClientKeywordReplyTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_qmclient_keyword_reply_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("QmClientKeywordReplyTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);

	const size_t TriggerEmptyTextPos = Body.find("pRuleRow->m_TriggerInput.SetEmptyText(\"\");");
	const size_t ReplyEmptyTextPos = Body.find("pRuleRow->m_ReplyInput.SetEmptyText(\"\");", TriggerEmptyTextPos);
	const size_t TriggerTextFieldPos = Body.find("ui_widget::InputField(QmClientKeywordReplyTextInputCtx, &pRuleRow->m_TriggerInput, TriggerCol, \"\", LgBodySize);", ReplyEmptyTextPos);
	const size_t ReplyTextFieldPos = Body.find("ui_widget::InputField(QmClientKeywordReplyTextInputCtx, &pRuleRow->m_ReplyInput, ReplyCol, \"\", LgBodySize);", TriggerTextFieldPos);
	EXPECT_NE(TriggerEmptyTextPos, std::string::npos);
	EXPECT_NE(ReplyEmptyTextPos, std::string::npos);
	EXPECT_NE(TriggerTextFieldPos, std::string::npos);
	EXPECT_NE(ReplyTextFieldPos, std::string::npos);
	EXPECT_LT(TriggerEmptyTextPos, ReplyEmptyTextPos);
	EXPECT_LT(ReplyEmptyTextPos, TriggerTextFieldPos);
	EXPECT_LT(TriggerTextFieldPos, ReplyTextFieldPos);

	EXPECT_NE(Body.find("BuildAutoReplyRulesFromRows(s_vKeywordRuleRows, aKeywordRules, sizeof(aKeywordRules));"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&pRuleRow->m_TriggerInput, &TriggerCol, LgBodySize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&pRuleRow->m_ReplyInput, &ReplyCol, LgBodySize"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientBlockWordsReplacementInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "CUIRect CardBlockWordsStart = Column;", "case EQmModuleId::Translate:");
	ASSERT_FALSE(Body.empty());

	const size_t InputPos = Body.find("static CLineInputBuffered<8> s_BlockWordsReplaceInput;");
	const size_t EmptyTextPos = Body.find("s_BlockWordsReplaceInput.SetEmptyText(\"*\");", InputPos);
	const size_t CtxPos = Body.find("IUiContext QmClientBlockWordsTextInputCtx;", EmptyTextPos);
	const size_t UiPos = Body.find("QmClientBlockWordsTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("QmClientBlockWordsTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("QmClientBlockWordsTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("QmClientBlockWordsTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_qmclient_block_words_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("QmClientBlockWordsTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	const size_t TextFieldPos = Body.find("if(ui_widget::InputField(QmClientBlockWordsTextInputCtx, &s_BlockWordsReplaceInput, ControlCol, \"*\", LgBodySize))", FrameDtPos);
	EXPECT_NE(InputPos, std::string::npos);
	EXPECT_NE(EmptyTextPos, std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(InputPos, EmptyTextPos);
	EXPECT_LT(EmptyTextPos, CtxPos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	EXPECT_LT(FrameDtPos, TextFieldPos);

	EXPECT_NE(Body.find("str_utf8_truncate(aReplacement, sizeof(aReplacement), s_BlockWordsReplaceInput.GetString(), 1);"), std::string::npos);
	EXPECT_NE(Body.find("str_copy(g_Config.m_QmBlockWordsReplacementChar, aReplacement, sizeof(g_Config.m_QmBlockWordsReplacementChar));"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_BlockWordsReplaceInput, &ControlCol, LgBodySize"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientLyricsTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderQmHudLyricsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext QmClientLyricsTextInputCtx;");
	const size_t UiPos = Body.find("QmClientLyricsTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("QmClientLyricsTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("QmClientLyricsTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("QmClientLyricsTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_qmclient_lyrics_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("QmClientLyricsTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);

	const size_t LambdaPos = Body.find("auto RenderLyricTextInput = [&](CLineInput *pLineInput, const char *pTextId, const char *pLabel, char *pValue, size_t ValueSize, const char *pEmptyText) {", FrameDtPos);
	const size_t EmptyTextPos = Body.find("pLineInput->SetEmptyText(pEmptyText);", LambdaPos);
	const size_t TextFieldPos = Body.find("if(ui_widget::InputField(QmClientLyricsTextInputCtx, pLineInput, ControlColValue, pEmptyText, BodySize))", EmptyTextPos);
	const size_t WriteBackPos = Body.find("str_copy(pValue, pLineInput->GetString(), ValueSize);", TextFieldPos);
	EXPECT_NE(LambdaPos, std::string::npos);
	EXPECT_NE(EmptyTextPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(WriteBackPos, std::string::npos);
	EXPECT_LT(LambdaPos, EmptyTextPos);
	EXPECT_LT(EmptyTextPos, TextFieldPos);
	EXPECT_LT(TextFieldPos, WriteBackPos);

	EXPECT_NE(Body.find("RenderLyricTextInput(&s_QmLyricsSourceOrder"), std::string::npos);
	EXPECT_NE(Body.find("RenderLyricTextInput(&s_QmLyricsProviderThresholds"), std::string::npos);
	EXPECT_NE(Body.find("RenderLyricTextInput(&s_QmLyricsIgnoreCacheProviders"), std::string::npos);
	EXPECT_NE(Body.find("RenderLyricTextInput(&s_QmLyricsAppleMusicMediaUserToken"), std::string::npos);
	EXPECT_NE(Body.find("RenderLyricTextInput(&s_QmLyricsLocalMediaFolders"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(pLineInput, &ControlColValue, LgBodySize"), std::string::npos);
}

TEST(QmMonitoringHelpers, LaserPreviewDrawsWeaponBodiesBeforePreviewLaser)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::DoLaserPreview(const CUIRect *pRect, const ColorHSLA LaserOutlineColor, const ColorHSLA LaserInnerColor, const int LaserType)");
	ASSERT_FALSE(Body.empty());

	const size_t PreviewLaserPos = Body.find("GameClient()->m_Items.RenderLaser(From, Pos, OuterColor, InnerColor, 4.0f, TicksHead, LaserType, g_Config.m_QmLaserGlowIntensity);");
	const size_t RifleBodyPos = Body.find("Graphics()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);");
	const size_t ShotgunBodyPos = Body.find("Graphics()->DrawSprite(Section.x + 30.0f, Section.y + Section.h / 2.0f, 60.0f);", RifleBodyPos + 1);

	ASSERT_NE(PreviewLaserPos, std::string::npos);
	ASSERT_NE(RifleBodyPos, std::string::npos);
	ASSERT_NE(ShotgunBodyPos, std::string::npos);
	EXPECT_LT(RifleBodyPos, PreviewLaserPos);
	EXPECT_LT(ShotgunBodyPos, PreviewLaserPos);
}

TEST(QmMonitoringHelpers, LaserRoundCapsRenderedInBothEnhancedAndPlainPaths)
{
	const std::string Source = ReadRepoFile("src/game/client/components/items.cpp");
	ASSERT_FALSE(Source.empty());

	// RenderLaser splits into an enhanced-glow branch and a plain else branch.
	// Round caps must be drawn in BOTH paths so toggling QmLaserRoundCaps is
	// visible even without QmLaserEnhanced (the original bug: the toggle had
	// no visible effect unless enhancement was also enabled).
	const std::string RoundCapGuard = "if(g_Config.m_QmLaserRoundCaps)";
	size_t Pos = 0;
	int Count = 0;
	while((Pos = Source.find(RoundCapGuard, Pos)) != std::string::npos)
	{
		++Count;
		Pos += RoundCapGuard.size();
	}
	EXPECT_GE(Count, 2);
}

TEST(QmMonitoringHelpers, QmClientMainPageRemovesSearchSnapshotSignature)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Body.find("BuildQmFunctionSnapshotSignature"), std::string::npos);
	EXPECT_EQ(Body.find("BuildQmFunctionSnapshotEntries"), std::string::npos);
	EXPECT_EQ(Body.find("SnapshotEntry.m_EstimatedHeight"), std::string::npos);
	EXPECT_EQ(Body.find("FunctionSnapshotSignature"), std::string::npos);
	EXPECT_NE(Body.find("auto GetQmModuleEstimatedHeight = [&](const SQmModuleEntry *pModule) -> float"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientFlatDragKeepsPreviewHeightAndVisualsStable)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	const std::string UpdateDropPreview = ExtractSourceBlock(Body, "auto UpdateDropPreview = [&]()", "// 拖拽期间不做可视占位");
	const std::string ComputeDropPreviewLayout = ExtractSourceBlock(Body, "auto ComputeDropPreviewLayout = [&]()", "auto RenderDragGhost = [&]()");
	const std::string RenderDragGhost = ExtractSourceBlock(Body, "auto RenderDragGhost = [&]()", "auto CommitDropPreview = [&]()");
	const std::string RenderColumnModules = ExtractSourceBlock(Body, "auto RenderColumnModules = [&](const std::vector<const SQmModuleEntry *> &Modules, EQmModuleColumn ColumnId)", "if(ShowFooter)");
	ASSERT_FALSE(UpdateDropPreview.empty());
	ASSERT_FALSE(ComputeDropPreviewLayout.empty());
	ASSERT_FALSE(RenderDragGhost.empty());
	ASSERT_FALSE(RenderColumnModules.empty());

	EXPECT_NE(ComputeDropPreviewLayout.find("拖拽期间不做可视占位"), std::string::npos);
	EXPECT_NE(ComputeDropPreviewLayout.find("s_DropPreview.m_PreviewValid = false;"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("s_DropPreview.m_PreviewValid = true;"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("const float LastDraggedH = s_aQmModuleLastHeights[DraggedSI];"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("const float DraggedH = (LastDraggedH > 0.0f) ? LastDraggedH : std::max(s_DragState.m_DraggedHeight, GetQmModuleDefaultEstimatedHeight(*pDragged));"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("const float DraggedH = std::max(s_DragState.m_DraggedHeight, GetQmModuleDefaultEstimatedHeight(*pDragged));"), std::string::npos);
	EXPECT_NE(Body.find("const float DropPreviewDwellSeconds = 0.22f;"), std::string::npos);
	EXPECT_EQ(Body.find("DragHoldSeconds"), std::string::npos);
	EXPECT_NE(Body.find("struct SQmModuleDropPreviewCandidate"), std::string::npos);
	EXPECT_NE(Body.find("static SQmModuleDropPreviewCandidate s_DropPreviewCandidate"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("auto BuildDropPreviewCandidate"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("auto SameDropPreviewCandidate"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("auto ClearActiveDropPreview"), std::string::npos);
	EXPECT_EQ(UpdateDropPreview.find("const float DragCenterX = Ui()->MouseX() - s_DragState.m_GrabOffset.x + s_DragState.m_DraggedWidth * 0.5f;"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("const float DragHeaderX = Ui()->MouseX();"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("const float DragHeaderY = Ui()->MouseY() - s_DragState.m_GrabOffset.y + LgCardPadding + LgHeadlineSize * 0.5f;"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("TargetColumn = DragHeaderX <= ColumnSplitX ? EQmModuleColumn::Left : EQmModuleColumn::Right;"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("const CUIRect &StableRect = pCard->m_TargetRect;"), std::string::npos);
	EXPECT_NE(UpdateDropPreview.find("const float HeaderAnchorY = StableRect.y + LgCardPadding + LgHeadlineSize * 0.5f;"), std::string::npos);
	EXPECT_EQ(UpdateDropPreview.find("const float MidY = StableRect.y + StableRect.h * 0.5f;"), std::string::npos);
	EXPECT_LT(UpdateDropPreview.find("ClearActiveDropPreview();"), UpdateDropPreview.find("return;\n\t\t}\n\t\tif(Client()->LocalTime() - s_DropPreviewCandidate.m_StartedAt < DropPreviewDwellSeconds"));
	EXPECT_NE(UpdateDropPreview.find("Client()->LocalTime() - s_DropPreviewCandidate.m_StartedAt < DropPreviewDwellSeconds"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("const float Yield = DraggedH + LgCardSpacing;"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("Y += Yield"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("auto LayoutOneColumn"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("auto MakeRect"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("CUIRect DragObstacle"), std::string::npos);
	EXPECT_EQ(ComputeDropPreviewLayout.find("PackCardsAboveObstacle"), std::string::npos);
	EXPECT_EQ(Body.find("QmCardLayoutAnimate"), std::string::npos);
	EXPECT_NE(Body.find("auto QmModuleCardAnimRect = [&](CUIRect Rect)"), std::string::npos);
	EXPECT_NE(Body.find("Rect.y -= QmScrollFrame.m_Offset.y;"), std::string::npos);
	EXPECT_NE(Body.find("auto QmModuleCardScreenRect = [&](CUIRect Rect)"), std::string::npos);
	EXPECT_NE(Body.find("Rect.y += QmScrollFrame.m_Offset.y;"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect AnimTargetBeforeRender = QmModuleCardAnimRect(TargetBeforeRender);"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect AnimEffectiveTarget = QmModuleCardAnimRect(EffectiveTarget);"), std::string::npos);
	EXPECT_NE(Body.find("DispRect = QmModuleCardScreenRect(DispRect);"), std::string::npos);
	EXPECT_NE(Body.find("const bool AnimateCardLayout = s_DropPreview.m_PreviewValid && !TabTransitionActive;"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->UiRuntimeV2()->Tree().ResolveLayoutTransition(GameClient()->UiRuntimeV2()->AnimRuntime(), NodeKey, AnimEffectiveTarget, ui_token::motion::CARD_REORDER, 1, AnimateCardLayout);"), std::string::npos);
	EXPECT_NE(Body.find("BuildUiAnimNodeKey(str_quickhash(\"qm_module_card_layout\"), static_cast<uint64_t>(pModule->m_Id))"), std::string::npos);
	EXPECT_EQ(Body.find("static std::array<vec2, QmModuleCount> s_aQmModuleDisplayVelocities{};"), std::string::npos);
	EXPECT_EQ(Body.find("const float SpringDt = std::clamp(Client()->RenderFrameTime(), 0.0f, 1.0f / 30.0f);"), std::string::npos);
	EXPECT_EQ(Body.find("auto SpringAxis = [&](float &Pos, float &Velocity, float Target)"), std::string::npos);
	EXPECT_EQ(Body.find("SpringAxis(Disp.x, Velocity.x, EffectiveTarget.x);"), std::string::npos);
	EXPECT_EQ(Body.find("SpringAxis(Disp.y, Velocity.y, EffectiveTarget.y);"), std::string::npos);
	EXPECT_EQ(Body.find("const float LerpT = 0.25f;"), std::string::npos);
	EXPECT_EQ(Body.find("Disp.x += (EffectiveTarget.x - Disp.x) * LerpT;"), std::string::npos);
	EXPECT_EQ(Body.find("Disp.y += (EffectiveTarget.y - Disp.y) * LerpT;"), std::string::npos);

	EXPECT_NE(RenderDragGhost.find("// A2 修订（扁平化）：被拖卡和其他卡同层渲染"), std::string::npos);
	EXPECT_EQ(RenderDragGhost.find("Shadow.Draw"), std::string::npos);
	EXPECT_EQ(RenderDragGhost.find("DragGhostColor"), std::string::npos);
	EXPECT_EQ(RenderDragGhost.find("DrawDragOutline"), std::string::npos);
	EXPECT_EQ(Body.find("s_DropPreview.m_LineRect.Draw(DropPreviewColor"), std::string::npos);

	EXPECT_NE(RenderColumnModules.find("QmCardOffsetX = 0.0f;"), std::string::npos);
	EXPECT_NE(RenderColumnModules.find("QmCardOffsetY = 0.0f;"), std::string::npos);
	const std::string DisplayRectOffset = ExtractSourceBlock(RenderColumnModules, "// 布局过渡偏移", "CPerfTimer ModuleTimer;");
	ASSERT_FALSE(DisplayRectOffset.empty());
	EXPECT_EQ(RenderColumnModules.find("if(SkipOffscreenModule(pModule, Column))"), std::string::npos);
	EXPECT_NE(Body.find("auto EstimateModuleTotalHeight"), std::string::npos);
	EXPECT_EQ(DisplayRectOffset.find("if(s_DragState.m_pDragging != nullptr)"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("CUIRect &DispRect = s_aQmModuleDisplayRects[SI];"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("SyncLayoutTransition(GameClient()->UiRuntimeV2()->AnimRuntime(), NodeKey, QmModuleCardAnimRect(DragTarget));"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("ResolveLayoutTransition(GameClient()->UiRuntimeV2()->AnimRuntime(), NodeKey, AnimTargetBeforeRender, ui_token::motion::CARD_REORDER, 1, false);"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("const bool AnimateCardLayout = s_DropPreview.m_PreviewValid && !TabTransitionActive;"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("ResolveLayoutTransition(GameClient()->UiRuntimeV2()->AnimRuntime(), NodeKey, AnimEffectiveTarget, ui_token::motion::CARD_REORDER, 1, AnimateCardLayout);"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("DispRect = QmModuleCardScreenRect(DispRect);"), std::string::npos);
	EXPECT_EQ(DisplayRectOffset.find("GameClient()->UiRuntimeV2()->AnimRuntime().SetValue(NodeKey, EUiAnimProperty::POS_X"), std::string::npos);
	EXPECT_EQ(DisplayRectOffset.find("GameClient()->UiRuntimeV2()->AnimRuntime().SetValue(NodeKey, EUiAnimProperty::POS_Y"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("QmCardOffsetX = DispRect.x - Column.x;"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("DrawGlassCardBackground(DisplayBgRect);"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("const bool TargetVisible = IsSectionVisible(TargetBeforeRender, ModuleCullContext);"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("const bool DisplayVisible = DispRect.w > 0.0f && IsSectionVisible(DispRect, ModuleCullContext);"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("Column.y += EstimateModuleTotalHeight(pModule);"), std::string::npos);

	const size_t PreRenderDragPos = Body.find("auto UpdateDropPreviewBeforeRender = [&]()");
	const size_t FirstRenderColumnPos = Body.find("RenderColumnModules(VisibleLeftModules, EQmModuleColumn::Left);");
	ASSERT_NE(PreRenderDragPos, std::string::npos);
	ASSERT_NE(FirstRenderColumnPos, std::string::npos);
	EXPECT_LT(PreRenderDragPos, FirstRenderColumnPos);
	EXPECT_LT(Body.find("UpdateDropPreviewBeforeRender();"), FirstRenderColumnPos);
	EXPECT_NE(Body.find("s_DragState.m_pDragging = pModule;"), std::string::npos);
	const std::string UpdateDropPreviewBeforeRender = ExtractSourceBlock(Body, "auto UpdateDropPreviewBeforeRender = [&]()", "UpdateDropPreviewBeforeRender();");
	ASSERT_FALSE(UpdateDropPreviewBeforeRender.empty());
	EXPECT_EQ(UpdateDropPreviewBeforeRender.find("for(const SQmModuleCardInfo &Card : ModuleCards)"), std::string::npos);
	EXPECT_EQ(UpdateDropPreviewBeforeRender.find("TryBeginModuleDrag(Card.m_pModule, Card.m_Rect)"), std::string::npos);
	EXPECT_NE(DisplayRectOffset.find("TryBeginModuleDrag(pModule, TargetBeforeRender);"), std::string::npos);
	EXPECT_LT(DisplayRectOffset.find("TryBeginModuleDrag(pModule, TargetBeforeRender);"), DisplayRectOffset.find("CUIRect &DispRect = s_aQmModuleDisplayRects[SI];"));
	EXPECT_EQ(UpdateDropPreviewBeforeRender.find("s_DragState.m_pDragging = s_DragState.m_pPressed;"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmLayoutTransitionCacheIsOwnedByTree)
{
	const std::string AnimHeader = ReadRepoFile("src/game/client/QmUi/QmAnim.h");
	const std::string TreeHeader = ReadRepoFile("src/game/client/QmUi/QmTree.h");
	const std::string TreeSource = ReadRepoFile("src/game/client/QmUi/QmTree.cpp");
	EXPECT_NE(AnimHeader.find("ResolveTargetValue(uint64_t NodeKey, EUiAnimProperty Property, float Target, const SUiAnimTransition &Transition);"), std::string::npos);
	EXPECT_NE(AnimHeader.find("SResolveTargetState"), std::string::npos);
	EXPECT_NE(AnimHeader.find("m_LastTargets"), std::string::npos);
	EXPECT_NE(AnimHeader.find("m_ResolveUseCounter"), std::string::npos);
	EXPECT_EQ(AnimHeader.find("SFlipRectState"), std::string::npos);
	EXPECT_EQ(AnimHeader.find("m_FlipRects"), std::string::npos);
	EXPECT_EQ(AnimHeader.find("m_FlipUseCounter"), std::string::npos);
	EXPECT_NE(TreeHeader.find("ResolveLayoutTransition(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, const CUIRect &Target, const struct SUiSpringConfig &Spring, int Priority = 1, bool Animate = true);"), std::string::npos);
	EXPECT_NE(TreeHeader.find("m_LayoutTransitions"), std::string::npos);
	EXPECT_NE(TreeHeader.find("m_LayoutUseCounter"), std::string::npos);
	EXPECT_NE(TreeHeader.find("PruneLayoutTransitionCache"), std::string::npos);
	EXPECT_NE(TreeSource.find("ResolveLayoutTransition(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, const CUIRect &Target, const SUiSpringConfig &Spring, int Priority, bool Animate)"), std::string::npos);
	EXPECT_NE(TreeSource.find("|| !Animate"), std::string::npos);
	EXPECT_NE(TreeSource.find("AnimRuntime.ResolveTargetValue("), std::string::npos);
	EXPECT_NE(TreeSource.find("m_LayoutTransitions"), std::string::npos);
	EXPECT_NE(TreeSource.find("m_LayoutUseCounter"), std::string::npos);
	EXPECT_NE(TreeSource.find("PruneLayoutTransitionCache"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiAnimatePresenceIsGenericWidgetHelper)
{
	const std::string Context = ReadRepoFile("src/game/client/QmUi/UiContext.h");
	const std::string Overlays = ReadRepoFile("src/game/client/QmUi/UiOverlays.h");
	const std::string TreeHeader = ReadRepoFile("src/game/client/QmUi/QmTree.h");
	const std::string TreeSource = ReadRepoFile("src/game/client/QmUi/QmTree.cpp");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(Context.find("CUiV2Tree *m_pTree = nullptr;"), std::string::npos);
	EXPECT_NE(Overlays.find("SAnimatePresenceResult"), std::string::npos);
	EXPECT_NE(Overlays.find("AnimatePresence(const IUiContext &Ctx, const void *pId, bool Visible"), std::string::npos);
	EXPECT_NE(Overlays.find("Ctx.m_pTree->ResolvePresence(*Ctx.m_pAnim"), std::string::npos);
	EXPECT_NE(Overlays.find("const SAnimatePresenceResult Presence = AnimatePresence(Ctx, pId, Visible, ui_token::motion::TOAST_SLIDE);"), std::string::npos);
	EXPECT_NE(Overlays.find("const SAnimatePresenceResult Presence = AnimatePresence(Ctx, pId, *pOpen, ui_token::motion::MODAL_IN);"), std::string::npos);
	EXPECT_NE(TreeHeader.find("struct SUiPresenceResult"), std::string::npos);
	EXPECT_NE(TreeHeader.find("ResolvePresence(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, bool Visible"), std::string::npos);
	EXPECT_NE(TreeSource.find("ResolvePresence(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, bool Visible"), std::string::npos);
	EXPECT_NE(QmClient.find("Ctx.m_pTree = PrewarmOnly ? nullptr : &GameClient()->UiRuntimeV2()->Tree();"), std::string::npos);
}

TEST(QmMonitoringHelpers, InputFieldsConsumeSharedLayoutHelper)
{
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string Header = ReadRepoFile("src/game/client/QmUi/UiForms.h");
	const std::string InputBody = ExtractSourceFunctionBody(Forms, "SInputFieldResult InputField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const SInputFieldOptions &Options)");
	ASSERT_FALSE(InputBody.empty());

	EXPECT_NE(Header.find("struct SInputFieldLayout"), std::string::npos);
	EXPECT_NE(Header.find("ResolveInputFieldLayout("), std::string::npos);
	EXPECT_NE(Header.find("struct SInputFieldOptions"), std::string::npos);
	EXPECT_NE(InputBody.find("ResolveInputFieldLayout(Rect, HasIcon, Options.m_Clearable, Ctx.m_UiScale)"), std::string::npos);
	EXPECT_NE(InputBody.find("Layout.m_ContentRect"), std::string::npos);
	EXPECT_NE(InputBody.find("Layout.m_ClearRect"), std::string::npos);
	EXPECT_NE(InputBody.find("Options.m_Mode == EInputFieldMode::MULTILINE"), std::string::npos);
}
TEST(QmMonitoringHelpers, SettingsCardShellConsumesCanonicalVisualContract)
{
	const std::string Source = ReadRepoFile("src/game/client/QmUi/SettingsCard.cpp");

	EXPECT_NE(Source.find("State.m_DrawOffsetY"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_DrawAlpha"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_DropFeedback"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_ReflowCompleteFeedback"), std::string::npos);
	EXPECT_NE(Source.find("DrawState.m_Hovered || DrawState.m_Focused"), std::string::npos);
	EXPECT_NE(Source.find("VisualOptions.m_RainbowTitles"), std::string::npos);
	EXPECT_NE(Source.find("RenderCanonicalSettingsCardHandle("), std::string::npos);
}

TEST(QmMonitoringHelpers, GlobalSearchTargetsGraphicsCanonicalCard)
{
	const std::string SearchSource = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string SettingsSource = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string SearchCardBody = ExtractSourceFunctionBody(SearchSource, "void CMenus::RenderSettingsGlobalSearchContent(CUIRect MainView, bool PrewarmOnly)");
	const std::string GraphicsBody = ExtractSourceFunctionBody(SettingsSource, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(SearchCardBody.empty());
	ASSERT_FALSE(GraphicsBody.empty());

	EXPECT_NE(SearchCardBody.find("Navigation.m_SettingsPage == SETTINGS_GRAPHICS"), std::string::npos);
	EXPECT_NE(SearchCardBody.find("RequestSettingsCardFocus(pCard->m_pStableId);"), std::string::npos);
	EXPECT_NE(GraphicsBody.find("m_SettingsCardDeck.RequestReveal(m_SettingsCardFocusStableId.c_str());"), std::string::npos);
	EXPECT_NE(GraphicsBody.find("m_SettingsCardDeck.Render("), std::string::npos);
}
TEST(QmMonitoringHelpers, RegistryNavigationBridgeOwnsSettingsTarget)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string SetPageBody = ExtractSourceFunctionBody(Source, "bool CMenus::SetSettingsPageFromCardTab(const char *pTab)");
	const std::string NavigateBody = ExtractSourceFunctionBody(Source, "void CMenus::NavigateToSettingsCard(const qm_card_registry::SCardNavigationTarget &Target)");
	ASSERT_FALSE(SetPageBody.empty());
	ASSERT_FALSE(NavigateBody.empty());

	EXPECT_NE(Header.find("bool SetSettingsPageFromCardTab(const char *pTab);"), std::string::npos);
	EXPECT_NE(Header.find("void NavigateToSettingsCard(const qm_card_registry::SCardNavigationTarget &Target);"), std::string::npos);
	EXPECT_NE(SetPageBody.find("str_comp(pTab, \"graphics\") == 0"), std::string::npos);
	EXPECT_NE(SetPageBody.find("g_Config.m_UiSettingsPage = SETTINGS_GRAPHICS;"), std::string::npos);
	EXPECT_NE(SetPageBody.find("str_comp(pTab, \"appearance-hud\") == 0"), std::string::npos);
	EXPECT_NE(SetPageBody.find("return false;"), std::string::npos);
	EXPECT_NE(NavigateBody.find("SetSettingsPageFromCardTab(Target.m_pTab)"), std::string::npos);
	EXPECT_NE(NavigateBody.find("m_SettingsCardDeck.RequestReveal(Target.m_pStableId);"), std::string::npos);
}

// 意图：registry 的动态 Localize 标题也必须显式进入提取器，避免生成语言文件后 Search 回退英文。
TEST(QmMonitoringHelpers, RegistrySearchTitlesRemainLocalizationSourceKeys)
{
	const std::string Registry = ReadRepoFile("src/game/client/QmUi/QmCardRegistry.cpp");
	EXPECT_NE(Registry.find("Localizable(\"Graphics display\")"), std::string::npos);
	EXPECT_NE(Registry.find("Localizable(\"Visual\")"), std::string::npos);
	EXPECT_NE(Registry.find("Localizable(\"Graphics backend\")"), std::string::npos);
	EXPECT_NE(Registry.find("Localizable(\"Display modes\")"), std::string::npos);
}
TEST(QmMonitoringHelpers, GraphicsDeckUsesPublicCoordinator)
{
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string SettingsSource = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string SettingsBody = ExtractSourceFunctionBody(SettingsSource, "void CMenus::RenderSettings(CUIRect MainView)");
	const std::string GraphicsBody = ExtractSourceFunctionBody(SettingsSource, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(SettingsBody.empty());
	ASSERT_FALSE(GraphicsBody.empty());

	EXPECT_NE(MenusHeader.find("CSettingsCardDeck m_SettingsCardDeck;"), std::string::npos);
	EXPECT_NE(MenusHeader.find("m_SettingsCardDeckDisplayCycle"), std::string::npos);
	EXPECT_EQ(MenusHeader.find("m_GraphicsSettingsCardDeck"), std::string::npos);
	const size_t DisplayKey = SettingsBody.find("const uint64_t DisplayKey");
	ASSERT_NE(DisplayKey, std::string::npos);
	const size_t CycleGuard = SettingsBody.find("if(!CollectingMenuTextPlan &&", DisplayKey);
	const size_t BeginDisplayCycle = SettingsBody.find("m_SettingsCardDeck.BeginDisplayCycle", DisplayKey);
	ASSERT_NE(CycleGuard, std::string::npos);
	ASSERT_NE(BeginDisplayCycle, std::string::npos);
	EXPECT_LT(CycleGuard, BeginDisplayCycle);
	EXPECT_NE(SettingsBody.find("m_SettingsCardDeckDisplayKey != DisplayKey"), std::string::npos);
	EXPECT_EQ(GraphicsBody.find("m_SettingsCardDeck.BeginDisplayCycle"), std::string::npos);
	EXPECT_NE(GraphicsBody.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(GraphicsBody.find("SettingsCardOrderModel()"), std::string::npos);
	EXPECT_NE(GraphicsBody.find("SaveSettingsCardOrderModel()"), std::string::npos);
}

TEST(QmMonitoringHelpers, GraphicsDeckRemovesOnlyThePublicBridge)
{
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_EQ(Header.find("m_GraphicsSettingsCardDeck"), std::string::npos);
	EXPECT_EQ(Body.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(Body.find("BeginSettingsCardDeckCard("), std::string::npos);
	EXPECT_EQ(Body.find("EndSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(Body.find("RenderSettingsCardDeckDragOverlay("), std::string::npos);
}

TEST(QmMonitoringHelpers, P6TClientBindWheelMigrationUsesThePublicDeckOnly)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string BindWheelBody = ExtractSourceFunctionBody(TClient, "void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView, bool PrewarmOnly)");
	EXPECT_NE(QmClient.find("m_SettingsCardDeck.Render("), std::string::npos);
	ASSERT_FALSE(BindWheelBody.empty());
	EXPECT_NE(BindWheelBody.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(BindWheelBody.find("CSettingsCardDeck &CardDeck = ReadOnly ? s_BindWheelPrewarmDeck : m_SettingsCardDeck;"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("CardDeck.Render("), std::string::npos);
	EXPECT_NE(BindWheelBody.find("SettingsCardOrderModel()"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("CScrollRegion s_BindWheelSettingsScrollRegion"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("deck:tclient-bind-wheel-editor"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("deck:tclient-bind-wheel-preview"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("const bool ReadOnly = PrewarmOnly || Ui()->RenderOnly();"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("s_BindWheelPrewarmOrderModel.LoadMerged(\"\", qm_card_registry::BuildDefaultEntries())"), std::string::npos);
	EXPECT_NE(BindWheelBody.find("const float EditorContentHeight = maximum("), std::string::npos);
	EXPECT_NE(BindWheelBody.find("InputState.m_AllowHeaderDrag = !ReadOnly;"), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("BeginSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("BeginSettingsCardDeckCard("), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("EndSettingsCardDeck("), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("s_BindWheelSettingsScrollY"), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("s_BindWheelEditorCardHeight"), std::string::npos);
	EXPECT_EQ(BindWheelBody.find("s_BindWheelPreviewCardHeight"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6VisualQmClientMigrationUsesThePublicDeckOnly)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string VisualDeckBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientVisualDeck(CUIRect MainView, bool PrewarmOnly)");
	const std::string QmClientBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(VisualDeckBody.empty());
	ASSERT_FALSE(QmClientBody.empty());

	EXPECT_NE(VisualDeckBody.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("SettingsCardOrderModel()"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("QmResolveScrollPolicy"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("QmScrollRegionParamsFromPolicy"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:chat_bubble"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:camera_view"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:skin_transition"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:focus_mode"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:weapon_animation"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:streamer"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:entity_overlay"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:collision_hitbox"), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("qm:translate_ui"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("qm:card_appearance"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("ParseLegacyQmCollapsed"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("SerializeLegacyQmCollapsed"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("m_MeasureEachFrame = true"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("m_RenderMeasured"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("ContentHeight = std::max(0.0f, Content.y - StartY)"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("const FSettingsCardRenderMeasured &Render"), std::string::npos);
	EXPECT_NE(VisualDeckBody.find("(CUIRect &Content) { RenderQmVisual"), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("s_GlassCards"), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("BeginSettingsQmScrollContainer"), std::string::npos);

	const size_t VisualDispatch = QmClientBody.find("RenderSettingsQmClientVisualDeck(ContentView, PrewarmOnly)");
	const size_t LegacyScroll = QmClientBody.find("BeginSettingsQmScrollContainer");
	ASSERT_NE(VisualDispatch, std::string::npos);
	ASSERT_NE(LegacyScroll, std::string::npos);
	EXPECT_LT(VisualDispatch, LegacyScroll);
}

TEST(QmMonitoringHelpers, P6VisualContentExtractionKeepsLegacyRendererAsTheOnlyShellOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string StreamerBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualStreamerContent(CUIRect &Content, float LineHeight, float LineSpacing)");
	const std::string TranslateUiBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualTranslateUiContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing)");
	const std::string CardAppearanceBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualCardAppearanceContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string EntityOverlayBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualEntityOverlayContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string CollisionHitboxBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualCollisionHitboxContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string WeaponAnimationBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualWeaponAnimationContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, float ContentGap, bool PrewarmOnly)");
	const std::string ChatBubbleBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualChatBubbleContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string SkinTransitionBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualSkinTransitionContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string FocusModeBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualFocusModeContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float ColumnGap, float LabelWidth)");
	const std::string CameraViewBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmVisualCameraViewContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	ASSERT_FALSE(StreamerBody.empty());
	ASSERT_FALSE(TranslateUiBody.empty());
	ASSERT_FALSE(CardAppearanceBody.empty());
	ASSERT_FALSE(EntityOverlayBody.empty());
	ASSERT_FALSE(CollisionHitboxBody.empty());
	ASSERT_FALSE(WeaponAnimationBody.empty());
	ASSERT_FALSE(ChatBubbleBody.empty());
	ASSERT_FALSE(SkinTransitionBody.empty());
	ASSERT_FALSE(FocusModeBody.empty());
	ASSERT_FALSE(CameraViewBody.empty());

	EXPECT_NE(Header.find("RenderQmVisualStreamerContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualTranslateUiContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualCardAppearanceContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualEntityOverlayContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualCollisionHitboxContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualWeaponAnimationContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualChatBubbleContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualSkinTransitionContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualFocusModeContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmVisualCameraViewContent"), std::string::npos);
	EXPECT_NE(Header.find("IsQmNewFeatureRead"), std::string::npos);
	EXPECT_NE(StreamerBody.find("RenderQmVisualCheckbox"), std::string::npos);
	EXPECT_NE(TranslateUiBody.find("NTranslateUiSettings::RenderTranslateUiModule"), std::string::npos);
	EXPECT_NE(CardAppearanceBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(EntityOverlayBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(CollisionHitboxBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(WeaponAnimationBody.find("MarkQmNewFeatureHovered"), std::string::npos);
	EXPECT_NE(ChatBubbleBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(SkinTransitionBody.find("MarkQmNewFeatureHovered"), std::string::npos);
	EXPECT_NE(SkinTransitionBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("toggle qm_focus_mode 0 1"), std::string::npos);
	EXPECT_NE(FocusModeBody.find("g_CommandBindCache"), std::string::npos);
	EXPECT_NE(CameraViewBody.find("QueueAspectApply"), std::string::npos);
	EXPECT_NE(CameraViewBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(StreamerBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(TranslateUiBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(CardAppearanceBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(EntityOverlayBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(CollisionHitboxBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(WeaponAnimationBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(ChatBubbleBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(SkinTransitionBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(FocusModeBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(CameraViewBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionKeyBindsContentExtractionKeepsTheLegacyShellAsOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string KeyBindsBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionKeyBindsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth)");
	const size_t LegacyCaseStart = QmClient.rfind("case EQmModuleId::KeyBinds:");
	const size_t LegacyCaseEnd = QmClient.find("case EQmModuleId::MiniFeatures:", LegacyCaseStart);
	const std::string LegacyCase = LegacyCaseStart != std::string::npos && LegacyCaseEnd != std::string::npos ? QmClient.substr(LegacyCaseStart, LegacyCaseEnd - LegacyCaseStart) : "";
	ASSERT_FALSE(KeyBindsBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionKeyBindsContent"), std::string::npos);
	EXPECT_NE(KeyBindsBody.find("RenderQmHudKeyBindRow"), std::string::npos);
	EXPECT_NE(KeyBindsBody.find("+toggle cl_dummy_hammer 1 0"), std::string::npos);
	EXPECT_NE(KeyBindsBody.find("qm_timeout_disconnect"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmFunctionKeyBindsContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth);"), std::string::npos);
	EXPECT_EQ(LegacyCase.find("DoKeyBindRow(CardContent"), std::string::npos);
	EXPECT_EQ(KeyBindsBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionGoresActorContentExtractionKeepsInputAndConditionalControls)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string GoresActorBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionGoresActorContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t LegacyCaseStart = QmClient.rfind("case EQmModuleId::GoresActor:");
	const size_t LegacyCaseEnd = QmClient.find("case EQmModuleId::Gores:", LegacyCaseStart);
	const std::string LegacyCase = LegacyCaseStart != std::string::npos && LegacyCaseEnd != std::string::npos ? QmClient.substr(LegacyCaseStart, LegacyCaseEnd - LegacyCaseStart) : "";
	ASSERT_FALSE(GoresActorBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionGoresActorContent"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("settings_qmclient_gores_actor_text_inputs"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("ui_widget::InputField"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("m_TcFreezeChatEnabled"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("m_TcFreezeChatEmoticon"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("m_TcFreezeChatEmoticonId"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("m_TcFreezeChatChance"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmFunctionGoresActorContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth, PrewarmOnly);"), std::string::npos);
	EXPECT_NE(GoresActorBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_EQ(GoresActorBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionShortCardContentExtractionKeepsPublicControlsAndDynamicBranches)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string JumpHintBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionJumpHintContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string WeaponTrajectoryBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionWeaponTrajectoryContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t JumpHintStart = QmClient.rfind("case EQmModuleId::JumpHint:");
	const size_t JumpHintEnd = QmClient.find("case EQmModuleId::WeaponTrajectory:", JumpHintStart);
	const size_t WeaponTrajectoryStart = QmClient.rfind("case EQmModuleId::WeaponTrajectory:");
	const size_t WeaponTrajectoryEnd = QmClient.find("case EQmModuleId::SkinTransition:", WeaponTrajectoryStart);
	const std::string JumpHintCase = JumpHintStart != std::string::npos && JumpHintEnd != std::string::npos ? QmClient.substr(JumpHintStart, JumpHintEnd - JumpHintStart) : "";
	const std::string WeaponTrajectoryCase = WeaponTrajectoryStart != std::string::npos && WeaponTrajectoryEnd != std::string::npos ? QmClient.substr(WeaponTrajectoryStart, WeaponTrajectoryEnd - WeaponTrajectoryStart) : "";
	ASSERT_FALSE(JumpHintBody.empty());
	ASSERT_FALSE(WeaponTrajectoryBody.empty());
	ASSERT_FALSE(JumpHintCase.empty());
	ASSERT_FALSE(WeaponTrajectoryCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionJumpHintContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmFunctionWeaponTrajectoryContent"), std::string::npos);
	EXPECT_NE(JumpHintBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(JumpHintBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(JumpHintBody.find("m_QmJumpHintSize"), std::string::npos);
	EXPECT_NE(WeaponTrajectoryBody.find("DoDropDown"), std::string::npos);
	EXPECT_NE(WeaponTrajectoryBody.find("m_QmWeaponTrajectory == 0"), std::string::npos);
	EXPECT_NE(WeaponTrajectoryBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(WeaponTrajectoryBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(JumpHintCase.find("RenderQmFunctionJumpHintContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth, PrewarmOnly);"), std::string::npos);
	EXPECT_NE(WeaponTrajectoryCase.find("RenderQmFunctionWeaponTrajectoryContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth, PrewarmOnly);"), std::string::npos);
	EXPECT_EQ(JumpHintBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(WeaponTrajectoryBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionFriendNotifyContentExtractionKeepsDynamicInputs)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string FriendNotifyBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionFriendNotifyContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t LegacyCaseStart = QmClient.rfind("case EQmModuleId::FriendNotify:");
	const size_t LegacyCaseEnd = QmClient.find("case EQmModuleId::BlockWords:", LegacyCaseStart);
	const std::string LegacyCase = LegacyCaseStart != std::string::npos && LegacyCaseEnd != std::string::npos ? QmClient.substr(LegacyCaseStart, LegacyCaseEnd - LegacyCaseStart) : "";
	ASSERT_FALSE(FriendNotifyBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionFriendNotifyContent"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("settings_qmclient_friend_enter_text_inputs"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("m_QmFriendOnlineAutoRefresh"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("m_QmFriendEnterBroadcast"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("m_QmFriendEnterAutoGreet"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("ui_widget::InputField"), std::string::npos);
	EXPECT_NE(FriendNotifyBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmFunctionFriendNotifyContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth, PrewarmOnly);"), std::string::npos);
	EXPECT_EQ(FriendNotifyBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionMiniFeaturesAndHJAssistContentExtractionKeepsInteractionState)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string MiniFeaturesBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionMiniFeaturesContent(CUIRect &Content, float LineHeight, float LineSpacing, bool PrewarmOnly)");
	const std::string HJAssistBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionHJAssistContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t MiniFeaturesStart = QmClient.rfind("case EQmModuleId::MiniFeatures:");
	const size_t MiniFeaturesEnd = QmClient.find("case EQmModuleId::JumpHint:", MiniFeaturesStart);
	const size_t HJAssistStart = QmClient.rfind("case EQmModuleId::HJAssist:");
	const size_t HJAssistEnd = QmClient.find("case EQmModuleId::SpeedrunTimer:", HJAssistStart);
	const std::string MiniFeaturesCase = MiniFeaturesStart != std::string::npos && MiniFeaturesEnd != std::string::npos ? QmClient.substr(MiniFeaturesStart, MiniFeaturesEnd - MiniFeaturesStart) : "";
	const std::string HJAssistCase = HJAssistStart != std::string::npos && HJAssistEnd != std::string::npos ? QmClient.substr(HJAssistStart, HJAssistEnd - HJAssistStart) : "";
	ASSERT_FALSE(MiniFeaturesBody.empty());
	ASSERT_FALSE(HJAssistBody.empty());
	ASSERT_FALSE(MiniFeaturesCase.empty());
	ASSERT_FALSE(HJAssistCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionMiniFeaturesContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmFunctionHJAssistContent"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("m_QmNewIme"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("qm_2_63_0_new_ime"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("MarkQmNewFeatureHovered"), std::string::npos);
	EXPECT_NE(MiniFeaturesBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_NE(HJAssistBody.find("m_QmAutoTeamLock"), std::string::npos);
	EXPECT_NE(HJAssistBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(HJAssistBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_NE(MiniFeaturesCase.find("RenderQmFunctionMiniFeaturesContent(CardContent, LgLineHeight, LgLineSpacing, PrewarmOnly);"), std::string::npos);
	EXPECT_NE(HJAssistCase.find("RenderQmFunctionHJAssistContent(CardContent, LgLineHeight, LgBodySize, LgLineSpacing, LgLabelWidth, PrewarmOnly);"), std::string::npos);
	EXPECT_EQ(MiniFeaturesBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(HJAssistBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionGoresContentExtractionPreservesTheLightPathAndInputState)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string GoresBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionGoresContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly, bool LightFirstFrame)");
	ASSERT_FALSE(GoresBody.empty());

	EXPECT_NE(Header.find("RenderQmFunctionGoresContent"), std::string::npos);
	EXPECT_NE(GoresBody.find("if(LightFirstFrame)"), std::string::npos);
	EXPECT_NE(GoresBody.find("settings_qmclient_gores_text_inputs"), std::string::npos);
	EXPECT_NE(GoresBody.find("s_AxiomLoginPassword"), std::string::npos);
	EXPECT_NE(GoresBody.find("s_AxiomDummyLoginPassword"), std::string::npos);
	EXPECT_NE(GoresBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_NE(GoresBody.find("toggle qm_gores 0 1"), std::string::npos);
	EXPECT_EQ(GoresBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionBlockWordsContentExtractionPreservesItsLightPathAndInputContracts)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string BlockWordsBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionBlockWordsContent(CUIRect &Content, float UiScale, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly, bool LightFirstFrame)");
	ASSERT_FALSE(BlockWordsBody.empty());

	EXPECT_NE(Header.find("RenderQmFunctionBlockWordsContent"), std::string::npos);
	EXPECT_NE(BlockWordsBody.find("if(LightFirstFrame)"), std::string::npos);
	EXPECT_NE(BlockWordsBody.find("str_utf8_truncate"), std::string::npos);
	EXPECT_NE(BlockWordsBody.find("EInputFieldMode::MULTILINE"), std::string::npos);
	EXPECT_NE(BlockWordsBody.find("CalcQiaFenInputHeight"), std::string::npos);
	EXPECT_NE(BlockWordsBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_EQ(BlockWordsBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionKeywordReplyContentExtractionKeepsRuleRowsAndConfigCodec)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string KeywordReplyBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionKeywordReplyContent(CUIRect &Content, float UiScale, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	ASSERT_FALSE(KeywordReplyBody.empty());

	EXPECT_NE(Header.find("RenderQmFunctionKeywordReplyContent"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("ParseAutoReplyRules"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("AutoReplyRowsMatchRules"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("QmKeywordReplyRules::DecodeFromConfig"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("QmKeywordReplyRules::EncodeForConfig"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("RenderQmFunctionCheckbox"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(KeywordReplyBody.find("IsAutoReplyRuleRowHalfFilled"), std::string::npos);
	EXPECT_EQ(KeywordReplyBody.find("RegisterModuleCard"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudSpeedrunContentExtractionKeepsLegacyRendererAsTheOnlyShellOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string SpeedrunBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudSpeedrunTimerContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t LegacyCaseStart = QmClient.rfind("case EQmModuleId::SpeedrunTimer:");
	const size_t LegacyCaseEnd = QmClient.find("case EQmModuleId::DebugGraph:", LegacyCaseStart);
	const std::string LegacyCase = LegacyCaseStart != std::string::npos && LegacyCaseEnd != std::string::npos ? QmClient.substr(LegacyCaseStart, LegacyCaseEnd - LegacyCaseStart) : "";
	ASSERT_FALSE(SpeedrunBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(Header.find("RenderQmHudSpeedrunTimerContent"), std::string::npos);
	EXPECT_NE(SpeedrunBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(SpeedrunBody.find("m_QmSpeedrunTimerAutoDisable"), std::string::npos);
	EXPECT_EQ(SpeedrunBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(SpeedrunBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmHudSpeedrunTimerContent"), std::string::npos);
	EXPECT_EQ(LegacyCase.find("RenderSliderWithValueInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudBasicContentExtractionsKeepLegacyRendererAsTheOnlyShellOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string DebugGraphBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudDebugGraphContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string InputOverlayBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudInputOverlayContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string DummyMiniViewBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudDummyMiniViewContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t DebugGraphStart = QmClient.rfind("case EQmModuleId::DebugGraph:");
	const size_t DebugGraphEnd = QmClient.find("case EQmModuleId::InputOverlay:", DebugGraphStart);
	const size_t InputOverlayStart = QmClient.rfind("case EQmModuleId::InputOverlay:");
	const size_t InputOverlayEnd = QmClient.find("case EQmModuleId::HudNotifications:", InputOverlayStart);
	const size_t DummyMiniViewStart = QmClient.rfind("case EQmModuleId::DummyMiniView:");
	const size_t DummyMiniViewEnd = QmClient.find("case EQmModuleId::Background3D:", DummyMiniViewStart);
	const std::string DebugGraphCase = DebugGraphStart != std::string::npos && DebugGraphEnd != std::string::npos ? QmClient.substr(DebugGraphStart, DebugGraphEnd - DebugGraphStart) : "";
	const std::string InputOverlayCase = InputOverlayStart != std::string::npos && InputOverlayEnd != std::string::npos ? QmClient.substr(InputOverlayStart, InputOverlayEnd - InputOverlayStart) : "";
	const std::string DummyMiniViewCase = DummyMiniViewStart != std::string::npos && DummyMiniViewEnd != std::string::npos ? QmClient.substr(DummyMiniViewStart, DummyMiniViewEnd - DummyMiniViewStart) : "";
	ASSERT_FALSE(DebugGraphBody.empty());
	ASSERT_FALSE(InputOverlayBody.empty());
	ASSERT_FALSE(DummyMiniViewBody.empty());
	ASSERT_FALSE(DebugGraphCase.empty());
	ASSERT_FALSE(InputOverlayCase.empty());
	ASSERT_FALSE(DummyMiniViewCase.empty());

	EXPECT_NE(Header.find("RenderQmHudCheckbox"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudDebugGraphContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudInputOverlayContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudDummyMiniViewContent"), std::string::npos);
	EXPECT_NE(DebugGraphBody.find("RenderQmHudKeyBindRow"), std::string::npos);
	EXPECT_NE(InputOverlayBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_NE(DummyMiniViewBody.find("RenderQmHudCheckbox"), std::string::npos);
	EXPECT_EQ(DebugGraphBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(InputOverlayBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_EQ(DummyMiniViewBody.find("s_GlassCards"), std::string::npos);
	EXPECT_NE(DebugGraphCase.find("RenderQmHudDebugGraphContent"), std::string::npos);
	EXPECT_NE(InputOverlayCase.find("RenderQmHudInputOverlayContent"), std::string::npos);
	EXPECT_NE(DummyMiniViewCase.find("RenderQmHudDummyMiniViewContent"), std::string::npos);
	EXPECT_EQ(DebugGraphCase.find("RenderSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(InputOverlayCase.find("RenderSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(DummyMiniViewCase.find("RenderSliderWithValueInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudMediaContentExtractionsKeepLegacyRendererAsTheOnlyShellOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string DynamicIslandBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudDynamicIslandContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string SmtcBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudSystemMediaControlsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, bool PrewarmOnly)");
	const size_t DynamicIslandStart = QmClient.rfind("case EQmModuleId::DynamicIsland:");
	const size_t DynamicIslandEnd = QmClient.find("case EQmModuleId::SystemMediaControls:", DynamicIslandStart);
	const size_t SmtcStart = QmClient.rfind("case EQmModuleId::SystemMediaControls:");
	const size_t SmtcEnd = QmClient.find("case EQmModuleId::Lyrics:", SmtcStart);
	const std::string DynamicIslandCase = DynamicIslandStart != std::string::npos && DynamicIslandEnd != std::string::npos ? QmClient.substr(DynamicIslandStart, DynamicIslandEnd - DynamicIslandStart) : "";
	const std::string SmtcCase = SmtcStart != std::string::npos && SmtcEnd != std::string::npos ? QmClient.substr(SmtcStart, SmtcEnd - SmtcStart) : "";
	ASSERT_FALSE(DynamicIslandBody.empty());
	ASSERT_FALSE(SmtcBody.empty());
	ASSERT_FALSE(DynamicIslandCase.empty());
	ASSERT_FALSE(SmtcCase.empty());

	EXPECT_NE(DynamicIslandBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(SmtcBody.find("m_SystemMediaControls.PlayPause"), std::string::npos);
	EXPECT_EQ(DynamicIslandBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(SmtcBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_NE(DynamicIslandCase.find("RenderQmHudDynamicIslandContent"), std::string::npos);
	EXPECT_NE(SmtcCase.find("RenderQmHudSystemMediaControlsContent"), std::string::npos);
	EXPECT_EQ(DynamicIslandCase.find("RenderSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(SmtcCase.find("DoQmSettingsMenuButton"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudNotificationsContentExtractionKeepsDynamicOptionsMeasured)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string BasicBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudNotificationsBasicContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string AdvancedBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudNotificationsAdvancedContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t CaseStart = QmClient.rfind("case EQmModuleId::HudNotifications:");
	const size_t CaseEnd = QmClient.find("case EQmModuleId::Voice:", CaseStart);
	const std::string LegacyCase = CaseStart != std::string::npos && CaseEnd != std::string::npos ? QmClient.substr(CaseStart, CaseEnd - CaseStart) : "";
	ASSERT_FALSE(BasicBody.empty());
	ASSERT_FALSE(AdvancedBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(BasicBody.find("m_QmHudNotificationsShowAdvanced"), std::string::npos);
	EXPECT_NE(AdvancedBody.find("m_QmHudNotificationsUseCategoryFilters"), std::string::npos);
	EXPECT_NE(AdvancedBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(AdvancedBody.find("DoDropDown"), std::string::npos);
	EXPECT_NE(AdvancedBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(BasicBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(AdvancedBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmHudNotificationsBasicContent"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmHudNotificationsAdvancedContent"), std::string::npos);
	EXPECT_EQ(QmClient.find("RenderHudNotificationsBasicSettings"), std::string::npos);
	EXPECT_EQ(QmClient.find("RenderHudNotificationsAdvancedSettings"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudPlayerStatsContentExtractionKeepsProgressBranchesMeasured)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string PlayerStatsBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudPlayerStatsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t CaseStart = QmClient.rfind("case EQmModuleId::PlayerStats:");
	const size_t CaseEnd = QmClient.find("case EQmModuleId::CollisionHitbox:", CaseStart);
	const std::string LegacyCase = CaseStart != std::string::npos && CaseEnd != std::string::npos ? QmClient.substr(CaseStart, CaseEnd - CaseStart) : "";
	ASSERT_FALSE(PlayerStatsBody.empty());
	ASSERT_FALSE(LegacyCase.empty());

	EXPECT_NE(PlayerStatsBody.find("m_QmPlayerStatsMapProgress"), std::string::npos);
	EXPECT_NE(PlayerStatsBody.find("m_QmPlayerStatsMapProgressStyle"), std::string::npos);
	EXPECT_NE(PlayerStatsBody.find("DoLine_ColorPicker"), std::string::npos);
	EXPECT_NE(PlayerStatsBody.find("RenderQmSettingsSliderWithValueInput"), std::string::npos);
	EXPECT_EQ(PlayerStatsBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(PlayerStatsBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_NE(LegacyCase.find("RenderQmHudPlayerStatsContent"), std::string::npos);
	EXPECT_EQ(LegacyCase.find("RenderSliderWithValueInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudRemainingContentExtractionsPreserveDynamicControls)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string FormsHeader = ReadRepoFile("src/game/client/QmUi/UiForms.h");
	const std::string CoordsBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudCoordsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string VoiceBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudVoiceContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, float UiScale, bool PrewarmOnly)");
	const std::string BackgroundBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudBackground3DContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const std::string LyricsBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmHudLyricsContent(CUIRect &Content, float LineHeight, float BodySize, float LineSpacing, float LabelWidth, bool PrewarmOnly)");
	const size_t CoordsStart = QmClient.rfind("case EQmModuleId::Coords:");
	const size_t CoordsEnd = QmClient.find("case EQmModuleId::Streamer:", CoordsStart);
	const size_t VoiceStart = QmClient.rfind("case EQmModuleId::Voice:");
	const size_t VoiceEnd = QmClient.find("case EQmModuleId::DummyMiniView:", VoiceStart);
	const size_t BackgroundStart = QmClient.rfind("case EQmModuleId::Background3D:");
	const size_t BackgroundEnd = QmClient.find("case EQmModuleId::DynamicIsland:", BackgroundStart);
	const size_t LyricsStart = QmClient.rfind("case EQmModuleId::Lyrics:");
	const size_t LyricsEnd = QmClient.find("default:", LyricsStart);
	const std::string CoordsCase = CoordsStart != std::string::npos && CoordsEnd != std::string::npos ? QmClient.substr(CoordsStart, CoordsEnd - CoordsStart) : "";
	const std::string VoiceCase = VoiceStart != std::string::npos && VoiceEnd != std::string::npos ? QmClient.substr(VoiceStart, VoiceEnd - VoiceStart) : "";
	const std::string BackgroundCase = BackgroundStart != std::string::npos && BackgroundEnd != std::string::npos ? QmClient.substr(BackgroundStart, BackgroundEnd - BackgroundStart) : "";
	const std::string LyricsCase = LyricsStart != std::string::npos && LyricsEnd != std::string::npos ? QmClient.substr(LyricsStart, LyricsEnd - LyricsStart) : "";
	ASSERT_FALSE(CoordsBody.empty());
	ASSERT_FALSE(VoiceBody.empty());
	ASSERT_FALSE(BackgroundBody.empty());
	ASSERT_FALSE(LyricsBody.empty());
	ASSERT_FALSE(CoordsCase.empty());
	ASSERT_FALSE(VoiceCase.empty());
	ASSERT_FALSE(BackgroundCase.empty());
	ASSERT_FALSE(LyricsCase.empty());

	EXPECT_NE(Header.find("RenderQmHudCoordsContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudVoiceContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudBackground3DContent"), std::string::npos);
	EXPECT_NE(Header.find("RenderQmHudLyricsContent"), std::string::npos);
	EXPECT_NE(FormsHeader.find("int m_ValueStep = 1;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("QuantizeNumericFieldStoredValue"), std::string::npos);
	EXPECT_NE(Forms.find("QuantizeNumericFieldStoredValue"), std::string::npos);
	EXPECT_NE(CoordsBody.find("Options.m_ValueStep = 100;"), std::string::npos);
	EXPECT_NE(CoordsBody.find("ui_widget::NumericField"), std::string::npos);
	EXPECT_EQ(CoordsBody.find("DoScrollbarH"), std::string::npos);
	EXPECT_NE(VoiceBody.find("ExportUiStatus"), std::string::npos);
	EXPECT_NE(VoiceBody.find("m_QmVoiceShowAdvanced"), std::string::npos);
	EXPECT_NE(BackgroundBody.find("m_Qm3DParticles"), std::string::npos);
	EXPECT_NE(BackgroundBody.find("DoDropDown"), std::string::npos);
	EXPECT_NE(LyricsBody.find("m_QmSmtcLyricsEnable"), std::string::npos);
	EXPECT_NE(LyricsBody.find("RenderLyricTextInput"), std::string::npos);
	EXPECT_NE(CoordsCase.find("RenderQmHudCoordsContent"), std::string::npos);
	EXPECT_NE(VoiceCase.find("RenderQmHudVoiceContent"), std::string::npos);
	EXPECT_NE(BackgroundCase.find("RenderQmHudBackground3DContent"), std::string::npos);
	EXPECT_NE(LyricsCase.find("RenderQmHudLyricsContent"), std::string::npos);
	EXPECT_EQ(CoordsCase.find("DoScrollbarH"), std::string::npos);
	EXPECT_EQ(VoiceCase.find("ExportUiStatus"), std::string::npos);
	EXPECT_EQ(BackgroundCase.find("RenderIntOption"), std::string::npos);
	EXPECT_EQ(LyricsCase.find("RenderLyricTextInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6HudQmClientMigrationUsesThePublicDeckOnly)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string HudDeckBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientHudDeck(CUIRect MainView, bool PrewarmOnly)");
	const std::string QmClientBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(HudDeckBody.empty());
	ASSERT_FALSE(QmClientBody.empty());

	EXPECT_NE(HudDeckBody.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(HudDeckBody.find("SettingsCardOrderModel()"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("QmResolveScrollPolicy"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("QmScrollRegionParamsFromPolicy"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("qm:dummy_miniview"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("qm:coords"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("qm:voice"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("qm:lyrics"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("qm:background_3d"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("ParseLegacyQmCollapsed"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("SerializeLegacyQmCollapsed"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("m_MeasureEachFrame = true"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("m_RenderMeasured"), std::string::npos);
	EXPECT_NE(HudDeckBody.find("ContentHeight = std::max(0.0f, Content.y - StartY)"), std::string::npos);
	EXPECT_EQ(HudDeckBody.find("s_GlassCards"), std::string::npos);
	EXPECT_EQ(HudDeckBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(HudDeckBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_EQ(HudDeckBody.find("BeginSettingsQmScrollContainer"), std::string::npos);

	const size_t HudDispatch = QmClientBody.find("RenderSettingsQmClientHudDeck(ContentView, PrewarmOnly)");
	const size_t LegacyScroll = QmClientBody.find("BeginSettingsQmScrollContainer");
	ASSERT_NE(HudDispatch, std::string::npos);
	ASSERT_NE(LegacyScroll, std::string::npos);
	EXPECT_LT(HudDispatch, LegacyScroll);
}

TEST(QmMonitoringHelpers, P6FunctionFavoriteMapsKeepsItsIndependentContentOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string OwnerBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionFavoriteMapsContent(");
	const std::string FunctionPageBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const size_t FavoriteMapsStart = FunctionPageBody.rfind("case EQmModuleId::FavoriteMaps:");
	const size_t FavoriteMapsEnd = FunctionPageBody.find("case EQmModuleId::HJAssist:", FavoriteMapsStart);
	const std::string FavoriteMapsCase = FavoriteMapsStart != std::string::npos && FavoriteMapsEnd != std::string::npos ? FunctionPageBody.substr(FavoriteMapsStart, FavoriteMapsEnd - FavoriteMapsStart) : std::string();
	ASSERT_FALSE(OwnerBody.empty());
	ASSERT_FALSE(FavoriteMapsCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionFavoriteMapsContent"), std::string::npos);
	EXPECT_NE(OwnerBody.find("constexpr int ServersPerFrame = 64"), std::string::npos);
	EXPECT_NE(OwnerBody.find("UpdateMapCategoryCache"), std::string::npos);
	EXPECT_NE(OwnerBody.find("GetCachedMapCategoryKey"), std::string::npos);
	EXPECT_NE(OwnerBody.find("SetClipboardText"), std::string::npos);
	EXPECT_NE(OwnerBody.find("RemoveFavoriteMap"), std::string::npos);
	EXPECT_NE(FavoriteMapsCase.find("RenderQmFunctionFavoriteMapsContent"), std::string::npos);
	EXPECT_EQ(FavoriteMapsCase.find("ServersPerFrame"), std::string::npos);
	EXPECT_EQ(FavoriteMapsCase.find("GetCachedMapCategoryKey"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionPieMenuKeepsPopupPreviewInItsContentOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string OwnerBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionPieMenuContent(");
	const std::string FunctionPageBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const size_t PieMenuStart = FunctionPageBody.rfind("case EQmModuleId::PieMenu:");
	const size_t PieMenuEnd = FunctionPageBody.find("case EQmModuleId::CameraView:", PieMenuStart);
	const std::string PieMenuCase = PieMenuStart != std::string::npos && PieMenuEnd != std::string::npos ? FunctionPageBody.substr(PieMenuStart, PieMenuEnd - PieMenuStart) : std::string();
	ASSERT_FALSE(OwnerBody.empty());
	ASSERT_FALSE(PieMenuCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionPieMenuContent"), std::string::npos);
	EXPECT_NE(OwnerBody.find("LightFirstFrame"), std::string::npos);
	EXPECT_NE(OwnerBody.find("ShowPopupColorPicker"), std::string::npos);
	EXPECT_NE(OwnerBody.find("DoButtonLogic"), std::string::npos);
	EXPECT_NE(OwnerBody.find("DrawCircle"), std::string::npos);
	EXPECT_NE(OwnerBody.find("qmclient-pie-menu-reset-colors"), std::string::npos);
	EXPECT_NE(PieMenuCase.find("RenderQmFunctionPieMenuContent"), std::string::npos);
	EXPECT_EQ(PieMenuCase.find("ShowPopupColorPicker"), std::string::npos);
	EXPECT_EQ(PieMenuCase.find("BlockPieMenuCardDrag"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionTranslateKeepsProviderControlsInItsContentOwner)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string OwnerBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderQmFunctionTranslateContent(");
	const std::string FunctionPageBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const size_t TranslateStart = FunctionPageBody.rfind("case EQmModuleId::Translate:");
	const size_t TranslateEnd = FunctionPageBody.find("case EQmModuleId::TranslateUi:", TranslateStart);
	const std::string TranslateCase = TranslateStart != std::string::npos && TranslateEnd != std::string::npos ? FunctionPageBody.substr(TranslateStart, TranslateEnd - TranslateStart) : std::string();
	ASSERT_FALSE(OwnerBody.empty());
	ASSERT_FALSE(TranslateCase.empty());

	EXPECT_NE(Header.find("RenderQmFunctionTranslateContent"), std::string::npos);
	EXPECT_NE(OwnerBody.find("tencentcloud"), std::string::npos);
	EXPECT_NE(OwnerBody.find("libretranslate"), std::string::npos);
	EXPECT_NE(OwnerBody.find("ftapi"), std::string::npos);
	EXPECT_NE(OwnerBody.find("\"llm\""), std::string::npos);
	EXPECT_NE(OwnerBody.find("RenderLanguageDropDownWithCustomInput"), std::string::npos);
	EXPECT_NE(OwnerBody.find("m_SelectionPopupContext.m_pScrollRegion"), std::string::npos);
	EXPECT_NE(OwnerBody.find("m_QmTranslateLlmConcurrency"), std::string::npos);
	EXPECT_NE(OwnerBody.find("m_QmTranslateLlmEnableThinking"), std::string::npos);
	EXPECT_NE(TranslateCase.find("RenderQmFunctionTranslateContent"), std::string::npos);
	EXPECT_EQ(TranslateCase.find("RenderLanguageDropDownWithCustomInput"), std::string::npos);
	EXPECT_EQ(TranslateCase.find("m_QmTranslateLlmEnableThinking"), std::string::npos);
}

TEST(QmMonitoringHelpers, P6FunctionDeckOwnsProductionCardLayout)
{
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string Header = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Registry = ReadRepoFile("src/game/client/QmUi/QmCardRegistry.cpp");
	const std::string FunctionDeckBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientFunctionDeck(");
	const std::string VisualDeckBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientVisualDeck(");
	const std::string FunctionPageBody = ExtractSourceFunctionBody(QmClient, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	const size_t FunctionDispatch = FunctionPageBody.find("RenderSettingsQmClientFunctionDeck(ContentView, PrewarmOnly)");
	const size_t LegacyScroll = FunctionPageBody.find("BeginSettingsQmScrollContainer");
	ASSERT_FALSE(FunctionDeckBody.empty());
	ASSERT_FALSE(VisualDeckBody.empty());
	ASSERT_NE(FunctionDispatch, std::string::npos);
	ASSERT_NE(LegacyScroll, std::string::npos);

	EXPECT_NE(Header.find("RenderSettingsQmClientFunctionDeck"), std::string::npos);
	EXPECT_LT(FunctionDispatch, LegacyScroll);
	EXPECT_NE(FunctionDeckBody.find("m_SettingsCardDeck.Render"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("SettingsCardOrderModel"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("ParseLegacyQmCollapsed"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("QmResolveScrollPolicy"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("qm:translate_ui"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("qm:qiafen"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("GetFavoriteMaps().size()"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("MeasureContentHeight"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("CalcQiaFenInputHeight(TextRender(), g_Config.m_QmBlockWordsList"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("QmKeywordReplyRules::DecodeFromConfig"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("CountAutoReplyRules(aDecodedRules)"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("m_QmTranslateLlmEnableThinking"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("m_QmTranslateLlmProvider == 2 || g_Config.m_QmTranslateLlmProvider == 3"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("m_QmFriendOnlineAutoRefresh"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("m_QmAutoTeamLock"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("Definition.m_MeasureEachFrame = true"), std::string::npos);
	EXPECT_NE(FunctionDeckBody.find("if(!Ui()->RenderOnly() && str_comp(aNormalizedCollapsed"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("s_aContentHeights"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("m_RenderMeasured"), std::string::npos);
	EXPECT_NE(Registry.find("{\"qm:translate_ui\", \"function\""), std::string::npos);
	EXPECT_NE(Registry.find("{\"qm:qiafen\", \"function\""), std::string::npos);
	EXPECT_EQ(VisualDeckBody.find("qm:translate_ui"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("s_GlassCards"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("RegisterModuleCard"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("HandleModuleDragState"), std::string::npos);
	EXPECT_EQ(FunctionDeckBody.find("BeginSettingsQmScrollContainer"), std::string::npos);
}

TEST(QmMonitoringHelpers, PublicSettingsCardDeckCoordinatesCanonicalDefinitions)
{
	const std::string Header = ReadRepoFile("src/game/client/QmUi/SettingsCardDeck.h");
	const std::string Source = ReadRepoFile("src/game/client/QmUi/SettingsCardDeck.cpp");

	EXPECT_NE(Header.find("struct SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(Header.find("struct SSettingsCardDeckInput"), std::string::npos);
	EXPECT_NE(Header.find("struct SSettingsCardDeckResult"), std::string::npos);
	EXPECT_NE(Header.find("class CSettingsCardDeck"), std::string::npos);
	EXPECT_NE(Header.find("SSettingsCardDeckResult Render("), std::string::npos);
	EXPECT_NE(Header.find("void RequestReveal("), std::string::npos);
	EXPECT_NE(Header.find("void BeginDisplayCycle("), std::string::npos);
	EXPECT_NE(Header.find("CProjectionCache m_ProjectionCache"), std::string::npos);
	EXPECT_NE(Source.find("m_ProjectionCache.Resolve("), std::string::npos);
	EXPECT_NE(Source.find("ApplySettingsCardDeckDragPlacement("), std::string::npos);
	EXPECT_NE(Source.find("ResolveSettingsCardDeckDropOrder("), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckAutoScrollDelta("), std::string::npos);
	EXPECT_NE(Source.find("CommitSettingsCardDeckDrop("), std::string::npos);
	EXPECT_NE(Source.find("SettingsCard("), std::string::npos);
	EXPECT_NE(Header.find("std::string m_PendingRevealStableId"), std::string::npos);
	EXPECT_NE(Header.find("m_ReflowCompleteFeedbackRemaining"), std::string::npos);
	EXPECT_NE(Header.find("m_vContentHeights"), std::string::npos);
	EXPECT_NE(Header.find("m_MeasureEachFrame"), std::string::npos);
	EXPECT_NE(Header.find("m_RenderMeasured"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardEntryNodeKey"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardReflowNodeKey"), std::string::npos);
	EXPECT_NE(Source.find("EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_NE(Source.find("m_DisplayCycle"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCard(Ctx, Card.m_Frame"), std::string::npos);
	EXPECT_NE(Source.find("Card.m_pDefinition->m_RenderMeasured"), std::string::npos);
	EXPECT_NE(Source.find("ResolveSettingsPageLayoutForScrollViewport"), std::string::npos);
	EXPECT_NE(Source.find("MouseInScrollViewport"), std::string::npos);
	EXPECT_NE(Source.find("Input.m_MouseX >= DrawLayout.m_aColumns[0].x"), std::string::npos);
	EXPECT_NE(Source.find("m_vContentHeights[StateIndex]"), std::string::npos);
	EXPECT_NE(Source.find("pDefinition->m_MeasureEachFrame"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckNeedsContentMeasure(Collapsed, pDefinition->m_MeasureEachFrame, ContentHeight)"), std::string::npos);
	EXPECT_NE(Source.find("SettingsCardDeckRendersContent(Collapsed) ? Card.m_pDefinition->m_Render : FSettingsCardRender{}"), std::string::npos);
	EXPECT_NE(Source.find("Model.Entry(Card.m_StateIndex).m_pStableId"), std::string::npos);
	EXPECT_EQ(Source.find("m_Drag.m_pStableId"), std::string::npos);

	const size_t AddRect = Source.find("pScrollRegion->AddRect(Card.m_Frame.m_Rect, Reveal)");
	const size_t Render = Source.find("SettingsCard(Ctx, Card.m_Frame");
	ASSERT_NE(AddRect, std::string::npos);
	ASSERT_NE(Render, std::string::npos);
	EXPECT_LT(AddRect, Render);

	const std::string CardSource = ReadRepoFile("src/game/client/QmUi/SettingsCard.cpp");
	EXPECT_NE(CardSource.find("HeaderAction(DrawFrame, DrawState.m_Collapsed)"), std::string::npos);
}
TEST(QmMonitoringHelpers, GraphicsUsesCanonicalSettingsCardShell)
{
	const std::string SettingsSource = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string GraphicsBody = ExtractSourceFunctionBody(SettingsSource, "void CMenus::RenderSettingsGraphics(CUIRect MainView)");
	ASSERT_FALSE(GraphicsBody.empty());

	EXPECT_NE(GraphicsBody.find("ResolveSettingsPageLayout("), std::string::npos);
	EXPECT_NE(GraphicsBody.find("SSettingsCardDefinition"), std::string::npos);
	EXPECT_NE(GraphicsBody.find("m_SettingsCardDeck.Render("), std::string::npos);
	EXPECT_NE(GraphicsBody.find("deck:graphics-display"), std::string::npos);
	EXPECT_EQ(GraphicsBody.find("RenderQmSettingsGlassCard("), std::string::npos);
}
TEST(QmMonitoringHelpers, DelayUpdateStaysOnUnifiedSliderInputPath)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string ScrollbarBody = ExtractSourceFunctionBody(Menus, "bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, int Subtab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)");
	const std::string FormsHeader = ReadRepoFile("src/game/client/QmUi/UiForms.h");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string NumericBody = ExtractSourceFunctionBody(Forms, "bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options)");
	ASSERT_FALSE(ScrollbarBody.empty());
	ASSERT_FALSE(NumericBody.empty());

	EXPECT_EQ(ScrollbarBody.find("Ui()->DoScrollbarOption("), std::string::npos);
	EXPECT_NE(ScrollbarBody.find("ui_widget::NumericField(InputCtx, pState"), std::string::npos);
	EXPECT_NE(ScrollbarBody.find("Options.m_CommitPolicy = (Flags & CUi::SCROLLBAR_OPTION_DELAYUPDATE) != 0 ?"), std::string::npos);
	EXPECT_EQ(Menus.find("DoSettingsSliderInputField"), std::string::npos);
	EXPECT_NE(FormsHeader.find("enum class EInputCommitPolicy"), std::string::npos);
	EXPECT_NE(FormsHeader.find("struct SNumericFieldOptions"), std::string::npos);
	EXPECT_NE(FormsHeader.find("struct SNumericFieldState"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState"), std::string::npos);
	EXPECT_NE(FormsHeader.find("CLineInputNumber m_Input;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("EInputCommitPolicy m_CommitPolicy = EInputCommitPolicy::LIVE;"), std::string::npos);
	EXPECT_EQ(FormsHeader.find("SSliderInputFieldOptions"), std::string::npos);
	EXPECT_EQ(FormsHeader.find("bool SliderInputField("), std::string::npos);
	EXPECT_NE(MenusHeader.find("std::unordered_map<const void *, std::unique_ptr<ui_widget::SNumericFieldState>> m_vpSettingsNumericFieldStates;"), std::string::npos);
	EXPECT_EQ(MenusHeader.find("DoSettingsSliderInputField"), std::string::npos);
	EXPECT_EQ(MenusHeader.find("m_vpSettingsSliderInputs"), std::string::npos);
	EXPECT_EQ(ScrollbarBody.find("GetSettingsSliderInput("), std::string::npos);
	EXPECT_NE(NumericBody.find("const bool SliderWasActive = pState->m_SliderWasActive;"), std::string::npos);
	EXPECT_NE(NumericBody.find("UpdateNumericFieldSliderCommit(*pState, Options.m_CommitPolicy, SliderActive, SliderReleased, CandidateStored, pValue)"), std::string::npos);
	EXPECT_NE(NumericBody.find("const int VisibleStoredValue = pState->m_HasPendingValue ? pState->m_PendingStoredValue : *pValue;"), std::string::npos);
}
TEST(QmMonitoringHelpers, SettingsNumericFieldsRemoveLegacySliderForwarding)
{
	const std::string FormsHeader = ReadRepoFile("src/game/client/QmUi/UiForms.h");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string ScrollbarBody = ExtractSourceFunctionBody(Menus, "bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, int Subtab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)");
	ASSERT_FALSE(ScrollbarBody.empty());

	EXPECT_NE(FormsHeader.find("struct SNumericFieldOptions"), std::string::npos);
	EXPECT_EQ(FormsHeader.find("SSliderInputFieldOptions"), std::string::npos);
	EXPECT_EQ(FormsHeader.find("bool SliderInputField("), std::string::npos);
	EXPECT_NE(Forms.find("bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState"), std::string::npos);
	EXPECT_EQ(Forms.find("bool SliderInputField("), std::string::npos);
	EXPECT_EQ(MenusHeader.find("DoSettingsSliderInputField"), std::string::npos);
	EXPECT_EQ(Menus.find("DoSettingsSliderInputField"), std::string::npos);
	EXPECT_NE(ScrollbarBody.find("ui_widget::NumericField(InputCtx, pState"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsPasswordFieldsUseSharedTextPrimitive)
{
	const std::string QmMenus = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TClientMenus = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string QmSettings = ExtractSourceFunctionBody(QmMenus, "void CMenus::RenderSettingsQmClientContent(CUIRect MainView, bool ContributorsPage, bool PrewarmOnly)");
	ASSERT_FALSE(QmSettings.empty());

	EXPECT_NE(QmSettings.find("IUiContext QmClientGoresTextInputCtx;"), std::string::npos);
	EXPECT_NE(QmSettings.find("ui_widget::InputField(QmClientGoresTextInputCtx, &s_AxiomLoginPassword, PasswordEditRect"), std::string::npos);
	EXPECT_NE(QmSettings.find("ui_widget::InputField(QmClientGoresTextInputCtx, &s_AxiomDummyLoginPassword, PasswordEditRect"), std::string::npos);
	EXPECT_EQ(QmSettings.find("Ui()->DoEditBox(&s_AxiomLoginPassword"), std::string::npos);
	EXPECT_EQ(QmSettings.find("Ui()->DoEditBox(&s_AxiomDummyLoginPassword"), std::string::npos);
	EXPECT_EQ(MenusHeader.find("DoEditBoxWithLabel"), std::string::npos);
	EXPECT_EQ(TClientMenus.find("bool CMenus::DoEditBoxWithLabel("), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsUiThemeIsInjectedIntoSharedInputPrimitives)
{
	const std::string Theme = ReadRepoFile("src/game/client/QmUi/UiTheme.h");
	const std::string Context = ReadRepoFile("src/game/client/QmUi/UiContext.h");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string ContextBody = ExtractSourceFunctionBody(Menus, "IUiContext CMenus::SettingsUiContext(const char *pScope, const float UiScale)");
	const std::string SliderBody = ExtractSourceFunctionBody(Menus, "bool CMenus::DoSettingsScrollbarOption(int Page, int Tab, int Subtab, const char *pTextId, const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix, const char *pMaxText)");
	ASSERT_FALSE(ContextBody.empty());
	ASSERT_FALSE(SliderBody.empty());

	EXPECT_NE(Theme.find("struct SUiTheme"), std::string::npos);
	EXPECT_NE(Theme.find("SUiTheme ResolveUiTheme"), std::string::npos);
	EXPECT_NE(Context.find("const SUiTheme *m_pTheme = nullptr;"), std::string::npos);
	EXPECT_NE(Context.find("float m_UiScale = 1.0f;"), std::string::npos);
	EXPECT_NE(Forms.find("const SUiTheme &ThemeFor(const IUiContext &Ctx)"), std::string::npos);
	EXPECT_NE(Forms.find("ResolveUiTheme(ColorHSLA(0.0f, 0.0f, 0.29f, 1.0f), 1.0f)"), std::string::npos);
	EXPECT_EQ(Forms.find("CUi::ms_LightButtonColorFunction"), std::string::npos);
	EXPECT_EQ(Forms.find("ui_token::color::BORDER_FOCUS"), std::string::npos);
	EXPECT_NE(ContextBody.find("m_SettingsUiTheme = ResolveUiTheme(ColorHSLA(g_Config.m_QmUiColor), g_Config.m_QmUiOpacity / 100.0f);"), std::string::npos);
	EXPECT_NE(ContextBody.find("Context.m_pIconManager = GameClient()->QmIconManager();"), std::string::npos);
	EXPECT_NE(ContextBody.find("Context.m_pTooltips = &GameClient()->m_Tooltips;"), std::string::npos);
	EXPECT_NE(ContextBody.find("Context.m_pTheme = &m_SettingsUiTheme;"), std::string::npos);
	EXPECT_NE(SliderBody.find("IUiContext InputCtx = SettingsUiContext(\"settings_slider_input\");"), std::string::npos);
	EXPECT_NE(MenusHeader.find("SUiTheme m_SettingsUiTheme;"), std::string::npos);
}
TEST(QmMonitoringHelpers, QmUiStateAnimationBacksWidgetFocusAndHover)
{
	const std::string FormsHeader = ReadRepoFile("src/game/client/QmUi/UiForms.h");
	const std::string Motion = ReadRepoFile("src/game/client/QmUi/UiMotion.h");
	const std::string Forms = ReadRepoFile("src/game/client/QmUi/UiForms.cpp");
	const std::string QmMenus = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string TClientMenus = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Navigation = ReadRepoFile("src/game/client/QmUi/UiNavigation.cpp");
	const std::string TextFieldPlateBody = ExtractSourceFunctionBody(Forms, "void DrawTextFieldPlate(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)");
	const std::string TextFieldBody = ExtractSourceFunctionBody(Forms, "bool TextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)");
	const std::string ClearableBody = ExtractSourceFunctionBody(Forms, "bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)");
	const std::string SearchBody = ExtractSourceFunctionBody(Forms, "bool SearchField(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool HotkeyEnabled)");
	const std::string TextFieldExBody = ExtractSourceFunctionBody(Forms, "SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options)");
	const std::string ClearableExBody = ExtractSourceFunctionBody(Forms, "SInputFieldResult ClearableTextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const char *pPlaceholder, float FontSize)");
	const std::string SearchExBody = ExtractSourceFunctionBody(Forms, "SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, float FontSize, bool HotkeyEnabled)");
	const std::string ListItemBody = ExtractSourceFunctionBody(Navigation, "bool ListItem(const IUiContext &Ctx, const void *pId, const char *pText, const CUIRect &Rect, const SListItemProps &Props)");
	ASSERT_FALSE(TextFieldPlateBody.empty());
	ASSERT_FALSE(TextFieldBody.empty());
	ASSERT_FALSE(ClearableBody.empty());
	ASSERT_FALSE(SearchBody.empty());
	ASSERT_FALSE(TextFieldExBody.empty());
	ASSERT_FALSE(ClearableExBody.empty());
	ASSERT_FALSE(SearchExBody.empty());
	ASSERT_FALSE(ListItemBody.empty());

	EXPECT_NE(FormsHeader.find("bool ClearableTextField(const IUiContext &Ctx, CLineInput *pInput"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool SearchField(const IUiContext &Ctx, CLineInput *pInput"), std::string::npos);
	EXPECT_NE(FormsHeader.find("struct SInputFieldResult"), std::string::npos);
	EXPECT_NE(FormsHeader.find("struct STextFieldOptions"), std::string::npos);
	EXPECT_NE(FormsHeader.find("int m_Corners = IGraphics::CORNER_ALL;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("float m_CornerRadius = ui_token::radius::BASE;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("int m_TextAlign = TEXTALIGN_ML;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput"), std::string::npos);
	EXPECT_NE(FormsHeader.find("SInputFieldResult TextFieldEx(const IUiContext &Ctx, CLineInput *pInput, const CUIRect &Rect, const STextFieldOptions &Options);"), std::string::npos);
	EXPECT_NE(FormsHeader.find("SInputFieldResult ClearableTextFieldEx(const IUiContext &Ctx, CLineInput *pInput"), std::string::npos);
	EXPECT_NE(FormsHeader.find("SInputFieldResult SearchFieldEx(const IUiContext &Ctx, CLineInput *pInput"), std::string::npos);
	EXPECT_NE(FormsHeader.find("inline SInputFieldResult BuildInputFieldResult(bool WasActive, bool IsActive, bool Changed, bool SubmitPressed, bool WasEmpty, bool IsEmpty, bool Clearable)"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool m_Changed = false;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool m_Committed = false;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool m_Submitted = false;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool m_Deactivated = false;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("bool m_Cleared = false;"), std::string::npos);
	EXPECT_NE(FormsHeader.find("enum class EInputFieldCapability"), std::string::npos);
	EXPECT_NE(FormsHeader.find("constexpr unsigned InputFieldCapabilities()"), std::string::npos);
	EXPECT_NE(FormsHeader.find("DOUBLE_CLICK_SELECT_ALL"), std::string::npos);
	EXPECT_NE(FormsHeader.find("CLICK_AWAY_COMMIT"), std::string::npos);
	EXPECT_NE(FormsHeader.find("CURSOR_INSERTION"), std::string::npos);
	EXPECT_NE(FormsHeader.find("MOUSE_DRAG_SELECTION"), std::string::npos);
	EXPECT_NE(Motion.find("AnimateStateValue(const IUiContext &Ctx, const void *pId, EUiAnimProperty Property, float Target"), std::string::npos);
	EXPECT_NE(Motion.find("Ctx.m_pAnim->ResolveTargetValue(NodeKey, Property, Target, Transition);"), std::string::npos);
	EXPECT_NE(Motion.find("return Target;"), std::string::npos);
	EXPECT_NE(Forms.find("#include \"UiMotion.h\""), std::string::npos);
	EXPECT_NE(Forms.find("#include <engine/graphics.h>"), std::string::npos);
	EXPECT_NE(Forms.find("#include <game/client/ui.h>"), std::string::npos);
	EXPECT_NE(Navigation.find("#include \"UiMotion.h\""), std::string::npos);
	EXPECT_NE(TextFieldPlateBody.find("AnimateStateValue(Ctx, pInput, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);"), std::string::npos);
	EXPECT_NE(TextFieldPlateBody.find("const SUiTheme &Theme = ThemeFor(Ctx);"), std::string::npos);
	EXPECT_NE(TextFieldPlateBody.find("const ColorRGBA PlateColor = Active ? Theme.m_InputSurfaceFocused : (Hovered ? Theme.m_SurfaceHovered : Theme.m_InputSurface);"), std::string::npos);
	EXPECT_EQ(TextFieldPlateBody.find("ui_token::color::SURFACE_ELEVATED"), std::string::npos);
	EXPECT_NE(TextFieldExBody.find("return InputField(Ctx, pInput, Rect, InputOptions);"), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("Ctx.m_pUi->DoEditBox("), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("DrawTextFieldPlate("), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("DoEditBox"), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("DrawTextFieldPlaceholder("), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("VMargin("), std::string::npos);
	EXPECT_NE(ClearableExBody.find("InputOptions.m_Clearable = true;"), std::string::npos);
	EXPECT_NE(ClearableExBody.find("return InputField(Ctx, pInput, Rect, InputOptions);"), std::string::npos);
	EXPECT_EQ(ClearableExBody.find("DoEditBox"), std::string::npos);
	EXPECT_EQ(ClearableExBody.find("DrawTextFieldPlate("), std::string::npos);
	EXPECT_EQ(ClearableExBody.find("DrawTextFieldPlaceholder("), std::string::npos);
	EXPECT_EQ(ClearableExBody.find("VMargin("), std::string::npos);
	EXPECT_NE(SearchExBody.find("InputOptions.m_Mode = EInputFieldMode::SEARCH;"), std::string::npos);
	EXPECT_NE(SearchExBody.find("return InputField(Ctx, pInput, Rect, InputOptions);"), std::string::npos);
	EXPECT_EQ(SearchExBody.find("DoEditBox"), std::string::npos);
	EXPECT_EQ(SearchExBody.find("VMargin("), std::string::npos);
	EXPECT_NE(Ui.find("bool CUi::DoClearableEditBox(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, int Corners, const std::vector<STextColorSplit> &vColorSplits, const SEditBoxRenderOptions &RenderOptions)"), std::string::npos);
	EXPECT_NE(Ui.find("bool CUi::DoEditBox_Search(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, bool HotkeyEnabled, const SEditBoxRenderOptions &RenderOptions)"), std::string::npos);
	EXPECT_NE(Ui.find("bool CUi::DoEditBoxMultiLine(CLineInput *pLineInput, const CUIRect *pRect, float FontSize, float LineSpacing, int TextAlign, const SEditBoxRenderOptions &RenderOptions)"), std::string::npos);
	EXPECT_EQ(QmMenus.find("static bool DoEditBoxMultiLine("), std::string::npos);
	EXPECT_EQ(TClientMenus.find("static bool DoEditBoxMultiLine("), std::string::npos);
	EXPECT_NE(QmMenus.find("BlockWordsInputOptions.m_Mode = ui_widget::EInputFieldMode::MULTILINE;"), std::string::npos);
	EXPECT_NE(TextFieldBody.find("return TextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;"), std::string::npos);
	EXPECT_NE(ClearableBody.find("return ClearableTextFieldEx(Ctx, pInput, Rect, pPlaceholder, FontSize).m_Changed;"), std::string::npos);
	EXPECT_NE(SearchBody.find("return SearchFieldEx(Ctx, pInput, Rect, FontSize, HotkeyEnabled).m_Changed;"), std::string::npos);
	EXPECT_NE(Forms.find("Ctx.m_pUi->DoEditBox(pInput"), std::string::npos);
	EXPECT_EQ(Forms.find("Ctx.m_pUi->DoClearableEditBox(pInput"), std::string::npos);
	EXPECT_EQ(Forms.find("Ctx.m_pUi->DoEditBox_Search(pInput"), std::string::npos);
	EXPECT_NE(Forms.find("return ui_widget::BuildInputFieldResult(WasActive, pInput->IsActive(), Changed, SubmitPressed, WasEmpty, pInput->IsEmpty(), Clearable);"), std::string::npos);
	EXPECT_NE(ClearableExBody.find("return InputField(Ctx, pInput, Rect, InputOptions);"), std::string::npos);
	EXPECT_NE(SearchExBody.find("return InputField(Ctx, pInput, Rect, InputOptions);"), std::string::npos);
	EXPECT_NE(ListItemBody.find("AnimateStateValue(Ctx, pId, EUiAnimProperty::ALPHA, TargetAlpha, ui_curve::DECELERATE);"), std::string::npos);
	EXPECT_EQ(TextFieldPlateBody.find("ResolveUiAnimValue(*Ctx.m_pAnim"), std::string::npos);
	EXPECT_EQ(TextFieldExBody.find("ResolveUiAnimValue(*Ctx.m_pAnim"), std::string::npos);
	EXPECT_EQ(ClearableExBody.find("ResolveUiAnimValue(*Ctx.m_pAnim"), std::string::npos);
	EXPECT_EQ(SearchBody.find("ResolveUiAnimValue(*Ctx.m_pAnim"), std::string::npos);
	EXPECT_EQ(ListItemBody.find("ResolveUiAnimValue(*Ctx.m_pAnim"), std::string::npos);
	const std::string SliderInputBody = ExtractSourceFunctionBody(Forms, "bool NumericField(const IUiContext &Ctx, SNumericFieldState *pState, const void *pId, int *pValue, int Min, int Max, const CUIRect &Rect, const SNumericFieldOptions &Options)");
	ASSERT_FALSE(SliderInputBody.empty());
	EXPECT_NE(SliderInputBody.find("const bool MultiLine = Options.m_Flags & CUi::SCROLLBAR_OPTION_MULTILINE;"), std::string::npos);
	EXPECT_NE(SliderInputBody.find("if(MultiLine)"), std::string::npos);
	EXPECT_NE(SliderInputBody.find("const float LabelFontSize = MultiLine ? std::min(Options.m_FontSize, Label.h * CUi::ms_FontmodHeight * 0.8f) : Options.m_FontSize;"), std::string::npos);
	EXPECT_NE(SliderInputBody.find("const float FieldFontSize = std::min(Options.m_FontSize, InputField.h * CUi::ms_FontmodHeight * 0.8f);"), std::string::npos);
	EXPECT_NE(SliderInputBody.find("SliderInputWheelStoredValue("), std::string::npos);
	EXPECT_NE(SliderInputBody.find("DoScrollbarH(pId, &ScrollBar, Normalized)"), std::string::npos);
	EXPECT_EQ(SliderInputBody.find("DoScrollbarH(pId, &ScrollBar, Normalized, &Inner)"), std::string::npos);
	EXPECT_NE(QmMenus.find("ui_widget::SNumericFieldState *pState = GetSettingsNumericFieldState(pId);"), std::string::npos);
	EXPECT_NE(QmMenus.find("ui_widget::NumericField(QmInputCtx, pState, pId, pValue, MinValue, MaxValue, ControlColumn, Options);"), std::string::npos);
	EXPECT_EQ(QmMenus.find("GetSettingsSliderInput("), std::string::npos);
	EXPECT_EQ(QmMenus.find("Ui()->DoValueSelectorWithState(pId, &InputRect"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientConfigSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientConfigs(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientConfigSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TClientConfigSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_config_search\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientConfigSearchCtx, &s_SearchInput, SearchEdit, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_SearchInput, &SearchEdit, EditBoxFontSize);"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientWarListSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientWarList(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t EntriesSearchPos = Body.find("ui_widget::InputField(TClientWarListEntriesSearchCtx, &s_EntriesFilterInput, EntriesSearch, 14.0f");
	const size_t EntriesFilterPos = Body.find("if(str_comp(s_aCachedEntriesFilter, s_EntriesFilterInput.GetString()) != 0", EntriesSearchPos);
	const size_t PlayerSearchPos = Body.find("ui_widget::InputField(TClientWarListPlayerSearchCtx, &s_PlayerSearchInput, PlayerSearch, 14.0f");
	const size_t PlayerFilterPos = Body.find("if(!str_find_nocase(Client.m_aName, s_PlayerSearchInput.GetString())", PlayerSearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientWarListEntriesSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TClientWarListEntriesSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_warlist_entries_search\");"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientWarListPlayerSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TClientWarListPlayerSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_warlist_player_search\");"), std::string::npos);
	EXPECT_NE(EntriesSearchPos, std::string::npos);
	EXPECT_NE(EntriesFilterPos, std::string::npos);
	EXPECT_LT(EntriesSearchPos, EntriesFilterPos);
	EXPECT_NE(PlayerSearchPos, std::string::npos);
	EXPECT_NE(PlayerFilterPos, std::string::npos);
	EXPECT_LT(PlayerSearchPos, PlayerFilterPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_PlayerSearchInput, &PlayerSearch"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientWarListTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientWarList(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientWarListTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TClientWarListTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_warlist_text_inputs\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientWarListTextInputCtx, &s_NameInput, ButtonL, Localize(\"Name\"), 12.0f);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientWarListTextInputCtx, &s_ClanInput, ButtonR, Localize(\"Clan\"), 12.0f);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientWarListTextInputCtx, &s_ReasonInput, Button, Localize(\"Reason\"), 12.0f);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientWarListTextInputCtx, &s_TypeNameInput, Button, Localize(\"Group name\"), 12.0f);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_NameInput, &ButtonL, 12.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_ClanInput, &ButtonR, 12.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_ReasonInput, &Button, 12.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_TypeNameInput, &Button, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientAutoReplyTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string CacheBody = ExtractSourceFunctionBody(Source, "float CMenus::LayoutTClientAutoReplyCacheSection(CUIRect &CurrentColumn, bool Render)");
	const std::string LegacyLayoutBody = ExtractSourceBlock(Source, "auto LayoutAutoReplySection", "auto MeasureAutoReplySection");
	const std::string LegacyInteractiveBody = ExtractSourceBlock(Source, "auto RenderAutoReplyInteractiveSection", "auto LayoutPlayerIndicatorSection");
	ASSERT_FALSE(CacheBody.empty());
	ASSERT_FALSE(LegacyLayoutBody.empty());
	ASSERT_FALSE(LegacyInteractiveBody.empty());

	const auto ExpectAutoReplyCtx = [](const std::string &Body) {
		const size_t CtxPos = Body.find("IUiContext TClientAutoReplyTextInputCtx;");
		const size_t UiPos = Body.find("TClientAutoReplyTextInputCtx.m_pUi = Ui();", CtxPos);
		const size_t AnimPos = Body.find("TClientAutoReplyTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
		const size_t TreePos = Body.find("TClientAutoReplyTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
		const size_t ScopePos = Body.find("TClientAutoReplyTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_auto_reply_text_inputs\");", TreePos);
		const size_t FrameDtPos = Body.find("TClientAutoReplyTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
		EXPECT_NE(CtxPos, std::string::npos);
		EXPECT_NE(UiPos, std::string::npos);
		EXPECT_NE(AnimPos, std::string::npos);
		EXPECT_NE(TreePos, std::string::npos);
		EXPECT_NE(ScopePos, std::string::npos);
		EXPECT_NE(FrameDtPos, std::string::npos);
		EXPECT_LT(CtxPos, UiPos);
		EXPECT_LT(UiPos, AnimPos);
		EXPECT_LT(AnimPos, TreePos);
		EXPECT_LT(TreePos, ScopePos);
		EXPECT_LT(ScopePos, FrameDtPos);
	};

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	ExpectAutoReplyCtx(CacheBody);
	EXPECT_NE(CacheBody.find("s_MutedReply.SetEmptyText(Localize(\"I muted you\"));"), std::string::npos);
	EXPECT_NE(CacheBody.find("s_MinimizedReply.SetEmptyText(Localize(\"I am away from the game window\"));"), std::string::npos);
	EXPECT_NE(CacheBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_NE(CacheBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectAutoReplyCtx(LegacyLayoutBody);
	EXPECT_NE(LegacyLayoutBody.find("s_MutedReply.SetEmptyText(Localize(\"I muted you\"));"), std::string::npos);
	EXPECT_NE(LegacyLayoutBody.find("s_MinimizedReply.SetEmptyText(Localize(\"I am away from the game window\"));"), std::string::npos);
	EXPECT_NE(LegacyLayoutBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_NE(LegacyLayoutBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectAutoReplyCtx(LegacyInteractiveBody);
	EXPECT_NE(LegacyInteractiveBody.find("s_MutedReply.SetEmptyText(Localize(\"I muted you\"));"), std::string::npos);
	EXPECT_NE(LegacyInteractiveBody.find("s_MinimizedReply.SetEmptyText(Localize(\"I am away from the game window\"));"), std::string::npos);
	EXPECT_NE(LegacyInteractiveBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_NE(LegacyInteractiveBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, Localize(\"I muted you\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, Localize(\"I am away from the game window\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, Localize(\"I muted you\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, Localize(\"I am away from the game window\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MutedReply, ReplyRect, Localize(\"I muted you\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("ui_widget::InputField(TClientAutoReplyTextInputCtx, &s_MinimizedReply, ReplyRect, Localize(\"I am away from the game window\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("Ui()->DoEditBox(&s_MutedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(CacheBody.find("Ui()->DoEditBox(&s_MinimizedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("Ui()->DoEditBox(&s_MutedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("Ui()->DoEditBox(&s_MinimizedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("Ui()->DoEditBox(&s_MutedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("Ui()->DoEditBox(&s_MinimizedReply, &ReplyRect, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientPetTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string CacheBody = ExtractSourceFunctionBody(Source, "float CMenus::LayoutTClientPetCacheSection(CUIRect &CurrentColumn, bool Render)");
	const std::string LegacyLayoutBody = ExtractSourceBlock(Source, "auto LayoutPetSection", "auto MeasurePetSection");
	const std::string LegacyInteractiveBody = ExtractSourceBlock(Source, "auto RenderPetInteractiveSection", "auto LayoutAutoReplySection");
	ASSERT_FALSE(CacheBody.empty());
	ASSERT_FALSE(LegacyLayoutBody.empty());
	ASSERT_FALSE(LegacyInteractiveBody.empty());

	const auto ExpectPetCtx = [](const std::string &Body) {
		const size_t CtxPos = Body.find("IUiContext TClientPetTextInputCtx;");
		const size_t UiPos = Body.find("TClientPetTextInputCtx.m_pUi = Ui();", CtxPos);
		const size_t AnimPos = Body.find("TClientPetTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
		const size_t TreePos = Body.find("TClientPetTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
		const size_t ScopePos = Body.find("TClientPetTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_pet_text_inputs\");", TreePos);
		const size_t FrameDtPos = Body.find("TClientPetTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
		EXPECT_NE(CtxPos, std::string::npos);
		EXPECT_NE(UiPos, std::string::npos);
		EXPECT_NE(AnimPos, std::string::npos);
		EXPECT_NE(TreePos, std::string::npos);
		EXPECT_NE(ScopePos, std::string::npos);
		EXPECT_NE(FrameDtPos, std::string::npos);
		EXPECT_LT(CtxPos, UiPos);
		EXPECT_LT(UiPos, AnimPos);
		EXPECT_LT(AnimPos, TreePos);
		EXPECT_LT(TreePos, ScopePos);
		EXPECT_LT(ScopePos, FrameDtPos);
	};

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	ExpectPetCtx(CacheBody);
	EXPECT_NE(CacheBody.find("ui_widget::InputField(TClientPetTextInputCtx, &s_PetSkin, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectPetCtx(LegacyLayoutBody);
	EXPECT_NE(LegacyLayoutBody.find("ui_widget::InputField(TClientPetTextInputCtx, &s_PetSkin, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectPetCtx(LegacyInteractiveBody);
	EXPECT_NE(LegacyInteractiveBody.find("ui_widget::InputField(TClientPetTextInputCtx, &s_PetSkin, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("Ui()->DoEditBox(&s_PetSkin, &Button, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("Ui()->DoEditBox(&s_PetSkin, &Button, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("Ui()->DoEditBox(&s_PetSkin, &Button, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientHudTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string CacheBody = ExtractSourceFunctionBody(Source, "float CMenus::LayoutTClientHudCacheSection(CUIRect &CurrentColumn, bool Render)");
	const std::string LegacyLayoutBody = ExtractSourceBlock(Source, "auto LayoutHudSection", "auto MeasureHudSection");
	const std::string LegacyInteractiveBody = ExtractSourceBlock(Source, "auto RenderHudInteractiveSection", "auto RenderHudSettingsMap");
	ASSERT_FALSE(CacheBody.empty());
	ASSERT_FALSE(LegacyLayoutBody.empty());
	ASSERT_FALSE(LegacyInteractiveBody.empty());

	const auto ExpectHudCtx = [](const std::string &Body) {
		const size_t CtxPos = Body.find("IUiContext TClientHudTextInputCtx;");
		const size_t UiPos = Body.find("TClientHudTextInputCtx.m_pUi = Ui();", CtxPos);
		const size_t AnimPos = Body.find("TClientHudTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
		const size_t TreePos = Body.find("TClientHudTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
		const size_t ScopePos = Body.find("TClientHudTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_hud_text_inputs\");", TreePos);
		const size_t FrameDtPos = Body.find("TClientHudTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
		EXPECT_NE(CtxPos, std::string::npos);
		EXPECT_NE(UiPos, std::string::npos);
		EXPECT_NE(AnimPos, std::string::npos);
		EXPECT_NE(TreePos, std::string::npos);
		EXPECT_NE(ScopePos, std::string::npos);
		EXPECT_NE(FrameDtPos, std::string::npos);
		EXPECT_LT(CtxPos, UiPos);
		EXPECT_LT(UiPos, AnimPos);
		EXPECT_LT(AnimPos, TreePos);
		EXPECT_LT(TreePos, ScopePos);
		EXPECT_LT(ScopePos, FrameDtPos);
	};

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	ExpectHudCtx(CacheBody);
	EXPECT_NE(CacheBody.find("s_LastInput.SetEmptyText(Localize(\"You're the last one!\"));"), std::string::npos);
	EXPECT_NE(CacheBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectHudCtx(LegacyLayoutBody);
	EXPECT_NE(LegacyLayoutBody.find("s_LastInput.SetEmptyText(Localize(\"You're the last one!\"));"), std::string::npos);
	EXPECT_NE(LegacyLayoutBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	ExpectHudCtx(LegacyInteractiveBody);
	EXPECT_NE(LegacyInteractiveBody.find("s_LastInput.SetEmptyText(Localize(\"You're the last one!\"));"), std::string::npos);
	EXPECT_NE(LegacyInteractiveBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, Localize(\"You're the last one!\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, Localize(\"You're the last one!\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("ui_widget::InputField(TClientHudTextInputCtx, &s_LastInput, Button, Localize(\"You're the last one!\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(CacheBody.find("Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyLayoutBody.find("Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(LegacyInteractiveBody.find("Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientWhiteFeetTextInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "auto LayoutVisualNameplateSection", "auto LayoutVisualEffectsSection");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientWhiteFeetTextInputCtx;");
	const size_t UiPos = Body.find("TClientWhiteFeetTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientWhiteFeetTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientWhiteFeetTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientWhiteFeetTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_white_feet_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientWhiteFeetTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	EXPECT_NE(Body.find("s_WhiteFeet.SetEmptyText(\"x_ninja\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientWhiteFeetTextInputCtx, &s_WhiteFeet, FeetBox, nullptr, EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("ui_widget::InputField(TClientWhiteFeetTextInputCtx, &s_WhiteFeet, FeetBox, \"x_ninja\", EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientAutoExecuteTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "auto LayoutAutoExecuteSection", "auto LayoutVotingSection");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientAutoExecuteTextInputCtx;");
	const size_t UiPos = Body.find("TClientAutoExecuteTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientAutoExecuteTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientAutoExecuteTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientAutoExecuteTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_auto_execute_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientAutoExecuteTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	const size_t BeforeConnectInputPos = Body.find("static CLineInput s_LineInput(g_Config.m_TcExecuteOnConnect, sizeof(g_Config.m_TcExecuteOnConnect));");
	const size_t BeforeConnectTextFieldPos = Body.find("ui_widget::InputField(TClientAutoExecuteTextInputCtx, &s_LineInput, Button, nullptr, EditBoxFontSize);", BeforeConnectInputPos);
	const size_t OnConnectInputPos = Body.find("static CLineInput s_LineInput(g_Config.m_TcExecuteOnJoin, sizeof(g_Config.m_TcExecuteOnJoin));", BeforeConnectTextFieldPos);
	const size_t OnConnectTextFieldPos = Body.find("ui_widget::InputField(TClientAutoExecuteTextInputCtx, &s_LineInput, Button, nullptr, EditBoxFontSize);", OnConnectInputPos);
	EXPECT_NE(BeforeConnectInputPos, std::string::npos);
	EXPECT_NE(BeforeConnectTextFieldPos, std::string::npos);
	EXPECT_NE(OnConnectInputPos, std::string::npos);
	EXPECT_NE(OnConnectTextFieldPos, std::string::npos);
	EXPECT_LT(BeforeConnectInputPos, BeforeConnectTextFieldPos);
	EXPECT_LT(BeforeConnectTextFieldPos, OnConnectInputPos);
	EXPECT_LT(OnConnectInputPos, OnConnectTextFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_LineInput, &Button, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientVotingTextInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "auto LayoutVotingSection", "auto LayoutPetSection");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientVotingTextInputCtx;");
	const size_t UiPos = Body.find("TClientVotingTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientVotingTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientVotingTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientVotingTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_voting_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientVotingTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	const size_t MessageRectPos = Body.find("CurrentColumn.HSplitTop(LineSize + MarginExtraSmall, Render ? &VoteMessage : &TmpRect, &CurrentColumn);");
	const size_t RenderGuardPos = Body.find("if(Render)", MessageRectPos);
	const size_t LabelPos = Body.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, nullptr, &Label, Localize(\"Message to send in chat:\"), FontSize, TEXTALIGN_ML);", RenderGuardPos);
	const size_t EmptyTextPos = Body.find("s_VoteMessage.SetEmptyText(Localize(\"Leave empty to disable\"));", LabelPos);
	const size_t TextFieldPos = Body.find("ui_widget::InputField(TClientVotingTextInputCtx, &s_VoteMessage, VoteMessage, nullptr, EditBoxFontSize);", EmptyTextPos);
	const size_t FinalSpacingPos = Body.find("CurrentColumn.HSplitTop(MarginExtraSmall, nullptr, &CurrentColumn);", TextFieldPos);
	EXPECT_NE(MessageRectPos, std::string::npos);
	EXPECT_NE(RenderGuardPos, std::string::npos);
	EXPECT_NE(LabelPos, std::string::npos);
	EXPECT_NE(EmptyTextPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(FinalSpacingPos, std::string::npos);
	EXPECT_LT(MessageRectPos, RenderGuardPos);
	EXPECT_LT(RenderGuardPos, LabelPos);
	EXPECT_LT(LabelPos, EmptyTextPos);
	EXPECT_LT(EmptyTextPos, TextFieldPos);
	EXPECT_LT(TextFieldPos, FinalSpacingPos);
	EXPECT_EQ(Body.find("ui_widget::InputField(TClientVotingTextInputCtx, &s_VoteMessage, VoteMessage, Localize(\"Leave empty to disable\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_VoteMessage, &VoteMessage, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientFinishNameTextInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "auto LayoutFinishNameSection", "std::vector<SSettingsSection> vRightSections");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientFinishNameTextInputCtx;");
	const size_t UiPos = Body.find("TClientFinishNameTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientFinishNameTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientFinishNameTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientFinishNameTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_finish_name_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientFinishNameTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	const size_t InputBoxPos = Body.find("CurrentColumn.HSplitTop(LineSize + MarginExtraSmall, Render ? &FinishNameBox : &TmpRect, &CurrentColumn);");
	const size_t RenderGuardPos = Body.find("if(Render)", InputBoxPos);
	const size_t LabelPos = Body.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, m_TClientSettingsTab, m_TClientSettingsTab, nullptr, &Label, Localize(\"Finish Name:\"), FontSize, TEXTALIGN_ML);", RenderGuardPos);
	const size_t InputPos = Body.find("static CLineInput s_FinishName(g_Config.m_TcFinishName, sizeof(g_Config.m_TcFinishName));", LabelPos);
	const size_t TextFieldPos = Body.find("ui_widget::InputField(TClientFinishNameTextInputCtx, &s_FinishName, Button, nullptr, EditBoxFontSize);", InputPos);
	EXPECT_NE(InputBoxPos, std::string::npos);
	EXPECT_NE(RenderGuardPos, std::string::npos);
	EXPECT_NE(LabelPos, std::string::npos);
	EXPECT_NE(InputPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(InputBoxPos, RenderGuardPos);
	EXPECT_LT(RenderGuardPos, LabelPos);
	EXPECT_LT(LabelPos, InputPos);
	EXPECT_LT(InputPos, TextFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_FinishName, &Button, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientStatusSchemeTextInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientStatusSchemeTextInputCtx;");
	const size_t UiPos = Body.find("TClientStatusSchemeTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientStatusSchemeTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientStatusSchemeTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientStatusSchemeTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_status_scheme_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientStatusSchemeTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);
	const size_t LabelPos = Body.find("DoSettingsMenuLabel(SETTINGS_TCLIENT, TCLIENT_TAB_STATUSBAR, TCLIENT_TAB_STATUSBAR, \"tclient-statusbar-scheme-label\", &Label, Localize(\"Status Scheme:\"), FontSize, TEXTALIGN_MR);");
	const size_t InputPos = Body.find("static CLineInput s_StatusScheme(g_Config.m_TcStatusBarScheme, sizeof(g_Config.m_TcStatusBarScheme));", LabelPos);
	const size_t EmptyTextPos = Body.find("s_StatusScheme.SetEmptyText(\"\");", InputPos);
	const size_t TextFieldPos = Body.find("ui_widget::InputField(TClientStatusSchemeTextInputCtx, &s_StatusScheme, StatusScheme, nullptr, EditBoxFontSize);", EmptyTextPos);
	EXPECT_NE(LabelPos, std::string::npos);
	EXPECT_NE(InputPos, std::string::npos);
	EXPECT_NE(EmptyTextPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(LabelPos, InputPos);
	EXPECT_LT(InputPos, EmptyTextPos);
	EXPECT_LT(EmptyTextPos, TextFieldPos);
	EXPECT_EQ(Body.find("ui_widget::InputField(TClientStatusSchemeTextInputCtx, &s_StatusScheme, StatusScheme, \"\", EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_StatusScheme, &StatusScheme, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientConfigEditorTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceBlock(Source, "CUIRect TopLine, Below;", "else if(pVar->m_Type == SConfigVariable::VAR_COLOR)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TClientConfigTextInputCtx;");
	const size_t UiPos = Body.find("TClientConfigTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t AnimPos = Body.find("TClientConfigTextInputCtx.m_pAnim = &GameClient()->UiRuntimeV2()->AnimRuntime();", UiPos);
	const size_t TreePos = Body.find("TClientConfigTextInputCtx.m_pTree = &GameClient()->UiRuntimeV2()->Tree();", AnimPos);
	const size_t ScopePos = Body.find("TClientConfigTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_config_text_inputs\");", TreePos);
	const size_t FrameDtPos = Body.find("TClientConfigTextInputCtx.m_FrameDt = GameClient()->UiRuntimeV2()->FrameDt();", ScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(AnimPos, std::string::npos);
	EXPECT_NE(TreePos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(FrameDtPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, AnimPos);
	EXPECT_LT(AnimPos, TreePos);
	EXPECT_LT(TreePos, ScopePos);
	EXPECT_LT(ScopePos, FrameDtPos);

	const size_t IntInputBoxPos = Body.find("Controls.VSplitLeft(60.0f, &InputBox, &Dummy);");
	const size_t IntTextFieldPos = Body.find("if(ui_widget::InputField(TClientConfigTextInputCtx, &State.m_Input, InputBox, nullptr, EditBoxFontSize))", IntInputBoxPos);
	const size_t IntReadPos = Body.find("int NewVal = State.m_Input.GetInteger();", IntTextFieldPos);
	EXPECT_NE(IntInputBoxPos, std::string::npos);
	EXPECT_NE(IntTextFieldPos, std::string::npos);
	EXPECT_NE(IntReadPos, std::string::npos);
	EXPECT_LT(IntInputBoxPos, IntTextFieldPos);
	EXPECT_LT(IntTextFieldPos, IntReadPos);

	const size_t StrTypePos = Body.find("else if(pVar->m_Type == SConfigVariable::VAR_STRING)");
	const size_t StrTextFieldPos = Body.find("if(ui_widget::InputField(TClientConfigTextInputCtx, &State.m_Input, Controls, nullptr, EditBoxFontSize))", StrTypePos);
	const size_t StrReadPos = Body.find("const char *NewVal = State.m_Input.GetString();", StrTextFieldPos);
	EXPECT_NE(StrTypePos, std::string::npos);
	EXPECT_NE(StrTextFieldPos, std::string::npos);
	EXPECT_NE(StrReadPos, std::string::npos);
	EXPECT_LT(StrTypePos, StrTextFieldPos);
	EXPECT_LT(StrTextFieldPos, StrReadPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&State.m_Input, &InputBox, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&State.m_Input, &Controls, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientBindWheelTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientBindWheelTextInputCtx = SettingsUiContext(\"settings_tclient_bindwheel_text_inputs\", UiScale);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientBindWheelTextInputCtx, &s_NameInput, Button, Localize(\"Name\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(TClientBindWheelTextInputCtx, &s_BindInput, Button, Localize(\"Command\"), EditBoxFontSize);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientChatBindsTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TextFieldPos = Body.find("ui_widget::InputField(TClientChatBindsTextInputCtx, &BindDefault.m_LineInput, Input, BindDefault.m_Bind.m_aName, EditBoxFontSize)");
	const size_t ActiveGuardPos = Body.find("&& BindDefault.m_LineInput.IsActive()", TextFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TClientChatBindsTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TClientChatBindsTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tclient_chatbinds_text_inputs\");"), std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(ActiveGuardPos, std::string::npos);
	EXPECT_LT(TextFieldPos, ActiveGuardPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&BindDefault.m_LineInput, &Input, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, ControlsQuickSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenusSettingsControls::Render(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext ControlsSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("ControlsSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_controls_search\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(ControlsSearchCtx, &m_FilterInput, QuickSearch, FONT_SIZE"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&m_FilterInput, &QuickSearch, FONT_SIZE"), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeSkinSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t SearchPos = Body.find("ui_widget::InputField(TeeSkinSearchCtx, &s_SkinFilterInput, QuickSearch, 14.0f");
	const size_t RefreshPos = Body.find("SkinList.ForceRefresh();", SearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext TeeSkinSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("TeeSkinSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee_skin_search\");"), std::string::npos);
	EXPECT_NE(SearchPos, std::string::npos);
	EXPECT_NE(RefreshPos, std::string::npos);
	EXPECT_LT(SearchPos, RefreshPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_SkinFilterInput, &QuickSearch, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, TeeSkinClearableInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t PrefixInputPos = Body.find("static CLineInput s_SkinPrefixInput(g_Config.m_ClSkinPrefix, sizeof(g_Config.m_ClSkinPrefix));");
	const size_t PrefixCtxPos = Body.find("IUiContext TeeSkinPrefixTextInputCtx;", PrefixInputPos);
	const size_t PrefixUiPos = Body.find("TeeSkinPrefixTextInputCtx.m_pUi = Ui();", PrefixCtxPos);
	const size_t PrefixScopePos = Body.find("TeeSkinPrefixTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee_skin_prefix_text_input\");", PrefixUiPos);
	const size_t PrefixOptionsPos = Body.find("ui_widget::SInputFieldOptions SkinPrefixInputOptions;", PrefixScopePos);
	const size_t PrefixClearablePos = Body.find("SkinPrefixInputOptions.m_Clearable = true;", PrefixOptionsPos);
	const size_t PrefixFieldPos = Body.find("ui_widget::InputField(TeeSkinPrefixTextInputCtx, &s_SkinPrefixInput, Button, SkinPrefixInputOptions).m_Changed", PrefixClearablePos);
	const size_t PrefixRefreshPos = Body.find("ShouldRefresh = true;", PrefixFieldPos);
	const size_t SkinInputPos = Body.find("static CLineInput s_SkinInput;", PrefixRefreshPos);
	const size_t SkinBufferPos = Body.find("s_SkinInput.SetBuffer(pSkinName, SkinNameSize);", SkinInputPos);
	const size_t SkinEmptyTextPos = Body.find("s_SkinInput.SetEmptyText(\"default\");", SkinBufferPos);
	const size_t SkinCtxPos = Body.find("IUiContext TeeSkinNameTextInputCtx;", SkinEmptyTextPos);
	const size_t SkinUiPos = Body.find("TeeSkinNameTextInputCtx.m_pUi = Ui();", SkinCtxPos);
	const size_t SkinScopePos = Body.find("TeeSkinNameTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee_skin_name_text_input\");", SkinUiPos);
	const size_t SkinOptionsPos = Body.find("ui_widget::SInputFieldOptions SkinNameInputOptions;", SkinScopePos);
	const size_t SkinClearablePos = Body.find("SkinNameInputOptions.m_Clearable = true;", SkinOptionsPos);
	const size_t SkinFieldPos = Body.find("ui_widget::InputField(TeeSkinNameTextInputCtx, &s_SkinInput, Button, SkinNameInputOptions).m_Changed", SkinClearablePos);
	const size_t NeedSendInfoPos = Body.find("SetNeedSendInfo();", SkinFieldPos);
	const size_t ScrollSelectedPos = Body.find("m_SkinListScrollToSelected = true;", NeedSendInfoPos);
	const size_t ForceRefreshPos = Body.find("SkinList.ForceRefresh();", ScrollSelectedPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(PrefixInputPos, std::string::npos);
	EXPECT_NE(PrefixCtxPos, std::string::npos);
	EXPECT_NE(PrefixUiPos, std::string::npos);
	EXPECT_NE(PrefixScopePos, std::string::npos);
	EXPECT_NE(PrefixOptionsPos, std::string::npos);
	EXPECT_NE(PrefixClearablePos, std::string::npos);
	EXPECT_NE(PrefixFieldPos, std::string::npos);
	EXPECT_NE(PrefixRefreshPos, std::string::npos);
	EXPECT_LT(PrefixInputPos, PrefixCtxPos);
	EXPECT_LT(PrefixCtxPos, PrefixUiPos);
	EXPECT_LT(PrefixUiPos, PrefixScopePos);
	EXPECT_LT(PrefixScopePos, PrefixFieldPos);
	EXPECT_LT(PrefixFieldPos, PrefixRefreshPos);
	EXPECT_NE(SkinInputPos, std::string::npos);
	EXPECT_NE(SkinBufferPos, std::string::npos);
	EXPECT_NE(SkinEmptyTextPos, std::string::npos);
	EXPECT_NE(SkinCtxPos, std::string::npos);
	EXPECT_NE(SkinUiPos, std::string::npos);
	EXPECT_NE(SkinScopePos, std::string::npos);
	EXPECT_NE(SkinOptionsPos, std::string::npos);
	EXPECT_NE(SkinClearablePos, std::string::npos);
	EXPECT_NE(SkinFieldPos, std::string::npos);
	EXPECT_NE(NeedSendInfoPos, std::string::npos);
	EXPECT_NE(ScrollSelectedPos, std::string::npos);
	EXPECT_NE(ForceRefreshPos, std::string::npos);
	EXPECT_LT(SkinInputPos, SkinBufferPos);
	EXPECT_LT(SkinBufferPos, SkinEmptyTextPos);
	EXPECT_LT(SkinEmptyTextPos, SkinCtxPos);
	EXPECT_LT(SkinCtxPos, SkinUiPos);
	EXPECT_LT(SkinUiPos, SkinScopePos);
	EXPECT_LT(SkinScopePos, SkinFieldPos);
	EXPECT_LT(SkinFieldPos, NeedSendInfoPos);
	EXPECT_LT(NeedSendInfoPos, ScrollSelectedPos);
	EXPECT_LT(ScrollSelectedPos, ForceRefreshPos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_SkinPrefixInput, &Button, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_SkinInput, &Button, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, SkinQueuePresetRenamePopupUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "CUi::EPopupMenuFunctionResult CMenus::PopupSkinQueuePresetRename(void *pContext, CUIRect View, bool Active)");
	ASSERT_FALSE(Body.empty());

	const size_t LabelPos = Body.find("pMenus->DoSettingsMenuLabel(SETTINGS_TEE, -1, -1, \"tee-skin-queue-new-preset-name\", &Label, Localize(\"New preset name\"), FontSize, TEXTALIGN_ML);");
	const size_t TextInputCtxPos = Body.find("IUiContext SkinQueuePresetRenameTextInputCtx;", LabelPos);
	const size_t TextInputUiPos = Body.find("SkinQueuePresetRenameTextInputCtx.m_pUi = pMenus->Ui();", TextInputCtxPos);
	const size_t TextInputScopePos = Body.find("SkinQueuePresetRenameTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_skin_queue_preset_rename_text_input\");", TextInputUiPos);
	const size_t TextFieldPos = Body.find("ui_widget::InputField(SkinQueuePresetRenameTextInputCtx, &pPopupContext->m_NameInput, Input, nullptr, FontSize + 1.0f);", TextInputScopePos);
	const size_t CancelPressedPos = Body.find("const bool CancelPressed", TextFieldPos);
	const size_t EscapeHotkeyPos = Body.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)", CancelPressedPos);
	const size_t ConfirmPressedPos = Body.find("const bool ConfirmPressed", TextFieldPos);
	const size_t EnterHotkeyPos = Body.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)", ConfirmPressedPos);
	const size_t RenamePos = Body.find("pMenus->GameClient()->m_Skins.RenameSkinQueuePreset((size_t)pPopupContext->m_PresetIndex, pPopupContext->m_NameInput.GetString(), pPopupContext->m_Dummy)", ConfirmPressedPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(LabelPos, std::string::npos);
	EXPECT_NE(TextInputCtxPos, std::string::npos);
	EXPECT_NE(TextInputUiPos, std::string::npos);
	EXPECT_NE(TextInputScopePos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(CancelPressedPos, std::string::npos);
	EXPECT_NE(EscapeHotkeyPos, std::string::npos);
	EXPECT_NE(ConfirmPressedPos, std::string::npos);
	EXPECT_NE(EnterHotkeyPos, std::string::npos);
	EXPECT_NE(RenamePos, std::string::npos);
	EXPECT_LT(LabelPos, TextInputCtxPos);
	EXPECT_LT(TextInputCtxPos, TextInputUiPos);
	EXPECT_LT(TextInputUiPos, TextInputScopePos);
	EXPECT_LT(TextInputScopePos, TextFieldPos);
	EXPECT_LT(TextFieldPos, CancelPressedPos);
	EXPECT_LT(CancelPressedPos, EscapeHotkeyPos);
	EXPECT_LT(TextFieldPos, ConfirmPressedPos);
	EXPECT_LT(ConfirmPressedPos, EnterHotkeyPos);
	EXPECT_LT(ConfirmPressedPos, RenamePos);
	EXPECT_EQ(Body.find("pMenus->Ui()->DoEditBox(&pPopupContext->m_NameInput, &Input, FontSize + 1.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, DDNetSettingsTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsDDNet(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t BackgroundInputPos = Body.find("static CLineInput s_BackgroundEntitiesInput(g_Config.m_ClBackgroundEntities, sizeof(g_Config.m_ClBackgroundEntities));");
	const size_t BackgroundWasActivePos = Body.find("const bool WasInputActive = s_BackgroundEntitiesInput.IsActive();", BackgroundInputPos);
	const size_t BackgroundCtxPos = Body.find("IUiContext DDNetBackgroundEntitiesTextInputCtx;", BackgroundWasActivePos);
	const size_t BackgroundUiPos = Body.find("DDNetBackgroundEntitiesTextInputCtx.m_pUi = Ui();", BackgroundCtxPos);
	const size_t BackgroundScopePos = Body.find("DDNetBackgroundEntitiesTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_ddnet_background_entities_text_input\");", BackgroundUiPos);
	const size_t BackgroundFieldPos = Body.find("const bool InputCommitted = ui_widget::InputField(DDNetBackgroundEntitiesTextInputCtx, &s_BackgroundEntitiesInput, EditBox, nullptr, 14.0f);", BackgroundScopePos);
	const size_t BackgroundApplyPos = Body.find("BackgroundChanged = ApplyBackgroundEntitiesInputValue(s_BackgroundEntitiesInput);", BackgroundFieldPos);
	const size_t BackgroundBlurPos = Body.find("ShouldCommitBackgroundEntitiesInputOnBlur(WasInputActive, s_BackgroundEntitiesInput.IsActive(), s_BackgroundEntitiesInput.GetString(), s_aBackgroundEntitiesSync)", BackgroundApplyPos);
	const size_t RunOnJoinInputPos = Body.find("static CLineInput s_RunOnJoinInput(g_Config.m_ClRunOnJoin, sizeof(g_Config.m_ClRunOnJoin));");
	const size_t RunOnJoinEmptyTextPos = Body.find("s_RunOnJoinInput.SetEmptyText(Localize(\"Chat command (e.g. showall 1)\"));", RunOnJoinInputPos);
	const size_t RunOnJoinCtxPos = Body.find("IUiContext DDNetRunOnJoinTextInputCtx;", RunOnJoinEmptyTextPos);
	const size_t RunOnJoinUiPos = Body.find("DDNetRunOnJoinTextInputCtx.m_pUi = Ui();", RunOnJoinCtxPos);
	const size_t RunOnJoinScopePos = Body.find("DDNetRunOnJoinTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_ddnet_run_on_join_text_input\");", RunOnJoinUiPos);
	const size_t RunOnJoinFieldPos = Body.find("ui_widget::InputField(DDNetRunOnJoinTextInputCtx, &s_RunOnJoinInput, Button, nullptr, 14.0f);", RunOnJoinScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(BackgroundInputPos, std::string::npos);
	EXPECT_NE(BackgroundWasActivePos, std::string::npos);
	EXPECT_NE(BackgroundCtxPos, std::string::npos);
	EXPECT_NE(BackgroundUiPos, std::string::npos);
	EXPECT_NE(BackgroundScopePos, std::string::npos);
	EXPECT_NE(BackgroundFieldPos, std::string::npos);
	EXPECT_NE(BackgroundApplyPos, std::string::npos);
	EXPECT_NE(BackgroundBlurPos, std::string::npos);
	EXPECT_LT(BackgroundInputPos, BackgroundWasActivePos);
	EXPECT_LT(BackgroundWasActivePos, BackgroundCtxPos);
	EXPECT_LT(BackgroundCtxPos, BackgroundUiPos);
	EXPECT_LT(BackgroundUiPos, BackgroundScopePos);
	EXPECT_LT(BackgroundScopePos, BackgroundFieldPos);
	EXPECT_LT(BackgroundFieldPos, BackgroundApplyPos);
	EXPECT_LT(BackgroundApplyPos, BackgroundBlurPos);
	EXPECT_NE(RunOnJoinInputPos, std::string::npos);
	EXPECT_NE(RunOnJoinEmptyTextPos, std::string::npos);
	EXPECT_NE(RunOnJoinCtxPos, std::string::npos);
	EXPECT_NE(RunOnJoinUiPos, std::string::npos);
	EXPECT_NE(RunOnJoinScopePos, std::string::npos);
	EXPECT_NE(RunOnJoinFieldPos, std::string::npos);
	EXPECT_LT(RunOnJoinInputPos, RunOnJoinEmptyTextPos);
	EXPECT_LT(RunOnJoinEmptyTextPos, RunOnJoinCtxPos);
	EXPECT_LT(RunOnJoinCtxPos, RunOnJoinUiPos);
	EXPECT_LT(RunOnJoinUiPos, RunOnJoinScopePos);
	EXPECT_LT(RunOnJoinScopePos, RunOnJoinFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_BackgroundEntitiesInput, &EditBox, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_RunOnJoinInput, &Button, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, TouchControlsLayoutInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame_touch_controls.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "bool CMenusIngameTouchControls::RenderLayoutSettingBlock(CUIRect Block)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TouchControlsLayoutTextInputCtx;");
	const size_t UiPos = Body.find("TouchControlsLayoutTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t ScopePos = Body.find("TouchControlsLayoutTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"touch_controls_layout_text_inputs\");", UiPos);
	const size_t XFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsLayoutTextInputCtx, &m_InputX, EditBox, nullptr, FONTSIZE)", ScopePos);
	const size_t XUpdatePos = Body.find("InputPosFunction(&m_InputX);", XFieldPos);
	const size_t YFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsLayoutTextInputCtx, &m_InputY, EditBox, nullptr, FONTSIZE)", XUpdatePos);
	const size_t YUpdatePos = Body.find("InputPosFunction(&m_InputY);", YFieldPos);
	const size_t WFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsLayoutTextInputCtx, &m_InputW, EditBox, nullptr, FONTSIZE)", YUpdatePos);
	const size_t WUpdatePos = Body.find("InputPosFunction(&m_InputW);", WFieldPos);
	const size_t HFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsLayoutTextInputCtx, &m_InputH, EditBox, nullptr, FONTSIZE)", WUpdatePos);
	const size_t HUpdatePos = Body.find("InputPosFunction(&m_InputH);", HFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(XFieldPos, std::string::npos);
	EXPECT_NE(XUpdatePos, std::string::npos);
	EXPECT_NE(YFieldPos, std::string::npos);
	EXPECT_NE(YUpdatePos, std::string::npos);
	EXPECT_NE(WFieldPos, std::string::npos);
	EXPECT_NE(WUpdatePos, std::string::npos);
	EXPECT_NE(HFieldPos, std::string::npos);
	EXPECT_NE(HUpdatePos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, ScopePos);
	EXPECT_LT(ScopePos, XFieldPos);
	EXPECT_LT(XFieldPos, XUpdatePos);
	EXPECT_LT(XUpdatePos, YFieldPos);
	EXPECT_LT(YFieldPos, YUpdatePos);
	EXPECT_LT(YUpdatePos, WFieldPos);
	EXPECT_LT(WFieldPos, WUpdatePos);
	EXPECT_LT(WUpdatePos, HFieldPos);
	EXPECT_LT(HFieldPos, HUpdatePos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_InputX, &EditBox, FONTSIZE"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_InputY, &EditBox, FONTSIZE"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_InputW, &EditBox, FONTSIZE"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_InputH, &EditBox, FONTSIZE"), std::string::npos);
}

TEST(QmMonitoringHelpers, TouchControlsBehaviorInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame_touch_controls.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "bool CMenusIngameTouchControls::RenderBehaviorSettingBlock(CUIRect Block)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TouchControlsBehaviorTextInputCtx;");
	const size_t UiPos = Body.find("TouchControlsBehaviorTextInputCtx.m_pUi = Ui();", CtxPos);
	const size_t ScopePos = Body.find("TouchControlsBehaviorTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"touch_controls_behavior_text_inputs\");", UiPos);
	const size_t BindCommandFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsBehaviorTextInputCtx, &m_vBehaviorElements[0]->m_InputCommand, MiddleButton, nullptr, 10.0f)", ScopePos);
	const size_t BindCommandUpdatePos = Body.find("m_vBehaviorElements[0]->UpdateCommand();", BindCommandFieldPos);
	const size_t BindLabelFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsBehaviorTextInputCtx, &m_vBehaviorElements[0]->m_InputLabel, MiddleButton, nullptr, 10.0f)", BindCommandUpdatePos);
	const size_t BindLabelUpdatePos = Body.find("m_vBehaviorElements[0]->UpdateLabel();", BindLabelFieldPos);
	const size_t ToggleCommandFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsBehaviorTextInputCtx, &m_vBehaviorElements[CommandIndex]->m_InputCommand, MiddleButton, nullptr, 10.0f)", BindLabelUpdatePos);
	const size_t ToggleCommandUpdatePos = Body.find("m_vBehaviorElements[CommandIndex]->UpdateCommand();", ToggleCommandFieldPos);
	const size_t ToggleLabelFieldPos = Body.find("ui_widget::ClearableTextField(TouchControlsBehaviorTextInputCtx, &m_vBehaviorElements[CommandIndex]->m_InputLabel, MiddleButton, nullptr, 10.0f)", ToggleCommandUpdatePos);
	const size_t ToggleLabelUpdatePos = Body.find("m_vBehaviorElements[CommandIndex]->UpdateLabel();", ToggleLabelFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(BindCommandFieldPos, std::string::npos);
	EXPECT_NE(BindCommandUpdatePos, std::string::npos);
	EXPECT_NE(BindLabelFieldPos, std::string::npos);
	EXPECT_NE(BindLabelUpdatePos, std::string::npos);
	EXPECT_NE(ToggleCommandFieldPos, std::string::npos);
	EXPECT_NE(ToggleCommandUpdatePos, std::string::npos);
	EXPECT_NE(ToggleLabelFieldPos, std::string::npos);
	EXPECT_NE(ToggleLabelUpdatePos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, ScopePos);
	EXPECT_LT(ScopePos, BindCommandFieldPos);
	EXPECT_LT(BindCommandFieldPos, BindCommandUpdatePos);
	EXPECT_LT(BindCommandUpdatePos, BindLabelFieldPos);
	EXPECT_LT(BindLabelFieldPos, BindLabelUpdatePos);
	EXPECT_LT(BindLabelUpdatePos, ToggleCommandFieldPos);
	EXPECT_LT(ToggleCommandFieldPos, ToggleCommandUpdatePos);
	EXPECT_LT(ToggleCommandUpdatePos, ToggleLabelFieldPos);
	EXPECT_LT(ToggleLabelFieldPos, ToggleLabelUpdatePos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_vBehaviorElements[0]->m_InputCommand, &MiddleButton, 10.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_vBehaviorElements[0]->m_InputLabel, &MiddleButton, 10.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_vBehaviorElements[CommandIndex]->m_InputCommand, &MiddleButton, 10.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_vBehaviorElements[CommandIndex]->m_InputLabel, &MiddleButton, 10.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, TouchControlsButtonBrowserSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame_touch_controls.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenusIngameTouchControls::RenderTouchButtonBrowser(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t CtxPos = Body.find("IUiContext TouchControlsBrowserSearchCtx;");
	const size_t UiPos = Body.find("TouchControlsBrowserSearchCtx.m_pUi = Ui();", CtxPos);
	const size_t ScopePos = Body.find("TouchControlsBrowserSearchCtx.m_ScopeHash = MakeUiScopeHash(\"touch_controls_button_browser_search\");", UiPos);
	const size_t SearchFieldPos = Body.find("ui_widget::SearchField(TouchControlsBrowserSearchCtx, &m_FilterInput, EditBox, FONTSIZE, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive())", ScopePos);
	const size_t NeedFilterPos = Body.find("m_NeedFilter = true;", SearchFieldPos);
	const size_t SearchLabelPos = Body.find("str_format(aBufSearch, sizeof(aBufSearch), \"%s:\", Localize(\"Search\"));");
	const size_t ManualSearchIconPos = Body.find("FontIcons::FONT_ICON_MAGNIFYING_GLASS");
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CtxPos, std::string::npos);
	EXPECT_NE(UiPos, std::string::npos);
	EXPECT_NE(ScopePos, std::string::npos);
	EXPECT_NE(SearchFieldPos, std::string::npos);
	EXPECT_NE(NeedFilterPos, std::string::npos);
	EXPECT_NE(SearchLabelPos, std::string::npos);
	EXPECT_EQ(ManualSearchIconPos, std::string::npos);
	EXPECT_LT(CtxPos, UiPos);
	EXPECT_LT(UiPos, ScopePos);
	EXPECT_LT(ScopePos, SearchFieldPos);
	EXPECT_LT(SearchFieldPos, NeedFilterPos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_FilterInput, &EditBox, FONTSIZE"), std::string::npos);
}

TEST(QmMonitoringHelpers, Tee7SkinSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings7.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTee7(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t SearchPos = Body.find("ui_widget::SearchField(Tee7SkinSearchCtx, &s_SkinFilterInput, QuickSearch, 14.0f");
	const size_t RefreshPos = Body.find("m_SkinList7LastRefreshTime = std::nullopt;", SearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext Tee7SkinSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("Tee7SkinSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee7_skin_search\");"), std::string::npos);
	EXPECT_NE(SearchPos, std::string::npos);
	EXPECT_NE(RefreshPos, std::string::npos);
	EXPECT_LT(SearchPos, RefreshPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_SkinFilterInput, &QuickSearch, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, PlayerFlagSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsPlayer(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext PlayerFlagSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("PlayerFlagSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_player_flag_search\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::InputField(PlayerFlagSearchCtx, &s_FlagFilterInput, QuickSearch, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_FlagFilterInput, &QuickSearch, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, PlayerIdentityTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string IdentityBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTeeIdentity(CUIRect MainView, CUIRect *pFlagButton)");
	const std::string PlayerBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsPlayer(CUIRect MainView)");
	ASSERT_FALSE(IdentityBody.empty());
	ASSERT_FALSE(PlayerBody.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(IdentityBody.find("IUiContext TeeIdentityTextInputCtx;"), std::string::npos);
	EXPECT_NE(IdentityBody.find("TeeIdentityTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_tee_identity_text_inputs\");"), std::string::npos);
	EXPECT_NE(IdentityBody.find("ui_widget::InputField(TeeIdentityTextInputCtx, &s_NameInput, NameInputRect, Client()->PlayerName(), 14.0f)"), std::string::npos);
	EXPECT_NE(IdentityBody.find("ui_widget::InputField(TeeIdentityTextInputCtx, &s_ClanInput, ClanInput, \"\", 14.0f)"), std::string::npos);
	EXPECT_EQ(IdentityBody.find("Ui()->DoEditBox(&s_NameInput, &NameInputRect, 14.0f"), std::string::npos);
	EXPECT_EQ(IdentityBody.find("Ui()->DoEditBox(&s_ClanInput, &ClanInput, 14.0f"), std::string::npos);

	EXPECT_NE(PlayerBody.find("IUiContext PlayerIdentityTextInputCtx;"), std::string::npos);
	EXPECT_NE(PlayerBody.find("PlayerIdentityTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_player_identity_text_inputs\");"), std::string::npos);
	EXPECT_NE(PlayerBody.find("ui_widget::InputField(PlayerIdentityTextInputCtx, &s_NameInput, Row, Client()->PlayerName(), 14.0f)"), std::string::npos);
	EXPECT_NE(PlayerBody.find("ui_widget::InputField(PlayerIdentityTextInputCtx, &s_ClanInput, Row, \"\", 14.0f)"), std::string::npos);
	EXPECT_EQ(PlayerBody.find("Ui()->DoEditBox(&s_NameInput, &Row, 14.0f"), std::string::npos);
	EXPECT_EQ(PlayerBody.find("Ui()->DoEditBox(&s_ClanInput, &Row, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameCallvoteSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerControl(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext CallvoteSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("CallvoteSearchCtx.m_ScopeHash = MakeUiScopeHash(\"ingame_callvote_search\");"), std::string::npos);
	EXPECT_NE(Body.find("bool Searching = ui_widget::SearchField(CallvoteSearchCtx, &m_FilterInput, QuickSearch, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&m_FilterInput, &QuickSearch, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameCallvoteTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerControl(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext CallvoteTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("CallvoteTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"ingame_callvote_text_inputs\");"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::TextField(CallvoteTextInputCtx, &m_CallvoteReasonInput, Reason, Localize(\"Reason\"), 14.0f);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::TextField(CallvoteTextInputCtx, &s_VoteDescriptionInput, Button, Localize(\"Vote description\"), 14.0f);"), std::string::npos);
	EXPECT_NE(Body.find("ui_widget::TextField(CallvoteTextInputCtx, &s_VoteCommandInput, Button, Localize(\"Vote command\"), 14.0f);"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_CallvoteReasonInput, &Reason, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_VoteDescriptionInput, &Button, 14.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_VoteCommandInput, &Button, 14.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameUnfinishedMapsPlayerNameUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderUnfinishedMaps(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TextFieldPos = Body.find("if(ui_widget::TextField(UnfinishedMapsTextInputCtx, &s_PlayerNameInput, Row, Client()->PlayerName(), 12.0f))");
	const size_t DirtyPos = Body.find("s_NameDirty = true;", TextFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext UnfinishedMapsTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("UnfinishedMapsTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"ingame_unfinished_maps_text_inputs\");"), std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(DirtyPos, std::string::npos);
	EXPECT_LT(TextFieldPos, DirtyPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_PlayerNameInput, &Row, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameMapNoteUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfo(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TextFieldPos = Body.find("if(ui_widget::TextField(ServerInfoTextInputCtx, &s_MapNoteInput, NoteInput, Localize(\"Note\"), FontSizeBody * 0.85f))");
	const size_t SaveNotePos = Body.find("GameClient()->m_TClient.SetMapNote(CurrentServerInfo.m_aMap, s_MapNoteInput.GetString());", TextFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext ServerInfoTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("ServerInfoTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"ingame_server_info_text_inputs\");"), std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(SaveNotePos, std::string::npos);
	EXPECT_LT(TextFieldPos, SaveNotePos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_MapNoteInput, &NoteInput, FontSizeBody * 0.85f"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoBrowserSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderDemoBrowserButtons(CUIRect ButtonsView, bool WasListboxItemActivated)");
	ASSERT_FALSE(Body.empty());

	const size_t NewUiSearchPos = Body.find("ui_widget::SearchField(DemoBrowserSearchCtx, &m_DemoSearchInput, DemoSearch, 13.0f");
	const size_t NewUiRefreshPos = Body.find("RefreshFilteredDemos();", NewUiSearchPos);
	const size_t LegacySearchPos = Body.find("ui_widget::SearchField(DemoBrowserSearchCtx, &m_DemoSearchInput, DemoSearch, 14.0f");
	const size_t LegacyRefreshPos = Body.find("RefreshFilteredDemos();", LegacySearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext DemoBrowserSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("DemoBrowserSearchCtx.m_ScopeHash = MakeUiScopeHash(\"demo_browser_search\");"), std::string::npos);
	EXPECT_NE(NewUiSearchPos, std::string::npos);
	EXPECT_NE(NewUiRefreshPos, std::string::npos);
	EXPECT_LT(NewUiSearchPos, NewUiRefreshPos);
	EXPECT_NE(LegacySearchPos, std::string::npos);
	EXPECT_NE(LegacyRefreshPos, std::string::npos);
	EXPECT_LT(LegacySearchPos, LegacyRefreshPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&m_DemoSearchInput, &DemoSearch"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerBrowserStatusInputsUseSharedQmFields)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserStatusBox(CUIRect StatusBox, bool WasListboxItemActivated)");
	ASSERT_FALSE(Body.empty());

	const size_t SearchHotkeyPos = Body.find("Input()->KeyPress(KEY_F) && Input()->ModifierIsPressed()");
	const size_t SearchPopupGuardPos = Body.find("!Ui()->IsPopupOpen()", SearchHotkeyPos > 40 ? SearchHotkeyPos - 40 : 0);
	const size_t SearchSetActivePos = Body.find("Ui()->SetActiveItem(&s_FilterInput)", SearchHotkeyPos);
	const size_t SearchSelectAllPos = Body.find("s_FilterInput.SelectAll();", SearchHotkeyPos);
	const size_t SearchFieldPos = Body.find("ui_widget::SearchField(ServerBrowserSearchCtx, &s_FilterInput, QuickSearch, 12.0f, false)");
	const size_t SearchRefreshPos = Body.find("Client()->ServerBrowserUpdate();", SearchFieldPos);
	const size_t ExcludeHotkeyPos = Body.find("Input()->KeyPress(KEY_X) && Input()->ShiftIsPressed() && Input()->ModifierIsPressed()");
	const size_t ExcludePopupGuardPos = Body.find("!Ui()->IsPopupOpen()", ExcludeHotkeyPos > 40 ? ExcludeHotkeyPos - 40 : 0);
	const size_t ExcludeSetActivePos = Body.find("Ui()->SetActiveItem(&s_ExcludeInput)", ExcludeHotkeyPos);
	const size_t ExcludeSelectAllPos = Body.find("s_ExcludeInput.SelectAll();", ExcludeHotkeyPos);
	const size_t ExcludeFieldPos = Body.find("ui_widget::SearchField(ServerBrowserExcludeCtx, &s_ExcludeInput, QuickExclude, 12.0f, false)");
	const size_t ExcludeRefreshPos = Body.find("Client()->ServerBrowserUpdate();", ExcludeFieldPos);
	const size_t AddressFieldPos = Body.find("ui_widget::ClearableTextField(ServerBrowserAddressCtx, &s_ServerAddressInput, ServerAddrEditBox, nullptr, 12.0f)");
	const size_t RevealPos = Body.find("m_ServerBrowserShouldRevealSelection = true;", AddressFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext ServerBrowserSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("ServerBrowserSearchCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_search\");"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext ServerBrowserExcludeCtx;"), std::string::npos);
	EXPECT_NE(Body.find("ServerBrowserExcludeCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_exclude\");"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext ServerBrowserAddressCtx;"), std::string::npos);
	EXPECT_NE(Body.find("ServerBrowserAddressCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_address\");"), std::string::npos);
	EXPECT_NE(SearchHotkeyPos, std::string::npos);
	EXPECT_NE(SearchPopupGuardPos, std::string::npos);
	EXPECT_NE(SearchSetActivePos, std::string::npos);
	EXPECT_NE(SearchSelectAllPos, std::string::npos);
	EXPECT_NE(SearchFieldPos, std::string::npos);
	EXPECT_NE(SearchRefreshPos, std::string::npos);
	EXPECT_LT(SearchPopupGuardPos, SearchHotkeyPos);
	EXPECT_LT(SearchHotkeyPos, SearchFieldPos);
	EXPECT_LT(SearchHotkeyPos, SearchSetActivePos);
	EXPECT_LT(SearchSetActivePos, SearchSelectAllPos);
	EXPECT_LT(SearchSelectAllPos, SearchFieldPos);
	EXPECT_LT(SearchFieldPos, SearchRefreshPos);
	EXPECT_EQ(Body.find(";\n", SearchFieldPos), SearchRefreshPos + str_length("Client()->ServerBrowserUpdate()"));
	EXPECT_NE(ExcludeHotkeyPos, std::string::npos);
	EXPECT_NE(ExcludePopupGuardPos, std::string::npos);
	EXPECT_NE(ExcludeSetActivePos, std::string::npos);
	EXPECT_NE(ExcludeSelectAllPos, std::string::npos);
	EXPECT_NE(ExcludeFieldPos, std::string::npos);
	EXPECT_NE(ExcludeRefreshPos, std::string::npos);
	EXPECT_LT(ExcludePopupGuardPos, ExcludeHotkeyPos);
	EXPECT_LT(ExcludeHotkeyPos, ExcludeFieldPos);
	EXPECT_LT(ExcludeHotkeyPos, ExcludeSetActivePos);
	EXPECT_LT(ExcludeSetActivePos, ExcludeSelectAllPos);
	EXPECT_LT(ExcludeSelectAllPos, ExcludeFieldPos);
	EXPECT_LT(ExcludeFieldPos, ExcludeRefreshPos);
	EXPECT_EQ(Body.find(";\n", ExcludeFieldPos), ExcludeRefreshPos + str_length("Client()->ServerBrowserUpdate()"));
	EXPECT_NE(AddressFieldPos, std::string::npos);
	EXPECT_NE(RevealPos, std::string::npos);
	EXPECT_LT(AddressFieldPos, RevealPos);
	EXPECT_EQ(Body.find(";\n", AddressFieldPos), RevealPos + str_length("m_ServerBrowserShouldRevealSelection = true"));
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_FilterInput, &QuickSearch"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_ExcludeInput, &QuickExclude"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_ServerAddressInput, &ServerAddrEditBox"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerBrowserFiltersTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserFilters(CUIRect View)");
	ASSERT_FALSE(Body.empty());

	const size_t GameTypeInputPos = Body.find("static CLineInput s_GametypeInput(g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype));");
	const size_t GameTypeCtxPos = Body.find("IUiContext ServerBrowserGameTypeCtx;", GameTypeInputPos);
	const size_t GameTypeUiPos = Body.find("ServerBrowserGameTypeCtx.m_pUi = Ui();", GameTypeCtxPos);
	const size_t GameTypeScopePos = Body.find("ServerBrowserGameTypeCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_filter_game_type\");", GameTypeCtxPos);
	const size_t GameTypeFieldPos = Body.find("ui_widget::TextField(ServerBrowserGameTypeCtx, &s_GametypeInput, Button, nullptr, FontSize)", GameTypeScopePos);
	const size_t GameTypeRefreshPos = Body.find("Client()->ServerBrowserUpdate();", GameTypeFieldPos);
	const size_t AddressInputPos = Body.find("static CLineInput s_FilterServerAddressInput(g_Config.m_BrFilterServerAddress, sizeof(g_Config.m_BrFilterServerAddress));");
	const size_t AddressCtxPos = Body.find("IUiContext ServerBrowserFilterAddressCtx;", AddressInputPos);
	const size_t AddressUiPos = Body.find("ServerBrowserFilterAddressCtx.m_pUi = Ui();", AddressCtxPos);
	const size_t AddressScopePos = Body.find("ServerBrowserFilterAddressCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_filter_address\");", AddressCtxPos);
	const size_t AddressFieldPos = Body.find("ui_widget::TextField(ServerBrowserFilterAddressCtx, &s_FilterServerAddressInput, Button, nullptr, FontSize)", AddressScopePos);
	const size_t AddressRefreshPos = Body.find("Client()->ServerBrowserUpdate();", AddressFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(GameTypeInputPos, std::string::npos);
	EXPECT_NE(GameTypeCtxPos, std::string::npos);
	EXPECT_NE(GameTypeUiPos, std::string::npos);
	EXPECT_NE(GameTypeScopePos, std::string::npos);
	EXPECT_NE(GameTypeFieldPos, std::string::npos);
	EXPECT_NE(GameTypeRefreshPos, std::string::npos);
	EXPECT_LT(GameTypeInputPos, GameTypeCtxPos);
	EXPECT_LT(GameTypeCtxPos, GameTypeUiPos);
	EXPECT_LT(GameTypeUiPos, GameTypeScopePos);
	EXPECT_LT(GameTypeScopePos, GameTypeFieldPos);
	EXPECT_LT(GameTypeFieldPos, GameTypeRefreshPos);
	EXPECT_EQ(Body.find(";\n", GameTypeFieldPos), GameTypeRefreshPos + str_length("Client()->ServerBrowserUpdate()"));
	EXPECT_NE(AddressInputPos, std::string::npos);
	EXPECT_NE(AddressCtxPos, std::string::npos);
	EXPECT_NE(AddressUiPos, std::string::npos);
	EXPECT_NE(AddressScopePos, std::string::npos);
	EXPECT_NE(AddressFieldPos, std::string::npos);
	EXPECT_NE(AddressRefreshPos, std::string::npos);
	EXPECT_LT(AddressInputPos, AddressCtxPos);
	EXPECT_LT(AddressCtxPos, AddressUiPos);
	EXPECT_LT(AddressUiPos, AddressScopePos);
	EXPECT_LT(AddressScopePos, AddressFieldPos);
	EXPECT_LT(AddressFieldPos, AddressRefreshPos);
	EXPECT_EQ(Body.find(";\n", AddressFieldPos), AddressRefreshPos + str_length("Client()->ServerBrowserUpdate()"));
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_GametypeInput, &Button, FontSize"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_FilterServerAddressInput, &Button, FontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerBrowserAddFriendInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserFriends(CUIRect View)");
	ASSERT_FALSE(Body.empty());

	const size_t NameInputPos = Body.find("static CLineInputBuffered<MAX_NAME_LENGTH> s_NameInput;");
	const size_t TextInputCtxPos = Body.find("IUiContext ServerBrowserAddFriendTextInputCtx;", NameInputPos);
	const size_t TextInputUiPos = Body.find("ServerBrowserAddFriendTextInputCtx.m_pUi = Ui();", TextInputCtxPos);
	const size_t TextInputScopePos = Body.find("ServerBrowserAddFriendTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_add_friend_text_inputs\");", TextInputUiPos);
	const size_t NameFieldPos = Body.find("ui_widget::TextField(ServerBrowserAddFriendTextInputCtx, &s_NameInput, Button, nullptr, FontSize + 2.0f);", TextInputScopePos);
	const size_t ClanInputPos = Body.find("static CLineInputBuffered<MAX_CLAN_LENGTH> s_ClanInput;", NameFieldPos);
	const size_t ClanFieldPos = Body.find("ui_widget::TextField(ServerBrowserAddFriendTextInputCtx, &s_ClanInput, Button, nullptr, FontSize + 2.0f);", ClanInputPos);
	const size_t AddFriendPos = Body.find("GameClient()->Friends()->AddFriend(s_NameInput.GetString(), s_ClanInput.GetString(), pCategory);", ClanFieldPos);
	const size_t ClearNamePos = Body.find("s_NameInput.Clear();", AddFriendPos);
	const size_t ClearClanPos = Body.find("s_ClanInput.Clear();", ClearNamePos);
	const size_t FriendlistUpdatePos = Body.find("FriendlistOnUpdate();", ClearClanPos);
	const size_t ServerBrowserUpdatePos = Body.find("Client()->ServerBrowserUpdate();", FriendlistUpdatePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(NameInputPos, std::string::npos);
	EXPECT_NE(TextInputCtxPos, std::string::npos);
	EXPECT_NE(TextInputUiPos, std::string::npos);
	EXPECT_NE(TextInputScopePos, std::string::npos);
	EXPECT_NE(NameFieldPos, std::string::npos);
	EXPECT_NE(ClanInputPos, std::string::npos);
	EXPECT_NE(ClanFieldPos, std::string::npos);
	EXPECT_NE(AddFriendPos, std::string::npos);
	EXPECT_NE(ClearNamePos, std::string::npos);
	EXPECT_NE(ClearClanPos, std::string::npos);
	EXPECT_NE(FriendlistUpdatePos, std::string::npos);
	EXPECT_NE(ServerBrowserUpdatePos, std::string::npos);
	EXPECT_LT(NameInputPos, TextInputCtxPos);
	EXPECT_LT(TextInputCtxPos, TextInputUiPos);
	EXPECT_LT(TextInputUiPos, TextInputScopePos);
	EXPECT_LT(TextInputScopePos, NameFieldPos);
	EXPECT_LT(NameFieldPos, ClanInputPos);
	EXPECT_LT(ClanInputPos, ClanFieldPos);
	EXPECT_LT(ClanFieldPos, AddFriendPos);
	EXPECT_LT(AddFriendPos, ClearNamePos);
	EXPECT_LT(ClearNamePos, ClearClanPos);
	EXPECT_LT(ClearClanPos, FriendlistUpdatePos);
	EXPECT_LT(FriendlistUpdatePos, ServerBrowserUpdatePos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_NameInput, &Button, FontSize + 2.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_ClanInput, &Button, FontSize + 2.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerBrowserScrollRegionsUseSharedQmScrollPresets)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string FilterBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserDDNetFilter(CUIRect View,\n\tIFilterList &Filter,\n\tfloat ItemHeight, int MaxItems, int ItemsPerRow,\n\tCScrollRegion &ScrollRegion, std::vector<unsigned char> &vItemIds,\n\tbool UpdateCommunityCacheOnChange, bool HideScrollbar,\n\tconst std::function<const char *(int ItemIndex)> &GetItemName,\n\tconst std::function<void(int ItemIndex, CUIRect Item, const void *pItemId, bool Active)> &RenderItem)");
	const std::string FriendsBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserFriends(CUIRect View)");
	const std::string InfoScoreboardBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserInfoScoreboard(CUIRect View, const CServerInfo *pSelectedServer)");
	const std::string QmBody = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerbrowserQm(CUIRect View)");
	ASSERT_FALSE(FilterBody.empty());
	ASSERT_FALSE(FriendsBody.empty());
	ASSERT_FALSE(InfoScoreboardBody.empty());
	ASSERT_FALSE(QmBody.empty());

	EXPECT_NE(Source.find("#include <game/client/ui_scrollregion.h>"), std::string::npos);
	EXPECT_NE(FilterBody.find("ScrollRequest.m_Profile = EQmScrollProfile::FILTER_GRID;"), std::string::npos);
	EXPECT_NE(FilterBody.find("ScrollRequest.m_RowExtent = ItemHeight;"), std::string::npos);
	EXPECT_NE(FilterBody.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy(ScrollRequest);"), std::string::npos);
	EXPECT_NE(FilterBody.find("CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);"), std::string::npos);
	EXPECT_EQ(FilterBody.find("m_ScrollUnit = 2.0f * ItemHeight"), std::string::npos);
	EXPECT_EQ(FilterBody.find("ScrollParams.m_ScrollbarThickness = 10.0f;"), std::string::npos);
	EXPECT_EQ(FilterBody.find("ScrollParams.m_ScrollbarMargin = 3.0f;"), std::string::npos);

	EXPECT_NE(FriendsBody.find("CScrollRegionParams ScrollParams = QmScrollRegionParamsForSize(EQmScrollSize::MEDIUM);"), std::string::npos);
	EXPECT_NE(FriendsBody.find("ScrollParams.m_ScrollUnit = 80.0f;"), std::string::npos);
	EXPECT_EQ(FriendsBody.find("m_ForceShowScrollbar"), std::string::npos);
	EXPECT_EQ(FriendsBody.find("ScrollParams.m_ScrollbarThickness = 16.0f;"), std::string::npos);
	EXPECT_EQ(FriendsBody.find("ScrollParams.m_ScrollbarMargin = 5.0f;"), std::string::npos);

	EXPECT_EQ(InfoScoreboardBody.find("QmScrollRegionParamsForSize"), std::string::npos);
	EXPECT_EQ(InfoScoreboardBody.find("SetScrollbarWidth"), std::string::npos);
	EXPECT_EQ(InfoScoreboardBody.find("SetScrollbarMargin"), std::string::npos);
	EXPECT_EQ(InfoScoreboardBody.find("s_ListBox.SetScrollbarWidth(16.0f);"), std::string::npos);
	EXPECT_EQ(InfoScoreboardBody.find("s_ListBox.SetScrollbarMargin(5.0f);"), std::string::npos);

	EXPECT_EQ(QmBody.find("QmScrollRegionParamsForSize"), std::string::npos);
	EXPECT_EQ(QmBody.find("SetScrollbarWidth"), std::string::npos);
	EXPECT_EQ(QmBody.find("SetScrollbarMargin"), std::string::npos);
	EXPECT_EQ(QmBody.find("s_QmServerListBox.SetScrollbarWidth(16.0f);"), std::string::npos);
	EXPECT_EQ(QmBody.find("s_QmServerListBox.SetScrollbarMargin(5.0f);"), std::string::npos);
}

TEST(QmMonitoringHelpers, CountryFilterUsesHiddenRailScrollPolicy)
{
	const std::string Browser = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string CountryFilterBody = ExtractSourceFunctionBody(Browser, "void CMenus::RenderServerbrowserCountriesFilter(CUIRect View)");
	ASSERT_FALSE(CountryFilterBody.empty());
	EXPECT_NE(CountryFilterBody.find("true, GetItemName, RenderItem"), std::string::npos);
	const std::string FilterBody = ExtractSourceFunctionBody(Browser, "void CMenus::RenderServerbrowserDDNetFilter(CUIRect View,");
	ASSERT_FALSE(FilterBody.empty());
	EXPECT_NE(FilterBody.find("ScrollParams.m_HideScrollbar = HideScrollbar;"), std::string::npos);
}

TEST(QmMonitoringHelpers, IngameMotdScrollRegionUsesSharedQmScrollPreset)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_ingame.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderServerInfoMotd(CUIRect Motd)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Source.find("#include <game/client/ui_scrollregion.h>"), std::string::npos);
	EXPECT_NE(Body.find("CScrollRegionParams ScrollParams = QmScrollRegionParamsForSize(EQmScrollSize::MEDIUM);"), std::string::npos);
	EXPECT_NE(Body.find("ScrollParams.m_ScrollUnit = 5 * MotdFontSize;"), std::string::npos);
	EXPECT_EQ(Body.find("CScrollRegionParams ScrollParams;\n\tScrollParams.m_ScrollUnit"), std::string::npos);
}

TEST(QmMonitoringHelpers, ServerBrowserFriendPopupsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_browser.cpp");
	const std::string CategoryBody = ExtractSourceFunctionBody(Source, "CUi::EPopupMenuFunctionResult CMenus::PopupFriendsCategory(void *pContext, CUIRect View, bool Active)");
	const std::string NoteBody = ExtractSourceFunctionBody(Source, "CUi::EPopupMenuFunctionResult CMenus::PopupFriendNote(void *pContext, CUIRect View, bool Active)");
	ASSERT_FALSE(CategoryBody.empty());
	ASSERT_FALSE(NoteBody.empty());

	const size_t CategoryLabelPos = CategoryBody.find("pMenus->Ui()->DoLabel(&Label, pPopupContext->m_Mode == CFriendsCategoryPopupContext::MODE_ADD ? Localize(\"Category name\") : Localize(\"New category name\"), FontSize, TEXTALIGN_ML);");
	const size_t CategoryCtxPos = CategoryBody.find("IUiContext FriendsCategoryTextInputCtx;", CategoryLabelPos);
	const size_t CategoryUiPos = CategoryBody.find("FriendsCategoryTextInputCtx.m_pUi = pMenus->Ui();", CategoryCtxPos);
	const size_t CategoryScopePos = CategoryBody.find("FriendsCategoryTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_friends_category_text_input\");", CategoryUiPos);
	const size_t CategoryFieldPos = CategoryBody.find("ui_widget::TextField(FriendsCategoryTextInputCtx, &pPopupContext->m_NameInput, Input, nullptr, FontSize + 1.0f);", CategoryScopePos);
	const size_t CategoryCancelPos = CategoryBody.find("const bool CancelPressed", CategoryFieldPos);
	const size_t CategoryEscapePos = CategoryBody.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)", CategoryCancelPos);
	const size_t CategoryConfirmPos = CategoryBody.find("const bool ConfirmPressed", CategoryFieldPos);
	const size_t CategoryEnterPos = CategoryBody.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)", CategoryConfirmPos);
	const size_t CategoryTrimPos = CategoryBody.find("str_copy(aCategory, str_utf8_skip_whitespaces(pPopupContext->m_NameInput.GetString()), sizeof(aCategory));", CategoryConfirmPos);
	const size_t CategoryUpdatePos = CategoryBody.find("pMenus->FriendlistOnUpdate();", CategoryTrimPos);
	const size_t NoteLabelPos = NoteBody.find("pMenus->Ui()->DoLabel(&Label, Localize(\"Friend note\"), FontSize, TEXTALIGN_ML);");
	const size_t NoteCtxPos = NoteBody.find("IUiContext FriendNoteTextInputCtx;", NoteLabelPos);
	const size_t NoteUiPos = NoteBody.find("FriendNoteTextInputCtx.m_pUi = pMenus->Ui();", NoteCtxPos);
	const size_t NoteScopePos = NoteBody.find("FriendNoteTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"server_browser_friend_note_text_input\");", NoteUiPos);
	const size_t NoteFieldPos = NoteBody.find("ui_widget::TextField(FriendNoteTextInputCtx, &pPopupContext->m_NoteInput, Input, nullptr, FontSize + 1.0f);", NoteScopePos);
	const size_t NoteCancelPos = NoteBody.find("const bool CancelPressed", NoteFieldPos);
	const size_t NoteEscapePos = NoteBody.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)", NoteCancelPos);
	const size_t NoteConfirmPos = NoteBody.find("const bool ConfirmPressed", NoteFieldPos);
	const size_t NoteEnterPos = NoteBody.find("pMenus->Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER)", NoteConfirmPos);
	const size_t NoteCopyPos = NoteBody.find("str_copy(aNote, pPopupContext->m_NoteInput.GetString(), sizeof(aNote));", NoteConfirmPos);
	const size_t NoteSavePos = NoteBody.find("pMenus->GameClient()->Friends()->SetFriendNote(pPopupContext->m_aName, pPopupContext->m_aClan, aNote);", NoteCopyPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(CategoryLabelPos, std::string::npos);
	EXPECT_NE(CategoryCtxPos, std::string::npos);
	EXPECT_NE(CategoryUiPos, std::string::npos);
	EXPECT_NE(CategoryScopePos, std::string::npos);
	EXPECT_NE(CategoryFieldPos, std::string::npos);
	EXPECT_NE(CategoryCancelPos, std::string::npos);
	EXPECT_NE(CategoryEscapePos, std::string::npos);
	EXPECT_NE(CategoryConfirmPos, std::string::npos);
	EXPECT_NE(CategoryEnterPos, std::string::npos);
	EXPECT_NE(CategoryTrimPos, std::string::npos);
	EXPECT_NE(CategoryUpdatePos, std::string::npos);
	EXPECT_LT(CategoryLabelPos, CategoryCtxPos);
	EXPECT_LT(CategoryCtxPos, CategoryUiPos);
	EXPECT_LT(CategoryUiPos, CategoryScopePos);
	EXPECT_LT(CategoryScopePos, CategoryFieldPos);
	EXPECT_LT(CategoryFieldPos, CategoryCancelPos);
	EXPECT_LT(CategoryCancelPos, CategoryEscapePos);
	EXPECT_LT(CategoryEscapePos, CategoryConfirmPos);
	EXPECT_LT(CategoryFieldPos, CategoryConfirmPos);
	EXPECT_LT(CategoryConfirmPos, CategoryEnterPos);
	EXPECT_LT(CategoryEnterPos, CategoryTrimPos);
	EXPECT_LT(CategoryConfirmPos, CategoryTrimPos);
	EXPECT_LT(CategoryTrimPos, CategoryUpdatePos);
	EXPECT_NE(NoteLabelPos, std::string::npos);
	EXPECT_NE(NoteCtxPos, std::string::npos);
	EXPECT_NE(NoteUiPos, std::string::npos);
	EXPECT_NE(NoteScopePos, std::string::npos);
	EXPECT_NE(NoteFieldPos, std::string::npos);
	EXPECT_NE(NoteCancelPos, std::string::npos);
	EXPECT_NE(NoteEscapePos, std::string::npos);
	EXPECT_NE(NoteConfirmPos, std::string::npos);
	EXPECT_NE(NoteEnterPos, std::string::npos);
	EXPECT_NE(NoteCopyPos, std::string::npos);
	EXPECT_NE(NoteSavePos, std::string::npos);
	EXPECT_LT(NoteLabelPos, NoteCtxPos);
	EXPECT_LT(NoteCtxPos, NoteUiPos);
	EXPECT_LT(NoteUiPos, NoteScopePos);
	EXPECT_LT(NoteScopePos, NoteFieldPos);
	EXPECT_LT(NoteFieldPos, NoteCancelPos);
	EXPECT_LT(NoteCancelPos, NoteEscapePos);
	EXPECT_LT(NoteEscapePos, NoteConfirmPos);
	EXPECT_LT(NoteFieldPos, NoteConfirmPos);
	EXPECT_LT(NoteConfirmPos, NoteEnterPos);
	EXPECT_LT(NoteEnterPos, NoteCopyPos);
	EXPECT_LT(NoteConfirmPos, NoteCopyPos);
	EXPECT_LT(NoteCopyPos, NoteSavePos);
	EXPECT_EQ(CategoryBody.find("pMenus->Ui()->DoEditBox(&pPopupContext->m_NameInput, &Input, FontSize + 1.0f"), std::string::npos);
	EXPECT_EQ(NoteBody.find("pMenus->Ui()->DoEditBox(&pPopupContext->m_NoteInput, &Input, FontSize + 1.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoSliceNameInputUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_demo.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderDemoPlayerSliceSavePopup(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TextFieldPos = Body.find("ui_widget::TextField(DemoSliceTextInputCtx, &m_DemoSliceInput, NameBox, Localize(\"New name\"), 12.0f);");
	const size_t OkButtonPos = Body.find("if(DoButton_Menu(&s_ButtonOk", TextFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext DemoSliceTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("DemoSliceTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"demo_slice_text_input\");"), std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(OkButtonPos, std::string::npos);
	EXPECT_LT(TextFieldPos, OkButtonPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_DemoSliceInput, &NameBox, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoRenamePopupUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderPopupFullscreen(CUIRect Screen)");
	ASSERT_FALSE(Body.empty());

	const size_t RenamePopupPos = Body.find("else if(m_Popup == POPUP_RENAME_DEMO)");
	const size_t OkButtonPos = Body.find("if(DoButton_Menu(&s_ButtonOk", RenamePopupPos);
	const size_t TextFieldPos = Body.find("ui_widget::TextField(DemoRenameTextInputCtx, &m_DemoRenameInput, TextBox, Localize(\"New name\"), 12.0f);", OkButtonPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext DemoRenameTextInputCtx;", RenamePopupPos), std::string::npos);
	EXPECT_NE(Body.find("DemoRenameTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"demo_rename_text_input\");", RenamePopupPos), std::string::npos);
	EXPECT_NE(RenamePopupPos, std::string::npos);
	EXPECT_NE(OkButtonPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(OkButtonPos, TextFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_DemoRenameInput, &TextBox, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, DemoRenderPopupUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderPopupFullscreen(CUIRect Screen)");
	ASSERT_FALSE(Body.empty());

	const size_t RenderPopupPos = Body.find("else if(m_Popup == POPUP_RENDER_DEMO)");
	const size_t VideoNameLabelPos = Body.find("Ui()->DoLabel(&Label, Localize(\"Video name:\"), 12.8f, TEXTALIGN_ML);", RenderPopupPos);
	const size_t TextFieldPos = Body.find("ui_widget::TextField(DemoRenderTextInputCtx, &m_DemoRenderInput, TextBox, Localize(\"Video name\"), 12.8f);", VideoNameLabelPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext DemoRenderTextInputCtx;", RenderPopupPos), std::string::npos);
	EXPECT_NE(Body.find("DemoRenderTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"demo_render_text_input\");", RenderPopupPos), std::string::npos);
	EXPECT_NE(RenderPopupPos, std::string::npos);
	EXPECT_NE(VideoNameLabelPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(VideoNameLabelPos, TextFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_DemoRenderInput, &TextBox, 12.8f"), std::string::npos);
}

TEST(QmMonitoringHelpers, PopupClearableInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderPopupFullscreen(CUIRect Screen)");
	ASSERT_FALSE(Body.empty());

	const size_t PasswordBufferPos = Source.find("m_PasswordInput.SetBuffer(g_Config.m_Password, sizeof(g_Config.m_Password));");
	const size_t PasswordHiddenPos = Source.find("m_PasswordInput.SetHidden(true);", PasswordBufferPos);
	const size_t PasswordPopupPos = Body.find("else if(m_Popup == POPUP_PASSWORD)");
	const size_t ConnectPos = Body.find("Client()->Connect(aAddr, g_Config.m_Password);", PasswordPopupPos);
	const size_t PasswordLabelPos = Body.find("Ui()->DoLabel(&Label, Localize(\"Password\"), 18.0f, TEXTALIGN_ML);", PasswordPopupPos);
	const size_t PasswordCtxPos = Body.find("IUiContext PasswordPopupTextInputCtx;", PasswordLabelPos);
	const size_t PasswordUiPos = Body.find("PasswordPopupTextInputCtx.m_pUi = Ui();", PasswordCtxPos);
	const size_t PasswordScopePos = Body.find("PasswordPopupTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"password_popup_text_input\");", PasswordUiPos);
	const size_t PasswordFieldPos = Body.find("ui_widget::ClearableTextField(PasswordPopupTextInputCtx, &m_PasswordInput, TextBox, nullptr, 12.0f);", PasswordScopePos);
	const size_t AddressLabelPos = Body.find("Ui()->DoLabel(&Label, Localize(\"Address\"), 18.0f, TEXTALIGN_ML);", PasswordFieldPos);
	const size_t SaveSkinPopupPos = Body.find("else if(m_Popup == POPUP_SAVE_SKIN)");
	const size_t YesButtonPos = Body.find("if(DoButton_Menu(&s_ButtonYes", SaveSkinPopupPos);
	const size_t ValidFilenamePos = Body.find("str_valid_filename(m_SkinNameInput.GetString())", YesButtonPos);
	const size_t SpecialSkinPos = Body.find("CSkins7::IsSpecialSkin(m_SkinNameInput.GetString())", ValidFilenamePos);
	const size_t SaveSkinFilePos = Body.find("GameClient()->m_Skins7.SaveSkinfile(m_SkinNameInput.GetString(), m_Dummy)", SpecialSkinPos);
	const size_t SaveSkinLabelPos = Body.find("Ui()->DoLabel(&Label, Localize(\"Name\"), 18.0f, TEXTALIGN_ML);", YesButtonPos);
	const size_t SaveSkinCtxPos = Body.find("IUiContext SaveSkinTextInputCtx;", SaveSkinLabelPos);
	const size_t SaveSkinUiPos = Body.find("SaveSkinTextInputCtx.m_pUi = Ui();", SaveSkinCtxPos);
	const size_t SaveSkinScopePos = Body.find("SaveSkinTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"save_skin_text_input\");", SaveSkinUiPos);
	const size_t SaveSkinFieldPos = Body.find("ui_widget::ClearableTextField(SaveSkinTextInputCtx, &m_SkinNameInput, TextBox, nullptr, 12.0f);", SaveSkinScopePos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(PasswordBufferPos, std::string::npos);
	EXPECT_NE(PasswordHiddenPos, std::string::npos);
	EXPECT_LT(PasswordBufferPos, PasswordHiddenPos);
	EXPECT_NE(PasswordPopupPos, std::string::npos);
	EXPECT_NE(ConnectPos, std::string::npos);
	EXPECT_NE(PasswordLabelPos, std::string::npos);
	EXPECT_NE(PasswordCtxPos, std::string::npos);
	EXPECT_NE(PasswordUiPos, std::string::npos);
	EXPECT_NE(PasswordScopePos, std::string::npos);
	EXPECT_NE(PasswordFieldPos, std::string::npos);
	EXPECT_NE(AddressLabelPos, std::string::npos);
	EXPECT_LT(PasswordPopupPos, ConnectPos);
	EXPECT_LT(ConnectPos, PasswordLabelPos);
	EXPECT_LT(PasswordLabelPos, PasswordCtxPos);
	EXPECT_LT(PasswordCtxPos, PasswordUiPos);
	EXPECT_LT(PasswordUiPos, PasswordScopePos);
	EXPECT_LT(PasswordScopePos, PasswordFieldPos);
	EXPECT_LT(PasswordFieldPos, AddressLabelPos);
	EXPECT_NE(SaveSkinPopupPos, std::string::npos);
	EXPECT_NE(YesButtonPos, std::string::npos);
	EXPECT_NE(ValidFilenamePos, std::string::npos);
	EXPECT_NE(SpecialSkinPos, std::string::npos);
	EXPECT_NE(SaveSkinFilePos, std::string::npos);
	EXPECT_NE(SaveSkinLabelPos, std::string::npos);
	EXPECT_NE(SaveSkinCtxPos, std::string::npos);
	EXPECT_NE(SaveSkinUiPos, std::string::npos);
	EXPECT_NE(SaveSkinScopePos, std::string::npos);
	EXPECT_NE(SaveSkinFieldPos, std::string::npos);
	EXPECT_LT(YesButtonPos, ValidFilenamePos);
	EXPECT_LT(ValidFilenamePos, SpecialSkinPos);
	EXPECT_LT(SpecialSkinPos, SaveSkinFilePos);
	EXPECT_LT(SaveSkinFilePos, SaveSkinLabelPos);
	EXPECT_LT(YesButtonPos, SaveSkinLabelPos);
	EXPECT_LT(SaveSkinLabelPos, SaveSkinCtxPos);
	EXPECT_LT(SaveSkinCtxPos, SaveSkinUiPos);
	EXPECT_LT(SaveSkinUiPos, SaveSkinScopePos);
	EXPECT_LT(SaveSkinScopePos, SaveSkinFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_PasswordInput, &TextBox, 12.0f"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&m_SkinNameInput, &TextBox, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, FirstLaunchPlayerNameUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderPopupFullscreen(CUIRect Screen)");
	ASSERT_FALSE(Body.empty());

	const size_t FirstLaunchPopupPos = Body.find("else if(m_Popup == POPUP_FIRST_LAUNCH)");
	const size_t EmptyTextPos = Body.find("s_PlayerNameInput.SetEmptyText(Client()->PlayerName());", FirstLaunchPopupPos);
	const size_t TextFieldPos = Body.find("ui_widget::TextField(FirstLaunchTextInputCtx, &s_PlayerNameInput, TextBox, Client()->PlayerName(), 12.0f);", EmptyTextPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext FirstLaunchTextInputCtx;", FirstLaunchPopupPos), std::string::npos);
	EXPECT_NE(Body.find("FirstLaunchTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"first_launch_text_input\");", FirstLaunchPopupPos), std::string::npos);
	EXPECT_NE(FirstLaunchPopupPos, std::string::npos);
	EXPECT_NE(EmptyTextPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_LT(EmptyTextPos, TextFieldPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_PlayerNameInput, &TextBox, 12.0f"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t SearchPos = Body.find("ui_widget::SearchField(AssetsSearchCtx, &s_aFilterInputs[s_CurCustomTab], QuickSearch, 14.0f");
	const size_t RefreshPos = Body.find("gs_aInitCustomList[s_CurCustomTab] = true;", SearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AssetsSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_assets_search\");"), std::string::npos);
	EXPECT_NE(SearchPos, std::string::npos);
	EXPECT_NE(RefreshPos, std::string::npos);
	EXPECT_LT(SearchPos, RefreshPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&s_aFilterInputs[s_CurCustomTab], &QuickSearch"), std::string::npos);
}

TEST(QmMonitoringHelpers, AudioPackEditorSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderAudioPackEditorScreen(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t SlotSearchPos = Body.find("ui_widget::InputField(AudioPackSlotSearchCtx, &m_AudioPackEditorState.m_FilterInput, SlotSearchInput, 12.0f");
	const size_t SlotFilterPos = Body.find("const char *pSlotFilter = m_AudioPackEditorState.m_FilterInput.GetString();", SlotSearchPos);
	const size_t CandidateSearchPos = Body.find("ui_widget::InputField(AudioPackCandidateSearchCtx, &m_AudioPackEditorState.m_CandidateFilterInput, CandidateSearchInput, 12.0f");
	const size_t CandidateFilterPos = Body.find("const char *pCandidateFilter = m_AudioPackEditorState.m_CandidateFilterInput.GetString();", CandidateSearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AudioPackSlotSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AudioPackSlotSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_audio_pack_slot_search\");"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AudioPackCandidateSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AudioPackCandidateSearchCtx.m_ScopeHash = MakeUiScopeHash(\"settings_audio_pack_candidate_search\");"), std::string::npos);
	EXPECT_NE(SlotSearchPos, std::string::npos);
	EXPECT_NE(SlotFilterPos, std::string::npos);
	EXPECT_LT(SlotSearchPos, SlotFilterPos);
	EXPECT_NE(CandidateSearchPos, std::string::npos);
	EXPECT_NE(CandidateFilterPos, std::string::npos);
	EXPECT_LT(CandidateSearchPos, CandidateFilterPos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&m_AudioPackEditorState.m_FilterInput, &SlotSearchInput"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox_Search(&m_AudioPackEditorState.m_CandidateFilterInput, &CandidateSearchInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, AudioPackEditorUsesStableSlotItemIds)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderAudioPackEditorScreen(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("static std::vector<int> s_vAudioPackEditorSlotItemIds;"), std::string::npos);
	EXPECT_NE(Body.find("s_vAudioPackEditorSlotItemIds.resize(vAllSlots.size());"), std::string::npos);
	EXPECT_NE(Body.find("s_AudioPackEditorSlotListBox.DoNextItem(&s_vAudioPackEditorSlotItemIds[SlotIndex], SelectedVisibleSlot == VisibleIndex)"), std::string::npos);
	EXPECT_EQ(Body.find("s_AudioPackEditorSlotListBox.DoNextItem(&Slot, SelectedVisibleSlot == VisibleIndex)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AudioPackDirectoryOpensWritableSaveFolder)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsSound(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("Storage()->GetCompletePath(IStorage::TYPE_SAVE, \"audio\", aBuf, sizeof(aBuf));"), std::string::npos);
	EXPECT_EQ(Body.find("Storage()->GetCompletePath(IStorage::TYPE_ALL, \"audio\""), std::string::npos);
}

TEST(QmMonitoringHelpers, SoundPageSwitchSkipsWholePageOffsetAnimation)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettings(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const bool AnimateSettingsPageSwitch = UseNewSettingsUi && g_Config.m_UiSettingsPage != SETTINGS_SOUND;"), std::string::npos);
	EXPECT_NE(Body.find("s_SettingsTransitionDirection = AnimateSettingsPageSwitch ? (g_Config.m_UiSettingsPage > s_PrevSettingsPage ? 1.0f : -1.0f) : 0.0f;"), std::string::npos);
	EXPECT_NE(Body.find("if(AnimateSettingsPageSwitch)\n\t\t\t\tTriggerUiSwitchAnimation(SettingsPageSwitchNode, 0.18f);"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEditorSearchUsesSharedQmSearchField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_assets_editor.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderAssetsEditorScreen(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t DonorSearchPos = Body.find("ui_widget::SearchField(AssetsEditorDonorSearchCtx, &s_aDonorSearchInputs[m_AssetsEditorState.m_Type], DonorSearchBox, EditBoxFontSize, false)");
	const size_t DonorFilterPos = Body.find("for(size_t Index = 0; Index < vFilteredDonorAssetIndices.size(); ++Index)", DonorSearchPos);
	const size_t MainSearchPos = Body.find("ui_widget::SearchField(AssetsEditorMainSearchCtx, &s_aMainSearchInputs[m_AssetsEditorState.m_Type], MainSearchBox, EditBoxFontSize, false)");
	const size_t MainFilterPos = Body.find("for(size_t Index = 0; Index < vFilteredMainAssetIndices.size(); ++Index)", MainSearchPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AssetsEditorDonorSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsEditorDonorSearchCtx.m_ScopeHash = MakeUiScopeHash(\"assets_editor_donor_search\");"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AssetsEditorMainSearchCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AssetsEditorMainSearchCtx.m_ScopeHash = MakeUiScopeHash(\"assets_editor_main_search\");"), std::string::npos);
	EXPECT_NE(DonorSearchPos, std::string::npos);
	EXPECT_NE(DonorFilterPos, std::string::npos);
	EXPECT_LT(DonorSearchPos, DonorFilterPos);
	EXPECT_NE(MainSearchPos, std::string::npos);
	EXPECT_NE(MainFilterPos, std::string::npos);
	EXPECT_LT(MainSearchPos, MainFilterPos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_aDonorSearchInputs[m_AssetsEditorState.m_Type], &DonorSearchBox"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoClearableEditBox(&s_aMainSearchInputs[m_AssetsEditorState.m_Type], &MainSearchBox"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsEditorExportNameUsesSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_assets_editor.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderAssetsEditorScreen(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t SetPlaceholderPos = Body.find("s_ExportNameInput.SetEmptyText(aExportPlaceholder);");
	const size_t TextInputCtxPos = Body.find("IUiContext AssetsEditorExportNameCtx;", SetPlaceholderPos);
	const size_t ScopeHashPos = Body.find("AssetsEditorExportNameCtx.m_ScopeHash = MakeUiScopeHash(\"assets_editor_export_name\");", TextInputCtxPos);
	const size_t TextFieldPos = Body.find("ui_widget::TextField(AssetsEditorExportNameCtx, &s_ExportNameInput, ExportRow, nullptr, EditBoxFontSize)", ScopeHashPos);
	const size_t CommitPos = Body.find("AssetsEditorCommitExportNameForType();", TextFieldPos);
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(SetPlaceholderPos, std::string::npos);
	EXPECT_NE(TextInputCtxPos, std::string::npos);
	EXPECT_NE(ScopeHashPos, std::string::npos);
	EXPECT_NE(TextFieldPos, std::string::npos);
	EXPECT_NE(CommitPos, std::string::npos);
	EXPECT_LT(SetPlaceholderPos, TextInputCtxPos);
	EXPECT_LT(TextInputCtxPos, ScopeHashPos);
	EXPECT_LT(ScopeHashPos, TextFieldPos);
	EXPECT_LT(TextFieldPos, CommitPos);
	EXPECT_EQ(Body.find(";\n", TextFieldPos), CommitPos + str_length("AssetsEditorCommitExportNameForType()"));
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&s_ExportNameInput, &ExportRow, EditBoxFontSize"), std::string::npos);
}

TEST(QmMonitoringHelpers, AudioPackEditorTextInputsUseSharedQmTextField)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderAudioPackEditorScreen(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t PackNamePos = Body.find("ui_widget::InputField(AudioPackEditorTextInputCtx, &m_AudioPackEditorState.m_PackNameInput, PackInput, Localize(\"Pack name\"), EditorEditBoxFontSize)");
	const size_t RefreshPos = Body.find("AudioPackEditorRefreshCandidates();", PackNamePos);
	const size_t ManualPathPos = Body.find("ui_widget::InputField(AudioPackEditorTextInputCtx, &m_AudioPackEditorState.m_SourcePathInput, ManualInput, Localize(\"Manual source file\"), 12.0f);");
	EXPECT_NE(Source.find("#include <game/client/QmUi/UiForms.h>"), std::string::npos);
	EXPECT_NE(Body.find("IUiContext AudioPackEditorTextInputCtx;"), std::string::npos);
	EXPECT_NE(Body.find("AudioPackEditorTextInputCtx.m_ScopeHash = MakeUiScopeHash(\"settings_audio_pack_text_inputs\");"), std::string::npos);
	EXPECT_NE(PackNamePos, std::string::npos);
	EXPECT_NE(RefreshPos, std::string::npos);
	EXPECT_LT(PackNamePos, RefreshPos);
	EXPECT_NE(ManualPathPos, std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_AudioPackEditorState.m_PackNameInput, &PackInput"), std::string::npos);
	EXPECT_EQ(Body.find("Ui()->DoEditBox(&m_AudioPackEditorState.m_SourcePathInput, &ManualInput"), std::string::npos);
}

TEST(QmMonitoringHelpers, DropdownPopupUsesComputedGeometrySize)
{
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string UiHeader = ReadRepoFile("src/game/client/ui.h");
	const std::string DropdownHeader = ReadRepoFile("src/game/client/QmUi/QmDropdown.h");
	const std::string DropdownSource = ReadRepoFile("src/game/client/QmUi/QmDropdown.cpp");
	const std::string PopupBody = ExtractSourceFunctionBody(Ui, "void CUi::DoPopupMenu(const SPopupMenuId *pId, float X, float Y, float Width, float Height, void *pContext, FPopupMenuFunction pfnFunc, const SPopupMenuProperties &Props)");
	const std::string SelectionResetBody = ExtractSourceFunctionBody(Ui, "void CUi::SSelectionPopupContext::Reset()");
	const std::string Body = ExtractSourceFunctionBody(Ui, "void CUi::ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext)");
	const std::string PopupSelectionBody = ExtractSourceFunctionBody(Ui, "CUi::EPopupMenuFunctionResult CUi::PopupSelection(void *pContext, CUIRect View, bool Active)");
	const std::string RenderPopupsBody = ExtractSourceFunctionBody(Ui, "void CUi::RenderPopupMenus()");
	ASSERT_FALSE(PopupBody.empty());
	ASSERT_FALSE(SelectionResetBody.empty());
	ASSERT_FALSE(Body.empty());
	ASSERT_FALSE(PopupSelectionBody.empty());
	ASSERT_FALSE(RenderPopupsBody.empty());

	EXPECT_NE(UiHeader.find("bool m_AutoReposition = true;"), std::string::npos);
	EXPECT_NE(UiHeader.find("bool m_AnchorVisible = true;"), std::string::npos);
	EXPECT_NE(UiHeader.find("bool m_PopupVisible = true;"), std::string::npos);
	EXPECT_NE(SelectionResetBody.find("m_AnchorVisible = true;"), std::string::npos);
	EXPECT_NE(SelectionResetBody.find("m_PopupVisible = true;"), std::string::npos);
	EXPECT_NE(PopupBody.find("if(Props.m_AutoReposition)"), std::string::npos);
	EXPECT_NE(DropdownHeader.find("bool m_PopupVisible = false;"), std::string::npos);
	EXPECT_NE(DropdownSource.find("Result.m_PopupVisible = Result.m_Rect.w > 0.0f && Result.m_Rect.h > 0.0f && RectsOverlap(Result.m_Rect, ViewportRect);"), std::string::npos);
	EXPECT_NE(UiHeader.find("bool m_ClipToViewport = false;"), std::string::npos);
	EXPECT_NE(UiHeader.find("bool m_BlockUnderlyingScroll = false;"), std::string::npos);
	EXPECT_NE(UiHeader.find("CUIRect m_Viewport{};"), std::string::npos);
	EXPECT_NE(UiHeader.find("SQmDropdownPopupPolicy m_PopupPolicy;"), std::string::npos);
	EXPECT_NE(SelectionResetBody.find("m_BlockUnderlyingScroll = false;"), std::string::npos);
	EXPECT_NE(SelectionResetBody.find("m_Viewport = {};"), std::string::npos);
	EXPECT_NE(Body.find("QmResolveDropdownPopupPolicy("), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect &Viewport = pContext->m_Viewport;"), std::string::npos);
	EXPECT_NE(Body.find("QmComputeDropdownPopupGeometry(AnchorRect, Viewport, GeometryConfig);"), std::string::npos);
	EXPECT_NE(Body.find("const bool OwnsWheel = pContext->m_PopupVisible && QmDropdownPopupOwnsWheel("), std::string::npos);
	EXPECT_NE(Body.find("RegisterWheelOwner(pContext, EUiWheelOwnerPriority::POPUP, PopupRect, OwnsWheel);"), std::string::npos);
	EXPECT_NE(Body.find("pContext->m_BlockUnderlyingScroll = OwnsWheel;"), std::string::npos);
	EXPECT_NE(PopupSelectionBody.find("QmResolveScrollPolicy(ScrollRequest)"), std::string::npos);
	EXPECT_EQ(PopupSelectionBody.find("m_ScrollbarThickness = 10.0f"), std::string::npos);
	EXPECT_EQ(PopupSelectionBody.find("m_ScrollUnit = 3 *"), std::string::npos);
	EXPECT_NE(RenderPopupsBody.find("PopupMenu.m_Props.m_BlockUnderlyingScroll"), std::string::npos);
	EXPECT_NE(RenderPopupsBody.find("PopupMenu.m_Props.m_ClipToViewport"), std::string::npos);
	EXPECT_NE(Body.find("pContext->m_AnchorVisible = Geometry.m_AnchorVisible;"), std::string::npos);
	EXPECT_NE(Body.find("pContext->m_PopupVisible = Geometry.m_PopupVisible;"), std::string::npos);
	EXPECT_NE(Body.find("if(!pContext->m_AnchorVisible || !pContext->m_PopupVisible)"), std::string::npos);
	EXPECT_NE(Body.find("ClosePopupMenu(pContext);"), std::string::npos);
	EXPECT_NE(Body.find("float PopupWidth = pContext->m_Width;"), std::string::npos);
	EXPECT_NE(Body.find("float PopupHeightResolved = PopupHeight;"), std::string::npos);
	EXPECT_NE(Body.find("pContext->m_Props.m_AutoReposition = false;"), std::string::npos);
	EXPECT_NE(Body.find("PopupWidth = Geometry.m_Rect.w;"), std::string::npos);
	EXPECT_NE(Body.find("PopupHeightResolved = Geometry.m_Rect.h;"), std::string::npos);
	EXPECT_NE(Body.find("DoPopupMenu(pContext, X, Y, PopupWidth, PopupHeightResolved, pContext, PopupSelection, pContext->m_Props);"), std::string::npos);
	EXPECT_EQ(Body.find("DoPopupMenu(pContext, X, Y, pContext->m_Width, PopupHeight, pContext, PopupSelection, pContext->m_Props);"), std::string::npos);
}

TEST(QmMonitoringHelpers, ListBoxResolvesMenuScrollPolicyFromRowSemantics)
{
	const std::string Header = ReadRepoFile("src/game/client/ui_listbox.h");
	const std::string Source = ReadRepoFile("src/game/client/ui_listbox.cpp");
	const std::string DoStartBody = ExtractSourceFunctionBody(Source, "void CListBox::DoStart(float RowHeight, int NumItems, int ItemsPerRow, int RowsPerScroll, int SelectedIndex, const CUIRect *pRect, bool Background, int BackgroundCorners)");
	ASSERT_FALSE(DoStartBody.empty());

	EXPECT_NE(Header.find("EQmScrollProfile m_ScrollProfile = EQmScrollProfile::MENU_LIST;"), std::string::npos);
	EXPECT_NE(Header.find("void SetScrollProfile(EQmScrollProfile Profile)"), std::string::npos);
	EXPECT_NE(DoStartBody.find("ScrollRequest.m_Profile = m_ScrollProfile;"), std::string::npos);
	EXPECT_NE(DoStartBody.find("ScrollRequest.m_RowExtent = m_ListBoxRowHeight + m_AutoSpacing;"), std::string::npos);
	EXPECT_NE(DoStartBody.find("ScrollRequest.m_RowsPerStep = RowsPerScroll;"), std::string::npos);
	EXPECT_NE(DoStartBody.find("const SQmResolvedScrollPolicy ScrollPolicy = QmResolveScrollPolicy(ScrollRequest);"), std::string::npos);
	EXPECT_NE(DoStartBody.find("CScrollRegionParams ScrollParams = QmScrollRegionParamsFromPolicy(ScrollPolicy);"), std::string::npos);
	EXPECT_NE(DoStartBody.find("QmListBoxScrollbarMetric(ScrollParams.m_ScrollbarThickness, m_ScrollbarWidth, m_ScrollbarWidthOverridden)"), std::string::npos);
	EXPECT_NE(DoStartBody.find("ScrollParams.m_ScrollbarThickness = m_ScrollbarWidth;"), std::string::npos);
	EXPECT_EQ(DoStartBody.find("m_ScrollbarThickness = ScrollbarWidthMax()"), std::string::npos);
	EXPECT_EQ(DoStartBody.find("m_ScrollbarMargin = ScrollbarMargin()"), std::string::npos);
	EXPECT_EQ(DoStartBody.find("m_ForceShowScrollbar = ForceShowScrollbar"), std::string::npos);
}

TEST(QmMonitoringHelpers, ScrollRegionsOnlyReserveRailsForRealOverflow)
{
	const std::string Header = ReadRepoFile("src/game/client/ui_scrollregion.h");
	const std::string Source = ReadRepoFile("src/game/client/ui_scrollregion.cpp");
	const std::string EndBody = ExtractSourceFunctionBody(Source, "void CScrollRegion::End()");
	const std::string ScrollbarShownBody = ExtractSourceFunctionBody(Source, "bool CScrollRegion::ScrollbarShown() const");
	const std::string UpdateHotScrollRegionBody = ExtractSourceFunctionBody(Source, "void CScrollRegion::UpdateHotScrollRegion()");
	ASSERT_FALSE(EndBody.empty());
	ASSERT_FALSE(ScrollbarShownBody.empty());
	ASSERT_FALSE(UpdateHotScrollRegionBody.empty());

	EXPECT_EQ(Header.find("m_ForceShowScrollbar"), std::string::npos);
	EXPECT_NE(EndBody.find("if(!ContentOverflows())"), std::string::npos);
	EXPECT_EQ(EndBody.find("m_ForceShowScrollbar"), std::string::npos);
	EXPECT_NE(ScrollbarShownBody.find("return !m_Params.m_HideScrollbar && ContentOverflows();"), std::string::npos);
	EXPECT_NE(UpdateHotScrollRegionBody.find("if(ScrollbarShown())"), std::string::npos);
}

TEST(QmMonitoringHelpers, ScrollRegionDelegatesMutableScrollStateToQmScrollState)
{
	const std::string Header = ReadRepoFile("src/game/client/ui_scrollregion.h");
	const std::string Source = ReadRepoFile("src/game/client/ui_scrollregion.cpp");
	const std::string AnimationBody = ExtractSourceFunctionBody(Source, "void CScrollRegion::AdvanceAnimation()");
	const std::string SliderBody = ExtractSourceFunctionBody(Source, "void CScrollRegion::DoSlider()");
	ASSERT_FALSE(AnimationBody.empty());
	ASSERT_FALSE(SliderBody.empty());

	EXPECT_NE(Header.find("CQmScrollState m_ScrollState;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_ScrollPos;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimTimeMax;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimTime;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimInitScrollPos;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_AnimTargetScrollPos;"), std::string::npos);
	EXPECT_EQ(Header.find("float m_SliderGrabPos;"), std::string::npos);
	EXPECT_NE(AnimationBody.find("m_ScrollState.Advance("), std::string::npos);
	EXPECT_NE(SliderBody.find("m_ScrollState.SetOffset("), std::string::npos);
}

TEST(QmMonitoringHelpers, QmScrollControllerReceivesExternalMutableState)
{
	const std::string Header = ReadRepoFile("src/game/client/QmUi/QmScroll.h");
	const std::string Source = ReadRepoFile("src/game/client/QmUi/QmScroll.cpp");
	const std::string Containers = ReadRepoFile("src/game/client/QmUi/UiContainers.h");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string QmClient = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string UpdateBody = ExtractSourceFunctionBody(Source, "SQmScrollContainerFrame CQmScrollController::UpdateInternal(CQmScrollState &State, const CUIRect &ViewRect, float ContentHeight, float Dt, const SQmScrollContainerInput &Input, const SQmScrollContainerStyle &Style, const SQmScrollConfig &Config, bool RenderRail) const");
	ASSERT_FALSE(UpdateBody.empty());

	EXPECT_NE(Header.find("SQmScrollContentDragState m_ContentDragState;"), std::string::npos);
	EXPECT_NE(Header.find("void ScrollByWheel(CQmScrollState &State,"), std::string::npos);
	EXPECT_NE(Header.find("PreviewFrame(const CQmScrollState &State,"), std::string::npos);
	EXPECT_NE(Header.find("UpdateInternal(CQmScrollState &State,"), std::string::npos);
	EXPECT_EQ(Header.find("CQmScrollState m_State;"), std::string::npos);
	EXPECT_EQ(Header.find("m_ScrollbarDragActive"), std::string::npos);
	EXPECT_EQ(Header.find("m_ScrollbarGrabY"), std::string::npos);
	EXPECT_EQ(Header.find("m_ContentDragActive"), std::string::npos);
	EXPECT_EQ(Header.find("m_ContentDragCandidate"), std::string::npos);
	EXPECT_EQ(Header.find("m_ContentDragPressMousePos"), std::string::npos);
	EXPECT_EQ(Header.find("m_ContentDragPressOffset"), std::string::npos);
	EXPECT_EQ(Header.find("m_ContentDragLastMousePos"), std::string::npos);
	EXPECT_NE(UpdateBody.find("State.BeginThumbDrag("), std::string::npos);
	EXPECT_NE(UpdateBody.find("SQmScrollContentDragState &ContentDrag = State.ContentDragState();"), std::string::npos);
	EXPECT_NE(Containers.find("ScrollContainer(const IUiContext &Ctx, CQmScrollState &State, CQmScrollContainer &Controller,"), std::string::npos);
	EXPECT_NE(Containers.find("Controller.Update(State, ViewRect, ContentHeight,"), std::string::npos);
	EXPECT_NE(MenusHeader.find("BeginSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer,"), std::string::npos);
	EXPECT_NE(MenusHeader.find("FinishSettingsQmScrollContainer(CQmScrollState &ScrollState, CQmScrollContainer &ScrollContainer,"), std::string::npos);
	EXPECT_NE(QmClient.find("static CScrollRegion s_QmOverviewSettingsScrollRegion;"), std::string::npos);
	EXPECT_NE(QmClient.find("static CScrollRegion s_GlobalSearchScrollRegion;"), std::string::npos);
	EXPECT_EQ(QmClient.find("CQmScrollState s_GlobalSearchScrollState;"), std::string::npos);
	EXPECT_NE(QmClient.find("CQmScrollState s_QmScrollState;"), std::string::npos);
}
TEST(QmMonitoringHelpers, WheelOwnershipFrameBeginsOnlyFromUiUpdate)
{
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string Update = ExtractSourceFunctionBody(Ui, "void CUi::Update()");
	const std::string Begin = ExtractSourceFunctionBody(Ui, "void CUi::BeginWheelOwnershipFrame()");
	ASSERT_FALSE(Update.empty());
	ASSERT_FALSE(Begin.empty());
	EXPECT_EQ(CountSubstring(Ui, "BeginWheelOwnershipFrame();"), 1u);
	EXPECT_LT(Update.find("BeginWheelOwnershipFrame();"), Update.find("const vec2 WindowSize"));
	EXPECT_NE(Begin.find("const uint64_t FrameId = Client()->PerfFrame();"), std::string::npos);
	EXPECT_NE(Begin.find("m_WheelOwnership.FrameStarted(FrameId)"), std::string::npos);
	EXPECT_NE(Begin.find("m_WheelOwnership.BeginFrame(FrameId, RawDelta"), std::string::npos);
}
TEST(QmMonitoringHelpers, DropdownRegistersWheelOwnerBeforeParentCanConsume)
{
	const std::string Ui = ReadRepoFile("src/game/client/ui.cpp");
	const std::string ShowPopup = ExtractSourceFunctionBody(Ui, "void CUi::ShowPopupSelection(float X, float Y, SSelectionPopupContext *pContext)");
	const std::string Register = ExtractSourceFunctionBody(Ui, "void CUi::RegisterWheelOwner(const void *pOwnerId, EUiWheelOwnerPriority Priority, const CUIRect &HotRect, bool Eligible)");
	const std::string Consume = ExtractSourceFunctionBody(Ui, "bool CUi::TryConsumeWheel(const void *pOwnerId, float *pDelta)");
	const std::string Popup = ExtractSourceFunctionBody(Ui, "CUi::EPopupMenuFunctionResult CUi::PopupSelection(void *pContext, CUIRect View, bool Active)");
	const std::string Region = ReadRepoFile("src/game/client/ui_scrollregion.cpp");
	const std::string Input = ExtractSourceFunctionBody(Region, "void CScrollRegion::DoScrollInput()");
	ASSERT_FALSE(ShowPopup.empty());
	ASSERT_FALSE(Register.empty());
	ASSERT_FALSE(Consume.empty());
	ASSERT_FALSE(Popup.empty());
	ASSERT_FALSE(Input.empty());
	EXPECT_NE(ShowPopup.find("RegisterWheelOwner(pContext"), std::string::npos);
	EXPECT_LT(ShowPopup.find("RegisterWheelOwner(pContext"), ShowPopup.find("DoPopupMenu("));
	EXPECT_NE(Register.find("QmRegisterWheelOwnerCandidate(m_WheelOwnership"), std::string::npos);
	EXPECT_NE(Consume.find("QmTryConsumeWheel(m_WheelOwnership, pOwnerId, pDelta)"), std::string::npos);
	EXPECT_NE(Popup.find("ScrollParams.m_pWheelOwnerId = pSelectionPopup"), std::string::npos);
	EXPECT_NE(Popup.find("ScrollParams.m_WheelOwnerPreRegistered = true"), std::string::npos);
	EXPECT_NE(Input.find("TryConsumeWheel(pWheelOwnerId"), std::string::npos);
	EXPECT_EQ(Input.find("ConsumeHotkey(CUi::HOTKEY_SCROLL_"), std::string::npos);
}
TEST(QmMonitoringHelpers, QmUiCardPresetCarriesQmClientSettingsStyle)
{
	const std::string Containers = ReadRepoFile("src/game/client/QmUi/UiContainers.h");
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string MenusHeader = ReadRepoFile("src/game/client/components/menus.h");
	const std::string StyleBody = ExtractSourceFunctionBody(Menus, "CMenus::SQmSettingsCardStyle CMenus::QmSettingsCardStyle(float UiScale) const");
	ASSERT_FALSE(StyleBody.empty());

	EXPECT_NE(Containers.find("SCardProps QmClientCardProps(float UiScale = 1.0f)"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_Padding = 14.0f * UiScale;"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_Radius = 10.0f * UiScale;"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_DrawBorder = true;"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_FillColor = ColorRGBA(0.17f, 0.18f, 0.22f, 0.72f);"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_HighlightColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f);"), std::string::npos);
	EXPECT_NE(Containers.find("Props.m_BorderColor = ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f);"), std::string::npos);
	EXPECT_NE(Containers.find("Rect.Draw(Props.m_FillColor"), std::string::npos);
	EXPECT_NE(Containers.find("Highlight.Draw(Props.m_HighlightColor"), std::string::npos);
	EXPECT_NE(Containers.find("BorderBg.Margin(-1.0f, &BorderBg);"), std::string::npos);
	EXPECT_NE(Containers.find("BorderBg.Draw(Props.m_BorderColor"), std::string::npos);
	EXPECT_EQ(Containers.find("Border.Margin(0.5f, &Border);"), std::string::npos);
	EXPECT_NE(Menus.find("#include <game/client/QmUi/UiContainers.h>"), std::string::npos);
	EXPECT_NE(StyleBody.find("const ui_widget::SCardProps CardProps = ui_widget::QmClientCardProps(UiScale);"), std::string::npos);
	EXPECT_NE(StyleBody.find("Style.m_Padding = CardProps.m_Padding;"), std::string::npos);
	EXPECT_NE(StyleBody.find("Style.m_CornerRadius = CardProps.m_Radius;"), std::string::npos);
	EXPECT_NE(StyleBody.find("Style.m_GlassColor = CardProps.m_FillColor;"), std::string::npos);
	EXPECT_NE(StyleBody.find("Style.m_HighlightColor = CardProps.m_HighlightColor;"), std::string::npos);
	EXPECT_NE(StyleBody.find("Style.m_HairlineColor = CardProps.m_BorderColor;"), std::string::npos);
	EXPECT_NE(MenusHeader.find("SQmSettingsCardStyle QmSettingsCardStyle(float UiScale) const;"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiPresenceBacksFavoriteCommunityTabs)
{
	const std::string Menus = ReadRepoFile("src/game/client/components/menus.cpp");
	const auto CountOccurrences = [](const std::string &Haystack, const char *pNeedle) {
		const std::string Needle = pNeedle;
		size_t Count = 0;
		size_t Pos = 0;
		while((Pos = Haystack.find(Needle, Pos)) != std::string::npos)
		{
			++Count;
			Pos += Needle.size();
		}
		return Count;
	};
	const auto ExtractFavoriteCommunityBlock = [](const std::string &Source, size_t Occurrence) {
		const std::string Anchor = "static CButtonContainer s_aFavoriteCommunityButtons[5];";
		size_t Pos = 0;
		for(size_t Index = 0; Index <= Occurrence; ++Index)
		{
			Pos = Source.find(Anchor, Pos);
			if(Pos == std::string::npos)
				return std::string();
			if(Index < Occurrence)
				Pos += Anchor.size();
		}
		const size_t End = Source.find("TextRender()->SetRenderFlags(0);", Pos);
		if(End == std::string::npos)
			return std::string();
		return Source.substr(Pos, End - Pos);
	};
	const std::string NewMenubarCommunityBlock = ExtractFavoriteCommunityBlock(Menus, 0);
	const std::string LegacyMenubarCommunityBlock = ExtractFavoriteCommunityBlock(Menus, 1);
	ASSERT_FALSE(NewMenubarCommunityBlock.empty());
	ASSERT_FALSE(LegacyMenubarCommunityBlock.empty());

	EXPECT_NE(Menus.find("#include <game/client/QmUi/QmTree.h>"), std::string::npos);
	EXPECT_NE(NewMenubarCommunityBlock.find("CUiV2Tree &Tree = GameClient()->UiRuntimeV2()->Tree();"), std::string::npos);
	EXPECT_NE(LegacyMenubarCommunityBlock.find("CUiV2Tree &Tree = GameClient()->UiRuntimeV2()->Tree();"), std::string::npos);
	EXPECT_EQ(CountOccurrences(Menus, "const SUiPresenceResult Presence = Tree.ResolvePresence(AnimRuntime, NodeKey, true, AppearTransition);"), 2u);
	EXPECT_EQ(CountOccurrences(Menus, "const float AppearStrength = std::clamp(Presence.m_Alpha, 0.0f, 1.0f);"), 2u);
	EXPECT_EQ(CountOccurrences(Menus, "const float RevealWidth = maximum(2.0f, Button.w * AppearStrength);"), 2u);
	EXPECT_EQ(CountOccurrences(Menus, "InactiveColor.a *= AppearStrength;"), 2u);
	EXPECT_EQ(CountOccurrences(Menus, "ActiveColor.a *= AppearStrength;"), 2u);
	EXPECT_EQ(CountOccurrences(Menus, "HoverColor.a *= AppearStrength;"), 2u);
	EXPECT_EQ(NewMenubarCommunityBlock.find("ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_EQ(LegacyMenubarCommunityBlock.find("ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_EQ(NewMenubarCommunityBlock.find("WasVisibleLastFrame"), std::string::npos);
	EXPECT_EQ(LegacyMenubarCommunityBlock.find("WasVisibleLastFrame"), std::string::npos);
	EXPECT_EQ(Menus.find("s_aPrevFavoriteCommunityAnimNodes"), std::string::npos);
	EXPECT_EQ(Menus.find("s_PrevFavoriteCommunityAnimNodeCount"), std::string::npos);
	EXPECT_EQ(Menus.find("WasVisibleLastFrame"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiPresenceBacksImeCandidatePopup)
{
	const std::string Source = ReadRepoFile("src/game/client/qm_ime_candidate_popup.cpp");
	const std::string Header = ReadRepoFile("src/game/client/qm_ime_candidate_popup.h");
	const std::string RenderBody = ExtractSourceFunctionBody(Source, "void CQmImeCandidatePopup::Render(CGameClient *pGameClient, const SQmImePopupState &State)");
	const std::string ResetBody = ExtractSourceFunctionBody(Source, "void CQmImeCandidatePopup::Reset()");
	ASSERT_FALSE(RenderBody.empty());
	ASSERT_FALSE(ResetBody.empty());

	EXPECT_NE(Source.find("#include \"QmUi/QmTree.h\""), std::string::npos);
	EXPECT_NE(Header.find("uint64_t m_PresenceGeneration = 1;"), std::string::npos);
	EXPECT_NE(ResetBody.find("++m_PresenceGeneration;"), std::string::npos);
	EXPECT_NE(ResetBody.find("if(m_PresenceGeneration == 0)"), std::string::npos);
	EXPECT_NE(RenderBody.find("CUiV2Tree &Tree = pGameClient->UiRuntimeV2()->Tree();"), std::string::npos);
	EXPECT_NE(RenderBody.find("const uint64_t PopupKey = BuildUiAnimNodeKey(str_quickhash(\"qm_ime_popup\"), m_PresenceGeneration);"), std::string::npos);
	EXPECT_NE(RenderBody.find("const SUiPresenceResult Presence = Tree.ResolvePresence(AnimRuntime, PopupKey, TargetVisible, PresenceTransition);"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(!Presence.m_Render)"), std::string::npos);
	EXPECT_NE(RenderBody.find("const float Alpha = minimum(Presence.m_Alpha, PresentationAlpha);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("AnimRuntime.SetValue(PopupKey, EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_EQ(RenderBody.find("ResolveMotionValue(AnimRuntime, PopupKey, EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_NE(RenderBody.find("ResolveImePopupPresentationValue(AnimRuntime, PopupKey, EUiAnimProperty::POS_Y"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiPresenceBacksFavoriteButtonVisibility)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "int CMenus::DoButton_Favorite(const void *pButtonId, const void *pParentId, bool Checked, const CUIRect *pRect)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("CUiV2Tree &Tree = GameClient()->UiRuntimeV2()->Tree();"), std::string::npos);
	EXPECT_NE(Body.find("VisibilityTransition.m_DurationSec = 0.12f;"), std::string::npos);
	EXPECT_NE(Body.find("const SUiPresenceResult Visibility = Tree.ResolvePresence(AnimRuntime, VisibilityNodeKey, ShouldShow, VisibilityTransition);"), std::string::npos);
	EXPECT_NE(Body.find("const float ShowAlpha = std::clamp(Visibility.m_Alpha, 0.0f, 1.0f);"), std::string::npos);
	const size_t RenderBranchPos = Body.find("if(Visibility.m_Render && ShowAlpha > MENU_TAB_ANIM_EPSILON)");
	ASSERT_NE(RenderBranchPos, std::string::npos);
	const size_t ButtonLogicPos = Body.find("return Ui()->DoButtonLogic(pButtonId, 0, pRect, BUTTONFLAG_LEFT);");
	ASSERT_NE(ButtonLogicPos, std::string::npos);
	EXPECT_LT(RenderBranchPos, ButtonLogicPos);
	EXPECT_EQ(Body.find("ResolveUiAnimValue(AnimRuntime, VisibilityNodeKey, EUiAnimProperty::ALPHA"), std::string::npos);
	EXPECT_NE(Body.find("ResolveUiAnimValue(AnimRuntime, HoverNodeKey, EUiAnimProperty::SCALE"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmUiScrollContainerContentDragIsOptIn)
{
	const std::string Containers = ReadRepoFile("src/game/client/QmUi/UiContainers.h");
	const std::string Dogfood = ReadRepoFile("src/game/client/QmUi/UiDogfood.cpp");

	EXPECT_NE(Containers.find("bool m_ContentDragAllowed = false;"), std::string::npos);
	EXPECT_NE(Containers.find("Input.m_ContentDragAllowed = Props.m_ContentDragAllowed;"), std::string::npos);
	EXPECT_NE(Containers.find("Input.m_ContentDragBlocked = Ctx.m_pUi->ActiveItem() != nullptr;"), std::string::npos);
	EXPECT_EQ(Dogfood.find("ScrollProps.m_ContentDragAllowed = true;"), std::string::npos);
}

TEST(QmMonitoringHelpers, QmClientModuleHeadlinesKeyTextCacheByLayout)
{
	const std::string Source = ReadRepoFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	const std::string HeadlineBlock = ExtractSourceBlock(Source, "auto RenderQmModuleHeadline = [&](CUIRect &Content", "auto RenderMenuImage =");
	ASSERT_FALSE(HeadlineBlock.empty());

	EXPECT_NE(HeadlineBlock.find("TitleLabelProps.m_MaxWidth = TitleRect.w"), std::string::npos);
	EXPECT_NE(HeadlineBlock.find("BuildMenuTextStyleKey(&TitleRect, LgHeadlineSize, TEXTALIGN_ML, TitleLabelProps)"), std::string::npos);
	EXPECT_NE(HeadlineBlock.find("BuildMenuTextStyleKey(&TitleRect, LgHeadlineSizeNew, TEXTALIGN_ML, TitleLabelProps)"), std::string::npos);
	EXPECT_NE(HeadlineBlock.find("SettingsTextElement(SETTINGS_QMCLIENT, m_QmClientSettingsTab, pTitle, TitleStyleKey)"), std::string::npos);
	EXPECT_NE(HeadlineBlock.find("DoSettingsLabelStreamed(TitleElement, &TitleRect, pTitle, LgHeadlineSize, TEXTALIGN_ML, TitleLabelProps)"), std::string::npos);
	EXPECT_NE(HeadlineBlock.find("DoSettingsLabelStreamed(TitleElement, &TitleRect, BuildQmFeatureLabel(pTitle, pNewFeatureId, aTitle, sizeof(aTitle)), LgHeadlineSizeNew, TEXTALIGN_ML, TitleLabelProps)"), std::string::npos);
	EXPECT_EQ(HeadlineBlock.find("SettingsTextElement(SETTINGS_QMCLIENT, m_QmClientSettingsTab, pTitle);"), std::string::npos);
}

TEST(QmMonitoringHelpers, SettingsPagesExposeSectionLevelPerfStages)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Controls = ReadRepoFile("src/game/client/components/menus_settings_controls.cpp");
	const std::string TClient = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");

	EXPECT_NE(Settings.find("ddnet_tab_shell"), std::string::npos);
	EXPECT_NE(Settings.find("ddnet_demo_section"), std::string::npos);
	EXPECT_NE(Settings.find("ddnet_gameplay_section"), std::string::npos);
	EXPECT_NE(Settings.find("ddnet_controls_section"), std::string::npos);
	EXPECT_NE(Controls.find("controls_tab_shell"), std::string::npos);
	EXPECT_NE(Controls.find("controls_bind_list"), std::string::npos);
	EXPECT_NE(Controls.find("controls_interactive_layer"), std::string::npos);
	EXPECT_NE(Controls.find("controls_text_cache"), std::string::npos);
	EXPECT_NE(TClient.find("tclient_tab_3_shell"), std::string::npos);
	EXPECT_NE(TClient.find("tclient_tab_4_shell"), std::string::npos);
	EXPECT_NE(TClient.find("tclient_tab_5_shell"), std::string::npos);
}

TEST(QmMonitoringHelpers, HudAppearanceTabExposesSectionLevelPerfStages)
{
	const std::string Settings = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Settings, "void CMenus::RenderSettingsAppearance(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("appearance_hud_tab_shell"), std::string::npos);
	EXPECT_NE(Body.find("appearance_hud_core_section"), std::string::npos);
	EXPECT_NE(Body.find("appearance_hud_ddrace_section"), std::string::npos);
	EXPECT_NE(Body.find("appearance_hud_freeze_bars_section"), std::string::npos);
	EXPECT_NE(Body.find("appearance_hud_text_cache"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientSettingsDoesNotWriteScrollMetadataBeforeFinish)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsTClientSettings(CUIRect MainView, bool PrewarmOnly)");
	ASSERT_FALSE(Body.empty());

	const size_t BeginPos = Body.find("BeginSettingsScrollRegion(s_ScrollRegion, &MainView, ScrollParams");
	const size_t FinishPos = Body.find("FinishSettingsScrollRegion(s_ScrollRegion, ScrollFrame, &ScrollRegion, SETTINGS_TCLIENT");
	ASSERT_NE(BeginPos, std::string::npos);
	ASSERT_NE(FinishPos, std::string::npos);
	EXPECT_LT(BeginPos, FinishPos);

	const std::string BeforeFinish = Body.substr(0, FinishPos);
	EXPECT_EQ(BeforeFinish.find("m_SettingsRuntimeMetadata.m_LastScrollY ="), std::string::npos);
	EXPECT_EQ(BeforeFinish.find("m_SettingsRuntimeMetadata.m_LastScrollPage ="), std::string::npos);
	EXPECT_EQ(BeforeFinish.find("m_SettingsRuntimeMetadata.m_Valid = true;"), std::string::npos);
}

TEST(QmMonitoringHelpers, TClientTabNamesInitializeBeforeLanguageChange)
{
	const std::string Source = ReadRepoFile("src/game/client/components/tclient/menus_tclient.cpp");
	const size_t TabNamesPos = Source.find("static const char *s_apTClientTabNames[NUMBER_OF_TCLIENT_TABS] = {};");
	ASSERT_NE(TabNamesPos, std::string::npos);
	const size_t FirstTabDrawPos = Source.find("DoButton_MenuTab(&s_aPageTabs[Tab], s_apTClientTabNames[Tab]", TabNamesPos);
	ASSERT_NE(FirstTabDrawPos, std::string::npos);
	const std::string TabNamesBody = Source.substr(TabNamesPos, FirstTabDrawPos - TabNamesPos);

	EXPECT_NE(TabNamesBody.find("s_TClientTabNamesInitialized"), std::string::npos);
	EXPECT_NE(TabNamesBody.find("!s_TClientTabNamesInitialized || str_comp"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuTextStyleKeyRejectsNonFiniteMaxWidth)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "CMenus::SMenuTextStyleKey CMenus::BuildMenuTextStyleKey");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("std::isfinite(MaxWidth)"), std::string::npos);
	EXPECT_NE(Body.find("MaxWidth >= 0.0f && std::isfinite(MaxWidth)"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuTextStyleKeyRejectsNonFiniteHiDpiScale)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "CMenus::SMenuTextStyleKey CMenus::BuildMenuTextStyleKey");
	ASSERT_FALSE(Body.empty());

	EXPECT_NE(Body.find("const float HiDpiScale ="), std::string::npos);
	EXPECT_NE(Body.find("std::isfinite(HiDpiScale)"), std::string::npos);
	EXPECT_NE(Body.find("HiDpiScale >= 0.0f && std::isfinite(HiDpiScale)"), std::string::npos);
}

TEST(QmMonitoringHelpers, AppearanceTabNamesInitializeBeforeLanguageChange)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsAppearance(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TabNamesPos = Body.find("static const char *s_apAppearanceTabNames[NUMBER_OF_APPEARANCE_TABS] = {};");
	ASSERT_NE(TabNamesPos, std::string::npos);
	const size_t FirstTabDrawPos = Body.find("DoButton_MenuTab(&s_aPageTabs[Tab], s_apAppearanceTabNames[Tab]", TabNamesPos);
	ASSERT_NE(FirstTabDrawPos, std::string::npos);
	const std::string TabNamesBody = Body.substr(TabNamesPos, FirstTabDrawPos - TabNamesPos);

	EXPECT_NE(TabNamesBody.find("s_AppearanceTabNamesInitialized"), std::string::npos);
	EXPECT_NE(TabNamesBody.find("!s_AppearanceTabNamesInitialized || str_comp"), std::string::npos);
}

TEST(QmMonitoringHelpers, AssetsTabNamesInitializeBeforeLanguageChange)
{
	const std::string Source = ReadRepoFile("src/game/client/components/menus_settings_assets.cpp");
	const std::string Body = ExtractSourceFunctionBody(Source, "void CMenus::RenderSettingsCustom(CUIRect MainView)");
	ASSERT_FALSE(Body.empty());

	const size_t TabNamesPos = Body.find("static const char *s_apAssetsTabNames[NUMBER_OF_ASSETS_TABS] = {};");
	ASSERT_NE(TabNamesPos, std::string::npos);
	const size_t FirstTabDrawPos = Body.find("DoButton_MenuTab(&s_aPageTabs[Tab], s_apAssetsTabNames[Tab]", TabNamesPos);
	ASSERT_NE(FirstTabDrawPos, std::string::npos);
	const std::string TabNamesBody = Body.substr(TabNamesPos, FirstTabDrawPos - TabNamesPos);

	EXPECT_NE(TabNamesBody.find("s_AssetsTabNamesInitialized"), std::string::npos);
	EXPECT_NE(TabNamesBody.find("!s_AssetsTabNamesInitialized || str_comp"), std::string::npos);
}

TEST(QmMonitoringHelpers, MenuPerfEventsExposePageAttributionFields)
{
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("event=page_switch from=%s to=%s dur_ms=%.3f"), std::string::npos);
		EXPECT_NE(Source.find("event=section page=%s section=%s dur_ms=%.3f visible=%d dirty=%s text_new=%d text_reused=%d"), std::string::npos);
		EXPECT_NE(Source.find("LogSettingsSectionPerf(IClient *pClient, int Page, int Tab, const char *pSectionId, double DurationMs, const char *pDirtyReason, int TextNew, int TextReused)"), std::string::npos);
		EXPECT_NE(Source.find("pDirtyReason != nullptr ? pDirtyReason : \"unknown\""), std::string::npos);
		EXPECT_NE(Source.find("TextNew, TextReused"), std::string::npos);
		EXPECT_NE(Source.find("CScopedSettingsTextPerfStats TextStats(this);"), std::string::npos);
		EXPECT_NE(Source.find("TextStats.Stats().m_New"), std::string::npos);
		EXPECT_NE(Source.find("TextStats.Stats().m_Reused"), std::string::npos);
		EXPECT_EQ(Source.find("SectionCacheHit ? \"clean\" : \"cache_miss\", 0, 0"), std::string::npos);
		EXPECT_EQ(Source.find("\"cache_miss\""), std::string::npos);
		EXPECT_NE(Source.find("page=%s transition=%d sections=%d sections_visible=%d tab=%s"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus.h"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("class CScopedSettingsTextPerfStats"), std::string::npos);
		EXPECT_NE(Source.find("m_pActiveSettingsTextPerfStats = &m_Stats;"), std::string::npos);
		EXPECT_NE(Source.find("m_pActiveSettingsTextPerfStats = m_pPrevious;"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("event=page_switch from=%s to=%s dur_ms=%.3f source=menu_page_switch"), std::string::npos);
		EXPECT_NE(Source.find("event=page_switch from=%s to=%s dur_ms=%.3f source=game_page_switch"), std::string::npos);
		EXPECT_NE(Source.find("DoSettingsLabelStreamed"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/ui.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("pTextContainerRecreated"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/section_loader.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("void CSectionLoader::InvalidateCache(ESettingsCacheDirtyReason Reason)"), std::string::npos);
		EXPECT_EQ(Source.find("(void)Reason;"), std::string::npos);
		EXPECT_NE(Source.find("m_LastDirtyReason = Reason;"), std::string::npos);
		EXPECT_NE(Source.find("Section.m_Dirty = true;"), std::string::npos);
		EXPECT_NE(Source.find("event=section_loader sections_total=%d sections_visible=%d sections_skipped=%d layout_dirty=%d dirty_reason=%s"), std::string::npos);
		EXPECT_NE(Source.find("SettingsCacheDirtyReasonName(m_LastFrameStats.m_DirtyReason)"), std::string::npos);
		EXPECT_EQ(Source.find("*pDirtyReason = Section.m_DirtyReason"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_demo.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("event=list_frame page=demo_browser items_total=%d rows_visible=%d rows_processed=%d rows_skipped=%d dur_ms=%.3f"), std::string::npos);
		EXPECT_NE(Source.find("const double ListFrameDurationMs = ListFrameTimer.ElapsedMs();"), std::string::npos);
		EXPECT_NE(Source.find("ListFrameDurationMs >= QmPerfThresholdMs()"), std::string::npos);
		EXPECT_NE(Source.find("ListFrameDurationMs);"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_browser.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("event=list_frame page=server_browser items_total=%d rows_visible=%d rows_rendered=%d rows_iterated=%d rows_skipped=%d dur_ms=%.3f source=server_browser"), std::string::npos);
		EXPECT_NE(Source.find("QmPerfShouldLogDuration(ListFrameDurationMs, false)"), std::string::npos);
		EXPECT_NE(Source.find("const bool PerfListFrameEnabled = QmPerfEnabled();"), std::string::npos);
		EXPECT_NE(Source.find("RowsIterated += PerfListFrameEnabled ? 1 : 0;"), std::string::npos);
	}
	{
		std::ifstream File(TestSourcePath("src/game/client/components/menus_settings.cpp"));
		ASSERT_TRUE(File.good());
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		const std::string Source = Buffer.str();

		EXPECT_NE(Source.find("event=work_drain page=settings:tee kind=merge count=%llu bytes=%d dur_ms=%.3f stop=%s source=list_drain_summary scope=session"), std::string::npos);
	}
}

TEST(QmMonitoringHelpers, VulkanStandardLinePipelineCreatesTexturedVariant)
{
	std::ifstream File(TestSourcePath("src/engine/client/backend/vulkan/backend_vulkan.cpp"));
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("SPipelineContainer m_StandardLinePipeline;"), std::string::npos);
	EXPECT_NE(Source.find("GetStandardPipe(IsLineGeometry, IsTextured, BlendModeIndex, DynamicIndex)"), std::string::npos);
	EXPECT_NE(Source.find("GetStandardPipeLayout(IsLineGeometry, IsTextured, BlendModeIndex, DynamicIndex)"), std::string::npos);
	EXPECT_NE(Source.find("if(!CreateStandardGraphicsPipeline(\"shader/vulkan/prim.vert.spv\", \"shader/vulkan/prim.frag.spv\", false, true))"), std::string::npos);
	EXPECT_NE(Source.find("if(!CreateStandardGraphicsPipeline(\"shader/vulkan/prim_textured.vert.spv\", \"shader/vulkan/prim_textured.frag.spv\", true, true))"), std::string::npos);
}

TEST(QmMonitoringHelpers, VulkanDescriptorPoolFreeAvoidsUserVisibleAccountingAssert)
{
	const std::string Source = ReadRepoFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string FreeBody = ExtractSourceFunctionBody(Source, "void FreeDescriptorSetFromPool(SDeviceDescriptorSet &DescrSet)");
	const std::string DestroyBody = ExtractSourceFunctionBody(Source, "void DestroyDescriptorPools()");
	const std::string CreateImageBody = ExtractSourceFunctionBody(Source, "[[nodiscard]] bool CreateImage(uint32_t Width, uint32_t Height, uint32_t Depth, size_t MipMapLevelCount, VkFormat Format, VkImageTiling Tiling, VkImage &Image, SMemoryImageBlock<IMAGE_BUFFER_CACHE_ID> &ImageMemory, VkImageUsageFlags ImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM)");
	const std::string CreateRenderTargetDescriptorSetBody = ExtractSourceFunctionBody(Source, "[[nodiscard]] bool CreateRenderTargetDescriptorSet(SRenderTarget &Target, size_t DescrIndex)");
	const std::string CreateRenderTargetBody = ExtractSourceFunctionBody(Source, "[[nodiscard]] bool Cmd_RenderTarget_Create(const CCommandBuffer::SCommand_RenderTarget_Create *pCommand)");
	ASSERT_FALSE(FreeBody.empty());
	ASSERT_FALSE(DestroyBody.empty());
	ASSERT_FALSE(CreateImageBody.empty());
	ASSERT_FALSE(CreateRenderTargetDescriptorSetBody.empty());
	ASSERT_FALSE(CreateRenderTargetBody.empty());

	EXPECT_NE(DestroyBody.find("DestroyDescriptorPoolList"), std::string::npos);
	EXPECT_NE(DestroyBody.find("DescriptorPools.m_vPools.clear();"), std::string::npos);
	EXPECT_NE(FreeBody.find("DescrSet.m_PoolIndex >= DescrSet.m_pPools->m_vPools.size()"), std::string::npos);
	EXPECT_NE(FreeBody.find("descriptor set references stale pool index"), std::string::npos);
	EXPECT_NE(FreeBody.find("Pool.m_CurSize == 0"), std::string::npos);
	EXPECT_NE(FreeBody.find("descriptor pool accounting underflow prevented"), std::string::npos);
	EXPECT_EQ(FreeBody.find("dbg_assert(Pool.m_CurSize > 0"), std::string::npos);
	EXPECT_NE(CreateImageBody.find("Image = VK_NULL_HANDLE;"), std::string::npos);
	EXPECT_NE(CreateImageBody.find("return false;"), std::string::npos);
	EXPECT_LT(CreateImageBody.find("return false;"), CreateImageBody.find("vkGetImageMemoryRequirements("));
	EXPECT_NE(CreateImageBody.find("if(!GetImageMemory("), std::string::npos);
	EXPECT_NE(CreateImageBody.find("vkDestroyImage(m_VKDevice, Image, nullptr);"), std::string::npos);
	EXPECT_NE(CreateImageBody.find("if(vkBindImageMemory("), std::string::npos);
	EXPECT_NE(CreateImageBody.find("FreeImageMemBlock(ImageMemory);"), std::string::npos);
	EXPECT_NE(CreateImageBody.find("ImageMemory = {};"), std::string::npos);
	EXPECT_NE(CreateRenderTargetDescriptorSetBody.find("FreeDescriptorSetFromPool(DescrSet);"), std::string::npos);
	EXPECT_NE(CreateRenderTargetDescriptorSetBody.find("m_FrameProfileStats.m_DescriptorAllocations++;"), std::string::npos);
	EXPECT_NE(CreateRenderTargetBody.find("DestroyRenderTarget(Target);\n\t\t\treturn false;"), std::string::npos);
}

TEST(QmMonitoringHelpers, WindowsReleaseBuildProducesPdbSymbols)
{
	const std::string Source = ReadRepoFile("CMakeLists.txt");

	EXPECT_NE(Source.find("$<$<CONFIG:Release,RelWithDebInfo>:ProgramDatabase>"), std::string::npos);
	EXPECT_NE(Source.find("$<$<CONFIG:Release,RelWithDebInfo>:/DEBUG>"), std::string::npos);
	EXPECT_NE(Source.find("$<$<CONFIG:Release,RelWithDebInfo>:/OPT:REF>"), std::string::npos);
	EXPECT_NE(Source.find("$<$<CONFIG:Release,RelWithDebInfo>:/OPT:ICF>"), std::string::npos);
}

// Phase A 阶段 1: IFrameScheduler service 抽出。独立 service 持有 per-consumer state，
// 让 scheduler 从 menus-private 升级为全局 service。本测试锁定 service 接口契约。
TEST(QmMonitoringHelpers, FrameSchedulerServiceExposesConsumerScopedInterface)
{
	const std::string Header = ReadRepoFile("src/game/client/frame_scheduler.h");
	const std::string Source = ReadRepoFile("src/game/client/frame_scheduler.cpp");
	const std::string Client = ReadRepoFile("src/engine/client/client.cpp");
	const std::string Cmake = ReadRepoFile("CMakeLists.txt");

	ASSERT_FALSE(Header.empty());
	ASSERT_FALSE(Source.empty());

	EXPECT_NE(Header.find("class IFrameScheduler : public IInterface"), std::string::npos);
	EXPECT_NE(Header.find("MACRO_INTERFACE(\"frame_scheduler\")"), std::string::npos);
	EXPECT_NE(Header.find("enum class EFrameSchedulerConsumer"), std::string::npos);
	EXPECT_NE(Header.find("SettingsText"), std::string::npos);
	EXPECT_NE(Header.find("IngameText"), std::string::npos);
	EXPECT_NE(Header.find("Assets"), std::string::npos);
	EXPECT_NE(Header.find("DemoBrowser"), std::string::npos);
	EXPECT_NE(Header.find("IngameServerInfo"), std::string::npos);
	EXPECT_NE(Header.find("Count"), std::string::npos);
	EXPECT_NE(Header.find("ComputeBudget"), std::string::npos);
	EXPECT_NE(Header.find("Reset()"), std::string::npos);
	EXPECT_NE(Header.find("BeginFrame"), std::string::npos);
	EXPECT_NE(Header.find("EndFrame"), std::string::npos);
	EXPECT_NE(Header.find("CreateFrameScheduler"), std::string::npos);

	EXPECT_NE(Source.find("SettingsAdaptiveBudgetStep(Input, m_aState"), std::string::npos);
	EXPECT_NE(Source.find("IFrameScheduler *CreateFrameScheduler()"), std::string::npos);

	EXPECT_NE(Client.find("#include <game/client/frame_scheduler.h>"), std::string::npos);
	EXPECT_NE(Client.find("IFrameScheduler *pFrameScheduler = CreateFrameScheduler();"), std::string::npos);
	EXPECT_NE(Client.find("pKernel->RegisterInterface(pFrameScheduler)"), std::string::npos);

	// CMakeLists.txt 必须显式列出 frame_scheduler.cpp 才会被纳入 GAME_CLIENT 目标；
	// set_src(GAME_CLIENT GLOB_RECURSE ...) 仅用 GLOB 校验与磁盘文件对齐，
	// 真正参与编译的是 ${ARGN} 显式列表（见 CMakeLists.txt set_glob 函数）。
	EXPECT_NE(Cmake.find("frame_scheduler.cpp"), std::string::npos);

	EXPECT_NE(Header.find("#include <game/client/components/settings_resource_jobs.h>"), std::string::npos);
}

// Phase A 阶段 2: CGameClient::OnRender 在帧入口/出口调用 IFrameScheduler 的 BeginFrame/EndFrame。
// 这是阶段 3 同步渲染路径消费 token 的前置：所有同步 UI 都在 frame scope 内执行。
TEST(QmMonitoringHelpers, OnRenderHooksFrameSchedulerBeginAndEndFrame)
{
	const std::string GameClientHeader = ReadRepoFile("src/game/client/gameclient.h");
	const std::string GameClientSource = ReadRepoFile("src/game/client/gameclient.cpp");

	ASSERT_FALSE(GameClientHeader.empty());
	ASSERT_FALSE(GameClientSource.empty());

	EXPECT_NE(GameClientHeader.find("class IFrameScheduler *m_pFrameScheduler = nullptr;"), std::string::npos);

	EXPECT_NE(GameClientSource.find("#include <game/client/frame_scheduler.h>"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_pFrameScheduler = Kernel()->RequestInterface<IFrameScheduler>();"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_pFrameScheduler->BeginFrame(Client()->PerfFrame());"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_pFrameScheduler->EndFrame();"), std::string::npos);
}

// Phase A 阶段 3 前置：把文档测试改成运行时行为测试。
// service 真正被调用并产出非平凡 token；per-consumer state 互相独立；
// LastOutput 反映最近一次 ComputeBudget 的结果。
TEST(QmMonitoringHelpers, FrameSchedulerServiceProducesRealTokensAndIsolatesConsumers)
{
	std::unique_ptr<IFrameScheduler, void (*)(IFrameScheduler *)> Scheduler(CreateFrameScheduler(), [](IFrameScheduler *p) { delete p; });
	ASSERT_NE(Scheduler, nullptr);

	SSettingsAdaptiveBudgetInput Input;
	Input.m_WindowActive = true;
	Input.m_TargetFrameMs = 8.333f;
	Input.m_FrameMsAverage = 5.0f;
	Input.m_FrameMsP95 = 6.0f;

	const SSettingsAdaptiveBudgetOutput IngameOutput = Scheduler->ComputeBudget(EFrameSchedulerConsumer::IngameServerInfo, Input);
	EXPECT_GE(IngameOutput.m_VisibleTokens, 1);
	EXPECT_GE(IngameOutput.m_TextContainerTokens, 1);
	EXPECT_EQ(IngameOutput.m_Mode, ESettingsAdaptiveBudgetMode::IDLE);

	const SSettingsAdaptiveBudgetOutput &IngameLast = Scheduler->LastOutput(EFrameSchedulerConsumer::IngameServerInfo);
	EXPECT_EQ(IngameLast.m_VisibleTokens, IngameOutput.m_VisibleTokens);
	EXPECT_EQ(IngameLast.m_TextContainerTokens, IngameOutput.m_TextContainerTokens);

	const SSettingsAdaptiveBudgetOutput SettingsLast = Scheduler->LastOutput(EFrameSchedulerConsumer::SettingsText);
	EXPECT_EQ(SettingsLast.m_VisibleTokens, 0);
	EXPECT_EQ(SettingsLast.m_TextContainerTokens, 0);

	Input.m_FrameMsAverage = 30.0f;
	Input.m_FrameMsP95 = 40.0f;
	const SSettingsAdaptiveBudgetOutput PressuredOutput = Scheduler->ComputeBudget(EFrameSchedulerConsumer::IngameServerInfo, Input);
	EXPECT_EQ(PressuredOutput.m_Mode, ESettingsAdaptiveBudgetMode::FRAME_PRESSURE);
}

TEST(QmMonitoringHelpers, FrameSchedulerResetClearsConsumerStateAndFrameScope)
{
	std::unique_ptr<IFrameScheduler, void (*)(IFrameScheduler *)> Scheduler(CreateFrameScheduler(), [](IFrameScheduler *p) { delete p; });
	ASSERT_NE(Scheduler, nullptr);

	Scheduler->BeginFrame(42);

	SSettingsAdaptiveBudgetInput Input;
	Input.m_WindowActive = true;
	Input.m_TargetFrameMs = 8.333f;
	Input.m_FrameMsAverage = 5.0f;
	Input.m_FrameMsP95 = 6.0f;

	const SSettingsAdaptiveBudgetOutput Output = Scheduler->ComputeBudget(EFrameSchedulerConsumer::IngameServerInfo, Input);
	ASSERT_GT(Output.m_TextContainerTokens, 0);
	ASSERT_GT(Scheduler->LastOutput(EFrameSchedulerConsumer::IngameServerInfo).m_TextContainerTokens, 0);
	ASSERT_EQ(Scheduler->CurrentFrameId(), 42);

	Scheduler->Reset();

	EXPECT_EQ(Scheduler->CurrentFrameId(), 0);
	for(size_t i = 0; i < FRAME_SCHEDULER_CONSUMER_COUNT; ++i)
	{
		const EFrameSchedulerConsumer Consumer = static_cast<EFrameSchedulerConsumer>(i);
		EXPECT_FALSE(Scheduler->State(Consumer).m_Initialized);
		EXPECT_EQ(Scheduler->State(Consumer).m_HealthyFrames, 0);
		EXPECT_EQ(Scheduler->State(Consumer).m_BackgroundWindow, 1);
		EXPECT_EQ(Scheduler->LastOutput(Consumer).m_VisibleTokens, 0);
		EXPECT_EQ(Scheduler->LastOutput(Consumer).m_TextContainerTokens, 0);
	}
}

TEST(QmMonitoringHelpers, AndroidBundledCryptoUsesBoringSslAndSystemCertificates)
{
	const std::string FindCrypto = ReadRepoFile("cmake/FindCrypto.cmake");
	const std::string Http = ReadRepoFile("src/engine/shared/http.cpp");

	EXPECT_NE(FindCrypto.find("set_extra_dirs_lib(CRYPTO boringssl)"), std::string::npos);
	EXPECT_EQ(FindCrypto.find("set_extra_dirs_lib(CRYPTO openssl)"), std::string::npos);
	EXPECT_NE(Http.find("curl_easy_setopt(pH, CURLOPT_CAPATH, \"/system/etc/security/cacerts\");"), std::string::npos);
	EXPECT_EQ(Http.find("curl_easy_setopt(pH, CURLOPT_CAINFO, \"data/cacert.pem\");"), std::string::npos);
}
