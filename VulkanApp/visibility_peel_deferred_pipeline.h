#ifndef _VISIBILITY_PEEL_DEFERRED_PIPELINE_H_
#define _VISIBILITY_PEEL_DEFERRED_PIPELINE_H_

#include "pipeline.h"
#include "render_target.h"

struct VisibilityPeelRenderData
{
	glm::vec4 screen_dimensions;
	glm::mat4 invView;
	glm::mat4 invProj;
};

/**
* Vulkan graphics pipeline for deferred rendering using a layered visibility buffer
*/
class VisibilityPeelDeferredPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();
};

#endif