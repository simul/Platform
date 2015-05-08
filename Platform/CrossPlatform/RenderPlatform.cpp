#include "RenderPlatform.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/TextRenderer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
#include "Simul/Platform/CrossPlatform/Material.h"
#include "Effect.h"

using namespace simul;
using namespace crossplatform;


RenderPlatform::RenderPlatform(simul::base::MemoryInterface *m)
	:memoryInterface(m)
	,mirrorY(false)
	,mirrorY2(false)
	,mirrorYText(false)
	,textRenderer(NULL)
	,shaderBuildMode(BUILD_IF_CHANGED)
	,solidEffect(NULL)
	,debugEffect(NULL)
{
	immediateContext.renderPlatform=this;
}

ID3D11Device *RenderPlatform::AsD3D11Device()
{
	return NULL;
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
	solidConstants.RestoreDeviceObjects(this);
	debugConstants.RestoreDeviceObjects(this);
}

void RenderPlatform::InvalidateDeviceObjects()
{
	if(textRenderer)
		textRenderer->InvalidateDeviceObjects();
	for(std::map<StandardRenderState,RenderState*>::iterator i=standardRenderStates.begin();i!=standardRenderStates.end();i++)
		SAFE_DELETE(i->second);
	standardRenderStates.clear();
	SAFE_DELETE(textRenderer);
	solidConstants.InvalidateDeviceObjects();
	debugConstants.InvalidateDeviceObjects();
	SAFE_DELETE(debugEffect);
	SAFE_DELETE(solidEffect);
}

void RenderPlatform::RecompileShaders()
{
	SAFE_DELETE(debugEffect);
	SAFE_DELETE(solidEffect);
	textRenderer->RecompileShaders();
	std::map<std::string, std::string> defines;
	debugEffect=CreateEffect("debug",defines);
	solidEffect=CreateEffect("solid",defines);
	solidConstants.LinkToEffect(solidEffect,"SolidConstants");
	debugConstants.LinkToEffect(debugEffect,"DebugConstants");
}

void RenderPlatform::PushTexturePath(const char *path_utf8)
{
	texturePathsUtf8.push_back(path_utf8);
}
void RenderPlatform::PopTexturePath()
{ 
	texturePathsUtf8.pop_back();
}

void RenderPlatform::PushShaderPath(const char *path_utf8)
{
	shaderPathsUtf8.push_back(std::string(path_utf8)+"/");
}
std::vector<std::string> RenderPlatform::GetShaderPathsUtf8()
{
	return shaderPathsUtf8;
}
void RenderPlatform::SetShaderPathsUtf8(const std::vector<std::string> &pathsUtf8)
{
	shaderPathsUtf8.clear();
	shaderPathsUtf8=pathsUtf8;
}
void RenderPlatform::PopShaderPath()
{
	shaderPathsUtf8.pop_back();
}

const char *RenderPlatform::GetShaderBinaryPath()
{
	return shaderBinaryPathUtf8.c_str();
}

void RenderPlatform::SetShaderBuildMode			(ShaderBuildMode s)
{
	shaderBuildMode=s;
}

ShaderBuildMode RenderPlatform::GetShaderBuildMode() const
{
	return shaderBuildMode;
}

std::vector<std::string> RenderPlatform::GetTexturePathsUtf8()
{
	return texturePathsUtf8;
}

void RenderPlatform::SetShaderBinaryPath(const char *path_utf8)
{
	shaderBinaryPathUtf8 = path_utf8;
	if(shaderBinaryPathUtf8.size())
	{
		char c=shaderBinaryPathUtf8.back();
		if(c!='\\'&&c!='/')
			shaderBinaryPathUtf8+='/';
	}
}

ConstantBuffer<DebugConstants> &RenderPlatform::GetDebugConstantBuffer()
{
	return debugConstants;
}

void RenderPlatform::DrawLine(crossplatform::DeviceContext &deviceContext,const float *startp, const float *endp,const float *colour,float width)
{
	PosColourVertex line_vertices[2];
	line_vertices[0].pos=startp;
	line_vertices[0].colour=colour;
	line_vertices[1].pos=endp;
	line_vertices[1].colour=colour;
	
	DrawLines(deviceContext,line_vertices,2,false,false,false);
}

void RenderPlatform::DrawCircle(DeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill)
{
	PosColourVertex line_vertices[36];
	math::Vector3 direction(dir);
	direction.Unit();
	math::Vector3 z(0,0,1.f);
	math::Vector3 y(0,1.f,0);
	math::Vector3 x=z^direction;
	if(x.Magnitude()>.1f)
		x.Unit();
	else
		x=direction^y;
	x*=radius;
	y=(direction^x);
	int l=0;
	for(int j=0;j<36;j++)
	{
		float angle					=(float(j)/35.0f)*2.0f*3.1415926536f;
		line_vertices[l].pos		=pos+(x*cos(angle)+y*sin(angle));
		line_vertices[l++].colour	=colr;
	}
	DrawLines(deviceContext,line_vertices,35,true,false,false);
}

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext, const double *m, const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj, deviceContext.viewStruct.view, deviceContext.viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj, model, viewproj);
	solidConstants.worldViewProj = modelviewproj;
	solidConstants.world = model;

	solidConstants.lightIrradiance = physicalLightRenderData.lightColour;
	solidConstants.lightDir = physicalLightRenderData.dirToLight;
	solidConstants.Apply(deviceContext);

	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_DEPTH_LESS_EQUAL);

}
void RenderPlatform::DrawCubemap		(DeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float exposure,float gamma)
{
}

