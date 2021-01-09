#pragma once

#include <fstream>

const int MAX_FRAME_DRAWS = 2;

constexpr void VK_ERROR(const int result, const char* message)
{
	if (result != VK_SUCCESS)
		throw std::runtime_error(message);
}

// Indices (locations) of queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;     // Location on the Graphics Queue Family
	int presentationFamily = -1; // Location of Presentation Queue Family

	// check if queue families are valid
	bool IsValid() const
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities{};    // surface properties (image size/extent)
	std::vector<VkSurfaceFormatKHR> formats;         // Surface image formats (Color and size)
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
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> fileBuffer(fileSize);

	// reset read position to start
	file.seekg(0);

	// read file data into buffer and go until we hit the end of the file.
	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}