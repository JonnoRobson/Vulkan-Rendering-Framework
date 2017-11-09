#ifndef _APP_H_
#define _APP_H_

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <algorithm>

#include "swap_chain.h"
#include "renderer.h"
#include "mesh.h"
#include "device.h"
#include "texture.h"
#include "light.h"

/*
	 " // C" denotes functions or variables with compartmentalised alternatives
*/

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);

#ifdef NDEBUG
#define ENABLE_VALIDATION_LAYERS false
#else
#define ENABLE_VALIDATION_LAYERS false
#endif

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct LightBufferObject
{
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 color;
	float range;
	float intensity;
	float light_type;
	float shadows_enabled;
};

class App
{
public:
	virtual void Run();

	void RecreateSwapChain();


public:

protected:
	virtual bool InitVulkan();
	virtual bool InitResources();
	virtual bool InitWindow();
	virtual void MainLoop();
	virtual void CleanUp();

	virtual void Update();
	virtual void DrawFrame();

	virtual bool CreateInstance();
	bool ValidateExtensions();
	virtual std::vector<const char*> GetRequiredExtensions();
	virtual bool CheckValidationLayerSupport();
	void SetupDebugCallback();
	void InitDevices();

protected:
	GLFWwindow *window_;
	VulkanDevices *vk_devices_;
	VulkanSwapChain* swap_chain_;
	VulkanRenderer* renderer_;

	VkInstance vk_instance_;
	VkDebugReportCallbackEXT vk_callback_;

	Mesh* chalet_mesh_;
	Mesh* test_mesh_;
	Light* test_light_;

	const int window_width_ = 800;
	const int window_height_ = 600;

	const std::vector<const char*> validation_layers_ = {
		"VK_LAYER_LUNARG_standard_validation",
		"VK_LAYER_LUNARG_monitor"
	};

	const std::vector<const char*> device_extensions_ = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	std::string debug_msg_;

protected:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData);

	static void OnWindowResized(GLFWwindow* window, int width, int height);
};


#endif