#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"

#include "SimulDirectX12.h"
#include <vector>
namespace simul
{
	namespace dx12
	{
		class RenderPlatform:
			public crossplatform::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
		};
	}
}