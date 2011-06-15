// simul_sky.frag - a GLSL fragment shader
// Copyright 2008 Simul Software Ltd

uniform sampler3D cloudDensity1;
uniform sampler3D cloudDensity2;
uniform sampler2D noiseSampler;
uniform sampler3D illumSampler;

uniform vec3 ambientColour;

uniform vec3 fractalScale;
uniform vec3 lightDir;
uniform vec4 lightResponse;
uniform float cloud_interp;

// the App updates uniforms "slowly" (eg once per frame) for animation.
uniform float hazeEccentricity;
uniform float cloudEccentricity;
uniform vec3 eyePosition;
uniform float fadeInterp;
uniform vec3 mieRayleighRatio;

// varyings are written by vert shader, interpolated, and read by frag shader.
varying vec2 noiseCoord;
varying float layerDensity;
varying vec4 texCoordDiffuse;
varying vec3 eyespacePosition;
varying vec3 wPosition;
varying vec3 sunlight;

varying vec3 loss;
varying vec4 insc;
uniform float altitudeTexCoord;

uniform vec4 lightningMultipliers;
uniform vec4 lightningColour;

varying vec3 texCoordLightning;

#define pi (3.1415926536)
float HenyeyGreenstein(float g,float cos0)
{
	float g2=g*g;
	return .5*0.079577+.5*(1.0-g2)/(4.0*pi*pow(1.0+g2-2.0*g*cos0,1.5));
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
	vec3 noiseval=texture2D(noiseSampler,noiseCoord).xyz-0.5;

	noiseval+=(texture2D(noiseSampler,noiseCoord*8.0).xyz-0.5)/2.0;
	noiseval*=texCoordDiffuse.w;

	vec3 view=normalize(wPosition);
	vec3 fade_texc;
	fade_texc.x=length(wPosition)/30000.0;
	fade_texc.y=0.5*(1.0-view.z);
	fade_texc.z=altitudeTexCoord;
   
	float cos0=dot(lightDir.xyz,view.xyz);
	// Would use InscatterFunction if we passed in an isotropic inscatter factors. But otherwise the angular dependence is
	// built into the vertex value.
	//vec3 inscatter=InscatterFunction(insc,cos0);
	vec3 pos=texCoordDiffuse.xyz+fractalScale*texCoordDiffuse.w*noiseval;

	vec4 density=texture3D(cloudDensity1,pos);

	float Beta=HenyeyGreenstein(cloudEccentricity,cos0);
	vec4 density2=texture3D(cloudDensity2,pos);

	vec4 lightning=texture3D(illumSampler,texCoordLightning.xyz);

	density=mix(density,density2,cloud_interp);

	float opacity=layerDensity*density.x;

	vec3 final=(lightResponse.y*density.y*Beta+lightResponse.z*density.z)*sunlight+density.w*ambientColour.rgb;
	final.rgb+=lightning.rgb;
	final.rgb*=loss;
	final.rgb+=insc.rgb;
    gl_FragColor=vec4(final.rgb,opacity);
}
