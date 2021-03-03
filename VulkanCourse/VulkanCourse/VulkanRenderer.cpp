#include "VulkanRenderer.h"
#include <algorithm>
#include <string>

VulkanRenderer::VulkanRenderer(GLFWwindow* pWindow)
	:
	window(pWindow)
{
	CreateInstance();
	CreateSurface();
	GetPhysicalDevice();
	CreateLogicalDevice();

	CreateSwapChain();
	CreateDepthBufferImage();
	CreateRenderPass();
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	CreateFrameBuffers();
	CreateCommandPool();

	CreateCommandBuffers();
	CreateTextureSampler();
	// AllocateDynamicBufferTransferSpace();
	CreateUniformBuffers();
	CreateDescriptorPools();
	CreateDescriptorSets();
	CreateSynchronization();

	uboViewProjection.projection = glm::perspective(glm::radians(45.0f),
	                                                (float)swapChainExtent.width / (float)swapChainExtent.height,
	                                                0.1f, 100.0f);
	uboViewProjection.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -4.0f),
	                                     glm::vec3(0.0f, 1.0f, 0.0f));

	uboViewProjection.projection[1][1] *= -1;

	// Default fallback texture
	CreateTexture("Default.png");
}

VulkanRenderer::~VulkanRenderer()
{
	// wait until there are no actions on the device before destroying
	VK_ERROR(vkDeviceWaitIdle(mainDevice.logicalDevice), "Failed to wait until the device was idle");

	for (auto& meshModel : modelList)
	{
		meshModel.DestroyMeshModel();
	}
	
	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

	for (size_t i = 0; i < textureImages.size(); ++i)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);

		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);

	for (auto framebuffer : swapChainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);

		/*vkDestroyBuffer(mainDevice.logicalDevice, modelDynamicUniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, modelDynamicUniformBufferMemory[i], nullptr);*/
	}

	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);

	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

	vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView, nullptr);
	vkDestroyImage(mainDevice.logicalDevice, depthBufferImage, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory, nullptr);

	for (const auto& image : swapChainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}

	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);

	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroyInstance(instance, nullptr);
}

void VulkanRenderer::Draw()
{
	// 1. Get next available image

	// wait for given fence to signal (open) from last draw before continuing
	VK_ERROR(vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_FALSE,
	                         std::numeric_limits<uint64_t>::max()), "Failed to wait for fences");
	// manually reset (close) the fence
	VK_ERROR(vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]), "Failed to reset fence");

	// get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	VK_ERROR(vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(),
	                               imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex),
	         "Failed to acquire next image"
	);

	RecordCommands(imageIndex);

	UpdateUniformBuffers(imageIndex);

	// 2. Submit Command buffer to render
	// queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT // Stages to check semaphores
	};
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex]; // command buffer to submit
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];

	VK_ERROR(vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]),
	         "Failed to submit command buffer to graphics queue");

	// 3. Present rendered image to screen
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	VK_ERROR(vkQueuePresentKHR(graphicsQueue, &presentInfo), "Failed to present Image");

	// Get next frame by mod with max frame draws to stay in range
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::UpdateModel(uint32_t modelId, glm::mat4 newModel)
{
	if (modelId >= modelList.size()) return;

	modelList[modelId].SetModel(newModel);
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
	deviceFeatures.samplerAnisotropy = VK_TRUE;

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

	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount <
		imageCount)
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
		uint32_t queueFamilyIndices[] = {
			static_cast<uint32_t>(indices.graphicsFamily), static_cast<uint32_t>(indices.presentationFamily)
		};
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
	VK_ERROR(vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain),
	         "Failed to create swapchain");

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

	for (VkImage image : images)
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

