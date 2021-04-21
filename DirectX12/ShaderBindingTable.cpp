#include "Platform/DirectX12/ShaderBindingTable.h"
#include "Platform/DirectX12/effect.h"
#include "Platform/Core/StringToWString.h"
#include "DirectXRaytracingHelper.h"
#include <string>

using namespace simul;
using namespace dx12;

ShaderBindingTable::ShaderBindingTable()
	:SBTResources( { 
		{crossplatform::ShaderRecord::Type::RAYGEN, nullptr},
		{crossplatform::ShaderRecord::Type::MISS, nullptr},
		{crossplatform::ShaderRecord::Type::HIT_GROUP, nullptr},
		{crossplatform::ShaderRecord::Type::CALLABLE, nullptr}
	} ) {}

ShaderBindingTable::~ShaderBindingTable()
{
	for (auto& sbtRes : SBTResources)
	{
		SAFE_RELEASE(sbtRes.second);
	}
}

std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> simul::dx12::ShaderBindingTable::GetShaderHandlesFromEffectPass(crossplatform::RenderPlatform* renderPlatform, crossplatform::EffectPass* pass)
{
	std::map<crossplatform::ShaderRecord::Type, std::vector<crossplatform::ShaderRecord::Handle>> result;

#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	dx12::EffectPass* d3d12Pass = reinterpret_cast<dx12::EffectPass*>(pass);
	if (!d3d12Pass)
	{
		SIMUL_BREAK_INTERNAL("No valid EffectPass.");
	}
	else
	{
		if (d3d12Pass->GetRayTracingPso() && renderPlatform->HasRenderingFeatures(crossplatform::RenderingFeatures::Raytracing))
		{
			ID3D12Device* device = renderPlatform->AsD3D12Device();
			ID3D12StateObjectProperties* rayTracingPipelineProperties = nullptr;
			V_CHECK(d3d12Pass->GetRayTracingPso()->QueryInterface(SIMUL_PPV_ARGS(&rayTracingPipelineProperties)));

			auto AddShaderRecord = [&](const std::string& name, crossplatform::ShaderRecord::Type type) -> void
			{
				std::wstring wstr_name = base::StringToWString(name);
				crossplatform::ShaderRecord::Handle handle = const_cast<void*>(rayTracingPipelineProperties->GetShaderIdentifier(wstr_name.c_str())); //Xbox retruns this as type: const void*, not void*.
				result[type].push_back(handle);
			};

			//Raygen 
			AddShaderRecord(d3d12Pass->shaders[crossplatform::SHADERTYPE_RAY_GENERATION]->entryPoint, crossplatform::ShaderRecord::Type::RAYGEN);

			//Miss shaders
			for (auto& miss : d3d12Pass->missShaders)
			{
				AddShaderRecord(miss.first, crossplatform::ShaderRecord::Type::MISS);
			}
			if (d3d12Pass->missShaders.empty() && d3d12Pass->shaders[crossplatform::SHADERTYPE_MISS])
			{
				AddShaderRecord(d3d12Pass->shaders[crossplatform::SHADERTYPE_MISS]->entryPoint, crossplatform::ShaderRecord::Type::MISS);
			}

			//HitGroup
			for (auto& hitGroup : d3d12Pass->raytraceHitGroups)
			{
				AddShaderRecord(hitGroup.first, crossplatform::ShaderRecord::Type::HIT_GROUP);
			}
			if (d3d12Pass->raytraceHitGroups.empty())
			{
				AddShaderRecord(base::WStringToString(std::wstring(d3d12Pass->hitGroupExportName)), crossplatform::ShaderRecord::Type::HIT_GROUP);
			}

			//Callable shaders
			for (auto& callable : d3d12Pass->callableShaders)
			{
				AddShaderRecord(callable.first, crossplatform::ShaderRecord::Type::CALLABLE);
			}
			if (d3d12Pass->callableShaders.empty() && d3d12Pass->shaders[crossplatform::SHADERTYPE_CALLABLE])
			{
				AddShaderRecord(d3d12Pass->shaders[crossplatform::SHADERTYPE_CALLABLE]->entryPoint, crossplatform::ShaderRecord::Type::CALLABLE);
			}
		}
	}
#endif

	return result;
}

void ShaderBindingTable::BuildShaderBindingTableResources(crossplatform::RenderPlatform* renderPlatform)
{
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
	ID3D12Device* device = renderPlatform->AsD3D12Device();
	const wchar_t* names[4] = {
		L"ShaderBinidngTable_RayGen",
		L"ShaderBinidngTable_Miss",
		L"ShaderBinidngTable_HitGroup",
		L"ShaderBinidngTable_Callable"
	};

	for (auto& sbtRes : shaderBindingTableResources)
	{
		if (sbtRes.second.size())
		{
			auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT>(sbtRes.second.size()));
			ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, SIMUL_PPV_ARGS(&SBTResources[sbtRes.first])));
			SBTResources[sbtRes.first]->SetName(names[(size_t)sbtRes.first]);

			uint8_t* mappedData;
			CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
			ThrowIfFailed(SBTResources[sbtRes.first]->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
			memcpy_s(mappedData, sbtRes.second.size(), sbtRes.second.data(), sbtRes.second.size());
			SBTResources[sbtRes.first]->Unmap(0, &readRange);
		}
	}
#endif
}
