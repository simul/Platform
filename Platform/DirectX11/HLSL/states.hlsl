#ifndef STATES_HLSL
#define STATES_HLSL
#include "../../CrossPlatform/SL/states.sl"

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

DepthStencilState TestDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
#if REVERSE_DEPTH==1
	DepthFunc = GREATER_EQUAL;
#else
	DepthFunc = LESS_EQUAL;
#endif
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
#if REVERSE_DEPTH==1
	DepthFunc = GREATER_EQUAL;
#else
	DepthFunc = LESS_EQUAL;
#endif
};

BlendState DoBlend
{
	BlendEnable[0] = TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend = One;
	DestBlend = INV_SRC_ALPHA;
};

BlendState AlphaToCoverageBlend
{
	BlendEnable[0] = TRUE;
	BlendEnable[1]	=TRUE;
	AlphaToCoverageEnable = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
};

BlendState CloudBlend
{
	BlendEnable[0] = TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = ADD;
    //RenderTargetWriteMask[0] = 0x0F;
};

BlendState CloudBufferBlend
{
	BlendEnable[0]	=TRUE;
	BlendEnable[1]	=TRUE;
	SrcBlend		=ONE;
	DestBlend		=SRC_ALPHA;
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

BlendState DontBlend
{
	BlendEnable[0] = FALSE;
	BlendEnable[1]	=FALSE;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

RasterizerState RenderFrontfaceCull
{
	CullMode = front;
};

RasterizerState RenderBackfaceCull
{
	CullMode = back;
};
#define DEPTH_BIAS_D32_FLOAT(d) (d/(1/pow(2,23)))
RasterizerState wireframeRasterizer
{
	FillMode					= WIREFRAME;
	CullMode					= none;
	FrontCounterClockwise		= false;
	DepthBias					= 0;//DEPTH_BIAS_D32_FLOAT(-0.00001);
	DepthBiasClamp				= 0.f;
	SlopeScaledDepthBias		= 0.f;
	DepthClipEnable				= false;
	ScissorEnable				= false;
	MultisampleEnable			= false;
	AntialiasedLineEnable		= true;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};


#endif