void VulkanRenderer::CreateRenderPass()
{
	// -- Attachments --
	// Color attachment of the render pass
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat; // format to use for the attachment
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // number of samples to write for multiSampling
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // what to do with the attachment before rendering
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// what to do with the data in the attachment after rendering
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // what to do with stencil before rendering
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // what to do with the stencil after rendering

	// Framebuffer data will be stored as an image, but images can be given different data layouts to give optimal use for certain operations
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // the expected layout before the render pass starts
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image data layout after the render pass

	// Depth attachment of render pass
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthBufferImageFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	// -- References --
	// Attachment reference uses an attachment index that refers to the index in the list passed to RenderPassCreateInfo
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment reference
	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Information about a particular subpass the renderpass is using
	VkSubpassDescription subPass = {};
	subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // pipeline to be bound to
	subPass.colorAttachmentCount = 1;
	subPass.pColorAttachments = &colorAttachmentRef;
	subPass.pDepthStencilAttachment = &depthAttachmentReference;

	// Need to determine when layout transitions occur using subPass dependencies
	std::array<VkSubpassDependency, 2> subpassDependencies{};

	// conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	// SubPass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Pipeline stage
	subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Stage access mask (memory access)

	// but before...
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	// conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// but before...
	subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies[1].dependencyFlags = 0;


	std::array<VkAttachmentDescription, 2> renderPassAttachments = {colorAttachment, depthAttachment};

	// Create info for Render Pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subPass;
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VK_ERROR(vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass),
	         "Failed to create render pass");
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	// UboViewProjection binding info
	VkDescriptorSetLayoutBinding vpLayoutBinding{};
	vpLayoutBinding.binding = 0;
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpLayoutBinding.descriptorCount = 1;
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// model binding info
	/*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;*/

	std::vector<VkDescriptorSetLayoutBinding> bindings = {vpLayoutBinding};

	// Create Descriptor Set Layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutCreateInfo.pBindings = bindings.data();

	VK_ERROR(vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout),
	         "Failed to create descriptor set layout");

	// CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo{};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	VK_ERROR(
		vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout),
		"Failed to create sampler descriptor set layout");
}

void VulkanRenderer::CreatePushConstantRange()
{
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(Model);
}

void VulkanRenderer::CreateGraphicsPipeline()
{
	// read in SPIR-V shader code
	const auto vertexShader = ReadFile("Shaders/VertexShader.vert.spv");
	const auto fragmentShader = ReadFile("Shaders/FragmentShader.frag.spv");

	// create Shader modules to link to Graphics pipeline
	const VkShaderModule vertexShaderModule = CreateShaderModule(vertexShader);
	const VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShader);

	// -- Shader stage creation information -- 
	// vertex stage creation
	VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
	vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderCreateInfo.module = vertexShaderModule;
	vertexShaderCreateInfo.pName = "main"; // the name of the function to run in the shader

	// fragment stage creation
	VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
	fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderCreateInfo.module = fragmentShaderModule;
	fragmentShaderCreateInfo.pName = "main"; // the name of the function to run in the shader

	// shader stage creation info array (required by pipeline)
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

	// How the data or a single vertex (including info such as position, color, tex coords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0; // can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // How to move between data after each vertex

	// How the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	// Position Attribute
	attributeDescriptions[0].binding = 0; // Which binding the data is at (should be same as above)
	attributeDescriptions[0].location = 0; // The location for the attribute in the shader
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	// Format the data will take (also helps define the size of data)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);
	// Where this attribute is defined in the data for a single vertex

	// Color Attribute
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// Texture Attribute
	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	// -- Vertex Input --
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	// list of vertex binding descriptions (data spacing / strides)
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	// list of vertex attribute descriptions (data format and where to bind to/from)

	// -- Input Assembly --
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // primitive type to assemble verts as
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE; // allow overriding of topology to start new primitives

	// -- Viewport & Scissor
	// create a viewport info struct
	VkViewport viewport = {};
	viewport.x = 0.0f; // x start coordinate
	viewport.y = 0.0f; // y start coordinate
	viewport.width = (float)swapChainExtent.width; // width of viewport
	viewport.height = (float)swapChainExtent.height; // height of viewport
	viewport.minDepth = 0.0f; // min frameBuffer depth
	viewport.maxDepth = 1.0f; // max frameBuffer depth

	// Create a scissor info struct
	VkRect2D scissor = {};
	scissor.offset = {0, 0}; // offset to the start of the region
	scissor.extent = swapChainExtent; // extent of the region starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	//// -- Dynamic States --
	//// Dynamic states to enable
	//std::vector<VkDynamicState> enabledDynamicStates;
	//enabledDynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT); // dynamic viewport which can be resized in command buffer
	//enabledDynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR); // dynamic scissor can be resized in command buffer

	//// Dynamic State creation info
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(enabledDynamicStates.size());

	// -- Rasterization --
	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = {};
	rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCreateInfo.depthClampEnable = VK_FALSE;
	// determines if fragments beyond far plane are clipped (default) or clamped to far plane
	rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	// discards data and skips rasterization (never creates fragments) used for pipeline without frameBuffer output
	rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // how to handle filling points between vertices
	rasterizationCreateInfo.lineWidth = 1.0f; // How thick lines should be when drawn
	rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT; // Which face to cull
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // The winding to determine which side is front
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
	// Whether to add a depth bias to fragments (good for limiting shadow acne)

	// -- MultiSampling --
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE; // enable multisample shading or not
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // number of samples to use per fragment

	// -- Blending --
	// blending decides how to blend a new color being written to a fragment, with the old value

	// blend attachment state (how blending is handled)
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT; // color channels to apply blending to
	colorBlendAttachment.blendEnable = VK_TRUE; // enable blending

	// blending uses the following equation (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE; // alternative to calculations is to use logical operations
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;

	CreatePushConstantRange();

	// -- Pipeline Layout --
	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {descriptorSetLayout, samplerSetLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create Pipeline Layout
	VK_ERROR(vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout),
	         "Failed to create pipeline layout");

	// -- Depth Stencil Testing --
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	// -- Create Graphics Pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	// Pipeline Derivatives: Can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Existing pipeline to derive from...
	pipelineCreateInfo.basePipelineIndex = -1;
	// or index of pipeline being created to derive from (in case creating multiple)

	// Create graphics pipeline
	VK_ERROR(vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr,
	                                   &graphicsPipeline),
	         "Failed to create graphics pipeline"
	);

	// Destroy shader modules no longer needed after pipeline created
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);
}

