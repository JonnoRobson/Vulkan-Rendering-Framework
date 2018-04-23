#ifndef _DEFERRED_PIPELINE_H_
#define _DEFERRED_PIPELINE_H_

#include "pipeline.h"

/**
* Vulkan graphics pipeline for deferred rendering
*/
class DeferredPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();
};

#endif