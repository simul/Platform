#ifndef COMMON_SL
#define COMMON_SL

struct posTexVertexOutput
{
	vec4 hPosition	: SV_POSITION;
	vec2 texCoords	: TEXCOORD0;		
};
struct positionColourVertexInput
{
	vec3 position	: POSITION;
	vec4 colour		: TEXCOORD0;		
};

shader posTexVertexOutput VS_SimpleFullscreen(idOnly IN)
{
	posTexVertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
	OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
	return OUT;
}

posTexVertexOutput VS_ScreenQuad(idOnly IN,vec4 rect)
{
	posTexVertexOutput OUT;
	vec2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(rect.xy+rect.zw*pos,0.0,1.0);
	OUT.hPosition.z	=0.0; 
	OUT.texCoords	=pos;
	OUT.texCoords.y	=1.0-OUT.texCoords.y;
	return OUT;
}
#endif
