#ifndef _VISIBILITY_PIPELINE_H_
#define _VISIBILITY_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

class VisibilityPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	inline void SetVisibilityBuffer(VulkanRenderTarget* visibility_buffer) { visibility_buffer_ = visibility_buffer; }

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VulkanRenderTarget* visibility_buffer_;
};

#endif