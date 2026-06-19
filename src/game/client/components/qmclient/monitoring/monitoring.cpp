#include "monitoring.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

#if defined(CONF_FAMILY_UNIX)
#include <sys/resource.h>
#endif

#if defined(CONF_PLATFORM_MACOS)
#include <mach/mach.h>
#endif

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

#define IStorage IStorageCOM
#include <dxgi1_4.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#undef IStorage

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#endif

namespace
{
	constexpr ColorRGBA PANEL_BG(0.04f, 0.09f, 0.16f, 0.88f);
	constexpr ColorRGBA SURFACE_BG(0.08f, 0.13f, 0.21f, 0.74f);
	constexpr ColorRGBA CARD_BG(0.12f, 0.17f, 0.25f, 0.68f);
	constexpr ColorRGBA GRID_COLOR(1.0f, 1.0f, 1.0f, 0.11f);
	constexpr ColorRGBA DIVIDER_COLOR(1.0f, 1.0f, 1.0f, 0.16f);
	constexpr ColorRGBA PING_COLOR(0.31f, 0.63f, 1.0f, 0.95f);
	constexpr ColorRGBA PRED_COLOR(0.96f, 0.64f, 0.23f, 0.95f);
	constexpr ColorRGBA PRED_MARGIN_COLOR(0.78f, 0.56f, 0.97f, 0.95f);
	constexpr ColorRGBA JITTER_COLOR(0.95f, 0.86f, 0.33f, 0.95f);
	constexpr ColorRGBA FPS_COLOR(0.45f, 0.88f, 0.66f, 0.95f);
	constexpr ColorRGBA GAME_MARGIN_COLOR(0.29f, 0.84f, 0.48f, 0.95f);

	static float BytesPerSecondDelta(int64_t CurrentBytes, int64_t PrevBytes, float DeltaSeconds)
	{
		if(DeltaSeconds <= 0.0f || CurrentBytes < PrevBytes)
			return 0.0f;
		return (float)(CurrentBytes - PrevBytes) / DeltaSeconds;
	}

	static EQmConnectionGrade DetermineConnectionGrade(const SQmNetworkMetrics &Net)
	{
		return QmDetermineConnectionGrade(Net);
	}

	static EQmDiagnosticCause DeterminePrimaryCause(const SQmNetworkMetrics &Net, const SQmPerformanceMetrics &Perf, EQmConnectionGrade Grade)
	{
		return QmDeterminePrimaryCause(Net, Perf, Grade);
	}

	static const char *LocalizeGradeSummary(EQmConnectionGrade Grade)
	{
		switch(Grade)
		{
		case EQmConnectionGrade::NORMAL: return Localize("Connection normal");
		case EQmConnectionGrade::ELEVATED: return Localize("Connection elevated");
		case EQmConnectionGrade::SEVERE: return Localize("Connection severely abnormal");
		case EQmConnectionGrade::DISCONNECTED: return Localize("Connection disconnected");
		}
		return Localize("Connection disconnected");
	}

	static const char *LocalizeCauseDetail(EQmDiagnosticCause Cause, EQmConnectionGrade Grade, const SQmNetworkMetrics &Net, const SQmPerformanceMetrics &Perf)
	{
		if(Grade == EQmConnectionGrade::DISCONNECTED)
			return Localize("Not connected to a game server");

		switch(Cause)
		{
		case EQmDiagnosticCause::DOWNSTREAM: return Localize("Server RTT is elevated, response path is unstable");
		case EQmDiagnosticCause::UPSTREAM: return Localize("Prediction latency is elevated, prediction path pressure is high");
		case EQmDiagnosticCause::JITTER: return Localize("Prediction jitter is obvious, latency changes are large");
		case EQmDiagnosticCause::PACKET_LOSS: return Localize("Resend signs detected, connection quality is suspicious");
		case EQmDiagnosticCause::CLIENT_PERFORMANCE:
			if(Perf.m_FrameTimeMs > 16.7f)
				return Localize("Client frame time is abnormal");
			if(Perf.m_CpuUsagePct >= 75.0f)
				return Localize("Client CPU usage is high");
			if(Perf.m_PredictionTimeMs >= Net.m_SnapshotLatencyMs + 12.0f || Perf.m_PredictionStress >= 12.0f)
				return Localize("Client prediction time is high");
			return Localize("Client performance pressure is high");
		case EQmDiagnosticCause::NONE: return Localize("No obvious anomaly");
		}
		return Localize("No obvious anomaly");
	}

	static const char *GradeBadgeText(EQmConnectionGrade Grade)
	{
		switch(Grade)
		{
		case EQmConnectionGrade::NORMAL: return Localize("Normal");
		case EQmConnectionGrade::ELEVATED: return Localize("Elevated");
		case EQmConnectionGrade::SEVERE: return Localize("Severe");
		case EQmConnectionGrade::DISCONNECTED: return Localize("Disconnected");
		}
		return Localize("Disconnected");
	}

	static ColorRGBA GradeBadgeColor(EQmConnectionGrade Grade)
	{
		switch(Grade)
		{
		case EQmConnectionGrade::NORMAL: return ColorRGBA(0.18f, 0.70f, 0.42f, 0.95f);
		case EQmConnectionGrade::ELEVATED: return ColorRGBA(0.96f, 0.70f, 0.18f, 0.95f);
		case EQmConnectionGrade::SEVERE: return ColorRGBA(0.88f, 0.32f, 0.28f, 0.95f);
		case EQmConnectionGrade::DISCONNECTED: return ColorRGBA(0.45f, 0.48f, 0.56f, 0.95f);
		}
		return ColorRGBA(0.45f, 0.48f, 0.56f, 0.95f);
	}

	static float HudOpacity()
	{
		return QmComputeMonitoringPanelOpacity(g_Config.m_QmMonitoringHudOpacity);
	}

	static ColorRGBA ApplyHudOpacity(ColorRGBA Color)
	{
		Color.a *= HudOpacity();
		return Color;
	}

	static void DrawSurface(CUIRect Rect, ColorRGBA Color, float Radius)
	{
		Rect.Draw(ApplyHudOpacity(Color), IGraphics::CORNER_ALL, Radius);
	}

	static void DrawGraphGrid(IGraphics *pGraphics, CUIRect Rect, int HorizontalSegments)
	{
		std::array<IGraphics::CLineItem, 8> aLines = {};
		int NumLines = 0;
		for(int i = 1; i < HorizontalSegments; ++i)
		{
			const float Y = Rect.y + Rect.h * (float)i / (float)HorizontalSegments;
			aLines[NumLines++] = IGraphics::CLineItem(Rect.x, Y, Rect.x + Rect.w, Y);
		}
		for(int i = 1; i < 4; ++i)
		{
			const float X = Rect.x + Rect.w * (float)i / 4.0f;
			aLines[NumLines++] = IGraphics::CLineItem(X, Rect.y, X, Rect.y + Rect.h);
		}

		pGraphics->TextureClear();
		pGraphics->LinesBegin();
		pGraphics->SetColor(GRID_COLOR);
		pGraphics->LinesDraw(aLines.data(), NumLines);
		pGraphics->LinesEnd();
	}

	static void DrawZeroAxis(IGraphics *pGraphics, CUIRect Rect)
	{
		const IGraphics::CLineItem Axis(Rect.x, Rect.y + Rect.h / 2.0f, Rect.x + Rect.w, Rect.y + Rect.h / 2.0f);
		pGraphics->TextureClear();
		pGraphics->LinesBegin();
		pGraphics->SetColor(DIVIDER_COLOR);
		pGraphics->LinesDraw(&Axis, 1);
		pGraphics->LinesEnd();
	}

