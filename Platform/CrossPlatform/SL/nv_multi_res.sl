// NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
#ifndef NV_MULTI_RES_SL
#define NV_MULTI_RES_SL

#ifndef __PSSL__
#define MULTIRES_ENABLED 1

#ifndef __cplusplus

struct NV_LensMatchedShading_Configuration
{
	float WarpLeft;
	float WarpRight;
	float WarpUp;
	float WarpDown;
	float2	ClipToWindowX[3];
	float2	ClipToWindowY[3];

	float	WindowToClipSplitsX[2];
	float	WindowToClipSplitsY[2];
	float2	WindowToClipX[3];
	float2	WindowToClipY[3];

	float2 BoundingRectOrigin;
	float2 BoundingRectSize;
	float2 BoundingRectSizeInv;
};

NV_LensMatchedShading_Configuration NV_VR_GetLensMatchedShading_Configuration_View(NvMultiResStruct nv)
{
	NV_LensMatchedShading_Configuration res;

	res.WarpLeft				=nv.linearToMultiResSplitsX.x;
	res.WarpRight				=nv.linearToMultiResSplitsX.y;
	res.WarpUp					=nv.linearToMultiResSplitsY.x;
	res.WarpDown				=nv.linearToMultiResSplitsY.y;
	res.ClipToWindowX[0]		=nv.linearToMultiResX0;
	res.ClipToWindowX[1]		=nv.linearToMultiResX1;
	res.ClipToWindowX[2]		=nv.linearToMultiResX2;
	res.ClipToWindowY[0]		=nv.linearToMultiResY0;
	res.ClipToWindowY[1]		=nv.linearToMultiResY1;
	res.ClipToWindowY[2]		=nv.linearToMultiResY2;
	res.WindowToClipSplitsX[0]	=nv.multiResToLinearSplitsX.x;
	res.WindowToClipSplitsX[1]	=nv.multiResToLinearSplitsX.y;
	res.WindowToClipSplitsY[0]	=nv.multiResToLinearSplitsY.x;
	res.WindowToClipSplitsY[1]	=nv.multiResToLinearSplitsY.y;
	res.WindowToClipX[0]		=nv.multiResToLinearX0;
	res.WindowToClipX[1]		=nv.multiResToLinearX1;
	res.WindowToClipX[2]		=nv.multiResToLinearX2;
	res.WindowToClipY[0]		=nv.multiResToLinearY0;
	res.WindowToClipY[1]		=nv.multiResToLinearY1;
	res.WindowToClipY[2]		=nv.multiResToLinearY2;
	res.BoundingRectOrigin		=nv.boundingRectOrigin.xy;
	res.BoundingRectSize		=nv.boundingRectSize.xy;
	res.BoundingRectSizeInv		=nv.boundingRectSizeInv.xy;

	return res;
};

float2 NV_VR_GetLensMatchedShading_factors(float2 clip_pos, NV_LensMatchedShading_Configuration c)
{
	float2 f;
	f.x = clip_pos.x < 0 ? -c.WarpLeft : +c.WarpRight;
	f.y = clip_pos.y < 0 ? -c.WarpDown : +c.WarpUp;
	return f;
}

float2 NV_VR_GetLensMatchedShading_factors(uint viewport, NV_LensMatchedShading_Configuration c)
{
	float2 f;
	f.x = ((viewport == 0) || (viewport == 2)) ? -c.WarpLeft : +c.WarpRight;
	f.y = ((viewport == 2) || (viewport == 3)) ? -c.WarpDown : +c.WarpUp;
	return f;
}

// todo: we can probably just change this to work in warped clip space instead of window space
// that probably also eliminates need for windowtoclip and cliptowindow transforms
float4 NV_VR_MapWindowToClip(
	NV_LensMatchedShading_Configuration	conf,
	float3				windowPos,
	bool				normalize = true)
{
	float A, B;
	float2 ViewportX, ViewportY;

	if (windowPos.x < conf.WindowToClipSplitsX[0])
	{
		A = +conf.WarpLeft;
		ViewportX = conf.WindowToClipX[0];
	}
	else
	{
		A = -conf.WarpRight;
		ViewportX = conf.WindowToClipX[1];
	}

	if (windowPos.y < conf.WindowToClipSplitsY[0])
	{
		B = -conf.WarpUp;
		ViewportY = conf.WindowToClipY[0];
	}
	else
	{
		B = +conf.WarpDown;
		ViewportY = conf.WindowToClipY[1];
	}

	float4 clipPos;
	clipPos.x = windowPos.x * ViewportX.x + ViewportX.y;
	clipPos.y = windowPos.y * ViewportY.x + ViewportY.y;
	clipPos.z = windowPos.z;
	clipPos.w = 1.0 + clipPos.x * A + clipPos.y * B;

	if (normalize)
	{
		clipPos.xyz /= clipPos.w;
		clipPos.w = 1.0;
	}

	return clipPos;
}

