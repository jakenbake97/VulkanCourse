#pragma once

#define GLFW_INCLUDE_VULKAN
#include <vector>
#include <GLFW/glfw3.h>

#include "Utilities.h"

struct Model
{
	glm::mat4 model;
};

class Mesh
{
	Model model;
	
	int vertexCount;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	int indexCount;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
public:
	Mesh();
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
	     VkCommandPool transferCommandPool, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);

	void SetModel(glm::mat4 newModel);
	glm::mat4 GetModelMat() const;
	Model* GetModelPtr();
	
	int GetVertexCount() const;
	int GetIndexCount() const;
	VkBuffer GetVertexBuffer() const;
	VkBuffer GetIndexBuffer() const;

	void DestroyMeshBuffers() const;

	~Mesh();

private:
	void CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices);
	void CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices);
};
