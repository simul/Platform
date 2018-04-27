#define NOMINMAX
#include "RenderPlatform.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/TextRenderer.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
#include "Simul/Platform/CrossPlatform/Material.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "Simul/Platform/CrossPlatform/GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Effect.h"
#include <algorithm>
#ifdef _MSC_VER
#define isinf !_finite
#else
#include <cmath>		// for isinf()
#endif
using namespace simul;
using namespace crossplatform;

ContextState& ContextState::operator=(const ContextState& cs)
{
	std::cerr<<"Warning: copying contextState is slow."<<std::endl;

	last_action_was_compute		=cs.last_action_was_compute;

	applyVertexBuffers			=cs.applyVertexBuffers;
	streamoutTargets			=cs.streamoutTargets;
	applyBuffers				=cs.applyBuffers;
	applyStructuredBuffers		=cs.applyStructuredBuffers;
	applyRwStructuredBuffers	=cs.applyRwStructuredBuffers;
	samplerStateOverrides		=cs.samplerStateOverrides;
	textureAssignmentMap		=cs.textureAssignmentMap;
	rwTextureAssignmentMap		=cs.rwTextureAssignmentMap;
	currentEffectPass			=cs.currentEffectPass;
	currentTechnique			=cs.currentTechnique;
	currentEffect				=cs.currentEffect;
	effectPassValid				=cs.effectPassValid;
	vertexBuffersValid			=cs.vertexBuffersValid;
	constantBuffersValid		=cs.constantBuffersValid;
	structuredBuffersValid		=cs.structuredBuffersValid;
	rwStructuredBuffersValid	=cs.rwStructuredBuffersValid;
	samplerStateOverridesValid	=cs.samplerStateOverridesValid;
	textureAssignmentMapValid	=cs.textureAssignmentMapValid;
	rwTextureAssignmentMapValid	=cs.rwTextureAssignmentMapValid;
	streamoutTargetsValid		=cs.streamoutTargetsValid;
	textureSlots				=cs.textureSlots;
	rwTextureSlots				=cs.rwTextureSlots;
	rwTextureSlotsForSB			=cs.rwTextureSlotsForSB;
	textureSlotsForSB			=cs.textureSlotsForSB;
	bufferSlots					=cs.bufferSlots;
	return *this;
}

RenderPlatform::RenderPlatform(simul::base::MemoryInterface *m)
	:mirrorY(false)
	,mirrorY2(false)
	,mirrorYText(false)
	,solidEffect(NULL)
	,copyEffect(NULL)
	,memoryInterface(m)
	,shaderBuildMode(BUILD_IF_CHANGED)
	,debugEffect(NULL)
	,textured(NULL)
	,untextured(nullptr)
	,showVolume(NULL)
#ifdef _XBOX_ONE
	,can_save_and_restore(false)
#else
	,can_save_and_restore(true)
#endif
	,textRenderer(NULL)
{
	immediateContext.renderPlatform=this;
	gpuProfiler=new GpuProfiler;
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
	delete gpuProfiler;

	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		Material *mat = i->second;
		delete mat;
	}
	materials.clear();
}

crossplatform::ContextState *RenderPlatform::GetContextState(crossplatform::DeviceContext &deviceContext)
{
	return &deviceContext.contextState;
}

ID3D12GraphicsCommandList* RenderPlatform::AsD3D12CommandList()
{
	return NULL;
}

ID3D12Device* RenderPlatform::AsD3D12Device()
{
	return NULL;
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
	desc.blend.RenderTarget[0].blendOperation		=BLEND_OP_NONE;
	desc.blend.RenderTarget[0].RenderTargetWriteMask=15;
	desc.blend.RenderTarget[0].SrcBlend				=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlend			=crossplatform::BLEND_ZERO;
	desc.blend.RenderTarget[0].SrcBlendAlpha		=crossplatform::BLEND_ONE;
	desc.blend.RenderTarget[0].DestBlendAlpha		=crossplatform::BLEND_ZERO;
	RenderState *opaque=CreateRenderState(desc);
	standardRenderStates[STANDARD_OPAQUE_BLENDING]=opaque;

	desc.blend.RenderTarget[0].blendOperation		=BLEND_OP_ADD;
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
	
	
	debugConstants.RestoreDeviceObjects(this);
	
	gpuProfiler->RestoreDeviceObjects(this);
	textureQueryResult.InvalidateDeviceObjects();
	textureQueryResult.RestoreDeviceObjects(this,1,true);

	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		crossplatform::Material *mat = (crossplatform::Material*)(i->second);
		mat->SetEffect(solidEffect);

	}
}

