#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"

using namespace simul;
using namespace crossplatform;

MeshRenderer::MeshRenderer()
	:renderPlatform(nullptr)
	, effect(nullptr)
{
}


MeshRenderer::~MeshRenderer()
{
	InvalidateDeviceObjects();
}

void MeshRenderer::RestoreDeviceObjects(RenderPlatform *r)
{
	renderPlatform = r;
	cameraConstants.RestoreDeviceObjects(r);
	solidConstants.RestoreDeviceObjects(r);
	solidConstants.LinkToEffect(effect, "SolidConstants");
}

void MeshRenderer::RecompileShaders()
{
	delete effect;
	effect = renderPlatform->CreateEffect("solid");
}

void MeshRenderer::InvalidateDeviceObjects()
{
	cameraConstants.InvalidateDeviceObjects();
	solidConstants.InvalidateDeviceObjects();
	delete effect;
	effect = nullptr;
}

void MeshRenderer::Render(GraphicsDeviceContext &deviceContext, Mesh *mesh, mat4 model, Texture *diffuseCubemap,Texture *specularCubemap)
{
	if (!effect)
		RecompileShaders();
	if (!effect)
		return;
	//model.transpose();
	mat4 w;
	mat4 tw =*( (mat4*)&(mesh->orientation.GetMatrix()));
//	tw._14 = tw._24 = tw._34 =  0.0f;
	//tw._44 = 1.0f;
	tw.transpose();
	mat4::mul(w, tw, model);
	
	for (auto c : mesh->children)
	{
		Render(deviceContext, c,w,diffuseCubemap,specularCubemap);
	}
	cameraConstants.world = w;
	mat4::mul(cameraConstants.worldViewProj, w, *((mat4*)(&deviceContext.viewStruct.viewProj)));
	mat4::mul(cameraConstants.modelView, w, *((mat4*)(&deviceContext.viewStruct.view)));
	cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
	//cameraConstants.worldViewProj = deviceContext.viewStruct.viewProj;#
	effect->SetTexture(deviceContext, "diffuseCubemap", diffuseCubemap);
	effect->SetTexture(deviceContext, "specularCubemap", specularCubemap);
	effect->SetConstantBuffer(deviceContext, &cameraConstants);
	for (int i=0;i<mesh->mSubMeshes.size();i++)
	{
		Material *mat=mesh->mSubMeshes[i]->material;
		ApplyMaterial(deviceContext, mat);
		effect->Apply(deviceContext, "solid", 0);
		mesh->Draw(deviceContext, i);
		effect->Unapply(deviceContext);
	}
}

void MeshRenderer::ApplyMaterial(DeviceContext &deviceContext, Material *material)
{
	//solidConstants.lightIrradiance = physicalLightRenderData.lightColour;
	//solidConstants.lightDir = physicalLightRenderData.dirToLight;
	//solidConstants.albedo = material->albedo.value;
	//solidConstants.roughness = material->roughness.value;
	//solidConstants.metal = material->metal.value;

	int lightCount=0;
	int reverseDepth=1;
	vec2 vec2_unit(1.0f,1.0f);
	vec4 vec4_unit(1.0f, 1.0f, 1.0f, 1.0f);
	vec4 vec3_unit(1.0f,  1.0f, 1.0f);
	solidConstants.diffuseOutputScalar						=vec4_unit;
	solidConstants.diffuseTexCoordsScalar_R					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_G					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_B					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_A					=vec2_unit;

	solidConstants.normalOutputScalar						=vec4_unit;
	solidConstants.normalTexCoordsScalar_R					=vec2_unit;
	solidConstants.normalTexCoordsScalar_G					=vec2_unit;
	solidConstants.normalTexCoordsScalar_B					=vec2_unit;
	solidConstants.normalTexCoordsScalar_A					=vec2_unit;

	solidConstants.combinedOutputScalarRoughMetalOcclusion	=vec4_unit;
	solidConstants.combinedTexCoordsScalar_R				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_G				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_B				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_A				=vec2_unit;

	solidConstants.emissiveOutputScalar						= vec4_unit;
	solidConstants.emissiveTexCoordsScalar_R				=vec2_unit;
	solidConstants.emissiveTexCoordsScalar_G				=vec2_unit;
	solidConstants.emissiveTexCoordsScalar_B				=vec2_unit;
	solidConstants.emissiveTexCoordsScalar_A				=vec2_unit;

	solidConstants.u_SpecularColour							=vec3_unit;

	solidConstants.u_DiffuseTexCoordIndex=0;
	solidConstants.u_NormalTexCoordIndex=1;
	solidConstants.u_CombinedTexCoordIndex=2;
	solidConstants.u_EmissiveTexCoordIndex=3;

	effect->SetConstantBuffer(deviceContext, &solidConstants);
}