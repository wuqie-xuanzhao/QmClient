#include "backend_metal.h"
#include "metal_frame_state.h"
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
		id<MTLBuffer> m_Staging = nil;
		SMetalTextureLayout m_Layout;
		size_t m_Width = 0;
		size_t m_Height = 0;
		EMetalTextureFormat m_Format = EMetalTextureFormat::RGBA8;
		bool m_Allocated = false;
	};

	struct SBufferSlot
	{
		id<MTLBuffer> m_Buffer = nil;
		size_t m_DataBytes = 0;
		bool m_Allocated = false;
		bool m_OneTimeUse = false;
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
	std::array<id<MTLRenderPipelineState>, 18> m_aPipelineStates{};
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
	uint32_t m_DrawableWidth = 0;
	uint32_t m_DrawableHeight = 0;
	size_t m_StreamMemoryBytes = 0;
	size_t m_BufferMemoryBytes = 0;
	std::vector<STextureSlot> m_vTextureSlots;
	std::vector<SBufferSlot> m_vBufferSlots;
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
		m_CurrentRenderEncoder = nil;
		m_CurrentBlitEncoder = nil;
		m_CurrentDrawable = nil;
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

	void EndRenderEncoderForBlit()
	{
		if(m_CurrentRenderEncoder == nil)
			return;
		[m_CurrentRenderEncoder endEncoding];
		m_CurrentRenderEncoder = nil;
		m_RenderEncoderStarted = false;
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
		if(VertexFunction == nil || ColorFunction == nil || TexturedFunction == nil || TextFunction == nil)
		{
			ReleaseMetalObject(VertexFunction);
			ReleaseMetalObject(ColorFunction);
			ReleaseMetalObject(TexturedFunction);
			ReleaseMetalObject(TextFunction);
			return false;
		}

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

		bool Success = true;
		for(int Textured = 0; Textured <= 1; ++Textured)
		{
			for(int Text = 0; Text <= 1; ++Text)
			{
				if(Textured == 0 && Text != 0)
					continue;
				for(int Blend = 0; Blend <= static_cast<int>(EMetalBlendMode::ADDITIVE); ++Blend)
				{
					MTLRenderPipelineDescriptor *pPipeline = [[MTLRenderPipelineDescriptor alloc] init];
					pPipeline.vertexFunction = VertexFunction;
					pPipeline.fragmentFunction = Text ? TextFunction : (Textured ? TexturedFunction : ColorFunction);
					pPipeline.vertexDescriptor = pVertexDescriptor;
					pPipeline.rasterSampleCount = 1;
					pPipeline.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
					const SMetalBlendState BlendState = MetalBlendState(static_cast<EMetalBlendMode>(Blend));
					pPipeline.colorAttachments[0].blendingEnabled = BlendState.m_Enabled;
					pPipeline.colorAttachments[0].sourceRGBBlendFactor = MetalBlendFactor(BlendState.m_Source);
					pPipeline.colorAttachments[0].destinationRGBBlendFactor = MetalBlendFactor(BlendState.m_Destination);
					pPipeline.colorAttachments[0].sourceAlphaBlendFactor = MetalBlendFactor(BlendState.m_Source);
					pPipeline.colorAttachments[0].destinationAlphaBlendFactor = MetalBlendFactor(BlendState.m_Destination);
					NSError *pError = nil;
					m_aPipelineStates[PipelineIndex(Textured != 0, Text != 0, static_cast<EMetalBlendMode>(Blend))] = [m_Device newRenderPipelineStateWithDescriptor:pPipeline error:&pError];
					Success = Success && m_aPipelineStates[PipelineIndex(Textured != 0, Text != 0, static_cast<EMetalBlendMode>(Blend))] != nil;
#if !__has_feature(objc_arc)
					[pPipeline release];
#endif
				}
			}
		}
#if !__has_feature(objc_arc)
		[pVertexDescriptor release];
		[VertexFunction release];
		[ColorFunction release];
		[TexturedFunction release];
		[TextFunction release];
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
		if(Texture.m_Staging != nil)
		{
			if(m_pStagingMemoryUsage != nullptr)
				m_pStagingMemoryUsage->fetch_sub([Texture.m_Staging length], std::memory_order_relaxed);
			ReleaseMetalObject(Texture.m_Staging);
		}
		ReleaseMetalObject(Texture.m_Texture);
		Texture = {};
	}

	bool CreateTexture(int Slot, size_t Width, size_t Height, EMetalTextureFormat Format, int Flags, uint8_t *pData)
	{
		if(!EnsureTextureSlot(Slot))
			return false;
		STextureSlot &Texture = m_vTextureSlots[Slot];
		if(Texture.m_Allocated)
			DestroyTexture(Slot);

		SMetalTextureLayout Layout;
		if(!MetalTextureLayout(Width, Height, Format, (Flags & TextureFlag::NO_MIPMAPS) != 0, Layout))
			return false;

		if(m_Device == nil || m_State != EMetalBackendState::INITIALIZED || m_CommandQueue == nil)
			return false;

		MTLTextureDescriptor *pDescriptor = [[MTLTextureDescriptor alloc] init];
		pDescriptor.textureType = MTLTextureType2D;
		pDescriptor.pixelFormat = Format == EMetalTextureFormat::R8 ? MTLPixelFormatR8Unorm : MTLPixelFormatRGBA8Unorm;
		pDescriptor.width = Width;
		pDescriptor.height = Height;
		pDescriptor.mipmapLevelCount = Layout.m_MipLevels;
		pDescriptor.usage = MTLTextureUsageShaderRead;
		pDescriptor.storageMode = MTLStorageModePrivate;
		id<MTLTexture> pTexture = [m_Device newTextureWithDescriptor:pDescriptor];
	#if !__has_feature(objc_arc)
		[pDescriptor release];
	#endif
		if(pTexture == nil)
			return false;

		Texture.m_Texture = pTexture;
		Texture.m_Layout = Layout;
		Texture.m_Width = Width;
		Texture.m_Height = Height;
		Texture.m_Format = Format;
		Texture.m_Allocated = true;
		AddTextureMemory(Layout.m_DataBytes);

		const bool Uploaded = pData == nullptr || UploadTextureRegion(Texture, 0, 0, Width, Height, pData);
		if(!Uploaded)
		{
			DestroyTexture(Slot);
			return false;
		}
		if(Layout.m_MipLevels > 1 && m_CurrentBlitEncoder != nil)
			[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_Texture];
		return true;
	}

	bool UpdateTexture(int Slot, size_t X, size_t Y, size_t Width, size_t Height, EMetalTextureFormat Format, uint8_t *pData)
	{
		if(!EnsureTextureSlot(Slot) || !m_vTextureSlots[Slot].m_Allocated || m_CommandQueue == nil)
			return false;
		STextureSlot &Texture = m_vTextureSlots[Slot];
		if(Texture.m_Format != Format || !MetalValidateSubregion(Texture.m_Width, Texture.m_Height, X, Y, Width, Height))
			return false;
		SMetalTextureLayout UpdateLayout;
		if(!MetalTextureLayout(Width, Height, Format, true, UpdateLayout))
			return false;
		const bool Uploaded = pData == nullptr || UploadTextureRegion(Texture, X, Y, Width, Height, pData);
		if(Uploaded && Texture.m_Layout.m_MipLevels > 1 && m_CurrentBlitEncoder != nil)
			[m_CurrentBlitEncoder generateMipmapsForTexture:Texture.m_Texture];
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
			m_pCapabilities->m_ShaderSupport = true;
			m_pCapabilities->m_TrianglesAsQuads = true;
			m_pCapabilities->m_pRenderTargetSupportReason = "metal_p1_disabled";
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
		if(m_CurrentCommandBuffer == nil || m_pLayer == nullptr)
			return false;
		if(m_CurrentBlitEncoder != nil)
		{
			[m_CurrentBlitEncoder endEncoding];
			m_CurrentBlitEncoder = nil;
		}
		if(m_CurrentDrawable == nil)
		{
			CAMetalLayer *pLayer = (__bridge CAMetalLayer *)m_pLayer;
			m_CurrentDrawable = [pLayer nextDrawable];
		}
		if(m_CurrentDrawable == nil)
			return false;
		MTLRenderPassDescriptor *pPass = [MTLRenderPassDescriptor renderPassDescriptor];
		pPass.colorAttachments[0].texture = m_CurrentDrawable.texture;
		pPass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pPass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pPass.colorAttachments[0].clearColor = ClearColor;
		m_CurrentRenderEncoder = [m_CurrentCommandBuffer renderCommandEncoderWithDescriptor:pPass];
		if(m_CurrentRenderEncoder == nil)
			return false;
		m_CurrentRenderEncoder.label = @"QmClient Metal frame";
		[m_CurrentRenderEncoder setViewport:(MTLViewport){0.0, 0.0, static_cast<double>(m_DrawableWidth), static_cast<double>(m_DrawableHeight), 0.0, 1.0}];
		m_RenderEncoderStarted = true;
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
		if(State.m_ClipEnable)
			[m_CurrentRenderEncoder setScissorRect:MTLScissorRect{static_cast<NSUInteger>(std::max(State.m_ClipX, 0)), static_cast<NSUInteger>(std::max<int>(0, static_cast<int>(m_DrawableHeight) - State.m_ClipY - State.m_ClipH)), static_cast<NSUInteger>(std::max(State.m_ClipW, 0)), static_cast<NSUInteger>(std::max(State.m_ClipH, 0))}];
		else
			[m_CurrentRenderEncoder setScissorRect:MTLScissorRect{0, 0, m_DrawableWidth, m_DrawableHeight}];
		if(PrimitiveType == EPrimitiveType::QUADS)
			[m_CurrentRenderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:PrimitiveCount * 6 indexType:MTLIndexTypeUInt16 indexBuffer:m_QuadIndexBuffer indexBufferOffset:0];
		else
			[m_CurrentRenderEncoder drawPrimitives:PrimitiveType == EPrimitiveType::LINES ? MTLPrimitiveTypeLine : MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:VertexCount];
		Frame.m_VertexOffset = UniformOffset + sizeof(Uniforms);
		return true;
	}

	void Cmd_Clear(const CCommandBuffer::SCommand_Clear *pCommand)
	{
		if(m_RenderEncoderStarted && m_CurrentDrawable != nil)
		{
			[m_CurrentRenderEncoder endEncoding];
			m_CurrentRenderEncoder = nil;
			m_RenderEncoderStarted = false;
		}
		if(!BeginRenderEncoder(MTLClearColorMake(pCommand->m_Color.r, pCommand->m_Color.g, pCommand->m_Color.b, 0.0)))
			SetUnsupportedCommandError(pCommand);
	}

	void Cmd_Render(const CCommandBuffer::SCommand_Render *pCommand)
	{
		if(!DrawPrimitive(pCommand->m_State, pCommand->m_PrimType, pCommand->m_PrimCount, pCommand->m_pVertices))
			SetUnsupportedCommandError(pCommand);
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
