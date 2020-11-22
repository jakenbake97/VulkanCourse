#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <vector>
#include <set>
#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer(GLFWwindow* pWindow);
	~VulkanRenderer();
private:
	// Vulkan Functions
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();

	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void SetupDebugMessenger();
	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
	                                             const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	                                             const VkAllocationCallbacks* pAllocator,
	                                             VkDebugUtilsMessengerEXT* pDebugMessenger);
	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	                                          const VkAllocationCallbacks* pAllocator);

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
	                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	                                                    void* pUserData);

	// - Getter Functions
	void GetPhysicalDevice();

	// - Support Functions
	// - - Checker Functions
	static bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	bool CheckDeviceSuitable(VkPhysicalDevice device);
	bool CheckValidationLayerSupport() const;

	// - - Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
	std::vector<const char*> GetRequiredGLFWExtensions() const;
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device);

	// - - Chooser Functions
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities);
	static VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	static VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);

	// - - Create Functions
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
private:
	GLFWwindow* window = nullptr;

	// Vulkan Components
	VkInstance instance = nullptr;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	struct
	{
		VkPhysicalDevice physicalDevice = nullptr;
		VkDevice logicalDevice = nullptr;
	} mainDevice;

	VkQueue graphicsQueue = nullptr;
	VkQueue presentationQueue = nullptr;
	VkSurfaceKHR surface{};
	VkSwapchainKHR swapchain;
	std::vector<SwapChainImage> swapChainImages;

	// Vulkan Utilities
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif
};
