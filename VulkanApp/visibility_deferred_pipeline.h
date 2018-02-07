#ifndef _VISIBILITY_DEFERRED_PIPELINE_H_
#define _VISIBILITY_DEFERRED_PIPELINE_H_

#include <glm\glm.hpp>

#include "pipeline.h"

struct VisibilityRenderData
{
	glm::vec4 screen_dimensions;
	glm::mat4 invView;
	glm::mat4 invProj;
};

class VisibilityDeferredPipeline : public VulkanPipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index);

protected:
	void CreatePipeline();
	void CreateRenderPass();
	void CreateFramebuffers();

};

#endif