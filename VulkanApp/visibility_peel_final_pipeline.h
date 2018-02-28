#ifndef _VISIBILITY_PEEL_FINAL_PIPELINE_H_
#define _VISIBILITY_PEEL_FINAL_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

#include <glm/glm.hpp>

// the visibility peel final pipeline writes to only the visibility targets for the final layer of visibility
class VisibilityPeelFinalPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetOutputBuffers(VkImageView front_buffer, VkImageView back_buffer);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VkImageView front_visibility_buffer_, back_visibility_buffer_;

};


#endif