void RenderPlatform::InvalidateDeviceObjects()
{
	gpuProfiler->InvalidateDeviceObjects();
	if(textRenderer)
		textRenderer->InvalidateDeviceObjects();
	for(std::map<StandardRenderState,RenderState*>::iterator i=standardRenderStates.begin();i!=standardRenderStates.end();i++)
		SAFE_DELETE(i->second);
	standardRenderStates.clear();
	SAFE_DELETE(textRenderer);
	debugConstants.InvalidateDeviceObjects();
	SAFE_DELETE(debugEffect);
	SAFE_DELETE(solidEffect);
	SAFE_DELETE(copyEffect);
	textured=NULL;
	untextured=nullptr;
	showVolume=NULL;
	textureQueryResult.InvalidateDeviceObjects();



	for (auto i = materials.begin(); i != materials.end(); i++)
	{
		Material *mat = i->second;
		mat->InvalidateDeviceObjects();
	}
}

void RenderPlatform::RecompileShaders()
{
	
	SAFE_DELETE(debugEffect);
	SAFE_DELETE(solidEffect);
	SAFE_DELETE(copyEffect);
	
	textRenderer->RecompileShaders();
	
	std::map<std::string, std::string> defines;
	debugEffect=CreateEffect("debug",defines);
	if(debugEffect)
	{
		textured=debugEffect->GetTechniqueByName("textured");
		untextured=debugEffect->GetTechniqueByName("untextured");
		showVolume=debugEffect->GetTechniqueByName("show_volume");
		volumeTexture=debugEffect->GetShaderResource("volumeTexture");
		imageTexture=debugEffect->GetShaderResource("imageTexture");
	}		
	solidEffect=CreateEffect("solid",defines);
	copyEffect=CreateEffect("copy",defines);
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

void RenderPlatform::PushRenderTargets(DeviceContext &deviceContext, TargetsAndViewport *tv)
{
	deviceContext.GetFrameBufferStack().push(tv);
}

void RenderPlatform::PopRenderTargets(DeviceContext &deviceContext)
{
	deviceContext.GetFrameBufferStack().pop();
}

void RenderPlatform::LatLongTextureToCubemap(DeviceContext &deviceContext,Texture *destination,Texture *source)
{
	debugEffect->SetTexture(deviceContext,"imageTexture",source);
	debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget2DArray",destination);
	debugEffect->Apply(deviceContext,"lat_long_to_cubemap",0);
	int D=6;
	int W=(destination->width+3)/4;
	DispatchCompute(deviceContext,W,W,D);
	debugEffect->Unapply(deviceContext);
	debugEffect->UnbindTextures(deviceContext);
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
void RenderPlatform::BeginEvent			(DeviceContext &deviceContext,const char *name){}
void RenderPlatform::EndEvent			(DeviceContext &deviceContext){}


void RenderPlatform::Clear				(DeviceContext &deviceContext,vec4 colour_rgba)
{
	crossplatform::EffectTechnique *clearTechnique=clearTechnique=debugEffect->GetTechniqueByName("clear");
	debugConstants.debugColour=colour_rgba;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,clearTechnique,0);
	DrawQuad(deviceContext);
	debugEffect->Unapply(deviceContext);
}

void RenderPlatform::ClearTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,const vec4& colour)
{
	// Silently return if not initialized
	if(!texture->IsValid())
		return;
	bool cleared				= false;
	debugConstants.debugColour=colour;
	debugConstants.texSize=uint4(texture->width,texture->length,texture->depth,1);
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	// Clear the texture: how we do this depends on what kind of texture it is.
	// Does it have rendertargets? We can clear each of these in turn.
	if(texture->HasRenderTargets()&&texture->arraySize)
	{
		int total_num=texture->arraySize*(texture->IsCubemap()?6:1);
		for(int i=0;i<total_num;i++)
		{
			for(int j=0;j<texture->mips;j++)
			{
				texture->activateRenderTarget(deviceContext,i,j);
				debugEffect->Apply(deviceContext,"clear",0);
					DrawQuad(deviceContext);
				debugEffect->Unapply(deviceContext);
				texture->deactivateRenderTarget(deviceContext);
			}
		}
		debugEffect->UnbindTextures(deviceContext);
		cleared = true;
	}
	// Otherwise, is it computable? We can set the colour value with a compute shader.
	// Is it mappable? We can set the colour from CPU memory.
	else if (texture->IsComputable() && !cleared)
	{
		int a=texture->NumFaces();
		if(a==0)
			a=1;
#if 1
		for(int i=0;i<a;i++)
		{
			int w=texture->width;
			int l=texture->length;
			int d=texture->depth;
			for(int j=0;j<texture->mips;j++)
			{
				const char *techname="compute_clear";
				int W=(w+4-1)/4;
				int L=(l+4-1)/4;
				int D=(d+4-1)/4;
				if (texture->dim == 2 && texture->NumFaces()>1)
				{
					W=(w+8-1)/8;
					L=(l+8-1)/8;
					D = d;
					techname = "compute_clear_2d_array";
					if(texture->GetFormat()==PixelFormat::RGBA_8_UNORM||texture->GetFormat()==PixelFormat::RGBA_8_UNORM_SRGB)
					{
						techname="compute_clear_2d_array_u8";
						debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget2DArrayU8",texture,i);
					}
					else
					{
						debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget2DArray",texture,i);
					}
				}
				else if(texture->dim==2)
				{
					W=(w+8-1)/8;
					L=(l+8-1)/8;
					debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget",texture,i,j);
					D=1;
				}
				else if(texture->dim==3)
				{
					if(texture->GetFormat()==PixelFormat::RGBA_8_UNORM||texture->GetFormat()==PixelFormat::RGBA_8_UNORM_SRGB)
					{
						techname = "compute_clear_3d_u8";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3DU8", texture, i);
					}
					else
					{
					techname="compute_clear_3d";
						debugEffect->SetUnorderedAccessView(deviceContext, "FastClearTarget3D", texture, i);
					}
				}
				else
				{
					SIMUL_CERR_ONCE<<("Can't clear texture dim.\n");
				}
				debugEffect->Apply(deviceContext,techname,0);
				if(deviceContext.platform_context!=immediateContext.platform_context)
				{
					DispatchCompute(deviceContext,W,L,D);
				}
				else
				{
#if 1//ndef __ORBIS__
					DispatchCompute(deviceContext,W,L,D);
#endif
				}
				debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget",nullptr);
				debugEffect->SetUnorderedAccessView(deviceContext,"FastClearTarget3D",nullptr);
				w/=2;
				l/=2;
				d/=2;
				debugEffect->Unapply(deviceContext);
			}
		}
#endif
		debugEffect->UnbindTextures(deviceContext);
	}
	//Finally, is the texture a depth stencil? In this case, we call the specified API's clear function in order to clear it.
	else if (texture->IsDepthStencil() && !cleared)
	{
		texture->ClearDepthStencil(deviceContext, colour.x, 0);
	}
	else
	{
		SIMUL_CERR_ONCE<<("No method was found to clear this texture.\n");
	}
}

