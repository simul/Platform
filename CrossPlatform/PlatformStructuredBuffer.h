#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/CrossPlatform/Topology.h"
#include "Platform/CrossPlatform/Layout.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/Query.h"
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <set>
#include <stdint.h>
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
typedef unsigned int GLuint;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		enum class BufferUsageHint
		{
			ONCE,
			ONCE_PER_FRAME,
			MANY_PER_FRAME
		};
		struct ShaderResource;
		/// A base class for structured buffers, used by StructuredBuffer internally.
		class SIMUL_CROSSPLATFORM_EXPORT PlatformStructuredBuffer
		{
		public:
			simul::crossplatform::RenderPlatform* renderPlatform;
		protected:
			int numCopies;	// for tracking when the data should be valid, i.e. when numCopies==Latency.
			bool cpu_read;
			std::string name;
			BufferUsageHint bufferUsageHint;
		public:
			PlatformStructuredBuffer() :renderPlatform(nullptr), numCopies(0), cpu_read(false), bufferUsageHint(BufferUsageHint::MANY_PER_FRAME) {}
			virtual ~PlatformStructuredBuffer() = default;
			virtual void RestoreDeviceObjects(RenderPlatform* r, int count, int unit_size, bool computable, bool cpu_read, void* init_data, const char* name, BufferUsageHint usageHint) = 0;
			virtual void InvalidateDeviceObjects() = 0;
			virtual void Apply(DeviceContext& deviceContext, const ShaderResource& shaderResource);
			virtual void ApplyAsUnorderedAccessView(DeviceContext& deviceContext, const ShaderResource& shaderResource);
			// Deprecated
			virtual void Apply(DeviceContext& deviceContext, Effect* effect, const ShaderResource& shaderResource);
			// Deprecated
			virtual void ApplyAsUnorderedAccessView(DeviceContext& deviceContext, Effect* effect, const ShaderResource& shaderResource);
			virtual void Unbind(DeviceContext& deviceContext) = 0;
			virtual void* GetBuffer(crossplatform::DeviceContext& deviceContext) = 0;
			virtual const void* OpenReadBuffer(crossplatform::DeviceContext& deviceContext) = 0;
			virtual void CloseReadBuffer(crossplatform::DeviceContext& deviceContext) = 0;
			virtual void CopyToReadBuffer(crossplatform::DeviceContext& deviceContext) = 0;
			virtual void SetData(crossplatform::DeviceContext& deviceContext, void* data) = 0;
			virtual ID3D11ShaderResourceView* AsD3D11ShaderResourceView() { return NULL; }
			virtual ID3D11UnorderedAccessView* AsD3D11UnorderedAccessView() { return NULL; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12ShaderResourceView(crossplatform::DeviceContext&) { return NULL; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12UnorderedAccessView(crossplatform::DeviceContext&, int = 0) { return NULL; }
			virtual ID3D12Resource* AsD3D12Resource(crossplatform::DeviceContext& deviceContext) { return NULL; }
			void ResetCopies()
			{
				numCopies = 0;
			}
			/// For RenderPlatform's use only: do not call.
			virtual void ActualApply(simul::crossplatform::DeviceContext& /*deviceContext*/, EffectPass* /*currentEffectPass*/, int /*slot*/, bool /*as uav*/) {}
		};

		class SIMUL_CROSSPLATFORM_EXPORT BaseStructuredBuffer
		{
		public:
			virtual ~BaseStructuredBuffer() = default;
			PlatformStructuredBuffer* platformStructuredBuffer=nullptr;
		};
		class PlatformStructuredBuffer;
		/// Templated structured buffer, which uses platform-specific implementations of PlatformStructuredBuffer.
		///
		/// Declare like so:
		/// \code
		/// 	StructuredBuffer<Example> example;
		/// \endcode
		template<class T, BufferUsageHint bufferUsageHint = BufferUsageHint::MANY_PER_FRAME> class StructuredBuffer : public BaseStructuredBuffer
		{
		public:
			StructuredBuffer()
				:count(0)
			{
			}
			~StructuredBuffer()
			{
				InvalidateDeviceObjects();
			}
#if defined( _MSC_VER) && !defined( _GAMING_XBOX )

			void RestoreDeviceObjects(RenderPlatform* p, int ct, bool computable = false, bool cpu_read = true, T* data = nullptr, const char* n = nullptr)
			{
				if (!p)
					return;
				count = ct;
				delete platformStructuredBuffer;
				platformStructuredBuffer = NULL;
				platformStructuredBuffer = p->CreatePlatformStructuredBuffer();
				platformStructuredBuffer->RestoreDeviceObjects(p, count, sizeof(T), computable, cpu_read, data, n, bufferUsageHint);
			}
#else 
			void RestoreDeviceObjects(RenderPlatform* p, int ct, bool computable = false, bool cpu_read = true, T* data = nullptr, const char* n = nullptr);
#endif
			T* GetBuffer(crossplatform::DeviceContext& deviceContext)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return NULL;
				}
				return (T*)platformStructuredBuffer->GetBuffer(deviceContext);
			}
			const T* OpenReadBuffer(crossplatform::DeviceContext& deviceContext)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return NULL;
				}
				return (const T*)platformStructuredBuffer->OpenReadBuffer(deviceContext);
			}
			void CloseReadBuffer(crossplatform::DeviceContext& deviceContext)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return;
				}
				platformStructuredBuffer->CloseReadBuffer(deviceContext);
			}
			void CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return;
				}
				platformStructuredBuffer->CopyToReadBuffer(deviceContext);
			}
			void SetData(crossplatform::DeviceContext& deviceContext, T* data)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return;
				}
				platformStructuredBuffer->SetData(deviceContext, (void*)data);
			}
			ID3D11ShaderResourceView* AsD3D11ShaderResourceView()
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return NULL;
				}
				return platformStructuredBuffer->AsD3D11ShaderResourceView();
			}
			ID3D11UnorderedAccessView* AsD3D11UnorderedAccessView(int mip = 0)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return NULL;
				}
				return platformStructuredBuffer->AsD3D11UnorderedAccessView();
			}
			void Apply(crossplatform::DeviceContext& pContext, crossplatform::Effect* , const crossplatform::ShaderResource& shaderResource)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return;
				}
				platformStructuredBuffer->Apply(pContext,  shaderResource);
			}
			void ApplyAsUnorderedAccessView(crossplatform::DeviceContext& pContext, crossplatform::Effect* , const crossplatform::ShaderResource& shaderResource)
			{
				if (!platformStructuredBuffer)
				{
					SIMUL_BREAK_ONCE("Null Platform structured buffer pointer.");
					return;
				}
				platformStructuredBuffer->ApplyAsUnorderedAccessView(pContext, shaderResource);
			}
			void InvalidateDeviceObjects()
			{
				if (platformStructuredBuffer)
					platformStructuredBuffer->InvalidateDeviceObjects();
				delete platformStructuredBuffer;
				platformStructuredBuffer = nullptr;
				count = 0;
			}
			void ResetCopies()
			{
				if (platformStructuredBuffer)
					platformStructuredBuffer->ResetCopies();
			}

			int count;
		};

	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
