#include "ThisPlatform/Direct3D12.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "DirectXRaytracingHelper.h"
#include "Platform/DirectX12/SimulDirectXHeader.h"
#include "Platform/DirectX12/BaseAccelerationStructure.h"
#include "Platform/DirectX12/TopLevelAccelerationStructure.h"
#include "Platform/DirectX12/BottomLevelAccelerationStructure.h"

using namespace simul;
using namespace dx12;


#if defined(_XBOX_ONE)
#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0
#endif

/////////////////////////////////
//TopLevelAccelerationStructure//
/////////////////////////////////

TopLevelAccelerationStructure::TopLevelAccelerationStructure(crossplatform::RenderPlatform* r)
	:crossplatform::TopLevelAccelerationStructure(r)
{

}

TopLevelAccelerationStructure::~TopLevelAccelerationStructure()
{
	InvalidateDeviceObjects();
}

void TopLevelAccelerationStructure::RestoreDeviceObjects()
{
}

void TopLevelAccelerationStructure::InvalidateDeviceObjects()
{
	initialized = false;
	SAFE_RELEASE(accelerationStructure);
	SAFE_RELEASE(scratchResource);
	SAFE_RELEASE(instanceDescsResource);
}

ID3D12Resource* TopLevelAccelerationStructure::AsD3D12ShaderResource(crossplatform::DeviceContext& deviceContext)
{
	if (!initialized)
		BuildAccelerationStructureAtRuntime(deviceContext);
	return accelerationStructure;
}

void TopLevelAccelerationStructure::BuildAccelerationStructureAtRuntime(crossplatform::DeviceContext& deviceContext)
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device* device = deviceContext.renderPlatform->AsD3D12Device();
	ID3D12Device5* device5 = nullptr;
	ID3D12GraphicsCommandList4* commandList4 = nullptr;
	bool platformValid = GetID3D12Device5andID3D12GraphicsCommandList4(deviceContext, device5, commandList4);

	if (!platformValid || !device5 || !commandList4)
	{
		// Initialized, but non-functional.
		initialized = true;
		return;
	}

	if (instanceDescs.empty())
		return;

	instanceDescs.clear();

	for (const auto& _instanceDesc : _instanceDescs)
	{
		const auto& BLAS = _instanceDesc.blas;
		const auto& Transform = _instanceDesc.transform;
		BottomLevelAccelerationStructure* d3d12BLAS = (dx12::BottomLevelAccelerationStructure*)BLAS;

		// Create an instance desc for the bottom-level acceleration structure.
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc;
		instanceDesc.Transform[0][0] = Transform.m00;
		instanceDesc.Transform[0][1] = Transform.m10;
		instanceDesc.Transform[0][2] = Transform.m20;
		instanceDesc.Transform[0][3] = Transform.m30;
		instanceDesc.Transform[1][0] = Transform.m01;
		instanceDesc.Transform[1][1] = Transform.m11;
		instanceDesc.Transform[1][2] = Transform.m21;
		instanceDesc.Transform[1][3] = Transform.m31;
		instanceDesc.Transform[2][0] = Transform.m02;
		instanceDesc.Transform[2][1] = Transform.m12;
		instanceDesc.Transform[2][2] = Transform.m22;
		instanceDesc.Transform[2][3] = Transform.m32;
		instanceDesc.InstanceID = static_cast<UINT>(instanceDescs.size()); //Use current size of the array as custom ID, starting at 0... 
		instanceDesc.InstanceMask = 0xFF;
		instanceDesc.InstanceContributionToHitGroupIndex = _instanceDesc.contributionToHitGroupIdx;
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instanceDesc.AccelerationStructure = d3d12BLAS->AsD3D12ShaderResource(deviceContext)->GetGPUVirtualAddress();
		instanceDescs.push_back(instanceDesc);
	}
	AllocateUploadBuffer(device, instanceDescs.data(), (instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC)), &instanceDescsResource, L"InstanceDescsResource");

	instanceCount = static_cast<uint32_t>(instanceDescs.size());

	// Get required sizes for an acceleration structure.
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	inputs.NumDescs = static_cast<UINT>(instanceDescs.size());
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();

	device5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
	ThrowIfFalse(prebuildInfo.ResultDataMaxSizeInBytes > 0);

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	AllocateUAVBuffer(device, prebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	AllocateUAVBuffer(device, prebuildInfo.ResultDataMaxSizeInBytes, &accelerationStructure, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

	// Top Level Acceleration Structure desc
	buildDesc.DestAccelerationStructureData = accelerationStructure->GetGPUVirtualAddress();
	buildDesc.Inputs = inputs;
	buildDesc.SourceAccelerationStructureData = 0;
	buildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	commandList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
	commandList4->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(accelerationStructure));
#endif
	initialized = true;
}
