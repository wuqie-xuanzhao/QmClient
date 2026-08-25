#include "backend_metal.h"
#include "metal_frame_state.h"
#include "metal_render_target_state.h"
#include "metal_types.h"

#include <base/log.h>
#include <base/str.h>

#include <engine/client/backend_sdl.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <SDL_metal.h>

#define pi qmclient_carbon_pi
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#undef pi
#include <dispatch/dispatch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr size_t gs_MetalGpuInfoStringSize = 256;
enum class EMetalBackendState
{
	UNINITIALIZED,
	PRE_INITIALIZED,
	INITIALIZED,
	SHUTDOWN,
};

const char *MetalBackendStateName(EMetalBackendState State)
{
	switch(State)
	{
	case EMetalBackendState::UNINITIALIZED: return "uninitialized";
	case EMetalBackendState::PRE_INITIALIZED: return "pre_initialized";
	case EMetalBackendState::INITIALIZED: return "initialized";
	case EMetalBackendState::SHUTDOWN: return "shutdown";
	}
	return "unknown";
}

class CCommandProcessorFragment_Metal final : public CCommandProcessorFragment_GLBase
{
	static constexpr size_t gs_FrameSlotCount = 3;
	static constexpr size_t gs_StreamBufferSize = 4 * 1024 * 1024;

	struct STextureSlot
	{
		id<MTLTexture> m_Texture = nil;
		id<MTLTexture> m_TextureArray = nil;
		id<MTLBuffer> m_Staging = nil;
		SMetalTextureLayout m_Layout;
		SMetalTextureLayout m_ArrayLayout;
		size_t m_Width = 0;
		size_t m_Height = 0;
		size_t m_ArrayWidth = 0;
		size_t m_ArrayHeight = 0;
		EMetalTextureFormat m_Format = EMetalTextureFormat::RGBA8;
		bool m_Is2DArray = false;
		bool m_Allocated = false;
	};

	struct SBufferSlot
	{
		id<MTLBuffer> m_Buffer = nil;
		size_t m_DataBytes = 0;
		bool m_Allocated = false;
		bool m_OneTimeUse = false;
	};

	struct SBufferContainerSlot
	{
		int m_Stride = 0;
		int m_VertBufferBindingIndex = -1;
		std::vector<SBufferContainerInfo::SAttribute> m_vAttributes;
		bool m_Allocated = false;
	};

	struct SRenderTarget
	{
		id<MTLTexture> m_Texture = nil;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		size_t m_DataBytes = 0;
		bool m_Allocated = false;
	};

	struct SFrameSlot
	{
		id<MTLBuffer> m_VertexBuffer = nil;
		id<MTLCommandBuffer> m_CommandBuffer = nil;
		size_t m_VertexOffset = 0;
		uint64_t m_FrameId = 0;
	};

	EMetalBackendState m_State = EMetalBackendState::UNINITIALIZED;
	uint64_t m_FrameId = 0;
	SDL_MetalView m_MetalView = nullptr;
	void *m_pLayer = nullptr;
	SDL_Window *m_pWindow = nullptr;
	bool m_VSync = true;
	id<MTLDevice> m_Device = nil;
	id<MTLCommandQueue> m_CommandQueue = nil;
	id<MTLBuffer> m_QuadIndexBuffer = nil;
	id<MTLLibrary> m_ShaderLibrary = nil;
	std::array<id<MTLRenderPipelineState>, 60> m_aPipelineStates{};
	id<MTLSamplerState> m_RepeatSampler = nil;
	id<MTLSamplerState> m_ClampSampler = nil;
	std::array<SFrameSlot, gs_FrameSlotCount> m_aFrameSlots{};
	CMetalFrameState m_FrameState{gs_FrameSlotCount};
	size_t m_CurrentFrameSlot = 0;
	id<MTLCommandBuffer> m_CurrentCommandBuffer = nil;
	id<MTLRenderCommandEncoder> m_CurrentRenderEncoder = nil;
	id<MTLBlitCommandEncoder> m_CurrentBlitEncoder = nil;
	id<CAMetalDrawable> m_CurrentDrawable = nil;
	id<MTLBuffer> m_LastPresentedReadback = nil;
	id<MTLCommandBuffer> m_LastPresentedCommandBuffer = nil;
	uint32_t m_LastPresentedReadbackWidth = 0;
	uint32_t m_LastPresentedReadbackHeight = 0;
	size_t m_LastPresentedReadbackRowBytes = 0;
	std::atomic_bool m_GpuFailure = false;
	std::atomic<uint64_t> m_GpuFailureFrameId = 0;
	std::atomic<int> m_GpuFailureCommandId = -1;
	std::atomic<int> m_GpuFailureStatus = 0;
	std::atomic<int> m_LastCommandId = -1;
	std::mutex m_GpuFailureMutex;
	std::string m_GpuFailureDescription;
	SBackendCapabilities *m_pCapabilities = nullptr;
	IStorage *m_pStorage = nullptr;
	bool m_CommandBufferCommitted = false;
	bool m_RenderEncoderStarted = false;
	bool m_BackbufferHasContents = false;
	CMetalRenderTargetState m_RenderTargetState;
	uint32_t m_DrawableWidth = 0;
	uint32_t m_DrawableHeight = 0;
	size_t m_StreamMemoryBytes = 0;
	size_t m_BufferMemoryBytes = 0;
	std::vector<STextureSlot> m_vTextureSlots;
	std::vector<SBufferSlot> m_vBufferSlots;
	std::vector<SBufferContainerSlot> m_vBufferContainers;
	std::vector<SRenderTarget> m_vRenderTargets;
	unsigned int m_RequiredIndicesNum = 0;
	std::atomic<uint64_t> *m_pTextureMemoryUsage = nullptr;
	std::atomic<uint64_t> *m_pBufferMemoryUsage = nullptr;
	std::atomic<uint64_t> *m_pStreamMemoryUsage = nullptr;
	std::atomic<uint64_t> *m_pStagingMemoryUsage = nullptr;
	char *m_pVendorString = nullptr;
	char *m_pVersionString = nullptr;
	char *m_pRendererString = nullptr;
	TTwGraphicsGpuList *m_pGpuList = nullptr;

	void SetDevice(id<MTLDevice> Device)
	{
#if !__has_feature(objc_arc)
		[m_Device release];
		m_Device = [Device retain];
#else
		m_Device = Device;
#endif
	}

	void ReleaseDevice()
	{
#if !__has_feature(objc_arc)
		[m_Device release];
#endif
		m_Device = nil;
	}

	static void ReleaseMetalObject(id Object)
	{
#if !__has_feature(objc_arc)
		[Object release];
#else
		(void)Object;
#endif
	}

	static id RetainMetalObject(id Object)
	{
#if !__has_feature(objc_arc)
		return [Object retain];
#else
		return Object;
#endif
	}

	void ReleaseCommandQueue()
	{
		ReleaseMetalObject(m_CommandQueue);
		m_CommandQueue = nil;
	}

	static size_t PipelineIndex(bool Textured, bool Text, EMetalBlendMode BlendMode)
	{
		return (Text ? 9 : (Textured ? 3 : 0)) + static_cast<size_t>(BlendMode);
	}

	static size_t TilePipelineIndex(bool Textured, bool Border, EMetalBlendMode BlendMode)
	{
		return 18 + (static_cast<size_t>(Border) * 2 + static_cast<size_t>(Textured)) * 3 + static_cast<size_t>(BlendMode);
	}

	static size_t QuadPipelineIndex(bool Textured, bool Grouped, EMetalBlendMode BlendMode)
	{
		return 30 + (static_cast<size_t>(Grouped) * 2 + static_cast<size_t>(Textured)) * 3 + static_cast<size_t>(BlendMode);
	}

	static size_t QuadContainerExPipelineIndex(bool Textured, EMetalBlendMode BlendMode)
	{
		return 42 + static_cast<size_t>(Textured) * 3 + static_cast<size_t>(BlendMode);
	}

	static size_t SpriteMultiplePipelineIndex(bool Textured, EMetalBlendMode BlendMode)
	{
		return 48 + static_cast<size_t>(Textured) * 3 + static_cast<size_t>(BlendMode);
	}

	static size_t TextureArrayPipelineIndex(EMetalBlendMode BlendMode)
	{
		return 54 + static_cast<size_t>(BlendMode);
	}

	static MTLBlendFactor MetalBlendFactor(EMetalBlendFactor Factor)
	{
		switch(Factor)
		{
		case EMetalBlendFactor::ZERO: return MTLBlendFactorZero;
		case EMetalBlendFactor::ONE: return MTLBlendFactorOne;
		case EMetalBlendFactor::SOURCE_ALPHA: return MTLBlendFactorSourceAlpha;
		case EMetalBlendFactor::ONE_MINUS_SOURCE_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
		}
		return MTLBlendFactorOne;
	}

	void ReleaseFrameSlotResources(size_t Slot)
	{
		if(Slot >= m_aFrameSlots.size())
			return;
		SFrameSlot &Frame = m_aFrameSlots[Slot];
		const CMetalFrameState::ESlotState State = m_FrameState.SlotState(Slot);
		if(State == CMetalFrameState::ESlotState::IN_FLIGHT)
			return;
		ReleaseMetalObject(Frame.m_CommandBuffer);
		ReleaseMetalObject(Frame.m_VertexBuffer);
		Frame = {};
	}

	void ReleaseGpuObjects()
	{
		DestroyAllRenderTargets();
		m_CurrentRenderEncoder = nil;
		m_CurrentBlitEncoder = nil;
		m_CurrentDrawable = nil;
		m_BackbufferHasContents = false;
		m_RenderTargetState.Reset();
		ReleaseMetalObject(m_LastPresentedReadback);
		m_LastPresentedReadback = nil;
		ReleaseMetalObject(m_LastPresentedCommandBuffer);
		m_LastPresentedCommandBuffer = nil;
		m_LastPresentedReadbackWidth = 0;
		m_LastPresentedReadbackHeight = 0;
		m_LastPresentedReadbackRowBytes = 0;
		m_CurrentCommandBuffer = nil;
		for(id<MTLRenderPipelineState> Pipeline : m_aPipelineStates)
			ReleaseMetalObject(Pipeline);
		m_aPipelineStates.fill(nil);
		ReleaseMetalObject(m_RepeatSampler);
		ReleaseMetalObject(m_ClampSampler);
		ReleaseMetalObject(m_ShaderLibrary);
		ReleaseMetalObject(m_QuadIndexBuffer);
		m_QuadIndexBuffer = nil;
		m_RepeatSampler = nil;
		m_ClampSampler = nil;
		m_ShaderLibrary = nil;
		for(size_t Slot = 0; Slot < m_aFrameSlots.size(); ++Slot)
		{
			ReleaseMetalObject(m_aFrameSlots[Slot].m_CommandBuffer);
			ReleaseMetalObject(m_aFrameSlots[Slot].m_VertexBuffer);
			m_aFrameSlots[Slot] = {};
		}
		if(m_StreamMemoryBytes != 0)
		{
			SubStreamMemory(m_StreamMemoryBytes);
			m_StreamMemoryBytes = 0;
		}
		if(m_BufferMemoryBytes != 0)
		{
			SubBufferMemory(m_BufferMemoryBytes);
			m_BufferMemoryBytes = 0;
		}
		ReleaseCommandQueue();
	}

	void WaitForGpuIdle()
	{
		if(m_CurrentCommandBuffer != nil && !m_CommandBufferCommitted)
			CommitCurrentFrame(false, true);
		for(SFrameSlot &Frame : m_aFrameSlots)
		{
			if(Frame.m_CommandBuffer != nil)
				[Frame.m_CommandBuffer waitUntilCompleted];
		}
		m_FrameState.DrainFrames();
	}

	void RecordGpuFailure(id<MTLCommandBuffer> Buffer, uint64_t FrameId)
	{
		std::string Description = "Metal command buffer failed";
		NSError *pError = Buffer.error;
		if(pError != nil)
		{
			const char *pDomain = pError.domain.UTF8String;
			const char *pLocalizedDescription = pError.localizedDescription.UTF8String;
			Description += " (domain=";
			Description += pDomain != nullptr ? pDomain : "unknown";
			Description += ", code=" + std::to_string(static_cast<long long>(pError.code));
			if(pLocalizedDescription != nullptr)
			{
				Description += ", description=";
				Description += pLocalizedDescription;
			}
			Description += ")";
		}
		{
			std::lock_guard<std::mutex> Lock(m_GpuFailureMutex);
			m_GpuFailureDescription = std::move(Description);
		}
		m_GpuFailureFrameId.store(FrameId, std::memory_order_relaxed);
		m_GpuFailureCommandId.store(m_LastCommandId.load(std::memory_order_relaxed), std::memory_order_relaxed);
		m_GpuFailureStatus.store(static_cast<int>(Buffer.status), std::memory_order_relaxed);
		m_GpuFailure.store(true, std::memory_order_release);
	}