	static void DrawPeakAnchor(IGraphics *pGraphics, float PeakX, float PeakY, CUIRect LabelRect, ColorRGBA Color)
	{
		const float LabelCenterX = LabelRect.x + LabelRect.w * 0.5f;
		const float LabelCenterY = LabelRect.y + LabelRect.h * 0.5f;
		const float AnchorRadius = 2.0f;
		const IGraphics::CLineItem Connector(PeakX, PeakY, LabelCenterX, LabelCenterY);
		CUIRect Anchor(PeakX - AnchorRadius, PeakY - AnchorRadius, AnchorRadius * 2.0f, AnchorRadius * 2.0f);

		pGraphics->TextureClear();
		pGraphics->LinesBegin();
		pGraphics->SetColor(Color);
		pGraphics->LinesDraw(&Connector, 1);
		pGraphics->LinesEnd();
		Anchor.Draw(Color, IGraphics::CORNER_ALL, AnchorRadius);
	}

	static void FormatGraphStats(char *pBuf, int BufSize, const SQmHistoryStats &Stats, const char *pUnit, int Precision = 0)
	{
		if(!Stats.m_HasData)
		{
			str_copy(pBuf, "--", BufSize);
			return;
		}

		if(Precision <= 0)
			str_format(pBuf, BufSize, "avg %.0f ↓%.0f ↑%.0f%s", Stats.m_Average, Stats.m_Min, Stats.m_Max, pUnit);
		else
			str_format(pBuf, BufSize, "avg %.*f ↓%.*f ↑%.*f%s", Precision, Stats.m_Average, Precision, Stats.m_Min, Precision, Stats.m_Max, pUnit);
	}

	static void FormatPercentValue(char *pBuf, int BufSize, float Value)
	{
		if(Value < 0.0f)
		{
			str_copy(pBuf, "--", BufSize);
			return;
		}
		str_format(pBuf, BufSize, "%.0f%%", Value);
	}

	static void FormatTickPairValue(char *pBuf, int BufSize, int GameTick, int PredictedTick)
	{
		str_format(pBuf, BufSize, "%d/%d", GameTick, PredictedTick);
	}

	static void FormatMemoryKiBValue(char *pBuf, int BufSize, uint64_t ValueKiB)
	{
		str_format(pBuf, BufSize, "%" PRIu64 " KiB", ValueKiB);
	}

	static void FormatTrafficStatsValue(char *pBuf, int BufSize, const SQmNetworkMetrics::STrafficStats &Stats)
	{
		str_format(
			pBuf,
			BufSize,
			"%" PRIu64 "p %" PRIu64 "+%" PRIu64 "=%" PRIu64 " %.0fKib/s avg %" PRIu64 "B",
			Stats.m_Packets,
			Stats.m_PayloadBytes,
			Stats.m_OverheadBytes,
			Stats.m_TotalBytes,
			Stats.m_RateKibPerSec,
			Stats.m_AveragePayloadBytes);
	}

	static bool RectsOverlap(const CUIRect &A, const CUIRect &B)
	{
		return A.x < B.x + B.w && A.x + A.w > B.x && A.y < B.y + B.h && A.y + A.h > B.y;
	}

	static CUIRect PlacePeakLabelRect(
		CUIRect PlotRect,
		float PeakX,
		float PeakY,
		float Width,
		float Height,
		float VerticalOffset,
		const std::array<CUIRect, 8> &aUsedRects,
		int UsedCount)
	{
		CUIRect LabelRect;
		LabelRect.w = Width;
		LabelRect.h = Height;

		const float XStep = Width * 0.55f;
		for(int Attempt = 0; Attempt < 7; ++Attempt)
		{
			const float Direction = Attempt % 2 == 0 ? 1.0f : -1.0f;
			const float Multiplier = (float)((Attempt + 1) / 2);
			LabelRect.x = std::clamp(PeakX - LabelRect.w * 0.5f + Direction * XStep * Multiplier, PlotRect.x, PlotRect.x + PlotRect.w - LabelRect.w);
			LabelRect.y = std::clamp(PeakY - LabelRect.h * 0.5f + VerticalOffset, PlotRect.y, PlotRect.y + PlotRect.h - LabelRect.h);

			bool Overlaps = false;
			for(int i = 0; i < UsedCount; ++i)
			{
				if(RectsOverlap(LabelRect, aUsedRects[i]))
				{
					Overlaps = true;
					break;
				}
			}
			if(!Overlaps)
				return LabelRect;
		}
		return LabelRect;
	}

