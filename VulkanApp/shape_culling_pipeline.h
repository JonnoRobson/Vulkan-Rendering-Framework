#ifndef _SHAPE_CULLING_PIPELINE_H_
#define _SHAPE_CULLING_PIPELINE_H_

#include "compute_pipeline.h"

/**
* Vulkan compute pipeline for executing basic geometry culling
*/
class ShapeCullingPipeline : public VulkanComputePipeline
{
public:
	void RecordCommands(VkCommandBuffer& command_buffer);

	inline void SetShapeCount(uint32_t count) { shape_count_ = count; }

protected:
	void CreatePipeline();

protected:
	uint32_t shape_count_;
};

#endif