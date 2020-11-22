#pragma once

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