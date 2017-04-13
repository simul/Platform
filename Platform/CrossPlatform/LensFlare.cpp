#include "Simul/Platform/CrossPlatform/LensFlare.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Pi.h"
#include <math.h>
using namespace simul;
using namespace math;
using namespace crossplatform;

LensFlare::LensFlare()
{
	min_cosine=cos(45.f*SIMUL_PI_F/180.f);
	lightPosition[0]=0;
	lightPosition[1]=0;
	lightPosition[2]=0;
	visibility=1.f;
	SunScale=1.f;
	
	AddArtifact(-0.005f,0.125f,"FlareRainbow");
	AddArtifact(-0.0175f,0.25f,"FlarePentagon");
	AddArtifact(-0.025f,0.125f,"FlareRainbow");
	AddArtifact(-0.03f,0.2f,"FlarePentagon");

	AddArtifact(0.05f,0.125f,"FlarePentagon");
	AddArtifact(0.1f,0.125f,"SunHalo");
	AddArtifact(0.125f,0.3f,"FlareRainbow");
	AddArtifact(0.2f,0.2f,"FlarePentagon");
	AddArtifact(0.25f,0.125f,"SunHalo");
	AddArtifact(0.375f,0.35f,"FlarePentagon");
	AddArtifact(0.75f,0.125f,"FlareRainbow");
	AddArtifact(0.8f,0.2f,"FlarePentagon");
	AddArtifact(0.875f,0.25f,"FlareRainbow");
	AddArtifact(0.9f,0.4f,"SunHalo");
	AddArtifact(0.95f,0.15f,"FlarePentagon");
}

LensFlare::~LensFlare()
{
}

void LensFlare::SetSunScale(float s)
{
	SunScale=s;
}

int LensFlare::GetNumArtifacts() const
{
	return (int)artifacts.size();
}

const float *LensFlare::GetArtifactPosition(int i) const
{
	return artifacts[i].pos;
}

float LensFlare::GetArtifactSize(int i) const
{
	return artifacts[i].size;
}

int LensFlare::GetArtifactType(int i) const
{
	return artifacts[i].type;
}

const char *LensFlare::GetTypeName(int t) const
{
	return types[t].c_str();
}

void LensFlare::AddArtifact(float coord,float size,const char *type)
{
	artifacts.push_back(Artifact());
	Artifact &A=artifacts.back();
	A.coord=coord;
	std::string tstr(type);
	
	int t=-1;
	for(t=0;t<(int)types.size();t++)
	{
		if(types[t]==tstr)
			break;
	}
	if(t==(int)types.size())
	{
		types.push_back(tstr);
		t=(int)types.size()-1;
	}
	A.type=t;
	A.size=size;
}

int LensFlare::GetNumArtifactTypes()
{
	int num_halo_textures=0;
	for(int i=0;i<GetNumArtifacts();i++)
	{
		int t=GetArtifactType(i);
		if(t+1>num_halo_textures)
			num_halo_textures=t+1;
	}
	return num_halo_textures;
}

void LensFlare::UpdateCamera(const float *cam_d,const float *dir_to_light)
{
	static float LightDistance=10000.f;
	Vector3 light_dir(dir_to_light);
	Vector3 cam_dir(cam_d);
	Vector3 LightPosition=LightDistance*light_dir;

	float cosine=cam_dir*light_dir;

	if(cosine<min_cosine)
		Strength=0.f;
	else
		Strength=1.f;
	lightPosition[0]=LightPosition.x;
	lightPosition[1]=LightPosition.y;
	lightPosition[2]=LightPosition.z;

	Vector3 CameraVect = LightDistance * cam_dir;

	// The LensFlare effect takes place along this vector.
	Vector3 LFvect = (CameraVect - LightPosition);

	static float LF_scale = 4.215f;
	LFvect*=LF_scale;

	// The different sprites are placed along this line.
	
	for(size_t i=0;i<artifacts.size();i++)
	{
		artifacts[i].pos=LightPosition+( LFvect*artifacts[i].coord);//0.500);
		artifacts[i].pos.Normalize();
	}
}