void RenderPlatform::GenerateMips(DeviceContext &deviceContext,Texture *t,bool wrap,int array_idx)
{
	if(!t||!t->IsValid())
		return;
	debugEffect->Apply(deviceContext,debugEffect->GetTechniqueByName("copy_2d"),"wrap");
	for(int i=0;i<t->mips-1;i++)
	{
		int m0=i,m1=i+1;
		debugEffect->SetTexture(deviceContext,"imageTexture",t,array_idx,m0);
		{
			t->activateRenderTarget(deviceContext,array_idx,m1);
			DrawQuad(deviceContext);
			t->deactivateRenderTarget(deviceContext);
		}
	}
	debugEffect->Unapply(deviceContext);
}

vec4 RenderPlatform::TexelQuery(DeviceContext &deviceContext,int query_id,uint2 pos,Texture *texture)
{
	if((int)query_id>=textureQueryResult.count)
	{
		textureQueryResult.InvalidateDeviceObjects();
		textureQueryResult.RestoreDeviceObjects(this,(int)query_id+1,true);
	}
	debugConstants.queryPos=pos;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	textureQueryResult.ApplyAsUnorderedAccessView(deviceContext,debugEffect,debugEffect->GetShaderResource("textureQueryResults"));
	debugEffect->SetTexture(deviceContext,"imageTexture",texture);
	debugEffect->Apply(deviceContext,"texel_query",0);
	DispatchCompute(deviceContext,textureQueryResult.count,1,1);
	debugEffect->Unapply(deviceContext);
	textureQueryResult.CopyToReadBuffer(deviceContext);
	const vec4 *result=textureQueryResult.OpenReadBuffer(deviceContext);
	vec4 r;
	if(result)
		r=result[query_id];
	textureQueryResult.CloseReadBuffer(deviceContext);
	return r;
}

