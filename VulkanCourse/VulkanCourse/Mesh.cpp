#include "Mesh.h"

Mesh::Mesh()
	: uboModel({glm::mat4(1.0f)}), vertexCount(0), vertexBuffer(0), vertexBufferMemory(0), indexCount(0), physicalDevice(nullptr), device(nullptr)
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
           VkCommandPool transferCommandPool, std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
	: uboModel({glm::mat4(1.0f)}), vertexCount(vertices->size()), indexCount(indices->size()), physicalDevice(newPhysicalDevice), device(newDevice)
{
	CreateVertexBuffer(transferQueue, transferCommandPool, vertices);
	CreateIndexBuffer(transferQueue, transferCommandPool, indices);
}

void Mesh::SetModel(const glm::mat4 newModel)
{
	uboModel.model = newModel;
}

glm::mat4 Mesh::GetModel() const
{
	return uboModel.model;
}

int Mesh::GetVertexCount() const
{
	return vertexCount;
}

int Mesh::GetIndexCount() const
{
	return indexCount;
}

VkBuffer Mesh::GetVertexBuffer() const
{
	return vertexBuffer;
}

VkBuffer Mesh::GetIndexBuffer() const
{
	return indexBuffer;
}

void Mesh::DestroyMeshBuffers() const
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);

	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}

Mesh::~Mesh() = default;

void Mesh::CreateVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex>* vertices)
{
	const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(sizeof(Vertex)) * vertices->size();

	// Temporary buffer to "stage" vertex data before transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// Create buffer and allocate memory to it
	CreateBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
	             &stagingBufferMemory);

	// Map memory to vertex buffer
	// 1. Create pointer to a point in normal memory
	void* data; 
	// 2. map the vertex buffer memory to that point	
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	// 3. Copy memory from vertices vector to the point										
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));
	// 4. unmap the vertex buffer memory
	vkUnmapMemory(device, stagingBufferMemory);

	// Create buffer with transfer destination bit, to mark as the recipient of the transfer data
	CreateBuffer(physicalDevice, device, bufferSize,
	             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);

	// Copy staging buffer to the vertex buffer on the GPU
	CopyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	// Clean up staging buffer parts
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::CreateIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
	const VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	// Temporary buffer to "stage" index data before transferring to GPU
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// Create buffer and allocate memory to it
	CreateBuffer(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer,
	             &stagingBufferMemory);

	// Map memory to Index buffer
	// 1. Create pointer to a point in normal memory
	void* data; 
	// 2. map the vertex buffer memory to that point	
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	// 3. Copy memory from vertices vector to the point										
	memcpy(data, indices->data(), static_cast<size_t>(bufferSize));
	// 4. unmap the vertex buffer memory
	vkUnmapMemory(device, stagingBufferMemory);

	
	// Create buffer for index data on GPU
	CreateBuffer(physicalDevice, device, bufferSize,
	             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, &indexBufferMemory);

	// Copy from staging buffer to GPU access buffer
	CopyBuffer(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	// Destroy and release staging buffer resources
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}
