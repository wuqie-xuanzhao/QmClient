// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H
#define GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H

#include <base/system.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

constexpr float QmHudMediaIslandDesignScale = 0.8f;

constexpr float QmHudMediaIslandScaled(float Value)
{
	return Value * QmHudMediaIslandDesignScale;
}

struct SHudMediaIslandTrackInput
{
	const char *m_pTitle = "";
	const char *m_pArtist = "";
	const char *m_pAlbum = "";
	IGraphics::CTextureHandle m_Cover;
	bool m_HasCover = false;
	int64_t m_DurationMs = 0;
};

struct SHudMediaIslandTrackSnapshot
{
	char m_aTitle[128] = {};
	char m_aArtist[128] = {};
	char m_aAlbum[128] = {};
	IGraphics::CTextureHandle m_Cover;
	bool m_HasCover = false;
	int64_t m_DurationMs = 0;

	void Reset()
	{
		m_aTitle[0] = '\0';
		m_aArtist[0] = '\0';
		m_aAlbum[0] = '\0';
		m_Cover = IGraphics::CTextureHandle();
		m_HasCover = false;
		m_DurationMs = 0;
	}

	void SetFrom(const SHudMediaIslandTrackInput &Input)
	{
		str_copy(m_aTitle, Input.m_pTitle != nullptr ? Input.m_pTitle : "", sizeof(m_aTitle));
		str_copy(m_aArtist, Input.m_pArtist != nullptr ? Input.m_pArtist : "", sizeof(m_aArtist));
		str_copy(m_aAlbum, Input.m_pAlbum != nullptr ? Input.m_pAlbum : "", sizeof(m_aAlbum));
		m_Cover = Input.m_Cover;
		m_HasCover = Input.m_HasCover && Input.m_Cover.IsValid();
		m_DurationMs = Input.m_DurationMs;
	}

	bool HasMeaningfulIdentity() const
	{
		return m_aTitle[0] != '\0' || m_aArtist[0] != '\0' || m_aAlbum[0] != '\0';
	}
};

enum class EHudMediaIslandTrackUpdate
{
	NONE,
	FIRST_IDENTITY,
	TRACK_CHANGED,
};

enum class EHudMediaIslandCountdownType
{
	SWAP = 0,
	SWITCH,
	MUTE,
};

enum class EHudMediaIslandMuteMessage
{
	NONE = 0,
	SPAM_BROADCAST,
	REMAINING,
};

struct SHudMediaIslandCountdownInput
{
	EHudMediaIslandCountdownType m_Type = EHudMediaIslandCountdownType::SWAP;
	int m_Id = 0;
	int64_t m_TriggerTick = 0;
	int64_t m_EndTick = 0;
	int64_t m_DurationTicks = 0;
	float m_Progress = 0.0f;
	bool m_Completed = false;
	bool m_SwapOutgoing = false;
};

enum class EQmSwitchCountdownMode
{
	FOLLOW_TEE = 0,
	MEDIA_ISLAND,
	BOTH,
};

inline bool QmHudSwitchCountdownShowsFollowTee(int Mode)
{
	return Mode == static_cast<int>(EQmSwitchCountdownMode::FOLLOW_TEE) ||
	       Mode == static_cast<int>(EQmSwitchCountdownMode::BOTH);
}

inline bool QmHudSwitchCountdownShowsMediaIsland(int Mode)
{
	return Mode == static_cast<int>(EQmSwitchCountdownMode::MEDIA_ISLAND) ||
	       Mode == static_cast<int>(EQmSwitchCountdownMode::BOTH);
}

inline int QmHudSwitchCountdownModeFromLocations(bool FollowTee, bool MediaIsland, int CurrentMode)
{
	if(FollowTee && MediaIsland)
		return static_cast<int>(EQmSwitchCountdownMode::BOTH);
	if(FollowTee)
		return static_cast<int>(EQmSwitchCountdownMode::FOLLOW_TEE);
	if(MediaIsland)
		return static_cast<int>(EQmSwitchCountdownMode::MEDIA_ISLAND);
	return std::clamp(CurrentMode, static_cast<int>(EQmSwitchCountdownMode::FOLLOW_TEE), static_cast<int>(EQmSwitchCountdownMode::BOTH));
}

struct SHudSwitchCountdownEntry
{
	int m_Team = 0;
	int m_Number = 0;
	int m_ClientId = -1;
	int m_Connection = 0;
	int m_TriggerTick = 0;
	int m_EndTick = 0;
	int m_CurrentTick = 0;
};

inline int QmHudSelectLatestSwitchCountdowns(
	const SHudSwitchCountdownEntry *pEntries,
	size_t EntryCount,
	SHudSwitchCountdownEntry *pSelected,
	size_t SelectedCapacity)
{
	if(pEntries == nullptr || pSelected == nullptr || SelectedCapacity == 0)
		return 0;

	const auto IsNewer = [](const SHudSwitchCountdownEntry &Left, const SHudSwitchCountdownEntry &Right) {
		if(Left.m_TriggerTick != Right.m_TriggerTick)
			return Left.m_TriggerTick > Right.m_TriggerTick;
		if(Left.m_Team != Right.m_Team)
			return Left.m_Team < Right.m_Team;
		return Left.m_Number < Right.m_Number;
	};

	size_t SelectedCount = 0;
	for(size_t i = 0; i < EntryCount; ++i)
	{
		const SHudSwitchCountdownEntry &Entry = pEntries[i];
		if(Entry.m_ClientId < 0 || Entry.m_TriggerTick <= 0 || Entry.m_EndTick <= Entry.m_CurrentTick)
			continue;

		if(SelectedCount < SelectedCapacity)
			pSelected[SelectedCount++] = Entry;
		else if(IsNewer(Entry, pSelected[SelectedCount - 1]))
			pSelected[SelectedCount - 1] = Entry;
		else
			continue;

		std::stable_sort(pSelected, pSelected + SelectedCount, IsNewer);
	}
	return static_cast<int>(SelectedCount);
}