void VulkanRenderer::CreateDepthBufferImage()
{
	depthBufferImageFormat = ChooseSupportedFormat
	(
		{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	depthBufferImage = CreateImage(swapChainExtent.width, swapChainExtent.height, depthBufferImageFormat,
	                               VK_IMAGE_TILING_OPTIMAL,
	                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	                               &depthBufferImageMemory);

	depthBufferImageView = CreateImageView(depthBufferImage, depthBufferImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::CreateFrameBuffers()
{
	// resize framebuffer count to equal swap chain image count
	swapChainFramebuffers.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainFramebuffers.size(); ++i)
	{
		std::array<VkImageView, 2> attachments = {swapChainImages[i].imageView, depthBufferImageView};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data(); // list of attachments (1:1 with render pass)
		framebufferCreateInfo.width = swapChainExtent.width;
		framebufferCreateInfo.height = swapChainExtent.height;
		framebufferCreateInfo.layers = 1;

		VK_ERROR(vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr,
		                             &swapChainFramebuffers[i]), "Failed to create framebuffer");
	}
}

void VulkanRenderer::CreateCommandPool()
{
	// get indices of queue families from device
	QueueFamilyIndices queueFamilyIndices = GetQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	// queue family type that buffers from this command pool will use

	// create a graphics queue family command pool
	VK_ERROR(vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool),
	         "Failed to create command pool");
}

void VulkanRenderer::CreateCommandBuffers()
{
	// resize command buffer count to be 1:1 with frame buffers
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo cBAllocateInfo = {};
	cBAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cBAllocateInfo.commandPool = graphicsCommandPool;
	cBAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cBAllocateInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	// allocate command buffers and place handles in array of buffers
	VK_ERROR(vkAllocateCommandBuffers(mainDevice.logicalDevice, &cBAllocateInfo, commandBuffers.data()),
	         "Failed to allocate command buffers");
}

void VulkanRenderer::CreateUniformBuffers()
{
	const VkDeviceSize vpBufferSize = sizeof(UboViewProjection);
	//const VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

	// One uniform buffer for each image / command buffer
	vpUniformBuffers.resize(swapChainImages.size());
	vpUniformBufferMemory.resize(swapChainImages.size());

	//modelDynamicUniformBuffers.resize(swapChainImages.size());
	//modelDynamicUniformBufferMemory.resize(swapChainImages.size());

	// Create uniform buffers
	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize,
		             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		             &vpUniformBuffers[i], &vpUniformBufferMemory[i]);

		/*CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize,
		             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		             &modelDynamicUniformBuffers[i], &modelDynamicUniformBufferMemory[i]);*/
	}
}

void VulkanRenderer::CreateDescriptorPools()
{
	// CREATE UNIFORM DESCRIPTOR POOL

	// Type of descriptors how many of them (not descriptor sets)
	VkDescriptorPoolSize vpPoolSize{};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffers.size());

	/*VkDescriptorPoolSize modelPoolSize{};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynamicUniformBuffers.size());*/

	std::vector<VkDescriptorPoolSize> poolSizes = {vpPoolSize};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();

	// Create Descriptor pool
	VK_ERROR(vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool),
	         "Failed to create descriptor pool");

	// CREATE SAMPLER DESCRIPTOR POOL
	VkDescriptorPoolSize samplerPoolSize{};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	VK_ERROR(vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool),
	         "Failed to create sampler descriptor pool");
}

