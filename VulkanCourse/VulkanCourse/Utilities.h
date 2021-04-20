#pragma once

#include <fstream>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 20;

constexpr void VK_ERROR(const int result, const char* message)
{
	if (result != VK_SUCCESS)
		throw std::runtime_error(message);
}

struct Vertex
{
	glm::vec3 pos; // Vertex Position (x,y,z)
	glm::vec3 col; // Vertex Color (r,g,b)
	glm::vec2 tex; // texture coords (u,v)
};

// Indices (locations) of queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1; // Location on the Graphics Queue Family
	int presentationFamily = -1; // Location of Presentation Queue Family

	// check if queue families are valid
	bool IsValid() const
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities{}; // surface properties (image size/extent)
	std::vector<VkSurfaceFormatKHR> formats; // Surface image formats (Color and size)
	std::vector<VkPresentModeKHR> presentationModes; // how images should be presented to screen
};

struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> ReadFile(const std::string& fileName)
{
	// open stream from given file
	// std::ios::binary tells stream to read file binary
	// std::ios::ate tells stream to start reading from end of file
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		const std::string message = "Failed to open file: " + fileName;
		VK_ERROR(-1, message.c_str());
	}

	// get current read position and use to size file buffer
	const size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> fileBuffer(fileSize);

	// reset read position to start
	file.seekg(0);

	// read file data into buffer and go until we hit the end of the file.
	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, const uint32_t allowedTypes,
                                    VkMemoryPropertyFlags properties)
{
	// Get Properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if ((allowedTypes & (1 << i)) // Index of memory type must match corresponding bit in allowed types
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			// Desired property bit flags are part of memory type's property flags
		{
			// This memory type is valid, so return its index
			return i;
		}
	}
	VK_ERROR(-1, "Couldn't find and appropriate memory type index");
	return UINT32_MAX;
}

static void CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize,
                         VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags bufferProperties, VkBuffer* buffer,
                         VkDeviceMemory* bufferMemory)
{
	// Information to create a buffer (doesn't include assigning memory)
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsage; // multiple types of buffer possible
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_ERROR(vkCreateBuffer(device, &bufferInfo, nullptr, buffer), "Failed to create Vertex buffer");

	// Get Buffer Memory Requirements
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	// Allocate Memory To Buffer
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits,
	                                                         bufferProperties);

	// Allocate memory to VkDeviceMemory
	VK_ERROR(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory),
	         "Failed to allocate vertex buffer memory");

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static VkCommandBuffer BeginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	// Command buffer to hold transfer commands
	VkCommandBuffer commandBuffer;

	// Command buffer details
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = commandPool;
	allocateInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	// We're only using the buffer once to transfer data, so it doesn't need to be reusable

	// Begin recording transfer commands
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void EndAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                                      VkCommandBuffer commandBuffer)
{
	// end recording commands
	vkEndCommandBuffer(commandBuffer);

	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// submit transfer command to transfer queue and wait until it finished
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	// Free temporary command buffer back to pool
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, VkBuffer srcBuffer,
                       VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	const VkCommandBuffer transferCommandBuffer = BeginCommandBuffer(device, transferCommandPool);
	{
		// Region of data to copy from and to
		VkBufferCopy bufferCopyRegion = {};
		bufferCopyRegion.srcOffset = 0;
		bufferCopyRegion.dstOffset = 0;
		bufferCopyRegion.size = bufferSize;

		// Command to copy src buffer to dst buffer
		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);
	}
	EndAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void CopyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                            VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
{
	const VkCommandBuffer transferCommandBuffer = BeginCommandBuffer(device, transferCommandPool);
	{
		VkBufferImageCopy imageRegion = {};
		imageRegion.bufferOffset = 0;
		imageRegion.bufferRowLength = 0;
		imageRegion.bufferImageHeight = 0;
		imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageRegion.imageSubresource.mipLevel = 0;
		imageRegion.imageSubresource.baseArrayLayer = 0;
		imageRegion.imageSubresource.layerCount = 1;
		imageRegion.imageOffset = {0, 0, 0};
		imageRegion.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		                       &imageRegion);
	}
	EndAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void TransitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image,
                                  VkImageLayout oldLayout, VkImageLayout newLayout)
{
	const VkCommandBuffer commandBuffer = BeginCommandBuffer(device, commandPool);
	{
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = oldLayout;
		imageMemoryBarrier.newLayout = newLayout;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.layerCount = 1;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;

		VkPipelineStageFlags srcStage{};
		VkPipelineStageFlags dstStage{};

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			// memory access stage transition must happen after
			imageMemoryBarrier.srcAccessMask = 0; // in this case the first stage of the pipeline

			// access must happen before
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			// the transfer write from the copy image to buffer method

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(commandBuffer,
			srcStage, dstStage, 
			0, 0, nullptr, 0, nullptr, 
			1, &imageMemoryBarrier
		);
	}
	EndAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}