inline int QmHudSwitchCountdownFollowSide(float TeeX, bool PetVisible, float PetX)
{
	if(PetVisible && PetX < TeeX)
		return 1;
	return -1;
}

inline vec2 QmHudSwitchCountdownFollowTarget(vec2 TeePosition, int Side, int Slot, float Now)
{
	constexpr float BaseOffsetX = 48.0f;
	constexpr float ItemSpacing = 24.0f;
	constexpr float OffsetY = 52.0f;
	constexpr float BobAmount = 4.0f;
	Side = Side < 0 ? -1 : 1;
	Slot = std::max(0, Slot);
	return TeePosition + vec2(
				     Side * (BaseOffsetX + ItemSpacing * Slot),
				     -OffsetY + std::sin(Now / 2.0f) * BobAmount);
}

struct SHudMediaIslandSwapLifecycle
{
	bool m_Visible = false;
	bool m_Completed = false;
	int m_SecondsLeft = 0;
	int64_t m_EndTick = 0;
	int64_t m_DurationTicks = 0;
	float m_Progress = 0.0f;
};

inline SHudMediaIslandSwapLifecycle QmHudMediaIslandSwapLifecycle(int64_t StartTick, int64_t NowTick, int TickSpeed)
{
	SHudMediaIslandSwapLifecycle Lifecycle;
	if(StartTick <= 0 || TickSpeed <= 0 || NowTick < StartTick)
		return Lifecycle;

	constexpr int CountdownSeconds = 30;
	constexpr int ReadyDisplaySeconds = 30;
	const int64_t ElapsedTicks = NowTick - StartTick;
	Lifecycle.m_DurationTicks = static_cast<int64_t>(CountdownSeconds) * TickSpeed;
	Lifecycle.m_EndTick = StartTick + Lifecycle.m_DurationTicks;
	if(ElapsedTicks >= static_cast<int64_t>(CountdownSeconds + ReadyDisplaySeconds) * TickSpeed)
		return Lifecycle;

	Lifecycle.m_Visible = true;
	Lifecycle.m_Completed = ElapsedTicks >= Lifecycle.m_DurationTicks;
	Lifecycle.m_SecondsLeft = Lifecycle.m_Completed ? 0 : CountdownSeconds - static_cast<int>(ElapsedTicks / TickSpeed);
	Lifecycle.m_Progress = Lifecycle.m_Completed ? 0.0f : std::clamp((Lifecycle.m_EndTick - NowTick) / static_cast<float>(Lifecycle.m_DurationTicks), 0.0f, 1.0f);
	return Lifecycle;
}

inline SHudMediaIslandCountdownInput QmHudMediaIslandSwapCountdownInput(int Id, int64_t StartTick, const SHudMediaIslandSwapLifecycle &Lifecycle, bool SwapOutgoing)
{
	return {
		EHudMediaIslandCountdownType::SWAP,
		Id,
		StartTick,
		Lifecycle.m_EndTick,
		Lifecycle.m_DurationTicks,
		Lifecycle.m_Progress,
		Lifecycle.m_Completed,
		SwapOutgoing};
}

inline bool QmHudMediaIslandSwapVisibleForConnection(int SwapConnection, int ActiveConnection)
{
	return SwapConnection >= 0 && ActiveConnection >= 0 && SwapConnection == ActiveConnection;
}

inline bool QmHudMediaIslandShouldShowTeam(bool ShowTeam, bool EntitiesDDRace, int Team)
{
	return ShowTeam && EntitiesDDRace && Team > 0;
}

struct SHudMediaIslandTimerRowLayout
{
	float m_RaceY = 0.0f;
	float m_RaceH = 0.0f;
	float m_CheckpointY = 0.0f;
	float m_CheckpointH = 0.0f;
};

inline SHudMediaIslandTimerRowLayout QmHudMediaIslandTimerRows(float BoxY, float BoxH, bool HasSecondaryLine)
{
	BoxH = std::max(0.0f, BoxH);
	if(!HasSecondaryLine)
		return {BoxY, BoxH, BoxY + BoxH, 0.0f};

	const float TopMargin = BoxH * 0.10f;
	const float RaceH = BoxH * 0.60f;
	const float SecondaryY = BoxY + BoxH * 0.70f;
	return {BoxY + TopMargin, RaceH, SecondaryY, BoxH - TopMargin - RaceH};
}

struct SHudMediaIslandSwapRows
{
	int m_InlineSwapCount = 0;
	int m_BottomSwapCount = 0;
	int m_BottomLineCount = 0;
	int m_LyricsLineIndex = -1;
};

inline SHudMediaIslandSwapRows QmHudMediaIslandSwapRows(int IncomingSwapCount, bool HasRaceTimer, bool ShowLyrics)
{
	SHudMediaIslandSwapRows Result;
	IncomingSwapCount = std::max(0, IncomingSwapCount);
	Result.m_InlineSwapCount = HasRaceTimer && IncomingSwapCount > 0 ? 1 : 0;
	Result.m_BottomSwapCount = IncomingSwapCount - Result.m_InlineSwapCount;
	Result.m_LyricsLineIndex = ShowLyrics ? Result.m_BottomSwapCount : -1;
	Result.m_BottomLineCount = Result.m_BottomSwapCount + (ShowLyrics ? 1 : 0);
	return Result;
}

struct SHudMediaIslandInfoStackLayout
{
	float m_TopCenterY = 0.0f;
	float m_BottomCenterY = 0.0f;
};

inline SHudMediaIslandInfoStackLayout QmHudMediaIslandMirroredInfoStack(float IslandY, float IslandHeight, float TextHeight, float TextGap)
{
	const float MidY = IslandY + std::max(0.0f, IslandHeight) * 0.5f;
	const float CenterOffset = (std::max(0.0f, TextHeight) + std::max(0.0f, TextGap)) * 0.5f;
	return {MidY - CenterOffset, MidY + CenterOffset};
}