void VulkanRenderer::CreateDescriptorSets()
{
	descriptorSets.resize(swapChainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(vpUniformBuffers.size(), descriptorSetLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	setAllocInfo.pSetLayouts = setLayouts.data();

	// allocate descriptor sets
	VK_ERROR(vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data()),
	         "Failed to allocate descriptor sets");

	for (size_t i = 0; i < swapChainImages.size(); ++i)
	{
		// View Projection Descriptor
		VkDescriptorBufferInfo vpBufferInfo = {};
		vpBufferInfo.buffer = vpUniformBuffers[i];
		vpBufferInfo.offset = 0;
		vpBufferInfo.range = sizeof(UboViewProjection);

		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.descriptorCount = 1;
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vpSetWrite.dstSet = descriptorSets[i];
		vpSetWrite.dstBinding = 0;
		vpSetWrite.dstArrayElement = 0;
		vpSetWrite.pBufferInfo = &vpBufferInfo;

		// Model Descriptor
		/*VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelDynamicUniformBuffers[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = modelUniformAlignment;

		VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.pBufferInfo = &modelBufferInfo;*/

		std::vector<VkWriteDescriptorSet> descriptorSetWrites = {vpSetWrite};

		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(descriptorSetWrites.size()),
		                       descriptorSetWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
	{
		VK_ERROR(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]),
		         "Failed to create 'image available' semaphore");
		VK_ERROR(vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]),
		         "Failed to create 'render finished' semaphore");
		VK_ERROR(vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]),
		         "Failed to create synchronization fence");
	}
}

void VulkanRenderer::CreateTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	// sort of a double negative unnormalized false == normalized coordinates true
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0;
	samplerCreateInfo.maxLod = 0;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.maxAnisotropy = 16;

	VK_ERROR(vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler),
	         "Failed to create a texture sampler");
}

void VulkanRenderer::RecordCommands(const uint32_t currentImage)
{
	// Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = {0.6f, 0.65f, 0.4f, 1.0f};
	clearValues[1].depthStencil.depth = 1.0f;


	renderPassBeginInfo.pClearValues = clearValues.data();
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());


	renderPassBeginInfo.framebuffer = swapChainFramebuffers[currentImage];

	// begin command buffer
	VK_ERROR(vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo),
	         "Failed to start recording a command buffer");
	{
		// begin render pass
		vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// bind pipeline to be used in render pass
			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			for (auto& thisModel : modelList)
			{
				vkCmdPushConstants(commandBuffers[currentImage], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
				                   sizeof(Model), thisModel.GetModelPtr());

				for (size_t j = 0; j < thisModel.GetMeshCount(); ++j)
				{
					auto* thisMesh = thisModel.GetMesh(j);
					
					VkBuffer vertexBuffers[] = {thisMesh->GetVertexBuffer()}; // buffers to bind
					VkDeviceSize offsets[] = {0}; // offsets into buffers being bound
					vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);
					// command to bind vertex buffer before
					//drawing with them

					// Bind mesh index buffer, with 0 offset and using uint32 type
					vkCmdBindIndexBuffer(commandBuffers[currentImage], thisMesh->GetIndexBuffer(), 0,
                                         VK_INDEX_TYPE_UINT32);

					// Dynamic offset Amount
					//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * i;

				

					std::array<VkDescriptorSet, 2> descriptorSetGroup = {
						descriptorSets[currentImage], samplerDescriptorSets[thisMesh->GetTexId()]
                    };

					// Bind Descriptor Sets
					vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                            0, static_cast<uint32_t>(descriptorSetGroup.size()),
                                            descriptorSetGroup.data(), 0, nullptr);

					// execute pipeline
					vkCmdDrawIndexed(commandBuffers[currentImage], thisMesh->GetIndexCount(), 1, 0, 0, 0);
				}
			}
		}
		vkCmdEndRenderPass(commandBuffers[currentImage]); // end render pass
	}
	// end command buffer
	VK_ERROR(vkEndCommandBuffer(commandBuffers[currentImage]), "Failed to stop recording a command buffer");
}

void VulkanRenderer::UpdateUniformBuffers(const uint32_t imageIndex)
{
	// Copy VP Data	
	void* data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);

	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));

	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);
}

void VulkanRenderer::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
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
		VK_ERROR(VK_ERROR_DEVICE_LOST, "Failed find suitable physical device");
	}

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
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
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	const QueueFamilyIndices indices = GetQueueFamilies(device);

	const bool extensionsSupported = CheckDeviceExtensionSupport(device);

	bool swapChainValid = false;

	if (extensionsSupported)
	{
		const SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}

	return indices.IsValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
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
		glfwGetFramebufferSize(window, &width, &height);
		// This gets the resolution in pixels (instead of screen coordinates 

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

