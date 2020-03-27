#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/CrossPlatform/Topology.h"
#include "Platform/CrossPlatform/Layout.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
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
		struct DeviceContext;
		class RenderPlatform;
		struct Query;
		class Effect;
		/// A disjoint query structure, like those in DirectX 11.
		/// Its main use is actually to get the clock frequency that will
		/// be used for timestamp queries, but the Disjoint value is
		/// used on some platforms to indicate whether the timestamp values are invalid.
		struct DisjointQueryStruct
		{
			uint64_t Frequency;
			int		Disjoint;
		};
		/// Crossplatform GPU query class.
		struct SIMUL_CROSSPLATFORM_EXPORT Query
		{
			crossplatform::RenderPlatform* renderPlatform;
			static const int QueryLatency = 4;
			bool QueryStarted;
			bool QueryFinished;
			int currFrame;
			QueryType type;
			bool gotResults[QueryLatency];
			bool doneQuery[QueryLatency];
			Query(QueryType t, crossplatform::RenderPlatform*r=nullptr)
				:renderPlatform(r)
				,QueryStarted(false)
				,QueryFinished(false)
				, currFrame(0)
				,type(t)
			{
				for(int i=0;i<QueryLatency;i++)
				{
					gotResults[i]=true;
					doneQuery[i]=false;
				}
			}
			virtual ~Query()
			{
			}
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void Begin(DeviceContext &deviceContext) =0;
			virtual void End(DeviceContext &deviceContext) =0;
			/// Get query data. Returns true if successful, or false otherwise.
			/// Blocking queries will return false until they succeed.
			virtual bool GetData(DeviceContext &deviceContext,void *data,size_t sz) =0;
			virtual void SetName(const char *){}
		};
		
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
