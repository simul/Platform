#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Shaders/SL/raytracing_constants.sl"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
namespace simul
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

		// Derived class for Bottom Level Acceleration Structures, which contains geometry to be built.
		class SIMUL_CROSSPLATFORM_EXPORT BottomLevelAccelerationStructure : public BaseAccelerationStructure
		{
		public:
			enum class GeometryType : uint32_t
			{
				UNKNOWN,
				TRIANGLE_MESH,
				AABB
			};

		protected:
			crossplatform::Mesh* mesh = nullptr;
			crossplatform::Raytracing_AABB_SB* aabbBuffer = nullptr;
			GeometryType geometryType = GeometryType::UNKNOWN;

		public:
			BottomLevelAccelerationStructure(crossplatform::RenderPlatform* r);
			virtual ~BottomLevelAccelerationStructure();
			
			void SetMesh(crossplatform::Mesh* mesh);
			void SetAABB(crossplatform::StructuredBuffer<Raytracing_AABB>* aabbBuffer);

			virtual void BuildAccelerationStructureAtRuntime(DeviceContext &deviceContext) { initialized=true; }
		};

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