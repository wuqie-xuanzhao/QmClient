#include "qm_netease_hook_protocol.h"

#include <cstring>
#include <mutex>

namespace QmNeteaseHook
{
	namespace
	{
		uint32_t CrcTable[256];
		std::once_flag CrcTableOnce;

		void InitCrcTable()
		{
			std::call_once(CrcTableOnce, [] {
				for(uint32_t i = 0; i < 256; ++i)
				{
					uint32_t Value = i;
					for(int Bit = 0; Bit < 8; ++Bit)
						Value = (Value & 1) != 0 ? (Value >> 1) ^ 0xEDB88320U : Value >> 1;
					CrcTable[i] = Value;
				}
			});
		}
	}

	uint32_t Crc32(const void *pData, size_t Size)
	{
		InitCrcTable();
		const auto *pBytes = static_cast<const uint8_t *>(pData);
		uint32_t Result = 0xFFFFFFFFU;
		for(size_t i = 0; i < Size; ++i)
			Result = CrcTable[(Result ^ pBytes[i]) & 0xFFU] ^ (Result >> 8);
		return Result ^ 0xFFFFFFFFU;
	}

	uint32_t CalculateChecksum(const SSnapshot &Snapshot)
	{
		SSnapshot Copy = Snapshot;
		Copy.m_Checksum = 0;
		return Crc32(&Copy, sizeof(Copy));
	}

	void FinalizeSnapshot(SSnapshot *pSnapshot)
	{
		if(pSnapshot == nullptr)
			return;
		pSnapshot->m_Magic = PROTOCOL_MAGIC;
		pSnapshot->m_SchemaVersion = PROTOCOL_SCHEMA_VERSION;
		pSnapshot->m_SnapshotSize = sizeof(SSnapshot);
		pSnapshot->m_Checksum = CalculateChecksum(*pSnapshot);
	}

	bool ValidateSnapshot(const SSnapshot &Snapshot)
	{
		if(Snapshot.m_Magic != PROTOCOL_MAGIC || Snapshot.m_SchemaVersion != PROTOCOL_SCHEMA_VERSION || Snapshot.m_SnapshotSize != sizeof(SSnapshot))
			return false;
		if(Snapshot.m_PlayerBuild != TARGET_BUILD || Snapshot.m_PlayerPatch != TARGET_PATCH)
			return false;
		if(Snapshot.m_Sequence == 0 || (Snapshot.m_Sequence & 1) != 0)
			return false;
		if(Snapshot.m_PlaybackRate < 0.0 || Snapshot.m_PlaybackRate > 8.0)
			return false;
		if((Snapshot.m_Status & STATUS_HAS_COVER) != 0 && !HasCover(Snapshot))
			return false;
		if(Snapshot.m_DurationMs < 0 || Snapshot.m_PositionMs < 0 || (Snapshot.m_DurationMs > 0 && Snapshot.m_PositionMs > Snapshot.m_DurationMs + 1000))
			return false;
		if(HasCurrentLine(Snapshot))
		{
			const bool UnknownBoundaries = Snapshot.m_CurrentLineStartMs == -1 && Snapshot.m_CurrentLineEndMs == -1;
			if(!UnknownBoundaries && (Snapshot.m_CurrentLineStartMs < 0 || Snapshot.m_CurrentLineEndMs <= Snapshot.m_CurrentLineStartMs ||
							 (Snapshot.m_DurationMs > 0 && Snapshot.m_CurrentLineStartMs > Snapshot.m_DurationMs + 1000) ||
							 (Snapshot.m_DurationMs > 0 && Snapshot.m_CurrentLineEndMs > Snapshot.m_DurationMs + 1000)))
				return false;
		}
		return Snapshot.m_Checksum == CalculateChecksum(Snapshot);
	}

	bool HasMedia(const SSnapshot &Snapshot)
	{
		return (Snapshot.m_Status & STATUS_HAS_MEDIA) != 0;
	}

	bool HasCurrentLine(const SSnapshot &Snapshot)
	{
		return (Snapshot.m_Status & STATUS_HAS_CURRENT_LINE) != 0 && Snapshot.m_aCurrentLine[0] != '\0';
	}

	bool HasCover(const SSnapshot &Snapshot)
	{
		return Snapshot.m_aCoverPath[0] != '\0' || Snapshot.m_aCoverUrl[0] != '\0';
	}

	bool IsStableSequence(uint64_t BeginSequence, uint64_t EndSequence)
	{
		return BeginSequence != 0 && (BeginSequence & 1) == 0 && BeginSequence == EndSequence;
	}

} // namespace QmNeteaseHook
