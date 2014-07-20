#version 330

uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylTexture; 
uniform sampler2D depthTexture;
uniform sampler3D noiseTexture3D;
uniform sampler3D lightningIlluminationTexture;

#include "../../CrossPlatform/SL/simul_inscatter_fns.sl"
#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/simul_cloud_constants.sl"
#include "../../CrossPlatform/SL/simul_clouds.sl"
#include "../../CrossPlatform/SL/depth.sl"
#include "saturate.glsl"

in vec2 texc;

vec4 calcDensity2(vec3 texCoords,float layerFade,vec3 noiseval)
{
	vec3 pos=texCoords.xyz;//+fractalScale.xyz*noiseval;
	vec4 density=texture(cloudDensity1,pos);
	return density;
}

void main()
{
	vec4 colour=vec4(0.0,0.0,0.0,1.0);
#if 0
	vec2 texCoords=texc.xy;
	texCoords.y=1.0-texc.y;
	vec4 dlookup=sampleLod(depthTexture,clampSamplerState,texCoords.xy,0);
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texc.x;
	pos.y+=2.0*texc.y;
	vec3 view=normalize((invViewProj*pos).xyz);
	vec3 viewPos=vec3(wrld[3][0],wrld[3][1],wrld[3][2]);
	float cos0=dot(lightDir.xyz,view.xyz);
	float sine=view.z;
	vec3 noise_pos	=(noiseMatrix*vec4(view.xyz,1.0)).xyz;
	vec2 noise_texc_0	=vec2(atan2(noise_pos.x,noise_pos.z),atan2(noise_pos.y,noise_pos.z));

	float min_texc_z=-fractalScale.z*1.5;
	float max_texc_z=1.0-min_texc_z;

	float depth=dlookup.r;
	float d=1.0;//depthToDistance(depth,pos.xy,nearZ,farZ,tanHalfFov);
	//[unroll(12)]
	for(int i=0;i<layerCount;i++)
	{
		//LayerData layer=layers[i];
		float dist=1000.0*(layerCount-i);//layer.layerDistance;
		float z=dist/300000.0;
		//if(z>d)
		//	continue;
		vec3 pos=viewPos+dist*view;
		//pos.z-=layer.verticalShift;
		vec3 texCoords=(pos-cornerPos)*inverseScales;
		//if(texCoords.z<min_texc_z||texCoords.z>max_texc_z)
		//	continue;
		/*vec2 noise_texc		=noise_texc_0*layer.noiseScale+layer.noiseOffset;
		vec3 noiseval=(noiseTexture.SampleLevel(wrapSamplerState,noise_texc.xy,3).xyz).xyz;
#ifdef DETAIL_NOISE
		//noiseval+=(noiseTexture.SampleLevel(noiseSamplerState,8.0*noise_texc.xy,0).xyz)/2.0;
#endif*/
		vec3 noiseval=vec3(0,0,0);
		vec4 density=calcDensity2(texCoords,1.0,noiseval);
		//if(density.z<=0)
		//	continue;
		vec4 c=calcColour(density,cos0,texCoords.z);
		vec2 fade_texc=vec2(sqrt(z),0.5*(1.0-sine));
		c.rgb=applyFades(c.rgb,fade_texc,cos0,earthshadowMultiplier);
		colour*=(1.0-c.a);
		colour.rgb+=c.rgb*c.a;
	}
	if(colour.a>=1.0)
		discard;
#endif
    gl_FragColor=vec4(colour.rgb,1.0-colour.a);
}