	template<size_t N>
	static void DrawPeakLabel(
		IGraphics *pGraphics,
		ITextRender *pTextRender,
		const std::array<float, N> &aHistory,
		int HistoryHead,
		int HistoryCount,
		CUIRect PlotRect,
		float Denominator,
		ColorRGBA TextColor,
		float FontSize,
		const char *pUnit,
		float VerticalOffset,
		std::array<CUIRect, 8> &aUsedRects,
		int &UsedCount)
	{
		if(HistoryCount <= 0 || Denominator <= 0.0f)
			return;

		const int Start = (HistoryHead - HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
		const int PeakIndex = QmFindLatestPeakIndex(aHistory, HistoryHead, HistoryCount);
		const float PeakValue = aHistory[(Start + PeakIndex) % (int)aHistory.size()];

		const float PeakX = PlotRect.x + PlotRect.w * (float)PeakIndex / (float)std::max(HistoryCount - 1, 1);
		const float PeakY = PlotRect.y + PlotRect.h - (PlotRect.h * std::clamp(PeakValue / Denominator, 0.0f, 1.0f));
		char aBuf[32];
		FormatMetricValue(aBuf, sizeof(aBuf), pUnit, PeakValue, 0);
		const float Width = std::max(52.0f, pTextRender->TextWidth(FontSize, aBuf) + 8.0f);
		const float Height = FontSize + 4.0f;
		const CUIRect LabelRect = PlacePeakLabelRect(PlotRect, PeakX, PeakY, Width, Height, VerticalOffset, aUsedRects, UsedCount);
		DrawPeakAnchor(pGraphics, PeakX, PeakY, LabelRect, TextColor);
		pTextRender->TextColor(TextColor);
		const float TextWidth = pTextRender->TextWidth(FontSize, aBuf);
		pTextRender->Text(LabelRect.x + (LabelRect.w - TextWidth) * 0.5f, LabelRect.y, FontSize, aBuf);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		if(UsedCount < (int)aUsedRects.size())
			aUsedRects[UsedCount++] = LabelRect;
	}

	template<size_t N>
	static void DrawSignedPeakLabel(
		IGraphics *pGraphics,
		ITextRender *pTextRender,
		const std::array<float, N> &aHistory,
		int HistoryHead,
		int HistoryCount,
		CUIRect PlotRect,
		float Denominator,
		ColorRGBA TextColor,
		float FontSize,
		const char *pUnit,
		std::array<CUIRect, 8> &aUsedRects,
		int &UsedCount)
	{
		if(HistoryCount <= 0 || Denominator <= 0.0f)
			return;

		const int Start = (HistoryHead - HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
		const int PeakIndex = QmFindLatestAbsolutePeakIndex(aHistory, HistoryHead, HistoryCount);
		const float PeakValue = aHistory[(Start + PeakIndex) % (int)aHistory.size()];

		const float PeakX = PlotRect.x + PlotRect.w * (float)PeakIndex / (float)std::max(HistoryCount - 1, 1);
		const float PeakY = PlotRect.y + PlotRect.h * 0.5f - (PlotRect.h * 0.5f * std::clamp(PeakValue / Denominator, -1.0f, 1.0f));
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "%.0f%s", PeakValue, pUnit);
		const float Width = std::max(52.0f, pTextRender->TextWidth(FontSize, aBuf) + 8.0f);
		const float Height = FontSize + 4.0f;
		const CUIRect LabelRect = PlacePeakLabelRect(PlotRect, PeakX, PeakY, Width, Height, 0.0f, aUsedRects, UsedCount);
		DrawPeakAnchor(pGraphics, PeakX, PeakY, LabelRect, TextColor);
		pTextRender->TextColor(TextColor);
		const float TextWidth = pTextRender->TextWidth(FontSize, aBuf);
		pTextRender->Text(LabelRect.x + (LabelRect.w - TextWidth) * 0.5f, LabelRect.y, FontSize, aBuf);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		if(UsedCount < (int)aUsedRects.size())
			aUsedRects[UsedCount++] = LabelRect;
	}

	static float SampleProcessCpuUsagePct()
	{
#if defined(CONF_FAMILY_UNIX)
		static int64_t s_LastWallNs = 0;
		static int64_t s_LastCpuNs = 0;

		struct rusage Usage;
		if(getrusage(RUSAGE_SELF, &Usage) != 0)
			return -1.0f;

		const int64_t WallNs = time_get_nanoseconds().count();
		const int64_t CpuNs =
			(int64_t)Usage.ru_utime.tv_sec * 1000000000LL + (int64_t)Usage.ru_utime.tv_usec * 1000LL +
			(int64_t)Usage.ru_stime.tv_sec * 1000000000LL + (int64_t)Usage.ru_stime.tv_usec * 1000LL;

		if(s_LastWallNs == 0 || WallNs <= s_LastWallNs || CpuNs < s_LastCpuNs)
		{
			s_LastWallNs = WallNs;
			s_LastCpuNs = CpuNs;
			return -1.0f;
		}

		const int64_t WallDeltaNs = WallNs - s_LastWallNs;
		const int64_t CpuDeltaNs = CpuNs - s_LastCpuNs;
		s_LastWallNs = WallNs;
		s_LastCpuNs = CpuNs;

		const float RawCpuUsagePct = (float)std::max((double)CpuDeltaNs / (double)WallDeltaNs * 100.0, 0.0);
		return QmNormalizeProcessCpuUsagePct(RawCpuUsagePct, std::thread::hardware_concurrency());
#elif defined(CONF_FAMILY_WINDOWS)
		static uint64_t s_LastWall100Ns = 0;
		static uint64_t s_LastCpu100Ns = 0;

		FILETIME CreationTime, ExitTime, KernelTime, UserTime, SystemTime;
		if(!GetProcessTimes(GetCurrentProcess(), &CreationTime, &ExitTime, &KernelTime, &UserTime))
			return -1.0f;
#if _WIN32_WINNT >= 0x0602
		GetSystemTimePreciseAsFileTime(&SystemTime);
#else
		GetSystemTimeAsFileTime(&SystemTime);
#endif

		ULARGE_INTEGER Wall;
		Wall.LowPart = SystemTime.dwLowDateTime;
		Wall.HighPart = SystemTime.dwHighDateTime;
		ULARGE_INTEGER Kernel;
		Kernel.LowPart = KernelTime.dwLowDateTime;
		Kernel.HighPart = KernelTime.dwHighDateTime;
		ULARGE_INTEGER User;
		User.LowPart = UserTime.dwLowDateTime;
		User.HighPart = UserTime.dwHighDateTime;

		const uint64_t Wall100Ns = Wall.QuadPart;
		const uint64_t Cpu100Ns = Kernel.QuadPart + User.QuadPart;
		if(s_LastWall100Ns == 0 || Wall100Ns <= s_LastWall100Ns || Cpu100Ns < s_LastCpu100Ns)
		{
			s_LastWall100Ns = Wall100Ns;
			s_LastCpu100Ns = Cpu100Ns;
			return -1.0f;
		}

		const uint64_t WallDelta100Ns = Wall100Ns - s_LastWall100Ns;
		const uint64_t CpuDelta100Ns = Cpu100Ns - s_LastCpu100Ns;
		s_LastWall100Ns = Wall100Ns;
		s_LastCpu100Ns = Cpu100Ns;

		const float RawCpuUsagePct = (float)std::max((double)CpuDelta100Ns / (double)WallDelta100Ns * 100.0, 0.0);
		return QmNormalizeProcessCpuUsagePct(RawCpuUsagePct, std::thread::hardware_concurrency());
#else
		return -1.0f;
#endif
	}

	static float SampleTotalCpuUsagePct()
	{
#if defined(CONF_PLATFORM_LINUX)
		static uint64_t s_LastIdle = 0;
		static uint64_t s_LastTotal = 0;

		FILE *pFile = std::fopen("/proc/stat", "r");
		if(pFile == nullptr)
			return -1.0f;

		char aLabel[8] = {};
		uint64_t User = 0;
		uint64_t Nice = 0;
		uint64_t System = 0;
		uint64_t Idle = 0;
		uint64_t Iowait = 0;
		uint64_t Irq = 0;
		uint64_t Softirq = 0;
		uint64_t Steal = 0;
		const int ReadCount = std::fscanf(pFile, "%7s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, aLabel, &User, &Nice, &System, &Idle, &Iowait, &Irq, &Softirq, &Steal);
		std::fclose(pFile);
		if(ReadCount < 5 || std::strcmp(aLabel, "cpu") != 0)
			return -1.0f;

		const uint64_t IdleAll = Idle + Iowait;
		const uint64_t Total = User + Nice + System + Idle + Iowait + Irq + Softirq + Steal;
		if(s_LastTotal == 0 || Total <= s_LastTotal || IdleAll < s_LastIdle)
		{
			s_LastIdle = IdleAll;
			s_LastTotal = Total;
			return -1.0f;
		}

		const float UsagePct = QmComputeTotalCpuUsagePct(s_LastIdle, s_LastTotal, IdleAll, Total);
		s_LastIdle = IdleAll;
		s_LastTotal = Total;
		return UsagePct;
#elif defined(CONF_FAMILY_WINDOWS)
		static uint64_t s_LastIdle100Ns = 0;
		static uint64_t s_LastKernel100Ns = 0;
		static uint64_t s_LastUser100Ns = 0;

		FILETIME IdleTime, KernelTime, UserTime;
		if(!GetSystemTimes(&IdleTime, &KernelTime, &UserTime))
			return -1.0f;

		ULARGE_INTEGER Idle;
		Idle.LowPart = IdleTime.dwLowDateTime;
		Idle.HighPart = IdleTime.dwHighDateTime;
		ULARGE_INTEGER Kernel;
		Kernel.LowPart = KernelTime.dwLowDateTime;
		Kernel.HighPart = KernelTime.dwHighDateTime;
		ULARGE_INTEGER User;
		User.LowPart = UserTime.dwLowDateTime;
		User.HighPart = UserTime.dwHighDateTime;

		if(s_LastKernel100Ns == 0 || Kernel.QuadPart < s_LastKernel100Ns || User.QuadPart < s_LastUser100Ns || Idle.QuadPart < s_LastIdle100Ns)
		{
			s_LastIdle100Ns = Idle.QuadPart;
			s_LastKernel100Ns = Kernel.QuadPart;
			s_LastUser100Ns = User.QuadPart;
			return -1.0f;
		}

		const uint64_t PreviousTotal = s_LastKernel100Ns + s_LastUser100Ns;
		const uint64_t CurrentTotal = Kernel.QuadPart + User.QuadPart;
		const float UsagePct = QmComputeTotalCpuUsagePct(s_LastIdle100Ns, PreviousTotal, Idle.QuadPart, CurrentTotal);
		s_LastIdle100Ns = Idle.QuadPart;
		s_LastKernel100Ns = Kernel.QuadPart;
		s_LastUser100Ns = User.QuadPart;

		return UsagePct;
#else
		return -1.0f;
#endif
	}

	static float SampleProcessMemoryMb()
	{
#if defined(CONF_PLATFORM_MACOS)
		mach_task_basic_info Info;
		mach_msg_type_number_t Count = MACH_TASK_BASIC_INFO_COUNT;
		if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&Info), &Count) != KERN_SUCCESS)
			return -1.0f;
		return (float)Info.resident_size / (1024.0f * 1024.0f);
#elif defined(CONF_FAMILY_UNIX)
		struct rusage Usage;
		if(getrusage(RUSAGE_SELF, &Usage) != 0)
			return -1.0f;
#if defined(CONF_PLATFORM_LINUX)
		return (float)Usage.ru_maxrss / 1024.0f;
#else
		return (float)Usage.ru_maxrss / (1024.0f * 1024.0f);
#endif
#elif defined(CONF_FAMILY_WINDOWS)
		PROCESS_MEMORY_COUNTERS_EX Counters;
		if(!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&Counters), sizeof(Counters)))
			return -1.0f;
		return (float)Counters.WorkingSetSize / (1024.0f * 1024.0f);