inline float QmHudMediaIslandWaveBarHeight(int BarIndex, float TimeSeconds, float Activity)
{
	constexpr float RestHeight = 0.20f;
	Activity = std::clamp(Activity, 0.0f, 1.0f);
	if(Activity <= 0.0f)
		return RestHeight;

	struct SWaveBarMotion
	{
		float m_PrimaryFrequency;
		float m_SecondaryFrequency;
		float m_PrimaryPhase;
		float m_SecondaryPhase;
		float m_PrimaryWeight;
	};
	constexpr std::array<SWaveBarMotion, 7> aMotions = {{
		{2.9f, 8.1f, 0.2f, 1.7f, 0.72f},
		{4.7f, 10.6f, 2.4f, 5.2f, 0.43f},
		{6.8f, 13.4f, 4.7f, 0.6f, 0.65f},
		{3.8f, 9.3f, 1.1f, 3.8f, 0.38f},
		{5.7f, 12.0f, 5.6f, 2.9f, 0.57f},
		{7.45f, 8.77f, 4.92f, 1.86f, 0.58f},
		{2.6f, 14.77f, 0.1f, 2.2f, 0.41f},
	}};
	const size_t MotionIndex = static_cast<size_t>(std::max(0, BarIndex)) % aMotions.size();
	const SWaveBarMotion &Motion = aMotions[MotionIndex];
	const float Primary = 0.5f + 0.5f * std::sin(TimeSeconds * Motion.m_PrimaryFrequency + Motion.m_PrimaryPhase);
	const float Secondary = 0.5f + 0.5f * std::sin(TimeSeconds * Motion.m_SecondaryFrequency + Motion.m_SecondaryPhase);
	const float DynamicHeight = std::clamp(RestHeight + (1.0f - RestHeight) * (Primary * Motion.m_PrimaryWeight + Secondary * (1.0f - Motion.m_PrimaryWeight)), RestHeight, 1.0f);
	return RestHeight + (DynamicHeight - RestHeight) * Activity;
}

inline float QmHudMediaIslandWaveBarSettleProgress(int BarIndex, int BarCount, float ElapsedSeconds)
{
	constexpr float LayerStaggerSeconds = 0.15f;
	constexpr float LayerSettleSeconds = 0.45f;
	if(BarCount <= 0)
		return 1.0f;

	BarIndex = std::clamp(BarIndex, 0, BarCount - 1);
	const int Layer = std::min(BarIndex, BarCount - 1 - BarIndex);
	const float LinearProgress = std::clamp((ElapsedSeconds - Layer * LayerStaggerSeconds) / LayerSettleSeconds, 0.0f, 1.0f);
	return LinearProgress * LinearProgress * (3.0f - 2.0f * LinearProgress);
}

struct SHudMediaIslandBlobPose
{
	float m_Travel = 0.0f;
	float m_RadiusScale = 0.0f;
	float m_StretchX = 1.0f;
	float m_StretchY = 1.0f;
	float m_ContentAlpha = 0.0f;
};

struct SHudMediaIslandLiquidCapsule
{
	CUIRect m_Rect{};
	float m_Radius = 0.0f;
	float m_SmoothUnion = 0.0f;
	float m_ContentAlpha = 0.0f;
};

struct SHudMediaIslandEntrancePose
{
	CUIRect m_Rect{};
	ColorRGBA m_BackgroundColor{};
	float m_Radius = 0.0f;
	float m_DisabledCornerRadius = 0.0f;
	float m_ContentAlpha = 0.0f;
};

struct SHudMediaIslandEntranceTimeline
{
	float m_DropProgress = 0.0f;
	float m_ExpandProgress = 0.0f;
};

inline float QmHudMediaIslandLiquidSmoothStep(float Value)
{
	Value = std::clamp(Value, 0.0f, 1.0f);
	return Value * Value * (3.0f - 2.0f * Value);
}

inline float QmHudMediaIslandLiquidSegment(float Progress, float Start, float End)
{
	if(End <= Start)
		return Progress >= End ? 1.0f : 0.0f;
	return QmHudMediaIslandLiquidSmoothStep((Progress - Start) / (End - Start));
}

inline float QmHudAdvanceMediaIslandEntranceProgress(float Current, float DeltaSeconds, int MotionLevel)
{
	Current = std::clamp(Current, 0.0f, 1.0f);
	MotionLevel = std::clamp(MotionLevel, 0, 2);
	if(MotionLevel == 0)
		return 1.0f;
	if(DeltaSeconds <= 0.0f)
		return Current;
	const float DurationSeconds = 0.55f * (MotionLevel == 1 ? 0.45f : 1.0f);
	return std::clamp(Current + DeltaSeconds / DurationSeconds, 0.0f, 1.0f);
}

inline float QmHudAdvanceMediaIslandEntranceDropProgress(float Current, float DeltaSeconds, int MotionLevel)
{
	Current = std::clamp(Current, 0.0f, 1.0f);
	MotionLevel = std::clamp(MotionLevel, 0, 2);
	if(MotionLevel == 0)
		return 1.0f;
	if(DeltaSeconds <= 0.0f)
		return Current;
	const float DurationSeconds = 0.18f * (MotionLevel == 1 ? 0.45f : 1.0f);
	return std::clamp(Current + DeltaSeconds / DurationSeconds, 0.0f, 1.0f);
}

inline SHudMediaIslandEntranceTimeline QmHudAdvanceMediaIslandEntranceTimeline(SHudMediaIslandEntranceTimeline Timeline, float DeltaSeconds, int MotionLevel)
{
	if(MotionLevel <= 0)
		return {1.0f, 1.0f};
	if(Timeline.m_DropProgress < 1.0f)
		Timeline.m_DropProgress = QmHudAdvanceMediaIslandEntranceDropProgress(Timeline.m_DropProgress, DeltaSeconds, MotionLevel);
	else
		Timeline.m_ExpandProgress = QmHudAdvanceMediaIslandEntranceProgress(Timeline.m_ExpandProgress, DeltaSeconds, MotionLevel);
	return Timeline;
}