float3 NV_VR_MapClipToWindow(
	NV_LensMatchedShading_Configuration	conf,
	float4				clipPos,
	bool				normalize = true)
{
	float A, B;
	float2 ViewportX, ViewportY;

	if (clipPos.x < 0)
	{
		A = -conf.WarpLeft;
		ViewportX = conf.ClipToWindowX[0];
	}
	else
	{
		A = +conf.WarpRight;
		ViewportX = conf.ClipToWindowX[1];
	}

	if (clipPos.y < 0)
	{
		B = -conf.WarpDown;
		ViewportY = conf.ClipToWindowY[0];
	}
	else
	{
		B = +conf.WarpUp;
		ViewportY = conf.ClipToWindowY[1];
	}

	float4 warpedPos = clipPos;
	warpedPos.w = clipPos.w + clipPos.x * A + clipPos.y * B;
	warpedPos.xyz /= warpedPos.w;
	warpedPos.w = 1.0;

	float3 windowPos;
	windowPos.x = warpedPos.x * ViewportX.x + ViewportX.y;
	windowPos.y = warpedPos.y * ViewportY.x + ViewportY.y;
	windowPos.z = warpedPos.z;

	return windowPos;
}

float2 NV_VR_MapUV_LinearToVR(
	NV_LensMatchedShading_Configuration	conf,
	float2				linearUV)
{
	float4 clipPos = float4(linearUV.x * 2 - 1, -linearUV.y * 2 + 1, 0, 1);
	float3 windowPos = NV_VR_MapClipToWindow(conf, clipPos, false);
	float2 vrUV = (windowPos.xy - conf.BoundingRectOrigin) * conf.BoundingRectSizeInv;
	return vrUV;
}

float2 NV_VR_MapUV_VRToLinear(
	NV_LensMatchedShading_Configuration	conf,
	float2				vrUV)
{
	float2 windowPos = vrUV * conf.BoundingRectSize + conf.BoundingRectOrigin;
	float4 clipPos = NV_VR_MapWindowToClip(conf, float3(windowPos, 0));
	float2 linearUV = float2(clipPos.x * 0.5 + 0.5, -clipPos.y * 0.5 + 0.5);
	return linearUV;
}

vec2 MultiResMapMultiResToLinearInternal(NvMultiResStruct nv,vec2 UV)
{
	if (nv.isLensMatchedEnabled)
	{
		return NV_VR_MapUV_VRToLinear(NV_VR_GetLensMatchedShading_Configuration_View(nv), UV);
	}

	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (UV.x < nv.multiResToLinearSplitsX.x)
		Result.x = UV.x * nv.multiResToLinearX0.x + nv.multiResToLinearX0.y;
	else if (UV.x < nv.multiResToLinearSplitsX.y)
		Result.x = UV.x * nv.multiResToLinearX1.x + nv.multiResToLinearX1.y;
	else
		Result.x = UV.x * nv.multiResToLinearX2.x + nv.multiResToLinearX2.y;

	if (UV.y < nv.multiResToLinearSplitsY.x)
		Result.y = UV.y * nv.multiResToLinearY0.x + nv.multiResToLinearY0.y;
	else if (UV.y < nv.multiResToLinearSplitsY.y)
		Result.y = UV.y * nv.multiResToLinearY1.x + nv.multiResToLinearY1.y;
	else
		Result.y = UV.y * nv.multiResToLinearY2.x + nv.multiResToLinearY2.y;

	return Result;
}

vec2 MultiResMapLinearToMultiResInternal(NvMultiResStruct nv,vec2 UV)
{
	if (nv.isLensMatchedEnabled)
	{
		return NV_VR_MapUV_LinearToVR(NV_VR_GetLensMatchedShading_Configuration_View(nv), UV);
	}

	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (UV.x < nv.linearToMultiResSplitsX.x)
		Result.x = UV.x * nv.linearToMultiResX0.x + nv.linearToMultiResX0.y;
	else if (UV.x < nv.linearToMultiResSplitsX.y)
		Result.x = UV.x * nv.linearToMultiResX1.x + nv.linearToMultiResX1.y;
	else
		Result.x = UV.x * nv.linearToMultiResX2.x + nv.linearToMultiResX2.y;

	if (UV.y < nv.linearToMultiResSplitsY.x)
		Result.y = UV.y * nv.linearToMultiResY0.x + nv.linearToMultiResY0.y;
	else if (UV.y < nv.linearToMultiResSplitsY.y)
		Result.y = UV.y * nv.linearToMultiResY1.x + nv.linearToMultiResY1.y;
	else
		Result.y = UV.y * nv.linearToMultiResY2.x + nv.linearToMultiResY2.y;

	return Result;
}

vec2 MultiResMapMultiResToLinear(NvMultiResStruct nv,vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (nv.isMultiResEnabled)
	{
		Result = MultiResMapMultiResToLinearInternal(nv,UV);
	}

	return Result;
}

