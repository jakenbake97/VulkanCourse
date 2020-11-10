#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>


#include <iostream>

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800,600, "Test Window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	printf("Extension count: %i\n", extensionCount);

	const glm::mat4 testMatrix(1.0f);
	const glm::vec4 testVector(1.0f);

	const auto testResult = testMatrix * testVector;

	printf("Matrix * Vector: %f, %f, %f, %f\n", testResult.x, testResult.y, testResult.z, testResult.w);
	
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	glfwTerminate();

	return 0;
}