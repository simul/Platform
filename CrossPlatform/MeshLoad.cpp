#include "Mesh.h"
#include "Macros.h"
#include "Platform/CrossPlatform/Material.h"
#include "Platform/CrossPlatform/AxesStandard.h"
#include "Platform/Core/StringFunctions.h"
using namespace simul;
using namespace crossplatform;

#ifdef PLATFORM_USE_ASSIMP
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <assimp/DefaultLogger.hpp>     // Post processing flags
#include <assimp/pbrmaterial.h>     // PBR defs

#include <iomanip> // for setw

using namespace Assimp;

void CopyNodesWithMeshes(Mesh *mesh,aiNode &node, Mesh::SubNode &subNode,float scale, AxesStandard fromStandard,AxesStandard toStandard)
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
	mat4 transform= *((mat4*)&node.mTransformation);
	transform=ConvertMatrix(fromStandard, AxesStandard::Engineering, transform);
	vec4 sc= { 1.f,1.f,1.f,scale };
	transform._m03*=scale;
	transform._m13*=scale;
	transform._m23*=scale;
	transform.transpose();
	subNode.orientation.Define(*((const math::Matrix4x4*)&transform));
	subNode.children.resize(node.mNumChildren);
	// continue for all child nodes
	for(unsigned i=0;i<node.mNumChildren;i++)
	{
		CopyNodesWithMeshes(mesh,*node.mChildren[i], subNode.children[i], scale, fromStandard, toStandard);
	}
}

static void ConvertMaterial(RenderPlatform *renderPlatform,Material* M, const aiMaterial *m)
{
	auto Colour4 = [&, m](const char* name,vec4 dflt= {1.f, 1.f, 1.f, 1.f})
	{
		aiColor4D aiColour;
		vec4 result=dflt;
		if (m->Get(name, aiTextureType_NONE, 0, aiColour) == aiReturn_SUCCESS)
			result = vec4(aiColour.r, aiColour.g, aiColour.b, aiColour.a);
		return result;
	};
	auto Colour3 = [&, m](const char* name, vec3 dflt = { 1.f, 1.f, 1.f })
	{
		aiColor3D aiColour;
		vec3 result = dflt;
		if(m->Get(name, aiTextureType_NONE, 0, aiColour)==aiReturn_SUCCESS)
			result=vec3(aiColour.r, aiColour.g, aiColour.b);
		return result;
	};
	auto Float = [&, m](const char* name, float deflt)
	{
		float result = deflt;
		if (m->Get(name, aiTextureType_NONE, 0, result) == aiReturn_SUCCESS)
			return result;
		else
			return deflt;
	};
	auto String = [&, m](const char* name)
	{
		aiString str;
		m->Get(name, aiTextureType_NONE, 0, str);
		return std::string(str.C_Str());
	};
	SIMUL_COUT<<"\n"<<String("?mat.name").c_str()<<std::endl;
	std::cout<<std::setw(5)<<std::setprecision(4)<<std::fixed;
	for(unsigned i=0;i<m->mNumProperties;i++)
	{
		const aiMaterialProperty *p= m->mProperties[i];
		SIMUL_COUT<<p->mKey.C_Str()<<", type "<<p->mType<<", index "<<p->mIndex<<":";
		if(p->mType==aiPTI_Float)
		{
			if(p->mDataLength==4)
			{
				float result=Float(p->mKey.C_Str(),0.0f);
				std::cout<<" "<<result;
			}
			if(p->mDataLength==12)
			{
				vec4 result=Colour3(p->mKey.C_Str());
				std::cout<<" "<<result.x<<"," << result.y << "," << result.z;
			}
			if(p->mDataLength==16)
			{
				vec4 result=Colour4(p->mKey.C_Str());
				std::cout<<" "<<result.x<<"," << result.y << "," << result.z << "," << result.w;
			}
		}
		std::cout<<std::endl;
	}
	aiTextureMapping mapping;
	unsigned uvindex = 0;
	ai_real blend = 1.0f;
	for(unsigned t= aiTextureType_DIFFUSE;t< aiTextureType_UNKNOWN;t++)
	{
		unsigned num=  m->GetTextureCount((aiTextureType)t);
		if(num>0)
			SIMUL_COUT << "Texture type "<<t << std::endl;
		for (unsigned i = 0; i <num; i++)
		{
			aiString texturePath;
			m->GetTexture((aiTextureType)t,i,&texturePath,&mapping,&uvindex,&blend);
			SIMUL_COUT <<"\t"<< texturePath.C_Str() << std::endl;
		}
	}
	M->albedo.value= Colour3("$clr.diffuse")+ Colour3("$clr.ambient") + Colour3("$clr.specular");
	M->emissive.value = Colour3("$clr.emissive");
	//m->Get(AI_MATKEY_COLOR_DIFFUSE, aiColour);
	m->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, M->metal.value);
	m->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, M->roughness.value);

	// Seems to be always 1.0
	M->ambientOcclusion.value=1.0f;
	
