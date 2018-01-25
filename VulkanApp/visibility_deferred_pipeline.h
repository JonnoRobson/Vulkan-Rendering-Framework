#ifndef _VISIBILITY_DEFERRED_PIPELINE_H_
#define _VISIBILITY_DEFERRED_PIPELINE_H_

#include "pipeline.h"

class VisibilityDeferredPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

};

#endif