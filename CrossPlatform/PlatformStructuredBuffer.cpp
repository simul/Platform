
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
#include "Platform/CrossPlatform/Effect.h"

using namespace simul;
using namespace crossplatform;
using namespace std;

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const ShaderResource& shaderResource)
{
#if SIMUL_INTERNAL_CHECKS
	if (!(static_cast<int>(shaderResource.shaderResourceType) & static_cast<int>(simul::crossplatform::ShaderResourceType::RW)))
		SIMUL_INTERNAL_CERR << "ShaderResource Type incorrect" << std::endl;
#endif
	if (shaderResource.slot >= 1000)
		deviceContext.contextState.applyRwStructuredBuffers[shaderResource.slot- 1000] = this;
	else if (shaderResource.slot >= 0)
		deviceContext.contextState.applyRwStructuredBuffers[shaderResource.slot] = this;
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, const ShaderResource& shaderResource)
{
#if SIMUL_INTERNAL_CHECKS
	if (shaderResource.shaderResourceType == simul::crossplatform::ShaderResourceType::UNKNOWN)
		SIMUL_INTERNAL_CERR << "ShaderResource Type has not been set" << std::endl;
#endif
	if (shaderResource.slot >= 0)
		deviceContext.contextState.applyStructuredBuffers[shaderResource.slot] = this;
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const ShaderResource& shaderResource)
{
	ApplyAsUnorderedAccessView(deviceContext,shaderResource);
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const ShaderResource& shaderResource)
{
	Apply(deviceContext,shaderResource);
}