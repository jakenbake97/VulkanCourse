#include "VulkanRenderer.h"

#define VKERROR(VKResult, message) if ((VKResult) != VK_SUCCESS) throw std::runtime_error((message))

VulkanRenderer::VulkanRenderer(GLFWwindow* pWindow)
	:
	window(pWindow),
	instance(nullptr), mainDevice()
{
}

VulkanRenderer::~VulkanRenderer()
{
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);
}

int VulkanRenderer::Initialize()
{
	try
	{
		CreateInstance();
		GetPhysicalDevice();
		CreateLogicalDevice();
	}
	catch (const std::runtime_error& e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void VulkanRenderer::CreateInstance()
{
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

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();

	// Set up extensions that the instance will use
	uint32_t glfwExtensionCount = 0; // GLFW may require multiple extensions

	// Extensions passed as array of cstrings, so this is the pointer to the array or pointers
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; ++i)
	{
		instanceExtensions.push_back(glfwExtensions[i]);
	}

	// check instance extensions supported
	if (!CheckInstanceExtensionSupport(&instanceExtensions))
	{
		VKERROR(-1, "VkInstance does not support required instance extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// TODO: set up validation layers that instance will use
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = nullptr;

	// create instance
	VKERROR(vkCreateInstance(&createInfo, nullptr, &instance), "Failed to create a Vulkan Instance");
}

void VulkanRenderer::CreateLogicalDevice()
{
	// get the queue family indices for the chosen physical device
	QueueFamilyIndices indices = GetQueueFamilies(mainDevice.physicalDevice);
	
	// Queues the logical device needs to create and info to do so
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;    // The index of the family to create a queue from
	queueCreateInfo.queueCount = 1;                               // number of queues to create
	float priority = 1.0f;
	queueCreateInfo.pQueuePriorities = &priority;                 // Vulkan needs to know how to handle multiple queues, so decide priority (1 = highest)
	
	// information to create a logical device (also know just as device)
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;                    // Number of queue create infos
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;        // List of queue create infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = 0;                   // number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = nullptr;           // List of enabled logical device extensions

	// physical device deatures that logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;          // Physical device features logical device will use

	// Create the logical device for the given Physical device
	VKERROR(vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice), "Failed to Create Logical device");

	// Queues are created at the same time as the device, so we want a handle to the queues
	// From a given logical device, of a given queue family, of a given queue index, store our queue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	
}

void VulkanRenderer::GetPhysicalDevice()
{
	// Enumberate the Physical devices the VkInstance can access
	uint32_t deviceCount = 0;
	VKERROR(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "Failed to get physical device count");

	// if no devices available, then none support Vulkan
	if (deviceCount == 0)
	{
		VKERROR(-1, "Can't Find any physical devices that support Vulkan Instance");
	}

	// get list of physical devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	VKERROR(vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data()),
	        "Failed to get list of physical devices");

	for (const auto& device : deviceList)
	{
		if(CheckDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			break;
		}
	}
}

bool VulkanRenderer::CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions)
{
	// need to get the number of extensions to create an array of the correct size to hold the extensions
	uint32_t extensionCount = 0;
	VKERROR(
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr),
		"Failed to get number of extension properties"
	);

	// create a list of vkExtensionsProperties using the count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	VKERROR(
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()),
		"Failed to get list of instance extensions"
	);

	// check if given extensions are in list of available extensions
	for (const auto& checkExtension : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto& extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName))
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

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device)
{
	//// information about the device itself (ID, name, type, vendor, etc)
	//VkPhysicalDeviceProperties deviceProperties;
	//
	//vkGetPhysicalDeviceProperties(device, &deviceProperties);

	//// Information about what the device can do (geo shader, tess shader, wide lines, etc)
	//VkPhysicalDeviceFeatures deviceFeatures;
	//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	QueueFamilyIndices indices = GetQueueFamilies(device);
	
	return indices.IsValid();
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	// get all queue famility property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

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

		// check if queue family indices are in a valid state, stop search if they all are
		if (indices.IsValid())
			break;
		
		++i;
	}

	return indices;
}
