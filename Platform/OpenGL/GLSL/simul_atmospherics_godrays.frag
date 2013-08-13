#version 140
#include "CppGlsl.hs"
#include "saturate.glsl"
#include "../../CrossPlatform/atmospherics_constants.sl"
#include "view_dir.glsl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
// Godrays are cloud-dependent. So we require the cloud texture.
uniform sampler2D cloudShadowTexture;
#include "../../CrossPlatform/cloud_shadow.sl"
#include "../../CrossPlatform/godrays.sl"
uniform sampler2D imageTexture;
uniform sampler2D lossTexture;
uniform sampler2D inscTexture;

in vec2 texCoords;

void main()
{
	vec3 view=texCoordToViewDirection(texCoords);
	float sine=view.z;
    vec4 lookup=texture(imageTexture,texCoords);
	float depth=lookup.a;
	vec2 texc2=vec2(pow(depth,0.5),0.5*(1.0-sine));
	vec4 insc=texture(inscTexture,texc2);
	float cos0=dot(view,lightDir);
	// trace the distance from 1.0 to zero
	// insc is the cumulative inscatter from 0 to dist.
	// between two distances, the inscatter (AS SEEN FROM 0) is:
	// insc2-insc1
	vec4 total_insc=vec4(0,0,0,0);
	float illumination=GetIlluminationAt(cloudShadowTexture,view*depth);
	#define NUM_STEPS 128
	float retain=(float(NUM_STEPS)-1.0)/float(NUM_STEPS);
	for(int i=0;i<NUM_STEPS+1;i++)
	{
		float u=(float(NUM_STEPS)-float(i))/float(NUM_STEPS);
		float eff=exp(-u/10.0);
		if(u<depth)
		{
			texc2.x=u;
			float prev_illumination=illumination;
			float d=u*u*maxDistance;
			illumination=mix(0.0,GetIlluminationAt(cloudShadowTexture,view*d),eff);
			vec4 prev_insc=insc;
		insc=texture(inscTexture,texc2);
			vec4 insc_diff=prev_insc-insc;
			float ill=prev_illumination;//0.5*(illumination+prev_illumination);
			total_insc.rgb+=insc_diff.rgb*ill;
			total_insc.a*=retain;
			total_insc.a+=insc_diff.a*ill;
		}
	}
//	total_insc.a=insc.a;
	vec3 gr=-InscatterFunction(total_insc,hazeEccentricity,cos0,mieRayleighRatio).rgb;
	gr=min(gr,vec3(0.0,0.0,0.0));
	
    gl_FragColor=vec4(gr,0.0);
}