inline SHudMediaIslandEntrancePose QmHudMediaIslandEntrancePose(const CUIRect &TargetRect, float TargetRadius, const ColorRGBA &TargetColor, float Progress, float DropProgress = 1.0f, float ScreenTop = 0.0f)
{
	constexpr float InitialDiameter = QmHudMediaIslandScaled(16.0f);
	constexpr float InitialRadius = InitialDiameter * 0.5f;
	constexpr float InitialHiddenGap = QmHudMediaIslandScaled(2.0f);
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	const float ShapeProgress = QmHudMediaIslandLiquidSmoothStep(Progress);
	const auto Lerp = [](float From, float To, float Amount) {
		return From + (To - From) * Amount;
	};

	const float TargetCenterX = TargetRect.x + TargetRect.w * 0.5f;
	const float TargetCenterY = TargetRect.y + TargetRect.h * 0.5f;
	const float Width = Lerp(InitialDiameter, TargetRect.w, ShapeProgress);
	const float Height = Lerp(InitialDiameter, TargetRect.h, ShapeProgress);
	DropProgress = std::clamp(DropProgress, 0.0f, 1.0f);
	const float DropEase = 1.0f - (1.0f - DropProgress) * (1.0f - DropProgress) * (1.0f - DropProgress);
	const float DropStartY = ScreenTop - InitialDiameter - InitialHiddenGap;
	const float DropEndY = TargetCenterY - InitialRadius;
	const float DropY = Lerp(DropStartY, DropEndY, DropEase);

	SHudMediaIslandEntrancePose Pose;
	Pose.m_Rect = {TargetCenterX - Width * 0.5f, TargetCenterY - Height * 0.5f, Width, Height};
	Pose.m_Rect.y += (DropY - DropEndY) * (1.0f - ShapeProgress);
	Pose.m_Radius = Lerp(InitialRadius, TargetRadius, ShapeProgress);
	Pose.m_DisabledCornerRadius = Lerp(InitialRadius, 0.0f, ShapeProgress);
	Pose.m_BackgroundColor = ColorRGBA(
		Lerp(0.0f, TargetColor.r, ShapeProgress),
		Lerp(0.0f, TargetColor.g, ShapeProgress),
		Lerp(0.0f, TargetColor.b, ShapeProgress),
		Lerp(1.0f, TargetColor.a, ShapeProgress));
	Pose.m_ContentAlpha = QmHudMediaIslandLiquidSegment(Progress, 0.92f, 1.0f);
	return Pose;
}

inline float QmHudMediaIslandBlobSpringTravel(float Progress)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	if(Progress <= 0.0f || Progress >= 1.0f)
		return Progress;
	constexpr float Response = 7.0f;
	const float EndValue = 1.0f - (1.0f + Response) * std::exp(-Response);
	return (1.0f - (1.0f + Response * Progress) * std::exp(-Response * Progress)) / EndValue;
}

inline float QmHudMediaIslandBlobSpringVelocity(float Progress)
{
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	if(Progress <= 0.0f || Progress >= 1.0f)
		return 0.0f;
	constexpr float Response = 7.0f;
	const float EndValue = 1.0f - (1.0f + Response) * std::exp(-Response);
	return Response * Response * Progress * std::exp(-Response * Progress) / EndValue;
}

inline SHudMediaIslandBlobPose QmHudMediaIslandBlobPose(float Progress)
{
	SHudMediaIslandBlobPose Pose;
	Pose.m_Travel = QmHudMediaIslandBlobSpringTravel(Progress);
	Pose.m_RadiusScale = QmHudMediaIslandLiquidSmoothStep(std::clamp(Pose.m_Travel * 1.55f, 0.0f, 1.0f));
	const float Motion = std::clamp(std::abs(QmHudMediaIslandBlobSpringVelocity(Progress)) * 0.32f, 0.0f, 1.0f);
	Pose.m_StretchX = 1.0f + Motion * 0.065f;
	Pose.m_StretchY = 1.0f - Motion * 0.035f;
	Pose.m_ContentAlpha = QmHudMediaIslandLiquidSmoothStep(std::clamp((Pose.m_RadiusScale - 0.25f) / 0.75f, 0.0f, 1.0f));
	return Pose;
}

inline float QmHudMediaIslandBlobBlend(float Radius, float RadiusScale)
{
	return std::max(0.0f, Radius) * 0.72f * std::clamp(RadiusScale, 0.0f, 1.0f);
}

inline float QmHudMediaIslandBlobConnectionStrength(float Travel)
{
	constexpr float DetachStart = 0.86f;
	constexpr float DetachEnd = 0.985f;
	const float DetachProgress = std::clamp((Travel - DetachStart) / (DetachEnd - DetachStart), 0.0f, 1.0f);
	return 1.0f - QmHudMediaIslandLiquidSmoothStep(DetachProgress);
}

inline SHudMediaIslandLiquidCapsule QmHudMediaIslandRightBlobCapsule(float MainRight, float CenterY, float Radius, float ContentWidth, float RestGap, const SHudMediaIslandBlobPose &Pose)
{
	Radius = std::max(0.0f, Radius);
	const float Diameter = Radius * 2.0f;
	const float FinalWidth = std::max(Diameter, ContentWidth);
	const float SpawnCenterX = MainRight - Radius * 0.15f;
	const float FinalCenterX = MainRight + std::max(0.0f, RestGap) + FinalWidth * 0.5f;
	const auto Lerp = [](float From, float To, float Amount) {
		return From + (To - From) * std::clamp(Amount, 0.0f, 1.1f);
	};
	const float CenterX = Lerp(SpawnCenterX, FinalCenterX, Pose.m_Travel);
	const float BaseWidth = Lerp(Diameter, FinalWidth, Pose.m_ContentAlpha);
	const float Width = std::max(0.0f, BaseWidth * Pose.m_RadiusScale * Pose.m_StretchX);
	const float Height = std::max(0.0f, Diameter * Pose.m_RadiusScale * Pose.m_StretchY);

	SHudMediaIslandLiquidCapsule Capsule;
	Capsule.m_Rect = {CenterX - Width * 0.5f, CenterY - Height * 0.5f, Width, Height};
	Capsule.m_Radius = std::min(Width, Height) * 0.5f;
	Capsule.m_SmoothUnion = QmHudMediaIslandBlobBlend(Radius, Pose.m_RadiusScale) * QmHudMediaIslandBlobConnectionStrength(Pose.m_Travel);
	Capsule.m_ContentAlpha = Pose.m_ContentAlpha;
	return Capsule;
}

