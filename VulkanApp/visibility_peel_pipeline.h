#ifndef _VISIBILITY_PEEL_PIPELINE_H_
#define _VISIBILITY_PEEL_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

#include <glm/glm.hpp>

#define VISIBILITY_PEEL_COUNT 3

// each visibility peel pipeline writes to both visibility and depth render targets for each layer
class VisibilityPeelPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetOutputBuffers(VkImageView min_depth, VkImageView max_depth);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VkImageView min_depth_buffer_, max_depth_buffer_;

};

#endif