VkFormat VulkanRenderer::ChooseSupportedFormat(const std::vector<VkFormat>& formats, const VkImageTiling tiling,
                                               const VkFormatFeatureFlags featureFlags) const
{
	// loop through options and find a compatible one
	for (auto& format : formats)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}

	VK_ERROR(VK_ERROR_FORMAT_NOT_SUPPORTED, "Failed to find a matching format");

	return VK_FORMAT_UNDEFINED;
}

VkImage VulkanRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propFlags,
                                    VkDeviceMemory* imageMemory) const
{
	// -- Create Image --
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling; // How image data should be arranged for reading
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = usageFlags;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImage image;
	VK_ERROR(vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image), "Failed to create an image");

	// -- Create memory for image --
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(mainDevice.physicalDevice, memRequirements.memoryTypeBits,
	                                                         propFlags);

	VK_ERROR(vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory),
	         "Failed to allocate memory for image");

	// connect memory to image
	VK_ERROR(vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0),
	         "Failed to bind image to allocated memory");

	return image;
}

VkImageView VulkanRenderer::CreateImageView(const VkImage image, const VkFormat format,
                                            VkImageAspectFlags aspectFlags) const
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
	VK_ERROR(vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView),
	         "Failed to create image view");

	return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& shaderCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = shaderCode.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

	VkShaderModule shaderModule;
	VK_ERROR(vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule),
	         "Failed to create shader module");

	return shaderModule;
}

int VulkanRenderer::CreateTextureImage(const std::string& fileName)
{
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc* imageData = LoadTextureFile(fileName, width, height, &imageSize);

	// Create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	CreateBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer,
	             &imageStagingBufferMemory);
	void* data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	// free original image data
	stbi_image_free(imageData);

	VkDeviceMemory texImageMemory;
	VkImage texImage = CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
	                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

	// Transition image to be DST for copy operation
	TransitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
	                      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy data to image
	CopyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width,
	                height);

	TransitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, texImage,
	                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	textureImages.push_back(texImage);
	textureImageMemory.push_back(texImageMemory);

	// Destroy staging buffers
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	return textureImages.size() - 1; // index of new textured image
}

int VulkanRenderer::CreateTexture(const std::string& fileName)
{
	const int textureImageLocation = CreateTextureImage(fileName);

	const VkImageView imageView = CreateImageView(textureImages[textureImageLocation], VK_FORMAT_R8G8B8A8_UNORM,
	                                              VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	const int descriptorLocation = CreateTextureDescriptor(imageView);

	return descriptorLocation;
}

int VulkanRenderer::CreateTextureDescriptor(VkImageView textureImage)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = samplerDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &samplerSetLayout;

	// allocate descriptor sets
	VK_ERROR(vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet),
	         "Failed to allocate texture descriptor sets");

	// Texture image info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureImage;
	imageInfo.sampler = textureSampler;

	// Descriptor write info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);
	return samplerDescriptorSets.size() - 1;
}

uint32_t VulkanRenderer::CreateMeshModel(const std::string& modelFile)
{
	// Import model "scene"
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

	if (!scene)
	{
		throw std::runtime_error("Failed to load model! (" + modelFile + ")");
	}

	// Vector of all materials with 1:1 Id placement
	std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

	// Conversion from the materials list IDs to our Descriptor Array IDs
	std::vector<int> matToTex(textureNames.size());

	// Loop over textureNames and Create textures for them
	for (size_t i = 0; i < textureNames.size(); ++i)
	{
		if (textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			matToTex[i] = CreateTexture(textureNames[i]);
		}
	}

	// Load in all of the meshes
	const std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, scene->mRootNode, scene, matToTex);

	modelList.emplace_back(modelMeshes);
}

VkBool32 VulkanRenderer::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                       void* pUserData)
{
	printf("\nValidation layer: %s \n", pCallbackData->pMessage);
	return VK_FALSE;
}

void VulkanRenderer::AllocateDynamicBufferTransferSpace()
{
	//// calculate alignment of model data
	//modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset - 1);

	//// allocate space in memory to hold dynamic buffer that is aligned to our required alignment and holds the number maximum allowed number of objects
	//modelTransferSpace = (Model*)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
}

stbi_uc* VulkanRenderer::LoadTextureFile(const std::string& fileName, int& width, int& height, VkDeviceSize* imageSize)
{
	int channels;

	const std::string fileLocation = "Textures/" + fileName;

	stbi_uc* image = stbi_load(fileLocation.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to load Texture file! (" + fileName + ")");
	}

	*imageSize = static_cast<VkDeviceSize>(width) * (height) * 4;

	return image;
}
