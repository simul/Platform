#include "SphereRenderer.h"

#if SIMUL_EDITOR
using namespace simul;
using namespace crossplatform;

SphereRenderer::SphereRenderer()
	:renderPlatform(nullptr)
{
}

SphereRenderer::~SphereRenderer()
{
	InvalidateDeviceObjects();
}

void SphereRenderer::RestoreDeviceObjects(RenderPlatform *r)
{
	renderPlatform=r;
	if(!renderPlatform)
		return;
	sphereConstants.RestoreDeviceObjects(r);
	delete effect;
	effect=renderPlatform->CreateEffect("sphere");
}

void SphereRenderer::InvalidateDeviceObjects()
{
	sphereConstants.InvalidateDeviceObjects();
	renderPlatform=nullptr;
	delete effect;
	effect=nullptr;
}
void SphereRenderer::RecompileShaders()
{
	delete effect;
	effect=renderPlatform->CreateEffect("sphere");
}

void SphereRenderer::DrawCrossSection(GraphicsDeviceContext &deviceContext,crossplatform::Effect *effect, crossplatform::Texture *t, vec2 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour)
{
	math::Matrix4x4 view = deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp, world;
	world.ResetToUnitMatrix();

	world._41 = origin.x;
	world._42 = origin.y;
	world._43 = origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp, world, view, proj);
	sphereConstants.debugWorldViewProj = wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float*)&cam_pos, (float*)&view_dir);
	crossplatform::EffectTechnique *tech = effect->GetTechniqueByName("draw_cross_section_on_sphere");
	effect->SetTexture(deviceContext, "cloudVolume", t);
	sphereConstants.quaternion = orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.sideview = qsize * 0.5f;
	sphereConstants.debugColour = colour;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.texcOffset = texcOffset;

	effect->SetConstantBuffer(deviceContext, &sphereConstants);

	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, 0);
	deviceContext.renderPlatform->Draw(deviceContext, 4, 0);
	effect->Unapply(deviceContext);
}
void SphereRenderer::DrawMultipleCrossSections(GraphicsDeviceContext& deviceContext, crossplatform::Effect* effect, crossplatform::Texture* t, vec2 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int slices)
{
	math::Matrix4x4 view = deviceContext.viewStruct.view;
	const math::Matrix4x4& proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp, world;
	world.ResetToUnitMatrix();

	world._41 = origin.x;
	world._42 = origin.y;
	world._43 = origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp, world, view, proj);
	sphereConstants.debugWorldViewProj = wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float*)&cam_pos, (float*)&view_dir);
	crossplatform::EffectTechnique* tech = effect->GetTechniqueByName("draw_multiple_cross_sections_on_sphere");
	effect->SetTexture(deviceContext, "cloudVolume", t);
	sphereConstants.quaternion = orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.sideview = qsize * 0.5f;
	sphereConstants.debugColour = colour;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.texcOffset = texcOffset;
	sphereConstants.slices = slices;
	effect->SetConstantBuffer(deviceContext, &sphereConstants);

	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, 0);
	deviceContext.renderPlatform->Draw(deviceContext, 4, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawLatLongSphere(GraphicsDeviceContext &deviceContext,int lat, int longt,vec3 origin,float radius,vec4 colour)
{
	math::Matrix4x4 &view=deviceContext.viewStruct.view;
	math::Matrix4x4 &proj = deviceContext.viewStruct.proj;
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	sphereConstants.multiplier		 = colour;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_lat_long_sphere");
	
	sphereConstants.latitudes		=lat;
	sphereConstants.longitudes		=longt;
	sphereConstants.radius			=radius;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);
	effect->Apply(deviceContext,tech,0);

	renderPlatform->SetTopology(deviceContext, Topology::LINESTRIP);
	// first draw the latitudes:
	renderPlatform->Draw(deviceContext, (sphereConstants.longitudes+1)*(sphereConstants.latitudes+1)*2, 0);
	// first draw the longitudes:
	renderPlatform->Draw(deviceContext, (sphereConstants.longitudes+1)*(sphereConstants.latitudes+1)*2, 0);

	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawQuad(GraphicsDeviceContext &deviceContext,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour, vec4 fill_colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_quad_on_sphere");

	sphereConstants.quaternion			=orient_quat;
	sphereConstants.radius				=sph_rad;
	sphereConstants.sideview			=qsize*0.5f;
	sphereConstants.debugColour			=colour;
	sphereConstants.multiplier			=fill_colour;
	sphereConstants.debugViewDir		=view_dir;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);
	if(fill_colour.w>0.0f)
	{
		effect->Apply(deviceContext, tech, "chequer");
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
		renderPlatform->Draw(deviceContext, 4, 0);
		effect->Unapply(deviceContext);
	}
	if (colour.w > 0.0f)
	{
		effect->Apply(deviceContext, tech,"outline");
		renderPlatform->SetTopology(deviceContext, Topology::LINELIST);
		renderPlatform->Draw(deviceContext, 16, 0);
		effect->Unapply(deviceContext);
	}
}

