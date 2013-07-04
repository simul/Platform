// simul_clouds.frag - a GLSL fragment shader
// Copyright 2008-2013 Simul Software Ltd
#version 140

#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "CppGlsl.hs"
#include "../../CrossPlatform/simul_cloud_constants.sl"
#include "../../CrossPlatform/depth.sl"
#include "saturate.glsl"

uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseSampler;
uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;
uniform sampler3D illumSampler;
uniform sampler2D depthTexture;

// varyings are written by vert shader, interpolated, and read by frag shader.
in float layerDensity;
in float rainFade;
in vec4 texCoordDiffuse;
in vec2 noiseCoord;
in vec3 wPosition;
in vec3 texCoordLightning;
in vec2 fade_texc;
in vec3 view;
in vec4 transformed_pos;

void main(void)
{
	vec3 half_vec		=vec3(0.5,0.5,0.5);//0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float cos0			=dot(lightDir.xyz,normalize(view.xyz));
#ifdef USE_DEPTH_TEXTURE
	vec2 clip_pos		=transformed_pos.xy/transformed_pos.w;
	vec2 screenCoord	=screenCoordOffset+0.5*(clip_pos.xy)+vec2(0.5,0.5);
	float depth			=texture(depthTexture,screenCoord).x;
	float dist			=depthToDistance(depth,clip_pos.xy,nearZ,farZ,tanHalfFov);
	float cloud_dist	=pow(fade_texc.x,2.0);
#endif
	vec4 texc=texCoordDiffuse;
#ifdef TILING_OFFSET
	vec2 tiling_offset=texture(noiseSampler,texc.xy/64.0).xy;
	texc.xy+=2.0*(tiling_offset.xy-noise_offset.xy);
#endif
	vec3 noiseval=2.0*(textureLod(noiseSampler,noiseCoord.xy,0).xyz-half_vec);
#ifdef DETAIL_NOISE
//	noiseval+=(textureLod(noiseSampler,noiseCoord*8.0,0).xyz-0.5)/2.0;
#endif
	noiseval*=texc.w;
	vec3 pos=texc.xyz+fractalScale*noiseval;
	vec4 density=textureLod(cloudDensity1,pos,0);
	vec4 density2=textureLod(cloudDensity2,pos,0);
	//vec4 lightning=texture(illumSampler,texCoordLightning.xyz);
	density=mix(density,density2,cloud_interp);
	float opacity=layerDensity*density.y;
	//opacity+=rain*rainFade*saturate((0.25-pos.z)*50.0)*(1.0-density.x);

#ifdef USE_DEPTH_TEXTURE
	//float depth_offset=dist-cloud_dist;
	//opacity*=saturate(depth_offset/0.01);
	if(opacity<=0.0)
		discard;
	if(dist<cloud_dist)
		discard;
#else
	if(opacity<=0.0)
		discard;
#endif
   // gl_FragColor=vec4(0,0,0,1.0-opacity);
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
	vec3 sunlightColour=mix(sunlightColour1,sunlightColour2,saturate(texCoordDiffuse.z));
	vec3 final=(density.z*Beta+lightResponse.y*density.w)*sunlightColour*earthshadowMultiplier+density.x*ambientColour.rgb;
	
	vec3 diff=wPosition.xyz-lightningSourcePos;
	float dist_from_lightning=length(diff.xyz);
	float cc=dist_from_lightning/2000.f;
	float pwr=exp(-cc*cc);
	final.rgb+=lightningColour.rgb*pwr;

	vec3 loss_lookup=texture(lossSampler,fade_texc).rgb;
	vec4 insc_lookup=earthshadowMultiplier*texture(inscatterSampler,fade_texc);
	vec3 skyl_lookup=texture(skylightSampler,fade_texc).rgb;

	final.rgb*=loss_lookup;
	final.rgb+=InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final.rgb+=skyl_lookup;
//final.rgb=fract(noiseCoord.xyy);
    gl_FragColor=vec4(final.rgb*opacity,1.0-opacity);
}
