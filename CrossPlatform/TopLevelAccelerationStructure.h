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
		struct InstanceDesc
		{
			crossplatform::BottomLevelAccelerationStructure* blas = nullptr;
			math::Matrix4x4 transform;
			uint32_t contributionToHitGroupIdx = 0;

			InstanceDesc() = delete;
			InstanceDesc(crossplatform::BottomLevelAccelerationStructure* BLAS, math::Matrix4x4 Transform, uint32_t ContributionToHitGroupIdx = 0)
				: blas(BLAS), transform(Transform), contributionToHitGroupIdx(ContributionToHitGroupIdx) {}
			~InstanceDesc() = default;
		};

		typedef std::vector<InstanceDesc> InstanceDescs;

		// Derived class for Top Level Acceleration Structures, which contain Bottom Level Acceleration Structures to be built.
		class SIMUL_CROSSPLATFORM_EXPORT TopLevelAccelerationStructure : public BaseAccelerationStructure
		{
		public:
			InstanceDescs _instanceDescs;

		protected:
			uint32_t instanceCount = 0;

		private:
			int ID;

		public:
			TopLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~TopLevelAccelerationStructure();
			void RestoreDeviceObjects() override {};
			void InvalidateDeviceObjects() override {};

			void SetInstanceDescs(const InstanceDescs& instanceDescs);
			InstanceDescs* GetInstanceDescs();

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext& deviceContext) { initialized = true; }
			
			inline int GetID() { return ID; }
		};
	}
}