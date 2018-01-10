#ifndef _TRANSPARENCY_COMPOSITE_PIPELINE_H_
#define _TRANSPARENCY_COMPOSITE_PIPELINE_H_

#include "pipeline.h"

class TransparencyCompositePipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();
};
#endif