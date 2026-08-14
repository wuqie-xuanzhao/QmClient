#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

class CCommandProcessorFragment_GLBase;

struct SVulkanVersion
{
	int m_Major;
	int m_Minor;
	int m_Patch;
};

static constexpr SVulkanVersion gs_BackendVulkanFallbackVersion = {1, 1, 0};

constexpr bool IsVulkanVersionAtLeast(const SVulkanVersion &Version, const SVulkanVersion &Required)
{
	if(Version.m_Major != Required.m_Major)
		return Version.m_Major > Required.m_Major;
	if(Version.m_Minor != Required.m_Minor)
		return Version.m_Minor > Required.m_Minor;
	return Version.m_Patch >= Required.m_Patch;
}

CCommandProcessorFragment_GLBase *CreateVulkanCommandProcessorFragment();

#endif
