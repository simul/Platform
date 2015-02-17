#ifndef OPTICS_CONSTANTS_SL
#define OPTICS_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(OpticsConstants,10)
	uniform mat4 worldViewProj;
	uniform vec4 colour;
	uniform mat4 view;			// Necessary??
	uniform mat4 invProj;		// Necessary??
	uniform mat4 invViewProj;	// or use this instead?

	uniform vec3 lightDir;
	uniform float radiusRadians;

	uniform vec4 depthToLinFadeDistParams;

	uniform float rainbowIntensity;
	uniform float dropletRadius;
	uniform float ahgage;
	uniform float aejhue;
SIMUL_CONSTANT_BUFFER_END

#endif