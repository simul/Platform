// simul_sky.frag - a GLSL fragment shader
// Copyright 2008 Simul Software Ltd
#include "simul_inscatter_fns.glsl"
#include "simul_inscatter_fns.glsl"


uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseSampler;
uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;
uniform sampler3D illumSampler;
uniform sampler2D depthAlphaTexture;

uniform vec3 ambientColour;
uniform vec3 fractalScale;
uniform vec4 lightResponse;
uniform float cloud_interp;
uniform vec3 lightDir;

uniform float cloudEccentricity;
uniform float fadeInterp;
uniform vec4 lightningMultipliers;
uniform vec4 lightningColour;
uniform float earthshadowMultiplier;
uniform vec2 screenCoordOffset;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 noiseCoord;
varying float layerDensity;
varying vec4 texCoordDiffuse;
varying vec3 eyespacePosition;
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
	vec3 noise_offset=vec3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float cos0=dot(lightDir.xyz,normalize(view.xyz));
	vec2 screenCoord			=screenCoordOffset+0.5*(transformed_pos.xy/transformed_pos.w)+vec2(0.5,0.5);
	float depth=texture(depthAlphaTexture,screenCoord).a;
	float cloud_depth=pow(fade_texc.x,2.0);
	
	vec3 noiseval=texture2D(noiseSampler,noiseCoord).xyz-noise_offset;
#if DETAIL_NOISE==1
	noiseval+=(texture2D(noiseSampler,noiseCoord*8.0).xyz-0.5)/2.0;
#endif
	noiseval*=texCoordDiffuse.w;
	vec3 pos=texCoordDiffuse.xyz+fractalScale*texCoordDiffuse.w*noiseval;
	vec4 density=texture3D(cloudDensity1,pos);
	vec4 density2=texture3D(cloudDensity2,pos);
	//vec4 lightning=texture3D(illumSampler,texCoordLightning.xyz);
	density=mix(density,density2,cloud_interp);
	float opacity=layerDensity*density.y;
	if(opacity<=0.0)
		discard;
	float depth_offset=depth-cloud_depth;
	opacity*=saturate(depth_offset/0.01);
	if(depth>0&&depth<cloud_depth)
		discard;
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
	vec3 final=(density.z*Beta+lightResponse.y*density.w)*sunlight+density.x*ambientColour.rgb;
	vec3 loss_lookup=texture2D(lossSampler,fade_texc).rgb;
	vec4 insc_lookup=earthshadowMultiplier*texture2D(inscatterSampler,fade_texc);
	vec3 skyl_lookup=texture2D(skylightSampler,fade_texc).rgb;

	final.rgb*=loss_lookup;
	final.rgb+=InscatterFunction(insc_lookup,cos0);
	final.rgb+=skyl_lookup;
    gl_FragColor=vec4(final.rgb*opacity,1.0-opacity);
}
