#ifndef _VISIBILITY_PEEL_PIPELINE_H_
#define _VISIBILITY_PEEL_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

#include <glm/glm.hpp>

#define VISIBILITY_PEEL_COUNT 2

struct VisibilityPeelData
{
	uint32_t pass_number;
	glm::vec2 screen_dimensions;
	float padding;
};

class VisibilityPeelPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	void SetVisibilityBuffer(VulkanRenderTarget* visibility_buffer, int pass_num);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	int pass_num_;
	VulkanRenderTarget* visibility_buffer_;
};

#endif