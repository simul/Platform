// NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
#ifndef NV_MULTI_RES_CONSTANTS_SL
#define NV_MULTI_RES_CONSTANTS_SL

struct NvMultiResStruct
{
	uint isMultiResEnabled;
	uint isLensMatchedEnabled;
	vec2 boundingRectOrigin;

	vec2 boundingRectSize;
	vec2 boundingRectSizeInv;

	vec2 multiResToLinearSplitsX;
	vec2 multiResToLinearSplitsY;
	vec2 multiResToLinearX0;
	vec2 multiResToLinearX1;
	vec2 multiResToLinearX2;
	vec2 multiResToLinearY0;
	vec2 multiResToLinearY1;
	vec2 multiResToLinearY2;

	vec2 linearToMultiResSplitsX;
	vec2 linearToMultiResSplitsY;
	vec2 linearToMultiResX0;
	vec2 linearToMultiResX1;
	vec2 linearToMultiResX2;
	vec2 linearToMultiResY0;
	vec2 linearToMultiResY1;
	vec2 linearToMultiResY2;

	vec4 renderTargetToViewRectUVScaleBias;
	vec4 viewRectToRenderTargetUVScaleBias;
};

SIMUL_CONSTANT_BUFFER(NvMultiResConstants,13)
	uniform NvMultiResStruct singleView;
SIMUL_CONSTANT_BUFFER_END


SIMUL_CONSTANT_BUFFER(NvMultiResConstants2,13)
	uniform NvMultiResStruct views[2];
SIMUL_CONSTANT_BUFFER_END

#ifndef __PSSL__


#define COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, MEMBER) \
	nvMultiResConstants.MEMBER = viewStruct.multiResConstants.MEMBER

#define INIT_MULTI_RES_CONSTANTS(nvMultiResConstants, viewStruct) \
	nvMultiResConstants.isMultiResEnabled = viewStruct.isMultiResEnabled;\
	nvMultiResConstants.isLensMatchedEnabled = uint(viewStruct.multiResConstants.isLensMatchedEnabled.x);\
	\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, boundingRectOrigin);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, boundingRectSize);\
	COPY_MULTI_RES_CONSTANT(nvMultiResConstants, viewStruct, boundingRectSizeInv);\
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


#endif

#endif
// NV_MULTI_RES_CONSTANTS_SL
// NVCHANGE_END: TrueSky + VR MultiRes Support
