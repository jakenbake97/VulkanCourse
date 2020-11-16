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
	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void SetupDebugMessenger();
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	                                             const VkAllocationCallbacks* pAllocator,
	                                             VkDebugUtilsMessengerEXT* pDebugMessenger);
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	// - Getter Functions
	void GetPhysicalDevice();

	// - Support Functions
	// - - Checker Functions
	static bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	static bool CheckDeviceSuitable(VkPhysicalDevice device);
	bool CheckValidationLayerSupport() const;

	// - - Getter Functions
	static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	std::vector<const char*> GetRequiredGLFWExtensions() const;


	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
	                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	                                                    void* pUserData);
private:
	GLFWwindow* window = nullptr;

	// Vulkan Components
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;

	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;

	VkQueue graphicsQueue;

	const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif
};
