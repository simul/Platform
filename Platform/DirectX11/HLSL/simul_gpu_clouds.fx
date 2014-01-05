#include "CppHLSL.hlsl"
uniform sampler2D inputTexture;
uniform sampler3D densityTexture SIMUL_TEXTURE_REGISTER(0);
uniform sampler3D lightTexture;
uniform sampler3D lightTexture1 SIMUL_TEXTURE_REGISTER(1);
uniform sampler3D lightTexture2 SIMUL_TEXTURE_REGISTER(2);
uniform sampler2D maskTexture SIMUL_TEXTURE_REGISTER(3);
uniform sampler3D ambientTexture;
uniform sampler3D ambientTexture1 SIMUL_TEXTURE_REGISTER(4);
uniform sampler3D ambientTexture2 SIMUL_TEXTURE_REGISTER(5);
uniform sampler3D volumeNoiseTexture SIMUL_TEXTURE_REGISTER(6);
RWTexture3D<float4> targetTexture SIMUL_RWTEXTURE_REGISTER(0);
RWTexture3D<float> targetTexture1 SIMUL_RWTEXTURE_REGISTER(1);

#include "states.hlsl"
#include "../../CrossPlatform/simul_gpu_clouds.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/noise.sl"
#include "../../CrossPlatform/spherical_harmonics_constants.sl"
#include "../../CrossPlatform/spherical_harmonics.sl"

#define pi (3.1415926536)
SamplerState lightSamplerState : register(s8);

struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
	float2 texCoords	: TEXCOORD0;		
};

vertexOutput VS_Main(idOnly IN)
{
    vertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	pos.y			=yRange.x+pos.y*yRange.y;
	float4 vert_pos	=float4(float2(-1.0,1.0)+2.0*vec2(pos.x,-pos.y),1.0,1.0);
    OUT.hPosition	=vert_pos;
    OUT.texCoords	=pos;
    return OUT;
}

struct ColourDepthOutput
{
	vec4 colour SIMUL_TARGET_OUTPUT;
	float depth	SIMUL_DEPTH_OUTPUT;
};

ColourDepthOutput PS_DensityMask(vertexOutput IN)
{
	ColourDepthOutput result;
	float dens					=GpuCloudMask(IN.texCoords, maskCentre, maskRadius,maskFeather, maskThickness);
	result.colour				=vec4(dens,dens,dens,dens);
	result.depth				=dens;
	return result;
}

[numthreads(8,8,8)]
void CS_Density(uint3 sub_pos				: SV_DispatchThreadID )	//SV_DispatchThreadID gives the combined id in each dimension.
{
    CS_CloudDensity(targetTexture,volumeNoiseTexture,maskTexture,sub_pos);
}

static const float glow=0.1;

[numthreads(8,8,1)]
void CS_Lighting(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float direct_light				=1.0;
	targetTexture1[int3(pos.xy,0)]	=1.0;
	const int C=1;
	for(uint i=1;i<dims.z;i++)
	{
		uint3 idx					=uint3(pos.xy,i);
		targetTexture1[idx]			=direct_light;
		for(int j=0;j<C;j++)
		{
			vec3 lightspace_texcoord	=vec3(pos.xy,float(i)+0.5+float(j)/float(C))/vec3(dims);
			vec3 densityspace_texcoord	=(mul(transformMatrix,vec4(lightspace_texcoord,1.0))).xyz;
			float density				=densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x;
			direct_light				*=exp(-extinctions.x*density*stepLength/float(C));
		}
		//if(density==0)
		//	direct_light=1.0;
	}
}

[numthreads(8,8,1)]
void CS_SecondaryDiffusion(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float direct_light				=1.0;
	targetTexture1[int3(pos.xy,0)]	=1.0;
	const int C=1;
	for(uint i=1;i<dims.z;i++)
	{
	}
}

vec3 randomDirection(float seed)
{
	float x			=rand(vec2(seed,7.71*seed));  
	float y			=rand(vec2(1.89*seed,5.41*seed));
	float theta		=2.0*acos(sqrt(1.0 - x)); 
	float phi		=2.0*pi*y;
	// convert spherical coords to unit vector 
	vec3 vec		=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),cos(theta)); 
	return vec;
}

#define NUM_SAMPLES (17)
#define NUM_STEPS (28)

[numthreads(8,8,8)]
void CS_SecondaryHarmonic(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec3 ratio						=vec3(dims.z/float(dims.x),dims.z/float(dims.y),1.0);
	float indirect_light			=0.0;
	// The indirect light is obtained by integrating a NUM number of directions for direct light.
	for(int i=0;i<NUM_SAMPLES;i++)
	{
		vec3 lightspace_texcoord	=(pos+0.5)/vec3(dims);
		vec3 densityspace_texcoord	=(mul(transformMatrix,vec4(lightspace_texcoord,1.0))).xyz;
		vec3 dir					=randomDirection(0.927*i+7.7*pos.x+11.94*pos.y+21.07*pos.z);

		float opt_depth				=0.0;
		vec3 lightDir				=vec3(0,0,1.0);
		float cos0					=dot(lightDir,dir);
		float beta					=1;//HenyeyGreenstein(.5,cos0);	
		for(int j=0;j<NUM_STEPS;j++)
		{
			float c					=float(j+0.5)/float(NUM_STEPS)/2.0;
			vec3 l					=lightspace_texcoord+ratio*dir*c;
			vec3 d					=mul(transformMatrix,vec4(l,1.0)).xyz;
			d.z						-=0.5/float(dims.z);
			float density			=densityTexture.SampleLevel(wwcSamplerState,d,0).x;
			float direct_light		=2.0*beta*lightTexture1.SampleLevel(wwcSamplerState,l,0).x;

			indirect_light			+=direct_light*density/float(NUM_STEPS);//*exp(-extinctions.x*opt_depth);
			opt_depth				+=density;
		}
	}
	targetTexture1[pos]			=indirect_light/float(NUM_SAMPLES);
}

