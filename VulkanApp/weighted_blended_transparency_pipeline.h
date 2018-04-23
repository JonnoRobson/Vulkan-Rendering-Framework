#ifndef _WEIGHTED_BLENDED_TRANSPARENCY_PIPELINE_H_
#define _WEIGHTED_BLENDED_TRANSPARENCY_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

/**
* Vulkan graphics pipeline for rendering transparency using weighted blended oit
*/
class WeightedBlendedTransparencyPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetRenderTargets(VulkanRenderTarget* accumulation, VulkanRenderTarget* revealage);

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:
	VulkanRenderTarget *accumulation_buffer_, *revealage_buffer_;
};

#endif