#else
		return -1.0f;
#endif
	}

	static CQmAsyncDevicePerfSampler &DevicePerfSamplerState()
	{
		static CQmAsyncDevicePerfSampler s_Sampler;
		return s_Sampler;
	}

	static SQmDevicePerfSample CachedDevicePerfSample(bool Enabled, bool &NewSample)
	{
		static uint64_t s_LastSeenVersion = 0;

		CQmAsyncDevicePerfSampler &Sampler = DevicePerfSamplerState();
		QmUpdateDevicePerfSamplerState(Sampler, Enabled);
		if(!Enabled)
		{
			NewSample = false;
			return {};
		}

		const SQmDevicePerfSnapshot Snapshot = Sampler.Snapshot();
		NewSample = Snapshot.m_Version != 0 && Snapshot.m_Version != s_LastSeenVersion;
		s_LastSeenVersion = Snapshot.m_Version;
		return Snapshot.m_Sample;
	}
}

void CQmMonitoring::ResetHistory()
{
	m_Snapshot = {};
	m_aPingHistory.fill(0.0f);
	m_aPredHistory.fill(0.0f);
	m_aPredictionMarginHistory.fill(0.0f);
	m_aJitterHistory.fill(0.0f);
	m_aGameTimeMarginHistory.fill(0.0f);
	m_aFpsHistory.fill(0.0f);
	m_HistoryHead = 0;
	m_HistoryCount = 0;
	m_LastSampleTick = 0;
}

void CQmMonitoring::OnInit()
{
	ResetHistory();
}

void CQmMonitoring::OnShutdown()
{
	DevicePerfSamplerState().Stop();
}

void CQmMonitoring::OnStateChange(int NewState, int OldState)
{
	if(NewState != OldState && (NewState != IClient::STATE_ONLINE || OldState != IClient::STATE_ONLINE))
		ResetHistory();
}

void CQmMonitoring::OnRender()
{
	const int64_t Now = time_get();
	if(m_LastSampleTick != 0 && Now - m_LastSampleTick < time_freq() / 20)
		return;

	UpdateSnapshot();
	PushHistorySample(
		m_Snapshot.m_Network.m_SnapshotLatencyMs,
		m_Snapshot.m_Network.m_PredictionLatencyMs,
		m_Snapshot.m_Network.m_PredictionMarginMs,
		m_Snapshot.m_Network.m_JitterMs,
		m_Snapshot.m_Network.m_GameTimeMarginMs,
		m_Snapshot.m_Performance.m_Fps);
	m_LastSampleTick = Now;
}

void CQmMonitoring::UpdateNetworkMetrics(SQmNetworkMetrics &Net)
{
	IClient *pClient = Client();
	Net.m_Connected = pClient->IsGameConnectionAlive();
	Net.m_ConnectionProblems = pClient->ConnectionProblems();
	Net.m_SnapshotLatencyMs = pClient->SnapshotLatencyMs();
	Net.m_PredictionLatencyMs = pClient->PredictionLatencyMs();
	Net.m_PredictionMarginMs = pClient->PredictionMarginMs();
	Net.m_JitterMs = pClient->PredictionJitterMs();
	Net.m_GameTimeMarginMs = pClient->GameTimeMarginMs();
	Net.m_ServerRollbackMs = QmComputeRollbackMs(Net.m_GameTimeMarginMs);
	const int PendingResendCount = pClient->PendingResendCount();

	NETSTATS Prev = {};
	NETSTATS Current = {};
	std::chrono::nanoseconds SampleInterval = std::chrono::nanoseconds::zero();
	pClient->NetStatsSnapshot(Prev, Current, SampleInterval);

	const float DeltaSeconds = SampleInterval.count() > 0 ? (float)SampleInterval.count() / 1000000000.0f : 0.0f;

	Net.m_DownBytesPerSec = BytesPerSecondDelta((int64_t)Current.recv_bytes, (int64_t)Prev.recv_bytes, DeltaSeconds);
	Net.m_UpBytesPerSec = BytesPerSecondDelta((int64_t)Current.sent_bytes, (int64_t)Prev.sent_bytes, DeltaSeconds);
	Net.m_Send = QmComputeTrafficStats(Prev.sent_packets, Prev.sent_bytes, Current.sent_packets, Current.sent_bytes);
	Net.m_Recv = QmComputeTrafficStats(Prev.recv_packets, Prev.recv_bytes, Current.recv_packets, Current.recv_bytes);
	Net.m_PacketLossPct = QmComputeDiagnosticPacketLossPct(Prev, Current, PendingResendCount);

	int NegativeSamples = 0;
	const int Start = (m_HistoryHead - m_HistoryCount + (int)m_aGameTimeMarginHistory.size()) % (int)m_aGameTimeMarginHistory.size();
	for(int i = 0; i < m_HistoryCount; ++i)
	{
		if(m_aGameTimeMarginHistory[(Start + i) % (int)m_aGameTimeMarginHistory.size()] < 0.0f)
			++NegativeSamples;
	}
	if(m_HistoryCount > 0)
		Net.m_ServerRollbackRatePct = (float)NegativeSamples * 100.0f / (float)m_HistoryCount;
}

