
mat4 cubeInvViewProj[6] = {
	 { {0,0,1,0}	,{0,-1,0,0}	,{-1,0,0,0}	,{0,0,0,1} }
	,{ {0,0,-1,0}	,{0,-1,0,0}	,{1,0,0,0}	,{0,0,0,1}	}
	,{ {-1,0,0,0}	,{0,0,1,0}	,{0,-1,0,0},{0,0,0,1}	}
	,{ {-1,0,0,0}	,{0,0,-1,0}	,{0,1,0,0}	,{0,0,0,1}	}
	,{ {-1,0,0,0}	,{0,-1,0,0}	,{0,0,-1,0}	,{0,0,0,1}	}
	,{ {1,0,0,0}	,{0,-1,0,0}	,{0,0,1,0}	,{0,0,0,1}	}
};
/*0 0 1 0
0 -1 0 0
0 0 0 1
-1 0 0 0

0 0 -1 0
0 -1 0 0
0 0 0 1
1 0 0 0

-1 0 0 0
0 0 1 0
0 0 0 1
0 -1 0 0

-1 0 0 0
0 0 -1 0
0 0 0 1
0 1 0 0

-1 0 0 0
0 -1 0 0
0 0 0 1
0 0 -1 0

1 0 0 0
0 -1 0 0
0 0 0 1
0 0 1 0
*/
#ifndef __cplusplus
vec3 CubeClipPosToView(int cube_face,vec4 clip_pos)
{
	vec3 view = -normalize(mul(cubeInvViewProj[cube_face], clip_pos).xyz);
	return view;
}

vec3 CubeClipPosToView(int cube_face, vec2 clip_pos2)
{
	vec4 clip_pos = vec4(clip_pos2, 1.0, 1.0);
	vec3 view = -normalize(mul(cubeInvViewProj[cube_face], clip_pos).xyz);
	return view;
}

vec3 CubeTexCoordsToView(int cube_face,vec2 texCoords)
{
	vec4 clip_pos = vec4(-1.0, 1.0, 1.0, 1.0);
	clip_pos.x += 2.0*texCoords.x;
	clip_pos.y -= 2.0*texCoords.y;
	return ClipPosToView(cube_face,clip_pos);
}

vec3 CubeFaceIndexToView(uint3 idx,uint2 dims)
{
	vec2 texCoords			=(vec2(idx.xy)+vec2(0.5,0.5))/vec2(dims);
	vec4 clip_pos			=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	vec3 view				=-normalize(mul(cubeInvViewProj[idx.z],clip_pos).xyz);
	return view;
}
#endif