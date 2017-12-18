#ifndef _SWAP_CHAIN_H_
#define _SWAP_CHAIN_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "device.h"

class VulkanSwapChain
{
public:	
	void Init(GLFWwindow* window, VkInstance instance);
	void Cleanup();
	void CreateSwapChain(VulkanDevices* devices);

	VkResult PreRender();
	VkResult PostRender(VkSemaphore signal_semaphore);
	void FinalizeIntermediateImage();

	VkFormat FindDepthFormat();

	VkSemaphore GetImageAvailableSemaphore() { return image_available_semaphore_; }
	uint32_t GetCurrentSwapChainImage() { return current_image_index_; }
	VkSurfaceKHR GetSurface() { return surface_; }
	VkSwapchainKHR GetSwapChain() { return swap_chain_; }
	std::vector<VkImage>& GetSwapChainImages() { return swap_chain_images_; }
	std::vector<VkImageView>& GetSwapChainImageViews() { return swap_chain_image_views_; }
	VkImage GetIntermediateImage() { return intermediate_image_; }
	VkImageView GetIntermediateImageView() { return intermediate_image_view_; }
	VkFormat GetSwapChainImageFormat() { return swap_chain_image_format_; }
	VkExtent2D GetSwapChainExtent() { return swap_chain_extent_; }
	VkImage GetDepthImage() { return depth_image_; }
	VkImageView GetDepthImageView() { return depth_image_view_; }

protected:
	void CreateSurface();
	void CreateImageViews();
	void CreateIntermediateImage();
	void CreateDepthResources();
	void CreateSemaphores();
	void CleanupSwapChain();

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> available_present_modes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);


protected:
	VkInstance instance_;
	GLFWwindow* window_;
	VulkanDevices* devices_;

	// swap chain components
	VkSurfaceKHR surface_;
	VkSwapchainKHR swap_chain_;
	std::vector<VkImage> swap_chain_images_;
	std::vector<VkImageView> swap_chain_image_views_;
	VkImage intermediate_image_;
	VkDeviceMemory intermediate_image_memory_;
	VkImageView intermediate_image_view_;
	VkFormat swap_chain_image_format_;
	VkExtent2D swap_chain_extent_;

	// depth buffer components
	VkImage depth_image_;
	VkDeviceMemory depth_image_memory_;
	VkImageView depth_image_view_;
	VkFormat depth_format_;

	// swap chain presentation components
	uint32_t current_image_index_;
	VkSemaphore image_available_semaphore_;
	VkQueue present_queue_;
};


#endif