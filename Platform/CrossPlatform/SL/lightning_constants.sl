//  Copyright (c) 2015-2018 Simul Software Ltd. All rights reserved.
#ifndef LIGHTNING_CONSTANTS_SL
#define LIGHTNING_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(LightningConstants,10)
	uniform vec3    lightningColour;
	uniform float   branchAngleRadians;

	uniform vec3    startPos;
	uniform uint    num_octaves;

	uniform vec3    endPos;
	uniform float   strikeThicknessMetres;

	uniform float   roughness;
	uniform float   motion;
    //! How many times should we branch
	uniform uint    numLevels;
    //! This is the number of branches requested by the lighting props
	uniform uint    numBranches;
    //! pow(numBranches,curLevel - 1) = this is the number of branches
    //! generated in the previous level
    uniform uint    numBranchesPrevious;

	uniform float   branchLengthMetres;
	uniform uint    branchInterval;
	uniform float   phaseTime;
	uniform int     randomSeed;

	uniform float   brightness;
	uniform float   progress;
	uniform float   vertical;
	uniform uint    branchIndex;
    uniform uint    curLevel;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(LightningPerViewConstants,8)
	uniform mat4 worldViewProj;
	uniform vec4 depthToLinFadeDistParams;
	uniform vec4 viewportToTexRegionScaleBias;
	uniform vec4 fullResToLowResTransformXYWH;
	uniform vec2 viewportPixels;
	uniform vec2 _line_width;

	uniform vec2 tanHalfFovUnused;
	uniform float brightnessToUnity;
	uniform float minPixelWidth;
	uniform vec4 tanHalfFov;
	uniform vec3 viewPosition;
	uniform float maxFadeDistance;
	uniform uint parent_point;
SIMUL_CONSTANT_BUFFER_END

struct LightningVertex
{
    vec3 position;
	vec2 texCoords;		// x= width in pixels
	float progress;
};

#endif