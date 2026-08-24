#include "metal_frame_state.h"

#include <base/dbg.h>

CMetalFrameState::CMetalFrameState(size_t SlotCount) :
	m_SlotCount(SlotCount)
{
	dbg_assert(m_SlotCount > 0, "Metal frame state requires at least one slot");
	if(m_SlotCount == 0)
		m_SlotCount = 1;
}

bool CMetalFrameState::BeginFrame(size_t Slot)
{
	if(m_CurrentFrameId != 0 && !m_CurrentFrameFinalized && !m_CurrentFrameFailed)
		return false;

	m_CurrentSlot = Slot % m_SlotCount;
	m_CurrentFrameId = m_NextFrameId++;
	m_CurrentFrameFinalized = false;
	m_CurrentFrameFailed = false;
	return true;
}

CMetalFrameState::EFinalizeResult CMetalFrameState::FinalizeFrameForPresent(bool DrawableAvailable)
{
	if(m_CurrentFrameId == 0)
		return EFinalizeResult::FAILED;
	if(m_CurrentFrameFinalized)
		return EFinalizeResult::ALREADY_FINALIZED;
	if(m_CurrentFrameFailed || !DrawableAvailable)
	{
		m_CurrentFrameFailed = true;
		return EFinalizeResult::FAILED;
	}

	m_CurrentFrameFinalized = true;
	m_LastPresentedFrameId = m_CurrentFrameId;
	m_LastPresentedSlot = m_CurrentSlot;
	m_CaptureRetained = true;
	return EFinalizeResult::PRESENTED;
}

bool CMetalFrameState::ReadLastPresentedFrame(SFrameCapture &Capture) const
{
	if(!m_CaptureRetained || m_LastPresentedFrameId == 0)
		return false;

	Capture.m_FrameId = m_LastPresentedFrameId;
	Capture.m_Slot = m_LastPresentedSlot;
	return true;
}
