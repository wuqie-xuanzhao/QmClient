/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "emoticon.h"

#include "chat.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/QmUi/QmAnimResolve.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/QmUi/UiTokens.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cmath>

static uint64_t EmoticonPresentationNodeKey(const char *pScope)
{
	static const uint64_t s_BaseKey = static_cast<uint64_t>(str_quickhash("qm_extra_emoticon_presentation"));
	return BuildUiAnimNodeKey(s_BaseKey, static_cast<uint64_t>(str_quickhash(pScope)));
}

static SUiSpringConfig EmoticonPresentationSpring()
{
	SUiSpringConfig Spring;
	Spring.m_Stiffness = 470.0f;
	Spring.m_Damping = 40.0f;
	Spring.m_RestEpsilon = 0.006f;
	Spring.m_RestVelocity = 0.08f;
	return Spring;
}

static int EmoticonClockwiseOrderFromTop(int Index, int Count)
{
	const float Angle = (2.0f * pi * Index) / Count;
	const float ClockwiseFromTop = std::fmod(Angle + pi / 2.0f + 2.0f * pi, 2.0f * pi);
	return std::clamp(static_cast<int>(std::round(ClockwiseFromTop / (2.0f * pi) * Count)), 0, Count - 1);
}

static float EmoticonStaggerReveal(int Index, int Count, float PresentationAlpha)
{
	if(Count <= 1)
		return PresentationAlpha;

	constexpr float MaxDelay = 0.42f;
	const int Order = EmoticonClockwiseOrderFromTop(Index, Count);
	const float Delay = MaxDelay * Order / (Count - 1);
	const float Denominator = std::max(0.001f, 1.0f - Delay);
	const float LocalT = std::clamp((PresentationAlpha - Delay) / Denominator, 0.0f, 1.0f);
	const float Inv = 1.0f - LocalT;
	return 1.0f - Inv * Inv * Inv * Inv;
}

CEmoticon::CEmoticon()
{
	OnReset();
}

void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData)
{
	CEmoticon *pSelf = (CEmoticon *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(!pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(pSelf->GameClient()->m_BindWheel.IsActive())
			pSelf->m_Active = false;
		else
			pSelf->m_Active = pResult->GetInteger(0) != 0;
	}
}

void CEmoticon::ConEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->Emote(pResult->GetInteger(0));
}

void CEmoticon::ConLocalBlink(IConsole::IResult *, void *pUserData)
{
	((CEmoticon *)pUserData)->TriggerLocalBlink();
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
	Console()->Register("qm_blink", "", CFGFLAG_CLIENT, ConLocalBlink, this, "Blink the active local tee");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_PresentationInitialized = false;
	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	for(auto &LocalBlinkState : m_aLocalBlinkStates)
		LocalBlinkState.Reset();
	m_TouchPressedOutside = false;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CEmoticon::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}
	return false;
}

