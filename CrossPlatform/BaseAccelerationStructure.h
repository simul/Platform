#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Shaders/raytracing_constants.sl"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
		class Mesh;
		typedef StructuredBuffer<Raytracing_AABB> Raytracing_AABB_SB;

		// Base class for Acceleration Structures
		class SIMUL_CROSSPLATFORM_EXPORT BaseAccelerationStructure
		{
		protected:
			crossplatform::RenderPlatform* renderPlatform = nullptr;
			bool initialized = false;

		protected:
			BaseAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~BaseAccelerationStructure();
			virtual void RestoreDeviceObjects();
			virtual void InvalidateDeviceObjects();

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext);

		public:
			bool IsInitialized() const
			{
				return initialized;
			}
		};
	}
}