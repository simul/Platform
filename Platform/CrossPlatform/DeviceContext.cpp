#include "DeviceContext.h"
using namespace simul;
using namespace crossplatform;


ContextState::ContextState()
				:last_action_was_compute(false)
				,effectPassValid(false)
				,vertexBuffersValid(false)
				,constantBuffersValid(false)
				,structuredBuffersValid(false)
				,rwStructuredBuffersValid(false)
				,samplerStateOverridesValid(true)
				,textureAssignmentMapValid(false)
				,rwTextureAssignmentMapValid(false)
				,streamoutTargetsValid(false)
				,textureSlots(0)
				,rwTextureSlots(0)
				,rwTextureSlotsForSB(0)
				,textureSlotsForSB(0)
				,bufferSlots(0)
{
	memset(viewports,0,8*sizeof(Viewport));
}

// Change this from a static construtor to a dynamic construtor so that the memory is allocated by the application rather than the C runtime
std::stack<crossplatform::TargetsAndViewport*>& DeviceContext::GetFrameBufferStack()
{
	return targetStack;
}

DeviceContext::DeviceContext():
	platform_context(0)
	,renderPlatform(0)
	,activeTechnique(0)
	,frame_number(0)
	,cur_backbuffer(0)
{
	viewStruct.depthTextureStyle=crossplatform::PROJECTION;
	setDefaultRenderTargets(nullptr,nullptr,0,0,0,0);
}


void DeviceContext::setDefaultRenderTargets(const ApiRenderTarget* rt,
	const ApiDepthRenderTarget* dt,
	uint32_t viewportLeft,
	uint32_t viewportTop,
	uint32_t viewportRight,
	uint32_t viewportBottom
)
{
	memset(&defaultTargetsAndViewport, 0, sizeof(defaultTargetsAndViewport));
	defaultTargetsAndViewport.num = 1;
	defaultTargetsAndViewport.m_rt[0] = rt;
	defaultTargetsAndViewport.m_rt[1] = nullptr;
	defaultTargetsAndViewport.m_rt[2] = nullptr;
	defaultTargetsAndViewport.m_rt[3] = nullptr;
	defaultTargetsAndViewport.m_dt = dt;
	defaultTargetsAndViewport.viewport.x = viewportLeft;
	defaultTargetsAndViewport.viewport.y = viewportTop;
	defaultTargetsAndViewport.viewport.w = viewportRight - viewportLeft;
	defaultTargetsAndViewport.viewport.h = viewportBottom - viewportTop;
}
