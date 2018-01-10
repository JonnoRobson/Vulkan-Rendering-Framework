#include "app.h"
#include <chrono>

void App::Run()
{
	if (InitWindow())
	{
		if (InitVulkan())
		{
			if (InitResources())
			{
				MainLoop();
			}
		}
	}
	CleanUp();
}

bool App::InitWindow()
{
	if (glfwInit() == GLFW_FALSE)
		return false;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// Don't create an opengl context
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		// Window should be resizable

	//window_ = glfwCreateWindow(1920, 1080, "Vulkan", glfwGetPrimaryMonitor(), nullptr);
	window_ = glfwCreateWindow(window_width_, window_height_, "Vulkan", nullptr, nullptr);

	glfwSetWindowUserPointer(window_, this);
	glfwSetWindowSizeCallback(window_, App::OnWindowResized);
	glfwSetKeyCallback(window_, keyCallback);

	input_ = new Input();

	if (!window_)
		return false;

	current_time_ = 0.0f;
	prev_time_ = 0.0f;
	frame_time_ = 0.0f;

	return true;
}

bool App::InitVulkan()
{
	// init vulkan instance and debug functionality
	if (!ValidateExtensions())
	{
		throw std::runtime_error("required extensions not found!");
	}

	if (ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport())
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	if (!CreateInstance())
		return false;

	SetupDebugCallback();

	// init the window surface
	swap_chain_ = new VulkanSwapChain();
	swap_chain_->Init(window_, vk_instance_);

	// init the physical and logical devices
	InitDevices();

	// init the swap chain
	swap_chain_->CreateSwapChain(devices_);

	// init the rendering pipeline
	renderer_ = new VulkanRenderer();
	renderer_->Init(devices_, swap_chain_, "../res/shaders/default_material.vert.spv", "../res/shaders/default_material.frag.spv");

	return true;
}

bool App::InitResources()
{
	std::string combined_filepaths;

	std::cout << "Select model to load: ";

	std::cin >> combined_filepaths;

	// separate out model filepaths
	std::vector<std::string> filepaths;
	int path_start = 0;
	for (int i = 0; i < combined_filepaths.length(); i++)
	{
		if (combined_filepaths[i] == ',')
		{
			std::string filepath = "../res/models/" + combined_filepaths.substr(path_start, i - path_start);
			filepaths.push_back(filepath);
			path_start = i;
		}
	}

	// add the trailing file path
	int last_path_start = combined_filepaths.find_last_of(',');
	if (last_path_start != std::string::npos)
	{
		// find and add the last filepath
		std::string filepath = "../res/models/" + combined_filepaths.substr(last_path_start + 1);
		filepaths.push_back(filepath);
	}
	else
	{
		// only one filename is contained
		filepaths.push_back("../res/models/" + combined_filepaths);
	}

	// set the first filepath used as the material directory for this load
	std::string texture_dir;
	size_t filename_begin = filepaths[0].find_last_of('/');
	size_t filename_end = filepaths[0].find_last_of('.');
	texture_dir = "../res/materials/" + (filepaths[0].substr(filename_begin + 1, (filename_end - 1) - filename_begin)) + "/";
	renderer_->SetTextureDirectory(texture_dir);

	for (std::string filepath : filepaths)
	{
		Mesh* loaded_mesh = new Mesh();
		loaded_mesh->CreateModelMesh(devices_, renderer_, filepath);
		loaded_meshes_.push_back(loaded_mesh);
	}

	
	Light* test_light = new Light();
	test_light->SetType(0.0f);
	test_light->SetPosition(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	test_light->SetDirection(glm::vec4(0.0f, -0.15f, -1.0f, 1.0f));
	//test_light->SetColor(glm::vec4(0.23f, 0.19f, 0.34f, 1.0f));
	test_light->SetColor(glm::vec4(1.0f, 0.94f, 0.88f, 1.0f));
	test_light->SetIntensity(1.0f);
	test_light->SetRange(1.0f);
	test_light->SetShadowsEnabled(true);
	test_light->Init(devices_, renderer_);
	lights_.push_back(test_light);
	
	Light* test_light_b = new Light();
	test_light_b->SetType(0.0f);
	test_light_b->SetPosition(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	test_light_b->SetDirection(glm::vec4(0.0f, 0.15f, -1.0f, 1.0f));
	test_light_b->SetColor(glm::vec4(1.0f, 0.94f, 0.88f, 1.0f));
	//test_light_b->SetColor(glm::vec4(0.11f, 0.24f, 0.89f, 1.0f));
	test_light_b->SetIntensity(1.0f);
	test_light_b->SetRange(1.0f);
	test_light_b->SetShadowsEnabled(true);
	test_light_b->Init(devices_, renderer_);
	lights_.push_back(test_light_b);

	/*
	Light* test_light_c = new Light();
	test_light_c->SetType(2.0f);
	test_light_c->SetPosition(glm::vec4(0.0f, 100.0f, 10.0f, 1.0f));
	test_light_c->SetDirection(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	test_light_c->SetColor(glm::vec4(0.95f, 0.95f, 0.95f, 1.0f));
	test_light_c->SetIntensity(2.0f);
	test_light_c->SetRange(10000.0f);
	test_light_c->SetShadowsEnabled(true);
	test_light_c->Init(devices_, renderer_);
	lights_.push_back(test_light_c);
	*/

	camera_.SetViewDimensions(swap_chain_->GetSwapChainExtent().width, swap_chain_->GetSwapChainExtent().height);
	camera_.SetFieldOfView(glm::radians(45.0f));
	camera_.SetPosition(glm::vec3(0.0f, -3.0f, 2.0f));
	camera_.SetRotation(glm::vec3(-30.0f, 0.0f, 0.0f));

	renderer_->InitPipelines();

	for (Mesh* mesh : loaded_meshes_)
	{
		renderer_->AddMesh(mesh);
	}

	renderer_->SetCamera(&camera_);

	for (Light* light : lights_)
	{
		light->GenerateShadowMap();
	}

	return true;
}

void App::CleanUp()
{
	// clean up resources	
	for (Mesh* mesh : loaded_meshes_)
	{
		delete mesh;
		mesh = nullptr;
	}
	loaded_meshes_.clear();

	for (Light* light : lights_)
	{
		light->Cleanup();
		delete light;
		light = nullptr;
	}
	lights_.clear();

	// clean up input manager
	delete input_;
	input_ = nullptr;

	// clean up swap chain
	swap_chain_->Cleanup();
	delete swap_chain_;
	swap_chain_ = nullptr;

	// clean up renderer
	renderer_->Cleanup();
	delete renderer_;
	renderer_ = nullptr;

	// clean up devices
	delete devices_;
	devices_ = nullptr;

	// destroy debug callback and instance
	DestroyDebugReportCallbackEXT(vk_instance_, vk_callback_, nullptr);
	vkDestroyInstance(vk_instance_, nullptr);

	// cleanup glfw
	glfwDestroyWindow(window_);
	glfwTerminate();
}

void App::MainLoop()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();
		Update();
		DrawFrame();
	}

	vkDeviceWaitIdle(devices_->GetLogicalDevice());
}