//$mat.twosided, type 5, index 0
//$mat.shininess, type 1, index 0
	aiString texturePath;
	if(aiReturn_SUCCESS==m->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture *T=renderPlatform->GetOrCreateTexture(texturePath.C_Str(),true);
		M->albedo.texture=T;
	}
	else
	{
		M->albedo.texture = renderPlatform->GetOrCreateTexture("white");
	}
	if (aiReturn_SUCCESS == m->GetTexture(aiTextureType_NORMALS, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture* T = renderPlatform->GetOrCreateTexture(texturePath.C_Str(),true);
		M->normal.texture = T;
	}
	else
	{
		M->normal.texture = renderPlatform->GetOrCreateTexture("blue");
	}
	if (aiReturn_SUCCESS == m->GetTexture(aiTextureType_METALNESS, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture* T = renderPlatform->GetOrCreateTexture(texturePath.C_Str());
		M->metal.texture = T;
	}
	else
	{
		M->metal.texture = renderPlatform->GetOrCreateTexture("black");
	}
	if (aiReturn_SUCCESS == m->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture* T = renderPlatform->GetOrCreateTexture(texturePath.C_Str());
		M->ambientOcclusion.texture = T;
	}
	else if(aiReturn_SUCCESS == m->GetTexture(aiTextureType_LIGHTMAP, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture* T = renderPlatform->GetOrCreateTexture(texturePath.C_Str());
		M->ambientOcclusion.texture = T;
	}
	else
	{
		M->ambientOcclusion.texture = renderPlatform->GetOrCreateTexture("white");
	}
	if (aiReturn_SUCCESS == m->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath, &mapping, &uvindex, &blend))
	{
		Texture* T = renderPlatform->GetOrCreateTexture(texturePath.C_Str());
		M->emissive.texture = T;
	}
	else
	{
		M->emissive.texture = renderPlatform->GetOrCreateTexture("black");
	}
	M->roughness.value = 1.0f-Float("$mat.shininess",1.0f);

	
}
#include "Platform/Core/StringFunctions.h"
void Mesh::Load(const char* filenameUtf8,float scale,AxesStandard fromStandard)
{
	InvalidateDeviceObjects();
	// Create an instance of the Importer class
	Importer importer;

	// Create a logger instance
	DefaultLogger::create("", Logger::VERBOSE);
	ERRNO_BREAK

	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll
	// probably to request more postprocessing than we do in this example.
	const aiScene* scene = importer.ReadFile(filenameUtf8,
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType);
	errno=0;
	if(!scene)
		return;
	std::string short_filename=scene->GetShortFilename(filenameUtf8);
	std::vector< Material*> materials;
	std::string file;
	std::vector<std::string>pathsplit=base::SplitPath(filenameUtf8);
	if(pathsplit.size())
		renderPlatform->PushTexturePath(pathsplit[0].c_str());
	for(unsigned i=0;i<scene->mNumMaterials;i++)
	{
		const aiMaterial *m=scene->mMaterials[i];
		aiString name;
		m->Get("?mat.name",0,0,name);
		if(name.length==0)
		{
			name=base::QuickFormat("%s %d",short_filename.c_str(),i);
		}
		Material *M=renderPlatform->GetOrCreateMaterial(base::QuickFormat("%s", name.C_Str()));
		materials.push_back(M);
		ConvertMaterial(renderPlatform,M,m);
	}
	if (pathsplit.size())
		renderPlatform->PopTexturePath();

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
			v.pos=scale*ConvertPosition(fromStandard,AxesStandard::Engineering,(*((vec3*)&(mesh->mVertices[j]))));
			if(mesh->mNormals)
			{
				v.normal= ConvertPosition(fromStandard, AxesStandard::Engineering, *((vec3*)&(mesh->mNormals[j])));
			}
			if (mesh->mTangents)
			{
				v.tangent = ConvertPosition(fromStandard, AxesStandard::Engineering, *((vec3*)&(mesh->mTangents[j])));
			}
			if(mesh->mTextureCoords)
			{
			// Textures converted from GL-style to D3D
				if(mesh->mTextureCoords[0])
				{
					v.texc0.x=mesh->mTextureCoords[0][j].x;
					v.texc0.y=1.0f- mesh->mTextureCoords[0][j].y;
				}
				if(mesh->mTextureCoords[1])
				{
					v.texc1.x = mesh->mTextureCoords[1][j].x;
					v.texc1.y = 1.0f - mesh->mTextureCoords[1][j].y;
				}
			}
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
		if (mesh->mMaterialIndex >= 0)
			subMesh->material=materials[mesh->mMaterialIndex];
	}
	if(scene->mRootNode)
		CopyNodesWithMeshes(this, *scene->mRootNode, rootNode,scale, fromStandard, AxesStandard::Engineering);
	// Kill it after the work is done
	DefaultLogger::kill();
	errno = 0;
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

