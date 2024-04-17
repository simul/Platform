#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/TopLevelAccelerationStructure.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace platform
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT AccelerationStructureManager
		{
			private:
				std::map<int, TopLevelAccelerationStructure*> TLASList;
				TopLevelAccelerationStructure* combinedTLAS;
				bool generatedCombinedAS;

			public:
				AccelerationStructureManager(RenderPlatform* r);
				~AccelerationStructureManager();
				void RestoreDeviceObjects();
				void InvalidateDeviceObjects();
				void GenerateCombinedAccelerationStructure();
				void BuildCombinedAccelerationStructure(crossplatform::DeviceContext& deviceContext);
				TopLevelAccelerationStructure* GetCombinedAccelerationStructure();
				void AddTopLevelAcclelerationStructure(TopLevelAccelerationStructure* newAS, int ID);
				void RemoveTopLevelAcceleratonStructure(int ID);
		};
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif