#include "deferred_compute_pipeline.h"

void DeferredComputePipeline::RecordCommands(VkCommandBuffer& command_buffer, uint32_t buffer_index)
{
	// bind the pipeline
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);

	// bind the descriptor sets
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0, 1, &descriptor_set_, 0, nullptr);

	// determine workgroup counts
	uint32_t workgroup_size_x = 32;
	uint32_t workgroup_size_y = 32;

	VkExtent2D swap_extent = swap_chain_->GetIntermediateImageExtent();

	uint32_t workgroup_count_x = swap_extent.width / workgroup_size_x;
	if (swap_extent.width % workgroup_size_x > 0)
		workgroup_count_x++;

	uint32_t workgroup_count_y = swap_extent.height / workgroup_size_y;
	if (swap_extent.height % workgroup_size_y > 0)
		workgroup_count_y++;

	vkCmdDispatch(command_buffer, workgroup_count_x, workgroup_count_y, 1);
}

void DeferredComputePipeline::CreatePipeline()
{
	// setup pipeline layout creation info
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout_;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = 0;
	
	// create the pipeline layout
	if (vkCreatePipelineLayout(devices_->GetLogicalDevice(), &pipeline_layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	// setup pipeline creation info
	VkComputePipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_info.stage = compute_shader_->GetShaderStageInfo();
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.flags = 0;

	if (vkCreateComputePipelines(devices_->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create deferred compute pipeline!");
	}
}

void DeferredComputePipeline::CreateFramebuffers()
{
	// we don't use any framebuffers for a compute pipeline
}

void DeferredComputePipeline::CreateRenderPass()
{
	// we don't use any render passes for a compute pipeline
}