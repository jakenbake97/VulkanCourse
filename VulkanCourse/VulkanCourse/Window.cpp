#include "Window.h"

Window::Window(const std::string wName, const int width, const int height)
{
	// Initialize GLFW
	glfwInit();

	// Set GLFW to not use OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
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

void Window::LoopWindow() const
{
	// loop until closed
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}
}
