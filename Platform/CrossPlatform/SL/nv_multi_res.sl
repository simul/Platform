// NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
#ifndef NV_MULTI_RES_SL
#define NV_MULTI_RES_SL

SIMUL_CONSTANT_BUFFER(NvMultiResConstants,13)
	uniform uint isMultiResEnabled;
	uniform uint3 multiResConstantsPad;

	uniform vec2 multiResToLinearSplitsX;
	uniform vec2 multiResToLinearSplitsY;
	uniform vec2 multiResToLinearX0;
	uniform vec2 multiResToLinearX1;
	uniform vec2 multiResToLinearX2;
	uniform vec2 multiResToLinearY0;
	uniform vec2 multiResToLinearY1;
	uniform vec2 multiResToLinearY2;

	uniform vec2 linearToMultiResSplitsX;
	uniform vec2 linearToMultiResSplitsY;
	uniform vec2 linearToMultiResX0;
	uniform vec2 linearToMultiResX1;
	uniform vec2 linearToMultiResX2;
	uniform vec2 linearToMultiResY0;
	uniform vec2 linearToMultiResY1;
	uniform vec2 linearToMultiResY2;

	uniform vec4 renderTargetToViewRectUVScaleBias;
	uniform vec4 viewRectToRenderTargetUVScaleBias;
SIMUL_CONSTANT_BUFFER_END

#define COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, MEMBER) \
	nvMultiResConstants.MEMBER = viewStruct.multiResConstants.MEMBER

#define INIT_MULTI_RES_CONSTANTS(nvMultiResConstants, viewStruct) \
	nvMultiResConstants.isMultiResEnabled = viewStruct.isMultiResEnabled;\
	\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearSplitsX);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearSplitsY);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearX0);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearX1);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearX2);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearY0);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearY1);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, multiResToLinearY2);\
	\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResSplitsX);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResSplitsY);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResX0);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResX1);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResX2);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResY0);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResY1);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, linearToMultiResY2);\
	\
	nvMultiResConstants.renderTargetToViewRectUVScaleBias = vec4();\
	nvMultiResConstants.viewRectToRenderTargetUVScaleBias = vec4();\

#ifndef __cplusplus

vec2 MultiResMapMultiResToLinearInternal(vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (UV.x < multiResToLinearSplitsX.x)
		Result.x = UV.x * multiResToLinearX0.x + multiResToLinearX0.y;
	else if (UV.x < multiResToLinearSplitsX.y)
		Result.x = UV.x * multiResToLinearX1.x + multiResToLinearX1.y;
	else
		Result.x = UV.x * multiResToLinearX2.x + multiResToLinearX2.y;

	if (UV.y < multiResToLinearSplitsY.x)
		Result.y = UV.y * multiResToLinearY0.x + multiResToLinearY0.y;
	else if (UV.y < multiResToLinearSplitsY.y)
		Result.y = UV.y * multiResToLinearY1.x + multiResToLinearY1.y;
	else
		Result.y = UV.y * multiResToLinearY2.x + multiResToLinearY2.y;

	return Result;
}

vec2 MultiResMapLinearToMultiResInternal(vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (UV.x < linearToMultiResSplitsX.x)
		Result.x = UV.x * linearToMultiResX0.x + linearToMultiResX0.y;
	else if (UV.x < linearToMultiResSplitsX.y)
		Result.x = UV.x * linearToMultiResX1.x + linearToMultiResX1.y;
	else
		Result.x = UV.x * linearToMultiResX2.x + linearToMultiResX2.y;

	if (UV.y < linearToMultiResSplitsY.x)
		Result.y = UV.y * linearToMultiResY0.x + linearToMultiResY0.y;
	else if (UV.y < linearToMultiResSplitsY.y)
		Result.y = UV.y * linearToMultiResY1.x + linearToMultiResY1.y;
	else
		Result.y = UV.y * linearToMultiResY2.x + linearToMultiResY2.y;

	return Result;
}

vec2 MultiResMapMultiResToLinear(vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (isMultiResEnabled)
	{
		Result = MultiResMapMultiResToLinearInternal(UV);
	}

	return Result;
}

vec2 MultiResMapLinearToMultiRes(vec2 UV)
{
	// Scale-bias U and V based on which viewport they're in
	vec2 Result = UV;

	if (isMultiResEnabled)
	{
		Result = MultiResMapLinearToMultiResInternal(UV);
	}

	return Result;
}

// Convert ViewRect NDC from multires space to linear space
vec2 MultiResMapNDCMultiResToLinear(vec2 NDC)
{
	vec2 Result = NDC;

	if (isMultiResEnabled)
	{
		Result = MultiResMapMultiResToLinearInternal(NDC * vec2(0.5, -0.5) + 0.5) * vec2(2, -2) + vec2(-1, 1);
	}

	return Result;
}

vec2 MultiResMapNDCLinearToMultiRes(vec2 NDC)
{
	vec2 Result = NDC;

	if (isMultiResEnabled)
	{
		Result = MultiResMapLinearToMultiResInternal(NDC * vec2(0.5, -0.5) + 0.5) * vec2(2, -2) + vec2(-1, 1);
	}

	return Result;
}

// Remap from render-target-relative UVs to view-rect-relative UVs
vec2 MultiResMapRenderTargetToViewRectInternal(vec2 UV)
{
	return UV * renderTargetToViewRectUVScaleBias.xy + renderTargetToViewRectUVScaleBias.zw;
}

// Remap back to render-target-relative UVs
vec2 MultiResMapViewRectToRenderTargetInternal(vec2 UV)
{
	return UV * viewRectToRenderTargetUVScaleBias.xy + viewRectToRenderTargetUVScaleBias.zw;
}

// Convert render target UV from linear space to multires space
vec2 MultiResMapRenderTargetLinearToMultiRes(vec2 UV)
{
	vec2 Result = UV;

	if (isMultiResEnabled)
	{
		UV = MultiResMapRenderTargetToViewRectInternal(UV);
		UV = MultiResMapLinearToMultiResInternal(UV);
		Result = MultiResMapViewRectToRenderTargetInternal(UV);
	}

	return Result;
}

#endif // __cplusplus

#endif // NV_MULTI_RES_SL
// NVCHANGE_END: TrueSky + VR MultiRes Support
