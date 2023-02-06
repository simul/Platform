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
	#include <dxgi1_5.h>
	#include <d3d12.h>
	#include "d3dx12.h"
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 0 
#endif

inline void SetD3DName(ID3D12Object* obj, const char* name)
{
	std::wstring n(name, name+strlen(name));
	obj->SetName(n.c_str());
}
inline void GetD3DName(ID3D12Object *obj,char *name,size_t maxsize)
{
	if(!maxsize)
		return;
	if(!obj)
	{
		name[0]=0;
		return;
	}
	UINT size=0;
#if defined(_GAMING_XBOX)
	// not implemented?????
	name[0] = 0;
#else
	GUID g = WKPDID_D3DDebugObjectName;
	HRESULT hr=(obj)->GetPrivateData(g,&size,	nullptr);
	if(hr==S_OK)
	{
		if(size <= maxsize)
			(obj)->GetPrivateData(g, &size, name);
	}
	else
	{
		g= WKPDID_D3DDebugObjectNameW;
		hr = (obj)->GetPrivateData(g, &size, nullptr);
		if (hr == S_OK)
		{
			wchar_t * src_w =new wchar_t[size+1];
			(obj)->GetPrivateData(g, &size, src_w);
			WideCharToMultiByte(CP_UTF8, 0, src_w, (int)size, name, (int)maxsize, NULL, NULL);
			delete [] src_w;
		}
	}
#endif
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
												char name[20];\
												GetD3DName((p),name,20);\
												SIMUL_COUT<< name<<" refct "<<refct<<std::endl;\
											}\
											(p)->Release();\
											(p)=nullptr;\
										}\
									}
#endif

#ifndef SAFE_RELEASE_LATER
#define SAFE_RELEASE_LATER(p)			{ if(p) {\
		auto renderPlatformDx12 = (dx12::RenderPlatform*)renderPlatform;\
		if(renderPlatformDx12)\
			renderPlatformDx12->PushToReleaseManager(p, #p);\
		else\
			p->Release();\
		p = nullptr;\
		 } }
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
	SIMUL_CERR << "Barrier: " << name << "(0x" << std::setfill('0') << std::setw(16) << std::hex << (unsigned long long)res << ") - from "\
	<< RenderPlatform::D3D12ResourceStateToString(before) << " to " << RenderPlatform::D3D12ResourceStateToString(after) << std::endl;
#endif

