/*
mat4 cubeInvViewProj[6] = {
	{ {0,0,-1,0}	,{0,-1,0,0}	,{1,0,0,0}	,{0,0,0,1}	}
	, { {0,0,1,0}	,{0,-1,0,0}	,{-1,0,0,0}	,{0,0,0,1} }
	,{ {-1,0,0,0}	,{0,0,-1,0}	,{0,1,0,0}	,{0,0,0,1}	}
	,{ {-1,0,0,0}	,{0,0,1,0}	,{0,-1,0,0},{0,0,0,1}	}
	,{ {-1,0,0,0}	,{0,-1,0,0}	,{0,0,-1,0}	,{0,0,0,1}	}
	,{ {1,0,0,0}	,{0,-1,0,0}	,{0,0,1,0}	,{0,0,0,1}	}
};
*/
#ifndef __cplusplus

mat4 GetCubeInvViewProj(int cube_face)
{
#ifndef SFX_SWITCH
	mat4 cubeInvViewProj[6] = {
		{ {0,0,-1,0}	,{0,-1,0,0}	,{1,0,0,0}	,{0,0,0,1}	}
		, { {0,0,1,0}	,{0,-1,0,0}	,{-1,0,0,0}	,{0,0,0,1} }
		,{ {-1,0,0,0}	,{0,0,-1,0}	,{0,1,0,0}	,{0,0,0,1}	}
		,{ {-1,0,0,0}	,{0,0,1,0}	,{0,-1,0,0},{0,0,0,1}	}
		,{ {-1,0,0,0}	,{0,-1,0,0}	,{0,0,-1,0}	,{0,0,0,1}	}
		,{ {1,0,0,0}	,{0,-1,0,0}	,{0,0,1,0}	,{0,0,0,1}	}
	};
	return cubeInvViewProj[cube_face];
#else
	mat4 identity			= { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj0	= { { 0, 0, -1, 0 }, { 0, -1, 0, 0 }, { 1, 0, 0, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj1	= { { 0, 0, 1, 0 }, { 0, -1, 0, 0 }, { -1, 0, 0, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj2	= { { -1, 0, 0, 0 }, { 0, 0, -1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj3	= { { -1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, -1, 0, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj4	= { { -1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 0, 0, -1, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj5	= { { 1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
	mat4 cubeInvViewProj;
	switch (cube_face)
	{
		case 0:
			return cubeInvViewProj0;
		case 1:
			return cubeInvViewProj1;
		case 2:
			return cubeInvViewProj2;
		case 3:
			return cubeInvViewProj3;
		case 4:
			return cubeInvViewProj4;
		case 5:
			return cubeInvViewProj5;
		default:
			return identity;
	};
	return identity;
#endif
}

vec3 CubeClipPosToView(int cube_face, vec4 clip_pos)
{
	vec3 view = -normalize(mul(GetCubeInvViewProj(cube_face), clip_pos).xyz);
	return view;
}

vec3 CubeClipPosToView(int cube_face, vec2 clip_pos2)
{
	vec4 clip_pos = vec4(clip_pos2, 1.0, 1.0);
	vec3 view = -normalize(mul(GetCubeInvViewProj(cube_face), clip_pos).xyz);
	return view;
}

vec3 CubeTexCoordsToView(int cube_face, vec2 texCoords)
{
	vec4 clip_pos = vec4(-1.0, 1.0, 1.0, 1.0);
	clip_pos.x += 2.0*texCoords.x;
	clip_pos.y -= 2.0*texCoords.y;
	return CubeClipPosToView(cube_face, clip_pos);
}

vec3 CubeFaceIndexToView(uint3 idx,uint2 dims)
{
	vec2 texCoords			=(vec2(idx.xy)+vec2(0.5,0.5))/vec2(dims);
	vec4 clip_pos			=vec4(-1.0,1.0,1.0,1.0);
	clip_pos.x				+=2.0*texCoords.x;
	clip_pos.y				-=2.0*texCoords.y;
	vec3 view				=-normalize(mul(GetCubeInvViewProj(idx.z),clip_pos).xyz);
	return view;
}
#endif