#include "SphereRenderer.h"
#include "Platform/CrossPlatform/Camera.h"

#if SIMUL_EDITOR
using namespace platform;
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
	LoadShaders();
}

void SphereRenderer::InvalidateDeviceObjects()
{
	sphereConstants.InvalidateDeviceObjects();
	renderPlatform=nullptr;
	effect=nullptr;
}

void SphereRenderer::RecompileShaders()
{
	renderPlatform->ScheduleRecompileEffects(
		{"sphere"},
		[this]
		{
			reload_shaders = true;
		});
}

void SphereRenderer::LoadShaders()
{
	reload_shaders = false;
	effect=renderPlatform->GetOrCreateEffect("sphere");
}

void SphereRenderer::DrawCrossSection(GraphicsDeviceContext &deviceContext,crossplatform::Effect *effect, crossplatform::Texture *t, vec3 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int pass)
{
	if (reload_shaders)
		LoadShaders();

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

	renderPlatform->SetConstantBuffer(deviceContext, &sphereConstants);

	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, pass);
	deviceContext.renderPlatform->Draw(deviceContext, 4, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawMultipleCrossSections(GraphicsDeviceContext& deviceContext, crossplatform::Effect* effect, crossplatform::Texture* t, vec3 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int slices)
{
	if (reload_shaders)
		LoadShaders();

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
	renderPlatform->SetConstantBuffer(deviceContext, &sphereConstants);

	deviceContext.renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, 0);
	deviceContext.renderPlatform->Draw(deviceContext, 4, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawLatLongSphere(GraphicsDeviceContext &deviceContext,int lat, int longt,vec3 origin,float radius,vec4 colour)
{
	if (reload_shaders)
		LoadShaders();

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
	
	sphereConstants.sphereCamPos	=cam_pos;
	sphereConstants.latitudes		=lat;
	sphereConstants.longitudes		=longt;
	static int loop=100;
	sphereConstants.loopSteps		=loop;
	sphereConstants.radius			=radius;
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);
	effect->Apply(deviceContext,tech,0);

	renderPlatform->SetTopology(deviceContext, Topology::LINESTRIP);
	// first draw the latitudes. We will draw (latitudes-1) loops - because at +-90 degrees the loop is just a point.
	// Each loop has loop+3 vertices, being (loop+1) total, plus one invisible vertex at the start, plus and one invisible one at the end.
	int lat_vertices=(loop+3)*(sphereConstants.latitudes-1);
	int long_vertices=(loop+3)*(sphereConstants.longitudes);
	renderPlatform->Draw(deviceContext,lat_vertices+long_vertices,0);//(sphereConstants.longitudes+1)*(loop+1)+, 0);
	//  draw the longitudes:
	//renderPlatform->Draw(deviceContext, (sphereConstants.longitudes+1)*(sphereConstants.latitudes+1)*2, 0);

	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawQuad(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, vec4 fill_colour, bool cheq)
{
	if (reload_shaders)
		LoadShaders();

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
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);
	if(fill_colour.w>0.0f)
	{
		effect->Apply(deviceContext, tech, cheq ? "chequer" : "fill");
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
		renderPlatform->Draw(deviceContext, 4, 0);
		effect->Unapply(deviceContext);
	}
	if (colour.w > 0.0f)
	{
		effect->Apply(deviceContext, tech,"outline");
		renderPlatform->SetTopology(deviceContext, Topology::LINELIST);
		// 8 vertices= no axes. 16 would draw axes also.
		renderPlatform->Draw(deviceContext, 8, 0);
		effect->Unapply(deviceContext);
	}
}

void SphereRenderer::DrawColouredSphere(GraphicsDeviceContext &deviceContext, vec3 origin, float sph_rad, vec4 clr)
{
	if (reload_shaders)
		LoadShaders();

	math::Matrix4x4 view = deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp, world;
	world.ResetToUnitMatrix();

	world._41 = origin.x;
	world._42 = origin.y;
	world._43 = origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp, world, view, proj);
	sphereConstants.debugWorldViewProj = wvp;
	sphereConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float *)&cam_pos, (float *)&view_dir);
	crossplatform::EffectTechnique *tech = effect->GetTechniqueByName("draw_coloured_sphere");
	// sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.debugColour = clr;
	sphereConstants.sphereCamPos = cam_pos;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.multiplier = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	renderPlatform->SetConstantBuffer(deviceContext, &sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext, tech, 0);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawTexturedSphere(GraphicsDeviceContext &deviceContext,vec3 origin,float sph_rad,crossplatform::Texture *texture,vec4 clr)
{
	if (reload_shaders)
		LoadShaders();

	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	sphereConstants.invWorldViewProj = deviceContext.viewStruct.invViewProj;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_textured_sphere");
	auto imageTexture=effect->GetShaderResource("imageTexture");
	renderPlatform->SetTexture(deviceContext, imageTexture, texture);
	//sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.debugColour		=clr;
	sphereConstants.sphereCamPos	=cam_pos;
	sphereConstants.debugViewDir	=view_dir;
	sphereConstants.multiplier		=vec4(1.0f, 1.0f, 1.0f, 1.0f);
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext,tech,0);
	renderPlatform->DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawTexture(GraphicsDeviceContext &deviceContext,crossplatform::Texture *t,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour)
{
	if (reload_shaders)
		LoadShaders();

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
	renderPlatform->SetTexture(deviceContext, imageTexture, t);
	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview		=qsize*0.5f;
	sphereConstants.debugColour		=colour;
	sphereConstants.debugViewDir	=view_dir;
	sphereConstants.multiplier		=vec4(1.0f, 1.0f, 1.0f, 1.0f);
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLESTRIP);
	effect->Apply(deviceContext,tech,0);
	renderPlatform->Draw(deviceContext,16, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawCurvedTexture(GraphicsDeviceContext& deviceContext, crossplatform::Texture* t, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour)
{
	if (reload_shaders)
		LoadShaders();

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
	renderPlatform->SetTexture(deviceContext, imageTexture, t);
	sphereConstants.quaternion = orient_quat;
	sphereConstants.radius = sph_rad;
	sphereConstants.sideview = qsize * 0.5f;
	sphereConstants.debugColour = colour;
	sphereConstants.debugViewDir = view_dir;
	sphereConstants.multiplier = colour;
	renderPlatform->SetConstantBuffer(deviceContext, &sphereConstants);

	renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
	effect->Apply(deviceContext, tech, 0);
	renderPlatform->Draw(deviceContext, 8 * 8 * 6, 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawCircle(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float rad,float sph_rad, vec4 colour, vec4 fill_colour)
{
	if (reload_shaders)
		LoadShaders();

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
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view, (float *)&cam_pos, (float *)&view_dir);
	crossplatform::EffectTechnique *tech = effect->GetTechniqueByName("draw_circle_on_sphere");

	sphereConstants.quaternion		=orient_quat;
	sphereConstants.radius			=sph_rad;
	sphereConstants.sideview			=rad;
	sphereConstants.debugColour		=colour; 
	sphereConstants.multiplier		= fill_colour;
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);

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
	if (reload_shaders)
		LoadShaders();

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
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_arc_on_sphere");

	sphereConstants.quaternion		=q1;
	sphereConstants.quaternion2		=q2;
	sphereConstants.radius			=sph_rad;
	sphereConstants.debugColour		=colour; 
	sphereConstants.multiplier		=colour;
	sphereConstants.latitudes		=12;
	sphereConstants.loopSteps		=12;
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);
	effect->Apply(deviceContext,tech,"outline");
	renderPlatform->SetTopology(deviceContext, Topology::LINESTRIP);
	renderPlatform->Draw(deviceContext,(sphereConstants.loopSteps+1), 0);
	effect->Unapply(deviceContext);
}

void SphereRenderer::DrawAxes(GraphicsDeviceContext &deviceContext,vec4 orient_quat,vec3 pos, float size)
{
	if (reload_shaders)
		LoadShaders();
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=0;
	world._42=0;
	world._43=0;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	sphereConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=effect->GetTechniqueByName("draw_axes_on_sphere");

	sphereConstants.quaternion			=orient_quat;
	sphereConstants.position			=pos;
	sphereConstants.sideview			=size;
	sphereConstants.debugColour			={1.f,1.f,1.f,1.f};
	sphereConstants.multiplier			={1.f,1.f,1.f,1.f};
	sphereConstants.debugViewDir		=view_dir;
	renderPlatform->SetConstantBuffer(deviceContext,&sphereConstants);
	{
		effect->Apply(deviceContext, tech,"main");
		renderPlatform->SetTopology(deviceContext, Topology::LINELIST);
		renderPlatform->Draw(deviceContext, 16, 0);
		effect->Unapply(deviceContext);
	}
}
#endif