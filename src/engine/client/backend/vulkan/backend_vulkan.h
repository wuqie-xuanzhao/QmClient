#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

class CCommandProcessorFragment_GLBase;

struct SVulkanVersion
{
	int m_Major;
	int m_Minor;
	int m_Patch;
};

static constexpr SVulkanVersion gs_BackendVulkanMinimumVersion = {1, 1, 0};
static constexpr SVulkanVersion gs_BackendVulkanMaximumVersion = {1, 4, 0};

constexpr bool IsVulkanVersionAtLeast(const SVulkanVersion &Version, const SVulkanVersion &Required)
{
	if(Version.m_Major != Required.m_Major)
		return Version.m_Major > Required.m_Major;
	if(Version.m_Minor != Required.m_Minor)
		return Version.m_Minor > Required.m_Minor;
	return Version.m_Patch >= Required.m_Patch;
}

constexpr SVulkanVersion NormalizeRequestedVulkanVersion(const SVulkanVersion &Requested)
{
	return Requested.m_Major == gs_BackendVulkanMaximumVersion.m_Major && Requested.m_Minor >= gs_BackendVulkanMaximumVersion.m_Minor ? gs_BackendVulkanMaximumVersion : gs_BackendVulkanMinimumVersion;
}

CCommandProcessorFragment_GLBase *CreateVulkanCommandProcessorFragment();

#endif