std::vector<std::string> RenderPlatform::GetTexturePathsUtf8()
{
	return texturePathsUtf8;
}

simul::base::MemoryInterface *RenderPlatform::GetMemoryInterface()
{
	return memoryInterface;
}

void RenderPlatform::SetMemoryInterface(simul::base::MemoryInterface *m)
{
	// TODO: shutdown old memory, test for leaks at RenderPlatform shutdown.
	memoryInterface=m;
}

crossplatform::Effect *RenderPlatform::GetDebugEffect()
{
	return debugEffect;
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
	line_vertices[0].pos= vec3(startp)-deviceContext.viewStruct.cam_pos;
	line_vertices[0].colour=colour;
	line_vertices[1].pos= vec3(endp)-deviceContext.viewStruct.cam_pos;
	line_vertices[1].colour=colour;

	DrawLines(deviceContext,line_vertices,2,true,false,true);
}

static float length(const vec3 &u)
{
	float size=u.x*u.x+u.y*u.y+u.z*u.z;
	return sqrt(size);
}
void RenderPlatform::DrawCircle(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill)
{
	vec3 pos=GetCameraPosVector(deviceContext.viewStruct.view);
	vec3 d=dir;
	d/=length(d);
	pos+=d;
	float radius=rads;
	DrawCircle(deviceContext,pos,dir,radius,colr,fill);
}

void RenderPlatform::DrawCircle(DeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill)
{
	PosColourVertex line_vertices[37];
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
	DrawLines(deviceContext,line_vertices,36,true,false,false);
}

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext, const double *m, const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{

	simul::crossplatform::Frustum frustum = simul::crossplatform::GetFrustumFromProjectionMatrix((const float*)deviceContext.viewStruct.proj);
	SetStandardRenderState(deviceContext, frustum.reverseDepth ? crossplatform::STANDARD_DEPTH_GREATER_EQUAL : crossplatform::STANDARD_DEPTH_LESS_EQUAL);
}


crossplatform::Material *RenderPlatform::GetOrCreateMaterial(const char *name)
{
	auto i = materials.find(name);
	if (i != materials.end())
		return i->second;
	crossplatform::Material *mat = new crossplatform::Material(name);
	mat->SetEffect(solidEffect);
	materials[name]=mat;
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new Mesh;
}

