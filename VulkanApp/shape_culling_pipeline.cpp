#include "shape_culling_pipeline.h"

void ShapeCullingPipeline::RecordCommands(VkCommandBuffer& command_buffer)
{
	// bind the pipeline
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);

	// bind the descriptor sets
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

	// set push constant for shape count
	vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &shape_count_);

	// determine workgroup counts
	uint32_t workgroup_size_x = 32;

	uint32_t workgroup_count_x = shape_count_ / workgroup_size_x;
	if (shape_count_ % workgroup_size_x > 0)
		workgroup_count_x++;
	
	vkCmdDispatch(command_buffer, workgroup_count_x, 1, 1);
}

void ShapeCullingPipeline::CreatePipeline()
{
	// setup push constant info
	VkPushConstantRange push_constant = {};
	push_constant.size = sizeof(uint32_t);
	push_constant.offset = 0;
	push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_constant;

	// create the pipeline layout
	if (vkCreatePipelineLayout(devices_->GetLogicalDevice(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup pipeline creation info
	VkComputePipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_info.stage = shader_->GetShaderStageInfo();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateComputePipelines(devices_->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create deferred compute pipeline!");
	}
}
