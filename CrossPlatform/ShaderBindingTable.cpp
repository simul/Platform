#include "Platform/CrossPlatform/ShaderBindingTable.h"
#include <algorithm>

using namespace simul;
using namespace crossplatform;

ShaderBindingTable::ShaderBindingTable()
	:shaderBindingTableStrides( { 
	{ShaderRecord::Type::RAYGEN, 0}, 
	{ShaderRecord::Type::MISS, 0}, 
	{ShaderRecord::Type::HIT_GROUP, 0}, 
	{ShaderRecord::Type::CALLABLE, 0} 
	} ) {}

void ShaderBindingTable::DefaultInitFromEffectPass(RenderPlatform* renderPlatform, EffectPass* pass)
{
	auto shaderHandles = GetShaderHandlesFromEffectPass(renderPlatform, pass);
	for (auto& shaderHandlesPerType : shaderHandles)
	{
		ShaderRecord::Type type = shaderHandlesPerType.first;
		for (auto& shaderHandle : shaderHandlesPerType.second)
		{
			AddShaderRecord(ShaderRecord(type, shaderHandle));
		}
	}
	FinalizeShaderRecords();
	BuildShaderBindingTableResources(renderPlatform);
}

void ShaderBindingTable::AddShaderRecord(const ShaderRecord& shaderRecord)
{
	shaderRecords.push_back(shaderRecord);
	shaderBindingTableStrides[shaderRecord.type] = MakeAlignedSize(std::max(shaderBindingTableStrides[shaderRecord.type], shaderRecord.GetSize()), ShaderRecord::DefaultAlignmentSize);
}

void ShaderBindingTable::FinalizeShaderRecords()
{
	for (auto& shaderRecord : shaderRecords)
	{
		const ShaderRecord::Type& type = shaderRecord.type;
		size_t currentSize = shaderBindingTableResources[type].size();
		size_t newSize = currentSize + shaderBindingTableStrides[type];
		shaderBindingTableResources[type].resize(newSize);

		char* dstPtr = shaderBindingTableResources[type].data() + currentSize;
		memcpy(dstPtr, shaderRecord.handle, ShaderRecord::DefaultHandleSize);

		if (shaderRecord.parameters && shaderBindingTableStrides[type] > ShaderRecord::DefaultHandleSize)
		{
			dstPtr += ShaderRecord::DefaultHandleSize;
			memcpy(dstPtr, shaderRecord.parameters, shaderRecord.parametersSize);
		}
	}
}