void CQmMonitoring::UpdatePerformanceMetrics(SQmPerformanceMetrics &Perf)
{
	Perf.m_FrameTimeMs = Client()->FrameTimeAverage() * 1000.0f;
	Perf.m_FrameTimeSpikeMs = Perf.m_FrameTimeMs;
	Perf.m_FrameTimeUs = Client()->FrameTimeAverage() * 1000000.0f;
	Perf.m_Fps = Perf.m_FrameTimeMs > 0.0f ? 1000.0f / Perf.m_FrameTimeMs : 0.0f;
	Perf.m_PredictionTimeMs = (float)Client()->GetPredictionTime();
	Perf.m_CpuUsagePct = SampleProcessCpuUsagePct();
	Perf.m_TotalCpuUsagePct = SampleTotalCpuUsagePct();
	Perf.m_MemoryUsageMb = SampleProcessMemoryMb();
	Perf.m_GameTick = Client()->GameTick(g_Config.m_ClDummy);
	Perf.m_PredictedTick = Client()->PredGameTick(g_Config.m_ClDummy);
	Perf.m_GraphicsMemory.m_TextureKiB = Graphics()->TextureMemoryUsage() / 1024;
	Perf.m_GraphicsMemory.m_BufferKiB = Graphics()->BufferMemoryUsage() / 1024;
	Perf.m_GraphicsMemory.m_StreamedKiB = Graphics()->StreamedMemoryUsage() / 1024;
	Perf.m_GraphicsMemory.m_StagingKiB = Graphics()->StagingMemoryUsage() / 1024;
	if(QmPerfEnabled())
	{
		bool NewDeviceSample = false;
		const SQmDevicePerfSample DeviceSample = CachedDevicePerfSample(true, NewDeviceSample);
		Perf.m_GpuUtilPct = DeviceSample.m_GpuUtilPct;
		Perf.m_GpuDedicatedVramMb = DeviceSample.m_GpuDedicatedVramMb;
		Perf.m_GpuDedicatedVramBudgetMb = DeviceSample.m_GpuDedicatedVramBudgetMb;
		Perf.m_GpuSharedVramMb = DeviceSample.m_GpuSharedVramMb;
		Perf.m_DiskReadMbPerSec = DeviceSample.m_DiskReadMbPerSec;
		Perf.m_DeviceSampleAvailable = DeviceSample.m_Available;

		if(NewDeviceSample)
		{
			char aPayload[384];
			str_format(aPayload, sizeof(aPayload),
				"event=sample gpu_util_percent=%.3f gpu_dedicated_vram_mb=%.3f gpu_dedicated_vram_budget_mb=%.3f gpu_shared_vram_mb=%.3f cpu_process_percent=%.3f cpu_total_percent=%.3f memory_process_mb=%.3f disk_read_mb_s=%.3f sample_available=%d",
				Perf.m_GpuUtilPct,
				Perf.m_GpuDedicatedVramMb,
				Perf.m_GpuDedicatedVramBudgetMb,
				Perf.m_GpuSharedVramMb,
				Perf.m_CpuUsagePct,
				Perf.m_TotalCpuUsagePct,
				Perf.m_MemoryUsageMb,
				Perf.m_DiskReadMbPerSec,
				Perf.m_DeviceSampleAvailable ? 1 : 0);
			QmPerfLogPayload("perf/device", aPayload, Client());
		}
	}
	else
	{
		bool NewDeviceSample = false;
		(void)CachedDevicePerfSample(false, NewDeviceSample);
		Perf.m_GpuUtilPct = -1.0f;
		Perf.m_GpuDedicatedVramMb = -1.0f;
		Perf.m_GpuDedicatedVramBudgetMb = -1.0f;
		Perf.m_GpuSharedVramMb = -1.0f;
		Perf.m_DiskReadMbPerSec = -1.0f;
		Perf.m_DeviceSampleAvailable = false;
	}
	const float SnapshotLatencyMs = Client()->SnapshotLatencyMs();
	Perf.m_PredictionStress =
		std::max(Perf.m_PredictionTimeMs - SnapshotLatencyMs, 0.0f) +
		std::max(Perf.m_FrameTimeMs - 16.7f, 0.0f);
}

void CQmMonitoring::UpdateDiagnosticVerdict(SQmDiagnosticVerdict &Verdict, const SQmNetworkMetrics &Net, const SQmPerformanceMetrics &Perf)
{
	Verdict.m_Grade = DetermineConnectionGrade(Net);
	Verdict.m_PrimaryCause = DeterminePrimaryCause(Net, Perf, Verdict.m_Grade);
	Verdict.m_pSummary = LocalizeGradeSummary(Verdict.m_Grade);
	Verdict.m_pDetail = LocalizeCauseDetail(Verdict.m_PrimaryCause, Verdict.m_Grade, Net, Perf);
}

void CQmMonitoring::PushHistorySample(float PingMs, float PredMs, float PredictionMarginMs, float JitterMs, float GameTimeMarginMs, float Fps)
{
	m_aPingHistory[m_HistoryHead] = PingMs;
	m_aPredHistory[m_HistoryHead] = PredMs;
	m_aPredictionMarginHistory[m_HistoryHead] = PredictionMarginMs;
	m_aJitterHistory[m_HistoryHead] = JitterMs;
	m_aGameTimeMarginHistory[m_HistoryHead] = GameTimeMarginMs;
	m_aFpsHistory[m_HistoryHead] = Fps;
	m_HistoryHead = (m_HistoryHead + 1) % (int)m_aPingHistory.size();
	m_HistoryCount = std::min(m_HistoryCount + 1, (int)m_aPingHistory.size());
}

void CQmMonitoring::UpdateSnapshot()
{
	SQmMonitoringSnapshot Snapshot;
	UpdateNetworkMetrics(Snapshot.m_Network);
	UpdatePerformanceMetrics(Snapshot.m_Performance);
	UpdateDiagnosticVerdict(Snapshot.m_Verdict, Snapshot.m_Network, Snapshot.m_Performance);
	m_Snapshot = Snapshot;
}

void CQmMonitoring::RenderHeader(CUIRect Rect) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float SummaryFontSize = 22.0f * UiScale;
	const float DetailFontSize = 15.0f * UiScale;
	const float BadgeFontSize = 14.0f * UiScale;
	const float SummaryHeight = 26.0f * UiScale;
	const float RightColumnWidth = 118.0f * UiScale;
	const float BadgeHeight = 24.0f * UiScale;

	CUIRect Left, Right, SummaryRect, DetailRect, BadgeRect;
	Rect.VSplitRight(RightColumnWidth, &Left, &Right);
	Left.HSplitTop(SummaryHeight, &SummaryRect, &DetailRect);
	Ui()->DoLabel(&SummaryRect, Localize(m_Snapshot.m_Verdict.m_pSummary), SummaryFontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&DetailRect, Localize(m_Snapshot.m_Verdict.m_pDetail), DetailFontSize, TEXTALIGN_ML);
	Right.HSplitTop(BadgeHeight, &BadgeRect, nullptr);
	BadgeRect.Draw(GradeBadgeColor(m_Snapshot.m_Verdict.m_Grade), IGraphics::CORNER_ALL, BadgeRect.h / 2.0f);
	Ui()->DoLabel(&BadgeRect, Localize(GradeBadgeText(m_Snapshot.m_Verdict.m_Grade)), BadgeFontSize, TEXTALIGN_MC);
}

