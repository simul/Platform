//  Copyright (c) 2016 Simul Software Ltd. All rights reserved.
#ifndef CAMERA_CONSTANTS_SL
#define CAMERA_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(CameraConstants,1)
	uniform mat4 worldViewProj;
	uniform mat4 world;
	uniform mat4 invWorldViewProj;
	uniform mat4 modelView;
	uniform mat4 view;
	uniform mat4 proj;
	uniform mat4 viewProj;
	uniform vec3 viewPosition;
	uniform int frameNumber;
SIMUL_CONSTANT_BUFFER_END

SIMUL_CONSTANT_BUFFER(StereoCameraConstants,2)
	uniform mat4 leftWorldViewProj;
	uniform mat4 leftWorld;
	uniform mat4 leftInvWorldViewProj;
	uniform mat4 leftModelView;
	uniform mat4 leftView;
	uniform mat4 leftProj;
	uniform mat4 leftViewProj;

	uniform mat4 rightWorldViewProj;
	uniform mat4 rightWorld;
	uniform mat4 rightInvWorldViewProj;
	uniform mat4 rightModelView;
	uniform mat4 rightView;
	uniform mat4 rightProj;
	uniform mat4 rightViewProj;

	uniform vec3 stereoViewPosition;
SIMUL_CONSTANT_BUFFER_END

#ifndef __cplusplus

//For Mono Camera

vec3 ClipPosToView(vec4 clip_pos)
{
	vec3 view = normalize(mul(invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 ClipPosToView(vec2 clip_pos2)
{
	vec4 clip_pos = vec4(clip_pos2, 1.0, 1.0);
	vec3 view = normalize(mul(invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 TexCoordsToView(vec2 texCoords)
{
	vec4 clip_pos = vec4(-1.0, 1.0, 1.0, 1.0);
	clip_pos.x += 2.0*texCoords.x;
	clip_pos.y -= 2.0*texCoords.y;
	return ClipPosToView(clip_pos);
}

//For Stereo Camera

vec3 ClipPosToView(vec4 clip_pos, uint viewID)
{
	mat4 _invWorldViewProj = viewID == 0 ? leftInvWorldViewProj : rightInvWorldViewProj;
	vec3 view = normalize(mul(_invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 ClipPosToView(vec2 clip_pos2, uint viewID)
{
	mat4 _invWorldViewProj = viewID == 0 ? leftInvWorldViewProj : rightInvWorldViewProj;
	vec4 clip_pos = vec4(clip_pos2, 1.0, 1.0);
	vec3 view = normalize(mul(_invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 TexCoordsToView(vec2 texCoords, uint viewID)
{
	vec4 clip_pos = vec4(-1.0, 1.0, 1.0, 1.0);
	clip_pos.x += 2.0 * texCoords.x;
	clip_pos.y -= 2.0 * texCoords.y;
	return ClipPosToView(clip_pos, viewID);
}
#endif
#endif
