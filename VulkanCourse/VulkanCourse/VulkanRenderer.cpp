#include "VulkanRenderer.h"
#include <algorithm>

VulkanRenderer::VulkanRenderer(GLFWwindow* pWindow)
	:
	window(pWindow)
{
	CreateInstance();
	CreateSurface();
	GetPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
}

VulkanRenderer::~VulkanRenderer()
{
	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	for(const auto& image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::CreateInstance()
{
	// Check to see if the application is requesting validation layers, and if so, make sure they are supported
	if (enableValidationLayers && !CheckValidationLayerSupport())
	{
		VK_ERROR(-1, "Validation layers requested, but not available or supported");
	}

	// info about the application, not just the Vulkan parts
	// most data here doesn't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Half-Way Engine - Vulkan"; // Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0); // custom application version
	appInfo.pEngineName = "Half-Way Engine"; // custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0); // custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_0; // Vulkan Version


	// Creation information for a Vulkan Instance
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	// add the validation layers in the Instance create info struct if they are enabled
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = static_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
	}
	else
	{
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = GetRequiredGLFWExtensions();

	// check instance extensions supported
	if (!CheckInstanceExtensionSupport(&instanceExtensions))
	{
		VK_ERROR(-1, "VkInstance does not support required instance extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// create instance
	VK_ERROR(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create a Vulkan Instance");
	SetupDebugMessenger();
}

void VulkanRenderer::CreateLogicalDevice()
{
	// get the queue family indices for the chosen physical device
	const QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information, and set for family indices
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentationFamily};

	float priority = 1.0f;
	
	// Queues the logical device needs to create and info to do so
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex; // The index of the family to create a queue from
		queueCreateInfo.queueCount = 1; // number of queues to create
		queueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// information to create a logical device (also know just as device)
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	// Number of queue create infos
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	// List of queue create infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	// number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); // List of enabled logical device extensions

	// physical device features that logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures; // Physical device features logical device will use

	// Create the logical device for the given Physical device
	VK_ERROR(vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice),
	         "Failed to Create Logical device");

	// Queues are created at the same time as the device, so we want a handle to the queues
	// From a given logical device, of a given queue family, of a given queue index, store our queue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::CreateSurface()
{
	// Creates a surface create info struct and runs the appropriate create surface function for the user's system
	VK_ERROR(glfwCreateWindowSurface(instance, window, nullptr, &surface), "GLFW failed to create a window surface");
}

void VulkanRenderer::CreateSwapChain()
{
	// Get Swap chain details so we can pick best settings
	const SwapChainDetails swapChainDetails = GetSwapChainDetails(mainDevice.physicalDevice);

	// find optimal surface values for our swap chain
	const VkSurfaceFormatKHR surfaceFormat = ChooseBestSurfaceFormat(swapChainDetails.formats);
	const VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);
	const VkExtent2D extent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swap chain? get 1 more than the minimum to allow for triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}
	
	// Swap chain creation info
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.imageExtent = extent;
	swapchainCreateInfo.minImageCount = imageCount;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;

	// Get Queue Family Indices
	const QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);

	// If Graphics and Presentation families are different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		uint32_t queueFamilyIndices[] = {static_cast<uint32_t>(indices.graphicsFamily), static_cast<uint32_t>(indices.presentationFamily)};
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// if old swap chain has been destroy and this one replaces it, then link old one to quickly hand over responsibilities
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// create swapchain
	VK_ERROR(vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain), "Failed to create swapchain");

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	// Get swap chain images (first count, then values)
	uint32_t swapChainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, nullptr);

	if (swapChainImageCount == 0)
	{
		VK_ERROR(-1, "Failed to get number of images in swapchain");
	}
	
	std::vector<VkImage> images(swapChainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapChainImageCount, images.data());

	for(VkImage image : images)
	{
		// store image handle
		SwapChainImage swapChainImage{
		swapChainImage.image = image,
		swapChainImage.imageView = CreateImageView(image, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT)
		};

		// Add to swapchain image list
		swapChainImages.push_back(swapChainImage);
	}
}

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr; // optional
}

