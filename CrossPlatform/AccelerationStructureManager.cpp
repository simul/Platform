#include "AccelerationStructureManager.h"

using namespace simul;
using namespace crossplatform;

AccelerationStructureManager::AccelerationStructureManager(RenderPlatform* r)
	:generatedCombinedAS(false)
{
	combinedTLAS = r->CreateTopLevelAccelerationStructure();
}

AccelerationStructureManager::~AccelerationStructureManager()
{
	InvalidateDeviceObjects();
}

void AccelerationStructureManager::RestoreDeviceObjects()
{

}

void AccelerationStructureManager::InvalidateDeviceObjects()
{
	TLASList.clear();
}

void AccelerationStructureManager::GenerateCombinedAccelerationStructure()
{
	BottomLevelAccelerationStructuresAndTransforms* tempBLASList = new BottomLevelAccelerationStructuresAndTransforms;

	for (auto TLAS : TLASList)
	{
		BottomLevelAccelerationStructuresAndTransforms* temp = TLAS.second->GetBottomLevelAccelerationStructuresAndTransforms();
		tempBLASList->insert(tempBLASList->end(), temp->begin(), temp->end());
	}

	combinedTLAS->SetBottomLevelAccelerationStructuresAndTransforms(*tempBLASList);
	generatedCombinedAS = true;
}

void AccelerationStructureManager::BuildCombinedAccelerationStructure(crossplatform::DeviceContext& deviceContext)
{
	if (!generatedCombinedAS)
		GenerateCombinedAccelerationStructure();
	combinedTLAS->BuildAccelerationStructureAtRuntime(deviceContext);
}

void AccelerationStructureManager::AddTopLevelAcclelerationStructure(TopLevelAccelerationStructure* newAS, int ID)
{
	TLASList[ID] = newAS;
	generatedCombinedAS = false;
}

void AccelerationStructureManager::RemoveTopLevelAcceleratonStructure(int ID)
{
	TLASList.erase(ID);
	generatedCombinedAS = false;
}

TopLevelAccelerationStructure* AccelerationStructureManager::GetCombinedAccelerationStructure()
{
	return combinedTLAS;
}
