#ifndef _G_BUFFER_PIPELINE_H_
#define _G_BUFFER_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

class GBufferPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	inline void SetGBuffer(VulkanRenderTarget* g_buffer) { g_buffer_ = g_buffer; }

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

protected:
	VulkanRenderTarget* g_buffer_;

};

#endif