inline float QmHudAdvanceMediaIslandLiquidProgress(float Current, bool TargetVisible, float DeltaSeconds, bool MotionEnabled)
{
	Current = std::clamp(Current, 0.0f, 1.0f);
	if(!MotionEnabled)
		return TargetVisible ? 1.0f : 0.0f;
	if(DeltaSeconds <= 0.0f)
		return Current;
	constexpr float DurationSeconds = 0.440f;
	const float Delta = DeltaSeconds / DurationSeconds;
	return std::clamp(Current + (TargetVisible ? Delta : -Delta), 0.0f, 1.0f);
}

struct SHudMediaIslandSpectatorIconPose
{
	float m_OpenAlpha = 0.0f;
	float m_ClosedAlpha = 1.0f;
	float m_OpenScaleX = 0.88f;
	float m_OpenScaleY = 0.44f;
	float m_ClosedScale = 1.0f;
	float m_CountAlpha = 0.0f;
	float m_CountOffsetX = -QmHudMediaIslandScaled(3.0f);
};

inline float QmHudAdvanceMediaIslandSpectatorIconProgress(float Current, float DeltaSeconds, int MotionLevel)
{
	Current = std::clamp(Current, 0.0f, 1.0f);
	MotionLevel = std::clamp(MotionLevel, 0, 2);
	if(MotionLevel == 0)
		return 1.0f;
	if(DeltaSeconds <= 0.0f)
		return Current;
	const float DurationSeconds = 0.180f * (MotionLevel == 1 ? 0.45f : 1.0f);
	return std::clamp(Current + DeltaSeconds / DurationSeconds, 0.0f, 1.0f);
}

inline float QmHudMediaIslandSpectatorIconProgressDuringExit(float IconProgressAtStart, float LiquidProgressAtStart, float LiquidProgress)
{
	IconProgressAtStart = std::clamp(IconProgressAtStart, 0.0f, 1.0f);
	LiquidProgressAtStart = std::clamp(LiquidProgressAtStart, 0.0f, 1.0f);
	LiquidProgress = std::clamp(LiquidProgress, 0.0f, 1.0f);
	if(LiquidProgressAtStart <= 0.0f)
		return 0.0f;
	return IconProgressAtStart * std::clamp(LiquidProgress / LiquidProgressAtStart, 0.0f, 1.0f);
}

inline SHudMediaIslandSpectatorIconPose QmHudMediaIslandSpectatorIconPose(float Progress)
{
	const float Eased = QmHudMediaIslandLiquidSmoothStep(Progress);
	SHudMediaIslandSpectatorIconPose Pose;
	Pose.m_OpenAlpha = Eased;
	Pose.m_ClosedAlpha = 1.0f - Eased;
	Pose.m_OpenScaleX = 0.88f + 0.12f * Eased;
	Pose.m_OpenScaleY = 0.44f + 0.56f * Eased;
	Pose.m_ClosedScale = 1.0f - 0.12f * Eased;
	Pose.m_CountAlpha = Eased;
	Pose.m_CountOffsetX = -QmHudMediaIslandScaled(3.0f) * (1.0f - Eased);
	return Pose;
}

inline bool QmHudMediaIslandShouldAnimateSpectatorEyeOpen(bool HasSpectators, bool HadSpectators, float LiquidProgress)
{
	return HasSpectators && !HadSpectators && LiquidProgress > 0.0f;
}

inline float QmHudMediaIslandSpectatorCountAlpha(bool HasSpectators, const SHudMediaIslandSpectatorIconPose &Pose)
{
	return HasSpectators ? Pose.m_CountAlpha : 0.0f;
}

// Value-only inputs for the GPU media-island SDF command. The renderer owns
// the fixed ABI packing; HUD animation state stays independent of backend
// handles and command-buffer lifetime.
constexpr int QmHudMediaIslandSdfMaxItems = IGraphics::MEDIA_ISLAND_SDF_MAX_ITEMS;

struct SHudMediaIslandSdfItem
{
	vec2 m_Center{};
	vec2 m_Radii{};
	float m_SmoothUnion = 0.0f;
	float m_ContentAlpha = 0.0f;
	float m_ContentScale = 1.0f;
	float m_CountdownProgress = 0.0f;
	ColorRGBA m_RingColor{};
};

struct SHudMediaIslandSdfCapsule
{
	CUIRect m_Rect{};
	float m_Radius = 0.0f;
	float m_SmoothUnion = 0.0f;
};

inline bool QmHudMediaIslandShouldPrepareBackdropBlur(int BackgroundOpacity)
{
	return BackgroundOpacity < 100;
}

inline vec4 QmHudMediaIslandBackdropUv(const CUIRect &OuterRect, const CUIRect &ScreenRect)
{
	if(OuterRect.w <= 0.0f || OuterRect.h <= 0.0f || ScreenRect.w <= 0.0f || ScreenRect.h <= 0.0f)
		return vec4();
	return vec4(
		(OuterRect.x - ScreenRect.x) / ScreenRect.w,
		1.0f - (OuterRect.y - ScreenRect.y) / ScreenRect.h,
		OuterRect.w / ScreenRect.w,
		-OuterRect.h / ScreenRect.h);
}

