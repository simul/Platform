#pragma once
#include "ThisPlatform/Direct3D12.h"
//! Improves perfomance of CreateConstantBufferViews and CopyDescriptors
//! by removing a run time "if". This should only be on release builds...
#define D3DCOMPILE_NO_DEBUG 1 

#ifndef _GAMING_XBOX
	#define SIMUL_DX12_SLOTS_CHECK
#endif

#include <DirectXMath.h>

#if defined(_GAMING_XBOX)
	#if defined(_GAMING_XBOX_SCARLETT)
		#undef PLATFORM_SUPPORT_D3D12_VIEWINSTANCING
		#define PLATFORM_SUPPORT_D3D12_VIEWINSTANCING 0 //Xbox Series X|S do not support View Instancing.
	#endif
	#if defined(_GAMING_XBOX_XBOXONE)
		#undef PLATFORM_SUPPORT_D3D12_RAYTRACING
		#define PLATFORM_SUPPORT_D3D12_RAYTRACING 0 //Xbox One X/S do not support Ray Tracing.
		#undef PLATFORM_SUPPORT_D3D12_VIEWINSTANCING
		#define PLATFORM_SUPPORT_D3D12_VIEWINSTANCING 0 //Xbox One X/S do not support View Instancing.
	#endif
	#define MONOLITHIC 1
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 1 
	#define SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK 1 
	#define SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE 0x40 
#else //PC
	#include <D3Dcompiler.h>
	#include <dxgi.h>
	#include <dxgi1_6.h>
	#include <d3d12.h>
	#include "d3dx12.h"
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 0 
#endif

//https://stackoverflow.com/questions/6693010/how-do-i-use-multibytetowidechar

inline void SetD3DName(ID3D12Object *obj, const std::string& name)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (UINT)name.length(), NULL, 0);
	std::wstring w_name(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, name.c_str(), (UINT)name.length(), &w_name[0], count);
	(obj)->SetPrivateData(WKPDID_D3DDebugObjectNameW, count * sizeof(std::wstring::value_type), w_name.c_str());
}

inline void GetD3DName(ID3D12Object *obj, std::string& name)
{
	if(!obj)
	{
		return;
	}

	UINT size = 0;
	HRESULT hr = (obj)->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);
	if (hr == S_OK)
	{
		std::wstring w_name(size / sizeof(std::wstring::value_type), 0);
		(obj)->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, &w_name[0]);

		int count = WideCharToMultiByte(CP_UTF8, 0, w_name.c_str(), (UINT)w_name.length(), NULL, 0, NULL, NULL);
		name.resize(count, 0);
		WideCharToMultiByte(CP_UTF8, 0, w_name.c_str(), (UINT)w_name.length(), &name[0], count, NULL, NULL);
	}
}

#ifndef SAFE_RELEASE
	#define SAFE_RELEASE(p)			{ if(p) { (p)->Release(); (p)=nullptr; } }
	#define SAFE_RELEASE_DEBUG(p)	{\
										if(p)\
										{\
											(p)->AddRef();\
											int refct=(p)->Release()-1;\
											if(refct!=21623460)\
											{\
												std::string name;\
												GetD3DName((p),name);\
												SIMUL_COUT<< name<<" refct "<<refct<<std::endl;\
											}\
											(p)->Release();\
											(p)=nullptr;\
										}\
									}
#endif

#ifndef SAFE_RELEASE_LATER
#define SAFE_RELEASE_LATER(p)	{\
									if(p) {\
										auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;\
										if(renderPlatformDx12)\
											renderPlatformDx12->PushToReleaseManager(p);\
										else\
											p->Release();\
										p = nullptr;\
									}\
								}
#endif
#ifndef SAFE_RELEASE_ALLOCATIR_LATER
#define SAFE_RELEASE_ALLOCATIR_LATER(p, a)	{\
												if(p && a) {\
													auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;\
													if(renderPlatformDx12)\
													{\
														renderPlatformDx12->PushToReleaseManager(p, a);\
													}\
													else\
													{\
														(a)->allocation->Release();\
														p->Release();\
													}\
													(a)->allocator = nullptr;\
													(a)->allocation = nullptr;\
													p = nullptr;\
												}\
											}
#endif


#ifndef SAFE_RELEASE_ARRAY
	#define SAFE_RELEASE_ARRAY(p,n)	{ if(p) for(int i=0;i<int(n);i++) if(p[i]) { (p[i])->Release(); (p[i])=nullptr; } }
#endif
#ifndef SAFE_DELETE
    #define SAFE_DELETE(p)          { if(p) { delete p; p=nullptr;} }
#endif
#ifndef SAFE_DELETE_ARRAY
	#define SAFE_DELETE_ARRAY(p)          { if(p) { delete[] p; p=nullptr;} }
#endif
#ifndef SAFE_DELETE_ARRAY_MEMBERS
	#define SAFE_DELETE_ARRAY_MEMBERS(p,n)	{ if(p) for(int i=0;i<int(n);i++) if(p[i]) { delete (p[i]); (p[i])=nullptr; } }
#endif

#if defined(_GAMING_XBOX)
    #define  SIMUL_PPV_ARGS IID_GRAPHICS_PPV_ARGS
#else
    #define  SIMUL_PPV_ARGS IID_PPV_ARGS
#endif

#if PLATFORM_DEBUG_BARRIERS
	#define LOG_BARRIER_INFO(name, res, before, after);\
	SIMUL_CERR << "Barrier: " << name << "(0x" << std::setfill('0') << std::setw(16) << std::hex << (uint64_t)res << ") - from "\
	<< RenderPlatform::D3D12ResourceStateToString(before) << " to " << RenderPlatform::D3D12ResourceStateToString(after) << std::endl;
#else
	#define LOG_BARRIER_INFO(name, res, before, after)
#endif

// D3D12MA
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#define D3D12MA_IID_PPV_ARGS SIMUL_PPV_ARGS
#include "D3D12MemAlloc.h"

namespace platform::dx12
{
	struct AllocationInfo
	{
		D3D12MA::Allocator *allocator = nullptr;
		D3D12MA::Allocation *allocation = nullptr;
	};
}