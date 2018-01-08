#ifndef _TONEMAP_PIPELINE_H_
#define _TONEMAP_PIPELINE_H_

#include "pipeline.h"

class TonemapPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:

};

#endif