void RenderPlatform::DrawLatLongSphere(DeviceContext &deviceContext,int lat, int longt,vec3 origin,float radius,vec4 colour)
{
	Viewport viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 &view=deviceContext.viewStruct.view;
	math::Matrix4x4 &proj = deviceContext.viewStruct.proj;
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	//float tan_x=1.0f/proj(0, 0);
	//float tan_y=1.0f/proj(1, 1);
	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=debugEffect->GetTechniqueByName("draw_lat_long_sphere");
	
	debugConstants.latitudes		=lat;
	debugConstants.longitudes		=longt;
	debugConstants.radius			=radius;
	debugConstants.multiplier		=colour;
	debugConstants.debugViewDir		=view_dir;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,tech,0);

	SetTopology(deviceContext,LINESTRIP);
	// first draw the latitudes:
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);
	// first draw the longitudes:
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	debugEffect->Unapply(deviceContext);
}

void RenderPlatform::DrawQuadOnSphere(DeviceContext &deviceContext,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour)
{
	Viewport viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=debugEffect->GetTechniqueByName("draw_quad_on_sphere");

	debugConstants.quaternion		=orient_quat;
	debugConstants.radius			=sph_rad;
	debugConstants.sideview			=qsize;
	debugConstants.debugColour		=colour;
	debugConstants.debugViewDir		=view_dir;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);

	debugEffect->Apply(deviceContext,tech,0);

	SetTopology(deviceContext,LINELIST);
	Draw(deviceContext,16, 0);

	debugEffect->Unapply(deviceContext);
}


void RenderPlatform::DrawTextureOnSphere(DeviceContext &deviceContext,crossplatform::Texture *t,vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour)
{
	Viewport viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=debugEffect->GetTechniqueByName("draw_texture_on_sphere");
	debugEffect->SetTexture(deviceContext,imageTexture,t);
	debugConstants.quaternion		=orient_quat;
	debugConstants.radius			=sph_rad;
	debugConstants.sideview			=qsize;
	debugConstants.debugColour		=colour;
	debugConstants.debugViewDir		=view_dir;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);

	SetTopology(deviceContext,TRIANGLESTRIP);
	debugEffect->Apply(deviceContext,tech,0);

	//DrawQuad(deviceContext);
	Draw(deviceContext,16, 0);

	debugEffect->Unapply(deviceContext);
}

