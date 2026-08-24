#ifndef ENGINE_CLIENT_BACKEND_METAL_METAL_FRAME_STATE_H
#define ENGINE_CLIENT_BACKEND_METAL_METAL_FRAME_STATE_H

#include <cstddef>
#include <cstdint>

class CMetalFrameState
{
public:
	struct SFrameCapture
	{
		uint64_t m_FrameId = 0;
		size_t m_Slot = 0;
	};

	enum class EFinalizeResult
	{
		PRESENTED,
		ALREADY_FINALIZED,
		FAILED,
	};

	explicit CMetalFrameState(size_t SlotCount = 3);

	bool BeginFrame(size_t Slot);
	EFinalizeResult FinalizeFrameForPresent(bool DrawableAvailable);
	bool ReadLastPresentedFrame(SFrameCapture &Capture) const;

	uint64_t CurrentFrameId() const { return m_CurrentFrameId; }
	uint64_t LastPresentedFrameId() const { return m_LastPresentedFrameId; }
	bool CurrentFrameFailed() const { return m_CurrentFrameFailed; }
	bool CurrentFrameFinalized() const { return m_CurrentFrameFinalized; }
	bool CaptureRetained() const { return m_CaptureRetained; }

private:
	size_t m_SlotCount;
	size_t m_CurrentSlot = 0;
	uint64_t m_NextFrameId = 1;
	uint64_t m_CurrentFrameId = 0;
	uint64_t m_LastPresentedFrameId = 0;
	size_t m_LastPresentedSlot = 0;
	bool m_CurrentFrameFinalized = false;
	bool m_CurrentFrameFailed = false;
	bool m_CaptureRetained = false;
};

#endif