void SphereRenderer::DrawTexture(GraphicsDeviceContext &deviceContext,crossplatform::Texture *t,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_texture_on_sphere");
	auto imageTexture=effect->GetShaderResource("imageTexture");
	effect->SetTexture(deviceContext,imageTexture,t);
	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview		=qsize*0.5f;
	sphereConstants.debugColour		=colour;
	sphereConstants.debugViewDir	=view_dir;
	sphereConstants.multiplier		=vec4(1.0f, 1.0f, 1.0f, 1.0f);
	effect->SetConstantBuffer(deviceContext,&sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext,tech,0);
	renderPlatform->Draw(deviceContext,16, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawCurvedTexture(GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour)
{
	math::Matrix4x4 view = deviceContext.viewStruct.view;
	const math::Matrix4x4& proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp, world;
	world.ResetToUnitMatrix();

	world._41 = origin.x;
	world._42 = origin.y;
	world._43 = origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp, world, view, proj);
	sphereConstants.debugWorldViewProj = wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float*)&cam_pos, (float*)&view_dir);
	crossplatform::EffectTechnique* tech = effect->GetTechniqueByName("draw_curved_texture_on_sphere");
	auto imageTexture = effect->GetShaderResource("imageTexture");
	effect->SetTexture(deviceContext, imageTexture, t);
	sphereConstants.quaternion = orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.sideview = qsize * 0.5f;
	sphereConstants.debugColour = colour;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.multiplier = colour;
	effect->SetConstantBuffer(deviceContext, &sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
	effect->Apply(deviceContext, tech, 0);
	renderPlatform->Draw(deviceContext, 8 * 8 * 6, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawCircle(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float rad,float sph_rad, vec4 colour, vec4 fill_colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_circle_on_sphere");

	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview			=rad;
	sphereConstants.debugColour		=colour; 
	sphereConstants.multiplier		= fill_colour;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);

	if (fill_colour.w > 0.0f)
	{
		effect->Apply(deviceContext, tech, "fill");
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
		renderPlatform->Draw(deviceContext, 64, 0);
		effect->Unapply(deviceContext);
	}

	effect->Apply(deviceContext,tech,"outline");
	renderPlatform->SetTopology(deviceContext, Topology::LINESTRIP);
	renderPlatform->Draw(deviceContext,32, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawArc(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 q1, vec4 q2, float sph_rad, vec4 colour)
{
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_arc_on_sphere");

	sphereConstants.quaternion		=q1;
	sphereConstants.quaternion2		=q2;
	sphereConstants.radius			=sph_rad;
	sphereConstants.debugColour		=colour; 
	sphereConstants.multiplier		=colour;
	sphereConstants.latitudes		=12;
	effect->SetConstantBuffer(deviceContext,&sphereConstants);
	effect->Apply(deviceContext,tech,"outline");
	renderPlatform->SetTopology(deviceContext, Topology::LINESTRIP);
	renderPlatform->Draw(deviceContext,13, 0);
	effect->Unapply(deviceContext);
}
#endif