void RenderPlatform::DrawCircleOnSphere(DeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float rad,float sph_rad, vec4 colour)
{
	Viewport viewport=GetViewport(deviceContext,0);
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	const math::Matrix4x4 &proj = deviceContext.viewStruct.proj;

	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();

	world._41=origin.x;
	world._42=origin.y;
	world._43=origin.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	vec3 view_dir;
	math::Vector3 cam_pos;
	crossplatform::GetCameraPosVector(deviceContext.viewStruct.view,(float*)&cam_pos,(float*)&view_dir);
	crossplatform::EffectTechnique*		tech		=debugEffect->GetTechniqueByName("draw_circle_on_sphere");

	debugConstants.quaternion		=orient_quat;
	debugConstants.radius			=sph_rad;
	debugConstants.sideview			=rad;
	debugConstants.debugColour		=colour;
	debugConstants.debugViewDir		=view_dir;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);

	debugEffect->Apply(deviceContext,tech,0);

	SetTopology(deviceContext, LINESTRIP);
	Draw(deviceContext,32, 0);

	debugEffect->Unapply(deviceContext);
}
void RenderPlatform::DrawCubemap(DeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float size,float exposure,float gamma,float displayLod)
{
	//unsigned int num_v=0;

	Viewport oldv=GetViewport(deviceContext,0);
	
	// Setup the viewport for rendering.
	Viewport viewport;
	viewport.w		=(int)(oldv.w*size);
	viewport.h		=(int)(oldv.h*size);
	viewport.zfar	=1.0f;
	viewport.znear	=0.0f;
	viewport.x		=(int)(0.5f*(1.f+offsetx)*oldv.w-viewport.w/2);
	viewport.y		=(int)(0.5f*(1.f-offsety)*oldv.h-viewport.h/2);
	SetViewports(deviceContext,1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=crossplatform::Camera::MakeDepthReversedProjectionMatrix(1.f,(float)viewport.h/(float)viewport.w,0.1f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.ResetToUnitMatrix();
	float tan_x=1.0f/proj(0, 0);
	//float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float sizem=3.f;
	float d=2.0f*sizem/size_req;
	simul::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	simul::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._41=offs.x;
	world._42=offs.y;
	world._43=offs.z;
	crossplatform::MakeWorldViewProjMatrix(wvp,world,view,proj);
	debugConstants.debugWorldViewProj=wvp;
	debugConstants.displayLod=displayLod;
	crossplatform::EffectTechnique*		tech		=debugEffect->GetTechniqueByName("draw_cubemap_sphere");
	debugEffect->SetTexture(deviceContext,"cubeTexture",cubemap);
	static float rr=6.f;
	debugConstants.latitudes		=16;
	debugConstants.longitudes		=32;
	debugConstants.radius			=rr;
	debugConstants.multiplier		=vec4(exposure,exposure,exposure,0.0f);
	debugConstants.debugGamma		=gamma;
	debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
	debugEffect->Apply(deviceContext,tech,0);

	SetTopology(deviceContext,TRIANGLESTRIP);
	Draw(deviceContext, (debugConstants.longitudes+1)*(debugConstants.latitudes+1)*2, 0);

	debugEffect->SetTexture(deviceContext, "cubeTexture", NULL);
	debugEffect->Unapply(deviceContext);
	SetViewports(deviceContext,1,&oldv);
}

void RenderPlatform::PrintAt3dPos(crossplatform::DeviceContext &deviceContext,const float *p,const char *text,const float* colr,const float* bkg,int offsetx,int offsety,bool centred)
{
	//unsigned int num_v=1;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	mat4 wvp;
	if(centred)
		crossplatform::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	else
		crossplatform::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	static bool tr=false;
	if(tr)
		wvp.transpose();
	vec4 clip_pos;
	clip_pos=wvp*vec4(p[0],p[1],p[2],1.0f);
	if(clip_pos.w<0)
		return;
	clip_pos.x/=clip_pos.w;
	clip_pos.y/=clip_pos.w;
	static bool ml=true;
	vec2 pos;
	pos.x=(1.0f+clip_pos.x)/2.0f;
	if(mirrorY)
		pos.y=(1.0f+clip_pos.y)/2.0f;
	else
		pos.y=(1.0f-clip_pos.y)/2.0f;
	if(ml)
	{
		pos.x*=viewport.w;
		pos.y*=viewport.h;
	}

	Print(deviceContext,(int)pos.x+offsetx,(int)pos.y+offsety,text,colr,bkg);
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend,float gamma,bool debug)
{
	static int level=0;
	static int lod=0;
	static int frames=100;
	static int count=frames;
	float displayLod=0.0f;
	if(debug)
	{
		count--;
		if(!count)
		{
			lod++;
			count=frames;
		}
		if(tex)
		{
			int m=tex->GetMipCount();
			displayLod=float((lod%(m?m:1)));
			if(!tex->IsValid())
				tex=nullptr;
		}
	}
	
	debugConstants.debugGamma=gamma;
	debugConstants.multiplier=mult;
	debugConstants.displayLod=displayLod;
	debugConstants.displayLevel=0;
	crossplatform::EffectTechnique *tech=textured;
	if(tex&&tex->GetDimension()==3)
	{
		tech=showVolume;
		debugEffect->SetTexture(deviceContext,volumeTexture,tex);
	}
	else if(tex&&tex->IsCubemap())
	{
		if(tex->arraySize>1)
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap_array");
			debugEffect->SetTexture(deviceContext,"cubeTextureArray",tex);
			if(debug)
			{
			static char c=0;
			static char cc=20;
			c--;
			if(!c)
			{
				level++;
				c=cc;
			}
			
			debugConstants.displayLevel=(float)(level%std::max(1,tex->arraySize));
		}
		}
		else
		{
			tech=debugEffect->GetTechniqueByName("show_cubemap");
			debugEffect->SetTexture(deviceContext,"cubeTexture",tex);
			debugConstants.displayLevel=0;
		}
	}
	else if(tex)
	{
		debugEffect->SetTexture(deviceContext,imageTexture,tex);
	}
	else
	{
		tech=untextured;
	}
	DrawQuad(deviceContext,x1,y1,dx,dy,debugEffect,tech,"noblend");
	debugEffect->UnbindTextures(deviceContext);
	if(debug)
	{
	vec4 white(1.0, 1.0, 1.0, 1.0);
	vec4 semiblack(0, 0, 0, 0.5);
	char txt[]="0";
	if(tex&&tex->GetMipCount()>1&&lod>0&&lod<10)
	{
		txt[0]='0'+lod;
		Print(deviceContext,x1,y1,txt,white,semiblack);
	}
	if(tex&&tex->arraySize>1)
	{
		int l=level%tex->arraySize;
		if(l<10)
			txt[0]='0'+(l);
		Print(deviceContext,x1,y1,txt,white,semiblack);
	}
}
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect
	,crossplatform::EffectTechnique *technique,const char *pass)
{
	//unsigned int num_v=1;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	vec4 r(2.f*(float)x1/(float)viewport.w-1.f
		,1.f-2.f*(float)(y1+dy)/(float)viewport.h
		,2.f*(float)dx/(float)viewport.w
		,2.f*(float)dy/(float)viewport.h);
	if(mirrorY)
		y1=(int)viewport.h-y1-dy;
	debugConstants.LinkToEffect(effect,"DebugConstants");
	debugConstants.rect=r;
	effect->SetConstantBuffer(deviceContext,&debugConstants);
	effect->Apply(deviceContext,technique,pass);
	DrawQuad(deviceContext);
	effect->Unapply(deviceContext);
}