	bool SetGpuFailureError()
	{
		if(!m_GpuFailure.load(std::memory_order_acquire))
			return false;
		const uint64_t FrameId = m_GpuFailureFrameId.load(std::memory_order_relaxed);
		const int CommandId = m_GpuFailureCommandId.load(std::memory_order_relaxed);
		const int Status = m_GpuFailureStatus.load(std::memory_order_relaxed);
		std::string Description;
		{
			std::lock_guard<std::mutex> Lock(m_GpuFailureMutex);
			Description = m_GpuFailureDescription;
		}
		m_Error.m_ErrorType = GFX_ERROR_TYPE_RENDER_SUBMIT_FAILED;
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{
			false,
			"Metal GPU command buffer failure (stage=completion, frame_id=" + std::to_string(FrameId) + ", command_id=" + std::to_string(CommandId) + ", status=" + std::to_string(Status) + "): " + Description});
		return true;
	}

	void AddTextureMemory(size_t Bytes)
	{
		if(m_pTextureMemoryUsage != nullptr)
			m_pTextureMemoryUsage->fetch_add(Bytes, std::memory_order_relaxed);
	}

	void SubTextureMemory(size_t Bytes)
	{
		if(m_pTextureMemoryUsage != nullptr)
			m_pTextureMemoryUsage->fetch_sub(Bytes, std::memory_order_relaxed);
	}

	void AddBufferMemory(size_t Bytes)
	{
		if(m_pBufferMemoryUsage != nullptr)
			m_pBufferMemoryUsage->fetch_add(Bytes, std::memory_order_relaxed);
	}

	void SubBufferMemory(size_t Bytes)
	{
		if(m_pBufferMemoryUsage != nullptr)
			m_pBufferMemoryUsage->fetch_sub(Bytes, std::memory_order_relaxed);
	}

	bool EnsureBufferSlot(int Slot)
	{
		if(Slot < 0)
			return false;
		if(static_cast<size_t>(Slot) >= m_vBufferSlots.size())
			m_vBufferSlots.resize(static_cast<size_t>(Slot) + 1);
		return true;
	}

	void DestroyBuffer(int Slot)
	{
		if(Slot < 0 || static_cast<size_t>(Slot) >= m_vBufferSlots.size())
			return;
		SBufferSlot &Buffer = m_vBufferSlots[Slot];
		if(!Buffer.m_Allocated)
			return;
		SubBufferMemory(Buffer.m_DataBytes);
		ReleaseMetalObject(Buffer.m_Buffer);
		Buffer = {};
	}

	void DestroyAllBuffers()
	{
		for(size_t Slot = 0; Slot < m_vBufferSlots.size(); ++Slot)
			DestroyBuffer(static_cast<int>(Slot));
		m_vBufferSlots.clear();
	}

	bool EnsureBufferContainerSlot(int Slot)
	{
		if(Slot < 0)
			return false;
		if(static_cast<size_t>(Slot) >= m_vBufferContainers.size())
			m_vBufferContainers.resize(static_cast<size_t>(Slot) + 1);
		return true;
	}

	void DestroyBufferContainer(int Slot, bool DestroyAllBO)
	{
		if(Slot < 0 || static_cast<size_t>(Slot) >= m_vBufferContainers.size())
			return;
		SBufferContainerSlot &Container = m_vBufferContainers[Slot];
		if(!Container.m_Allocated)
			return;
		if(DestroyAllBO && Container.m_VertBufferBindingIndex >= 0)
			DestroyBuffer(Container.m_VertBufferBindingIndex);
		Container = {};
	}

	void DestroyAllBufferContainers()
	{
		for(size_t Slot = 0; Slot < m_vBufferContainers.size(); ++Slot)
			DestroyBufferContainer(static_cast<int>(Slot), false);
		m_vBufferContainers.clear();
		m_RequiredIndicesNum = 0;
	}

	bool UpdateBufferContainer(int Slot, int Stride, int VertBufferBindingIndex, size_t AttributeCount, const SBufferContainerInfo::SAttribute *pAttributes, bool ReplaceExisting)
	{
		if(!EnsureBufferContainerSlot(Slot) || (AttributeCount > 0 && pAttributes == nullptr))
			return false;
		if(Stride < 0)
			return false;
		if(Stride == 0)
		{
			size_t TightStride = 0;
			for(size_t Index = 0; Index < AttributeCount; ++Index)
			{
				const SBufferContainerInfo::SAttribute &Attribute = pAttributes[Index];
				const size_t TypeBytes = MetalVertexDataTypeBytes(Attribute.m_Type);
				const size_t DataTypeCount = Attribute.m_DataTypeCount > 0 ? static_cast<size_t>(Attribute.m_DataTypeCount) : 0;
				if(DataTypeCount == 0 || TypeBytes == 0 || DataTypeCount > std::numeric_limits<size_t>::max() / TypeBytes)
					return false;
				const size_t AttributeBytes = DataTypeCount * TypeBytes;
				const size_t AttributeOffset = reinterpret_cast<uintptr_t>(Attribute.m_pOffset);
				if(AttributeOffset > std::numeric_limits<size_t>::max() - AttributeBytes)
					return false;
				TightStride = std::max(TightStride, AttributeOffset + AttributeBytes);
			}
			if(TightStride == 0 || TightStride > static_cast<size_t>(std::numeric_limits<int>::max()))
				return false;
			Stride = static_cast<int>(TightStride);
		}
		SBufferContainerSlot &Container = m_vBufferContainers[Slot];
		for(size_t Index = 0; Index < AttributeCount; ++Index)
		{
			const SBufferContainerInfo::SAttribute &Attribute = pAttributes[Index];
			if(Attribute.m_DataTypeCount <= 0)
				return false;
			const SMetalVertexAttribute MetalAttribute{static_cast<uint32_t>(Attribute.m_DataTypeCount), Attribute.m_Type, Attribute.m_Normalized, reinterpret_cast<uintptr_t>(Attribute.m_pOffset), Attribute.m_FuncType};
			if(!MetalValidateVertexAttribute(Stride, MetalAttribute))
				return false;
		}
		if(ReplaceExisting && Container.m_Allocated)
			DestroyBufferContainer(Slot, true);
		Container.m_Stride = Stride;
		Container.m_VertBufferBindingIndex = VertBufferBindingIndex;
		Container.m_vAttributes.clear();
		if(AttributeCount > 0)
			Container.m_vAttributes.assign(pAttributes, pAttributes + AttributeCount);
		Container.m_Allocated = true;
		return true;
	}

	void EndRenderEncoderForBlit()
	{
		if(m_CurrentRenderEncoder == nil)
			return;
		[m_CurrentRenderEncoder endEncoding];
		m_CurrentRenderEncoder = nil;
		m_RenderEncoderStarted = false;
	}

	void DestroyRenderTarget(int TargetId)
	{
		if(TargetId < 0 || static_cast<size_t>(TargetId) >= m_vRenderTargets.size())
			return;
		if(!m_RenderTargetState.CanDestroy(TargetId))
			return;
		SRenderTarget &Target = m_vRenderTargets[TargetId];
		if(Target.m_Allocated)
		{
			SubTextureMemory(Target.m_DataBytes);
			ReleaseMetalObject(Target.m_Texture);
		}
		Target = {};
	}

	bool CreateRenderTarget(int TargetId, int Width, int Height)
	{
		if(TargetId < 0 || Width <= 0 || Height <= 0 || m_Device == nil || m_State != EMetalBackendState::INITIALIZED)
			return false;
		if(static_cast<size_t>(TargetId) >= m_vRenderTargets.size())
			m_vRenderTargets.resize(static_cast<size_t>(TargetId) + 1);
		if(!m_RenderTargetState.CanDestroy(TargetId))
			return false;
		DestroyRenderTarget(TargetId);
		if(static_cast<size_t>(Width) > std::numeric_limits<size_t>::max() / static_cast<size_t>(Height))
			return false;
		const size_t PixelCount = static_cast<size_t>(Width) * static_cast<size_t>(Height);
		if(PixelCount > std::numeric_limits<size_t>::max() / 4)
			return false;
		MTLTextureDescriptor *pDescriptor = [[MTLTextureDescriptor alloc] init];
		pDescriptor.textureType = MTLTextureType2D;
		pDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
		pDescriptor.width = static_cast<NSUInteger>(Width);
		pDescriptor.height = static_cast<NSUInteger>(Height);
		pDescriptor.mipmapLevelCount = 1;
		pDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		pDescriptor.storageMode = MTLStorageModePrivate;
		id<MTLTexture> Texture = [m_Device newTextureWithDescriptor:pDescriptor];
#if !__has_feature(objc_arc)
		[pDescriptor release];
#endif
		if(Texture == nil)
			return false;
		SRenderTarget &Target = m_vRenderTargets[TargetId];
		Target.m_Texture = Texture;
		Target.m_Width = static_cast<uint32_t>(Width);
		Target.m_Height = static_cast<uint32_t>(Height);
		Target.m_DataBytes = PixelCount * 4;
		Target.m_Allocated = true;
		AddTextureMemory(Target.m_DataBytes);
		return true;
	}

	void DestroyAllRenderTargets()
	{
		if(m_RenderTargetState.IsActive())
		{
			EndActiveEncoders();
			m_RenderTargetState.Reset();
		}
		for(size_t TargetId = 0; TargetId < m_vRenderTargets.size(); ++TargetId)
			DestroyRenderTarget(static_cast<int>(TargetId));
		m_vRenderTargets.clear();
	}

	bool CopyIntoBuffer(id<MTLBuffer> Destination, size_t DestinationOffset, const void *pData, size_t DataBytes)
	{
		if(Destination == nil || m_CurrentCommandBuffer == nil || DataBytes == 0)
			return false;
		id<MTLBuffer> Staging = [m_Device newBufferWithLength:DataBytes options:MTLResourceStorageModeShared];
		if(Staging == nil)
			return false;
		if(pData != nullptr)
			mem_copy(Staging.contents, pData, DataBytes);
		else
			std::memset(Staging.contents, 0, DataBytes);
		EndRenderEncoderForBlit();
		if(m_CurrentBlitEncoder == nil)
			m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
		const bool Success = m_CurrentBlitEncoder != nil;
		if(Success)
			[m_CurrentBlitEncoder copyFromBuffer:Staging sourceOffset:0 toBuffer:Destination destinationOffset:DestinationOffset size:DataBytes];
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->fetch_add(DataBytes, std::memory_order_relaxed);
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->fetch_sub(DataBytes, std::memory_order_relaxed);
		ReleaseMetalObject(Staging);
		return Success;
	}

	bool CreateBuffer(int Slot, size_t DataBytes, const void *pData, int Flags)
	{
		if(!EnsureBufferSlot(Slot))
			return false;
		if(m_vBufferSlots[Slot].m_Allocated)
			DestroyBuffer(Slot);
		const bool OneTimeUse = (Flags & IGraphics::EBufferObjectCreateFlags::BUFFER_OBJECT_CREATE_FLAGS_ONE_TIME_USE_BIT) != 0;
		SMetalBufferLayout Layout;
		if(!MetalBufferLayout(DataBytes, OneTimeUse, Layout) || m_Device == nil || m_CurrentCommandBuffer == nil)
			return false;
		const MTLResourceOptions Options = OneTimeUse ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate;
		id<MTLBuffer> Buffer = [m_Device newBufferWithLength:DataBytes options:Options];
		if(Buffer == nil)
			return false;
		bool Success = true;
		if(OneTimeUse)
		{
			if(pData != nullptr)
				mem_copy(Buffer.contents, pData, DataBytes);
			else
				std::memset(Buffer.contents, 0, DataBytes);
		}
		else
		{
			Success = CopyIntoBuffer(Buffer, 0, pData, DataBytes);
		}
		if(!Success)
		{
			ReleaseMetalObject(Buffer);
			return false;
		}
		SBufferSlot &SlotInfo = m_vBufferSlots[Slot];
		SlotInfo.m_Buffer = Buffer;
		SlotInfo.m_DataBytes = DataBytes;
		SlotInfo.m_OneTimeUse = OneTimeUse;
		SlotInfo.m_Allocated = true;
		AddBufferMemory(DataBytes);
		return true;
	}

	bool UpdateBuffer(int Slot, size_t Offset, size_t DataBytes, const void *pData)
	{
		if(Slot < 0 || static_cast<size_t>(Slot) >= m_vBufferSlots.size() || !m_vBufferSlots[Slot].m_Allocated)
			return false;
		SBufferSlot &Buffer = m_vBufferSlots[Slot];
		if(!MetalValidateBufferRange(Buffer.m_DataBytes, Offset, DataBytes))
			return false;
		if(Buffer.m_OneTimeUse)
		{
			if(pData != nullptr)
				mem_copy(static_cast<uint8_t *>(Buffer.m_Buffer.contents) + Offset, pData, DataBytes);
			else
				std::memset(static_cast<uint8_t *>(Buffer.m_Buffer.contents) + Offset, 0, DataBytes);
			return true;
		}
		return CopyIntoBuffer(Buffer.m_Buffer, Offset, pData, DataBytes);
	}

	bool CopyBuffer(int WriteSlot, int ReadSlot, size_t WriteOffset, size_t ReadOffset, size_t CopyBytes)
	{
		if(WriteSlot < 0 || ReadSlot < 0 || static_cast<size_t>(WriteSlot) >= m_vBufferSlots.size() || static_cast<size_t>(ReadSlot) >= m_vBufferSlots.size() || !m_vBufferSlots[WriteSlot].m_Allocated || !m_vBufferSlots[ReadSlot].m_Allocated)
			return false;
		SBufferSlot &WriteBuffer = m_vBufferSlots[WriteSlot];
		SBufferSlot &ReadBuffer = m_vBufferSlots[ReadSlot];
		if(!MetalValidateBufferRange(WriteBuffer.m_DataBytes, WriteOffset, CopyBytes) || !MetalValidateBufferRange(ReadBuffer.m_DataBytes, ReadOffset, CopyBytes))
			return false;
		if(WriteBuffer.m_OneTimeUse && ReadBuffer.m_OneTimeUse)
		{
			if(WriteSlot == ReadSlot && WriteOffset < ReadOffset + CopyBytes && ReadOffset < WriteOffset + CopyBytes)
				std::memmove(static_cast<uint8_t *>(WriteBuffer.m_Buffer.contents) + WriteOffset, static_cast<uint8_t *>(ReadBuffer.m_Buffer.contents) + ReadOffset, CopyBytes);
			else
				mem_copy(static_cast<uint8_t *>(WriteBuffer.m_Buffer.contents) + WriteOffset, static_cast<uint8_t *>(ReadBuffer.m_Buffer.contents) + ReadOffset, CopyBytes);
			return true;
		}
		if(m_CurrentCommandBuffer == nil)
			return false;
		EndRenderEncoderForBlit();
		if(m_CurrentBlitEncoder == nil)
			m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
		if(m_CurrentBlitEncoder == nil)
			return false;
		[m_CurrentBlitEncoder copyFromBuffer:ReadBuffer.m_Buffer sourceOffset:ReadOffset toBuffer:WriteBuffer.m_Buffer destinationOffset:WriteOffset size:CopyBytes];
		return true;
	}

	void AddStreamMemory(size_t Bytes)
	{
		if(m_pStreamMemoryUsage != nullptr)
			m_pStreamMemoryUsage->fetch_add(Bytes, std::memory_order_relaxed);
	}

	void SubStreamMemory(size_t Bytes)
	{
		if(m_pStreamMemoryUsage != nullptr)
			m_pStreamMemoryUsage->fetch_sub(Bytes, std::memory_order_relaxed);
	}

	bool LoadShaderLibrary(const SCommand_Init *pCommand)
	{
		if(pCommand->m_pStorage == nullptr)
			return false;
		void *pShaderData = nullptr;
		unsigned ShaderSize = 0;
		if(!pCommand->m_pStorage->ReadFile("data/shader/metal/qmclient.metallib", IStorage::TYPE_ALL, &pShaderData, &ShaderSize) || pShaderData == nullptr || ShaderSize == 0)
			return false;

		dispatch_data_t Data = dispatch_data_create(pShaderData, ShaderSize, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
			free(pShaderData);
		});
		NSError *pError = nil;
		m_ShaderLibrary = [m_Device newLibraryWithData:Data error:&pError];
#if !OS_OBJECT_USE_OBJC
		dispatch_release(Data);
