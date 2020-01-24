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
SIMUL_CONSTANT_BUFFER_END

#ifndef __cplusplus
vec3 ClipPosToView(vec4 clip_pos)
{
	vec3 view = -normalize(mul(invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 ClipPosToView(vec2 clip_pos2)
{
	vec4 clip_pos = vec4(clip_pos2, 1.0, 1.0);
	vec3 view = -normalize(mul(invWorldViewProj, clip_pos).xyz);
	return view;
}

vec3 TexCoordsToView(vec2 texCoords)
{
	vec4 clip_pos = vec4(-1.0, 1.0, 1.0, 1.0);
	clip_pos.x += 2.0*texCoords.x;
	clip_pos.y -= 2.0*texCoords.y;
	return ClipPosToView(clip_pos);
}
#endif
#endif
