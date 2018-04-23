#ifndef _VISIBILITY_FRONT_PEEL_PIPELINE_H_
#define _VISIBILITY_FRONT_PEEL_PIPELINE_H_

#include "pipeline.h"

#define VISIBILITY_PEEL_COUNT 4

/**
* Vulkan graphics pipeline for rendering a layered visibility buffer
*/
class VisibilityFrontPeelPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);
	void SetOutputBuffers(VkImageView depth_buffer, VkImageView visibility_buffer);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VkImageView depth_buffer_, visibility_buffer_;

};

#endif