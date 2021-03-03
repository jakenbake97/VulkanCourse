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

	const auto modelIndex = renderer.CreateMeshModel("Models/TexturedMonkey.fbx");
	// loop until closed
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		const float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 1.0f * deltaTime;
		if (angle > 360.0f)
		{
			angle -= 360.0f;
		}

		glm::mat4 testMat = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
		testMat = glm::rotate(testMat,-90.0f, glm::vec3(1.0f, 0.0f, 0.0f));
		testMat = glm::rotate(testMat, angle, glm::vec3(0, 1, 0));
		renderer.UpdateModel(modelIndex, testMat);
		
		renderer.Draw();
	}
}