struct SHudMediaIslandSdfRenderState
{
	CUIRect m_Rect{};
	CUIRect m_MainRect{};
	float m_MainRadius = 0.0f;
	int m_MainCorners = IGraphics::CORNER_NONE;
	float m_MainDisabledCornerRadius = 0.0f;
	std::array<SHudMediaIslandSdfItem, QmHudMediaIslandSdfMaxItems> m_Items{};
	int m_ItemCount = 0;
	bool m_HasRightCapsule = false;
	SHudMediaIslandSdfCapsule m_RightCapsule{};
	float m_RingRadius = 0.0f;
	float m_RingThickness = 0.0f;
	ColorRGBA m_BackgroundColor{};
	float m_ScreenPixelSize = 1.0f;
	float m_OuterShadowSize = 0.0f;
	float m_OuterShadowOpacity = 0.0f;
	vec4 m_BackdropUv{};
};

inline float QmHudMediaIslandSdfPadding(const SHudMediaIslandSdfRenderState &State)
{
	float MaxSmoothUnion = 0.0f;
	const int ItemCount = std::clamp(State.m_ItemCount, 0, QmHudMediaIslandSdfMaxItems);
	for(int i = 0; i < ItemCount; ++i)
		MaxSmoothUnion = std::max(MaxSmoothUnion, State.m_Items[i].m_SmoothUnion);
	if(ItemCount > 1)
		MaxSmoothUnion = std::max(MaxSmoothUnion, std::max(0.0f, State.m_MainRadius) * 0.28f);
	if(State.m_HasRightCapsule)
		MaxSmoothUnion = std::max(MaxSmoothUnion, State.m_RightCapsule.m_SmoothUnion);

	// SmoothUnion can move the zero contour outwards by Blend / 4. Keep the
	// shader feather inside the quad as well, otherwise the liquid edge clips.
	const float Feather = std::max(State.m_ScreenPixelSize, 0.0001f) * 0.9f;
	const float ShapeOverflow = std::max(0.0f, MaxSmoothUnion) * 0.25f + Feather;
	const float ShadowOverflow = std::max(0.0f, State.m_OuterShadowSize) + Feather;
	return std::max(1.5f, std::max(ShapeOverflow, ShadowOverflow));
}

inline CUIRect QmHudMediaIslandSdfOuterRect(const SHudMediaIslandSdfRenderState &State)
{
	float Left = State.m_MainRect.x;
	float Top = State.m_MainRect.y;
	float Right = State.m_MainRect.x + State.m_MainRect.w;
	float Bottom = State.m_MainRect.y + State.m_MainRect.h;
	const int ItemCount = std::clamp(State.m_ItemCount, 0, QmHudMediaIslandSdfMaxItems);
	for(int i = 0; i < ItemCount; ++i)
	{
		const SHudMediaIslandSdfItem &Item = State.m_Items[i];
		const float RadiusX = std::max(0.0f, Item.m_Radii.x);
		const float RadiusY = std::max(0.0f, Item.m_Radii.y);
		Left = std::min(Left, Item.m_Center.x - RadiusX);
		Top = std::min(Top, Item.m_Center.y - RadiusY);
		Right = std::max(Right, Item.m_Center.x + RadiusX);
		Bottom = std::max(Bottom, Item.m_Center.y + RadiusY);
	}
	if(State.m_HasRightCapsule && State.m_RightCapsule.m_Rect.w > 0.0f && State.m_RightCapsule.m_Rect.h > 0.0f)
	{
		const CUIRect &Rect = State.m_RightCapsule.m_Rect;
		Left = std::min(Left, Rect.x);
		Top = std::min(Top, Rect.y);
		Right = std::max(Right, Rect.x + Rect.w);
		Bottom = std::max(Bottom, Rect.y + Rect.h);
	}

	const float Padding = QmHudMediaIslandSdfPadding(State);
	return {Left - Padding, Top - Padding, Right - Left + Padding * 2.0f, Bottom - Top + Padding * 2.0f};
}

inline vec4 QmHudMediaIslandSdfRectVec4(const CUIRect &Rect)
{
	return vec4(Rect.x, Rect.y, Rect.w, Rect.h);
}

inline bool QmHudMediaIslandBuildGpuSdfParams(const SHudMediaIslandSdfRenderState &State, IGraphics::SMediaIslandSdfParams &Params)
{
	if(State.m_Rect.w <= 0.0f || State.m_Rect.h <= 0.0f || State.m_MainRect.w <= 0.0f || State.m_MainRect.h <= 0.0f || State.m_ItemCount < 0 || State.m_ItemCount > QmHudMediaIslandSdfMaxItems)
		return false;

	Params.Clear();
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RECT] = QmHudMediaIslandSdfRectVec4(State.m_Rect);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_MAIN_RECT] = QmHudMediaIslandSdfRectVec4(State.m_MainRect);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_CAPSULE_RECT] = QmHudMediaIslandSdfRectVec4(State.m_RightCapsule.m_Rect);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKGROUND] = vec4(State.m_BackgroundColor.r, State.m_BackgroundColor.g, State.m_BackgroundColor.b, State.m_BackgroundColor.a);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_MAIN_PARAMS] = vec4(State.m_MainRadius, State.m_MainDisabledCornerRadius, State.m_RingRadius, State.m_RingThickness);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_METADATA] = vec4((float)State.m_ItemCount, (float)State.m_MainCorners, State.m_HasRightCapsule ? 1.0f : 0.0f, std::max(State.m_ScreenPixelSize, 0.0001f));
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_CAPSULE_PARAMS] = vec4(State.m_RightCapsule.m_Radius, State.m_RightCapsule.m_SmoothUnion, 0.0f, 0.0f);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RESERVED] = vec4(std::max(0.0f, State.m_OuterShadowSize), std::clamp(State.m_OuterShadowOpacity, 0.0f, 1.0f), 0.0f, 0.0f);
	Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV] = State.m_BackdropUv;

	for(int i = 0; i < State.m_ItemCount; ++i)
	{
		const SHudMediaIslandSdfItem &Item = State.m_Items[i];
		Params.Item(i, 0) = vec4(Item.m_Center.x, Item.m_Center.y, Item.m_Radii.x, Item.m_Radii.y);
		Params.Item(i, 1) = vec4(Item.m_SmoothUnion, Item.m_ContentAlpha, Item.m_ContentScale, std::clamp(Item.m_CountdownProgress, 0.0f, 1.0f));
		Params.Item(i, 2) = vec4(Item.m_RingColor.r, Item.m_RingColor.g, Item.m_RingColor.b, Item.m_RingColor.a);
	}
	Params.SetItemCount(State.m_ItemCount);
	Params.SetMainCorners(State.m_MainCorners);
	Params.SetHasRightCapsule(State.m_HasRightCapsule);
	return true;
}

