#include "Window.h"

#include <stdexcept>
#include "VulkanRenderer.h"

Window::Window(const std::string& wName, const int width, const int height)
{
	// Initialize GLFW
	if (glfwInit() != GLFW_TRUE)
		throw std::runtime_error("Failed to initialize GLFW");

	// Set GLFW to not use OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
	if (window == nullptr)
		throw std::runtime_error("Failed to create GLFW window instance");
}

Window::~Window()
{
	// destroy GLFW window and stop it
	glfwDestroyWindow(window);
	glfwTerminate();
}

GLFWwindow* Window::GetGLFWWindow() const
{
	return window;
}

void Window::LoopWindow(VulkanRenderer& renderer) const
{
	// loop until closed
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		renderer.Draw();
	}
}
