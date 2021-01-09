#include "Window.h"
#include "VulkanRenderer.h"
#include <stdexcept>
#include <vector>
#include <iostream>


int main()
{
	try
	{
		// Create Window
		const Window window("Half-Way Engine", 1920, 1080);

		// Create VulkanRenderer Instance
		VulkanRenderer renderer(window.GetGLFWWindow());

		window.LoopWindow(renderer);
	}
	catch (std::runtime_error& e)
	{
		printf("\nERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