void CQmMonitoring::RenderMainGraph(CUIRect Rect) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float Margin = 12.0f * UiScale;
	const float HeaderHeight = 82.0f * UiScale;
	const float ItemGap = 12.0f * UiScale;
	const float ColorWidth = 14.0f * UiScale;
	const float ColorRadius = 4.0f * UiScale;
	const float HeaderFontSize = 16.0f * UiScale;
	const float HeaderValueFontSize = 17.0f * UiScale;
	const float FooterFontSize = 13.0f * UiScale;
	const float PeakFontSize = 15.0f * UiScale;
	const float CornerRadius = 8.0f * UiScale;

	DrawSurface(Rect, SURFACE_BG, CornerRadius);

	if(m_HistoryCount < 2)
		return;

	CUIRect Inner, HeaderRect, PlotRect;
	Rect.Margin(Margin, &Inner);
	Inner.HSplitTop(HeaderHeight, &HeaderRect, &Inner);
	PlotRect = Inner;
	DrawGraphGrid(Graphics(), PlotRect, 4);

	const auto SampleAt = [this](const std::array<float, QM_MONITORING_HISTORY_CAPACITY> &aHistory, int Index) {
		const int Start = (m_HistoryHead - m_HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
		return aHistory[(Start + Index) % (int)aHistory.size()];
	};

	float MaxValue = 30.0f;
	for(int i = 0; i < m_HistoryCount; ++i)
	{
		MaxValue = std::max(MaxValue, SampleAt(m_aPingHistory, i));
		MaxValue = std::max(MaxValue, SampleAt(m_aPredHistory, i));
		MaxValue = std::max(MaxValue, SampleAt(m_aPredictionMarginHistory, i));
		MaxValue = std::max(MaxValue, SampleAt(m_aJitterHistory, i));
	}

	const auto DrawSeries = [&](const std::array<float, QM_MONITORING_HISTORY_CAPACITY> &aHistory, const ColorRGBA &Color) {
		std::array<IGraphics::CLineItem, QM_MONITORING_HISTORY_CAPACITY - 1> aLines = {};
		int NumLines = 0;
		for(int i = 1; i < m_HistoryCount; ++i)
		{
			const float PrevValue = SampleAt(aHistory, i - 1);
			const float CurrValue = SampleAt(aHistory, i);
			const float X0 = PlotRect.x + PlotRect.w * (float)(i - 1) / (float)(m_HistoryCount - 1);
			const float X1 = PlotRect.x + PlotRect.w * (float)i / (float)(m_HistoryCount - 1);
			const float Y0 = PlotRect.y + PlotRect.h - (PlotRect.h * std::clamp(PrevValue / MaxValue, 0.0f, 1.0f));
			const float Y1 = PlotRect.y + PlotRect.h - (PlotRect.h * std::clamp(CurrValue / MaxValue, 0.0f, 1.0f));
			aLines[NumLines++] = IGraphics::CLineItem(X0, Y0, X1, Y1);
		}

		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(Color);
		if(NumLines > 0)
			Graphics()->LinesDraw(aLines.data(), NumLines);
		Graphics()->LinesEnd();
	};

	DrawSeries(m_aPingHistory, PING_COLOR);
	DrawSeries(m_aPredHistory, PRED_COLOR);
	DrawSeries(m_aPredictionMarginHistory, PRED_MARGIN_COLOR);
	DrawSeries(m_aJitterHistory, JITTER_COLOR);
	std::array<CUIRect, 8> aPeakRects = {};
	int PeakRectCount = 0;
	DrawPeakLabel(Graphics(), TextRender(), m_aPingHistory, m_HistoryHead, m_HistoryCount, PlotRect, MaxValue, PING_COLOR, PeakFontSize, "ms", 0.0f, aPeakRects, PeakRectCount);
	DrawPeakLabel(Graphics(), TextRender(), m_aPredHistory, m_HistoryHead, m_HistoryCount, PlotRect, MaxValue, PRED_COLOR, PeakFontSize, "ms", 0.0f, aPeakRects, PeakRectCount);
	DrawPeakLabel(Graphics(), TextRender(), m_aPredictionMarginHistory, m_HistoryHead, m_HistoryCount, PlotRect, MaxValue, PRED_MARGIN_COLOR, PeakFontSize, "ms", 0.0f, aPeakRects, PeakRectCount);
	DrawPeakLabel(Graphics(), TextRender(), m_aJitterHistory, m_HistoryHead, m_HistoryCount, PlotRect, MaxValue, JITTER_COLOR, PeakFontSize, "ms", 0.0f, aPeakRects, PeakRectCount);

	const struct SLegendItem
	{
		const char *m_pLabel;
		float m_Value;
		ColorRGBA m_Color;
		SQmHistoryStats m_Stats;
	} aLegend[] = {
		{Localize("Latency"), m_Snapshot.m_Network.m_SnapshotLatencyMs, PING_COLOR, QmComputeHistoryStats(m_aPingHistory, m_HistoryHead, m_HistoryCount)},
		{Localize("Prediction"), m_Snapshot.m_Network.m_PredictionLatencyMs, PRED_COLOR, QmComputeHistoryStats(m_aPredHistory, m_HistoryHead, m_HistoryCount)},
		{Localize("Prediction margin"), m_Snapshot.m_Network.m_PredictionMarginMs, PRED_MARGIN_COLOR, QmComputeHistoryStats(m_aPredictionMarginHistory, m_HistoryHead, m_HistoryCount)},
		{Localize("Jitter"), m_Snapshot.m_Network.m_JitterMs, JITTER_COLOR, QmComputeHistoryStats(m_aJitterHistory, m_HistoryHead, m_HistoryCount)},
	};

	CUIRect TopRow, BottomRow;
	HeaderRect.HSplitMid(&TopRow, &BottomRow, 10.0f * UiScale);
	std::array<CUIRect, 4> aCells = {};
	CUIRect TopLeft, TopRight, BottomLeft, BottomRight;
	TopRow.VSplitMid(&TopLeft, &TopRight, ItemGap);
	BottomRow.VSplitMid(&BottomLeft, &BottomRight, ItemGap);
	aCells[0] = TopLeft;
	aCells[1] = TopRight;
	aCells[2] = BottomLeft;
	aCells[3] = BottomRight;
	for(int i = 0; i < 4; ++i)
	{
		CUIRect HeaderCell = aCells[i];
		CUIRect ColorRect, TextRect, LabelRect, ValueRect, StatsRect;

		HeaderCell.VSplitLeft(ColorWidth, &ColorRect, &HeaderCell);
		HeaderCell.VSplitLeft(5.0f * UiScale, nullptr, &HeaderCell);
		TextRect = HeaderCell;
		TextRect.HSplitTop(26.0f * UiScale, &TextRect, &StatsRect);
		TextRect.VSplitLeft(TextRect.w * 0.64f, &LabelRect, &ValueRect);
		ColorRect.HMargin(4.0f * UiScale, &ColorRect);
		ColorRect.Draw(aLegend[i].m_Color, IGraphics::CORNER_ALL, ColorRadius);

		char aValueBuf[32];
		FormatMetricValue(aValueBuf, sizeof(aValueBuf), "ms", aLegend[i].m_Value, 0);
		Ui()->DoLabel(&LabelRect, aLegend[i].m_pLabel, HeaderFontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&ValueRect, aValueBuf, HeaderValueFontSize, TEXTALIGN_MR);

		char aStatsBuf[64];
		FormatGraphStats(aStatsBuf, sizeof(aStatsBuf), aLegend[i].m_Stats, "ms", 0);
		Ui()->DoLabel(&StatsRect, aStatsBuf, FooterFontSize, TEXTALIGN_ML);
	}
}

