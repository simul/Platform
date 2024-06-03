#include "DeviceContext.h"
#include "RenderPlatform.h"

using namespace simul;
using namespace crossplatform;


ContextState::ContextState()
{
	memset(viewports,0,8*sizeof(Viewport));
}

bool ContextState::IsDepthActive() const
{
	crossplatform::RenderState* depthStencilState = currentEffectPass ? currentEffectPass->depthStencilState : nullptr;
	bool depthTest = depthStencilState ? (depthStencilState->desc.depth.test) : false;
	bool depthWrite = depthStencilState ? (depthStencilState->desc.depth.write) : false;
	return (depthTest || depthWrite);
}

// Change this from a static construtor to a dynamic construtor so that the memory is allocated by the application rather than the C runtime
std::stack<crossplatform::TargetsAndViewport*>& GraphicsDeviceContext::GetFrameBufferStack()
{
	return targetStack;
}

GraphicsDeviceContext::GraphicsDeviceContext():
	cur_backbuffer(0)
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
			defaultTargetsAndViewport.textureTargets[i].texture=texture_targets[i];
			defaultTargetsAndViewport.textureTargets[i].layer=0;
			defaultTargetsAndViewport.textureTargets[i].mip=0;
			if (texture_targets[i])
				defaultTargetsAndViewport.rtFormats[i] = texture_targets[i]->GetFormat();
		}
		defaultTargetsAndViewport.depthTarget.texture=depth_target;
		defaultTargetsAndViewport.depthTarget.layer=0;
		defaultTargetsAndViewport.depthTarget.mip=0;
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
