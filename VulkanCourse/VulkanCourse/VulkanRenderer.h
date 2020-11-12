#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer(GLFWwindow* pWindow);
	~VulkanRenderer();
	int Initialize();
private:
	// Vulkan Functions
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	

	// - Getter Functions
	void GetPhysicalDevice();

	// - Support Functions
	// - - Checker Functions
	static bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceSuitable(VkPhysicalDevice device);

	// - - Getter Functions
	static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	
private:
	GLFWwindow* window = nullptr;

	// Vulkan Components
	VkInstance instance;

	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;
};