#endif
		return m_ShaderLibrary != nil;
	}

	bool CreateSamplerStates()
	{
		MTLSamplerDescriptor *pDescriptor = [[MTLSamplerDescriptor alloc] init];
		pDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
		pDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
		pDescriptor.mipFilter = MTLSamplerMipFilterLinear;
		pDescriptor.sAddressMode = MTLSamplerAddressModeRepeat;
		pDescriptor.tAddressMode = MTLSamplerAddressModeRepeat;
		m_RepeatSampler = [m_Device newSamplerStateWithDescriptor:pDescriptor];
		pDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
		pDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
		m_ClampSampler = [m_Device newSamplerStateWithDescriptor:pDescriptor];
#if !__has_feature(objc_arc)
		[pDescriptor release];
#endif
		return m_RepeatSampler != nil && m_ClampSampler != nil;
	}

	bool CreatePipelineStates()
	{
		id<MTLFunction> VertexFunction = [m_ShaderLibrary newFunctionWithName:@"qmclient_vertex"];
		id<MTLFunction> ColorFunction = [m_ShaderLibrary newFunctionWithName:@"qmclient_fragment"];
		id<MTLFunction> TexturedFunction = [m_ShaderLibrary newFunctionWithName:@"qmclient_textured_fragment"];
		id<MTLFunction> TextFunction = [m_ShaderLibrary newFunctionWithName:@"qmclient_text_fragment"];
		id<MTLFunction> TexArrayVertex = [m_ShaderLibrary newFunctionWithName:@"qmclient_tex_array_vertex"];
		id<MTLFunction> TexArrayFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_tex_array_fragment"];
		if(VertexFunction == nil || ColorFunction == nil || TexturedFunction == nil || TextFunction == nil || TexArrayVertex == nil || TexArrayFragment == nil)
		{
			ReleaseMetalObject(VertexFunction);
			ReleaseMetalObject(ColorFunction);
			ReleaseMetalObject(TexturedFunction);
			ReleaseMetalObject(TextFunction);
			ReleaseMetalObject(TexArrayVertex);
			ReleaseMetalObject(TexArrayFragment);
			return false;
		}

		bool Success = true;
		auto CreatePipeline = [this, &Success](size_t Index, id<MTLFunction> Vertex, id<MTLFunction> Fragment, MTLVertexDescriptor *pDescriptor) {
			for(int Blend = 0; Blend <= static_cast<int>(EMetalBlendMode::ADDITIVE); ++Blend)
			{
				MTLRenderPipelineDescriptor *pPipeline = [[MTLRenderPipelineDescriptor alloc] init];
				pPipeline.vertexFunction = Vertex;
				pPipeline.fragmentFunction = Fragment;
				pPipeline.vertexDescriptor = pDescriptor;
				pPipeline.rasterSampleCount = 1;
				pPipeline.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
				const SMetalBlendState BlendState = MetalBlendState(static_cast<EMetalBlendMode>(Blend));
				pPipeline.colorAttachments[0].blendingEnabled = BlendState.m_Enabled;
				pPipeline.colorAttachments[0].sourceRGBBlendFactor = MetalBlendFactor(BlendState.m_Source);
				pPipeline.colorAttachments[0].destinationRGBBlendFactor = MetalBlendFactor(BlendState.m_Destination);
				pPipeline.colorAttachments[0].sourceAlphaBlendFactor = MetalBlendFactor(BlendState.m_Source);
				pPipeline.colorAttachments[0].destinationAlphaBlendFactor = MetalBlendFactor(BlendState.m_Destination);
				NSError *pError = nil;
				m_aPipelineStates[Index + static_cast<size_t>(Blend)] = [m_Device newRenderPipelineStateWithDescriptor:pPipeline error:&pError];
				Success = Success && m_aPipelineStates[Index + static_cast<size_t>(Blend)] != nil;
#if !__has_feature(objc_arc)
				[pPipeline release];
#endif
			}
		};

		MTLVertexDescriptor *pVertexDescriptor = [[MTLVertexDescriptor alloc] init];
		pVertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		pVertexDescriptor.attributes[0].offset = offsetof(GL_SVertex, m_Pos);
		pVertexDescriptor.attributes[0].bufferIndex = 0;
		pVertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
		pVertexDescriptor.attributes[1].offset = offsetof(GL_SVertex, m_Tex);
		pVertexDescriptor.attributes[1].bufferIndex = 0;
		pVertexDescriptor.attributes[2].format = MTLVertexFormatUChar4Normalized;
		pVertexDescriptor.attributes[2].offset = offsetof(GL_SVertex, m_Color);
		pVertexDescriptor.attributes[2].bufferIndex = 0;
		pVertexDescriptor.layouts[0].stride = sizeof(GL_SVertex);
		pVertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		for(int Textured = 0; Textured <= 1; ++Textured)
		{
			for(int Text = 0; Text <= 1; ++Text)
			{
				if(Textured == 0 && Text != 0)
					continue;
				CreatePipeline(PipelineIndex(Textured != 0, Text != 0, EMetalBlendMode::NONE), VertexFunction, Text ? TextFunction : (Textured ? TexturedFunction : ColorFunction), pVertexDescriptor);
			}
		}

		MTLVertexDescriptor *pTexArrayDescriptor = [[MTLVertexDescriptor alloc] init];
		pTexArrayDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		pTexArrayDescriptor.attributes[0].offset = offsetof(GL_SVertexTex3DStream, m_Pos);
		pTexArrayDescriptor.attributes[0].bufferIndex = 0;
		pTexArrayDescriptor.attributes[1].format = MTLVertexFormatUChar4Normalized;
		pTexArrayDescriptor.attributes[1].offset = offsetof(GL_SVertexTex3DStream, m_Color);
		pTexArrayDescriptor.attributes[1].bufferIndex = 0;
		pTexArrayDescriptor.attributes[2].format = MTLVertexFormatFloat3;
		pTexArrayDescriptor.attributes[2].offset = offsetof(GL_SVertexTex3DStream, m_Tex);
		pTexArrayDescriptor.attributes[2].bufferIndex = 0;
		pTexArrayDescriptor.layouts[0].stride = sizeof(GL_SVertexTex3DStream);
		pTexArrayDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		CreatePipeline(TextureArrayPipelineIndex(EMetalBlendMode::NONE), TexArrayVertex, TexArrayFragment, pTexArrayDescriptor);

		id<MTLFunction> TileVertex = [m_ShaderLibrary newFunctionWithName:@"qmclient_tile_vertex"];
		id<MTLFunction> TilePlainVertex = [m_ShaderLibrary newFunctionWithName:@"qmclient_tile_plain_vertex"];
		id<MTLFunction> TileFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_tile_fragment"];
		id<MTLFunction> TileTexturedFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_tile_textured_fragment"];
		id<MTLFunction> QuadVertexGrouped = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_vertex_grouped"];
		id<MTLFunction> QuadVertexUngrouped = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_vertex_ungrouped"];
		id<MTLFunction> QuadFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_fragment"];
		id<MTLFunction> QuadTexturedFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_textured_fragment"];
		id<MTLFunction> QuadContainerExVertex = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_container_ex_vertex"];
		id<MTLFunction> QuadContainerExFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_container_ex_fragment"];
		id<MTLFunction> QuadContainerExTexturedFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_quad_container_ex_textured_fragment"];
		id<MTLFunction> SpriteMultipleVertex = [m_ShaderLibrary newFunctionWithName:@"qmclient_sprite_multiple_vertex"];
		id<MTLFunction> SpriteMultipleFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_sprite_multiple_fragment"];
		id<MTLFunction> SpriteMultipleTexturedFragment = [m_ShaderLibrary newFunctionWithName:@"qmclient_sprite_multiple_textured_fragment"];
		if(TileVertex == nil || TilePlainVertex == nil || TileFragment == nil || TileTexturedFragment == nil || QuadVertexGrouped == nil || QuadVertexUngrouped == nil || QuadFragment == nil || QuadTexturedFragment == nil || QuadContainerExVertex == nil || QuadContainerExFragment == nil || QuadContainerExTexturedFragment == nil || SpriteMultipleVertex == nil || SpriteMultipleFragment == nil || SpriteMultipleTexturedFragment == nil)
			Success = false;

		MTLVertexDescriptor *pTileDescriptor = [[MTLVertexDescriptor alloc] init];
		pTileDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		pTileDescriptor.attributes[0].offset = 0;
		pTileDescriptor.attributes[0].bufferIndex = 0;
		pTileDescriptor.attributes[1].format = MTLVertexFormatUChar4;
		pTileDescriptor.attributes[1].offset = sizeof(float) * 2;
		pTileDescriptor.attributes[1].bufferIndex = 0;
		pTileDescriptor.layouts[0].stride = sizeof(float) * 2 + sizeof(uint8_t) * 4;
		pTileDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		MTLVertexDescriptor *pTilePlainDescriptor = [[MTLVertexDescriptor alloc] init];
		pTilePlainDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		pTilePlainDescriptor.attributes[0].offset = 0;
		pTilePlainDescriptor.attributes[0].bufferIndex = 0;
		pTilePlainDescriptor.layouts[0].stride = sizeof(float) * 2;
		pTilePlainDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		if(TileVertex != nil && TileTexturedFragment != nil)
			CreatePipeline(TilePipelineIndex(true, false, EMetalBlendMode::NONE), TileVertex, TileTexturedFragment, pTileDescriptor);
		if(TilePlainVertex != nil && TileFragment != nil)
			CreatePipeline(TilePipelineIndex(false, false, EMetalBlendMode::NONE), TilePlainVertex, TileFragment, pTilePlainDescriptor);
		if(TileVertex != nil && TileTexturedFragment != nil)
			CreatePipeline(TilePipelineIndex(true, true, EMetalBlendMode::NONE), TileVertex, TileTexturedFragment, pTileDescriptor);
		if(TilePlainVertex != nil && TileFragment != nil)
			CreatePipeline(TilePipelineIndex(false, true, EMetalBlendMode::NONE), TilePlainVertex, TileFragment, pTilePlainDescriptor);

		MTLVertexDescriptor *pQuadDescriptor = [[MTLVertexDescriptor alloc] init];
		pQuadDescriptor.attributes[0].format = MTLVertexFormatFloat4;
		pQuadDescriptor.attributes[0].offset = 0;
		pQuadDescriptor.attributes[0].bufferIndex = 0;
		pQuadDescriptor.attributes[1].format = MTLVertexFormatUChar4Normalized;
		pQuadDescriptor.attributes[1].offset = sizeof(float) * 4;
		pQuadDescriptor.attributes[1].bufferIndex = 0;
		pQuadDescriptor.attributes[2].format = MTLVertexFormatFloat2;
		pQuadDescriptor.attributes[2].offset = sizeof(float) * 4 + sizeof(uint8_t) * 4;
		pQuadDescriptor.attributes[2].bufferIndex = 0;
		pQuadDescriptor.layouts[0].stride = sizeof(float) * 4 + sizeof(uint8_t) * 4 + sizeof(float) * 2;
		pQuadDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		MTLVertexDescriptor *pQuadPlainDescriptor = [[MTLVertexDescriptor alloc] init];
		pQuadPlainDescriptor.attributes[0].format = MTLVertexFormatFloat4;
		pQuadPlainDescriptor.attributes[0].offset = 0;
		pQuadPlainDescriptor.attributes[0].bufferIndex = 0;
		pQuadPlainDescriptor.attributes[1].format = MTLVertexFormatUChar4Normalized;
		pQuadPlainDescriptor.attributes[1].offset = sizeof(float) * 4;
		pQuadPlainDescriptor.attributes[1].bufferIndex = 0;
		pQuadPlainDescriptor.layouts[0].stride = sizeof(float) * 4 + sizeof(uint8_t) * 4;
		pQuadPlainDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
		for(int Textured = 0; Textured <= 1; ++Textured)
		{
			MTLVertexDescriptor *pDescriptor = Textured ? pQuadDescriptor : pQuadPlainDescriptor;
			id<MTLFunction> Fragment = Textured ? QuadTexturedFragment : QuadFragment;
			if(QuadVertexGrouped != nil && Fragment != nil)
				CreatePipeline(QuadPipelineIndex(Textured != 0, true, EMetalBlendMode::NONE), QuadVertexGrouped, Fragment, pDescriptor);
			if(QuadVertexUngrouped != nil && Fragment != nil)
				CreatePipeline(QuadPipelineIndex(Textured != 0, false, EMetalBlendMode::NONE), QuadVertexUngrouped, Fragment, pDescriptor);
		}
		if(QuadContainerExVertex != nil && QuadContainerExFragment != nil)
			CreatePipeline(QuadContainerExPipelineIndex(false, EMetalBlendMode::NONE), QuadContainerExVertex, QuadContainerExFragment, pVertexDescriptor);
		if(QuadContainerExVertex != nil && QuadContainerExTexturedFragment != nil)
			CreatePipeline(QuadContainerExPipelineIndex(true, EMetalBlendMode::NONE), QuadContainerExVertex, QuadContainerExTexturedFragment, pVertexDescriptor);
		if(SpriteMultipleVertex != nil && SpriteMultipleFragment != nil)
			CreatePipeline(SpriteMultiplePipelineIndex(false, EMetalBlendMode::NONE), SpriteMultipleVertex, SpriteMultipleFragment, pVertexDescriptor);
		if(SpriteMultipleVertex != nil && SpriteMultipleTexturedFragment != nil)
			CreatePipeline(SpriteMultiplePipelineIndex(true, EMetalBlendMode::NONE), SpriteMultipleVertex, SpriteMultipleTexturedFragment, pVertexDescriptor);
#if !__has_feature(objc_arc)
		[pVertexDescriptor release];
		[pTexArrayDescriptor release];
		[pTileDescriptor release];
		[pTilePlainDescriptor release];
		[pQuadDescriptor release];
		[pQuadPlainDescriptor release];
		[VertexFunction release];
		[ColorFunction release];
		[TexturedFunction release];
		[TextFunction release];
		[TexArrayVertex release];
		[TexArrayFragment release];
		[TileVertex release];
		[TilePlainVertex release];
		[TileFragment release];
		[TileTexturedFragment release];
		[QuadVertexGrouped release];
		[QuadVertexUngrouped release];
		[QuadFragment release];
		[QuadTexturedFragment release];
		[QuadContainerExVertex release];
		[QuadContainerExFragment release];
		[QuadContainerExTexturedFragment release];
		[SpriteMultipleVertex release];
		[SpriteMultipleFragment release];
		[SpriteMultipleTexturedFragment release];
#endif
		return Success;
	}

	bool CreateFrameResources()
	{
		for(SFrameSlot &Frame : m_aFrameSlots)
		{
			Frame.m_VertexBuffer = [m_Device newBufferWithLength:gs_StreamBufferSize options:MTLResourceStorageModeShared];
			if(Frame.m_VertexBuffer == nil)
				return false;
		}

		const size_t QuadCount = CCommandBuffer::MAX_VERTICES / 4;
		std::vector<uint16_t> vIndices;
		vIndices.reserve(QuadCount * 6);
		for(uint16_t Quad = 0; Quad < QuadCount; ++Quad)
		{
			const uint16_t Base = static_cast<uint16_t>(Quad * 4);
			vIndices.push_back(Base + 0);
			vIndices.push_back(Base + 1);
			vIndices.push_back(Base + 2);
			vIndices.push_back(Base + 0);
			vIndices.push_back(Base + 2);
			vIndices.push_back(Base + 3);
		}
		m_QuadIndexBuffer = [m_Device newBufferWithBytes:vIndices.data() length:vIndices.size() * sizeof(uint16_t) options:MTLResourceStorageModePrivate];
		if(m_QuadIndexBuffer == nil)
			return false;

		m_StreamMemoryBytes = gs_FrameSlotCount * gs_StreamBufferSize;
		m_BufferMemoryBytes = vIndices.size() * sizeof(uint16_t);
		AddStreamMemory(m_StreamMemoryBytes);
		AddBufferMemory(m_BufferMemoryBytes);
		return true;
	}

	bool EnsureTextureSlot(int Slot)
	{
		if(Slot < 0 || static_cast<size_t>(Slot) >= CCommandBuffer::MAX_TEXTURES)
			return false;
		if(static_cast<size_t>(Slot) >= m_vTextureSlots.size())
			m_vTextureSlots.resize(static_cast<size_t>(Slot) + 1);
		return true;
	}

	void DestroyTexture(int Slot)
	{
		if(!EnsureTextureSlot(Slot))
			return;
		STextureSlot &Texture = m_vTextureSlots[Slot];
		if(!Texture.m_Allocated)
			return;
		SubTextureMemory(Texture.m_Layout.m_DataBytes);
		SubTextureMemory(Texture.m_ArrayLayout.m_DataBytes);
		if(Texture.m_Staging != nil)
		{
			if(m_pStagingMemoryUsage != nullptr)
				m_pStagingMemoryUsage->fetch_sub([Texture.m_Staging length], std::memory_order_relaxed);
			ReleaseMetalObject(Texture.m_Staging);
		}
		ReleaseMetalObject(Texture.m_Texture);
		ReleaseMetalObject(Texture.m_TextureArray);
		Texture = {};
	}

	bool CreateTexture(int Slot, size_t Width, size_t Height, EMetalTextureFormat Format, int Flags, uint8_t *pData)
	{
		if(!EnsureTextureSlot(Slot))
			return false;
		STextureSlot &Texture = m_vTextureSlots[Slot];
		if(Texture.m_Allocated)
			DestroyTexture(Slot);

		const bool Wants2D = (Flags & TextureFlag::NO_2D_TEXTURE) == 0;
		const bool Wants2DArray = (Flags & TextureFlag::TO_2D_ARRAY_TEXTURE) != 0;
		if(!Wants2D && !Wants2DArray)
			return false;
		SMetalTextureLayout Layout;
		if(Wants2D && !MetalTextureLayout(Width, Height, Format, (Flags & TextureFlag::NO_MIPMAPS) != 0, Layout))
			return false;
		size_t ArrayWidth = 0;
		size_t ArrayHeight = 0;
		SMetalTextureLayout ArrayLayout;
		if(Wants2DArray && (Width > static_cast<size_t>(std::numeric_limits<int>::max()) || Height > static_cast<size_t>(std::numeric_limits<int>::max()) || !MetalTextureArrayLayout(Width, Height, Format, (Flags & TextureFlag::NO_MIPMAPS) != 0, ArrayWidth, ArrayHeight, ArrayLayout)))
			return false;
		if(Layout.m_DataBytes > std::numeric_limits<size_t>::max() - ArrayLayout.m_DataBytes)
			return false;

		if(m_Device == nil || m_State != EMetalBackendState::INITIALIZED || m_CommandQueue == nil)
			return false;

		const MTLPixelFormat PixelFormat = Format == EMetalTextureFormat::R8 ? MTLPixelFormatR8Unorm : MTLPixelFormatRGBA8Unorm;
		id<MTLTexture> pTexture = nil;
		if(Wants2D)
		{
			MTLTextureDescriptor *pDescriptor = [[MTLTextureDescriptor alloc] init];
			pDescriptor.textureType = MTLTextureType2D;
			pDescriptor.pixelFormat = PixelFormat;
			pDescriptor.width = Width;
			pDescriptor.height = Height;
			pDescriptor.mipmapLevelCount = Layout.m_MipLevels;
			pDescriptor.usage = MTLTextureUsageShaderRead;
			pDescriptor.storageMode = MTLStorageModePrivate;
			pTexture = [m_Device newTextureWithDescriptor:pDescriptor];
	#if !__has_feature(objc_arc)
			[pDescriptor release];
	#endif
			if(pTexture == nil)
				return false;
		}
		id<MTLTexture> pTextureArray = nil;
		if(Wants2DArray)
		{
			MTLTextureDescriptor *pArrayDescriptor = [[MTLTextureDescriptor alloc] init];
			pArrayDescriptor.textureType = MTLTextureType2DArray;
			pArrayDescriptor.pixelFormat = PixelFormat;
			pArrayDescriptor.width = ArrayWidth;
			pArrayDescriptor.height = ArrayHeight;
			pArrayDescriptor.arrayLength = METAL_TEXTURE_ARRAY_LAYERS;
			pArrayDescriptor.mipmapLevelCount = ArrayLayout.m_MipLevels;
			pArrayDescriptor.usage = MTLTextureUsageShaderRead;
			pArrayDescriptor.storageMode = MTLStorageModePrivate;
			pTextureArray = [m_Device newTextureWithDescriptor:pArrayDescriptor];
	#if !__has_feature(objc_arc)
			[pArrayDescriptor release];
	#endif
			if(pTextureArray == nil)
			{
				ReleaseMetalObject(pTexture);
				return false;
			}
		}

		Texture.m_Texture = pTexture;
		Texture.m_TextureArray = pTextureArray;
		Texture.m_Layout = Layout;
		Texture.m_ArrayLayout = ArrayLayout;
		Texture.m_Width = Width;
		Texture.m_Height = Height;
		Texture.m_ArrayWidth = ArrayWidth;
		Texture.m_ArrayHeight = ArrayHeight;
		Texture.m_Format = Format;
		Texture.m_Is2DArray = Wants2DArray;
		Texture.m_Allocated = true;
		AddTextureMemory(Layout.m_DataBytes + ArrayLayout.m_DataBytes);

		bool Uploaded = pData == nullptr || pTexture == nil;
		if(pData != nullptr && pTexture != nil)
			Uploaded = UploadTextureRegion(Texture, 0, 0, Width, Height, pData);
		if(Uploaded && pTextureArray != nil && pData != nullptr)
		{
			size_t SourceBytes = 0;
			if(!MetalCheckedMul(Width, Height, SourceBytes) || !MetalCheckedMul(SourceBytes, MetalTextureBytesPerPixel(Format), SourceBytes))
				Uploaded = false;
			else
			{
				std::vector<uint8_t> vArrayData(SourceBytes);
				int ConvertedWidth = 0;
				int ConvertedHeight = 0;
				Texture2DTo3D(pData, static_cast<int>(Width), static_cast<int>(Height), MetalTextureBytesPerPixel(Format), 16, 16, vArrayData.data(), ConvertedWidth, ConvertedHeight);
				Uploaded = ConvertedWidth == static_cast<int>(ArrayWidth) && ConvertedHeight == static_cast<int>(ArrayHeight) && UploadTextureArray(Texture, vArrayData.data(), ArrayWidth, ArrayHeight, 0, METAL_TEXTURE_ARRAY_LAYERS);
			}
		}
		if(!Uploaded)
		{
			DestroyTexture(Slot);
			return false;
		}
		if(m_CurrentBlitEncoder != nil)
		{
			if(Layout.m_MipLevels > 1 && Texture.m_Texture != nil)
				[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_Texture];
			if(ArrayLayout.m_MipLevels > 1 && Texture.m_TextureArray != nil)
				[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_TextureArray];
		}
		return true;
	}

	bool UpdateTexture(int Slot, size_t X, size_t Y, size_t Width, size_t Height, EMetalTextureFormat Format, uint8_t *pData)
	{
		if(!EnsureTextureSlot(Slot) || !m_vTextureSlots[Slot].m_Allocated || m_CommandQueue == nil)
			return false;
		STextureSlot &Texture = m_vTextureSlots[Slot];
		if(Texture.m_Format != Format || !MetalValidateSubregion(Texture.m_Width, Texture.m_Height, X, Y, Width, Height))
			return false;
		if(Texture.m_Is2DArray && (X != 0 || Y != 0 || Width != Texture.m_Width || Height != Texture.m_Height))
			return false;
		if(pData == nullptr)
			return true;
		bool Uploaded = Texture.m_Texture == nil || UploadTextureRegion(Texture, X, Y, Width, Height, pData);
		if(Uploaded && Texture.m_Is2DArray)
		{
			size_t SourceBytes = 0;
			if(!MetalCheckedMul(Texture.m_Width, Texture.m_Height, SourceBytes) || !MetalCheckedMul(SourceBytes, MetalTextureBytesPerPixel(Format), SourceBytes) || Texture.m_Width > static_cast<size_t>(std::numeric_limits<int>::max()) || Texture.m_Height > static_cast<size_t>(std::numeric_limits<int>::max()))
				return false;
			std::vector<uint8_t> vArrayData(SourceBytes);
			int ConvertedWidth = 0;
			int ConvertedHeight = 0;
			Texture2DTo3D(pData, static_cast<int>(Width), static_cast<int>(Height), MetalTextureBytesPerPixel(Format), 16, 16, vArrayData.data(), ConvertedWidth, ConvertedHeight);
			Uploaded = ConvertedWidth == static_cast<int>(Texture.m_ArrayWidth) && ConvertedHeight == static_cast<int>(Texture.m_ArrayHeight) && UploadTextureArray(Texture, vArrayData.data(), Texture.m_ArrayWidth, Texture.m_ArrayHeight, 0, METAL_TEXTURE_ARRAY_LAYERS);
		}
		if(Uploaded && m_CurrentBlitEncoder != nil)
		{
			if(Texture.m_Layout.m_MipLevels > 1 && Texture.m_Texture != nil)
				[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_Texture];
			if(Texture.m_ArrayLayout.m_MipLevels > 1 && Texture.m_TextureArray != nil)
				[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_TextureArray];
		}
		return Uploaded;
	}

	static STWGraphicGpu::ETWGraphicsGpuType DeviceType(id<MTLDevice> Device)
	{
		if(str_startswith(Device.name.UTF8String, "Apple"))
			return STWGraphicGpu::ETWGraphicsGpuType::GRAPHICS_GPU_TYPE_INTEGRATED;
		return Device.lowPower ? STWGraphicGpu::ETWGraphicsGpuType::GRAPHICS_GPU_TYPE_INTEGRATED : STWGraphicGpu::ETWGraphicsGpuType::GRAPHICS_GPU_TYPE_DISCRETE;
	}

	static const char *VendorName(id<MTLDevice> Device)
	{
		const char *pName = Device.name.UTF8String;
		if(str_startswith(pName, "Apple"))
			return "Apple";
		if(str_startswith_nocase(pName, "AMD"))
			return "AMD";
		if(str_startswith_nocase(pName, "Intel"))
			return "Intel";
		if(str_startswith_nocase(pName, "NVIDIA"))
			return "NVIDIA";
		return "Metal";
	}

	void UpdateDrawableSize()
	{
		if(m_pWindow == nullptr || m_pLayer == nullptr)
			return;
		int Width = 0;
		int Height = 0;
		SDL_Metal_GetDrawableSize(m_pWindow, &Width, &Height);
		CAMetalLayer *pLayer = (__bridge CAMetalLayer *)m_pLayer;
		pLayer.drawableSize = CGSizeMake(std::max(Width, 0), std::max(Height, 0));
		m_DrawableWidth = static_cast<uint32_t>(std::max(Width, 0));
		m_DrawableHeight = static_cast<uint32_t>(std::max(Height, 0));
	}

	void ConfigureLayer()
	{
		CAMetalLayer *pLayer = (__bridge CAMetalLayer *)m_pLayer;
		pLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		pLayer.maximumDrawableCount = 3;
		pLayer.framebufferOnly = NO;
		pLayer.presentsWithTransaction = NO;
		pLayer.displaySyncEnabled = m_VSync;
		UpdateDrawableSize();
	}

	void SelectDevice(const char *pConfiguredGpuName)
	{
		NSArray<id<MTLDevice>> *pDevices = MTLCopyAllDevices();
		id<MTLDevice> pDefaultDevice = MTLCreateSystemDefaultDevice();
		id<MTLDevice> pSelectedDevice = pDefaultDevice;
		const bool IsAuto = pConfiguredGpuName == nullptr || str_comp(pConfiguredGpuName, "auto") == 0;
		bool ExplicitMatch = false;

		if(m_pGpuList != nullptr)
		{
			m_pGpuList->m_vGpus.clear();
			m_pGpuList->m_AutoGpu = {};
			for(id<MTLDevice> pDevice in pDevices)
			{
				STWGraphicGpu::STWGraphicGpuItem Gpu = {};
				str_copy(Gpu.m_aName, pDevice.name.UTF8String);
				Gpu.m_GpuType = DeviceType(pDevice);
				m_pGpuList->m_vGpus.push_back(Gpu);
				if(pDefaultDevice == pDevice)
					m_pGpuList->m_AutoGpu = Gpu;
				if(!IsAuto && str_comp(pDevice.name.UTF8String, pConfiguredGpuName) == 0)
				{
					pSelectedDevice = pDevice;
					ExplicitMatch = true;
				}
			}
		}

		if(!IsAuto && !ExplicitMatch)
			log_warn("gfx/metal", "Configured GPU '%s' was not found; using the system default Metal device.", pConfiguredGpuName);

		SetDevice(pSelectedDevice);
		if(m_Device == nil)
		{
			if(m_pVendorString != nullptr)
				str_copy(m_pVendorString, "Metal", gs_MetalGpuInfoStringSize);
			if(m_pVersionString != nullptr)
				str_copy(m_pVersionString, "Metal native", gs_MetalGpuInfoStringSize);
			if(m_pRendererString != nullptr)
				str_copy(m_pRendererString, "No Metal device", gs_MetalGpuInfoStringSize);
		}
		else
		{
			if(m_pVendorString != nullptr)
				str_copy(m_pVendorString, VendorName(m_Device), gs_MetalGpuInfoStringSize);
			if(m_pVersionString != nullptr)
				str_copy(m_pVersionString, "Metal native", gs_MetalGpuInfoStringSize);
			if(m_pRendererString != nullptr)
				str_copy(m_pRendererString, m_Device.name.UTF8String, gs_MetalGpuInfoStringSize);
			CAMetalLayer *pLayer = (__bridge CAMetalLayer *)m_pLayer;
			pLayer.device = m_Device;
			ConfigureLayer();
		}

#if !__has_feature(objc_arc)
		[pDevices release];
		[pDefaultDevice release];
#endif
	}

	bool GetPresentedImageData(uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) override
	{
		if(m_LastPresentedReadback == nil || m_LastPresentedCommandBuffer == nil || m_LastPresentedReadbackWidth == 0 || m_LastPresentedReadbackHeight == 0)
			return false;
		[m_LastPresentedCommandBuffer waitUntilCompleted];
		if(m_LastPresentedCommandBuffer.status != MTLCommandBufferStatusCompleted)
			return false;
		size_t PixelCount = 0;
		size_t DataBytes = 0;
		if(!MetalCheckedMul(static_cast<size_t>(m_LastPresentedReadbackWidth), static_cast<size_t>(m_LastPresentedReadbackHeight), PixelCount) || !MetalCheckedMul(PixelCount, 4, DataBytes))
			return false;
		vDstData.resize(DataBytes);
		const uint8_t *pReadback = static_cast<const uint8_t *>(m_LastPresentedReadback.contents);
		for(uint32_t Y = 0; Y < m_LastPresentedReadbackHeight; ++Y)
			CopyBgraToRgba(vDstData.data() + static_cast<size_t>(Y) * m_LastPresentedReadbackWidth * 4, pReadback + static_cast<size_t>(Y) * m_LastPresentedReadbackRowBytes, m_LastPresentedReadbackWidth);
		Width = m_LastPresentedReadbackWidth;
		Height = m_LastPresentedReadbackHeight;
		Format = CImageInfo::FORMAT_RGBA;
		return true;
	}

	void SetUnsupportedCommandError(const CCommandBuffer::SCommand *pCommand, EGfxErrorType ErrorType = GFX_ERROR_TYPE_RENDER_CMD_FAILED)
	{
		m_Error = {};
		m_Error.m_ErrorType = ErrorType;
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{
			false,
			"Metal command processor does not implement command " + std::to_string(pCommand->m_Cmd) +
				" (backend_state=" + MetalBackendStateName(m_State) + ", frame_id=" + std::to_string(m_FrameId) + ")"});
	}

	void SetResourceCommandError(const CCommandBuffer::SCommand *pCommand)
	{
		m_Error = {};
		m_Error.m_ErrorType = GFX_ERROR_TYPE_RENDER_CMD_FAILED;
		m_Error.m_vErrors.emplace_back(SGfxErrorContainer::SError{
			false,
			"Metal buffer resource command failed " + std::to_string(pCommand->m_Cmd) +
			" (backend_state=" + MetalBackendStateName(m_State) + ", frame_id=" + std::to_string(m_FrameId) + ")"});
	}

	void Cmd_PreInit(const SCommand_PreInit *pCommand)
	{
		m_State = EMetalBackendState::UNINITIALIZED;
		m_pVendorString = pCommand->m_pVendorString;
		m_pVersionString = pCommand->m_pVersionString;
		m_pRendererString = pCommand->m_pRendererString;
		m_pGpuList = pCommand->m_pGpuList;
		m_pWindow = pCommand->m_pWindow;
		m_VSync = pCommand->m_VSync;
		if(m_MetalView != nullptr)
		{
			SDL_Metal_DestroyView(m_MetalView);
			m_MetalView = nullptr;
			m_pLayer = nullptr;
		}
		if(pCommand->m_pWindow == nullptr)
		{
			if(pCommand->m_pInitError != nullptr)
				*pCommand->m_pInitError = -1;
			if(pCommand->m_pErrStringPtr != nullptr)
				*pCommand->m_pErrStringPtr = "Metal pre-init requires an SDL window";
			return;
		}

		m_MetalView = SDL_Metal_CreateView(pCommand->m_pWindow);
		m_pLayer = m_MetalView != nullptr ? SDL_Metal_GetLayer(m_MetalView) : nullptr;
		if(m_MetalView == nullptr || m_pLayer == nullptr)
		{
			if(m_MetalView != nullptr)
				SDL_Metal_DestroyView(m_MetalView);
			m_MetalView = nullptr;
			m_pLayer = nullptr;
			if(pCommand->m_pInitError != nullptr)
				*pCommand->m_pInitError = -1;
			if(pCommand->m_pErrStringPtr != nullptr)
				*pCommand->m_pErrStringPtr = "SDL could not create the Metal view or layer";
			return;
		}
		SelectDevice(g_Config.m_GfxGpuName);
		if(m_Device == nil)
		{
			if(pCommand->m_pInitError != nullptr)
				*pCommand->m_pInitError = -1;
			if(pCommand->m_pErrStringPtr != nullptr)
				*pCommand->m_pErrStringPtr = "No Metal device is available";
			return;
		}
		m_State = EMetalBackendState::PRE_INITIALIZED;
	}

	void Cmd_Init(const SCommand_Init *pCommand)
	{
		m_pCapabilities = pCommand->m_pCapabilities;
		m_pStorage = pCommand->m_pStorage;
		if(pCommand->m_pReadPresentedImageDataFunc != nullptr)
			*pCommand->m_pReadPresentedImageDataFunc = [this](uint32_t &Width, uint32_t &Height, CImageInfo::EImageFormat &Format, std::vector<uint8_t> &vDstData) {
				return GetPresentedImageData(Width, Height, Format, vDstData);
			};
		m_pBufferMemoryUsage = pCommand->m_pBufferMemoryUsage;
		m_pStreamMemoryUsage = pCommand->m_pStreamMemoryUsage;
		m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
		m_pStagingMemoryUsage = pCommand->m_pStagingMemoryUsage;
		if(m_pCapabilities != nullptr)
			m_pCapabilities->Reset();
		if(m_pBufferMemoryUsage != nullptr)
			m_pBufferMemoryUsage->store(0, std::memory_order_relaxed);
		if(m_pStreamMemoryUsage != nullptr)
			m_pStreamMemoryUsage->store(0, std::memory_order_relaxed);
		if(m_pTextureMemoryUsage != nullptr)
			m_pTextureMemoryUsage->store(0, std::memory_order_relaxed);
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->store(0, std::memory_order_relaxed);
		if(pCommand->m_pInitError != nullptr)
			*pCommand->m_pInitError = -1;
		if(pCommand->m_pErrStringPtr != nullptr)
			*pCommand->m_pErrStringPtr = "native Metal initialization failed";

		if(m_State != EMetalBackendState::PRE_INITIALIZED || m_Device == nil || m_pLayer == nullptr)
		{
			if(pCommand->m_pErrStringPtr != nullptr)
				*pCommand->m_pErrStringPtr = "Metal init requires a pre-initialized device and layer";
			return;
		}

		m_CommandQueue = [m_Device newCommandQueue];
		if(m_CommandQueue == nil || !LoadShaderLibrary(pCommand) || !CreateSamplerStates() || !CreatePipelineStates() || !CreateFrameResources())
		{
			ReleaseGpuObjects();
			if(pCommand->m_pErrStringPtr != nullptr)
				*pCommand->m_pErrStringPtr = "Metal queue, shader, pipeline, or frame resource creation failed";
			return;
		}

		m_FrameState.DrainFrames();
		m_CurrentFrameSlot = 0;
		m_FrameId = 0;
		m_GpuFailure.store(false, std::memory_order_relaxed);
		m_GpuFailureFrameId.store(0, std::memory_order_relaxed);
		m_GpuFailureCommandId.store(-1, std::memory_order_relaxed);
		m_GpuFailureStatus.store(0, std::memory_order_relaxed);
		m_LastCommandId.store(-1, std::memory_order_relaxed);
		{
			std::lock_guard<std::mutex> Lock(m_GpuFailureMutex);
			m_GpuFailureDescription.clear();
		}
		m_State = EMetalBackendState::INITIALIZED;
		if(m_pCapabilities != nullptr)
		{
			m_pCapabilities->m_MipMapping = true;
			m_pCapabilities->m_NPOTTextures = true;
			m_pCapabilities->m_2DArrayTextures = true;
			m_pCapabilities->m_ShaderSupport = true;
			m_pCapabilities->m_QuadContainerBuffering = true;
			m_pCapabilities->m_TextBuffering = true;
			m_pCapabilities->m_TrianglesAsQuads = true;
			m_pCapabilities->m_RenderTargets = false;
			m_pCapabilities->m_RenderTargetGaussianBlur = false;
			m_pCapabilities->m_BackbufferCapture = false;
			m_pCapabilities->m_pRenderTargetSupportReason = "metal_render_target_readback_not_implemented";
		}
		if(pCommand->m_pInitError != nullptr)
			*pCommand->m_pInitError = 0;
		if(pCommand->m_pErrStringPtr != nullptr)
			*pCommand->m_pErrStringPtr = nullptr;
	}

	void Cmd_Shutdown(const SCommand_Shutdown *pCommand)
	{
		(void)pCommand;
		m_State = EMetalBackendState::SHUTDOWN;
	}

	void Cmd_PostShutdown(const SCommand_PostShutdown *pCommand)
	{
		(void)pCommand;
		WaitForGpuIdle();
		DestroyAllBufferContainers();
		DestroyAllBuffers();
		ReleaseGpuObjects();
		m_FrameState.DrainFrames();
		m_GpuFailure.store(false, std::memory_order_relaxed);
		m_GpuFailureFrameId.store(0, std::memory_order_relaxed);
		m_GpuFailureCommandId.store(-1, std::memory_order_relaxed);
		m_GpuFailureStatus.store(0, std::memory_order_relaxed);
		m_LastCommandId.store(-1, std::memory_order_relaxed);
		{
			std::lock_guard<std::mutex> Lock(m_GpuFailureMutex);
			m_GpuFailureDescription.clear();
		}
		if(m_MetalView != nullptr)
			SDL_Metal_DestroyView(m_MetalView);
		m_MetalView = nullptr;
		m_pLayer = nullptr;
		m_pWindow = nullptr;
		for(size_t Slot = 0; Slot < m_vTextureSlots.size(); ++Slot)
			DestroyTexture(static_cast<int>(Slot));
		m_vTextureSlots.clear();
		ReleaseCommandQueue();
		ReleaseDevice();
		m_State = EMetalBackendState::UNINITIALIZED;
	}

	void ErroneousCleanup() override
	{
		EndActiveEncoders();
		WaitForGpuIdle();
		DestroyAllBufferContainers();
		DestroyAllBuffers();
		ReleaseGpuObjects();
		if(m_MetalView != nullptr)
			SDL_Metal_DestroyView(m_MetalView);
		m_MetalView = nullptr;
		m_pLayer = nullptr;
		m_pWindow = nullptr;
		m_State = EMetalBackendState::SHUTDOWN;
	}

	bool BeginRenderEncoder(const MTLClearColor &ClearColor)
	{
		if(m_RenderEncoderStarted)
			return m_CurrentRenderEncoder != nil;
		const int ActiveTargetId = m_RenderTargetState.ActiveTargetId();
		if(ActiveTargetId >= 0 && static_cast<size_t>(ActiveTargetId) < m_vRenderTargets.size())
		{
			const SRenderTarget &Target = m_vRenderTargets[ActiveTargetId];
			return Target.m_Allocated && BeginRenderEncoderForTexture(Target.m_Texture, Target.m_Width, Target.m_Height, ClearColor, MTLLoadActionLoad, false);
		}
		if(m_CurrentCommandBuffer == nil || m_pLayer == nullptr)
			return false;
		if(m_CurrentDrawable == nil)
		{
			CAMetalLayer *pLayer = (__bridge CAMetalLayer *)m_pLayer;
			m_CurrentDrawable = [pLayer nextDrawable];
		}
		if(m_CurrentDrawable == nil)
			return false;
		return BeginRenderEncoderForTexture(m_CurrentDrawable.texture, m_DrawableWidth, m_DrawableHeight, ClearColor, m_BackbufferHasContents ? MTLLoadActionLoad : MTLLoadActionClear, true);
	}

	bool BeginRenderEncoderForTexture(id<MTLTexture> Texture, uint32_t Width, uint32_t Height, const MTLClearColor &ClearColor, MTLLoadAction LoadAction, bool Backbuffer)
	{
		if(Texture == nil || Width == 0 || Height == 0 || m_CurrentCommandBuffer == nil)
			return false;
		if(m_CurrentBlitEncoder != nil)
		{
			[m_CurrentBlitEncoder endEncoding];
			m_CurrentBlitEncoder = nil;
		}
		MTLRenderPassDescriptor *pPass = [MTLRenderPassDescriptor renderPassDescriptor];
		pPass.colorAttachments[0].texture = Texture;
		pPass.colorAttachments[0].loadAction = LoadAction;
		pPass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pPass.colorAttachments[0].clearColor = ClearColor;
		m_CurrentRenderEncoder = [m_CurrentCommandBuffer renderCommandEncoderWithDescriptor:pPass];
		if(m_CurrentRenderEncoder == nil)
			return false;
		m_CurrentRenderEncoder.label = @"QmClient Metal frame";
		[m_CurrentRenderEncoder setViewport:(MTLViewport){0.0, 0.0, static_cast<double>(Width), static_cast<double>(Height), 0.0, 1.0}];
		m_RenderEncoderStarted = true;
		if(Backbuffer)
			m_BackbufferHasContents = true;
		return true;
	}

	bool UploadTextureRegion(STextureSlot &Texture, size_t X, size_t Y, size_t Width, size_t Height, const uint8_t *pData)
	{
		if(pData == nullptr || m_CurrentCommandBuffer == nil || Texture.m_Texture == nil)
			return false;
		const size_t BytesPerPixel = MetalTextureBytesPerPixel(Texture.m_Format);
		const size_t RowBytes = Width * BytesPerPixel;
		const size_t AlignedRowBytes = (RowBytes + 255) & ~size_t(255);
		std::vector<uint8_t> vUpload(AlignedRowBytes * Height, 0);
		for(size_t Row = 0; Row < Height; ++Row)
			mem_copy(vUpload.data() + Row * AlignedRowBytes, pData + Row * RowBytes, RowBytes);
		id<MTLBuffer> Staging = [m_Device newBufferWithBytes:vUpload.data() length:vUpload.size() options:MTLResourceStorageModeShared];
		if(Staging == nil)
			return false;
		if(m_CurrentRenderEncoder != nil)
		{
			[m_CurrentRenderEncoder endEncoding];
			m_CurrentRenderEncoder = nil;
			m_RenderEncoderStarted = false;
		}
		if(m_CurrentBlitEncoder == nil)
			m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
		if(m_CurrentBlitEncoder == nil)
		{
			ReleaseMetalObject(Staging);
			return false;
		}
		[m_CurrentBlitEncoder copyFromBuffer:Staging sourceOffset:0 sourceBytesPerRow:AlignedRowBytes sourceBytesPerImage:AlignedRowBytes * Height sourceSize:MTLSizeMake(Width, Height, 1) toTexture:Texture.m_Texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(X, Y, 0)];
		if(Texture.m_Staging != nil)
		{
			if(m_pStagingMemoryUsage != nullptr)
				m_pStagingMemoryUsage->fetch_sub([Texture.m_Staging length], std::memory_order_relaxed);
			ReleaseMetalObject(Texture.m_Staging);
		}
		Texture.m_Staging = RetainMetalObject(Staging);
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->fetch_add([Staging length], std::memory_order_relaxed);
		ReleaseMetalObject(Staging);
		return true;
	}

	bool UploadTextureArray(STextureSlot &Texture, const uint8_t *pData, size_t Width, size_t Height, size_t LayerStart, size_t LayerCount)
	{
		if(pData == nullptr || m_CurrentCommandBuffer == nil || Texture.m_TextureArray == nil || LayerCount == 0 || LayerStart > METAL_TEXTURE_ARRAY_LAYERS - 1 || LayerCount > METAL_TEXTURE_ARRAY_LAYERS - LayerStart || Width == 0 || Height == 0)
			return false;
		const size_t BytesPerPixel = MetalTextureBytesPerPixel(Texture.m_Format);
		if(Width > std::numeric_limits<size_t>::max() / BytesPerPixel)
			return false;
		const size_t RowBytes = Width * BytesPerPixel;
		const size_t AlignedRowBytes = (RowBytes + 255) & ~size_t(255);
		if(AlignedRowBytes < RowBytes || Height > std::numeric_limits<size_t>::max() / AlignedRowBytes)
			return false;
		const size_t SliceBytes = AlignedRowBytes * Height;
		if(LayerCount > std::numeric_limits<size_t>::max() / SliceBytes)
			return false;
		std::vector<uint8_t> vUpload(SliceBytes * LayerCount, 0);
		const size_t SourceSliceBytes = RowBytes * Height;
		for(size_t Layer = 0; Layer < LayerCount; ++Layer)
			for(size_t Row = 0; Row < Height; ++Row)
				mem_copy(vUpload.data() + Layer * SliceBytes + Row * AlignedRowBytes, pData + Layer * SourceSliceBytes + Row * RowBytes, RowBytes);
		id<MTLBuffer> Staging = [m_Device newBufferWithBytes:vUpload.data() length:vUpload.size() options:MTLResourceStorageModeShared];
		if(Staging == nil)
			return false;
		if(m_CurrentRenderEncoder != nil)
		{
			[m_CurrentRenderEncoder endEncoding];
			m_CurrentRenderEncoder = nil;
			m_RenderEncoderStarted = false;
		}
		if(m_CurrentBlitEncoder == nil)
			m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
		if(m_CurrentBlitEncoder == nil)
		{
			ReleaseMetalObject(Staging);
			return false;
		}
		for(size_t Layer = 0; Layer < LayerCount; ++Layer)
			[m_CurrentBlitEncoder copyFromBuffer:Staging sourceOffset:Layer * SliceBytes sourceBytesPerRow:AlignedRowBytes sourceBytesPerImage:SliceBytes sourceSize:MTLSizeMake(Width, Height, 1) toTexture:Texture.m_TextureArray destinationSlice:LayerStart + Layer destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		if(Texture.m_Staging != nil)
		{
			if(m_pStagingMemoryUsage != nullptr)
				m_pStagingMemoryUsage->fetch_sub([Texture.m_Staging length], std::memory_order_relaxed);
			ReleaseMetalObject(Texture.m_Staging);
		}
		Texture.m_Staging = RetainMetalObject(Staging);
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->fetch_add([Staging length], std::memory_order_relaxed);
		ReleaseMetalObject(Staging);
		return true;
	}

	bool DrawPrimitive(const CCommandBuffer::SState &State, EPrimitiveType PrimitiveType, unsigned PrimitiveCount, const GL_SVertex *pVertices)
	{
		if(pVertices == nullptr || PrimitiveCount == 0 || m_CurrentCommandBuffer == nil)
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		size_t VertexCount = 0;
		EMetalPrimitiveType MetalType = EMetalPrimitiveType::TRIANGLES;
		switch(PrimitiveType)
		{
		case EPrimitiveType::LINES: MetalType = EMetalPrimitiveType::LINES; break;
		case EPrimitiveType::QUADS: MetalType = EMetalPrimitiveType::QUADS; break;
		case EPrimitiveType::TRIANGLES: MetalType = EMetalPrimitiveType::TRIANGLES; break;
		}
		if(!MetalPrimitiveVertexCount(MetalType, PrimitiveCount, VertexCount))
			return false;
		const size_t Bytes = VertexCount * sizeof(GL_SVertex);
		SFrameSlot &Frame = m_aFrameSlots[m_CurrentFrameSlot];
		const size_t VertexOffset = (Frame.m_VertexOffset + 255) & ~size_t(255);
		if(VertexOffset > gs_StreamBufferSize || Bytes > gs_StreamBufferSize - VertexOffset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + VertexOffset, pVertices, Bytes);
		SMetalUniforms Uniforms;
		Uniforms.m_MVP = {};
		const float Width = State.m_ScreenBR.x - State.m_ScreenTL.x;
		const float Height = State.m_ScreenTL.y - State.m_ScreenBR.y;
		if(Width == 0.0f || Height == 0.0f)
			return false;
		Uniforms.m_MVP.m_v[0] = 2.0f / Width;
		Uniforms.m_MVP.m_v[5] = 2.0f / Height;
		Uniforms.m_MVP.m_v[10] = 1.0f;
		Uniforms.m_MVP.m_v[12] = -(State.m_ScreenBR.x + State.m_ScreenTL.x) / Width;
		Uniforms.m_MVP.m_v[13] = -(State.m_ScreenTL.y + State.m_ScreenBR.y) / Height;
		Uniforms.m_MVP.m_v[15] = 1.0f;
		Uniforms.m_Color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		const size_t UniformOffset = (VertexOffset + Bytes + 255) & ~size_t(255);
		if(UniformOffset > gs_StreamBufferSize || sizeof(Uniforms) > gs_StreamBufferSize - UniformOffset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + UniformOffset, &Uniforms, sizeof(Uniforms));
		const bool Textured = State.m_Texture >= 0 && static_cast<size_t>(State.m_Texture) < m_vTextureSlots.size() && m_vTextureSlots[State.m_Texture].m_Allocated;
		const EMetalBlendMode BlendMode = static_cast<EMetalBlendMode>(State.m_BlendMode);
		id<MTLRenderPipelineState> Pipeline = m_aPipelineStates[PipelineIndex(Textured, false, BlendMode)];
		if(Pipeline == nil)
			return false;
		[m_CurrentRenderEncoder setRenderPipelineState:Pipeline];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:VertexOffset atIndex:0];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:UniformOffset atIndex:1];
		if(Textured)
		{
			[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[State.m_Texture].m_Texture atIndex:0];
			[m_CurrentRenderEncoder setFragmentSamplerState:State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
		}
		SetScissor(State);
		if(PrimitiveType == EPrimitiveType::QUADS)
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:PrimitiveCount * 6 indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:0];
		else
			[m_CurrentRenderEncoder drawPrimitives:PrimitiveType == EPrimitiveType::LINES ? MTLPrimitiveTypeLine : MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];
		Frame.m_VertexOffset = UniformOffset + sizeof(Uniforms);
		return true;
	}

	bool DrawTex3D(const CCommandBuffer::SCommand_RenderTex3D &Command)
	{
		if(Command.m_pVertices == nullptr || Command.m_PrimCount == 0 || Command.m_State.m_Texture < 0 || static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated || m_vTextureSlots[Command.m_State.m_Texture].m_TextureArray == nil)
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		size_t VertexCount = 0;
		EMetalPrimitiveType MetalType = EMetalPrimitiveType::TRIANGLES;
		switch(Command.m_PrimType)
		{
		case EPrimitiveType::LINES: MetalType = EMetalPrimitiveType::LINES; break;
		case EPrimitiveType::QUADS: MetalType = EMetalPrimitiveType::QUADS; break;
		case EPrimitiveType::TRIANGLES: MetalType = EMetalPrimitiveType::TRIANGLES; break;
		}
		if(!MetalPrimitiveVertexCount(MetalType, Command.m_PrimCount, VertexCount) || VertexCount > std::numeric_limits<size_t>::max() / sizeof(GL_SVertexTex3DStream))
			return false;
		const size_t Bytes = VertexCount * sizeof(GL_SVertexTex3DStream);
		SFrameSlot &Frame = m_aFrameSlots[m_CurrentFrameSlot];
		const size_t VertexOffset = (Frame.m_VertexOffset + 255) & ~size_t(255);
		if(VertexOffset > gs_StreamBufferSize || Bytes > gs_StreamBufferSize - VertexOffset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + VertexOffset, Command.m_pVertices, Bytes);
		SMetalUniforms Uniforms;
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		size_t UniformOffset = 0;
		const EMetalBlendMode BlendMode = static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode);
		if(static_cast<size_t>(BlendMode) > static_cast<size_t>(EMetalBlendMode::ADDITIVE))
			return false;
		const size_t Pipeline = TextureArrayPipelineIndex(BlendMode);
		if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || Pipeline >= m_aPipelineStates.size() || m_aPipelineStates[Pipeline] == nil)
			return false;
		[m_CurrentRenderEncoder setRenderPipelineState:m_aPipelineStates[Pipeline]];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:VertexOffset atIndex:0];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:UniformOffset atIndex:1];
		[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_TextureArray atIndex:0];
		[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
		SetScissor(Command.m_State);
		if(Command.m_PrimType == EPrimitiveType::QUADS)
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:static_cast<NSUInteger>(Command.m_PrimCount) * 6 indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:0];
		else
			[m_CurrentRenderEncoder drawPrimitives:Command.m_PrimType == EPrimitiveType::LINES ? MTLPrimitiveTypeLine : MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:static_cast<NSUInteger>(VertexCount)];
		Frame.m_VertexOffset = UniformOffset + sizeof(Uniforms);
		return true;
	}

	bool BuildMvp(const CCommandBuffer::SState &State, SMetalFloat4x4 &Mvp) const
	{
		Mvp = {};
		const float Width = State.m_ScreenBR.x - State.m_ScreenTL.x;
		const float Height = State.m_ScreenTL.y - State.m_ScreenBR.y;
		if(Width == 0.0f || Height == 0.0f)
			return false;
		Mvp.m_v[0] = 2.0f / Width;
		Mvp.m_v[5] = 2.0f / Height;
		Mvp.m_v[10] = 1.0f;
		Mvp.m_v[12] = -(State.m_ScreenBR.x + State.m_ScreenTL.x) / Width;
		Mvp.m_v[13] = -(State.m_ScreenTL.y + State.m_ScreenBR.y) / Height;
		Mvp.m_v[15] = 1.0f;
		return true;
	}

	bool AllocateUniformData(const void *pData, size_t DataBytes, size_t &Offset)
	{
		if(pData == nullptr || DataBytes == 0)
			return false;
		SFrameSlot &Frame = m_aFrameSlots[m_CurrentFrameSlot];
		Offset = (Frame.m_VertexOffset + 255) & ~size_t(255);
		if(Offset > gs_StreamBufferSize || DataBytes > gs_StreamBufferSize - Offset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + Offset, pData, DataBytes);
		Frame.m_VertexOffset = Offset + DataBytes;
		return true;
	}

	bool GetContainerDrawResources(int ContainerIndex, size_t RequiredVertexCount, const SBufferContainerSlot *&pContainer, SBufferSlot *&pBuffer)
	{
		if(ContainerIndex < 0 || static_cast<size_t>(ContainerIndex) >= m_vBufferContainers.size())
			return false;
		const SBufferContainerSlot &Container = m_vBufferContainers[ContainerIndex];
		if(!Container.m_Allocated || Container.m_Stride <= 0 || Container.m_VertBufferBindingIndex < 0 || static_cast<size_t>(Container.m_VertBufferBindingIndex) >= m_vBufferSlots.size())
			return false;
		SBufferSlot &Buffer = m_vBufferSlots[Container.m_VertBufferBindingIndex];
		if(!Buffer.m_Allocated || Buffer.m_Buffer == nil || RequiredVertexCount > Buffer.m_DataBytes / static_cast<size_t>(Container.m_Stride))
			return false;
		pContainer = &Container;
		pBuffer = &Buffer;
		return true;
	}

	static bool MatchesContainerAttribute(const SBufferContainerInfo::SAttribute &Attribute, uint32_t DataTypeCount, uint32_t Type, bool Normalized, size_t Offset, uint32_t FuncType)
	{
		return MetalVertexAttributeEquals(SMetalVertexAttribute{static_cast<uint32_t>(Attribute.m_DataTypeCount), Attribute.m_Type, Attribute.m_Normalized, reinterpret_cast<uintptr_t>(Attribute.m_pOffset), Attribute.m_FuncType}, DataTypeCount, Type, Normalized, Offset, FuncType);
	}

	static bool MatchesContainerLayout(const SBufferContainerSlot &Container, bool Textured, bool Quad)
	{
		if(Quad)
		{
			if(Container.m_vAttributes.size() != (Textured ? 3 : 2) || Container.m_Stride != static_cast<int>(Textured ? sizeof(float) * 4 + sizeof(uint8_t) * 4 + sizeof(float) * 2 : sizeof(float) * 4 + sizeof(uint8_t) * 4))
				return false;
			if(!MatchesContainerAttribute(Container.m_vAttributes[0], 4, METAL_GRAPHICS_TYPE_FLOAT, false, 0, 0) || !MatchesContainerAttribute(Container.m_vAttributes[1], 4, METAL_GRAPHICS_TYPE_UNSIGNED_BYTE, true, sizeof(float) * 4, 0))
				return false;
			return !Textured || MatchesContainerAttribute(Container.m_vAttributes[2], 2, METAL_GRAPHICS_TYPE_FLOAT, false, sizeof(float) * 4 + sizeof(uint8_t) * 4, 0);
		}
		if(Container.m_vAttributes.size() != (Textured ? 2 : 1) || Container.m_Stride != static_cast<int>(Textured ? sizeof(float) * 2 + sizeof(uint8_t) * 4 : sizeof(float) * 2))
			return false;
		if(!MatchesContainerAttribute(Container.m_vAttributes[0], 2, METAL_GRAPHICS_TYPE_FLOAT, false, 0, 0))
			return false;
		return !Textured || MatchesContainerAttribute(Container.m_vAttributes[1], 4, METAL_GRAPHICS_TYPE_UNSIGNED_BYTE, false, sizeof(float) * 2, 1);
	}

	static bool MatchesStandardVertexLayout(const SBufferContainerSlot &Container)
	{
		return Container.m_vAttributes.size() == 3 && Container.m_Stride == static_cast<int>(sizeof(GL_SVertex)) &&
			MatchesContainerAttribute(Container.m_vAttributes[0], 2, METAL_GRAPHICS_TYPE_FLOAT, false, offsetof(GL_SVertex, m_Pos), 0) &&
			MatchesContainerAttribute(Container.m_vAttributes[1], 2, METAL_GRAPHICS_TYPE_FLOAT, false, offsetof(GL_SVertex, m_Tex), 0) &&
			MatchesContainerAttribute(Container.m_vAttributes[2], 4, METAL_GRAPHICS_TYPE_UNSIGNED_BYTE, true, offsetof(GL_SVertex, m_Color), 0);
	}

	void SetScissor(const CCommandBuffer::SState &State)
	{
		if(m_CurrentRenderEncoder == nil)
			return;
		uint32_t Width = m_DrawableWidth;
		uint32_t Height = m_DrawableHeight;
		const int ActiveTargetId = m_RenderTargetState.ActiveTargetId();
		if(ActiveTargetId >= 0 && static_cast<size_t>(ActiveTargetId) < m_vRenderTargets.size())
		{
			const SRenderTarget &Target = m_vRenderTargets[ActiveTargetId];
			Width = Target.m_Width;
			Height = Target.m_Height;
		}
		if(!State.m_ClipEnable)
		{
			[m_CurrentRenderEncoder setScissorRect:MTLScissorRect{0, 0, Width, Height}];
			return;
		}
		const auto ClampCoordinate = [](long long Value, uint32_t Dimension) {
			return std::clamp(Value, 0LL, static_cast<long long>(Dimension));
		};
		const long long ClipLeft = ClampCoordinate(State.m_ClipX, Width);
		const long long ClipRight = ClampCoordinate(static_cast<long long>(State.m_ClipX) + std::max(State.m_ClipW, 0), Width);
		const long long ClipTop = ClampCoordinate(State.m_ClipY, Height);
		const long long ClipBottom = ClampCoordinate(static_cast<long long>(State.m_ClipY) + std::max(State.m_ClipH, 0), Height);
		[m_CurrentRenderEncoder setScissorRect:MTLScissorRect{
			static_cast<NSUInteger>(ClipLeft),
			static_cast<NSUInteger>(static_cast<long long>(Height) - ClipBottom),
			static_cast<NSUInteger>(std::max(ClipRight - ClipLeft, 0LL)),
			static_cast<NSUInteger>(std::max(ClipBottom - ClipTop, 0LL))}];
	}

	bool PrepareContainerPipeline(const CCommandBuffer::SState &State, size_t Pipeline, SBufferSlot &Buffer, size_t UniformOffset)
	{
		if(Pipeline >= m_aPipelineStates.size() || static_cast<size_t>(State.m_BlendMode) > static_cast<size_t>(EMetalBlendMode::ADDITIVE) || m_aPipelineStates[Pipeline] == nil)
			return false;
		[m_CurrentRenderEncoder setRenderPipelineState:m_aPipelineStates[Pipeline]];
		[m_CurrentRenderEncoder setVertexBuffer:Buffer.m_Buffer offset:0 atIndex:0];
		[m_CurrentRenderEncoder setVertexBuffer:m_aFrameSlots[m_CurrentFrameSlot].m_VertexBuffer offset:UniformOffset atIndex:1];
		SetScissor(State);
		return true;
	}

	static bool DecodeQuadIndexRange(const char *pIndicesOffset, unsigned int DrawCount, size_t &QuadOffset, size_t &QuadCount)
	{
		const size_t OffsetBytes = static_cast<size_t>(reinterpret_cast<uintptr_t>(pIndicesOffset));
		if(OffsetBytes % (6 * sizeof(uint32_t)) != 0 || DrawCount % 6 != 0)
			return false;
		QuadOffset = OffsetBytes / (6 * sizeof(uint32_t));
		QuadCount = DrawCount / 6;
		return true;
	}

	bool DrawTileLayer(const CCommandBuffer::SCommand_RenderTileLayer &Command, bool Border, const vec2 &Offset, const vec2 &Scale)
	{
		if(Command.m_IndicesDrawNum <= 0 || Command.m_pIndicesOffsets == nullptr || Command.m_pDrawCount == nullptr)
			return true;
		const bool Textured = Command.m_State.m_Texture >= 0;
		if(Textured && (static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated))
			return false;
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(!GetContainerDrawResources(Command.m_BufferContainerIndex, 1, pContainer, pBuffer) || !MatchesContainerLayout(*pContainer, Textured, false))
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		SMetalTileUniforms Uniforms;
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{Command.m_Color.r, Command.m_Color.g, Command.m_Color.b, Command.m_Color.a}};
		Uniforms.m_Transform = Border ? SMetalFloat4{{Offset.x, Offset.y, Scale.x, Scale.y}} : SMetalFloat4{{0.0f, 0.0f, 1.0f, 1.0f}};
		size_t UniformOffset = 0;
		if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, TilePipelineIndex(Textured, Border, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
			return false;
		[m_CurrentRenderEncoder setFragmentBuffer:m_aFrameSlots[m_CurrentFrameSlot].m_VertexBuffer offset:UniformOffset atIndex:1];
		if(Textured)
		{
			[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_Texture atIndex:0];
			[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
		}
		for(int Index = 0; Index < Command.m_IndicesDrawNum; ++Index)
		{
			size_t QuadOffset = 0;
			size_t QuadCount = 0;
			if(!DecodeQuadIndexRange(Command.m_pIndicesOffsets[Index], Command.m_pDrawCount[Index], QuadOffset, QuadCount) || QuadCount == 0 || QuadOffset > std::numeric_limits<size_t>::max() / 4 || QuadCount > std::numeric_limits<size_t>::max() / 4 || QuadOffset * 4 > pBuffer->m_DataBytes / static_cast<size_t>(pContainer->m_Stride) || QuadCount * 4 > pBuffer->m_DataBytes / static_cast<size_t>(pContainer->m_Stride) - QuadOffset * 4 || (QuadOffset + QuadCount) * 6 > [m_QuadIndexBuffer length] / sizeof(uint16_t))
				return false;
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:static_cast<NSUInteger>(QuadCount * 6) indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:QuadOffset * 6 * sizeof(uint16_t)];
		}
		return true;
	}

	bool DrawBorderTile(const CCommandBuffer::SCommand_RenderBorderTile &Command)
	{
		if(Command.m_DrawNum > std::numeric_limits<unsigned int>::max() / 6)
			return false;
		CCommandBuffer::SCommand_RenderTileLayer Tile;
		unsigned int DrawCount = Command.m_DrawNum * 6;
		Tile.m_State = Command.m_State;
		Tile.m_Color = Command.m_Color;
		Tile.m_pIndicesOffsets = const_cast<char **>(&Command.m_pIndicesOffset);
		Tile.m_pDrawCount = &DrawCount;
		Tile.m_IndicesDrawNum = 1;
		Tile.m_BufferContainerIndex = Command.m_BufferContainerIndex;
		return DrawTileLayer(Tile, true, Command.m_Offset, Command.m_Scale);
	}

	bool DrawQuadLayer(const CCommandBuffer::SCommand_RenderQuadLayer &Command, bool Grouped)
	{
		if(Command.m_QuadNum == 0 || Command.m_pQuadInfo == nullptr || Command.m_QuadNum > std::numeric_limits<size_t>::max() / 4)
			return Command.m_QuadNum == 0;
		const bool Textured = Command.m_State.m_Texture >= 0;
		if(Textured && (static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated))
			return false;
		const size_t QuadOffset = Command.m_QuadOffset < 0 ? std::numeric_limits<size_t>::max() : static_cast<size_t>(Command.m_QuadOffset);
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(QuadOffset == std::numeric_limits<size_t>::max() || Command.m_QuadNum > std::numeric_limits<size_t>::max() - QuadOffset || QuadOffset + Command.m_QuadNum > std::numeric_limits<size_t>::max() / 4 || !GetContainerDrawResources(Command.m_BufferContainerIndex, (QuadOffset + Command.m_QuadNum) * 4, pContainer, pBuffer) || !MatchesContainerLayout(*pContainer, Textured, true))
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		const size_t MaxQuads = Grouped ? Command.m_QuadNum : std::min(Command.m_QuadNum, static_cast<size_t>(METAL_MAX_QUADS));
		for(size_t RenderOffset = 0; RenderOffset < Command.m_QuadNum; RenderOffset += Grouped ? Command.m_QuadNum : MaxQuads)
		{
			const size_t RenderCount = Grouped ? Command.m_QuadNum : std::min(MaxQuads, Command.m_QuadNum - RenderOffset);
			SMetalQuadUniforms Uniforms;
			if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
				return false;
			const size_t InfoIndex = Grouped ? 0 : RenderOffset;
			for(size_t Index = 0; Index < (Grouped ? 1 : RenderCount); ++Index)
			{
				const SQuadRenderInfo &Info = Command.m_pQuadInfo[InfoIndex + Index];
				Uniforms.m_aColors[Index] = {{Info.m_Color.r, Info.m_Color.g, Info.m_Color.b, Info.m_Color.a}};
				Uniforms.m_aOffsetsRotations[Index] = {{Info.m_Offsets.x, Info.m_Offsets.y, Info.m_Rotation, 0.0f}};
			}
			Uniforms.m_QuadOffset = static_cast<int>(QuadOffset + RenderOffset);
			size_t UniformOffset = 0;
			if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, QuadPipelineIndex(Textured, Grouped, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
				return false;
			if(Textured)
			{
				[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_Texture atIndex:0];
				[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
			}
			if((QuadOffset + RenderOffset + RenderCount) * 6 > [m_QuadIndexBuffer length] / sizeof(uint16_t))
				return false;
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:static_cast<NSUInteger>(RenderCount * 6) indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:(QuadOffset + RenderOffset) * 6 * sizeof(uint16_t)];
			if(Grouped)
				break;
		}
		return true;
	}

	bool GetStandardQuadDrawResources(int ContainerIndex, const void *pOffset, unsigned int DrawNum, size_t &QuadOffset, size_t &QuadCount, const SBufferContainerSlot *&pContainer, SBufferSlot *&pBuffer)
	{
		if(!DecodeQuadIndexRange(static_cast<const char *>(pOffset), DrawNum, QuadOffset, QuadCount) || QuadCount == 0 || QuadOffset > std::numeric_limits<size_t>::max() - QuadCount || QuadOffset + QuadCount > std::numeric_limits<size_t>::max() / 4)
			return false;
		const size_t QuadEnd = QuadOffset + QuadCount;
		if(QuadEnd > std::numeric_limits<size_t>::max() / 6 || QuadEnd * 6 > [m_QuadIndexBuffer length] / sizeof(uint16_t))
			return false;
		if(!GetContainerDrawResources(ContainerIndex, QuadEnd * 4, pContainer, pBuffer) || !MatchesStandardVertexLayout(*pContainer))
			return false;
		return true;
	}

	bool DrawQuadContainer(const CCommandBuffer::SCommand_RenderQuadContainer &Command)
	{
		if(Command.m_DrawNum == 0)
			return true;
		const bool Textured = Command.m_State.m_Texture >= 0;
		if(Textured && (static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated))
			return false;
		size_t QuadOffset = 0;
		size_t QuadCount = 0;
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(!GetStandardQuadDrawResources(Command.m_BufferContainerIndex, Command.m_pOffset, Command.m_DrawNum, QuadOffset, QuadCount, pContainer, pBuffer))
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		SMetalUniforms Uniforms;
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		size_t UniformOffset = 0;
		if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, PipelineIndex(Textured, false, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
			return false;
		if(Textured)
		{
			[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_Texture atIndex:0];
			[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
		}
		[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:Command.m_DrawNum indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:QuadOffset * 6 * sizeof(uint16_t)];
		(void)pContainer;
		(void)QuadCount;
		return true;
	}

	bool DrawText(const CCommandBuffer::SCommand_RenderText &Command)
	{
		if(Command.m_DrawNum == 0)
			return true;
		if(Command.m_DrawNum < 0 || Command.m_TextureSize <= 0 || Command.m_BufferContainerIndex < 0)
			return false;
		if(Command.m_TextTextureIndex < 0 || Command.m_TextOutlineTextureIndex < 0 || static_cast<size_t>(Command.m_TextTextureIndex) >= m_vTextureSlots.size() || static_cast<size_t>(Command.m_TextOutlineTextureIndex) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_TextTextureIndex].m_Allocated || !m_vTextureSlots[Command.m_TextOutlineTextureIndex].m_Allocated)
			return false;
		size_t QuadOffset = 0;
		size_t QuadCount = 0;
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(!GetStandardQuadDrawResources(Command.m_BufferContainerIndex, nullptr, static_cast<unsigned int>(Command.m_DrawNum), QuadOffset, QuadCount, pContainer, pBuffer))
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		SMetalTextUniforms Uniforms;
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{Command.m_TextColor.r, Command.m_TextColor.g, Command.m_TextColor.b, Command.m_TextColor.a}};
		Uniforms.m_OutlineColor = {{Command.m_TextOutlineColor.r, Command.m_TextOutlineColor.g, Command.m_TextOutlineColor.b, Command.m_TextOutlineColor.a}};
		Uniforms.m_Params = {{static_cast<float>(Command.m_TextureSize), 0.0f, 0.0f, 0.0f}};
		size_t UniformOffset = 0;
		if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, PipelineIndex(true, true, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
			return false;
		[m_CurrentRenderEncoder setFragmentBuffer:m_aFrameSlots[m_CurrentFrameSlot].m_VertexBuffer offset:UniformOffset atIndex:1];
		[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_TextTextureIndex].m_Texture atIndex:0];
		[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_TextOutlineTextureIndex].m_Texture atIndex:1];
		[m_CurrentRenderEncoder setFragmentSamplerState:m_ClampSampler atIndex:0];
		[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:static_cast<NSUInteger>(Command.m_DrawNum) indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:QuadOffset * 6 * sizeof(uint16_t)];
		(void)pContainer;
		(void)QuadCount;
		return true;
	}

	bool DrawQuadContainerEx(const CCommandBuffer::SCommand_RenderQuadContainerEx &Command)
	{
		if(Command.m_DrawNum == 0)
			return true;
		const bool Textured = Command.m_State.m_Texture >= 0;
		if(Textured && (static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated))
			return false;
		size_t QuadOffset = 0;
		size_t QuadCount = 0;
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(!GetStandardQuadDrawResources(Command.m_BufferContainerIndex, Command.m_pOffset, Command.m_DrawNum, QuadOffset, QuadCount, pContainer, pBuffer))
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		SMetalQuadContainerUniforms Uniforms;
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_CenterRotation = {{Command.m_Center.x, Command.m_Center.y, Command.m_Rotation, 0.0f}};
		Uniforms.m_VertexColor = {{Command.m_VertexColor.r, Command.m_VertexColor.g, Command.m_VertexColor.b, Command.m_VertexColor.a}};
		size_t UniformOffset = 0;
		if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, QuadContainerExPipelineIndex(Textured, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
			return false;
		if(Textured)
		{
			[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_Texture atIndex:0];
			[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
		}
		[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:Command.m_DrawNum indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:QuadOffset * 6 * sizeof(uint16_t)];
		(void)pContainer;
		(void)QuadCount;
		return true;
	}

	bool DrawQuadContainerAsSpriteMultiple(const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple &Command)
	{
		if(Command.m_DrawNum == 0 || Command.m_DrawCount == 0)
			return true;
		if(Command.m_pRenderInfo == nullptr)
			return false;
		const bool Textured = Command.m_State.m_Texture >= 0;
		if(Textured && (static_cast<size_t>(Command.m_State.m_Texture) >= m_vTextureSlots.size() || !m_vTextureSlots[Command.m_State.m_Texture].m_Allocated))
			return false;
		size_t QuadOffset = 0;
		size_t QuadCount = 0;
		const SBufferContainerSlot *pContainer = nullptr;
		SBufferSlot *pBuffer = nullptr;
		if(!GetStandardQuadDrawResources(Command.m_BufferContainerIndex, Command.m_pOffset, Command.m_DrawNum, QuadOffset, QuadCount, pContainer, pBuffer) || QuadCount != 1)
			return false;
		if(!BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		SMetalSpriteMultipleUniforms Uniforms{};
		if(!BuildMvp(Command.m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Center = {{Command.m_Center.x, Command.m_Center.y, 0.0f, 0.0f}};
		Uniforms.m_VertexColor = {{Command.m_VertexColor.r, Command.m_VertexColor.g, Command.m_VertexColor.b, Command.m_VertexColor.a}};
		for(unsigned int RenderOffset = 0; RenderOffset < Command.m_DrawCount;)
		{
			const unsigned int RenderCount = std::min(Command.m_DrawCount - RenderOffset, static_cast<unsigned int>(METAL_MAX_SPRITES));
			for(unsigned int Index = 0; Index < RenderCount; ++Index)
			{
				const IGraphics::SRenderSpriteInfo &Info = Command.m_pRenderInfo[RenderOffset + Index];
				Uniforms.m_aRenderInfo[Index] = {{Info.m_Pos.x, Info.m_Pos.y, Info.m_Scale, Info.m_Rotation}};
			}
			size_t UniformOffset = 0;
			if(!AllocateUniformData(&Uniforms, sizeof(Uniforms), UniformOffset) || !PrepareContainerPipeline(Command.m_State, SpriteMultiplePipelineIndex(Textured, static_cast<EMetalBlendMode>(Command.m_State.m_BlendMode)), *pBuffer, UniformOffset))
				return false;
			if(Textured)
			{
				[m_CurrentRenderEncoder setFragmentTexture:m_vTextureSlots[Command.m_State.m_Texture].m_Texture atIndex:0];
				[m_CurrentRenderEncoder setFragmentSamplerState:Command.m_State.m_WrapMode == EWrapMode::CLAMP ? m_ClampSampler : m_RepeatSampler atIndex:0];
			}
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:Command.m_DrawNum indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:QuadOffset * 6 * sizeof(uint16_t) instanceCount:RenderCount baseVertex:0 baseInstance:0];
			RenderOffset += RenderCount;
		}
		(void)pContainer;
		return true;
	}

	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
	{
		EndActiveEncoders();
		const MTLClearColor ClearColor = MTLClearColorMake(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0);
		const int ActiveTargetId = m_RenderTargetState.ActiveTargetId();
		if(ActiveTargetId >= 0 && static_cast<size_t>(ActiveTargetId) < m_vRenderTargets.size())
		{
			const SRenderTarget &Target = m_vRenderTargets[ActiveTargetId];
			if(Target.m_Allocated && BeginRenderEncoderForTexture(Target.m_Texture, Target.m_Width, Target.m_Height, ClearColor, MTLLoadActionClear, false))
				return;
		}
		else
		{
			m_BackbufferHasContents = false;
			if(BeginRenderEncoder(ClearColor))
				return;
		}
		if(m_CurrentRenderEncoder == nil)
			SetUnsupportedCommandError(pCommand);
	}

	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
	{
		if(!DrawPrimitive(pCommand->m_State, pCommand->m_PrimType, pCommand->m_PrimCount, pCommand->m_pVertices))
			SetUnsupportedCommandError(pCommand);
	}

	bool Cmd_RenderTarget_Create(const CCommandBuffer::SCommand_RenderTarget_Create *pCommand)
	{
		if(pCommand->m_TargetId < 0 || pCommand->m_Width <= 0 || pCommand->m_Height <= 0)
			return true;
		return CreateRenderTarget(pCommand->m_TargetId, pCommand->m_Width, pCommand->m_Height);
	}

	bool Cmd_RenderTarget_Destroy(const CCommandBuffer::SCommand_RenderTarget_Destroy *pCommand)
	{
		if(pCommand->m_TargetId < 0 || static_cast<size_t>(pCommand->m_TargetId) >= m_vRenderTargets.size() || !m_RenderTargetState.CanDestroy(pCommand->m_TargetId))
			return true;
		DestroyRenderTarget(pCommand->m_TargetId);
		return true;
	}

	bool Cmd_RenderTarget_Begin(const CCommandBuffer::SCommand_RenderTarget_Begin *pCommand)
	{
		if(pCommand->m_TargetId < 0 || static_cast<size_t>(pCommand->m_TargetId) >= m_vRenderTargets.size() || m_RenderTargetState.IsActive())
			return true;
		const SRenderTarget &Target = m_vRenderTargets[pCommand->m_TargetId];
		if(!Target.m_Allocated || Target.m_Texture == nil)
			return true;
		EndActiveEncoders();
		if(!m_RenderTargetState.Begin(pCommand->m_TargetId))
			return true;
		if(BeginRenderEncoderForTexture(Target.m_Texture, Target.m_Width, Target.m_Height, MTLClearColorMake(pCommand->m_ClearColor.r, pCommand->m_ClearColor.g, pCommand->m_ClearColor.b, pCommand->m_ClearColor.a), MTLLoadActionClear, false))
			return true;
		m_RenderTargetState.Reset();
		return false;
	}

	bool Cmd_RenderTarget_End(const CCommandBuffer::SCommand_RenderTarget_End *pCommand)
	{
		(void)pCommand;
		const int ActiveTargetId = m_RenderTargetState.ActiveTargetId();
		if(ActiveTargetId < 0 || static_cast<size_t>(ActiveTargetId) >= m_vRenderTargets.size())
			return true;
		EndActiveEncoders();
		m_RenderTargetState.End();
		return true;
	}

	bool Cmd_RenderTarget_Draw(const CCommandBuffer::SCommand_RenderTarget_Draw *pCommand)
	{
		if(pCommand->m_TargetId < 0 || static_cast<size_t>(pCommand->m_TargetId) >= m_vRenderTargets.size() || pCommand->m_W <= 0.0f || pCommand->m_H <= 0.0f || pCommand->m_pVertices == nullptr || pCommand->m_PrimCount == 0 || !m_RenderTargetState.CanDraw(pCommand->m_TargetId))
			return true;
		const size_t PrimCount = static_cast<size_t>(pCommand->m_PrimCount);
		if(PrimCount > std::numeric_limits<size_t>::max() / (4 * sizeof(CCommandBuffer::SVertex)))
			return false;
		const SRenderTarget &Target = m_vRenderTargets[pCommand->m_TargetId];
		if(!Target.m_Allocated || Target.m_Texture == nil || m_CurrentCommandBuffer == nil)
			return false;
		const size_t VertexBytes = PrimCount * 4 * sizeof(CCommandBuffer::SVertex);
		SFrameSlot &Frame = m_aFrameSlots[m_CurrentFrameSlot];
		const size_t VertexOffset = (Frame.m_VertexOffset + 255) & ~size_t(255);
		if(VertexOffset > gs_StreamBufferSize || VertexBytes > gs_StreamBufferSize - VertexOffset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + VertexOffset, pCommand->m_pVertices, VertexBytes);
		SMetalUniforms Uniforms;
		if(!BuildMvp(pCommand->m_State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		const size_t UniformOffset = (VertexOffset + VertexBytes + 255) & ~size_t(255);
		const EMetalBlendMode BlendMode = static_cast<EMetalBlendMode>(pCommand->m_State.m_BlendMode);
		if(UniformOffset > gs_StreamBufferSize || sizeof(Uniforms) > gs_StreamBufferSize - UniformOffset || static_cast<size_t>(BlendMode) > static_cast<size_t>(EMetalBlendMode::ADDITIVE))
			return false;
		id<MTLRenderPipelineState> Pipeline = m_aPipelineStates[PipelineIndex(true, false, BlendMode)];
		if(Pipeline == nil || !BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + UniformOffset, &Uniforms, sizeof(Uniforms));
		[m_CurrentRenderEncoder setRenderPipelineState:Pipeline];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:VertexOffset atIndex:0];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:UniformOffset atIndex:1];
		[m_CurrentRenderEncoder setFragmentTexture:Target.m_Texture atIndex:0];
		[m_CurrentRenderEncoder setFragmentSamplerState:m_ClampSampler atIndex:0];
		SetScissor(pCommand->m_State);
		[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:static_cast<NSUInteger>(pCommand->m_PrimCount) * 6 indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:0];
		Frame.m_VertexOffset = UniformOffset + sizeof(Uniforms);
		return true;
	}

	bool Cmd_RenderTarget_CaptureBackbuffer(const CCommandBuffer::SCommand_RenderTarget_CaptureBackbuffer *pCommand)
	{
		if(m_RenderTargetState.IsActive() || pCommand->m_TargetId < 0 || static_cast<size_t>(pCommand->m_TargetId) >= m_vRenderTargets.size())
			return true;
		const SRenderTarget &Target = m_vRenderTargets[pCommand->m_TargetId];
		if(!Target.m_Allocated || Target.m_Texture == nil || Target.m_Width == 0 || Target.m_Height == 0)
			return true;

		std::array<CCommandBuffer::SVertex, 4> aVertices{};
		aVertices[0].m_Pos = vec2(0.0f, 0.0f);
		aVertices[0].m_Tex = vec2(0.0f, 0.0f);
		aVertices[1].m_Pos = vec2(static_cast<float>(Target.m_Width), 0.0f);
		aVertices[1].m_Tex = vec2(1.0f, 0.0f);
		aVertices[2].m_Pos = vec2(static_cast<float>(Target.m_Width), static_cast<float>(Target.m_Height));
		aVertices[2].m_Tex = vec2(1.0f, 1.0f);
		aVertices[3].m_Pos = vec2(0.0f, static_cast<float>(Target.m_Height));
		aVertices[3].m_Tex = vec2(0.0f, 1.0f);
		for(CCommandBuffer::SVertex &Vertex : aVertices)
			Vertex.m_Color = CCommandBuffer::SColor{255, 255, 255, 255};

		SFrameSlot &Frame = m_aFrameSlots[m_CurrentFrameSlot];
		const size_t VertexOffset = (Frame.m_VertexOffset + 255) & ~size_t(255);
		const size_t VertexBytes = sizeof(aVertices);
		const size_t UniformOffset = (VertexOffset + VertexBytes + 255) & ~size_t(255);
		if(VertexOffset > gs_StreamBufferSize || VertexBytes > gs_StreamBufferSize - VertexOffset || UniformOffset > gs_StreamBufferSize || sizeof(SMetalUniforms) > gs_StreamBufferSize - UniformOffset)
			return false;
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + VertexOffset, aVertices.data(), VertexBytes);
		SMetalUniforms Uniforms;
		CCommandBuffer::SState State{};
		State.m_ScreenTL = vec2(0.0f, 0.0f);
		State.m_ScreenBR = vec2(static_cast<float>(Target.m_Width), static_cast<float>(Target.m_Height));
		if(!BuildMvp(State, Uniforms.m_MVP))
			return false;
		Uniforms.m_Color = {{1.0f, 1.0f, 1.0f, 1.0f}};
		mem_copy(static_cast<uint8_t *>(Frame.m_VertexBuffer.contents) + UniformOffset, &Uniforms, sizeof(Uniforms));
		id<MTLRenderPipelineState> Pipeline = m_aPipelineStates[PipelineIndex(true, false, EMetalBlendMode::NONE)];
		if(Pipeline == nil || m_CurrentCommandBuffer == nil)
			return false;
		if(m_CurrentDrawable == nil && !BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return false;
		EndActiveEncoders();
		if(m_CurrentDrawable == nil || !BeginRenderEncoderForTexture(Target.m_Texture, Target.m_Width, Target.m_Height, MTLClearColorMake(0.0, 0.0, 0.0, 1.0), MTLLoadActionClear, false))
			return false;
		[m_CurrentRenderEncoder setRenderPipelineState:Pipeline];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:VertexOffset atIndex:0];
		[m_CurrentRenderEncoder setVertexBuffer:Frame.m_VertexBuffer offset:UniformOffset atIndex:1];
		[m_CurrentRenderEncoder setFragmentTexture:m_CurrentDrawable.texture atIndex:0];
		[m_CurrentRenderEncoder setFragmentSamplerState:m_ClampSampler atIndex:0];
		[m_CurrentRenderEncoder setScissorRect:MTLScissorRect{0, 0, Target.m_Width, Target.m_Height}];
		[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:6 indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:0];
		Frame.m_VertexOffset = UniformOffset + sizeof(Uniforms);
		EndActiveEncoders();
		return true;
	}

	void EndActiveEncoders()
	{
		if(m_CurrentRenderEncoder != nil)
		{
			[m_CurrentRenderEncoder endEncoding];
			m_CurrentRenderEncoder = nil;
			m_RenderEncoderStarted = false;
		}
		if(m_CurrentBlitEncoder != nil)
		{
			[m_CurrentBlitEncoder endEncoding];
			m_CurrentBlitEncoder = nil;
		}
	}

	bool CommitCurrentFrame(bool Present, bool WaitForCompletion)
	{
		if(m_CurrentCommandBuffer == nil || m_CommandBufferCommitted)
			return false;
		EndActiveEncoders();
		const CMetalFrameState::EFinalizeResult Result = m_FrameState.FinalizeFrameForPresent(Present && m_CurrentDrawable != nil);
		if(Result == CMetalFrameState::EFinalizeResult::PRESENTED)
			[m_CurrentCommandBuffer presentDrawable:m_CurrentDrawable];
		[m_CurrentCommandBuffer commit];
		m_CommandBufferCommitted = true;
		if(!WaitForCompletion)
			return Result == CMetalFrameState::EFinalizeResult::PRESENTED;
		[m_CurrentCommandBuffer waitUntilCompleted];
		return Result == CMetalFrameState::EFinalizeResult::PRESENTED && m_CurrentCommandBuffer.status == MTLCommandBufferStatusCompleted;
	}

	id<MTLBuffer> EncodeDrawableReadback(size_t Width, size_t Height, size_t &RowBytes)
	{
		if(m_CurrentCommandBuffer == nil || Width == 0 || Height == 0)
			return nil;
		if(m_CurrentDrawable == nil && !BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
			return nil;
		EndActiveEncoders();
		size_t UnalignedRowBytes = 0;
		if(!MetalCheckedMul(Width, 4, UnalignedRowBytes) || UnalignedRowBytes > std::numeric_limits<size_t>::max() - 255)
			return nil;
		RowBytes = (UnalignedRowBytes + 255) & ~size_t(255);
		if(Height > 0 && RowBytes > std::numeric_limits<size_t>::max() / Height)
			return nil;
		id<MTLBuffer> Readback = [m_Device newBufferWithLength:RowBytes * Height options:MTLResourceStorageModeShared];
		if(Readback == nil)
			return nil;
		m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
		if(m_CurrentBlitEncoder == nil)
		{
			ReleaseMetalObject(Readback);
			return nil;
		}
		[m_CurrentBlitEncoder copyFromTexture:m_CurrentDrawable.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(Width, Height, 1) toBuffer:Readback destinationOffset:0 destinationBytesPerRow:RowBytes destinationBytesPerImage:RowBytes * Height];
		return Readback;
	}

	void StoreLastPresentedReadback(id<MTLBuffer> Readback, uint32_t Width, uint32_t Height, size_t RowBytes)
	{
		ReleaseMetalObject(m_LastPresentedReadback);
		m_LastPresentedReadback = RetainMetalObject(Readback);
		m_LastPresentedReadbackWidth = Width;
		m_LastPresentedReadbackHeight = Height;
		m_LastPresentedReadbackRowBytes = RowBytes;
	}

	static void CopyBgraToRgba(uint8_t *pDst, const uint8_t *pSrc, size_t PixelCount)
	{
		for(size_t Pixel = 0; Pixel < PixelCount; ++Pixel)
		{
			pDst[Pixel * 4 + 0] = pSrc[Pixel * 4 + 2];
			pDst[Pixel * 4 + 1] = pSrc[Pixel * 4 + 1];
			pDst[Pixel * 4 + 2] = pSrc[Pixel * 4 + 0];
			pDst[Pixel * 4 + 3] = 255;
		}
	}

	void Cmd_Screenshot(const CCommandBuffer::SCommand_TrySwapAndScreenshot *pCommand)
	{
		if(pCommand->m_pImage == nullptr)
			return;
		pCommand->m_pImage->m_pData = nullptr;
		pCommand->m_pImage->m_Width = 0;
		pCommand->m_pImage->m_Height = 0;
		pCommand->m_pImage->m_Format = CImageInfo::FORMAT_RGBA;
		if(pCommand->m_pSwapped == nullptr)
			return;

		id<MTLBuffer> Readback = nil;
		size_t RowBytes = 0;
		uint32_t Width = m_DrawableWidth;
		uint32_t Height = m_DrawableHeight;
		if(!*pCommand->m_pSwapped)
		{
			Readback = EncodeDrawableReadback(Width, Height, RowBytes);
			if(Readback == nil || !CommitCurrentFrame(true, true))
			{
				ReleaseMetalObject(Readback);
				return;
			}
			*pCommand->m_pSwapped = true;
			StoreLastPresentedReadback(Readback, Width, Height, RowBytes);
			ReleaseMetalObject(m_LastPresentedCommandBuffer);
			m_LastPresentedCommandBuffer = RetainMetalObject(m_CurrentCommandBuffer);
			ReleaseMetalObject(Readback);
		}
		else
		{
			Readback = m_LastPresentedReadback;
			Width = m_LastPresentedReadbackWidth;
			Height = m_LastPresentedReadbackHeight;
			RowBytes = m_LastPresentedReadbackRowBytes;
		}
		if(Readback == nil || Width == 0 || Height == 0)
		{
			if(!*pCommand->m_pSwapped)
				ReleaseMetalObject(Readback);
			return;
		}
		size_t DataBytes = 0;
		size_t PixelBytes = 0;
		if(!MetalCheckedMul(static_cast<size_t>(Width), static_cast<size_t>(Height), PixelBytes) || !MetalCheckedMul(PixelBytes, 4, DataBytes))
		{
			if(Readback != m_LastPresentedReadback)
				ReleaseMetalObject(Readback);
			return;
		}
		uint8_t *pImageData = static_cast<uint8_t *>(malloc(DataBytes));
		if(pImageData == nullptr)
		{
			if(Readback != m_LastPresentedReadback)
				ReleaseMetalObject(Readback);
			return;
		}
		const uint8_t *pReadback = static_cast<const uint8_t *>(Readback.contents);
		for(uint32_t Y = 0; Y < Height; ++Y)
			CopyBgraToRgba(pImageData + static_cast<size_t>(Y) * Width * 4, pReadback + static_cast<size_t>(Y) * RowBytes, Width);
		pCommand->m_pImage->m_Width = Width;
		pCommand->m_pImage->m_Height = Height;
		pCommand->m_pImage->m_pData = pImageData;
		if(Readback != m_LastPresentedReadback)
			ReleaseMetalObject(Readback);
	}

	void Cmd_ReadPixel(const CCommandBuffer::SCommand_TrySwapAndReadPixel *pCommand)
	{
		if(pCommand->m_pColor == nullptr || pCommand->m_pSwapped == nullptr)
			return;
		*pCommand->m_pColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(!*pCommand->m_pSwapped)
		{
			const int X = pCommand->m_Position.x;
			const int Y = pCommand->m_Position.y;
			if(X < 0 || Y < 0 || static_cast<uint32_t>(X) >= m_DrawableWidth || static_cast<uint32_t>(Y) >= m_DrawableHeight)
				return;
			if(m_CurrentDrawable == nil && !BeginRenderEncoder({0.0, 0.0, 0.0, 1.0}))
				return;
			EndActiveEncoders();
			const size_t RowBytes = 256;
			id<MTLBuffer> Readback = [m_Device newBufferWithLength:RowBytes options:MTLResourceStorageModeShared];
			if(Readback == nil)
				return;
			m_CurrentBlitEncoder = [m_CurrentCommandBuffer blitCommandEncoder];
			if(m_CurrentBlitEncoder == nil)
			{
				ReleaseMetalObject(Readback);
				return;
			}
			[m_CurrentBlitEncoder copyFromTexture:m_CurrentDrawable.texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(X, Y, 0) sourceSize:MTLSizeMake(1, 1, 1) toBuffer:Readback destinationOffset:0 destinationBytesPerRow:RowBytes destinationBytesPerImage:RowBytes];
			if(!CommitCurrentFrame(true, true))
			{
				ReleaseMetalObject(Readback);
				return;
			}
			const uint8_t *pPixel = static_cast<const uint8_t *>(Readback.contents);
			*pCommand->m_pColor = ColorRGBA(pPixel[2] / 255.0f, pPixel[1] / 255.0f, pPixel[0] / 255.0f, 1.0f);
			*pCommand->m_pSwapped = true;
			ReleaseMetalObject(Readback);
			return;
		}
		if(m_LastPresentedReadback == nil || pCommand->m_Position.x < 0 || pCommand->m_Position.y < 0 || static_cast<uint32_t>(pCommand->m_Position.x) >= m_LastPresentedReadbackWidth || static_cast<uint32_t>(pCommand->m_Position.y) >= m_LastPresentedReadbackHeight)
			return;
		const uint8_t *pPixel = static_cast<const uint8_t *>(m_LastPresentedReadback.contents) + static_cast<size_t>(pCommand->m_Position.y) * m_LastPresentedReadbackRowBytes + static_cast<size_t>(pCommand->m_Position.x) * 4;
		*pCommand->m_pColor = ColorRGBA(pPixel[2] / 255.0f, pPixel[1] / 255.0f, pPixel[0] / 255.0f, 1.0f);
	}

	void Cmd_Swap(const CCommandBuffer::SCommand_Swap *pCommand)
	{
		(void)pCommand;
		if(m_CurrentDrawable != nil && m_DrawableWidth > 0 && m_DrawableHeight > 0)
		{
			size_t RowBytes = 0;
			id<MTLBuffer> Readback = EncodeDrawableReadback(m_DrawableWidth, m_DrawableHeight, RowBytes);
			if(Readback != nil)
			{
				id<MTLCommandBuffer> PresentedCommandBuffer = RetainMetalObject(m_CurrentCommandBuffer);
				if(CommitCurrentFrame(true, false))
				{
					StoreLastPresentedReadback(Readback, m_DrawableWidth, m_DrawableHeight, RowBytes);
					ReleaseMetalObject(m_LastPresentedCommandBuffer);
					m_LastPresentedCommandBuffer = PresentedCommandBuffer;
					PresentedCommandBuffer = nil;
				}
				ReleaseMetalObject(PresentedCommandBuffer);
				ReleaseMetalObject(Readback);
				return;
			}
		}
		CommitCurrentFrame(true, false);
	}

	void CompleteCurrentFrameIfNeeded()
	{
		CommitCurrentFrame(false, false);
	}

	void StartCommands(size_t CommandCount, size_t EstimatedRenderCallCount) override
	{
		(void)CommandCount;
		(void)EstimatedRenderCallCount;
		if(m_State != EMetalBackendState::INITIALIZED || m_CommandQueue == nil)
			return;
		const size_t Slot = m_FrameId == 0 ? 0 : (m_CurrentFrameSlot + 1) % gs_FrameSlotCount;
		SFrameSlot &Frame = m_aFrameSlots[Slot];
		if(m_FrameState.SlotState(Slot) == CMetalFrameState::ESlotState::IN_FLIGHT && Frame.m_CommandBuffer != nil)
		{
			[Frame.m_CommandBuffer waitUntilCompleted];
			m_FrameState.CompleteFrame({Frame.m_FrameId, Slot}, Frame.m_CommandBuffer.status == MTLCommandBufferStatusCompleted);
		}
		ReleaseFrameSlotResources(Slot);
		if(!m_FrameState.BeginFrame(Slot))
			return;
		m_CurrentFrameSlot = Slot;
		Frame.m_CommandBuffer = RetainMetalObject([m_CommandQueue commandBuffer]);
		m_CurrentCommandBuffer = Frame.m_CommandBuffer;
		m_CommandBufferCommitted = false;
		m_RenderEncoderStarted = false;
		m_CurrentRenderEncoder = nil;
		m_CurrentBlitEncoder = nil;
		m_CurrentDrawable = nil;
		m_BackbufferHasContents = false;
		m_RenderTargetState.Reset();
		Frame.m_VertexOffset = 0;
		Frame.m_FrameId = m_FrameState.CurrentFrameId();
		m_FrameId = Frame.m_FrameId;
		const CMetalFrameState::SFrameCapture Capture{Frame.m_FrameId, Slot};
		[m_CurrentCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> Buffer) {
			const bool Success = Buffer.status == MTLCommandBufferStatusCompleted;
			if(!Success)
				RecordGpuFailure(Buffer, Capture.m_FrameId);
			m_FrameState.CompleteFrame(Capture, Success);
		}];
	}

	void EndCommands() override
	{
		CompleteCurrentFrameIfNeeded();
	}

public:
	~CCommandProcessorFragment_Metal() override
	{
		WaitForGpuIdle();
		DestroyAllBufferContainers();
		DestroyAllBuffers();
		ReleaseGpuObjects();
		if(m_MetalView != nullptr)
			SDL_Metal_DestroyView(m_MetalView);
		for(size_t Slot = 0; Slot < m_vTextureSlots.size(); ++Slot)
			DestroyTexture(static_cast<int>(Slot));
		ReleaseCommandQueue();
		ReleaseDevice();
	}

	ERunCommandReturnTypes RunCommand(const CCommandBuffer::SCommand *pBaseCommand) override
	{
		@autoreleasepool
		{
			m_Error = {};
			const bool IsLifecycleCommand = pBaseCommand->m_Cmd == CCommandProcessorFragment_GLBase::CMD_PRE_INIT ||
				pBaseCommand->m_Cmd == CCommandProcessorFragment_GLBase::CMD_INIT ||
				pBaseCommand->m_Cmd == CCommandProcessorFragment_GLBase::CMD_SHUTDOWN ||
				pBaseCommand->m_Cmd == CCommandProcessorFragment_GLBase::CMD_POST_SHUTDOWN;
			if(!IsLifecycleCommand && SetGpuFailureError())
				return RUN_COMMAND_COMMAND_ERROR;
			if(!IsLifecycleCommand)
				m_LastCommandId.store(pBaseCommand->m_Cmd, std::memory_order_relaxed);
			switch(pBaseCommand->m_Cmd)
			{
			case CCommandProcessorFragment_GLBase::CMD_PRE_INIT:
				Cmd_PreInit(static_cast<const SCommand_PreInit *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandProcessorFragment_GLBase::CMD_INIT:
				Cmd_Init(static_cast<const SCommand_Init *>(pBaseCommand));
				// 初始化失败通过 SCommand_Init 的输出参数传播，不能污染包装器的粘滞错误，
				// 否则部分初始化清理无法提交后续 shutdown 命令。
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandProcessorFragment_GLBase::CMD_SHUTDOWN:
				Cmd_Shutdown(static_cast<const SCommand_Shutdown *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandProcessorFragment_GLBase::CMD_POST_SHUTDOWN:
				Cmd_PostShutdown(static_cast<const SCommand_PostShutdown *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_VSYNC:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_VSync *>(pBaseCommand);
				m_VSync = pCommand->m_VSync != 0;
				if(m_pLayer != nullptr)
					((__bridge CAMetalLayer *)m_pLayer).displaySyncEnabled = m_VSync;
				if(pCommand->m_pRetOk != nullptr)
					*pCommand->m_pRetOk = m_pLayer != nullptr;
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_UPDATE_VIEWPORT:
				UpdateDrawableSize();
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_CREATE_BUFFER_OBJECT:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_CreateBufferObject *>(pBaseCommand);
				const bool Success = CreateBuffer(pCommand->m_BufferIndex, pCommand->m_DataSize, pCommand->m_pUploadData, pCommand->m_Flags);
				if(pCommand->m_DeletePointer && pCommand->m_pUploadData != nullptr)
					free(pCommand->m_pUploadData);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RECREATE_BUFFER_OBJECT:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_RecreateBufferObject *>(pBaseCommand);
				DestroyBuffer(pCommand->m_BufferIndex);
				const bool Success = CreateBuffer(pCommand->m_BufferIndex, pCommand->m_DataSize, pCommand->m_pUploadData, pCommand->m_Flags);
				if(pCommand->m_DeletePointer && pCommand->m_pUploadData != nullptr)
					free(pCommand->m_pUploadData);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_UPDATE_BUFFER_OBJECT:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_UpdateBufferObject *>(pBaseCommand);
				const size_t Offset = reinterpret_cast<uintptr_t>(pCommand->m_pOffset);
				const bool Success = UpdateBuffer(pCommand->m_BufferIndex, Offset, pCommand->m_DataSize, pCommand->m_pUploadData);
				if(pCommand->m_DeletePointer && pCommand->m_pUploadData != nullptr)
					free(pCommand->m_pUploadData);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_COPY_BUFFER_OBJECT:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_CopyBufferObject *>(pBaseCommand);
				const bool Success = CopyBuffer(pCommand->m_WriteBufferIndex, pCommand->m_ReadBufferIndex, pCommand->m_WriteOffset, pCommand->m_ReadOffset, pCommand->m_CopySize);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_DELETE_BUFFER_OBJECT:
				DestroyBuffer(static_cast<const CCommandBuffer::SCommand_DeleteBufferObject *>(pBaseCommand)->m_BufferIndex);
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_CREATE_BUFFER_CONTAINER:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_CreateBufferContainer *>(pBaseCommand);
				const bool Success = UpdateBufferContainer(pCommand->m_BufferContainerIndex, pCommand->m_Stride, pCommand->m_VertBufferBindingIndex, pCommand->m_AttrCount, pCommand->m_pAttributes, true);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_UPDATE_BUFFER_CONTAINER:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_UpdateBufferContainer *>(pBaseCommand);
				const bool Success = UpdateBufferContainer(pCommand->m_BufferContainerIndex, pCommand->m_Stride, pCommand->m_VertBufferBindingIndex, pCommand->m_AttrCount, pCommand->m_pAttributes, false);
				if(!Success)
					SetResourceCommandError(pCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_DELETE_BUFFER_CONTAINER:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_DeleteBufferContainer *>(pBaseCommand);
				DestroyBufferContainer(pCommand->m_BufferContainerIndex, pCommand->m_DestroyAllBO);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_INDICES_REQUIRED_NUM_NOTIFY:
				m_RequiredIndicesNum = std::max(m_RequiredIndicesNum, static_cast<const CCommandBuffer::SCommand_IndicesRequiredNumNotify *>(pBaseCommand)->m_RequiredIndicesNum);
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_TEXTURE_CREATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand);
				CreateTexture(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::RGBA8, pCommand->m_Flags, pCommand->m_pData);
				if(pCommand->m_pData != nullptr)
					free(pCommand->m_pData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXTURE_UPDATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand);
				UpdateTexture(pCommand->m_Slot, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::RGBA8, pCommand->m_pData);
				if(pCommand->m_pData != nullptr)
					free(pCommand->m_pData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXTURE_DESTROY:
				DestroyTexture(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand)->m_Slot);
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_TEXT_TEXTURES_CREATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_TextTextures_Create *>(pBaseCommand);
				CreateTexture(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::R8, TextureFlag::NO_MIPMAPS, pCommand->m_pTextData);
				CreateTexture(pCommand->m_SlotOutline, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::R8, TextureFlag::NO_MIPMAPS, pCommand->m_pTextOutlineData);
				if(pCommand->m_pTextData != nullptr)
					free(pCommand->m_pTextData);
				if(pCommand->m_pTextOutlineData != nullptr)
					free(pCommand->m_pTextOutlineData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXT_TEXTURE_UPDATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_TextTexture_Update *>(pBaseCommand);
				const bool Success = UpdateTexture(pCommand->m_Slot, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::R8, pCommand->m_pData);
				if(!Success && pCommand->m_pData != nullptr)
					free(pCommand->m_pData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXT_TEXTURES_DESTROY:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_TextTextures_Destroy *>(pBaseCommand);
				DestroyTexture(pCommand->m_Slot);
				DestroyTexture(pCommand->m_SlotOutline);
					return RUN_COMMAND_COMMAND_HANDLED;
				}
			case CCommandBuffer::CMD_CLEAR:
				Cmd_Clear(static_cast<const CCommandBuffer::SCommand_Clear *>(pBaseCommand));
				return m_Error.m_ErrorType == GFX_ERROR_TYPE_NONE ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			case CCommandBuffer::CMD_RENDER:
				Cmd_Render(static_cast<const CCommandBuffer::SCommand_Render *>(pBaseCommand));
				return m_Error.m_ErrorType == GFX_ERROR_TYPE_NONE ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			case CCommandBuffer::CMD_RENDER_TEX3D:
			{
				const bool Success = DrawTex3D(*static_cast<const CCommandBuffer::SCommand_RenderTex3D *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TARGET_CREATE:
			{
				const bool Success = Cmd_RenderTarget_Create(static_cast<const CCommandBuffer::SCommand_RenderTarget_Create *>(pBaseCommand));
				if(!Success)
					SetResourceCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TARGET_DESTROY:
				Cmd_RenderTarget_Destroy(static_cast<const CCommandBuffer::SCommand_RenderTarget_Destroy *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_RENDER_TARGET_BEGIN:
			{
				const bool Success = Cmd_RenderTarget_Begin(static_cast<const CCommandBuffer::SCommand_RenderTarget_Begin *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TARGET_END:
				Cmd_RenderTarget_End(static_cast<const CCommandBuffer::SCommand_RenderTarget_End *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_RENDER_TARGET_DRAW:
			{
				const bool Success = Cmd_RenderTarget_Draw(static_cast<const CCommandBuffer::SCommand_RenderTarget_Draw *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TARGET_CAPTURE_BACKBUFFER:
			{
				const bool Success = Cmd_RenderTarget_CaptureBackbuffer(static_cast<const CCommandBuffer::SCommand_RenderTarget_CaptureBackbuffer *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TILE_LAYER:
			{
				const bool Success = DrawTileLayer(*static_cast<const CCommandBuffer::SCommand_RenderTileLayer *>(pBaseCommand), false, {}, {});
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_BORDER_TILE:
			{
				const bool Success = DrawBorderTile(*static_cast<const CCommandBuffer::SCommand_RenderBorderTile *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_QUAD_LAYER:
			case CCommandBuffer::CMD_RENDER_QUAD_LAYER_GROUPED:
			{
				const bool Grouped = pBaseCommand->m_Cmd == CCommandBuffer::CMD_RENDER_QUAD_LAYER_GROUPED;
				const bool Success = DrawQuadLayer(*static_cast<const CCommandBuffer::SCommand_RenderQuadLayer *>(pBaseCommand), Grouped);
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_TEXT:
			{
				const bool Success = DrawText(*static_cast<const CCommandBuffer::SCommand_RenderText *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER:
			{
				const bool Success = DrawQuadContainer(*static_cast<const CCommandBuffer::SCommand_RenderQuadContainer *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_EX:
			{
				const bool Success = DrawQuadContainerEx(*static_cast<const CCommandBuffer::SCommand_RenderQuadContainerEx *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_RENDER_QUAD_CONTAINER_SPRITE_MULTIPLE:
			{
				const bool Success = DrawQuadContainerAsSpriteMultiple(*static_cast<const CCommandBuffer::SCommand_RenderQuadContainerAsSpriteMultiple *>(pBaseCommand));
				if(!Success)
					SetUnsupportedCommandError(pBaseCommand);
				return Success ? RUN_COMMAND_COMMAND_HANDLED : RUN_COMMAND_COMMAND_ERROR;
			}
			case CCommandBuffer::CMD_SWAP:
				Cmd_Swap(static_cast<const CCommandBuffer::SCommand_Swap *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_TRY_SWAP_AND_READ_PIXEL:
				Cmd_ReadPixel(static_cast<const CCommandBuffer::SCommand_TrySwapAndReadPixel *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_TRY_SWAP_AND_SCREENSHOT:
				Cmd_Screenshot(static_cast<const CCommandBuffer::SCommand_TrySwapAndScreenshot *>(pBaseCommand));
				return RUN_COMMAND_COMMAND_HANDLED;

			// 这些命令由组合处理器中的 SDL/General fragment 接管。
			case CCommandBuffer::CMD_SIGNAL:
			case CCommandBuffer::CMD_MULTISAMPLING:
			case CCommandBuffer::CMD_WINDOW_CREATE_NTF:
			case CCommandBuffer::CMD_WINDOW_DESTROY_NTF:
				return RUN_COMMAND_COMMAND_UNHANDLED;

			default:
				SetUnsupportedCommandError(pBaseCommand);
				return RUN_COMMAND_COMMAND_ERROR;
			}
		}
	}
};
} // namespace

CCommandProcessorFragment_GLBase *CreateMetalCommandProcessorFragment()
{
	return new CCommandProcessorFragment_Metal();
}
