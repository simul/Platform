// simul_sky.frag - a GLSL fragment shader
// Copyright 2008 Simul Software Ltd

uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseSampler;
uniform sampler2D lossSampler;
uniform sampler2D inscatterSampler;
uniform sampler2D skylightSampler;
uniform sampler3D illumSampler;

uniform vec3 ambientColour;
uniform vec3 fractalScale;
uniform vec4 lightResponse;
uniform float cloud_interp;
uniform vec3 lightDir;

// the App updates uniforms "slowly" (eg once per frame) for animation.
uniform float hazeEccentricity;
uniform float cloudEccentricity;
uniform float fadeInterp;
uniform vec3 mieRayleighRatio;
uniform vec4 lightningMultipliers;
uniform vec4 lightningColour;


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
#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	float u=1.0+g2-2.0*g*cos0;
	return 0.5*0.079577+0.5*(1.0-g2)*pow(u,-1.5)/(4.0*pi);
	//return 0.5*0.079577+.5*(1.0-g2)/(4.0*pi*sqrt(u*u*u));
}

vec3 InscatterFunction(vec4 inscatter_factor,float cos0)
{
	float BetaRayleigh=0.0596831*(1.0+cos0*cos0);
	float BetaMie=HenyeyGreenstein(hazeEccentricity,cos0);		// Mie's phase function
	vec3 BetaTotal=(BetaRayleigh+BetaMie*inscatter_factor.a*mieRayleighRatio.xyz)
		/(vec3(1.0,1.0,1.0)+inscatter_factor.a*mieRayleighRatio.xyz);
	vec3 colour=BetaTotal*inscatter_factor.rgb;
	return colour;
}

void main(void)
{
	vec3 noise_offset=vec3(0.49803921568627452,0.49803921568627452,0.49803921568627452);
	float cos0=dot(lightDir.xyz,normalize(view.xyz));
	vec3 noiseval=texture2D(noiseSampler,noiseCoord).xyz-noise_offset;
#if DETAIL_NOISE==1
	noiseval+=(texture2D(noiseSampler,noiseCoord*8.0).xyz-0.5)/2.0;
#endif
	noiseval*=texCoordDiffuse.w;
	// Would use InscatterFunction if we passed in an isotropic inscatter factors. But otherwise the angular dependence is
	// built into the vertex value.
	//vec3 inscatter=InscatterFunction(insc,cos0);
	vec3 pos=texCoordDiffuse.xyz+fractalScale*texCoordDiffuse.w*noiseval;
	vec4 density=texture3D(cloudDensity1,pos);
	vec4 density2=texture3D(cloudDensity2,pos);
	//vec4 lightning=texture3D(illumSampler,texCoordLightning.xyz);
	density=mix(density,density2,cloud_interp);
	if(density.y<=0.0)
		discard;
	float Beta=lightResponse.x*HenyeyGreenstein(cloudEccentricity*density.z,cos0);
	float opacity=layerDensity*density.y;
	vec3 final=(density.z*Beta+lightResponse.y*density.w)*sunlight+density.x*ambientColour.rgb;
	vec3 loss_lookup=texture2D(lossSampler,fade_texc).rgb;
	vec4 insc_lookup=texture2D(inscatterSampler,fade_texc);
	vec3 skyl_lookup=texture2D(skylightSampler,fade_texc);
	//final.rgb+=lightning.rgb;
	//final.rgb*=texture2D(lossSampler,fade_texc).rgb;
	//final.rgb+=0.05*texture2D(inscatterSampler,fade_texc).rgb;
	final.rgb*=loss_lookup;
	final.rgb+=InscatterFunction(insc_lookup,cos0);
	final.rgb+=skyl_lookup;
    gl_FragColor=vec4(final.rgb*opacity,1.0-opacity);
}
