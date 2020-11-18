#pragma once

// Indices (locations) of queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;   // Location on the Graphics Queue Family

	// check if queue families are valid
	bool IsValid() const
	{
		return graphicsFamily >= 0;
	}
};

constexpr void VK_ERROR(const int result, const char* message)
{
	if (result != VK_SUCCESS)
		throw std::runtime_error(message);
}