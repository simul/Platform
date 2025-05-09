#include "DeviceContext.h"
#include "RenderPlatform.h"

using namespace platform;
using namespace crossplatform;

ContextState::ContextState()
{
	memset(viewports,0,8*sizeof(Viewport));
}

ContextState::~ContextState()
{
}

ContextState::ContextState(const ContextState& cs)
{
	operator=(cs);
}

ContextState& ContextState::operator=(const ContextState& cs)
{
	SIMUL_BREAK_ONCE_INTERNAL("Warning: copying contextState is slow");
	last_action_was_compute		=cs.last_action_was_compute;

	indexBuffer					=cs.indexBuffer;
	applyVertexBuffers			=cs.applyVertexBuffers;
	streamoutTargets			=cs.streamoutTargets;
	applyBuffers				=cs.applyBuffers;
	applyStructuredBuffers		=cs.applyStructuredBuffers;
	applyRwStructuredBuffers	=cs.applyRwStructuredBuffers;
	samplerStateOverrides		=cs.samplerStateOverrides;
	textureAssignmentMap		=cs.textureAssignmentMap;
	rwTextureAssignmentMap		=cs.rwTextureAssignmentMap;
	currentEffectPass			=cs.currentEffectPass;
	
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
	viewMask					=cs.viewMask;
	vulkanInsideRenderPass		=cs.vulkanInsideRenderPass;
	return *this;
}

bool ContextState::IsDepthActive() const
{
	crossplatform::RenderState* depthStencilState = currentEffectPass ? currentEffectPass->depthStencilState:nullptr;
	bool depthTest = depthStencilState ? (depthStencilState->desc.depth.test ) : false;
	bool depthWrite = depthStencilState ? ( depthStencilState->desc.depth.write) : false;
	return (depthTest||depthWrite);
}

// Change this from a static construtor to a dynamic construtor so that the memory is allocated by the application rather than the C runtime
long long DeviceContext::GetFrameNumber() const
{

	return frame_number;
}

//! Only RenderPlatform should call this.
void DeviceContext::SetFrameNumber(long long n)
{
	frame_number = n;
}

GraphicsDeviceContext::GraphicsDeviceContext()
	//:cur_backbuffer(0)
{
	deviceContextType=DeviceContextType::GRAPHICS;
	viewStruct.depthTextureStyle=crossplatform::PROJECTION;
	setDefaultRenderTargets(nullptr,nullptr,0,0,0,0);
}

GraphicsDeviceContext::~GraphicsDeviceContext()
{
	if (renderPlatform)
		renderPlatform->EndRenderPass(*(DeviceContext*)this);
}

std::stack<crossplatform::TargetsAndViewport*>& GraphicsDeviceContext::GetFrameBufferStack()
{
	return targetStack;
}

crossplatform::TargetsAndViewport *GraphicsDeviceContext::GetCurrentTargetsAndViewport()
{
	if(GetFrameBufferStack().size())
	{
		crossplatform::TargetsAndViewport *f=GetFrameBufferStack().top();
		if(f)
			return f;
	}
	return &defaultTargetsAndViewport;
}

void GraphicsDeviceContext::setDefaultRenderTargets(const ApiRenderTarget* rt
	,const ApiDepthRenderTarget* dt
	,uint32_t viewportLeft
	,uint32_t viewportTop
	,uint32_t viewportRight
	,uint32_t viewportBottom
	,Texture **texture_targets
	,int num_targets
	,Texture *depth_target
)
{
	//ApiCallCounter=0;
	memset(&defaultTargetsAndViewport, 0, sizeof(defaultTargetsAndViewport));
	defaultTargetsAndViewport.num = num_targets;
	if(texture_targets)
	{
		for(int i=0;i<num_targets;i++)
		{
			defaultTargetsAndViewport.textureTargets[i].texture = texture_targets[i];
			defaultTargetsAndViewport.textureTargets[i].subresource = crossplatform::DefaultSubresourceLayers;
			if (texture_targets[i])
				defaultTargetsAndViewport.rtFormats[i] = texture_targets[i]->GetFormat();
		}
		defaultTargetsAndViewport.depthTarget.texture = depth_target;
		defaultTargetsAndViewport.depthTarget.subresource = crossplatform::DefaultSubresourceLayers;
		defaultTargetsAndViewport.depthTarget.subresource.aspectMask = TextureAspectFlags::DEPTH;
		if (depth_target)
			defaultTargetsAndViewport.depthFormat = depth_target->GetFormat();
	}

	//TargetsAndViewport::num is defaulted to 0, but an valid RTV here should count.
	if (defaultTargetsAndViewport.num == 0 && rt)
		defaultTargetsAndViewport.num = 1;

	defaultTargetsAndViewport.m_rt[0] = rt;
	defaultTargetsAndViewport.m_rt[1] = nullptr;
	defaultTargetsAndViewport.m_rt[2] = nullptr;
	defaultTargetsAndViewport.m_rt[3] = nullptr;
	defaultTargetsAndViewport.m_rt[4] = nullptr;
	defaultTargetsAndViewport.m_rt[5] = nullptr;
	defaultTargetsAndViewport.m_rt[6] = nullptr;
	defaultTargetsAndViewport.m_rt[7] = nullptr;
	defaultTargetsAndViewport.m_dt = dt;

	defaultTargetsAndViewport.viewport.x = viewportLeft;
	defaultTargetsAndViewport.viewport.y = viewportTop;
	defaultTargetsAndViewport.viewport.w = viewportRight - viewportLeft;
	defaultTargetsAndViewport.viewport.h = viewportBottom - viewportTop;
}

MultiviewGraphicsDeviceContext::MultiviewGraphicsDeviceContext()
{
	deviceContextType = DeviceContextType::MULTIVIEW_GRAPHICS;
}

ComputeDeviceContext::ComputeDeviceContext()
{
	deviceContextType = DeviceContextType::COMPUTE;
}

// TODO: this is terrible, let's get rid of it.
void ComputeDeviceContext::UpdateFrameNumbers(DeviceContext& deviceContext)
{
	this->frame_number = renderPlatform->GetFrameNumber();
}