[numthreads(8,8,1)]
void CS_SecondaryLighting(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos						=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float indirect_light			=0.5;
	if(pos.z>0)
	{
		int Z				=pos.z-1;
		int x1				=(pos.x+2)%dims.x;
		int xn				=(pos.x+dims.x-2)%dims.x;
		int y1				=(pos.y+2)%dims.y;
		int yn				=(pos.y+dims.y-2)%dims.y;
		int3 sample_pts[]	={int3(pos.xy,Z),int3(xn,pos.y,Z),int3(x1,pos.y,Z),int3(pos.x,yn,Z),int3(pos.x,y1,Z)};
		for(int i=0;i<5;i++)
		{
			indirect_light	+=targetTexture1[sample_pts[i]];
		}
		indirect_light		/=5.0;
	}
	int i=pos.z;
	{
		uint3 idx					=uint3(pos.xy,i);
		vec3 lightspace_texcoord	=(vec3(idx)+0.5)/vec3(dims);

		vec3 densityspace_texcoord	=(mul(transformMatrix,vec4(lightspace_texcoord,1.0))).xyz;
		float density				=densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x;
		float direct_light			=lightTexture1.SampleLevel(wwcSamplerState,lightspace_texcoord,0).x;
		indirect_light				*=exp(-extinctions.y*density*stepLength);
		indirect_light				+=.5*direct_light*density;
		if(density==0)
			indirect_light			=0.5;//-(1.0-indirect_light)*exp(-5.0*stepLength);
		targetTexture1[idx]			=indirect_light;
	}
}

[numthreads(8,8,1)]
void CS_GaussianFilter(uint3 sub_pos : SV_DispatchThreadID)
{
	uint3 dims;
	uint3 pos		=sub_pos+threadOffset;
	targetTexture1.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	float light		=lightTexture1[pos].x;
	if(pos.z>0)
	{
		int Z		=pos.z-1;
		light		+=lightTexture1[int3(pos+gaussianOffset)].x;
		light		+=lightTexture1[int3(pos.xy,pos.z+1)].x;
	}
}

float filterLight(Texture3D tex,vec3 texc)
{
	uint3 dims;
	tex.GetDimensions(dims.x,dims.y,dims.z);
	vec3 up			=vec3(0,0,1.0/float(dims.z));
	vec3 forward	=vec3(0,1.0/float(dims.y),0);
	vec3 right		=vec3(1.0/float(dims.x),0,0);
	vec3 offsets[]	={vec3(0,0,0),up,-up,right,-right,forward,-forward};
	float res=0.0;
	for(int i=0;i<7;i++)
		res+=tex.SampleLevel(lightSamplerState,texc+offsets[i],0).x;
	return res/7.0;
}

[numthreads(8,8,8)]
void CS_Transform(uint3 sub_pos	: SV_DispatchThreadID)	//SV_DispatchThreadID gives the combined id in each dimension.
{
	uint3 dims;
	uint3 pos=sub_pos+threadOffset;
	targetTexture.GetDimensions(dims.x,dims.y,dims.z);
	if(pos.x>=dims.x||pos.y>=dims.y||pos.z>=dims.z)
		return;
	vec3 densityspace_texcoord	=(pos.xyz+0.5)/vec3(dims);
	vec3 ambient_texcoord		=vec3(densityspace_texcoord.xy,1.0-zPixel/2.0-densityspace_texcoord.z);
	vec3 lightspace_texcoord	=mul(transformMatrix,vec4(densityspace_texcoord+vec3(0,0,zPixel),1.0)).xyz;
//	lightspace_texcoord.z		-=zPixelLightspace;
	vec2 light_lookup			=vec2(filterLight(lightTexture1,lightspace_texcoord)
		//lightTexture1.SampleLevel(lightSamplerState,lightspace_texcoord,0).x
										,lightTexture2.SampleLevel(lightSamplerState,lightspace_texcoord,0).x);
	vec2 amb_texel				=vec2(ambientTexture1.SampleLevel(wwcSamplerState,ambient_texcoord,0).x
										,ambientTexture2.SampleLevel(wwcSamplerState,ambient_texcoord,0).x);
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(densityTexture.SampleLevel(wwcSamplerState,densityspace_texcoord,0).x);

    vec4 res					=vec4(light_lookup.y,light_lookup.x,density,ambient_lookup);
   // res							=vec4(lightspace_texcoord.zz,density,lightspace_texcoord.z);
	targetTexture[pos]			=res;
}


technique11 density_mask
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState(ReverseDepth, 0 );
		SetBlendState(DontBlend,float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader(CompileShader(vs_4_0,VS_Main()));
        SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0,PS_DensityMask()));
    }
}

technique11 gpu_density_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Density()));
    }
}

technique11 gpu_lighting_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Lighting()));
    }
}

technique11 gpu_secondary_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_SecondaryLighting()));
    }
}

technique11 gpu_secondary_harmonic
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_SecondaryHarmonic()));
    }
}

technique11 gpu_transform_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_Transform()));
    }
}