inline float QmHudMediaIslandSdfCircle(vec2 Point, vec2 Center, float Radius)
{
	return distance(Point, Center) - std::max(0.0f, Radius);
}

inline float QmHudMediaIslandSdfRoundedRect(vec2 Point, const CUIRect &Rect, float Radius, int Corners, float DisabledCornerRadius = 0.0f)
{
	if(Rect.w <= 0.0f || Rect.h <= 0.0f)
		return 1000000.0f;

	const vec2 HalfSize(Rect.w * 0.5f, Rect.h * 0.5f);
	const vec2 Center(Rect.x + HalfSize.x, Rect.y + HalfSize.y);
	const vec2 Local = Point - Center;
	int Corner = IGraphics::CORNER_BR;
	if(Local.x < 0.0f)
		Corner = Local.y < 0.0f ? IGraphics::CORNER_TL : IGraphics::CORNER_BL;
	else if(Local.y < 0.0f)
		Corner = IGraphics::CORNER_TR;

	const float RequestedCornerRadius = (Corners & Corner) != 0 ? Radius : DisabledCornerRadius;
	const float CornerRadius = std::clamp(RequestedCornerRadius, 0.0f, std::min(HalfSize.x, HalfSize.y));
	const vec2 DistanceToInner(
		std::abs(Local.x) - HalfSize.x + CornerRadius,
		std::abs(Local.y) - HalfSize.y + CornerRadius);
	const vec2 Outside(std::max(DistanceToInner.x, 0.0f), std::max(DistanceToInner.y, 0.0f));
	return length(Outside) + std::min(std::max(DistanceToInner.x, DistanceToInner.y), 0.0f) - CornerRadius;
}

inline float QmHudMediaIslandSdfSmoothUnion(float Left, float Right, float Blend)
{
	if(Blend <= 0.0f)
		return std::min(Left, Right);
	const float H = std::max(Blend - std::abs(Left - Right), 0.0f) / Blend;
	return std::min(Left, Right) - H * H * Blend * 0.25f;
}

inline void QmHudSortMediaIslandCountdowns(SHudMediaIslandCountdownInput *pInputs, size_t Count)
{
	if(pInputs == nullptr || Count < 2)
		return;

	std::stable_sort(pInputs, pInputs + Count, [](const SHudMediaIslandCountdownInput &Left, const SHudMediaIslandCountdownInput &Right) {
		if(Left.m_Type != Right.m_Type)
			return static_cast<int>(Left.m_Type) < static_cast<int>(Right.m_Type);
		if(Left.m_TriggerTick != Right.m_TriggerTick)
			return Left.m_TriggerTick < Right.m_TriggerTick;
		return Left.m_Id < Right.m_Id;
	});
}

inline float QmHudMediaIslandCountdownProgress(const SHudMediaIslandCountdownInput &Input, int64_t Now)
{
	if(Input.m_DurationTicks <= 0)
		return 0.0f;
	return std::clamp((Input.m_EndTick - Now) / static_cast<float>(Input.m_DurationTicks), 0.0f, 1.0f);
}

inline float QmHudMediaIslandDesiredBottomWidth(
	bool ShowLyrics,
	bool ShowTopRow,
	bool HasUtilityContent,
	float UtilityContentWidth,
	float LyricsOnlyContentWidth,
	float MaxUnifiedWidth,
	float HorizontalPadding)
{
	if(!HasUtilityContent && (!ShowLyrics || ShowTopRow))
		return 0.0f;

	const float ClampedMaxWidth = std::max(0.0f, MaxUnifiedWidth);
	const float ClampedPadding = std::max(0.0f, HorizontalPadding);
	const float MaxContentWidth = std::max(0.0f, ClampedMaxWidth - ClampedPadding * 2.0f);
	float ContentWidth = HasUtilityContent ? std::max(0.0f, UtilityContentWidth) : 0.0f;
	if(ShowLyrics && !ShowTopRow)
		ContentWidth = std::max(ContentWidth, std::max(0.0f, LyricsOnlyContentWidth));
	ContentWidth = std::min(ContentWidth, MaxContentWidth);
	return std::min(ClampedMaxWidth, ContentWidth + ClampedPadding * 2.0f);
}

inline float QmHudMediaIslandSatelliteWidth(int ItemCount, float Diameter, float ItemGap)
{
	if(ItemCount <= 0 || Diameter <= 0.0f)
		return 0.0f;
	return Diameter * ItemCount + std::max(0, ItemCount - 1) * std::max(0.0f, ItemGap);
}

inline int QmHudMediaIslandVisibleSuffixStart(int ItemCount, int VisibleLimit)
{
	return std::max(0, ItemCount - std::max(0, VisibleLimit));
}

inline int QmHudMediaIslandSwitchInstanceId(int Team, int Number)
{
	return (Team << 8) | (Number & 0xff);
}

namespace HudMediaIslandDetail
{
	inline bool ParsePositiveSeconds(const char *pText, int &Seconds, const char **ppEnd)
	{
		if(pText == nullptr || pText[0] < '0' || pText[0] > '9')
			return false;

		int Value = 0;
		const char *pCursor = pText;
		while(*pCursor >= '0' && *pCursor <= '9')
		{
			if(Value > 31536000)
				return false;
			Value = Value * 10 + (*pCursor - '0');
			++pCursor;
		}
		if(Value <= 0)
			return false;

		Seconds = Value;
		if(ppEnd != nullptr)
			*ppEnd = pCursor;
		return true;
	}

