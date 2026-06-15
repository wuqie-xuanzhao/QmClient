/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmRt.h"

#include "../gameclient.h"

#include <base/perf_timer.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/shared/config.h>

#include <game/client/components/qmclient/perf_logging.h>

#include <algorithm>

namespace
{
	void LogPerfStage(IClient *pClient, const char *pPage, const char *pStage, const double DurationMs, const bool Force = false, const char *pExtra = nullptr)
	{
		QmPerfLogStage("perf/ui_runtime", pStage, DurationMs, Force, pClient, pPage, nullptr, pExtra);
	}
}

void CUiRuntimeV2::Init(CGameClient *pGameClient)
{
	m_pGameClient = pGameClient;
	Reset();
}

void CUiRuntimeV2::Reset()
{
	m_Tree.Reset();
	m_AnimRuntime.Reset();
	m_RenderBridge.BeginFrame();
	m_LastStats = {};
	m_DebugLogAccumulator = 0.0f;
	ClearPerfContext();
}

bool CUiRuntimeV2::Enabled() const
{
	return m_pGameClient != nullptr;
}

void CUiRuntimeV2::SetPerfContext(const char *pPage, const char *pOperation)
{
	str_copy(m_aPerfPage, pPage != nullptr ? pPage : "", sizeof(m_aPerfPage));
	str_copy(m_aPerfOperation, pOperation != nullptr ? pOperation : "", sizeof(m_aPerfOperation));
}

void CUiRuntimeV2::ClearPerfContext()
{
	m_aPerfPage[0] = '\0';
	m_aPerfOperation[0] = '\0';
}

void CUiRuntimeV2::OnRender()
{
	if(!Enabled())
		return;

	CPerfTimer RenderTimer;
	float Dt = m_pGameClient->Client()->RenderFrameTime();
	if(Dt < 0.0f)
		Dt = 0.0f;
	Dt = std::min(Dt, 1.0f / 15.0f);

	float TreeBeginMs = 0.0f;
	float AnimAdvanceMs = 0.0f;
	float RenderBridgeBeginMs = 0.0f;
	float TreeEndMs = 0.0f;

	{
		CPerfTimer StageTimer;
		m_Tree.BeginFrame();
		TreeBeginMs = StageTimer.ElapsedMs();
		LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr, "tree_begin_frame", TreeBeginMs);
	}
	{
		CPerfTimer StageTimer;
		m_AnimRuntime.Advance(Dt);
		AnimAdvanceMs = StageTimer.ElapsedMs();
		LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr, "anim_advance", AnimAdvanceMs);
	}
	{
		CPerfTimer StageTimer;
		m_RenderBridge.BeginFrame();
		RenderBridgeBeginMs = StageTimer.ElapsedMs();
		LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr, "render_bridge_begin_frame", RenderBridgeBeginMs);
	}
	{
		CPerfTimer StageTimer;
		m_Tree.EndFrame();
		TreeEndMs = StageTimer.ElapsedMs();
		char aExtra[96];
		str_format(aExtra, sizeof(aExtra), "dt_ms=%.3f", Dt * 1000.0f);
		LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr, "tree_end_frame", TreeEndMs, false, aExtra);
	}

	m_LastStats.m_BuildTreeMs = TreeBeginMs + TreeEndMs;
	m_LastStats.m_AnimMs = AnimAdvanceMs;
	m_LastStats.m_RenderBridgeMs = RenderBridgeBeginMs;
	m_LastStats.m_NodeCount = m_Tree.NodeCount();
	m_LastStats.m_ActiveAnimCount = m_AnimRuntime.ActiveTrackCount();
	m_LastStats.m_QueuedAnimCount = m_AnimRuntime.QueuedTrackCount();

	if(g_Config.m_QmUiRuntimeV2Debug)
	{
		m_DebugLogAccumulator += Dt;
		if(m_DebugLogAccumulator >= 2.0f && m_LastStats.m_AnimMs >= QmPerfThresholdMs())
		{
			m_DebugLogAccumulator = 0.0f;
			dbg_msg("qm_ui", "runtime active: nodes=%d, anim_ms=%.3f", m_LastStats.m_NodeCount, m_LastStats.m_AnimMs);
		}
	}

	char aExtra[96];
	str_format(aExtra, sizeof(aExtra), "nodes=%d", m_LastStats.m_NodeCount);
	LogPerfStage(m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr, "ui_runtime_total", RenderTimer.ElapsedMs(), false, aExtra);

	char aPayload[256];
	str_format(aPayload, sizeof(aPayload),
		"event=ui_runtime operation=%s nodes=%d anim_ms=%.3f active_anims=%d queued_anims=%d render_bridge_ms=%.3f duration_ms=%.3f",
		m_aPerfOperation[0] != '\0' ? m_aPerfOperation : "unknown",
		m_LastStats.m_NodeCount,
		m_LastStats.m_AnimMs,
		m_LastStats.m_ActiveAnimCount,
		m_LastStats.m_QueuedAnimCount,
		m_LastStats.m_RenderBridgeMs,
		RenderTimer.ElapsedMs());
	QmPerfLogPayload("perf/ui_runtime", aPayload, m_pGameClient->Client(), m_aPerfPage[0] != '\0' ? m_aPerfPage : nullptr);
}

const SUiV2PerfStats &CUiRuntimeV2::LastStats() const
{
	return m_LastStats;
}
