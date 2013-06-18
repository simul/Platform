#ifndef SIMUL_VIEWDIR_GLSL
#define SIMUL_VIEWDIR_GLSL

vec3 texCoordToViewDirection(vec2 texCoords)
{
	vec4 pos=vec4(-1.0,-1.0,1.0,1.0);
	pos.x+=2.0*texCoords.x;//+texelOffsets.x;
	pos.y+=2.0*texCoords.y;//+texelOffsets.y;
	vec3 view=(invViewProj*pos).xyz;
	view=normalize(view);
	return view;
}

#endif