void CEmoticon::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static const auto PositiveMod = [](float x, float y) -> float {
		return std::fmod(x + y, y);
	};

	static const float s_InnerMouseLimitRadius = 40.0f;
	static const float s_InnerOuterMouseBoundaryRadius = 110.0f;
	static const float s_OuterMouseLimitRadius = 170.0f;
	static const float s_InnerItemRadius = 70.0f;
	static const float s_OuterItemRadius = 150.0f;
	static const float s_InnerCircleRadius = 100.0f;
	static const float s_OuterCircleRadius = 190.0f;

	if(!m_Active)
	{
		if(m_TouchPressedOutside)
		{
			m_SelectedEmote = -1;
			m_SelectedEyeEmote = -1;
			m_TouchPressedOutside = false;
		}

		if(m_WasActive && m_SelectedEmote != -1)
			Emote(m_SelectedEmote);
		if(m_WasActive && m_SelectedEyeEmote != -1)
			EyeEmote(m_SelectedEyeEmote);
		m_WasActive = false;
	}
	else
	{
		m_WasActive = true;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active || !GameClient()->m_Snap.m_pLocalCharacter)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	const CUIRect Screen = *Ui()->Screen();

	if(m_Active)
	{
		const bool WasTouchPressed = m_TouchState.m_AnyPressed;
		Ui()->UpdateTouchState(m_TouchState);
		if(m_TouchState.m_AnyPressed)
		{
			const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
			const float TouchCenterDistance = length(TouchPos);
			if(TouchCenterDistance <= s_OuterMouseLimitRadius)
			{
				m_SelectorMouse = TouchPos;
			}
			else if(TouchCenterDistance > s_OuterCircleRadius)
			{
				m_TouchPressedOutside = true;
			}
		}
		else if(WasTouchPressed)
		{
			m_Active = false;
		}
	}

	const bool ExtraAnimations = g_Config.m_QmExtraAnimations != 0 && GameClient()->UiRuntimeV2()->Enabled();
	float PresentationAlpha = m_Active ? 1.0f : 0.0f;
	float PresentationScale = m_Active ? 1.0f : 0.88f;
	if(ExtraAnimations)
	{
		CUiV2AnimationRuntime &AnimRuntime = GameClient()->UiRuntimeV2()->AnimRuntime();
		const SUiSpringConfig Spring = EmoticonPresentationSpring();
		const uint64_t PanelNode = EmoticonPresentationNodeKey("panel");
		if(!m_PresentationInitialized)
		{
			SetUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::ALPHA, 0.0f);
			SetUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::SCALE, 0.88f);
			m_PresentationInitialized = true;
		}
		PresentationAlpha = std::clamp(ResolveUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::ALPHA, m_Active ? 1.0f : 0.0f, Spring, 3, 0.004f), 0.0f, 1.0f);
		PresentationScale = std::max(0.01f, ResolveUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::SCALE, m_Active ? 1.0f : 0.88f, Spring, 3, 0.004f));
	}

	if(!m_Active && (!ExtraAnimations || PresentationAlpha <= 0.01f))
		return;

	if(m_Active)
	{
		if(length(m_SelectorMouse) > s_OuterMouseLimitRadius)
			m_SelectorMouse = normalize(m_SelectorMouse) * s_OuterMouseLimitRadius;

		const float SelectorAngle = angle(m_SelectorMouse);

		m_SelectedEmote = -1;
		m_SelectedEyeEmote = -1;
		if(length(m_SelectorMouse) > s_InnerOuterMouseBoundaryRadius)
			m_SelectedEmote = PositiveMod(std::round(SelectorAngle / (2.0f * pi) * NUM_EMOTICONS), NUM_EMOTICONS);
		else if(length(m_SelectorMouse) > s_InnerMouseLimitRadius)
			m_SelectedEyeEmote = PositiveMod(std::round(SelectorAngle / (2.0f * pi) * NUM_EMOTES), NUM_EMOTES);
	}

	const vec2 ScreenCenter = Screen.Center();

	Ui()->MapScreen();

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ui_token::color::SURFACE_OVERLAY.WithMultipliedAlpha(0.95f * PresentationAlpha));
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_OuterCircleRadius * PresentationScale, 64);
	Graphics()->SetColor(ui_token::color::ACCENT_PRIMARY_DIM.WithMultipliedAlpha(0.95f * PresentationAlpha));
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_InnerOuterMouseBoundaryRadius * PresentationScale, 64);
	Graphics()->QuadsEnd();

	Graphics()->WrapClamp();
	for(int Emote = 0; Emote < NUM_EMOTICONS; Emote++)
	{
		float Angle = 2.0f * pi * Emote / NUM_EMOTICONS;
		if(Angle > pi)
			Angle -= 2.0f * pi;

		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		Graphics()->QuadsBegin();
		const float Reveal = EmoticonStaggerReveal(Emote, NUM_EMOTICONS, PresentationAlpha);
		const float ItemAlpha = PresentationAlpha * Reveal;
		const float ItemScale = PresentationScale * (0.70f + 0.30f * Reveal);
		const vec2 Nudge = direction(Angle) * s_OuterItemRadius * ItemScale;
		const float HoverPhase = Emote == m_SelectedEmote ? 1.0f : 0.0f;
		const float Size = (50.0f + HoverPhase * 30.0f) * ItemScale;
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, ItemAlpha);
		IGraphics::CQuadItem QuadItem(ScreenCenter.x + Nudge.x, ScreenCenter.y + Nudge.y, Size, Size);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	Graphics()->WrapNormal();

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ui_token::color::SURFACE_HIGHLIGHT.WithMultipliedAlpha(2.0f * PresentationAlpha));
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_InnerCircleRadius * PresentationScale, 64);
		Graphics()->QuadsEnd();

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_RenderInfo;

		for(int Emote = 0; Emote < NUM_EMOTES; Emote++)
		{
			float Angle = 2.0f * pi * Emote / NUM_EMOTES;
			if(Angle > pi)
				Angle -= 2.0f * pi;

			const float Reveal = EmoticonStaggerReveal(Emote, NUM_EMOTES, PresentationAlpha);
			const float ItemAlpha = PresentationAlpha * Reveal;
			const float ItemScale = PresentationScale * (0.76f + 0.24f * Reveal);
			const vec2 Nudge = direction(Angle) * s_InnerItemRadius * ItemScale;
			const float HoverPhase = Emote == m_SelectedEyeEmote ? 1.0f : 0.0f;
			TeeInfo.m_Size = (48.0f + HoverPhase * 18.0f) * ItemScale;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1.0f, 0.0f), ScreenCenter + Nudge, ItemAlpha);
		}

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ui_token::color::SURFACE_ELEVATED.WithMultipliedAlpha(PresentationAlpha));
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 30.0f * PresentationScale, 64);
		Graphics()->QuadsEnd();
	}
	else
	{
		m_SelectedEyeEmote = -1;
	}

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse * PresentationScale, 24.0f * PresentationScale, PresentationAlpha);
}