void CQmMonitoring::RenderFpsGraph(CUIRect Rect) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float Margin = 14.0f * UiScale;
	const float HeaderHeight = 44.0f * UiScale;
	const float HeaderFontSize = 15.0f * UiScale;
	const float StatsFontSize = 13.0f * UiScale;
	const float PeakFontSize = 14.0f * UiScale;
	const float CornerRadius = 8.0f * UiScale;

	DrawSurface(Rect, SURFACE_BG, CornerRadius);

	if(m_HistoryCount < 2)
		return;

	const auto SampleAt = [this](const std::array<float, QM_MONITORING_HISTORY_CAPACITY> &aHistory, int Index) {
		const int Start = (m_HistoryHead - m_HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
		return aHistory[(Start + Index) % (int)aHistory.size()];
	};

	CUIRect Inner, HeaderRect, PlotRect;
	Rect.Margin(Margin, &Inner);
	Inner.HSplitTop(HeaderHeight, &HeaderRect, &PlotRect);

	CUIRect FpsRect, GameMarginRect;
	PlotRect.HSplitMid(&FpsRect, &GameMarginRect, 8.0f * UiScale);
	DrawGraphGrid(Graphics(), FpsRect, 4);
	DrawGraphGrid(Graphics(), GameMarginRect, 4);
	DrawZeroAxis(Graphics(), GameMarginRect);

	float MaxFpsValue = 30.0f;
	for(int i = 0; i < m_HistoryCount; ++i)
		MaxFpsValue = std::max(MaxFpsValue, SampleAt(m_aFpsHistory, i));

	float MaxGameMarginAbs = 25.0f;
	for(int i = 0; i < m_HistoryCount; ++i)
		MaxGameMarginAbs = std::max(MaxGameMarginAbs, std::abs(SampleAt(m_aGameTimeMarginHistory, i)));

	std::array<IGraphics::CLineItem, QM_MONITORING_HISTORY_CAPACITY - 1> aFpsLines = {};
	int NumFpsLines = 0;
	for(int i = 1; i < m_HistoryCount; ++i)
	{
		const float PrevValue = SampleAt(m_aFpsHistory, i - 1);
		const float CurrValue = SampleAt(m_aFpsHistory, i);
		const float X0 = FpsRect.x + FpsRect.w * (float)(i - 1) / (float)(m_HistoryCount - 1);
		const float X1 = FpsRect.x + FpsRect.w * (float)i / (float)(m_HistoryCount - 1);
		const float Y0 = FpsRect.y + FpsRect.h - (FpsRect.h * std::clamp(PrevValue / MaxFpsValue, 0.0f, 1.0f));
		const float Y1 = FpsRect.y + FpsRect.h - (FpsRect.h * std::clamp(CurrValue / MaxFpsValue, 0.0f, 1.0f));
		aFpsLines[NumFpsLines++] = IGraphics::CLineItem(X0, Y0, X1, Y1);
	}

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(FPS_COLOR);
	if(NumFpsLines > 0)
		Graphics()->LinesDraw(aFpsLines.data(), NumFpsLines);
	Graphics()->LinesEnd();
	std::array<CUIRect, 8> aPeakRects = {};
	int PeakRectCount = 0;
	DrawPeakLabel(Graphics(), TextRender(), m_aFpsHistory, m_HistoryHead, m_HistoryCount, FpsRect, MaxFpsValue, FPS_COLOR, PeakFontSize, "", -8.0f * UiScale, aPeakRects, PeakRectCount);

	std::array<IGraphics::CLineItem, QM_MONITORING_HISTORY_CAPACITY - 1> aGameMarginLines = {};
	int NumGameMarginLines = 0;
	for(int i = 1; i < m_HistoryCount; ++i)
	{
		const float PrevValue = SampleAt(m_aGameTimeMarginHistory, i - 1);
		const float CurrValue = SampleAt(m_aGameTimeMarginHistory, i);
		const float X0 = GameMarginRect.x + GameMarginRect.w * (float)(i - 1) / (float)(m_HistoryCount - 1);
		const float X1 = GameMarginRect.x + GameMarginRect.w * (float)i / (float)(m_HistoryCount - 1);
		const float Y0 = GameMarginRect.y + GameMarginRect.h * 0.5f - (GameMarginRect.h * 0.5f * std::clamp(PrevValue / MaxGameMarginAbs, -1.0f, 1.0f));
		const float Y1 = GameMarginRect.y + GameMarginRect.h * 0.5f - (GameMarginRect.h * 0.5f * std::clamp(CurrValue / MaxGameMarginAbs, -1.0f, 1.0f));
		aGameMarginLines[NumGameMarginLines++] = IGraphics::CLineItem(X0, Y0, X1, Y1);
	}

	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(GAME_MARGIN_COLOR);
	if(NumGameMarginLines > 0)
		Graphics()->LinesDraw(aGameMarginLines.data(), NumGameMarginLines);
	Graphics()->LinesEnd();
	DrawSignedPeakLabel(Graphics(), TextRender(), m_aGameTimeMarginHistory, m_HistoryHead, m_HistoryCount, GameMarginRect, MaxGameMarginAbs, GAME_MARGIN_COLOR, PeakFontSize, "ms", aPeakRects, PeakRectCount);

	const SQmHistoryStats FpsStats = QmComputeHistoryStats(m_aFpsHistory, m_HistoryHead, m_HistoryCount);
	const SQmHistoryStats GameMarginStats = QmComputeHistoryStats(m_aGameTimeMarginHistory, m_HistoryHead, m_HistoryCount);
	CUIRect LeftHeader, RightHeader;
	HeaderRect.VSplitMid(&LeftHeader, &RightHeader, 10.0f * UiScale);

	char aFpsValueBuf[32];
	FormatMetricValue(aFpsValueBuf, sizeof(aFpsValueBuf), "", m_Snapshot.m_Performance.m_Fps, 0);
	Ui()->DoLabel(&LeftHeader, Localize("Frame rate"), HeaderFontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&LeftHeader, aFpsValueBuf, HeaderFontSize, TEXTALIGN_MR);
	CUIRect LeftStatsRect = LeftHeader;
	LeftStatsRect.y += 18.0f * UiScale;
	char aFpsStatsBuf[64];
	FormatGraphStats(aFpsStatsBuf, sizeof(aFpsStatsBuf), FpsStats, "", 0);
	Ui()->DoLabel(&LeftStatsRect, aFpsStatsBuf, StatsFontSize, TEXTALIGN_ML);

	char aGameMarginValueBuf[32];
	FormatMetricValue(aGameMarginValueBuf, sizeof(aGameMarginValueBuf), "ms", m_Snapshot.m_Network.m_GameTimeMarginMs, 0);
	Ui()->DoLabel(&RightHeader, Localize("Game time margin"), HeaderFontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&RightHeader, aGameMarginValueBuf, HeaderFontSize, TEXTALIGN_MR);
	CUIRect RightStatsRect = RightHeader;
	RightStatsRect.y += 18.0f * UiScale;
	char aGameMarginStatsBuf[64];
	FormatGraphStats(aGameMarginStatsBuf, sizeof(aGameMarginStatsBuf), GameMarginStats, "ms", 0);
	Ui()->DoLabel(&RightStatsRect, aGameMarginStatsBuf, StatsFontSize, TEXTALIGN_ML);
}

void CQmMonitoring::RenderPrimaryCards(CUIRect Rect) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float Gap = 10.0f * UiScale;
	const float CornerRadius = 8.0f * UiScale;

	const struct SCard
	{
		const char *m_pLabel;
		float m_Value;
		const char *m_pUnit;
		int m_Precision;
		ColorRGBA m_Color;
		bool m_IsRate = false;
		bool m_IsPercent = false;
		bool m_IsCpu = false;
	} aCards[] = {
		{Localize("Frame rate"), m_Snapshot.m_Performance.m_Fps, "", 0, FPS_COLOR},
		{Localize("Frame time"), m_Snapshot.m_Performance.m_FrameTimeUs, "us", 0, FPS_COLOR},
		{Localize("DDNet/total CPU"), m_Snapshot.m_Performance.m_CpuUsagePct, "", 0, GAME_MARGIN_COLOR, false, false, true},
		{Localize("Memory"), m_Snapshot.m_Performance.m_MemoryUsageMb, "MB", 0, GAME_MARGIN_COLOR},
		{Localize("Connection downstream"), m_Snapshot.m_Network.m_DownBytesPerSec, "", 0, PING_COLOR, true},
		{Localize("Connection upstream"), m_Snapshot.m_Network.m_UpBytesPerSec, "", 0, PRED_COLOR, true},
		{Localize("Server rollback"), m_Snapshot.m_Network.m_ServerRollbackMs, "ms", 0, GAME_MARGIN_COLOR},
		{Localize("Rollback rate"), m_Snapshot.m_Network.m_ServerRollbackRatePct, "", 0, JITTER_COLOR, false, true},
	};

	const int CardCount = std::size(aCards);
	const int Columns = 4;
	const int Rows = (CardCount + Columns - 1) / Columns;
	const float CardWidth = std::max((Rect.w - Gap * (Columns - 1)) / (float)Columns, 0.0f);
	const float CardHeight = std::max((Rect.h - Gap * (Rows - 1)) / (float)Rows, 0.0f);
	const float Margin = std::clamp(CardHeight * 0.09f, 6.0f, 10.0f);
	const float LabelFontSize = std::clamp(CardHeight * 0.30f, 14.0f, 19.0f);
	const float ValueFontSize = std::clamp(CardHeight * 0.44f, 21.0f, 28.0f);
	const float RowHeight = std::max(LabelFontSize, ValueFontSize) + 4.0f;
	const float AccentWidth = std::clamp(CardHeight * 0.06f, 3.0f, 5.0f);
	for(int i = 0; i < CardCount; ++i)
	{
		const int RowIndex = i / Columns;
		const int ColumnIndex = i % Columns;
		CUIRect CardRect(
			Rect.x + (CardWidth + Gap) * ColumnIndex,
			Rect.y + (CardHeight + Gap) * RowIndex,
			CardWidth,
			CardHeight);

		DrawSurface(CardRect, CARD_BG, CornerRadius);
		CUIRect Inner, RowRect, LabelRect, ValueRect, AccentRect;
		CardRect.Margin(Margin, &Inner);
		Inner.VSplitLeft(AccentWidth, &AccentRect, &Inner);
		Inner.VSplitLeft(6.0f * UiScale, nullptr, &Inner);
		const float TopPadding = std::max((Inner.h - RowHeight) * 0.5f, 0.0f);
		Inner.HSplitTop(TopPadding, nullptr, &Inner);
		Inner.HSplitTop(RowHeight, &RowRect, nullptr);
		RowRect.VSplitLeft(RowRect.w * 0.36f, &LabelRect, &ValueRect);

		char aBuf[32];
		if(aCards[i].m_IsRate)
			FormatRateValue(aBuf, sizeof(aBuf), aCards[i].m_Value);
		else if(aCards[i].m_IsCpu)
			FormatCpuRatioValue(aBuf, sizeof(aBuf), aCards[i].m_Value, m_Snapshot.m_Performance.m_TotalCpuUsagePct);
		else if(aCards[i].m_IsPercent)
			FormatPercentValue(aBuf, sizeof(aBuf), aCards[i].m_Value);
		else
			FormatMetricValue(aBuf, sizeof(aBuf), aCards[i].m_pUnit, aCards[i].m_Value, aCards[i].m_Precision);
		Ui()->DoLabel(&LabelRect, aCards[i].m_pLabel, LabelFontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&ValueRect, aBuf, ValueFontSize, TEXTALIGN_MR);
		AccentRect.Draw(aCards[i].m_Color, IGraphics::CORNER_ALL, AccentWidth / 2.0f);
	}
}