	inline bool ParseSpamBroadcastForName(const char *pText, const char *pExpected, int &Seconds)
	{
		if(pText == nullptr || pText[0] != '\'' || pExpected == nullptr || pExpected[0] == '\0')
			return false;
		const int NameLength = str_length(pExpected);
		if(str_length(pText) <= NameLength + 1 || str_comp_num(pText + 1, pExpected, NameLength) != 0 || pText[NameLength + 1] != '\'')
			return false;

		constexpr const char *pMutedPrefix = " has been muted for ";
		const char *pSeconds = str_startswith(pText + NameLength + 2, pMutedPrefix);
		const char *pEnd = nullptr;
		return pSeconds != nullptr && ParsePositiveSeconds(pSeconds, Seconds, &pEnd) && str_comp(pEnd, " seconds (Spam protection)") == 0;
	}
}

inline EHudMediaIslandMuteMessage QmHudParseSpamProtectionMute(const char *pText, const char *pMainName, const char *pDummyName, int &Seconds)
{
	Seconds = 0;
	if(pText == nullptr)
		return EHudMediaIslandMuteMessage::NONE;

	constexpr const char *pRemainingPrefix = "You are not permitted to talk for the next ";
	if(const char *pSeconds = str_startswith(pText, pRemainingPrefix))
	{
		const char *pEnd = nullptr;
		if(HudMediaIslandDetail::ParsePositiveSeconds(pSeconds, Seconds, &pEnd) && str_comp(pEnd, " seconds.") == 0)
			return EHudMediaIslandMuteMessage::REMAINING;
		Seconds = 0;
		return EHudMediaIslandMuteMessage::NONE;
	}

	if(HudMediaIslandDetail::ParseSpamBroadcastForName(pText, pMainName, Seconds) || HudMediaIslandDetail::ParseSpamBroadcastForName(pText, pDummyName, Seconds))
		return EHudMediaIslandMuteMessage::SPAM_BROADCAST;
	Seconds = 0;
	return EHudMediaIslandMuteMessage::NONE;
}

inline float QmHudTopEffectY(float DefaultY, float EffectHeight, float EffectLeft, float EffectRight, const CUIRect &IslandRect, bool IslandRectValid, float Gap = 3.0f)
{
	if(!IslandRectValid)
		return DefaultY;

	const float IslandRight = IslandRect.x + IslandRect.w;
	const float IslandBottom = IslandRect.y + IslandRect.h;
	const bool HorizontallyOverlaps = EffectRight > IslandRect.x && EffectLeft < IslandRight;
	const bool VerticallyOverlaps = DefaultY + EffectHeight > IslandRect.y && DefaultY < IslandBottom;
	if(!HorizontallyOverlaps || !VerticallyOverlaps)
		return DefaultY;

	return std::max(DefaultY, IslandBottom + Gap);
}

inline bool QmHudMediaIslandTrackChanged(const SHudMediaIslandTrackSnapshot &Current, const SHudMediaIslandTrackInput &Next)
{
	if(!Current.HasMeaningfulIdentity())
		return false;

	const char *pNextTitle = Next.m_pTitle != nullptr ? Next.m_pTitle : "";
	const char *pNextArtist = Next.m_pArtist != nullptr ? Next.m_pArtist : "";
	const char *pNextAlbum = Next.m_pAlbum != nullptr ? Next.m_pAlbum : "";
	const bool HasComparableTitle = Current.m_aTitle[0] != '\0' && pNextTitle[0] != '\0';
	const bool HasComparableArtist = Current.m_aArtist[0] != '\0' && pNextArtist[0] != '\0';
	const bool HasComparableAlbum = Current.m_aAlbum[0] != '\0' && pNextAlbum[0] != '\0';

	return (HasComparableTitle && str_comp(Current.m_aTitle, pNextTitle) != 0) ||
	       (HasComparableArtist && str_comp(Current.m_aArtist, pNextArtist) != 0) ||
	       (HasComparableAlbum && str_comp(Current.m_aAlbum, pNextAlbum) != 0);
}

inline bool QmHudMediaIslandTrackInputHasMeaningfulIdentity(const SHudMediaIslandTrackInput &Input)
{
	return (Input.m_pTitle != nullptr && Input.m_pTitle[0] != '\0') ||
	       (Input.m_pArtist != nullptr && Input.m_pArtist[0] != '\0') ||
	       (Input.m_pAlbum != nullptr && Input.m_pAlbum[0] != '\0');
}

inline EHudMediaIslandTrackUpdate QmHudMediaIslandUpdateTrackSnapshots(
	SHudMediaIslandTrackSnapshot &Current,
	SHudMediaIslandTrackSnapshot &Outgoing,
	bool &HasTrackIdentity,
	bool &TrackTransitionActive,
	bool &TrackTransitionNeedsNodeReset,
	int64_t &TrackTransitionStartTick,
	int64_t Now,
	const SHudMediaIslandTrackInput &Next)
{
	if(!HasTrackIdentity || !Current.HasMeaningfulIdentity())
	{
		Current.SetFrom(Next);
		Outgoing.Reset();
		HasTrackIdentity = true;
		TrackTransitionActive = false;
		TrackTransitionNeedsNodeReset = false;
		TrackTransitionStartTick = 0;
		return EHudMediaIslandTrackUpdate::FIRST_IDENTITY;
	}

	if(!QmHudMediaIslandTrackChanged(Current, Next))
	{
		if(QmHudMediaIslandTrackInputHasMeaningfulIdentity(Next))
			Current.SetFrom(Next);
		return EHudMediaIslandTrackUpdate::NONE;
	}

	Outgoing = Current;
	Current.SetFrom(Next);
	TrackTransitionActive = true;
	TrackTransitionNeedsNodeReset = true;
	TrackTransitionStartTick = Now;
	return EHudMediaIslandTrackUpdate::TRACK_CHANGED;
}

#endif