void RenderPlatform::DrawTexture(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult,bool blend,float gamma)
{
	DrawTexture(deviceContext,x1,y1,dx,dy,tex,vec4(mult,mult,mult,0.0f),blend,gamma);
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v
	,const float *proj)
{
	crossplatform::EffectTechnique *tech	=debugEffect->GetTechniqueByName("show_depth");
	if(tex&&tex->GetSampleCount()>0)
	{
		tech=debugEffect->GetTechniqueByName("show_depth_ms");
		debugEffect->SetTexture(deviceContext,"imageTextureMS",tex);
	}
	else if(tex&&tex->IsCubemap())
	{
		tech=debugEffect->GetTechniqueByName("show_depth_cube");
		debugEffect->SetTexture(deviceContext,"cubeTexture",tex);
	}
	else
	{
		debugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	if(!proj)
		proj=deviceContext.viewStruct.proj;
	simul::crossplatform::Frustum frustum=simul::crossplatform::GetFrustumFromProjectionMatrix(proj);
	debugConstants.debugTanHalfFov=(frustum.tanHalfFov);

	vec4 depthToLinFadeDistParams=crossplatform::GetDepthToDistanceParameters(deviceContext.viewStruct,isinf(frustum.farZ)?300000.0f:frustum.farZ);
	debugConstants.debugDepthToLinFadeDistParams=depthToLinFadeDistParams;
	crossplatform::Viewport viewport=GetViewport(deviceContext,0);
	if(mirrorY2)
	{
		y1=(int)viewport.h-y1-dy;
		//dy*=-1;
	}
	{
		if(tex&&v)
			debugConstants.viewport=vec4((float)v->x/(float)tex->width,(float)v->y/(float)tex->length,(float)v->w/(float)tex->width,(float)v->h/(float)tex->length);
		else
			debugConstants.viewport=vec4(0.f,0.f,1.f,1.f);
		debugConstants.rect=vec4(2.f*(float)x1/(float)viewport.w-1.f
								,1.f-2.f*(float)(y1+dy)/(float)viewport.h
								,2.f*(float)dx/(float)viewport.w
								,2.f*(float)dy/(float)viewport.h);
		debugEffect->SetConstantBuffer(deviceContext,&debugConstants);
		debugEffect->Apply(deviceContext,tech,frustum.reverseDepth?"reverse_depth":"forward_depth");
		DrawQuad(deviceContext);
		debugEffect->UnbindTextures(deviceContext);
		debugEffect->Unapply(deviceContext);
	}
}

void RenderPlatform::Draw2dLine(DeviceContext &deviceContext,vec2 pos1,vec2 pos2,vec4 colour)
{
	PosColourVertex pts[2];
	pts[0].pos=pos1;
	pts[0].pos.z=0;
	pts[0].colour=colour;
	pts[1].pos=pos2;
	pts[1].pos.z=0;
	pts[1].colour=colour;
	Draw2dLines(deviceContext,pts,2,false);
}

void RenderPlatform::Print(DeviceContext &deviceContext,int x,int y,const char *text,const float* colr,const float* bkg)
{
	SIMUL_COMBINED_PROFILE_START(deviceContext, "text")
	static float clr[]={1.f,1.f,0.f,1.f};
	static float black[]={0.f,0.f,0.f,0.0f};
	if(!colr)
		colr=clr;
	if(!bkg)
		bkg=black;
	//unsigned int num_v=1;
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
	SIMUL_COMBINED_PROFILE_END(deviceContext)
}
		
crossplatform::Viewport RenderPlatform::PlatformGetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	crossplatform::Viewport v;
	memset(&v,0,sizeof(v));
	return v;
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,const crossplatform::Viewport *vps)
{
	if(deviceContext.GetFrameBufferStack().size())
	{
		crossplatform::TargetsAndViewport *f=deviceContext.GetFrameBufferStack().top();
		if(f)
			f->viewport=*vps;
	}
	else
	{
		deviceContext.defaultTargetsAndViewport.viewport=*vps;
	}
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	crossplatform::Viewport v;
	if(deviceContext.GetFrameBufferStack().size())
	{
		v= deviceContext.GetFrameBufferStack().top()->viewport;
	}
	else
	{
		if(deviceContext.defaultTargetsAndViewport.viewport.w*deviceContext.defaultTargetsAndViewport.viewport.h==0)
		{
			SIMUL_BREAK_ONCE("The default viewport is empty. Please call deviceContext.setDefaultRenderTargets() at least once on initialization or at the start of the trueSKY frame.");
		}
		v= deviceContext.defaultTargetsAndViewport.viewport;
	}
	return v;
}

