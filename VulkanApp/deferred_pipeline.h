#ifndef _DEFERRED_PIPELINE_H_
#define _DEFERRED_PIPELINE_H_

#include "pipeline.h"

class DeferredPipeline : public VulkanPipeline
{
public:
	virtual void RecordRenderCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	virtual void CreateRenderPass();
	virtual void CreateFramebuffers();
	virtual void CreatePipeline();
};

#endif