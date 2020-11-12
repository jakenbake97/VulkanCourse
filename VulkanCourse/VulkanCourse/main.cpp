#include "Window.h"
#include "VulkanRenderer.h"
#include <stdexcept>
#include <vector>
#include <iostream>


int main()
{
	// Create Window
	Window window("Half-Way Engine", 1920, 1080);

	// Create VulkanRenderer Instance
	VulkanRenderer renderer(window.GetGLFWWindow());
	
	if (renderer.Initialize() != EXIT_SUCCESS) return EXIT_FAILURE;

	window.LoopWindow();
	
	return EXIT_SUCCESS;
}