void RenderPlatform::SetLayout(DeviceContext &deviceContext,Layout *l)
{
	if(l)
		l->Apply(deviceContext);
}

crossplatform::GpuProfiler		*RenderPlatform::GetGpuProfiler()
{
	return gpuProfiler;
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
		SamplerState *s=sharedSamplerStates[str];
		if(!desc||s->default_slot==desc->slot||s->default_slot==-1||desc->slot==-1)
		{
			if(desc&&desc->slot!=-1)
				s->default_slot=desc->slot;
			ss=s;
		}
		else
		{
			SIMUL_COUT<<"Simul fx: the sampler state "<<name_utf8<<" was declared with slots "<<s->default_slot<<" and "<<desc->slot
				<<"\nTherefore it will have no default slot."<<std::endl;
			s->default_slot=-1;
			ss=s;
		}
	}
	else
	{
		ss=CreateSamplerState(desc);
		sharedSamplerStates[str]=ss;
	}
	return ss;
}

Effect *RenderPlatform::GetEffect(const char *filename_utf8)
{
	auto i = effects.find(filename_utf8);
	if (i == effects.end())
		return nullptr;
	return i->second;
}

Effect *RenderPlatform::CreateEffect(const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	Effect *e=CreateEffect(filename_utf8,defines);
	return e;
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	std::string fn(filename_utf8);
	crossplatform::Effect *e=CreateEffect();
	effects[fn] = e;
	e->Load(this,filename_utf8,defines);
	e->SetName(filename_utf8);
	return e;
}

crossplatform::Shader *RenderPlatform::EnsureShader(const char *filenameUtf8, crossplatform::ShaderType t)
{
	std::string name(filenameUtf8);
	if (shaders.find(name) != shaders.end())
		return shaders[name];
	Shader *s = CreateShader();
	shaders[name] = s;
	s->load(this, filenameUtf8, t);
	return s;
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