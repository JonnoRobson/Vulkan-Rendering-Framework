#ifndef _DEFERRED_COMPUTE_PIPELINE_H_
#define _DEFERRED_COMPUTE_PIPELINE_H_

#include "pipeline.h"
#include "compute_shader.h"

class DeferredComputePipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

	inline void SetComputeShader(VulkanComputeShader* shader) { compute_shader_ = shader; }

protected:
	void CreateRenderPass();
	void CreateFramebuffers();
	void CreatePipeline();

protected:
	VulkanComputeShader* compute_shader_;
};

#endif