void App::Update()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();

	prev_time_ = current_time_;
	current_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;
	frame_time_ = current_time_ - prev_time_;

	// camera movement
	if (input_->IsKeyPressed(GLFW_KEY_W))
		camera_.MoveForward(frame_time_);
	else if (input_->IsKeyPressed(GLFW_KEY_S))
		camera_.MoveBackward(frame_time_);
	
	if (input_->IsKeyPressed(GLFW_KEY_A))
		camera_.MoveLeft(frame_time_);
	else if (input_->IsKeyPressed(GLFW_KEY_D))
		camera_.MoveRight(frame_time_);

	// camera turning
	if (input_->IsKeyPressed(GLFW_KEY_UP))
		camera_.TurnUp(frame_time_ * 100.0f);
	else if (input_->IsKeyPressed(GLFW_KEY_DOWN))
		camera_.TurnDown(frame_time_ * 100.0f);
	else if (input_->IsKeyPressed(GLFW_KEY_LEFT))
		camera_.TurnLeft(frame_time_ * 100.0f);
	else if (input_->IsKeyPressed(GLFW_KEY_RIGHT))
		camera_.TurnRight(frame_time_ * 100.0f);

	// camera speed
	if (input_->IsKeyPressed(GLFW_KEY_Q))
	{
		float current_speed = camera_.GetSpeed();
		current_speed += 10.0f;
		camera_.SetSpeed(current_speed);
		input_->SetKeyUp(GLFW_KEY_Q);
	}
	else if (input_->IsKeyPressed(GLFW_KEY_E))
	{
		float current_speed = camera_.GetSpeed();
		current_speed -= 10.0f;
		if (current_speed <= 0.0f)
		{
			current_speed = 1.0f;
		}
		camera_.SetSpeed(current_speed);
		input_->SetKeyUp(GLFW_KEY_E);
	}

	// render mode switch
	if (input_->IsKeyPressed(GLFW_KEY_TAB))
	{
		renderer_->SetRenderMode(VulkanRenderer::RenderMode::FORWARD);
		input_->SetKeyUp(GLFW_KEY_TAB);
	}
	else if (input_->IsKeyPressed(GLFW_KEY_LEFT_SHIFT))
	{
		renderer_->SetRenderMode(VulkanRenderer::RenderMode::DEFERRED);
		input_->SetKeyUp(GLFW_KEY_LEFT_SHIFT);
	}
	else if (input_->IsKeyPressed(GLFW_KEY_LEFT_ALT))
	{
		renderer_->SetRenderMode(VulkanRenderer::RenderMode::BUFFER_VIS);
		input_->SetKeyUp(GLFW_KEY_LEFT_ALT);
	}
	else if (input_->IsKeyPressed(GLFW_KEY_LEFT_CONTROL))
	{
		renderer_->SetRenderMode(VulkanRenderer::RenderMode::DEFERRED_COMPUTE);
		input_->SetKeyUp(GLFW_KEY_LEFT_CONTROL);
	}

	// hdr toggle
	if (input_->IsKeyPressed(GLFW_KEY_H))
	{
		renderer_->GetHDR()->CycleHDRMode();
		input_->SetKeyUp(GLFW_KEY_H);
	}
}