void CQmMonitoring::RenderDebugDetails(CUIRect Rect) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float Margin = 10.0f * UiScale;
	const float Gap = 14.0f * UiScale;
	const float RowGap = 6.0f * UiScale;
	const float LabelFontSize = 13.0f * UiScale;
	const float ValueFontSize = 13.0f * UiScale;
	const float CornerRadius = 8.0f * UiScale;
	const float RowHeight = 18.0f * UiScale;

	DrawSurface(Rect, SURFACE_BG, CornerRadius);

	CUIRect Inner, Left, Right;
	Rect.Margin(Margin, &Inner);
	Inner.VSplitMid(&Left, &Right, Gap);

	struct SDetailRow
	{
		const char *m_pLabel;
		const char *m_pValue;
	};

	char aTickBuf[32];
	char aPredictionBuf[32];
	char aSendBuf[96];
	char aRecvBuf[96];
	char aTextureBuf[32];
	char aBufferBuf[32];
	char aStreamedBuf[32];
	char aStagingBuf[32];

	FormatTickPairValue(aTickBuf, sizeof(aTickBuf), m_Snapshot.m_Performance.m_GameTick, m_Snapshot.m_Performance.m_PredictedTick);
	FormatMetricValue(aPredictionBuf, sizeof(aPredictionBuf), "ms", m_Snapshot.m_Performance.m_PredictionTimeMs, 0);
	FormatTrafficStatsValue(aSendBuf, sizeof(aSendBuf), m_Snapshot.m_Network.m_Send);
	FormatTrafficStatsValue(aRecvBuf, sizeof(aRecvBuf), m_Snapshot.m_Network.m_Recv);
	FormatMemoryKiBValue(aTextureBuf, sizeof(aTextureBuf), m_Snapshot.m_Performance.m_GraphicsMemory.m_TextureKiB);
	FormatMemoryKiBValue(aBufferBuf, sizeof(aBufferBuf), m_Snapshot.m_Performance.m_GraphicsMemory.m_BufferKiB);
	FormatMemoryKiBValue(aStreamedBuf, sizeof(aStreamedBuf), m_Snapshot.m_Performance.m_GraphicsMemory.m_StreamedKiB);
	FormatMemoryKiBValue(aStagingBuf, sizeof(aStagingBuf), m_Snapshot.m_Performance.m_GraphicsMemory.m_StagingKiB);

	const SDetailRow aLeftRows[] = {
		{Localize("Game/predicted tick"), aTickBuf},
		{Localize("Prediction time"), aPredictionBuf},
		{Localize("Send"), aSendBuf},
		{Localize("Receive"), aRecvBuf},
	};
	const SDetailRow aRightRows[] = {
		{Localize("Texture memory"), aTextureBuf},
		{Localize("Buffer memory"), aBufferBuf},
		{Localize("Streamed memory"), aStreamedBuf},
		{Localize("Staging memory"), aStagingBuf},
	};

	const auto RenderColumn = [&](CUIRect ColumnRect, const SDetailRow *pRows, int RowCount) {
		for(int i = 0; i < RowCount; ++i)
		{
			CUIRect RowRect, LabelRect, ValueRect;
			ColumnRect.HSplitTop(RowHeight, &RowRect, &ColumnRect);
			RowRect.VSplitLeft(RowRect.w * 0.31f, &LabelRect, &ValueRect);
			Ui()->DoLabel(&LabelRect, pRows[i].m_pLabel, LabelFontSize, TEXTALIGN_ML);
			Ui()->DoLabel(&ValueRect, pRows[i].m_pValue, ValueFontSize, TEXTALIGN_MR);
			if(i + 1 < RowCount)
				ColumnRect.HSplitTop(RowGap, nullptr, &ColumnRect);
		}
	};

	RenderColumn(Left, aLeftRows, std::size(aLeftRows));
	RenderColumn(Right, aRightRows, std::size(aRightRows));
}

void CQmMonitoring::RenderHud(CUIRect View) const
{
	const float UiScale = QmComputeMonitoringUiScale(Graphics()->ScreenWidth(), Graphics()->ScreenHeight());
	const float Padding = std::round(QM_MONITORING_PANEL_PADDING * UiScale);
	const float HeaderHeight = QM_MONITORING_HEADER_HEIGHT * UiScale;
	const float SectionGap = QM_MONITORING_SECTION_GAP * UiScale;

	View.Draw(ApplyHudOpacity(PANEL_BG), IGraphics::CORNER_ALL, 8.0f * UiScale);
	CUIRect Content;
	View.Margin(Padding, &Content);

	CUIRect Header, MainGraph, FpsGraph, MetricsGrid, DebugDetails;
	const SQmMonitoringBodyLayout BodyLayout = QmComputeMonitoringBodyLayout(Content.h, UiScale);
	Content.HSplitTop(HeaderHeight, &Header, &Content);
	Content.HSplitTop(SectionGap, nullptr, &Content);
	Content.HSplitTop(BodyLayout.m_MainGraphHeight, &MainGraph, &Content);
	Content.HSplitTop(SectionGap, nullptr, &Content);
	Content.HSplitTop(BodyLayout.m_FpsGraphHeight, &FpsGraph, &Content);
	Content.HSplitTop(SectionGap, nullptr, &Content);
	Content.HSplitTop(BodyLayout.m_PrimaryCardsHeight, &MetricsGrid, &Content);
	Content.HSplitTop(SectionGap, nullptr, &Content);
	Content.HSplitTop(BodyLayout.m_MetricsExtraHeight, &DebugDetails, &Content);

	RenderHeader(Header);
	RenderMainGraph(MainGraph);
	RenderFpsGraph(FpsGraph);
	RenderPrimaryCards(MetricsGrid);
	if(DebugDetails.h > 0.0f)
		RenderDebugDetails(DebugDetails);
}