void CEmoticon::Emote(int Emoticon)
{
	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgDummy(NETMSGTYPE_CL_EMOTICON, false);
		MsgDummy.AddInt(Emoticon);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgDummy, MSGFLAG_VITAL);
	}
}

void CEmoticon::EyeEmote(int Emote)
{
	char aBuf[32];
	switch(Emote)
	{
	case EMOTE_NORMAL:
		str_format(aBuf, sizeof(aBuf), "/emote normal %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_PAIN:
		str_format(aBuf, sizeof(aBuf), "/emote pain %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_HAPPY:
		str_format(aBuf, sizeof(aBuf), "/emote happy %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_SURPRISE:
		str_format(aBuf, sizeof(aBuf), "/emote surprise %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_ANGRY:
		str_format(aBuf, sizeof(aBuf), "/emote angry %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_BLINK:
		str_format(aBuf, sizeof(aBuf), "/emote blink %d", g_Config.m_ClEyeDuration);
		break;
	}
	GameClient()->m_Chat.SendChat(0, aBuf);
}

void CEmoticon::TriggerLocalBlink()
{
	const int Dummy = g_Config.m_ClDummy;
	const int ClientId = GameClient()->m_aLocalIds[Dummy];
	if(Client()->State() != IClient::STATE_ONLINE || ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return;

	m_aLocalBlinkStates[Dummy].Trigger(Client()->GameTick(Dummy));
}

bool CEmoticon::ShouldRenderLocalBlink(int ClientId) const
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)
	{
		if(GameClient()->m_aLocalIds[Dummy] == ClientId && m_aLocalBlinkStates[Dummy].IsActive(Client()->GameTick(Dummy)))
			return true;
	}
	return false;
}
