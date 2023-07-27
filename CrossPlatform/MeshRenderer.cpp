#include "MeshRenderer.h"
#include "Material.h"

using namespace platform;
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

void MeshRenderer::LoadShaders()
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

void MeshRenderer::DrawSubMesh(GraphicsDeviceContext& deviceContext, Mesh* mesh, int index)
{
	cameraConstants.world = deviceContext.viewStruct.model;
	cameraConstants.worldViewProj = deviceContext.viewStruct.viewProj;
	cameraConstants.view = deviceContext.viewStruct.view;
	cameraConstants.proj = deviceContext.viewStruct.proj;
	cameraConstants.viewPosition = deviceContext.viewStruct.cam_pos;
	mat4::mul(cameraConstants.worldViewProj, cameraConstants.world, *((mat4*)(&deviceContext.viewStruct.viewProj)));
	mat4::mul(cameraConstants.modelView, cameraConstants.world, *((mat4*)(&deviceContext.viewStruct.view)));
	//effect->SetConstantBuffer(deviceContext, &cameraConstants);
	renderPlatform->SetConstantBuffer(deviceContext, &cameraConstants);
	Mesh::SubMesh* subMesh = mesh->GetSubMesh(index);
	
	Material* mat = subMesh->material;
	ApplyMaterial(deviceContext, mat);
	auto *vb= mesh->GetVertexBuffer();
	renderPlatform->SetVertexBuffers(deviceContext, 0, 1, &vb, mesh->GetLayout());
	renderPlatform->SetIndexBuffer(deviceContext, mesh->GetIndexBuffer());
	renderPlatform->DrawIndexed(deviceContext, subMesh->TriangleCount * 3, subMesh->IndexOffset, 0);
}

void MeshRenderer::DrawSubNode(GraphicsDeviceContext& deviceContext, Mesh* mesh, const Mesh::SubNode& subNode)
{
	auto mat = subNode.orientation.GetMatrix();
	mat.Transpose();
	deviceContext.viewStruct.PushModelMatrix(mat);
	for (int i = 0; i < subNode.subMeshes.size(); i++)
		DrawSubMesh(deviceContext, mesh,subNode.subMeshes[i]);
	for (int i = 0; i < subNode.children.size(); i++)
		DrawSubNode(deviceContext, mesh, subNode.children[i]);
	deviceContext.viewStruct.PopModelMatrix();
}

void MeshRenderer::Render(GraphicsDeviceContext &deviceContext, Mesh *mesh, mat4 model, Texture *diffuseCubemap,Texture *specularCubemap,Texture *screenspaceShadowTexture)
{
	if (!effect)
		LoadShaders();
	if (!effect)
		return;
	deviceContext.viewStruct.PushModelMatrix(*((math::Matrix4x4*)&model));
	effect->SetTexture(deviceContext, "diffuseCubemap", diffuseCubemap);
	effect->SetTexture(deviceContext, "specularCubemap", specularCubemap);
	effect->SetTexture(deviceContext, "screenspaceShadowTexture", screenspaceShadowTexture);
	mesh->BeginDraw(deviceContext, platform::crossplatform::ShadingMode::SHADING_MODE_SHADED);
	mat4 w;
	mat4 tw =*( (mat4*)&(mesh->orientation.GetMatrix()));
	tw.transpose();
	mat4::mul(w, tw, model);
	
	for (auto c : mesh->children)
	{
		Render(deviceContext, c,w,diffuseCubemap,specularCubemap,screenspaceShadowTexture);
	}
	DrawSubNode(deviceContext,mesh,mesh->GetRootNode());
	mesh->EndDraw(deviceContext);
	deviceContext.viewStruct.PopModelMatrix();
}

void MeshRenderer::ApplyMaterial(DeviceContext &deviceContext, Material *material)
{
	//solidConstants.lightIrradiance = physicalLightRenderData.lightColour;
	//solidConstants.lightDir = physicalLightRenderData.dirToLight;
	//solidConstants.albedo = material->albedo.value;
	//solidConstants.roughness = material->roughness.value;
	//solidConstants.metal = material->metal.value;

	vec2 vec2_unit(1.0f,1.0f);
	vec4 vec4_unit(1.0f, 1.0f, 1.0f, 1.0f);
	vec4 vec3_unit(1.0f,  1.0f, 1.0f);
	renderPlatform->SetTexture(deviceContext,effect->GetShaderResource("diffuseTexture"),material->albedo.texture);
	renderPlatform->SetTexture(deviceContext, effect->GetShaderResource("normalTexture"), material->normal.texture);
	renderPlatform->SetTexture(deviceContext, effect->GetShaderResource("metalTexture"), material->metal.texture);
	renderPlatform->SetTexture(deviceContext, effect->GetShaderResource("ambientOcclusionTexture"), material->ambientOcclusion.texture);
	renderPlatform->SetTexture(deviceContext, effect->GetShaderResource("emissiveTexture"), material->emissive.texture);
	solidConstants.diffuseOutputScalar						=vec4(material->albedo.value,1.0f);
	solidConstants.diffuseTexCoordsScalar_R					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_G					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_B					=vec2_unit;
	solidConstants.diffuseTexCoordsScalar_A					=vec2_unit;

	solidConstants.normalOutputScalar						=vec4_unit;
	solidConstants.normalTexCoordsScalar_R					=vec2_unit;
	solidConstants.normalTexCoordsScalar_G					=vec2_unit;
	solidConstants.normalTexCoordsScalar_B					=vec2_unit;
	solidConstants.normalTexCoordsScalar_A					=vec2_unit;

	solidConstants.combinedOutputScalarRoughMetalOcclusion	=vec4(material->roughness.value,material->metal.value,material->ambientOcclusion.value,0);
	solidConstants.combinedTexCoordsScalar_R				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_G				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_B				=vec2_unit;
	solidConstants.combinedTexCoordsScalar_A				=vec2_unit;

	solidConstants.emissiveOutputScalar						= material->emissive.value;
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