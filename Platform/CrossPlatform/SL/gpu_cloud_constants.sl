//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL
#define SIMUL_CROSSPLATFORM_GPU_CLOUD_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(GpuCloudConstants,8)
	uniform mat4 transformMatrix;
	uniform vec4 yRange;
	uniform vec3 noiseScale;
	uniform float noiseDimsZ;
	uniform vec2 extinctions;
	uniform float stepLength;
	uniform float cloudBaseKm;		
	uniform uint3 gaussianOffset;
	uniform int octaves;
	uniform uint3 threadOffset;
	uniform float zPosition;

	uniform float time;
	uniform float persistence;
	uniform float humidity;
	uniform float zPixelLightspace;

	uniform float zPixel;
	uniform float zSize;
	uniform float baseLayer;
	uniform float transition;

	uniform float upperDensity;
	uniform float diffusivity;
	uniform float invFractalSum;
	uniform float maskThickness;

	uniform vec2 maskCentre;
	uniform float maskRadius;
	uniform float maskFeather;
	
	uniform vec3 cloudScalesKm;
	uniform float time_step;

	uniform vec3 lightDir;
	uniform float alpha;		// simulation constant;

	uniform float invBeta;
	uniform int wrap;
SIMUL_CONSTANT_BUFFER_END

#endif
