#pragma once
#include <assimp/scene.h>

#include "Mesh.h"

class MeshModel
{
	std::vector<Mesh> meshList;
	glm::mat4 model;

public:
	MeshModel();
	MeshModel(std::vector<Mesh> newMeshList);

	~MeshModel();

	static std::vector<std::string> LoadMaterials(const aiScene* scene);
	static std::vector<Mesh> LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
	                                  VkCommandPool commandPool, aiNode* node, const aiScene* scene,
	                                  std::vector<int> matToTex);
	static Mesh LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
	                                  VkCommandPool commandPool, aiMesh* mesh, std::vector<int> matToTex);

	size_t GetMeshCount() const;
	Mesh* GetMesh(size_t index);

	glm::mat4 GetModel() const;
	glm::mat4* GetModelPtr();
	void SetModel(glm::mat4 newModel);

	void DestroyMeshModel();
};
