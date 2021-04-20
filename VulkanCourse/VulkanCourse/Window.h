#pragma once
#include <string>

class VulkanRenderer;
struct GLFWwindow;

class Window
{
public:
	Window(const std::string& wName = "Half-Way Engine Window", const int width = 800, const int height = 600);
	~Window();
	
	Window(Window& window) = default;
	Window& operator=(const Window& window) = default;
	Window(Window&& window) = default;
	Window& operator=(Window&& window) = default;

	GLFWwindow* GetGLFWWindow() const;
	void LoopWindow(VulkanRenderer& renderer) const;

private:
	GLFWwindow* window = nullptr;
};
