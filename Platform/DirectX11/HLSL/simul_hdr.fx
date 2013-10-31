#include "CppHlsl.hlsl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/hdr_constants.sl"
#include "../../CrossPlatform/mixed_resolution.sl"
#include "states.hlsl"
Texture2D imageTexture;
Texture2D nearImageTexture;
Texture2D depthTexture;
Texture2D lowResDepthTexture;
Texture2D cloudDepthTexture;
Texture2D<uint> glowTexture;

struct a2v
{
	uint vertex_id	: SV_VertexID;
    float4 position	: POSITION;
    float2 texcoord	: TEXCOORD0;
};

struct v2f
{
    float4 hPosition	: SV_POSITION;
    float2 texCoords	: TEXCOORD0;
};

v2f MainVS(idOnly IN)
{
	v2f OUT;
	vec2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	vec2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	// Set to far plane so can use depth test as we want this geometry effectively at infinity
#ifdef REVERSE_DEPTH
	OUT.hPosition.z	=0.0; 
#else
	OUT.hPosition.z	=OUT.hPosition.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(pos.x,-pos.y));
    return OUT;
}

v2f QuadVS(idOnly IN)
{
    v2f OUT;
	float2 poss[4]=
	{
		{ 1.0, 0.0},
		{ 1.0, 1.0},
		{ 0.0, 0.0},
		{ 0.0, 1.0},
	};
	float2 pos		=poss[IN.vertex_id];
	OUT.hPosition	=vec4(pos,0.0,1.0);
	OUT.hPosition	=float4(vec2(2.0*rect.x-1.0,1.0-2.0*(rect.y+rect.w))+2.0*rect.zw*pos,0.0,1.0);
    OUT.texCoords	=vec2(pos.x,1.0-pos.y);
    return OUT;
}

vec4 ShowDepthPS(v2f IN) : SV_TARGET
{
	vec4 depth		=texture_clamp(depthTexture,IN.texCoords);
	float dist		=10.0*depthToFadeDistance(depth.x,2.0*(IN.texCoords-0.5),depthToLinFadeDistParams,tanHalfFov);
    return vec4(1,dist,dist,1.0);
}

