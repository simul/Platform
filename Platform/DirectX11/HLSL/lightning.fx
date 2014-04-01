#include "CppHlsl.hlsl"
#include "states.hlsl"
#include "../../CrossPlatform/lightning_constants.sl"
#include "../../CrossPlatform/depth.sl"
#include "states.hlsl"

Texture2D lightningTexture;
Texture2D depthTexture;
Texture2DMS<float4> depthTextureMS;
Texture2D cloudDepthTexture;

struct transformedVertex
{
    vec4 hPosition	: SV_POSITION;
	vec4 texCoords	: TEXCOORD0;
    vec2 hPosCentre1: TEXCOORD1;
    vec2 hPosCentre2: TEXCOORD2;
	vec2 screenPos:	TEXCOORD3;
    float along: TEXCOORD4;
	float width: TEXCOORD5;
};

struct transformedThinVertex
{
    vec4 hPosition	: SV_POSITION;
	vec4 texCoords	: TEXCOORD0;
};

transformedThinVertex VS_Thin(LightningVertexInput IN)
{
    transformedThinVertex OUT;
    OUT.hPosition	=mul(worldViewProj, vec4(IN.position.xyz , 1.0));
	OUT.texCoords	=IN.texCoords;
    return OUT;
}

LightningVertexInput VS_Thick(LightningVertexInput IN)
{
    LightningVertexInput OUT;
    OUT.position	=mul(worldViewProj, vec4(IN.position.xyz , 1.0));
	OUT.texCoords	=IN.texCoords;
    return OUT;
}

vec2 PixelPos(vec4 vertex)
{
	return vec2(vertex.xy/vertex.w)*viewportPixels;
}

