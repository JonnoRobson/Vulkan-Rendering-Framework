#include "app.h"
#include <chrono>
#include <fstream>

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
	// read in mesh filenames before creating window as fullscreen workaround
	std::cout << "Enter model to load: ";
	std::cin >> mesh_filenames_;

	// read in resolution
	std::cout << "Enter window resolution: ";
	std::cin >> window_width_;
	std::cin >> window_height_;

	window_width_ = (window_width_ > 0) ? window_width_ : 1920;
	window_height_ = (window_height_ > 0) ? window_height_ : 1080;

	// read in the multisample level
	std::cout << "Enter multisample level: ";
	std::cin >> multisample_level_;
	multisample_level_ = std::max(1, std::min(8, multisample_level_));

	if (glfwInit() == GLFW_FALSE)
		return false;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// Don't create an opengl context
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		// Window should be resizable

#ifdef NDEBUG
	window_ = glfwCreateWindow(window_width_, window_height_, "Vulkan", glfwGetPrimaryMonitor(), nullptr);
#else	
	window_ = glfwCreateWindow(window_width_, window_height_, "Vulkan", nullptr, nullptr);
#endif

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
	validation_layers_available_ = true;

	// init vulkan instance and debug functionality
	if (!ValidateExtensions())
	{
		throw std::runtime_error("required extensions not found!");
	}

	if (ENABLE_VALIDATION_LAYERS && !CheckValidationLayerSupport())
	{
		validation_layers_available_ = false;
		std::cout << "validation layers requested, but not available!\n";
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
	swap_chain_->CreateSwapChain(devices_, window_width_, window_height_, multisample_level_);
	
	// init the rendering pipeline
	renderer_ = new VulkanRenderer();
	renderer_->Init(devices_, swap_chain_, multisample_level_);
	renderer_->LoadCapturePoints(mesh_filenames_);

	return true;
}

bool App::InitResources()
{
	std::string combined_filepaths = mesh_filenames_;

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
	std::string texture_dir, lightmap_filepath;
	size_t filename_begin = filepaths[0].find_last_of('/');
	size_t filename_end = filepaths[0].find_last_of('.');
	std::string typeless_filename = (filepaths[0].substr(filename_begin + 1, (filename_end - 1) - filename_begin));
	texture_dir = "../res/materials/" + typeless_filename + "/";
	renderer_->SetTextureDirectory(texture_dir);


	for (std::string filepath : filepaths)
	{
		Mesh* loaded_mesh = new Mesh();
		loaded_mesh->CreateModelMesh(devices_, renderer_, filepath);
		loaded_meshes_.push_back(loaded_mesh);
		renderer_->AddMesh(loaded_mesh);
	}

	// load lightmap

	// determine the lightmap filename
	lightmap_filepath = "../res/lightmaps/" + typeless_filename + ".txt";

	std::ifstream file;
	file.open(lightmap_filepath, std::ios::in);
	if (!file.is_open())
	{
		// if there is no lightmap for this file use default setup
		Light* test_light = new Light();
		test_light->SetType(0.0f);
		test_light->SetPosition(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		test_light->SetDirection(glm::vec4(0.0f, -0.15f, -1.0f, 1.0f));
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
		test_light_b->SetIntensity(1.0f);
		test_light_b->SetRange(1.0f);
		test_light_b->SetShadowsEnabled(true);
		test_light_b->Init(devices_, renderer_);
		lights_.push_back(test_light_b);
	}
	else
	{
		// read in the light map
		while (!file.eof())
		{
			Light* light = new Light();
			float data_x, data_y, data_z;
			std::string ignore;

			// read in light type
			file >> data_x;
			if (data_x > 2.0f || data_x < 0.0f) // check eof errors
				break;
			light->SetType(data_x);

			// read in light position
			file >> data_x;
			file >> data_y;
			file >> data_z;
			light->SetPosition(glm::vec4(data_x, data_y, data_z, 1.0f));

			// read in light direction
			file >> data_x;
			file >> data_y;
			file >> data_z;
			light->SetDirection(glm::vec4(data_x, data_y, data_z, 1.0f));

			// read in light color
			file >> data_x;
			file >> data_y;
			file >> data_z;
			light->SetColor(glm::vec4(data_x, data_y, data_z, 1.0f));

			// read in light intensity
			file >> data_x;
			light->SetIntensity(data_x);

			// read in light range
			file >> data_x;
			light->SetRange(data_x);

			// read in shadows enabled
			file >> data_x;
			light->SetShadowsEnabled(data_x == 1.0);

			// read in ignoring transparent geometry
			file >> data_x;
			light->SetIgnoreTransparent(data_x == 1.0);

			// ignore the divider
			file >> ignore;

			// initialize the light and add it to the light list
			light->Init(devices_, renderer_);
			lights_.push_back(light);
		}

		file.close();
	}

	camera_.SetViewDimensions(swap_chain_->GetIntermediateImageExtent().width, swap_chain_->GetIntermediateImageExtent().height);
	camera_.SetFieldOfView(glm::radians(45.0f));
	camera_.SetPosition(glm::vec3(0.0f, -75.0f, 100.0f));
	camera_.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));

	renderer_->SetCamera(&camera_);

	for (Light* light : lights_)
	{
		light->GenerateShadowMap(renderer_->GetCommandPool(), renderer_->GetMeshes());
	}

	renderer_->InitPipelines();

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

	// quitting
	if (input_->IsKeyPressed(GLFW_KEY_ESCAPE))
		glfwSetWindowShouldClose(window_, 1);

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

	// hdr toggle
	if (input_->IsKeyPressed(GLFW_KEY_H))
	{
		renderer_->GetHDR()->CycleHDRMode();
		input_->SetKeyUp(GLFW_KEY_H);
	}

	// renderer timing
	if (input_->IsKeyPressed(GLFW_KEY_ENTER))
	{
		renderer_->StartPerformanceCapture();
		input_->SetKeyUp(GLFW_KEY_ENTER);
	}

	// capture point recording
	if (input_->IsKeyPressed(GLFW_KEY_TAB))
	{
		glm::vec3 cam_pos = camera_.GetPosition();
		glm::vec3 cam_rot = camera_.GetRotation();

		std::string sample_point = "\n" + std::to_string(cam_pos.x) + "\n";
		sample_point += std::to_string(cam_pos.y) + "\n";
		sample_point += std::to_string(cam_pos.z) + "\n";
		sample_point += std::to_string(cam_rot.x) + "\n";
		sample_point += std::to_string(cam_rot.y) + "\n";
		sample_point += std::to_string(cam_rot.z);

		VulkanDevices::AppendFile("../res/data/capture_points/" + mesh_filenames_ + "_capture_points.txt", sample_point);

		input_->SetKeyUp(GLFW_KEY_TAB);
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
	if (ENABLE_VALIDATION_LAYERS && validation_layers_available_)
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
	device_features.multiDrawIndirect = VK_TRUE;
	device_features.geometryShader = VK_TRUE;
	device_features.shaderStorageImageMultisample = VK_TRUE;
	device_features.sampleRateShading = VK_TRUE;

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
	swap_chain_->CreateSwapChain(devices_, window_width_, window_height_, multisample_level_);
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
