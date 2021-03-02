#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
#include <set>
#include <array>
#include <glm/glm.hpp>
#include <glm/glm.hpp>

#include "stb_image.h"

#include "Utilities.h"
#include "Mesh.h"

class VulkanRenderer
{
private:
	GLFWwindow* window = nullptr;

	int currentFrame = 0;

	// Scene Objects
	std::vector<Mesh> meshList;

	// Scene Settings
	struct UboViewProjection
	{
		glm::mat4 view;
		glm::mat4 projection;
	}uboViewProjection;
	
	// Vulkan Components
	VkInstance instance = nullptr;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	struct Device
	{
		VkPhysicalDevice physicalDevice = nullptr;
		VkDevice logicalDevice = nullptr;
	} mainDevice;

	VkQueue graphicsQueue = nullptr;
	VkQueue presentationQueue = nullptr;
	VkSurfaceKHR surface{};
	VkSwapchainKHR swapchain{};
	
	std::vector<SwapChainImage> swapChainImages;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkImage depthBufferImage;
	VkDeviceMemory depthBufferImageMemory;
	VkImageView depthBufferImageView;

	VkSampler textureSampler;
	
	std::vector<VkCommandBuffer> commandBuffers;

	// Descriptors
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkPushConstantRange pushConstantRange;
	
	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> samplerDescriptorSets;
	
	std::vector<VkBuffer> vpUniformBuffers;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;
	
	//std::vector<VkBuffer> modelDynamicUniformBuffers;
	//std::vector<VkDeviceMemory> modelDynamicUniformBufferMemory;

	// Model* modelTransferSpace;

	// Assets
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;
	std::vector<VkImageView> textureImageViews;
	
	// Pipeline
	VkPipeline graphicsPipeline{};
	VkPipelineLayout pipelineLayout{};
	VkRenderPass renderPass{};

	// Pools
	VkCommandPool graphicsCommandPool{};

	// Vulkan Utilities
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent{};

	VkFormat depthBufferImageFormat;

	VkDeviceSize minUniformBufferOffset;
	size_t modelUniformAlignment;

	// Synchronization
	std::vector<VkSemaphore> imageAvailable;
	std::vector<VkSemaphore> renderFinished;
	std::vector<VkFence> drawFences;

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

public:
	VulkanRenderer(GLFWwindow* pWindow);
	~VulkanRenderer();
	void Draw();
	void UpdateModel(unsigned modelId, glm::mat4 newModel);

private:
	// Vulkan Functions
	// - Create Functions
	void CreateInstance();
	void CreateLogicalDevice();
	void CreateSurface();
	void CreateSwapChain();
	void CreateRenderPass();
	void CreateDescriptorSetLayout();
	void CreatePushConstantRange();
	void CreateGraphicsPipeline();
	void CreateDepthBufferImage();
	void CreateFrameBuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateUniformBuffers();
	void CreateDescriptorPools();
	void CreateDescriptorSets();
	void CreateSynchronization();
	void CreateTextureSampler();

	void RecordCommands(uint32_t currentImage);

	void UpdateUniformBuffers(uint32_t imageIndex);
	
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

	void AllocateDynamicBufferTransferSpace();
	
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
	VkFormat ChooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags) const;

	// - - Create Functions
	VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory) const;
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
	VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);

	int CreateTextureImage(const std::string& fileName);
	int CreateTexture(const std::string& fileName);
	int CreateTextureDescriptor(VkImageView textureImage);
	
	// - - Loader Functions
	stbi_uc* LoadTextureFile(const std::string& fileName, int& width, int& height, VkDeviceSize* imageSize);
};