vec2 MultiResMapLinearToMultiRes(NvMultiResStruct nv,vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (nv.isMultiResEnabled)
	{
		Result = MultiResMapLinearToMultiResInternal(nv,UV);
	}

	return Result;
}

// Convert ViewRect NDC from multires space to linear space
vec2 MultiResMapNDCMultiResToLinear(NvMultiResStruct nv,vec2 NDC)
{
	vec2 Result = NDC;

	if (nv.isMultiResEnabled)
	{
		Result = MultiResMapMultiResToLinearInternal(nv,NDC * vec2(0.5, -0.5) + 0.5) * vec2(2, -2) + vec2(-1, 1);
	}

	return Result;
}

vec2 MultiResMapNDCLinearToMultiRes(NvMultiResStruct nv,vec2 NDC)
{
	vec2 Result = NDC;

	if (nv.isMultiResEnabled)
	{
		Result = MultiResMapLinearToMultiResInternal(nv,NDC * vec2(0.5, -0.5) + 0.5) * vec2(2, -2) + vec2(-1, 1);
	}

	return Result;
}

// Remap from render-target-relative UVs to view-rect-relative UVs
vec2 MultiResMapRenderTargetToViewRectInternal(NvMultiResStruct nv,vec2 UV)
{
	return UV * nv.renderTargetToViewRectUVScaleBias.xy + nv.renderTargetToViewRectUVScaleBias.zw;
}

// Remap back to render-target-relative UVs
vec2 MultiResMapViewRectToRenderTargetInternal(NvMultiResStruct nv,vec2 UV)
{
	return UV * nv.viewRectToRenderTargetUVScaleBias.xy + nv.viewRectToRenderTargetUVScaleBias.zw;
}

// Convert render target UV from linear space to multires space
vec2 MultiResMapRenderTargetLinearToMultiRes(NvMultiResStruct nv,vec2 UV)
{
	vec2 Result = UV;

	if (nv.isMultiResEnabled)
	{
		UV = MultiResMapRenderTargetToViewRectInternal(nv,UV);
		UV = MultiResMapLinearToMultiResInternal(nv,UV);
		Result = MultiResMapViewRectToRenderTargetInternal(nv,UV);
	}

	return Result;
}

float LensMatchedWarpedViewRectUVZToClipDepthInternal(NvMultiResStruct nv,vec2 UV, float depth)
{
	NV_LensMatchedShading_Configuration	conf = NV_VR_GetLensMatchedShading_Configuration_View(nv);

	vec3 windowsPos;
	windowsPos.xy = UV * conf.BoundingRectSize + conf.BoundingRectOrigin;
	windowsPos.z = depth;

	return NV_VR_MapWindowToClip(conf, windowsPos).z;
}

float LensMatchedWarpedRenderTargetUVZToClipDepth(NvMultiResStruct nv,vec3 UVZ)
{
	float depth = UVZ.z;

	if (nv.isLensMatchedEnabled)
	{
		vec2 UV = MultiResMapRenderTargetToViewRectInternal(nv,UVZ.xy);

		depth =  LensMatchedWarpedViewRectUVZToClipDepthInternal(nv,UV, depth);
	}

	return depth;
}

float LensMatchedWarpedViewRectNDCToClipDepth(NvMultiResStruct nv,vec3 NDC)
{
	float depth = NDC.z;

	if (nv.isLensMatchedEnabled)
	{
		vec2 UV = NDC.xy * vec2(0.5, -0.5) + 0.5;

		depth =  LensMatchedWarpedViewRectUVZToClipDepthInternal(nv,UV, depth);
	}

	return depth;
}


#endif
// __cplusplus
#else

#define MULTIRES_ENABLED 0

#define MultiResMapMultiResToLinearInternal(nv,x) x
#define MultiResMapLinearToMultiResInternal(nv,x) x

#define MultiResMapMultiResToLinear(nv,x) x
#define MultiResMapLinearToMultiRes(nv,x) x
#define MultiResMapNDCMultiResToLinear(nv,x) x

#define MultiResMapNDCLinearToMultiRes(nv,x) x
#define MultiResMapRenderTargetToViewRectInternal(nv,x) x
#define MultiResMapViewRectToRenderTargetInternal(nv,x) x
#define MultiResMapRenderTargetLinearToMultiRes(nv,x) x

float LensMatchedWarpedRenderTargetUVZToClipDepth(NvMultiResStruct nv,vec3 UVZ)
{
	float depth = UVZ.z;
	return depth;
}
float LensMatchedWarpedViewRectNDCToClipDepth(NvMultiResStruct nv,vec3 NDC)
{
	float depth = NDC.z;
	return depth;
}

#endif
#endif
// NV_MULTI_RES_SL
// NVCHANGE_END: TrueSky + VR MultiRes Support
