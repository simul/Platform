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

void MeshRenderer::Render(DeviceContext &deviceContext, Mesh *mesh, mat4 model, Texture *diffuseCubemap,Texture *specularCubemap)
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
	solidConstants.albedo = material->albedo.value;
	solidConstants.roughness = material->roughness.value;
	solidConstants.metal = material->metal.value;
	effect->SetConstantBuffer(deviceContext, &solidConstants);
}