[maxvertexcount(10)]
void GS_Thick(lineadj LightningVertexInput input[4], inout TriangleStream<transformedVertex> SpriteStream)
{
	transformedVertex output;
    //  a - - - - - - - - - - - - - - - - b
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  | - - -start - - - - - - end- - - |
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  d - - - - - - - - - - - - - - - - c
	vec2 p0		=PixelPos(input[0].position);
	vec2 p1		=PixelPos(input[1].position);
	vec2 p2		=PixelPos(input[2].position);
	vec2 p3		=PixelPos(input[3].position);
	vec2 area	=viewportPixels * 1.2;
	if(p1.x<-area.x||p1.x>area.x) return;
	if(p1.y<-area.y||p1.y>area.y) return;
	if(p2.x<-area.x||p2.x>area.x) return;
	if(p2.y<-area.y||p2.y>area.y) return;
	if(input[0].position.z<0) return;
	if(input[0].position.z>1.0) return;
    vec4 start			=input[1].position;
    vec4 end			=input[2].position;
	// determine the direction of each of the 3 segments (previous, current, next
	vec2 v0				=normalize(p1-p0);
	vec2 v1				=normalize(p2-p1);
	vec2 v2				=normalize(p3-p2);
	// determine the normal of each of the 3 segments (previous, current, next)
	vec2 n0				=vec2(-v0.y,v0.x);
	vec2 n1				=vec2(-v1.y,v1.x);
	vec2 n2				=vec2(-v2.y,v2.x);
	// determine miter lines by averaging the normals of the 2 segments
	vec2 miter_a		=normalize(n0 + n1);	// miter at start of current segment
	vec2 miter_b		=normalize(n1 + n2);	// miter at end of current segment
	// determine the length of the miter by projecting it onto normal and then inverse it
	float width1		=input[1].texCoords.x;
	float width2		=input[2].texCoords.x;
	float lengthPixels_a		=width1/start.w	*viewportPixels.x/dot(miter_a, n1);
	float lengthPixels_b		=width2/end.w	*viewportPixels.x/dot(miter_b, n1);
	const float	MITER_LIMIT=1.0;
	output.hPosCentre1	=vec2(p1.xy/viewportPixels);
	output.hPosCentre2	=vec2(p2.xy/viewportPixels);
	output.width		=width1/start.w;
	vec2 diff	=output.hPosCentre2-output.hPosCentre1;
	float dist	=length(diff);
	float d2	=dist*dist;
	// prevent excessively long miters at sharp corners
	if( dot(v0,v1) < -MITER_LIMIT )
	{
		miter_a = n1;
		lengthPixels_a = width1;
		// close the gap
		if(dot(v0,n1)>0)
		{
			output.hPosition	=vec4( (p1 + width1 * n0) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			output.hPosition	=vec4( (p1 + width1 * n1) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			output.hPosition	=vec4( p1 / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0.5,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			SpriteStream.RestartStrip();
		}
		else
		{
			output.hPosition	=vec4( (p1 - width2 * n1) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			output.hPosition	=vec4( (p1 - width2 * n0) / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			output.hPosition	=vec4( p1 / viewportPixels, 0.0, 1.0 );
			output.screenPos	=output.hPosition.xy;
			output.along		=dot(output.hPosition.xy-output.hPosCentre1.xy,diff)/d2;
			output.texCoords	=vec4(0.5,input[1].texCoords.yzw);
			SpriteStream.Append(output);
			SpriteStream.RestartStrip();
		}
	}
	if( dot(v1,v2) < -MITER_LIMIT )
	{
		miter_b = n1;
		lengthPixels_b = width2;
	}
  // generate the triangle strip
	output.width		=width1/start.w;
	output.screenPos	=(p1 + lengthPixels_a * miter_a)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*start.w,start.z,start.w);
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2*1.1-0.05;
	output.texCoords	=vec4(0.0,input[1].texCoords.yzw);
	SpriteStream.Append(output);
	output.screenPos	=(p1 - lengthPixels_a * miter_a)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*start.w,start.z,start.w);
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(1.0,input[1].texCoords.yzw);
	SpriteStream.Append(output);
	output.width		=width2/end.w;
	output.screenPos	=(p2 + lengthPixels_b * miter_b)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*end.w,end.z,end.w);
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(0.0,input[2].texCoords.yzw);
	SpriteStream.Append(output);
	output.screenPos	=(p2 - lengthPixels_b * miter_b)/viewportPixels;
	output.hPosition	=vec4(output.screenPos.xy*end.w,end.z,end.w);
	output.along		=dot(output.screenPos.xy-output.hPosCentre1.xy,diff)/d2;
	output.texCoords	=vec4(1.0,input[2].texCoords.yzw);
	SpriteStream.Append(output);
    SpriteStream.RestartStrip();
}

float4 PS_Main(transformedVertex IN): SV_TARGET
{
	vec2 texCoords=IN.screenPos*0.5-0.5;
	vec4 dlookup 		=sampleLod(depthTexture,samplerStateNearest,viewportCoordToTexRegionCoord(texCoords.xy,viewportToTexRegionScaleBias),0);
	vec4 clip_pos		=vec4(IN.screenPos,1.0,1.0);

	//if(dlookup.x<depth)
	//	discard;
	
	float b			=2.0*(IN.texCoords.x-0.5);
	float br		=pow(1.0-b*b,4.0)*IN.texCoords.w;// w is the local brightness factor
	vec2 centre		=lerp(IN.hPosCentre1,IN.hPosCentre2,saturate(IN.along));
	vec2 diff		=IN.screenPos-centre;
	float dist		=length(diff)/IN.width;
	br				*=exp(-6.0*dist);
	float4 colour	=br*lightningColour;//lightningTexture.Sample(clampSamplerState,IN.texCoords.xy);
    return colour;
}

float4 PS_Thin(transformedThinVertex IN): SV_TARGET
{
	float4 colour=lightningColour*IN.texCoords.w;//lightningTexture.Sample(clampSamplerState,IN.texCoords.xy);
    return colour;
}

technique11 lightning_thick
{
    pass p0 
    {
		SetDepthStencilState(TestDepth,0);
        SetRasterizerState(RenderNoCull);
		SetBlendState(DoBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Thick()));
        SetGeometryShader( CompileShader(gs_5_0, GS_Thick()));
		SetPixelShader(CompileShader(ps_5_0,PS_Main()));
    }
}
RasterizerState lightningLineRasterizer
{
	FillMode					= WIREFRAME;
	CullMode					= none;
	FrontCounterClockwise		= false;
	DepthBias					= 0;//DEPTH_BIAS_D32_FLOAT(-0.00001);
	DepthBiasClamp				= 0.f;
	SlopeScaledDepthBias		= 0.f;
	DepthClipEnable				= false;
	ScissorEnable				= false;
	MultisampleEnable			= true;
	AntialiasedLineEnable		= false;
};

technique11 lightning_thin
{
    pass p0 
    {
		SetDepthStencilState(TestDepth,0);
        SetRasterizerState(lightningLineRasterizer);
		SetBlendState(DoBlend,vec4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,VS_Thin()));
		SetPixelShader(CompileShader(ps_5_0,PS_Thin()));
    }
}

