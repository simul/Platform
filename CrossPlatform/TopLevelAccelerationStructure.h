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
		typedef std::vector<std::pair<crossplatform::BottomLevelAccelerationStructure*, math::Matrix4x4>> BottomLevelAccelerationStructuresAndTransforms;

		// Derived class for Top Level Acceleration Structures, which contain Bottom Level Acceleration Structures to be built.
		class SIMUL_CROSSPLATFORM_EXPORT TopLevelAccelerationStructure : public BaseAccelerationStructure
		{
		public:
			BottomLevelAccelerationStructuresAndTransforms BLASandTransforms;

		public:
			TopLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~TopLevelAccelerationStructure();

			void SetBottomLevelAccelerationStructuresAndTransforms(const BottomLevelAccelerationStructuresAndTransforms& BLASandTransforms);

			BottomLevelAccelerationStructuresAndTransforms* GetBottomLevelAccelerationStructuresAndTransforms();

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext) { initialized = true; }
			
			int GetID();

		private:
			int ID;
		};
	}
}
#pragma once
