#ifndef STATES_HLSL
#define STATES_HLSL
#include "../../CrossPlatform/states.sl"

DepthStencilState DisableDepth
{
	DepthEnable = FALSE;
	DepthWriteMask = ZERO;
}; 

DepthStencilState TestDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ZERO;
#ifdef REVERSE_DEPTH
	DepthFunc = GREATER_EQUAL;
#else
	DepthFunc = LESS_EQUAL;
#endif
};

DepthStencilState EnableDepth
{
	DepthEnable = TRUE;
	DepthWriteMask = ALL;
#ifdef REVERSE_DEPTH
	DepthFunc = GREATER_EQUAL;
#else
	DepthFunc = LESS_EQUAL;
#endif
};

BlendState DoBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = One;
	DestBlend = INV_SRC_ALPHA;
};

BlendState AlphaBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
};

BlendState MultiplyBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = ZERO;
	DestBlend = SRC_COLOR;
};

BlendState AddBlend
{
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = ONE;
};

BlendState DontBlend
{
	BlendEnable[0] = FALSE;
};

RasterizerState RenderNoCull
{
	CullMode = none;
};

BlendState NoBlend
{
	BlendEnable[0] = FALSE;
};


#endif