void RenderPlatform::DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult,bool blend)
{
	DrawTexture(deviceContext,x1,y1,dx,dy,tex,vec4(mult,mult,mult,0.0f),blend);
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v)
{
	crossplatform::EffectTechnique *tech	=debugEffect->GetTechniqueByName("show_depth");
	if(tex->GetSampleCount()>0)
	{
		tech=debugEffect->GetTechniqueByName("show_depth_ms");
		debugEffect->SetTexture(deviceContext,"imageTextureMS",tex);
	}
	else
	{
		debugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	simul::crossplatform::Frustum frustum=simul::crossplatform::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	debugConstants.debugTanHalfFov=vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov);

	vec4 depthToLinFadeDistParams=crossplatform::GetDepthToDistanceParameters(deviceContext.viewStruct,frustum.farZ);
	debugConstants.debugDepthToLinFadeDistParams=depthToLinFadeDistParams;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	if(mirrorY2)
	{
		y1=(int)viewport.h-y1-dy;
		//dy*=-1;
	}
	{
		if(v)
			debugConstants.viewport=vec4((float)v->x/(float)tex->width,(float)v->y/(float)tex->length,(float)v->w/(float)tex->width,(float)v->h/(float)tex->length);
		else
			debugConstants.viewport=vec4(0.f,0.f,1.f,1.f);
		debugConstants.rect=vec4(2.f*(float)x1/(float)viewport.w-1.f
								,1.f-2.f*(float)(y1+dy)/(float)viewport.h
								,2.f*(float)dx/(float)viewport.w
								,2.f*(float)dy/(float)viewport.h);
		debugConstants.Apply(deviceContext);
		debugEffect->Apply(deviceContext,tech,frustum.reverseDepth?"reverse_depth":"forward_depth");
		DrawQuad(deviceContext);
		debugEffect->UnbindTextures(deviceContext);
		debugEffect->Unapply(deviceContext);
	}
}

void RenderPlatform::Print(DeviceContext &deviceContext,int x,int y,const char *text,const float* colr,const float* bkg)
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
		textRenderer->Render(deviceContext,(float)x,(float)y,(float)viewport.w,(float)h,text,colr,bkg,mirrorYText);
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

void RenderPlatform::SetLayout(DeviceContext &deviceContext,Layout *l)
{
	if(l)
		l->Apply(deviceContext);
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

SamplerState *RenderPlatform::GetOrCreateSamplerStateByName	(const char *name_utf8,simul::crossplatform::SamplerStateDesc *desc)
{
	SamplerState *ss=NULL;
	std::string str(name_utf8);
	if(sharedSamplerStates.find(str)!=sharedSamplerStates.end())
	{
		ss=sharedSamplerStates[str];
	}
	else
	{
		ss=CreateSamplerState(desc);;
		sharedSamplerStates[str]=ss;
	}
	return ss;
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
		vec4 ViewportToTexCoordsXYWH(const Viewport *v,const Texture *t)
		{
		
			vec4 texcXYWH;
			if(v&&t)
			{
				texcXYWH.x=(float)v->x/(float)t->width;
				texcXYWH.y=(float)v->y/(float)t->length;
				texcXYWH.z=(float)v->w/(float)t->width;
				texcXYWH.w=(float)v->h/(float)t->length;
			}
			else
			{
				texcXYWH=vec4(0,0,1.f,1.f);
			}
			return texcXYWH;
		}
		void DrawGrid(crossplatform::DeviceContext &deviceContext,vec3 centrePos,float square_size,float brightness,int numLines)
		{
			// 101 lines across, 101 along.
			numLines++;
			crossplatform::PosColourVertex *lines=new crossplatform::PosColourVertex[2*numLines*2];
			// one metre apart
		//	vec3 cam_pos=crossplatform::GetCameraPosVector(deviceContext.viewStruct.view);
		//	vec3 centrePos(square_size*(int)(cam_pos.x/square_size),square_size*(int)(cam_pos.y/square_size),0);
			crossplatform::PosColourVertex *vertex=lines;
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
				vertex->colour	=vec4(l5,0.5f*l5,0.f,0.2f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(l5,0.1f*l5,0.f,0.2f);
				vertex++;
			}
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				vec3 pos1		=centrePos+vec3(-square_size*halfOffset	,square_size*j	,0);
				vec3 pos2		=centrePos+vec3( square_size*halfOffset	,square_size*j	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(0.f,l10,0.1f*l5,0.2f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(0.f,l25,0.1f*l5,0.2f);
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