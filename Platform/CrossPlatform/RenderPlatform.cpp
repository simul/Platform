#include "RenderPlatform.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/TextRenderer.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Effect.h"

using namespace simul;
using namespace crossplatform;


RenderPlatform::RenderPlatform()
	:mirrorY(false)
	,mirrorY2(false)
	,textRenderer(NULL)
{
}
DeviceContext &RenderPlatform::GetImmediateContext()
{
	return immediateContext;
}

void RenderPlatform::RestoreDeviceObjects(void*)
{
	crossplatform::RenderStateDesc desc;
	memset(&desc,0,sizeof(desc));
	desc.type=crossplatform::BLEND;
	desc.blend.numRTs=1;
	desc.blend.RenderTarget[0].BlendEnable			=false;
	desc.blend.RenderTarget[0].RenderTargetWriteMask=15;
	desc.blend.RenderTarget[0].SrcBlend				=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlend			=crossplatform::BLEND_ZERO;
	desc.blend.RenderTarget[0].SrcBlendAlpha		=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlendAlpha		=crossplatform::BLEND_ZERO;
	RenderState *opaque=CreateRenderState(desc);
	standardRenderStates[STANDARD_OPAQUE_BLENDING]=opaque;

	desc.blend.RenderTarget[0].BlendEnable			=true;
	desc.blend.RenderTarget[0].SrcBlend				=crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].DestBlend			=crossplatform::BLEND_INV_SRC_ALPHA;
	desc.blend.RenderTarget[0].SrcBlendAlpha		=crossplatform::BLEND_SRC_ALPHA;
	desc.blend.RenderTarget[0].DestBlendAlpha		=crossplatform::BLEND_INV_SRC_ALPHA;
	RenderState *alpha=CreateRenderState(desc);
	standardRenderStates[STANDARD_ALPHA_BLENDING]=alpha;
	
	memset(&desc,0,sizeof(desc));
	desc.type=crossplatform::DEPTH;
	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_GREATER_EQUAL;
	desc.depth.test			=true;
	desc.depth.write		=true;

	RenderState *depth_ge=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_GREATER_EQUAL]=depth_ge;
	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_LESS_EQUAL;
	RenderState *depth_le=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_LESS_EQUAL]=depth_le;

	desc.depth.write			=false;
	RenderState *depth_tle=CreateRenderState(desc);
	standardRenderStates[STANDARD_TEST_DEPTH_LESS_EQUAL]=depth_tle;

	desc.depth.comparison	=crossplatform::DepthComparison::DEPTH_GREATER_EQUAL;
	RenderState *depth_tge=CreateRenderState(desc);
	standardRenderStates[STANDARD_TEST_DEPTH_GREATER_EQUAL]=depth_tge;

	desc.depth.test			=false;
	RenderState *depth_no=CreateRenderState(desc);
	standardRenderStates[STANDARD_DEPTH_DISABLE]=depth_no;

	SAFE_DELETE(textRenderer);
	textRenderer=new TextRenderer;
	textRenderer->RestoreDeviceObjects(this);
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if(textRenderer)
		textRenderer->InvalidateDeviceObjects();
	for(std::map<StandardRenderState,RenderState*>::iterator i=standardRenderStates.begin();i!=standardRenderStates.end();i++)
		SAFE_DELETE(i->second);
	standardRenderStates.clear();
	SAFE_DELETE(textRenderer);
}
void RenderPlatform::RecompileShaders()
{
	textRenderer->RecompileShaders();
}

void RenderPlatform::DrawCubemap		(DeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float exposure,float gamma)
{
}

void RenderPlatform::Print(DeviceContext &deviceContext,int x,int y	,const char *text,const float* colr,const float* bkg)
{
	float clr[]={1.f,1.f,0.f,1.f};
	float black[]={0.f,0.f,0.f,0.0f};
	if(!colr)
		colr=clr;
	if(!bkg)
		bkg=black;
	unsigned int num_v=1;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	int h=(int)viewport.h;
	int pos=0;
	
	while(*text!=0)
	{
		textRenderer->Render(deviceContext,(float)x,(float)y,(float)viewport.w,(float)h,text,colr,bkg,mirrorY2);
		while(*text!='\n'&&*text!=0)
		{
			text++;
			pos++;
		}
		if(!(*text))
			break;
		text++;
		pos++;
		y+=16;
	}
}

void RenderPlatform::EnsureEffectIsBuiltPartialSpec(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options,const std::map<std::string,std::string> &defines)
{
	if(options.size())
	{
		std::vector<crossplatform::EffectDefineOptions> opts=options;
		opts.pop_back();
		crossplatform::EffectDefineOptions opt=options.back();
		for(int i=0;i<(int)opt.options.size();i++)
		{
			std::map<std::string,std::string> defs=defines;
			defs[opt.name]=opt.options[i];
			EnsureEffectIsBuiltPartialSpec(filename_utf8,opts,defs);
		}
	}
	else
	{
		crossplatform::Effect *e=CreateEffect(filename_utf8,defines);
		delete e;
	}
}

Effect *RenderPlatform::CreateEffect(const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(filename_utf8,defines);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &opts)
{
	const std::map<std::string,std::string> defines;
	static bool enabled=true;
	if(enabled&&simul::base::GetFeatureLevel()>=base::EXPERIMENTAL)
		EnsureEffectIsBuiltPartialSpec(filename_utf8,opts,defines);
}

namespace simul
{
	namespace crossplatform
	{
		void DrawGrid(crossplatform::DeviceContext &deviceContext,float square_size,float brightness,int numLines)
		{
			// 101 lines across, 101 along.
			numLines++;
			crossplatform::RenderPlatform::Vertext *lines=new crossplatform::RenderPlatform::Vertext[2*numLines*2];
			// one metre apart
			vec3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
			vec3 centrePos(square_size*(int)(cam_pos.x/square_size),square_size*(int)(cam_pos.y/square_size),0);
			crossplatform::RenderPlatform::Vertext *vertex=lines;
			int halfOffset=numLines/2;
			float l10=brightness;
			float l5=brightness*0.5f;
			float l25=brightness*2.5f;
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				vec3 pos1		=centrePos+vec3(square_size*j	,-square_size*halfOffset	,0);
				vec3 pos2		=centrePos+vec3(square_size*j	, square_size*halfOffset	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(l5,l5,0.f,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(l5,0.1f*l5,0.f,1.0f);
				vertex++;
			}
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				vec3 pos1		=centrePos+vec3(-square_size*halfOffset	,square_size*j	,0);
				vec3 pos2		=centrePos+vec3( square_size*halfOffset	,square_size*j	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(0.f,l10,l5,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(0.f,l25,l5,1.0f);
				vertex++;
			}
			deviceContext.renderPlatform->DrawLines(deviceContext,lines,2*numLines*2,false,true);
			delete[] lines;
		}
	}
}

void RenderPlatform::SetStandardRenderState	(DeviceContext &deviceContext,StandardRenderState s)
{
	SetRenderState(deviceContext,standardRenderStates[s]);
}
