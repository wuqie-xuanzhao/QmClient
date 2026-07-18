// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H
#define GAME_CLIENT_COMPONENTS_HUD_MEDIA_ISLAND_LOGIC_H

#include <base/system.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

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

enum class EHudMediaIslandLiquidPhase
{
	BULGE,
	FORM,
	STRETCH,
	BREAK,
	REBOUND,
};

struct SHudMediaIslandLiquidPose
{
	EHudMediaIslandLiquidPhase m_Phase = EHudMediaIslandLiquidPhase::BULGE;
	float m_CenterProgress = 0.0f;
	float m_RadiusScale = 0.0f;
	float m_StretchX = 1.0f;
	float m_StretchY = 1.0f;
	float m_SmoothUnionScale = 0.0f;
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

inline SHudMediaIslandEntrancePose QmHudMediaIslandEntrancePose(const CUIRect &TargetRect, float TargetRadius, const ColorRGBA &TargetColor, float Progress)
{
	constexpr float InitialDiameter = 16.0f;
	constexpr float InitialRadius = InitialDiameter * 0.5f;
	Progress = std::clamp(Progress, 0.0f, 1.0f);
	const float ShapeProgress = QmHudMediaIslandLiquidSmoothStep(Progress);
	const auto Lerp = [](float From, float To, float Amount) {
		return From + (To - From) * Amount;
	};

	const float TargetCenterX = TargetRect.x + TargetRect.w * 0.5f;
	const float TargetCenterY = TargetRect.y + TargetRect.h * 0.5f;
	const float Width = Lerp(InitialDiameter, TargetRect.w, ShapeProgress);
	const float Height = Lerp(InitialDiameter, TargetRect.h, ShapeProgress);

	SHudMediaIslandEntrancePose Pose;
	Pose.m_Rect = {TargetCenterX - Width * 0.5f, TargetCenterY - Height * 0.5f, Width, Height};
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

inline SHudMediaIslandLiquidPose QmHudMediaIslandLiquidPose(float Progress)
{
	constexpr float BulgeEnd = 90.0f / 560.0f;
	constexpr float FormEnd = 210.0f / 560.0f;
	constexpr float StretchEnd = 370.0f / 560.0f;
	constexpr float BreakEnd = 440.0f / 560.0f;
	constexpr float Pi = 3.14159265359f;
	Progress = std::clamp(Progress, 0.0f, 1.0f);

	SHudMediaIslandLiquidPose Pose;
	if(Progress <= 0.0f)
		return Pose;

	const auto Lerp = [](float From, float To, float Amount) {
		return From + (To - From) * Amount;
	};
	if(Progress < BulgeEnd)
	{
		const float T = QmHudMediaIslandLiquidSegment(Progress, 0.0f, BulgeEnd);
		Pose.m_RadiusScale = Lerp(0.0f, 0.30f, T);
		Pose.m_SmoothUnionScale = Lerp(0.0f, 0.90f, T);
		return Pose;
	}
	if(Progress < FormEnd)
	{
		Pose.m_Phase = EHudMediaIslandLiquidPhase::FORM;
		const float T = QmHudMediaIslandLiquidSegment(Progress, BulgeEnd, FormEnd);
		Pose.m_CenterProgress = Lerp(0.0f, 0.25f, T);
		Pose.m_RadiusScale = Lerp(0.30f, 1.0f, T);
		Pose.m_StretchX = Lerp(1.0f, 1.02f, T);
		Pose.m_StretchY = Lerp(1.0f, 0.98f, T);
		Pose.m_SmoothUnionScale = 0.90f;
		Pose.m_ContentAlpha = Lerp(0.0f, 0.25f, T);
		return Pose;
	}
	if(Progress < StretchEnd)
	{
		Pose.m_Phase = EHudMediaIslandLiquidPhase::STRETCH;
		const float T = QmHudMediaIslandLiquidSegment(Progress, FormEnd, StretchEnd);
		Pose.m_CenterProgress = Lerp(0.25f, 0.82f, T);
		Pose.m_RadiusScale = 1.0f;
		Pose.m_StretchX = Lerp(1.02f, 1.28f, T);
		Pose.m_StretchY = Lerp(0.98f, 0.84f, T);
		Pose.m_SmoothUnionScale = Lerp(0.90f, 0.35f, T);
		Pose.m_ContentAlpha = Lerp(0.25f, 0.80f, T);
		return Pose;
	}
	if(Progress < BreakEnd)
	{
		Pose.m_Phase = EHudMediaIslandLiquidPhase::BREAK;
		const float T = QmHudMediaIslandLiquidSegment(Progress, StretchEnd, BreakEnd);
		Pose.m_CenterProgress = Lerp(0.82f, 1.06f, T);
		Pose.m_RadiusScale = 1.0f;
		Pose.m_StretchX = Lerp(1.28f, 0.96f, T);
		Pose.m_StretchY = Lerp(0.84f, 1.0f, T);
		Pose.m_SmoothUnionScale = Lerp(0.35f, 0.0f, T);
		Pose.m_ContentAlpha = Lerp(0.80f, 1.0f, T);
		return Pose;
	}

	Pose.m_Phase = EHudMediaIslandLiquidPhase::REBOUND;
	const float T = QmHudMediaIslandLiquidSegment(Progress, BreakEnd, 1.0f);
	Pose.m_CenterProgress = 1.0f + 0.06f * (1.0f - T) * (1.0f - T) * std::cos(2.0f * Pi * T);
	Pose.m_RadiusScale = 1.0f;
	Pose.m_StretchX = Lerp(0.96f, 1.0f, T);
	Pose.m_StretchY = 1.0f;
	Pose.m_ContentAlpha = 1.0f;
	return Pose;
}

inline SHudMediaIslandLiquidCapsule QmHudMediaIslandRightLiquidCapsule(float MainRight, float CenterY, float Radius, float ContentWidth, float RestGap, const SHudMediaIslandLiquidPose &Pose)
{
	Radius = std::max(0.0f, Radius);
	const float Diameter = Radius * 2.0f;
	const float FinalWidth = std::max(Diameter, ContentWidth);
	const float SpawnCenterX = MainRight - Radius * 0.15f;
	const float FinalCenterX = MainRight + std::max(0.0f, RestGap) + FinalWidth * 0.5f;
	const auto Lerp = [](float From, float To, float Amount) {
		return From + (To - From) * std::clamp(Amount, 0.0f, 1.1f);
	};
	const float CenterX = Lerp(SpawnCenterX, FinalCenterX, Pose.m_CenterProgress);
	const float BaseWidth = Lerp(Diameter, FinalWidth, Pose.m_ContentAlpha);
	const float Width = std::max(0.0f, BaseWidth * Pose.m_RadiusScale * Pose.m_StretchX);
	const float Height = std::max(0.0f, Diameter * Pose.m_RadiusScale * Pose.m_StretchY);

	SHudMediaIslandLiquidCapsule Capsule;
	Capsule.m_Rect = {CenterX - Width * 0.5f, CenterY - Height * 0.5f, Width, Height};
	Capsule.m_Radius = std::min(Width, Height) * 0.5f;
	Capsule.m_SmoothUnion = Radius * Pose.m_SmoothUnionScale * 1.55f;
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
	constexpr float DurationSeconds = 0.56f;
	const float Delta = DeltaSeconds / DurationSeconds;
	return std::clamp(Current + (TargetVisible ? Delta : -Delta), 0.0f, 1.0f);
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
