//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef RENDER_STATES_SL
#define RENDER_STATES_SL
#include "sampler_states.sl"

#ifdef SFX
	RenderTargetFormatState WaterTargetFormats
	{
		TargetFormat[0] = FMT_32_ABGR;
		TargetFormat[1] = FMT_FP16_ABGR;
		TargetFormat[2] = FMT_FP16_ABGR;
		TargetFormat[3] = FMT_FP16_ABGR;
		TargetFormat[4] = FMT_32_ABGR;
	};
#else
	#define WaterTargetFormats 0
#endif

BlendState WaterNoBlend
{
	BlendEnable[0] = FALSE;	
	BlendEnable[1] = FALSE;
	BlendEnable[2] = FALSE;
	BlendEnable[3] = FALSE;
	BlendEnable[4] = FALSE;
	RenderTargetWriteMask[0]=15;
	RenderTargetWriteMask[1]=15;
	RenderTargetWriteMask[2]=15;
	RenderTargetWriteMask[3]=15;
	RenderTargetWriteMask[4]=15;
};

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

// We DO NOT here specify what kind of depth test to do - it depends on the projection matrix.
// So any shader that uses this MUST have code to set the depth state.
// Remember that for REVERSE_DEPTH projection we use DepthFunc = GREATER_EQUAL
// but for Forward Depth matrices we use DepthFunc = LESS_EQUAL
DepthStencilState TestDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
};

DepthStencilState TestReverseDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
	DepthFunc = GREATER_EQUAL;
};

DepthStencilState TestForwardDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
	DepthFunc = LESS_EQUAL;
};

DepthStencilState WriteDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = ALWAYS;
};

DepthStencilState ReverseDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = GREATER_EQUAL;
};

DepthStencilState ForwardDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
	DepthFunc = LESS_EQUAL;
};

DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
};

BlendState MixBlend
{
	BlendEnable[0]	=TRUE;
	SrcBlend		=BLEND_FACTOR;
	DestBlend		=INV_BLEND_FACTOR;
};

BlendState DoBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=One;
	DestBlend		=INV_SRC_ALPHA;
};

BlendState StencilBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	BlendOp			=MAX;
	SrcBlend		=One;
	DestBlend		=One;
};

BlendState AlphaToCoverageBlend
{
	BlendEnable[0]			=TRUE;
	BlendEnable[1]			=TRUE;
	AlphaToCoverageEnable	=TRUE;
	SrcBlend				=SRC_ALPHA;
	DestBlend				=INV_SRC_ALPHA;
};

BlendState CloudBlend
{
	BlendEnable[0]		= TRUE;
	BlendEnable[1]		= TRUE;
	SrcBlend			= SRC_ALPHA;
	DestBlend			= INV_SRC_ALPHA;
    BlendOp				= ADD;
    SrcBlendAlpha		= ZERO;
    DestBlendAlpha		= INV_SRC_ALPHA;
    BlendOpAlpha		= ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

BlendState FoamBlend
{
	BlendEnable[0]	= TRUE;
	BlendEnable[1]	= TRUE;
	SrcBlend		= ONE;
	DestBlend		= SRC_ALPHA;
	BlendOp			= ADD;
};

BlendState AddDestInvAlphaBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=ONE;
	DestBlend		=INV_SRC_ALPHA;
    BlendOp			=ADD;
};

BlendState AlphaBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=SRC_ALPHA;
	DestBlend		=INV_SRC_ALPHA;
};

BlendState MultiplyBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=ZERO;
	DestBlend		=SRC_COLOR;
};

BlendState AddBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=ONE;
	DestBlend		=ONE;
};

BlendState AddAlphaBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=SRC_ALPHA;
	DestBlend		=ONE;
};

BlendState BlendWithoutWrite
{
	BlendEnable[0] = TRUE;
	BlendEnable[1] = TRUE;
	SrcBlend = ONE;
	DestBlend = ONE;
	RenderTargetWriteMask[0] = 0;
	RenderTargetWriteMask[1] = 0;
};

BlendState AddBlendDontWriteAlpha
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=ONE;
	DestBlend		=ONE;
	RenderTargetWriteMask[0]=7;
	RenderTargetWriteMask[1]=7;
};

BlendState SubtractBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	BlendOp			=SUBTRACT;
	SrcBlend		=ONE;
	DestBlend		=ONE;
};

// special blend for 3-colour blending!
//TODO: implement different blend for each RT on all platforms.
BlendState CompositeBlend
{
	BlendEnable[0]	=TRUE;
	//BlendEnable[1]	=TRUE;
	SrcBlend		=ONE;
	DestBlend		=SRC1_COLOR;
	SrcBlendAlpha	=ZERO;
	DestBlendAlpha	=ONE;
    BlendOp			=ADD;
	RenderTargetWriteMask[0]=7;
	RenderTargetWriteMask[1]=7;
};

RasterizerState RenderNoCull
{
	FillMode					= SOLID;
	CullMode = none;
};

RasterizerState RenderFrontfaceCull
{
	FillMode					= SOLID;
	CullMode = front;
};

RasterizerState RenderBackfaceCull
{
	FillMode					= SOLID;
	CullMode = back;
};
#define DEPTH_BIAS_D32_FLOAT(d) (d/(1/pow(2,23)))
RasterizerState wireframeRasterizer
{
	FillMode					= WIREFRAME;
	CullMode					= none;
	FrontCounterClockwise		= false;
	DepthBias					= 0;//DEPTH_BIAS_D32_FLOAT(-0.00001);
	DepthBiasClamp				= 0.0;
	SlopeScaledDepthBias		= 0.0;
	DepthClipEnable				= false;
	ScissorEnable				= false;
	MultisampleEnable			= false;
	AntialiasedLineEnable		= true;
};

BlendState DontBlend
{
	BlendEnable[0] = FALSE;
	BlendEnable[1]	=FALSE;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};


#endif
