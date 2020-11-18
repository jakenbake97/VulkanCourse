#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

class Window
{
public:
	Window(const std::string& wName = "Test Window", const int width = 800, const int height = 600);
	~Window();

	GLFWwindow* GetGLFWWindow() const;
	void LoopWindow() const;
private:
	GLFWwindow* window = nullptr;
};