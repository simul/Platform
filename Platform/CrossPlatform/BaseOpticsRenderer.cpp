#include "BaseOpticsRenderer.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Base/RuntimeError.h"
using namespace simul;
using namespace crossplatform;

BaseOpticsRenderer::BaseOpticsRenderer(simul::base::MemoryInterface *)
	:flare_magnitude(0.f)
	,flare_angular_size(3.14159f/180.f*10.f)
	,renderPlatform(NULL)
	,effect(NULL)
	,flare_texture(NULL)
{
	FlareTexture=("SunFlare.png");
}

BaseOpticsRenderer::~BaseOpticsRenderer()
{
	InvalidateDeviceObjects();
}

void BaseOpticsRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	SAFE_DELETE(flare_texture);
	flare_texture=renderPlatform->CreateTexture(FlareTexture.c_str());
	
	opticsConstants.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_DELETE(halo_textures[i]);
	}
	halo_textures.clear();
	int num_halo_textures=lensFlare.GetNumArtifactTypes();
	halo_textures.resize(num_halo_textures);
	for(int i=0;i<num_halo_textures;i++)
	{
		std::string tn=(lensFlare.GetTypeName(i));
		crossplatform::Texture* &tex=halo_textures[i];
		SAFE_DELETE(tex);
		tex=renderPlatform->CreateTexture((tn+".png").c_str());
	}

	RecompileShaders();
}

void BaseOpticsRenderer::InvalidateDeviceObjects()
{
	opticsConstants.InvalidateDeviceObjects();
	SAFE_DELETE(effect);
	SAFE_DELETE(flare_texture);
	for(size_t i=0;i<halo_textures.size();i++)
	{
		SAFE_DELETE(halo_textures[i]);
	}
	halo_textures.clear();
}

void BaseOpticsRenderer::EnsureEffectsAreBuilt(crossplatform::RenderPlatform *r)
{
	if (!r)
		return;
	std::vector<crossplatform::EffectDefineOptions> opts;
	r->EnsureEffectIsBuilt("optics", opts);
}

void BaseOpticsRenderer::RecompileShaders()
{
	if(!renderPlatform)
		return;
	std::map<std::string,std::string> defines;
	SAFE_DELETE(effect);
	effect=renderPlatform->CreateEffect("optics",defines);
	m_hTechniqueFlare			=effect->GetTechniqueByName("simul_flare");
	opticsConstants.LinkToEffect(effect,"OpticsConstants");
}

void BaseOpticsRenderer::RenderFlare(crossplatform::DeviceContext &deviceContext,float exposure,const float *dir,const float *light)
{
	if(!effect)
		return;
	deviceContext.renderPlatform->StoreRenderState(deviceContext);
	math::Vector3 sun_dir(dir);
	float magnitude=exposure;
	simul::math::FirstOrderDecay(flare_magnitude,magnitude,5.f,0.1f);
	if(flare_magnitude>1.f)
		flare_magnitude=1.f;
	//float alt_km=0.001f*cam_pos.y;
	vec3 sunlight(light);//=skyKeyframer->GetLocalIrradiance(alt_km)*lensFlare.GetStrength();
	// GetLocalIrradiance returns a value in Irradiance (watts per square metre).
	// But our colour values are in Radiance (watts per sq.m. per steradian)
	// So to get the sun colour, divide by the approximate angular area of the sun.
	// As the sun has angular radius of about 1/2 a degree, the angular area is 
	// equal to pi/(120^2), or about 1/2700 steradians;
	static float sun_mult=0.05f;
	simul::math::Vector3 cam_pos,cam_dir;
	simul::crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&cam_dir);
	lensFlare.UpdateCamera(cam_dir,sun_dir);
	flare_magnitude*=lensFlare.GetStrength();
	sunlight*=sun_mult*flare_magnitude;
	if(flare_magnitude>0.f)
	{
		effect->SetTexture(deviceContext,"flareTexture",flare_texture);
		SetOpticsConstants(opticsConstants,deviceContext.viewStruct,dir,sunlight,flare_angular_size*flare_magnitude);

effect->SetConstantBuffer(deviceContext,&		opticsConstants);
		effect->Apply(deviceContext,m_hTechniqueFlare,0);
		renderPlatform->DrawQuad(deviceContext);
		sunlight*=0.25f;
		for(int i=0;i<lensFlare.GetNumArtifacts();i++)
		{
			math::Vector3 pos=lensFlare.GetArtifactPosition(i);
			float sz=lensFlare.GetArtifactSize(i);
			int t=lensFlare.GetArtifactType(i);
			effect->SetTexture(deviceContext,"flareTexture",halo_textures[t]);
			SetOpticsConstants(opticsConstants,deviceContext.viewStruct,pos,sunlight,flare_angular_size*sz*flare_magnitude);

effect->SetConstantBuffer(deviceContext,&			opticsConstants);
			effect->Reapply(deviceContext);
			renderPlatform->DrawQuad(deviceContext);
		}
		effect->UnbindTextures(deviceContext);
		effect->Unapply(deviceContext);
	}
	deviceContext.renderPlatform->RestoreRenderState(deviceContext );
}

void BaseOpticsRenderer::SetOpticsConstants(OpticsConstants &c,const crossplatform::ViewStruct &viewStruct,const float *direction,const float *colour,float angularRadiusRadians)
{
	simul::math::Vector3 dir(direction);
	float Yaw=atan2(dir.x,dir.y);
	float Pitch=-asin(dir.z);
	simul::math::Matrix4x4 world, tmp1, tmp2;
	simul::math::Matrix4x4 view(viewStruct.view);
	simul::math::Matrix4x4 proj(viewStruct.proj);
	
	simul::geometry::SimulOrientation ori;
	ori.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
	ori.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
	world=ori.GetMatrix();
	//set up matrices
	view._41=0.f;
	view._42=0.f;
	view._43=0.f;
	simul::math::Vector3 sun2;
	simul::math::Multiply4x4(tmp1,world,view);
	simul::math::Multiply4x4(tmp2,tmp1,proj);
	c.worldViewProj	=tmp2;
	c.colour		=colour;
	c.radiusRadians	=angularRadiusRadians;

	simul::math::Matrix4x4 invProj;
	proj.Inverse(invProj);
	c.view		=view;
	c.invProj	=invProj;
	c.view.transpose();
	c.invProj.transpose();
	

	simul::math::Matrix4x4 viewproj,ivp;
	simul::math::Multiply4x4(viewproj,view,proj);
	viewproj.Inverse(ivp);

	c.invViewProj	=ivp;
	
	static float max_distance_metres=10000.0f;
	c.depthToLinFadeDistParams	=crossplatform::GetDepthToDistanceParameters(viewStruct,max_distance_metres);//(vec4(
									/*viewStruct.proj.m[3][2]
									,max_distance_metres
									,viewStruct.proj.m[2][2]*max_distance_metres
									,0.0f);*/

	c.lightDir	=direction;

	static float dropletRadius	=6.25f;
	static float rainbowIntensity=0.1f;

	c.dropletRadius		=dropletRadius; // up to 8.0f
	c.rainbowIntensity	=rainbowIntensity;	// Up to 0.1f
}
