#ifndef _VISIBILITY_PEEL_INIT_PIPELINE_H_
#define _VISIBILITY_PEEL_INIT_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

class VisibilityPeelInitPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetDepthBuffers(VkImageView min_depth_buffer, VkImageView max_depth_buffer);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VkImageView min_depth_buffer_, max_depth_buffer_;
};
 
#endif