void App::DrawFrame()
{
	// acquire the next image in the swap chain
	VkResult result = swap_chain_->PreRender();

	// recreate the swap chain if it is out of date
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// draw the scene using the renderer
	renderer_->RenderScene();
	
	// present the swap chain image to the window
	result = swap_chain_->PostRender(renderer_->GetSignalSemaphore());

	// recreate the swap chain if it is out of date or non-optimal
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}
}

bool App::CreateInstance()
{
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Vulkan Renderer";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;

	std::vector<const char*> extensions = GetRequiredExtensions();
	create_info.enabledExtensionCount = (uint32_t)extensions.size();
	create_info.ppEnabledExtensionNames = extensions.data();

	// if running debug then enable validation layers
	if (ENABLE_VALIDATION_LAYERS)
	{
		create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
		create_info.ppEnabledLayerNames = validation_layers_.data();
	}
	else
	{
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&create_info, nullptr, &vk_instance_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create instance!");
	}

	return true;
}

bool App::ValidateExtensions()
{
	uint32_t extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

	std::cout << "available extensions:" << std::endl;

	for (const auto& extension : extensions)
		std::cout << "\t" << extension.extensionName << std::endl;

	std::vector<const char*> required_extensions = GetRequiredExtensions();

	bool extensions_found = true;

	for (int i = 0; i < required_extensions.size(); i++)
	{
		bool found = false;

		for (const auto& extension : extensions)
		{
			if (strcmp(required_extensions[i], extension.extensionName) == 0)
				found = true;
		}

		if (!found)
		{
			extensions_found = false;
			std::cout << "extension not found: " << required_extensions[i] << std::endl;
		}
	}

	if (extensions_found)
		return true;
	else
		return false;
}

std::vector<const char*> App::GetRequiredExtensions()
{
	std::vector<const char*> extensions;

	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	for (unsigned int i = 0; i < glfw_extension_count; i++)
	{
		extensions.push_back(glfw_extensions[i]);
	}

	if (ENABLE_VALIDATION_LAYERS)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

bool App::CheckValidationLayerSupport()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char* layer_name : validation_layers_)
	{
		bool layer_found = false;

		for (const auto& layer_properties : available_layers)
		{
			if (strcmp(layer_name, layer_properties.layerName) == 0)
			{
				layer_found = true;
				break;
			}
		}

		if (!layer_found)
		{
			return false;
		}
	}

	return true;
}

void App::SetupDebugCallback()
{
	if (!ENABLE_VALIDATION_LAYERS)
		return;

	// delete existing error file
	remove("error.txt");

	VkDebugReportCallbackCreateInfoEXT create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	create_info.pfnCallback = debugCallback;

	if (CreateDebugReportCallbackEXT(vk_instance_, &create_info, nullptr, &vk_callback_) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug callback!");
	}
}

void App::InitDevices()
{
	VkPhysicalDeviceFeatures device_features = {};
	device_features.samplerAnisotropy = VK_TRUE;
	device_features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
	device_features.independentBlend = VK_TRUE;

	// create the physical device
	devices_ = new VulkanDevices(vk_instance_, swap_chain_->GetSurface(), device_features, device_extensions_);

	// setup requirements for logical device
	QueueFamilyIndices indices = devices_->GetQueueFamilyIndices();
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<int> unique_queue_families = { indices.graphics_family, indices.present_family };

	float queue_priority = 1.0f;
	for (int queueFamily : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = indices.graphics_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	// create the logical device
	devices_->CreateLogicalDevice(device_features, queue_create_infos, device_extensions_, validation_layers_);
}


void App::RecreateSwapChain()
{
	swap_chain_->CreateSwapChain(devices_);
	renderer_->RecreateSwapChainFeatures();
}

VKAPI_ATTR VkBool32 VKAPI_CALL App::debugCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData) {

	std::cerr << "validation layer: " << msg << std::endl;
	VulkanDevices::WriteDebugFile(msg);

	return VK_FALSE;
}

void App::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
		app->input_->SetKeyDown(key);
	}
	else if (action == GLFW_RELEASE)
	{
		App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
		app->input_->SetKeyUp(key);
	}
}

void App::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
	app->input_->SetCursorPosition(xpos, ypos);
}

void App::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
		app->input_->SetMouseButtonDown(button);
	}
	else if (action == GLFW_RELEASE)
	{
		App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
		app->input_->SetMouseButtonUp(button);
	}
}

void App::OnWindowResized(GLFWwindow* window, int width, int height)
{
	if (width == 0 || height == 0)
		return;

	
	App* app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
	//app->RecreateSwapChain();
}

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{

	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr)
		return func(instance, pCreateInfo, pAllocator, pCallback);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
	{
		func(instance, callback, pAllocator);
	}
}
