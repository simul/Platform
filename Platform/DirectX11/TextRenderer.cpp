#include "TextRenderer.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"

using namespace simul;
using namespace dx11;

struct FontIndex
{
	float x;
	float w;
	int pixel_width;
};

FontIndex fontIndices[]={
{0.0f		,0.0f			,5},
{0.0f		,0.000976563f	,1},
{0.00195313f,0.00488281f	,3},
{0.00585938f,0.0136719f		,8},
{0.0146484f	,0.0195313f		,5},
{0.0205078f	,0.0302734f		,10},
{0.03125f	,0.0390625f		,8},
{0.0400391f	,0.0410156f		,1},
{0.0419922f	,0.0449219f		,3},
{0.0458984f	,0.0488281f		,3},
{0.0498047f	,0.0546875f		,5},
{0.0556641f	,0.0625f		,7},
{0.0634766f	,0.0644531f		,1},
{0.0654297f	,0.0683594f		,3},
{0.0693359f	,0.0703125f		,1},
{0.0712891f	,0.0751953f		,4},
{0.0761719f	,0.0820313f		,6},
{0.0830078f	,0.0859375f		,3},
{0.0869141f	,0.0927734f		,6},
{0.09375f	,0.0996094f		,6},
{0.100586f	,0.106445f		,6},
{0.107422f	,0.113281f		,6},
{0.114258f	,0.120117f		,6},
{0.121094f	,0.126953f		,6},
{0.12793f,0.133789f,6},
{0.134766f,0.140625f,6},
{0.141602f,0.142578f,1},
{0.143555f,0.144531f,1},
{0.145508f,0.151367f,6},
{0.152344f,0.15918f,7},
{0.160156f,0.166016f,6},
{0.166992f,0.171875f,5},
{0.172852f,0.18457f,12},
{0.185547f,0.194336f,9},
{0.195313f,0.202148f,7},
{0.203125f,0.209961f,7},
{0.210938f,0.217773f,7},
{0.21875f,0.225586f,7},
{0.226563f,0.232422f,6},
{0.233398f,0.241211f,8},
{0.242188f,0.249023f,7},
{0.25f,0.250977f,1},
{0.251953f,0.256836f,5},
{0.257813f,0.265625f,8},
{0.266602f,0.272461f,6},
{0.273438f,0.282227f,9},
{0.283203f,0.290039f,7},
{0.291016f,0.298828f,8},
{0.299805f,0.306641f,7},
{0.307617f,0.31543f,8},
{0.316406f,0.323242f,7},
{0.324219f,0.331055f,7},
{0.332031f,0.338867f,7},
{0.339844f,0.34668f,7},
{0.347656f,0.356445f,9},
{0.357422f,0.370117f,13},
{0.371094f,0.37793f,7},
{0.378906f,0.385742f,7},
{0.386719f,0.393555f,7},
{0.394531f,0.396484f,2},
{0.397461f,0.401367f,4},
{0.402344f,0.404297f,2},
{0.405273f,0.410156f,5},
{0.411133f,0.417969f,7},
{0.418945f,0.420898f,2},
{0.421875f,0.426758f,5},
{0.427734f,0.432617f,5},
{0.433594f,0.438477f,5},
{0.439453f,0.444336f,5},
{0.445313f,0.450195f,5},
{0.451172f,0.455078f,4},
{0.456055f,0.460938f,5},
{0.461914f,0.466797f,5},
{0.467773f,0.46875f,1},
{0.469727f,0.472656f,3},
{0.473633f,0.478516f,5},
{0.479492f,0.480469f,1},
{0.481445f,0.490234f,9},
{0.491211f,0.496094f,5},
{0.49707f,0.501953f,5},
{0.50293f,0.507813f,5},
{0.508789f,0.513672f,5},
{0.514648f,0.517578f,3},
{0.518555f,0.523438f,5},
{0.524414f,0.527344f,3},
{0.52832f,0.533203f,5},
{0.53418f,0.539063f,5},
{0.540039f,0.548828f,9},
{0.549805f,0.554688f,5},
{0.555664f,0.560547f,5},
{0.561523f,0.566406f,5},
{0.567383f,0.570313f,3},
{0.571289f,0.572266f,1},
{0.573242f,0.576172f,3},
{0.577148f,0.583984f,7},
};

TextRenderer::TextRenderer()
	:m_pd3dDevice(NULL),effect(NULL),font_texture_SRV(NULL)
{
}

TextRenderer::~TextRenderer()
{
	InvalidateDeviceObjects();
}

void TextRenderer::RestoreDeviceObjects(ID3D11Device* device)
{
	m_pd3dDevice=device;
	constantBuffer.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
	SAFE_RELEASE(font_texture_SRV);
	font_texture_SRV=simul::dx11::LoadTexture(m_pd3dDevice,"Font16.png");
}

void TextRenderer::InvalidateDeviceObjects()
{
	constantBuffer.InvalidateDeviceObjects();
	SAFE_RELEASE(effect);
	SAFE_RELEASE(font_texture_SRV);
	m_pd3dDevice=NULL;
}

void TextRenderer::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	SAFE_RELEASE(effect);
	effect=LoadEffect(m_pd3dDevice,"font.fx",defines);
	constantBuffer.LinkToEffect(effect,"FontConstants");
}

void TextRenderer::Render(void *context,float x,float y,float screen_width,float screen_height,const char *txt,const float *clr,const float *bck,bool mirrorY)
{
	float transp[]={0.f,0.f,0.f,0.f};
	float white[]={1.f,1.f,1.f,1.f};
	if(!clr)
		clr=white;
	if(!bck)
		bck=transp;
	simul::dx11::setTexture(effect,"fontTexture"	,font_texture_SRV);
	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	constantBuffer.colour		=vec4(clr);
	constantBuffer.background	=vec4(bck);
	// Calc width and draw background:
	int w=0;
	for(int i=0;i<32;i++)
	{
		if(txt[i]==0)
			break;
		int idx=(int)txt[i]-32;
		if(idx<0||idx>100)
			continue;
		const FontIndex &f=fontIndices[idx];
		w+=f.pixel_width+1;
	}
	ApplyPass(pContext,effect->GetTechniqueByName("backg")->GetPassByIndex(0));
	constantBuffer.rect		=vec4(2.0f*x/screen_width-1.f,1.f-2.0f*(y+16.f)/screen_height,2.0f*(float)w/screen_width,2.0f*16.f/screen_height);
	if(mirrorY)
	{
		constantBuffer.rect.y=-constantBuffer.rect.y;
		constantBuffer.rect.w*=-1.0f;
	}
	constantBuffer.Apply(pContext);
	pContext->Draw(4,0);
	ApplyPass(pContext,effect->GetTechniqueByName("text")->GetPassByIndex(0));

	for(int i=0;i<32;i++)
	{
		if(txt[i]==0)
			break;
		int idx=(int)txt[i]-32;
		if(idx<0||idx>100)
			continue;
		const FontIndex &f=fontIndices[idx];
		if(idx>0)
		{
			constantBuffer.rect.x	=2.0f*x/screen_width-1.f;
			constantBuffer.rect.z	=2.0f*(float)f.pixel_width/screen_width;
			static float u			=1024.f/598.f;
			constantBuffer.texc		=vec4(f.x*u,0.0f,(f.w-f.x)*u,1.0f);
			constantBuffer.Apply(pContext);
			pContext->Draw(4,0);
		}
		x+=f.pixel_width+1;
	}
	pContext->IASetPrimitiveTopology(previousTopology);
}