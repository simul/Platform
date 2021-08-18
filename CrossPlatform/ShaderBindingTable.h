#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/Core/RuntimeError.h"
#include <map>
#include <vector>

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		class EffectPass; 

		class ShaderRecord
		{
		public:
			typedef void* Handle;
			typedef void* Parameters;

			enum class Type : uint32_t
			{
				RAYGEN, MISS, HIT_GROUP, CALLABLE
			};

			static const size_t DefaultHandleSize = 32;		//Based on D3D12 - 10.0.19041, Vulkan should be compatible
			static const size_t DefaultAlignmentSize = 32;	//Based on D3D12 - 10.0.19041, Vulkan should be compatible
			static const size_t DefaultMaxStride = 4096;	//Based on D3D12 - 10.0.19041, Vulkan should be compatible

		public:
			Type type;
			
			Handle handle;
			size_t handleSize = DefaultHandleSize;
			Parameters parameters;
			size_t parametersSize;
		
		public:
			ShaderRecord() = delete;
			~ShaderRecord() = default;

			ShaderRecord(Type shaderRecordType, Handle shaderHandle, Parameters shaderParameters = nullptr, size_t shaderParameterSize = 0)
				:type(shaderRecordType), handle(shaderHandle), parameters(shaderParameters), parametersSize(shaderParameterSize)
			{
				SIMUL_ASSERT((handleSize + parametersSize) <= ShaderRecord::DefaultMaxStride);
			}

			inline const size_t GetSize() const { return handleSize + parametersSize; }
			inline size_t GetSize() { return handleSize + parametersSize; }
		};

		class SIMUL_CROSSPLATFORM_EXPORT ShaderBindingTable
		{
		public:
			static const size_t DefaultAlignmentSize = 64;	//Based on D3D12 - 10.0.19041, Vulkan should be compatible

		protected:
			std::vector<ShaderRecord> shaderRecords;
			std::map<ShaderRecord::Type, size_t> shaderBindingTableStrides;
			std::map<ShaderRecord::Type, std::vector<char>> shaderBindingTableResources;

		public:
			ShaderBindingTable();
			virtual ~ShaderBindingTable() = default;
			virtual void RestoreDeviceObjects(RenderPlatform* r) {};
			virtual void InvalidateDeviceObjects() {};

			void DefaultInitFromEffectPass(RenderPlatform* renderPlatform, EffectPass* pass);

			void AddShaderRecord(const ShaderRecord& shaderRecord);
			void FinalizeShaderRecords();

			virtual void BuildShaderBindingTableResources(RenderPlatform* renderPlatform) {};
			virtual std::map<ShaderRecord::Type, std::vector<ShaderRecord::Handle>> GetShaderHandlesFromEffectPass(RenderPlatform* renderPlatform, EffectPass* pass) 
			{
				return std::map<ShaderRecord::Type, std::vector<ShaderRecord::Handle>>();
			};
		
		protected:
			inline size_t MakeAlignedSize(size_t value, size_t alignment) const
			{
				return (value + alignment - 1) & ~(alignment - 1); 
			}
		};
	}
}