#ifndef _SHADOW_MAP_PIPELINE_H_
#define _SHADOW_MAP_PIPELINE_H_

#include "pipeline.h"

class ShadowMapPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetImageViews(VkFormat format, VkFormat depth_format, VkImageView image_view, VkImageView depth_image_view);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:
	VkImageView shadow_map_image_view_;
	VkImageView shadow_map_depth_image_view_;

	VkFormat shadow_map_format_;
	VkFormat shadow_map_depth_format_;
};

#endif