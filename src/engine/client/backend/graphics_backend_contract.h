#ifndef ENGINE_CLIENT_BACKEND_GRAPHICS_BACKEND_CONTRACT_H
#define ENGINE_CLIENT_BACKEND_GRAPHICS_BACKEND_CONTRACT_H

#include <engine/graphics.h>

namespace graphics_backend
{
	struct SSafeBackendConfig
	{
		const char *m_pBackend;
		int m_GLMajor;
		int m_GLMinor;
		int m_GLPatch;
		int m_FsaaSamples;
		int m_Fullscreen;
		int m_Borderless;
	};

	constexpr SSafeBackendConfig SafeBackendConfig()
	{
		return {"OpenGL", 4, 1, 0, 0, 0, 0};
	}

	constexpr bool IsMetalCompiled()
	{
#if defined(CONF_PLATFORM_MACOS) && defined(CONF_BACKEND_METAL) && defined(CONF_BACKEND_METAL_READY)
		return true;
#else
		return false;
#endif
	}

	constexpr bool IsBackendCompiled(EBackendType BackendType)
	{
		switch(BackendType)
		{
		case BACKEND_TYPE_OPENGL:
			return true;
		case BACKEND_TYPE_OPENGL_ES:
#if defined(CONF_BACKEND_OPENGL_ES) || defined(CONF_BACKEND_OPENGL_ES3)
			return true;
#else
			return false;
#endif
		case BACKEND_TYPE_VULKAN:
#if defined(CONF_BACKEND_VULKAN)
			return true;
#else
			return false;
#endif
		case BACKEND_TYPE_METAL:
			return IsMetalCompiled();
		case BACKEND_TYPE_AUTO:
		case BACKEND_TYPE_COUNT:
			return false;
		}
		return false;
	}

	constexpr bool IsBackendSelectable(EBackendType BackendType)
	{
		return BackendType != BACKEND_TYPE_AUTO && IsBackendCompiled(BackendType);
	}

	constexpr bool UsesOpenGLVersionTuple(EBackendType BackendType)
	{
		return BackendType == BACKEND_TYPE_OPENGL || BackendType == BACKEND_TYPE_OPENGL_ES;
	}

	constexpr bool PreservesOpenGLVersionTuple(EBackendType BackendType)
	{
		return BackendType == BACKEND_TYPE_METAL;
	}

	constexpr bool RequiresFrameSerializationWorkaround(EBackendType BackendType)
	{
		return BackendType == BACKEND_TYPE_VULKAN;
	}

	const char *BackendName(EBackendType BackendType);
	bool IsKnownBackendName(const char *pName);
	bool IsKnownUnavailableBackendName(const char *pName);
	EBackendType ParseBackendName(const char *pName, EBackendType Fallback);
	EBackendType ResolveBackend(EBackendType Requested, EBackendType Fallback);
	bool MatchesConfiguredBackend(EBackendType CandidateBackend, const char *pCandidateName, int CandidateMajor, int CandidateMinor, int CandidatePatch, const char *pConfiguredName, int ConfiguredMajor, int ConfiguredMinor, int ConfiguredPatch);
} // namespace graphics_backend

#endif
