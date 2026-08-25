#include "backend_metal.h"
#include "metal_types.h"

#include <base/log.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <SDL_metal.h>

#define pi qmclient_carbon_pi
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#undef pi

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <string>
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

	EMetalBackendState m_State = EMetalBackendState::UNINITIALIZED;
	uint64_t m_FrameId = 0;
	SDL_MetalView m_MetalView = nullptr;
	void *m_pLayer = nullptr;
	SDL_Window *m_pWindow = nullptr;
	bool m_VSync = true;
	id<MTLDevice> m_Device = nil;
	id<MTLCommandQueue> m_CommandQueue = nil;
	std::vector<STextureSlot> m_vTextureSlots;
	std::atomic<uint64_t> *m_pTextureMemoryUsage = nullptr;
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

	void ReleaseCommandQueue()
	{
		ReleaseMetalObject(m_CommandQueue);
		m_CommandQueue = nil;
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

		// 上传编码器由后续 present 任务接入；没有它时不创建空纹理。
		if(pData != nullptr)
			free(pData);
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
		if(pData != nullptr)
			free(pData);
		return true;
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

	bool GetPresentedImageData(uint32_t &, uint32_t &, CImageInfo::EImageFormat &, std::vector<uint8_t> &) override
	{
		return false;
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
		m_pTextureMemoryUsage = pCommand->m_pTextureMemoryUsage;
		m_pStagingMemoryUsage = pCommand->m_pStagingMemoryUsage;
		if(m_pTextureMemoryUsage != nullptr)
			m_pTextureMemoryUsage->store(0, std::memory_order_relaxed);
		if(m_pStagingMemoryUsage != nullptr)
			m_pStagingMemoryUsage->store(0, std::memory_order_relaxed);
		if(pCommand->m_pInitError != nullptr)
			*pCommand->m_pInitError = -1;
		if(pCommand->m_pErrStringPtr != nullptr)
			*pCommand->m_pErrStringPtr = "native Metal command processor is not initialized yet";
	}

	void Cmd_Shutdown(const SCommand_Shutdown *pCommand)
	{
		(void)pCommand;
		m_State = EMetalBackendState::SHUTDOWN;
	}

	void Cmd_PostShutdown(const SCommand_PostShutdown *pCommand)
	{
		(void)pCommand;
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

public:
	~CCommandProcessorFragment_Metal() override
	{
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
			case CCommandBuffer::CMD_TEXTURE_CREATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_Texture_Create *>(pBaseCommand);
				const bool Success = CreateTexture(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::RGBA8, pCommand->m_Flags, pCommand->m_pData);
				if(!Success && pCommand->m_pData != nullptr)
					free(pCommand->m_pData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXTURE_UPDATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_Texture_Update *>(pBaseCommand);
				const bool Success = UpdateTexture(pCommand->m_Slot, pCommand->m_X, pCommand->m_Y, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::RGBA8, pCommand->m_pData);
				if(!Success && pCommand->m_pData != nullptr)
					free(pCommand->m_pData);
				return RUN_COMMAND_COMMAND_HANDLED;
			}
			case CCommandBuffer::CMD_TEXTURE_DESTROY:
				DestroyTexture(static_cast<const CCommandBuffer::SCommand_Texture_Destroy *>(pBaseCommand)->m_Slot);
				return RUN_COMMAND_COMMAND_HANDLED;
			case CCommandBuffer::CMD_TEXT_TEXTURES_CREATE:
			{
				const auto *pCommand = static_cast<const CCommandBuffer::SCommand_TextTextures_Create *>(pBaseCommand);
				const bool TextSuccess = CreateTexture(pCommand->m_Slot, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::R8, TextureFlag::NO_MIPMAPS, pCommand->m_pTextData);
				const bool OutlineSuccess = CreateTexture(pCommand->m_SlotOutline, pCommand->m_Width, pCommand->m_Height, EMetalTextureFormat::R8, TextureFlag::NO_MIPMAPS, pCommand->m_pTextOutlineData);
				if(!TextSuccess && pCommand->m_pTextData != nullptr)
					free(pCommand->m_pTextData);
				if(!OutlineSuccess && pCommand->m_pTextOutlineData != nullptr)
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

			// 这些命令由组合处理器中的 SDL/General fragment 接管。
			case CCommandBuffer::CMD_SIGNAL:
			case CCommandBuffer::CMD_MULTISAMPLING:
			case CCommandBuffer::CMD_SWAP:
			case CCommandBuffer::CMD_WINDOW_CREATE_NTF:
			case CCommandBuffer::CMD_WINDOW_DESTROY_NTF:
				if(pBaseCommand->m_Cmd == CCommandBuffer::CMD_SWAP)
					++m_FrameId;
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
