#pragma once

// Indices (locations) of queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;   // Location on the Graphics Queue Family

	// check if queue families are valid
	bool IsValid()
	{
		return graphicsFamily >= 0;
	}
};
