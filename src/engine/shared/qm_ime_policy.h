#ifndef ENGINE_SHARED_QM_IME_POLICY_H
#define ENGINE_SHARED_QM_IME_POLICY_H

#include <cstddef>
#include <cstdint>
#include <optional>

inline bool QmImeShouldUseSystemCandidateUi()
{
#if defined(CONF_FAMILY_WINDOWS)
	return false;
#else
	return true;
#endif
}

inline bool QmImeShouldRenderCustomCandidateUi()
{
	if(QmImeShouldUseSystemCandidateUi())
		return false;
	return true;
}

inline bool QmImeNotifyFlagsIncludeCandidateList(unsigned CandidateListFlags, unsigned CandidateListIndex)
{
	if(CandidateListIndex >= 32)
		return false;
	if(CandidateListFlags == 0)
		return CandidateListIndex == 0;
	return (CandidateListFlags & (1u << CandidateListIndex)) != 0;
}

inline unsigned QmImeCandidatePageSizeOrCount(unsigned PageSize, unsigned CandidateCount)
{
	return PageSize > 0 ? PageSize : CandidateCount;
}

inline size_t QmImeCandidateOffsetCapacity(size_t BufferSize, size_t OffsetTableStart)
{
	if(BufferSize < OffsetTableStart)
		return 0;
	return (BufferSize - OffsetTableStart) / sizeof(uint32_t);
}

inline std::optional<size_t> QmImeBoundedUtf16Length(const unsigned char *pBuffer, size_t BufferSize, size_t Offset)
{
	constexpr size_t CodeUnitSize = sizeof(uint16_t);
	if(pBuffer == nullptr || Offset % CodeUnitSize != 0 || Offset > BufferSize || BufferSize - Offset < CodeUnitSize)
		return {};

	for(size_t Position = Offset; BufferSize - Position >= CodeUnitSize; Position += CodeUnitSize)
	{
		if(pBuffer[Position] == 0 && pBuffer[Position + 1] == 0)
			return (Position - Offset) / CodeUnitSize;
	}
	return {};
}

#endif
