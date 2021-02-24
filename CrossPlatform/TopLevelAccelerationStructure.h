#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Shaders/SL/raytracing_constants.sl"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
#include "BaseAccelerationStructure.h"
#include "BottomLevelAccelerationStructure.h"
namespace simul
{
	namespace crossplatform
	{
		// Derived class for Top Level Acceleration Structures, which contain Bottom Level Acceleration Structures to be built.
		class SIMUL_CROSSPLATFORM_EXPORT TopLevelAccelerationStructure : public BaseAccelerationStructure
		{
		public:
			typedef std::vector<std::pair<crossplatform::BottomLevelAccelerationStructure*, math::Matrix4x4>> BottomLevelAccelerationStructuresAndTransforms;
			BottomLevelAccelerationStructuresAndTransforms BLASandTransforms;

		public:
			TopLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~TopLevelAccelerationStructure();

			void SetBottomLevelAccelerationStructuresAndTransforms(const BottomLevelAccelerationStructuresAndTransforms& BLASandTransforms);

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext) { initialized = true; }
		};
	}
}
#pragma once
