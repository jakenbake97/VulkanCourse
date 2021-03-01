#include "Window.h"

#include <stdexcept>
#include "VulkanRenderer.h"

Window::Window(const std::string& wName, const int width, const int height)
{
	// Initialize GLFW
	const auto result = glfwInit();
	if (result != GLFW_TRUE)
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
	float angle = 0.0f;
	auto deltaTime = 0.0f;
	float lastTime = 0.0f;

	// loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		const float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;
		if (angle > 360.0f)
		{
			angle -= 360.0f;
		}

		glm::mat4 firstModel(1.0f);
		glm::mat4 secondModel(1.0f);

		firstModel = glm::translate(firstModel, glm::vec3(-2.0f, 0.0f, -2.5f));
		firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));

		secondModel = glm::translate(secondModel, glm::vec3(2.0f, 0.0f, -2.5f));
		secondModel = glm::rotate(secondModel, glm::radians(-angle * 100), glm::vec3(0.0f, 0.0f, 1.0f));

		renderer.UpdateModel(0, firstModel);
		renderer.UpdateModel(1, secondModel);
		renderer.Draw();
	}
}
