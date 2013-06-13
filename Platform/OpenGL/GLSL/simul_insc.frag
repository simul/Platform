#version 140
#include "saturate.glsl"
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;
#include "../../CrossPlatform/simul_inscatter_fns.sl"
uniform sampler2D imageTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform float maxDistance;

// Godrays are cloud-dependent. So we require the cloud texture.
uniform sampler2D cloudShadowTexture;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
uniform vec3 cloudOrigin;
uniform vec3 cloudScale;
uniform vec3 viewPosition;
uniform float overcast;

uniform vec3 lightDir;

varying vec2 texCoords;

#include "view_dir.glsl"

float GetIlluminationAt(vec3 vd)
{
	vec3 pos=vd+viewPosition;
	vec3 cloud_texc=(pos-cloudOrigin)*cloudScale;
	vec4 cloud_texel=texture(cloudShadowTexture,cloud_texc.xy);
	float illumination=cloud_texel.z;
	float above=saturate(cloud_texc.z);
	illumination=saturate(illumination-above);
	return illumination;
}

vec4 simple()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texCoords);
	float depth=lookup.x;
	vec2 texc=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec4 insc=texture(inscTexture,texc);
	float cos0=dot(view,lightDir);
	vec3 colour=InscatterFunction(insc,hazeEccentricity,cos0,mieRayleighRatio);
	vec3 skyl=texture(skylightTexture,texc).rgb;
	colour+=skyl;
    return vec4(colour,1.0);
}
	
vec4 godrays()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(depthTexture,texCoords);
	float depth=lookup.x;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec4 insc=texture(inscTexture,texc2);
	vec3 skyl=texture(skylightTexture,texc2).rgb;
	// trace the distance from 1.0 to zero
	
	// insc is the cumulative inscatter from 0 to dist.
	// between two distances, the inscatter (AS SEEN FROM 0) is:
	// insc2-insc1
	vec4 total_insc=vec4(0,0,0,0);
	float illumination=GetIlluminationAt(view*depth);
	#define C 48
	float retain=(float(C)-1.0)/float(C);
	for(int i=0;i<C+1;i++)
	{
		float u=(float(C)-float(i))/float(C);
		if(u<depth)
		{
			texc2.x=u;
			float prev_illumination=illumination;
			float d=u*u*maxDistance;
			illumination=GetIlluminationAt(view*d);
			vec4 prev_insc=insc;
		insc=texture(inscTexture,texc2);
			vec4 insc_diff=prev_insc-insc;
			float ill=prev_illumination;//0.5*(illumination+prev_illumination);
			total_insc.rgb+=insc_diff.rgb*ill;
			total_insc.a*=retain;
			total_insc.a+=insc_diff.a*ill;
		}
	}	
	float cos0=dot(view,lightDir);
	vec3 colour=InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio);
	colour+=skyl;
    return vec4(colour,1.0);
}

void main()
{
#ifdef GODRAYS
	gl_FragColor=godrays();
#else
	gl_FragColor=simple();
#endif
}
