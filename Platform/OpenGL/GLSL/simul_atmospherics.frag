#version 140
#include "simul_inscatter_fns.glsl"
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;
uniform sampler2D skylightTexture;
uniform float maxDistance;
uniform vec3 viewPosition;

// Godrays are cloud-dependent. So we require the cloud texture.
uniform sampler2D cloudShadowTexture;
// X, Y and Z for the bottom-left corner of the cloud shadow texture.
uniform vec3 cloudOrigin;
uniform vec3 cloudScale;

uniform vec3 lightDir;
uniform mat4 invViewProj;
varying vec2 texCoords;

vec3 makeViewVector()
{
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(invViewProj*pos).xyz;
	view=normalize(view);
	return view;
}

vec4 simple()
{
	vec3 view=makeViewVector();
	float sine=view.z;
    vec4 lookup=texture(imageTexture,texCoords);
	float depth=lookup.a;
	if(depth>=1.0) 
		discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc2).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
	vec4 insc=texture(inscTexture,texc2);
	float cos0=dot(view,lightDir);
	colour+=InscatterFunction(insc,cos0);
	vec3 skyl=texture(skylightTexture,texc2).rgb;
	colour+=skyl;
    return vec4(colour,1.0);
}

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
	
vec4 godrays()
{
	vec3 view=makeViewVector();
	float sine=view.z;
    vec4 lookup=texture(imageTexture,texCoords);
	float depth=lookup.a;
	if(depth>=1.0) 
		depth=1.0;
	//if(depth>=1.0) 
	//	discard;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec3 loss=texture(lossTexture,texc2).rgb;
	vec3 colour=lookup.rgb;
	colour*=loss;
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
	colour+=InscatterFunction(total_insc,cos0);
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