vec4 convertInt(Texture2D<uint> glowTexture,vec2 texCoord)
{
	uint2 tex_dim;
	glowTexture.GetDimensions(tex_dim.x, tex_dim.y);

	uint2 location = uint2((uint)(tex_dim.x * texCoord.x), (uint)(tex_dim.y * texCoord.y));
	uint int_color = glowTexture[location];

	// Convert R11G11B10 to float3
	vec4 color;
	color.r = (float)(int_color >> 21) / 2047.0f;
	color.g = (float)((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b = (float)(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;

	return color;
}

vec4 GlowExposureGammaPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_clamp(imageTexture,IN.texCoords);
	vec4 glow=convertInt(glowTexture,IN.texCoords);
	c.rgb+=glow.rgb;
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

vec4 ExposureGammaPS(v2f IN) : SV_TARGET
{
	vec4 c=texture_clamp(imageTexture,IN.texCoords);
	c.rgb*=exposure;
	c.rgb=pow(c.rgb,gamma);
    return vec4(c.rgb,1.f);
}

vec4 DirectPS(v2f IN) : SV_TARGET
{
	vec4 c=exposure*texture_clamp(imageTexture,IN.texCoords);
    return vec4(c.rgba);
}

// Blend the low-res clouds into the scene, using a hi-res depth texture.
// depthTexture.x is the depth value.
// imageTexture.a is the depth that was used to calculate the particular cloud value.

// the blend is 1.0, SRC_ALPHA.
vec4 CloudBlendPS(v2f IN) : SV_TARGET
{
	vec4 res			=texture_nearest_lod(imageTexture,IN.texCoords,0);
	res.rgb				*=exposure;
	vec4 solid			=texture_clamp_lod(depthTexture,IN.texCoords,0);
	vec4 lowres			=texture_nearest_lod(lowResDepthTexture,IN.texCoords,0);
	vec4 cloud			=texture_clamp_lod(cloudDepthTexture,IN.texCoords,0);
	float solid_dist	=depthToFadeDistance(solid.x,vec2(0,0),depthToLinFadeDistParams,vec2(1.0,1.0));
	float cloud_dist	=depthToFadeDistance(cloud.x,vec2(0,0),depthToLinFadeDistParams,vec2(1.0,1.0));
	// We only care about edges that are IN FRONT of the clouds. So solid
	float u				=saturate(10000.0*(0.05+cloud_dist-solid_dist));
	float edge			=saturate(1.0*(abs(lowres.y)+abs(lowres.z)))*u;
//	if(edge>0.5)
	{
		float blend		=saturate((cloud_dist-solid_dist)*100000.0);
		res				=lerp(res,vec4(0,0,0,1.0),blend);
		res.g=blend;
	}
	//res.g=edge;
    return res;
}

void UpdateNearestSample(	inout float MinDist,
							inout vec2 NearestUV,
							float Z,
							vec2 UV,
							float ZFull
							)
{
	float Dist = abs(Z - ZFull);
	if (Dist < MinDist)
	{
		MinDist = Dist;
		NearestUV = UV;
	}
}

bool IsSampleNearer(inout float MinDist,float Z,float ZFull)
{
	float Dist = abs(Z - ZFull);
	return (Dist < MinDist);
}

vec4 NearestDepthCloudBlendPS(v2f IN) : SV_TARGET
{
	vec4 res			=texture_nearest_lod(imageTexture,IN.texCoords,0);
	vec4 solid			=texture_nearest_lod(depthTexture,IN.texCoords,0);
	vec4 lowres			=texture_nearest_lod(lowResDepthTexture,IN.texCoords,0);
	vec4 cloud			=texture_nearest_lod(cloudDepthTexture,IN.texCoords,0);
	float solid_dist	=depthToLinearDistance(solid.x,depthToLinFadeDistParams);
	float cloud_dist	=depthToLinearDistance(cloud.x,depthToLinFadeDistParams);
	float lowres_dist	=depthToLinearDistance(lowres.x,depthToLinFadeDistParams);
	vec2 lowResTexCoords=IN.texCoords;//uint2(IN.texCoords/lowResTexelSize)*lowResTexelSize;
	vec2 NearestUV		=lowResTexCoords;
	float u				=saturate(10000.0*(0.05+cloud_dist-solid_dist));
	float edge			=saturate(1.0*(abs(lowres.y)+abs(lowres.z)))*u;
	if(edge>0.5)
	{
		// GatherRed seems to not fit the bill. We will sample as follows:
		// x = right, y = up, z = left, w = down
		float4 nearDepths	;//=lowResDepthTexture.GatherRed(samplerStateNearest,lowResTexCoords);
		vec2 texc_right		=vec2(lowResTexCoords.x+lowResTexelSize.x,lowResTexCoords.y);
		vec2 texc_left		=vec2(lowResTexCoords.x-lowResTexelSize.x,lowResTexCoords.y);
		vec2 texc_up		=vec2(lowResTexCoords.x,lowResTexCoords.y+lowResTexelSize.y);
		vec2 texc_down		=vec2(lowResTexCoords.x,lowResTexCoords.y-lowResTexelSize.y);
		nearDepths.x		=texture_nearest_lod(lowResDepthTexture,texc_right	,0).x;
		nearDepths.y		=texture_nearest_lod(lowResDepthTexture,texc_up		,0).x;
		nearDepths.z		=texture_nearest_lod(lowResDepthTexture,texc_left	,0).x;
		nearDepths.w		=texture_nearest_lod(lowResDepthTexture,texc_down	,0).x;

		vec4 near_dist		=depthToLinearDistance(nearDepths,depthToLinFadeDistParams);
		// Whichever depth is nearest to the hi-res depth, use the colour from that one.
		res.rgb=0;
		float MinDist		=1.e8f;
		UpdateNearestSample	(MinDist,NearestUV,lowres_dist,lowResTexCoords,solid_dist);
		UpdateNearestSample	(MinDist,NearestUV,near_dist.x,texc_right	,solid_dist);
		UpdateNearestSample	(MinDist,NearestUV,near_dist.y,texc_up		,solid_dist);
		UpdateNearestSample	(MinDist,NearestUV,near_dist.z,texc_left	,solid_dist);
		UpdateNearestSample	(MinDist,NearestUV,near_dist.w,texc_down	,solid_dist);
		res=texture_nearest_lod(imageTexture,NearestUV,0);
	}
	res.rgb				*=exposure;
    return res;
}

// texture_clamp_lod texture_nearest_lod
vec4 NearFarDepthCloudBlendPS(v2f IN) : SV_TARGET
{
	vec4 solid			=texture_clamp_lod(depthTexture,IN.texCoords,0);
	float trueDist		=depthToLinearDistance(solid.x,depthToLinFadeDistParams);
	vec4 cloudFar		//=texture_clamp_lod(imageTexture,IN.texCoords,0);				// low-res cloud image, far depth used.
						=depthDependentFilteredImage(imageTexture,lowResDepthTexture,IN.texCoords,vec4(1.0,0,0,0),depthToLinFadeDistParams,trueDist);
	vec4 cloudNear		=vec4(0,0,0,1.0);
	vec4 lowres			=texture_clamp_lod(lowResDepthTexture,IN.texCoords,0);
	float edge			=lowres.z;
	vec4 result;
	vec2 nearFarDist	=depthToLinearDistance(lowres.yx,depthToLinFadeDistParams);
	if(edge>0.0)
	{
		cloudNear		//=texture_clamp_lod(nearImageTexture,IN.texCoords,0);
						=depthDependentFilteredImage(nearImageTexture,lowResDepthTexture,IN.texCoords,vec4(0,1.0,0,0),depthToLinFadeDistParams,trueDist);
	}
	else
	{
		nearFarDist.x	=0.0;
	}
	float interp		=edge*saturate((nearFarDist.y-trueDist)/(nearFarDist.y-nearFarDist.x));
	result				=lerp(cloudFar,cloudNear,interp);
	result.rgb			*=exposure;
	//result.g=edge;
    return result;
}


vec4 GlowPS(v2f IN) : SV_TARGET
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture_clamp(imageTexture,IN.texCoords+offset/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset/2.0);
	vec2 offset2=offset;
	offset2.x=offset.x*-1.0;
	c+=texture_clamp(imageTexture,IN.texCoords+offset2/2.0);
	c+=texture_clamp(imageTexture,IN.texCoords-offset2/2.0);
	c=c*exposure/4.0;
	c-=1.0*vec4(1.0,1.0,1.0,1.0);
	c=clamp(c,vec4(0.0,0.0,0.0,0.0),vec4(10.0,10.0,10.0,10.0));
    return c;
}

technique11 simul_direct
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,DirectPS()));
    }
}


technique11 exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,ExposureGammaPS()));
    }
}

technique11 glow_exposure_gamma
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowExposureGammaPS()));
    }
}

technique11 simul_sky_blend
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(CloudBufferBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_5_0,MainVS()));
		SetPixelShader(CompileShader(ps_5_0,NearFarDepthCloudBlendPS()));
    }
}

technique11 simul_glow
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend,vec4(1.0f,1.0f,1.0f,1.0f ), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,MainVS()));
		SetPixelShader(CompileShader(ps_4_0,GlowPS()));
    }
}


technique11 show_depth
{
    pass p0
    {
		SetRasterizerState( RenderNoCull );
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(NoBlend,vec4( 0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,QuadVS()));
		SetPixelShader(CompileShader(ps_4_0,ShowDepthPS()));
    }
}