void VulkanRenderer::SetupDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	VK_ERROR(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger),
	         "Failed to set up debug messenger");
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(VkInstance instance,
                                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                      const VkAllocationCallbacks* pAllocator,
                                                      VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	const auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                                   const VkAllocationCallbacks* pAllocator)
{
	const auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

void VulkanRenderer::GetPhysicalDevice()
{
	// Enumerate the Physical devices the VkInstance can access
	uint32_t deviceCount = 0;
	VK_ERROR(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "Failed to get physical device count");

	// if no devices available, then none support Vulkan
	if (deviceCount == 0)
	{
		VK_ERROR(-1, "Can't Find any physical devices that support Vulkan Instance");
	}

	// get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	VK_ERROR(vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data()),
	         "Failed to get list of physical devices");

	for (const auto& device : deviceList)
	{
		if (CheckDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}

	if (mainDevice.physicalDevice == nullptr)
	{
		VK_ERROR(-1, "Failed find suitable physical device");
	}
}

bool VulkanRenderer::CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	// need to get the number of extensions to create an array of the correct size to hold the extensions
	uint32_t extensionCount = 0;
	VK_ERROR(
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
		"Failed to get number of extension properties"
	);

	// create a list of vkExtensionsProperties using the count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	VK_ERROR(
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()),
		"Failed to get list of instance extensions"
	);

	// check if given extensions are in list of available extensions
	for (const auto& checkExtension : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}

		if (!hasExtension)
			return false;
	}

	return true;
}

bool VulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
	// get device extension count
	uint32_t extensionCount = 0;
	VK_ERROR(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr),
		"Failed to get number of device extension properties"
	);

	// if no extensions found return false
	if (extensionCount == 0)
		return false;

	// populate list of extensions
	std::vector<VkExtensionProperties> extensions(extensionCount);
	VK_ERROR(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()), 
		"Failed to get list of device extensions"
	);

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	
	// check for extension
	for (const auto& extension : extensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device)
{
	const QueueFamilyIndices indices = GetQueueFamilies(device);

	const bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainValid = false;

	if (extensionsSupported)
	{
		const SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.IsValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::CheckValidationLayerSupport() const
{
	uint32_t layerCount;
	VK_ERROR(vkEnumerateInstanceLayerProperties(&layerCount, nullptr),
	         "Failed to get the layer properties available to the instance");

	std::vector<VkLayerProperties> layerList(layerCount);
	VK_ERROR(vkEnumerateInstanceLayerProperties(&layerCount, layerList.data()),
	         "Failed to return available layers in layer list");

	for (const auto* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : layerList)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	// get all queue family property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	if (queueFamilyCount == 0)
	{
		VK_ERROR(-1, "Failed to get any physical device queue properties");
	}

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// go through each queue family and check if it has at least 1 of the required types of queue
	int i = 0;
	for (const auto& queueFamily : queueFamilyList)
	{
		// First check if queue family has at least 1 queue in that family (could have no queues).
		// Queue can be multiple types defined through bitFlags. Need to bitwise AND with VK_QUEUE_*_BIT
		// to check if has required type
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i; // If queue family is valid, then get index
		}

		// check if queue family supports presentation
		VkBool32 presentationSupport = false;
		VK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport),
		         "Failed to check physical device surface support");

		// check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		// check if queue family indices are in a valid state, stop search if they all are
		if (indices.IsValid())
			break;

		++i;
	}

	return indices;
}

std::vector<const char*> VulkanRenderer::GetRequiredGLFWExtensions() const
{
	// Set up extensions that the instance will use
	uint32_t glfwExtensionCount = 0; // GLFW may require multiple extensions

	// Extensions passed as array of c-strings, so this is the pointer to the array or pointers
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// Surface capabilities
	VK_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities),
	         "Failed to get physical device surface capabilities"
	);


	// Formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}

	// Presentation modes
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount,
		                                          swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities)
{
	// if current extent is at numeric limits, then extent can vary. Otherwise, it is the size of the window.
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height); // This gets the resolution in pixels (instead of screen coordinates 

		// Surface also defines max and min, so make sure it is within boundaries by clamping values
		return
		{
			std::max(surfaceCapabilities.minImageExtent.width,
			         std::min(surfaceCapabilities.maxImageExtent.width, static_cast<uint32_t>(width))),
			
			std::max(surfaceCapabilities.minImageExtent.height,
			         std::min(surfaceCapabilities.maxImageExtent.height, static_cast<uint32_t>(height)))
		};
	}
}

// Best format is subjective, but this case will use
// format : VK_FORMAT_R8G8B8A8_SRGB (VK_FORMAT_B8G8R8A8_SRGB as backup)
// colorSpace : VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// if only 1 format available and is undefined, then this means ALL formats are available
	if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	// if restricted search for preferred format
	for (const auto& format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_SRGB || format.format == VK_FORMAT_B8G8R8A8_SRGB) && format.
			colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	for (const auto& presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}

	// if mailbox isn't found, uses FIFO as Vulkan spec says it must always be available
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkImageView VulkanRenderer::CreateImageView(const VkImage image, const VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	// Create image view and return it
	VkImageView imageView;
	VK_ERROR(vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView), "Failed to create image view");

	return imageView;
}

VkBool32 VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                       void* pUserData)
{
	printf("Validation layer: %s \n", pCallbackData->pMessage);
	return VK_FALSE;
}
