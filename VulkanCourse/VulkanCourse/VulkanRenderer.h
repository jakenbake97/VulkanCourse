#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include "stb_image.h"
#include "Utilities.h"
#include "MeshModel.h"

class VulkanRenderer
{
private:
	GLFWwindow* window = nullptr;

	int currentFrame = 0;

	// Scene Objects
	std::vector<MeshModel> modelList;
	
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

	std::vector<VkImage> colorBufferImages;
	std::vector<VkDeviceMemory> colorBufferImageMemory;
	std::vector<VkImageView> colorBufferImageViews;

	std::vector<VkImage> depthBufferImages;
	std::vector<VkDeviceMemory> depthBufferImageMemory;
	std::vector<VkImageView> depthBufferImageViews;

	VkSampler textureSampler;
	
	std::vector<VkCommandBuffer> commandBuffers;

	// Descriptors
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkDescriptorSetLayout inputSetLayout;
	VkPushConstantRange pushConstantRange;
	
	VkDescriptorPool descriptorPool;
	VkDescriptorPool samplerDescriptorPool;
	VkDescriptorPool inputDescriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> samplerDescriptorSets;
	std::vector<VkDescriptorSet> inputDescriptorSets;
	
	std::vector<VkBuffer> vpUniformBuffers;
	std::vector<VkDeviceMemory> vpUniformBufferMemory;
	
	//std::vector<VkBuffer> modelDynamicUniformBuffers;
	//std::vector<VkDeviceMemory> modelDynamicUniformBufferMemory;

	// Assets	
	std::vector<VkImage> textureImages;
	std::vector<VkDeviceMemory> textureImageMemory;
	std::vector<VkImageView> textureImageViews;
	
	// Pipeline
	VkPipeline graphicsPipeline{};
	VkPipelineLayout pipelineLayout{};

	VkPipeline secondPipeline{};
	VkPipelineLayout secondPipelineLayout{};
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
	VulkanRenderer(VulkanRenderer& other) = delete;
	VulkanRenderer& operator= (const VulkanRenderer& other) = delete;
	VulkanRenderer(VulkanRenderer&& other) = delete;
	VulkanRenderer& operator= (VulkanRenderer&& other) = delete;
	
	void Draw();
	void UpdateModel(uint32_t modelId, glm::mat4 newModel);
	uint32_t CreateMeshModel(const std::string& modelFile);

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
	void CreateColorBufferImage();
	void CreateDepthBufferImage();
	void CreateFrameBuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateUniformBuffers();
	void CreateDescriptorPools();
	void CreateDescriptorSets();
	void CreateInputDescriptorSets();
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
	
	// - Getter Functions
	void GetPhysicalDevice();

	// - Support Functions
	// - - Checker Functions
	static bool CheckInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
	bool CheckDeviceSuitable(VkPhysicalDevice device);
	bool CheckValidationLayerSupport() const;

	// - - Getter Functions
	QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device) const;
	std::vector<const char*> GetRequiredGLFWExtensions() const;
	SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device) const;

	// - - Chooser Functions
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const;
	static VkSurfaceFormatKHR ChooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	static VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes);
	VkFormat ChooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags) const;

	// - - Create Functions
	VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory) const;
	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
	VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode) const;

	int CreateTextureImage(const std::string& fileName);
	int CreateTexture(const std::string& fileName);
	int CreateTextureDescriptor(VkImageView textureImage);
	
	// - - Loader Functions
	stbi_uc* LoadTextureFile(const std::string& fileName, int& width, int& height, VkDeviceSize* imageSize);
};
