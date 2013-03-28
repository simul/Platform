// simul_clouds.frag - a GLSL fragment shader
// Copyright 2008-2013 Simul Software Ltd
#version 140
#include "simul_inscatter_fns.glsl"
#include "CloudConstants.glsl"

uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseSampler;
uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;
uniform sampler3D illumSampler;
uniform sampler2D depthAlphaTexture;
uniform float hazeEccentricity;
uniform vec3 mieRayleighRatio;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 noiseCoord;
varying float layerDensity;
varying vec4 texCoordDiffuse;
varying vec3 wPosition;
varying vec3 sunlight;

varying vec3 loss;
varying vec4 insc;
varying vec3 texCoordLightning;
varying vec2 fade_texc;
varying vec3 view;
varying vec4 transformed_pos;

void main(void)
{
	vec3 noise_offset	=vec3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float cos0			=dot(lightDir.xyz,normalize(view.xyz));
#ifdef USE_DEPTH_TEXTURE
	vec2 screenCoord	=screenCoordOffset+0.5*(transformed_pos.xy/transformed_pos.w)+vec2(0.5,0.5);
	float depth			=texture(depthAlphaTexture,screenCoord).a;
	float cloud_depth	=pow(fade_texc.x,2.0);
#endif
	vec4 texc=texCoordDiffuse;
#ifdef TILING_OFFSET
	vec2 tiling_offset=texture(noiseSampler,texc.xy/64.0).xy;
	texc.xy+=2.0*(tiling_offset.xy-noise_offset.xy);
#endif
	vec3 noiseval=texture(noiseSampler,noiseCoord).xyz-noise_offset;
#if DETAIL_NOISE==1
	noiseval+=(texture(noiseSampler,noiseCoord*8.0).xyz-0.5)/2.0;
#endif
	noiseval*=texc.w;
	vec3 pos=texc.xyz+fractalScale*texc.w*noiseval;
	vec4 density=texture(cloudDensity1,pos);
	vec4 density2=texture(cloudDensity2,pos);
	//vec4 lightning=texture(illumSampler,texCoordLightning.xyz);
	density=mix(density,density2,cloud_interp);
	float opacity=layerDensity*density.y;
	if(opacity<=0.0)
		discard;
#ifdef USE_DEPTH_TEXTURE
	float depth_offset=depth-cloud_depth;
	opacity*=saturate(depth_offset/0.01);
	if(depth>0&&depth<cloud_depth||depth>=1.0)
		discard;
#endif
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
	vec3 final=(density.z*Beta+lightResponse.y*density.w)*sunlight*earthshadowMultiplier+density.x*ambientColour.rgb;
	
	vec3 diff=wPosition.xyz-lightningSourcePos;
	float dist_from_lightning=length(diff.xyz);
	float cc=dist_from_lightning/2000.f;
	float pwr=exp(-cc*cc);
	final.rgb+=lightningColour*pwr;

	vec3 loss_lookup=texture(lossSampler,fade_texc).rgb;
	vec4 insc_lookup=earthshadowMultiplier*texture(inscatterSampler,fade_texc);
	vec3 skyl_lookup=texture(skylightSampler,fade_texc).rgb;

	final.rgb*=loss_lookup;
	final.rgb+=InscatterFunction(insc_lookup,hazeEccentricity,cos0,mieRayleighRatio);
	final.rgb+=skyl_lookup;
    gl_FragColor=vec4(final.rgb*opacity,1.0-opacity);
}
