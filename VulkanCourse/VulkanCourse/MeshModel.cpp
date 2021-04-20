#include "MeshModel.h"

#include <utility>

std::vector<std::string> MeshModel::LoadMaterials(const aiScene* scene)
{
	std::vector<std::string> textureList(scene->mNumMaterials);

	for (size_t i = 0; i < scene->mNumMaterials; ++i)
	{
		// Get the material at i
		aiMaterial* material = scene->mMaterials[i];

		// Initialize the texture to empty string (will be replaced if texture exists)
		textureList[i] = "";

		if (material->GetTextureCount(aiTextureType_DIFFUSE))
		{
			// Get the path of the texture file
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
			{
				// cut off any directory information already present
				const int idx = std::string(path.data).rfind('\\');
				const std::string fileName = std::string(path.data).substr(idx + 1);

				textureList[i] = fileName;
			}
		}
	}

	return textureList;
}

std::vector<Mesh> MeshModel::LoadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
                                      VkCommandPool commandPool, aiNode* node, const aiScene* scene, std::vector<int>& matToTex)
{
	std::vector<Mesh> meshList;

	for (size_t i = 0; i < node->mNumMeshes; ++i)
	{
		meshList.push_back(LoadMesh(newPhysicalDevice, newDevice, transferQueue, commandPool, scene->mMeshes[node->mMeshes[i]], matToTex));
	}

	for (size_t i = 0; i < node->mNumChildren; ++i)
	{
		std::vector<Mesh> childList = LoadNode(newPhysicalDevice, newDevice, transferQueue, commandPool, node->mChildren[i], scene, matToTex);
		meshList.insert(meshList.end(), childList.begin(), childList.end());
	}

	return meshList;
}

Mesh MeshModel::LoadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue,
	VkCommandPool commandPool, aiMesh* mesh, std::vector<int> matToTex)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vertices.resize(mesh->mNumVertices);

	for (size_t i = 0; i < mesh->mNumVertices; ++i)
	{
		vertices[i].pos = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};

		if (mesh->mTextureCoords[0])
		{
			vertices[i].tex = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y,};
		}
		else
		{
			vertices[i].tex = {0.0f, 0.0f};
		}

		vertices[i].col = {1.0f, 1.0f, 1.0f};
	}

	// Iterate over indices through faces and copy across
	for (size_t i = 0; i < mesh->mNumFaces; ++i)
	{
		// get face
		const aiFace face = mesh->mFaces[i];
		
		for (size_t j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	return Mesh(newPhysicalDevice, newDevice, transferQueue, commandPool, &vertices, &indices, matToTex[mesh->mMaterialIndex]);

	
}

MeshModel::MeshModel(): model()
{
}

MeshModel::MeshModel(std::vector<Mesh> newMeshList)
	: meshList(std::move(newMeshList)), model()
{
}

MeshModel::~MeshModel()
{
	DestroyMeshModel();
}

size_t MeshModel::GetMeshCount() const
{
	return meshList.size();
}

Mesh* MeshModel::GetMesh(const size_t index)
{
	if (index >= meshList.size()) return nullptr;
	
	return &meshList[index];
}

glm::mat4 MeshModel::GetModel() const
{
	return model;
}

glm::mat4* MeshModel::GetModelPtr()
{
	return &model;
}

void MeshModel::SetModel(const glm::mat4 newModel)
{
	model = newModel;
}

void MeshModel::DestroyMeshModel()
{
	for (auto& mesh : meshList)
	{
		mesh.DestroyMeshBuffers();
	}

	meshList.clear();
}
