#define NOMINMAX
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
#include "Platform/CrossPlatform/Effect.h"

using namespace simul;
using namespace crossplatform;
using namespace std;

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const ShaderResource& shaderResource)
{
	if (shaderResource.slot >= 0)
		deviceContext.contextState.applyRwStructuredBuffers[shaderResource.slot] = this;
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const ShaderResource& shaderResource)
{
	if (shaderResource.slot >= 0)
		deviceContext.contextState.applyStructuredBuffers[shaderResource.slot] = this;
}