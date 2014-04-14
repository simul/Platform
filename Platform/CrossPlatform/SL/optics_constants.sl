#ifndef OPTICS_CONSTANTS_SL
#define OPTICS_CONSTANTS_SL

uniform_buffer OpticsConstants SIMUL_BUFFER_REGISTER(10)
{
	uniform mat4 worldViewProj;
	uniform vec4 colour;
	uniform mat4 view;			// Necessary??
	uniform mat4 invProj;		// Necessary??
	uniform mat4 invViewProj;	// or use this instead?

	uniform vec3 lightDir;
	uniform float radiusRadians;

	uniform vec3 depthToLinFadeDistParams;
	uniform float dropletRadius;

	uniform float rainbowIntensity;
	uniform float ahgaad,ahgage,aejhue;
};

#endif