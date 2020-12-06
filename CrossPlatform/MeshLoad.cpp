#include "Mesh.h"
#include "Macros.h"
using namespace simul;
using namespace crossplatform;

#ifdef PLATFORM_USE_ASSIMP
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <assimp/DefaultLogger.hpp>     // Post processing flags

using namespace Assimp;

void CopyNodesWithMeshes(Mesh *mesh,aiNode &node, Mesh::SubNode &subNode)
{
	static aiMatrix4x4 identity(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	// copy the meshes
	subNode.subMeshes.resize(node.mNumMeshes);
	for(unsigned i=0;i<node.mNumMeshes;i++)
	{
		subNode.subMeshes[i]=node.mMeshes[i];
	}
	// the new object is the parent for all child nodes
	math::Matrix4x4 transform= *((const math::Matrix4x4*)&node.mTransformation);
	subNode.orientation.Define(transform);
	subNode.children.resize(node.mNumChildren);
	// continue for all child nodes
	for(unsigned i=0;i<node.mNumChildren;i++)
	{
		CopyNodesWithMeshes(mesh,*node.mChildren[i], subNode.children[i]);
	}
}

void Mesh::Load(const char* filenameUtf8)
{
	InvalidateDeviceObjects();
	// Create an instance of the Importer class
	Importer importer;

	// Create a logger instance
	DefaultLogger::create("", Logger::VERBOSE);

	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll
	// probably to request more postprocessing than we do in this example.
	const aiScene* scene = importer.ReadFile(filenameUtf8,
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType);
	numVertices = 0;
	numIndices = 0;
	struct Vertex
	{
		vec3 pos;
		vec3 normal;
		vec4 tangent;
		vec2 texc0;
		vec2 texc1;
	};
	// Vertex declaration
	crossplatform::LayoutDesc layoutDesc[] =
	{
		{ "POSITION", 0, crossplatform::RGB_32_FLOAT, 0, 0, false, 0 },
		{ "NORMAL", 0, crossplatform::RGB_32_FLOAT, 0, 12, false, 0 },
		{ "TANGENT", 0, crossplatform::RGBA_32_FLOAT, 0, 24, false, 0 },
		{ "TEXCOORD", 0, crossplatform::RG_32_FLOAT, 0, 40, false, 0 },
		{ "TEXCOORD", 1, crossplatform::RG_32_FLOAT, 0, 48, false, 0 },
	};
	stride = sizeof(Vertex);
	SAFE_DELETE(layout);
	layout = renderPlatform->CreateLayout(5, layoutDesc, true);
	for(unsigned i=0;i<scene->mNumMeshes;i++)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		numVertices+=mesh->mNumVertices;
		numIndices+=mesh->mNumFaces*3;
	}
	unsigned vertex = 0;
	unsigned index= 0;
	std::vector<Vertex> vertices(numVertices);
	std::vector<unsigned> indices(numIndices);
	for (unsigned i = 0; i < scene->mNumMeshes; i++)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		int meshVertex=vertex;
		for(unsigned j=0;j<mesh->mNumVertices;j++)
		{
			Vertex &v=vertices[vertex++];
			v.pos=*((vec3*)&(mesh->mVertices[j]));
			if(mesh->mNormals)
				v.normal=*((vec3*)&(mesh->mNormals[j]));
		}
		for (unsigned j = 0; j < mesh->mNumFaces; j++)
		{
			for(unsigned k=0;k< mesh->mFaces[j].mNumIndices;k++)
			{
				if(mesh->mFaces[j].mNumIndices!=3)
				{
					SIMUL_CERR<<"Num indices is "<< mesh->mFaces[j].mNumIndices<<std::endl;
				}
				indices[index++] = meshVertex+mesh->mFaces[j].mIndices[k];
			}
		}
	}
	init(renderPlatform, numVertices, numIndices, vertices.data(), indices.data());
	vertex = 0;
	index = 0;
	for (unsigned i = 0; i < scene->mNumMeshes; i++)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		SubMesh *subMesh=SetSubMesh(i, index, mesh->mNumFaces * 3, nullptr);
		
	//	subMesh->orientation.Define(mat);
		vertex += mesh->mNumVertices;
		index += mesh->mNumFaces * 3;
	}
	if(scene->mRootNode)
		CopyNodesWithMeshes(this, *scene->mRootNode, rootNode);
	// Kill it after the work is done
	DefaultLogger::kill();
	// If the import failed, report it
	if (!scene)
	{
		SIMUL_CERR_ONCE<<"Failed to load "<<filenameUtf8<<std::endl;
		return;
	}
}

#else
void Mesh::Load(const char *filenameUtf8)
{
	SIMUL_CERR_ONCE << "Can't load " << filenameUtf8 <<" - no importer enabled."<< std::endl;
}
#endif

