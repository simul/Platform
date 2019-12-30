//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SOLID_CONSTANTS_SL
#define SOLID_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(SolidConstants,13)
	vec3 albedo;
	float roughness;
	vec4 depthToLinFadeDistParams;
	vec4 fullResToLowResTransformXYWH;
	vec3 lightIrradiance;
	float metal;
	vec3 lightDir;
	int reverseDepth;
SIMUL_CONSTANT_BUFFER_END

#endif
