#pragma once
#include "ThisPlatform/Direct3D12.h"
//! Improves perfomance of CreateConstantBufferViews and CopyDescriptors
//! by removing a run time "if". This should only be on release builds...
#define D3DCOMPILE_NO_DEBUG 1 

#ifndef _XBOX_ONE
#ifndef _GAMING_XBOX
	#define SIMUL_DX12_SLOTS_CHECK
#endif
#endif

#include <DirectXMath.h>

#if defined(_XBOX_ONE) || defined(_GAMING_XBOX)
	#if defined(_GAMING_XBOX_SCARLETT) ||  defined(_GAMING_XBOX_XBOXONE)
		#include "ThisPlatform/Direct3D12.h"
	#else
		#ifndef _GAMING_XBOX //Deprecated from the GDK
			#include <D3Dcompiler_x.h>
		#else
			#include <D3Dcompiler.h>
		#endif
	#include <d3d12_x.h>		//! core 12.x header
	#include <d3dx12_x.h>		//! utility 12.x header
	#endif
//	#include <dxgi1_6.h>
	#define MONOLITHIC 1
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 1 
	#define SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK 1 
	#define SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE 0x40 
#else
	#include <D3Dcompiler.h>
	#include <dxgi.h>
	#include <dxgi1_5.h>
	#include <d3d12.h>
	#include "d3dx12.h"
	#define SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT 0 
#endif

inline void GetD3DName(ID3D12Object *obj,char *name,size_t maxsize)
{
	UINT size=0;
#if defined(_XBOX_ONE) || defined(_GAMING_XBOX)
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

#if defined(_XBOX_ONE) || defined(_GAMING_XBOX)
    #define  SIMUL_PPV_ARGS IID_GRAPHICS_PPV_ARGS
#else
    #define  SIMUL_PPV_ARGS IID_PPV_ARGS
#endif

#if !defined(_XBOX_ONE) && !defined(_GAMING_XBOX_SCARLETT) && !defined(_GAMING_XBOX_XBOXONE)
#include <iostream>

inline std::string HRESULTToString(HRESULT error)
{
	if (error != 0)
	{
		char* formatedMessage = nullptr;
		DWORD formatedMessageSize = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, error, 0, (char*)&formatedMessage, 0, nullptr);

		if (formatedMessage != nullptr && formatedMessageSize > 0)
		{
			std::string result(formatedMessage, formatedMessageSize);
			LocalFree(formatedMessage);
			return result;
		}
	}
	return std::string();
}

inline void ID3D12DeviceRemovedExtendedDataParser(ID3D12Device* device)
{
	if (!device)
		return;

	HRESULT res = device->GetDeviceRemovedReason();
	std::cerr << HRESULTToString(res);

	ID3D12DeviceRemovedExtendedData* pDred = nullptr;
	res = device->QueryInterface(IID_PPV_ARGS(&pDred));
	if (SUCCEEDED(res) && pDred)
	{
		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
		res = pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
		if (SUCCEEDED(res))
		{
			std::cerr << "DRED - Breadcrumbs:" << std::endl;
			const D3D12_AUTO_BREADCRUMB_NODE* breadcrumb = DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
			while (breadcrumb)
			{
				auto D3D12_AUTO_BREADCRUMB_OP_ToString = [](D3D12_AUTO_BREADCRUMB_OP op) -> std::string
				{
					switch (op)
					{
					case D3D12_AUTO_BREADCRUMB_OP_SETMARKER:
						return "D3D12_AUTO_BREADCRUMB_OP_SETMARKER";
					case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT:
						return "D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT";
					case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT:
						return "D3D12_AUTO_BREADCRUMB_OP_ENDEVENT";
					case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:
						return "D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED";
					case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
						return "D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED";
					case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT:
						return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT";
					case D3D12_AUTO_BREADCRUMB_OP_DISPATCH:
						return "D3D12_AUTO_BREADCRUMB_OP_DISPATCH";
					case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION:
						return "D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION";
					case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:
						return "D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION";
					case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE:
						return "D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE";
					case D3D12_AUTO_BREADCRUMB_OP_COPYTILES:
						return "D3D12_AUTO_BREADCRUMB_OP_COPYTILES";
					case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE";
					case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
						return "D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW";
					case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW:
						return "D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW";
					case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:
						return "D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW";
					case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER";
					case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE:
						return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE";
					case D3D12_AUTO_BREADCRUMB_OP_PRESENT:
						return "D3D12_AUTO_BREADCRUMB_OP_PRESENT";
					case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA";
					case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION:
						return "D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION";
					case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION:
						return "D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION";
					case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME:
						return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME";
					case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES:
						return "D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES";
					case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT:
						return "D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT";
					case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64:
						return "D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64";
					case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION";
					case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE:
						return "D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE";
					case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1:
						return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1";
					case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION:
						return "D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION";
					case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2:
						return "D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2";
					case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1:
						return "D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1";
					case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE:
						return "D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE";
					case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO:
						return "D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO";
					case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE:
						return "D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE";
					case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS:
						return "D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS";
					case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND:
						return "D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND";
					case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND:
						return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND";
					case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION:
						return "D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION";
					case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP";
					case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1:
						return "D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1";
					case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND:
						return "D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND";
					case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND:
						return "D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND";
					case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH:
						return "D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH";
				#if (WDK_NTDDI_VERSION > NTDDI_WIN10_VB)
					case D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME:
						return "D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME";
					case D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA:
						return "D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA";
				#endif
					default:
						return "";
					}
				};

				if (breadcrumb->pCommandListDebugNameA)
					std::cerr << breadcrumb->pCommandListDebugNameA;
				else if (breadcrumb->pCommandListDebugNameW)
					std::wcerr << breadcrumb->pCommandListDebugNameW;
				else
					std::cerr << "Unknown CommandListDebugName";
				std::cerr << " : " << std::hex << "0x" << (uint64_t)(void*)(breadcrumb->pCommandList) << std::dec << std::endl;

				if (breadcrumb->pCommandQueueDebugNameA)
					std::cerr << breadcrumb->pCommandQueueDebugNameA;
				else if (breadcrumb->pCommandQueueDebugNameW)
					std::wcerr << breadcrumb->pCommandQueueDebugNameW;
				else
					std::cerr << "Unknown CommandQueueDebugName";
				std::cerr << " : " << std::hex << "0x" << (uint64_t)(void*)(breadcrumb->pCommandQueue) << std::dec << std::endl;

				if (breadcrumb->pCommandHistory)
				{
					std::cerr << "CommandHistory - GPU Operation Entries: " << breadcrumb->BreadcrumbCount << std::endl;
					for (UINT32 i = 0; i < breadcrumb->BreadcrumbCount; i++)
					{
						D3D12_AUTO_BREADCRUMB_OP gpuOperation = breadcrumb->pCommandHistory[i];
						std::cerr << i << ": " << D3D12_AUTO_BREADCRUMB_OP_ToString(gpuOperation) << std::endl;
					}
					std::cerr << "CommandHistory - Last Render Or Compute GPU Operation" << std::endl;
					if (breadcrumb->pLastBreadcrumbValue)
					{
						const UINT32 lastRenderOrComputeGPUOperation = *(breadcrumb->pLastBreadcrumbValue);
						D3D12_AUTO_BREADCRUMB_OP gpuOperation = breadcrumb->pCommandHistory[lastRenderOrComputeGPUOperation];
						std::cerr << D3D12_AUTO_BREADCRUMB_OP_ToString(gpuOperation) << std::endl;
					}
				}
				breadcrumb = breadcrumb->pNext;
				std::cerr << std::endl;
			}
		}
		else
		{
			std::cerr << "ID3D12DeviceRemovedExtendedDataSettings::SetAutoBreadcrumbsEnablement() is not set correctly." << std::endl;
		}

		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		res = pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput);
		if (SUCCEEDED(res))
		{
			std::cerr << "DRED - Page Fault on GPU Virtual Address: " << DredPageFaultOutput.PageFaultVA << std::endl;

			auto D3D12_DRED_ALLOCATION_TYPE_ToString = [](D3D12_DRED_ALLOCATION_TYPE type) -> std::string
			{
				switch (type)
				{
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE";
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR";
				case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE:
					return "D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE";
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST";
				case D3D12_DRED_ALLOCATION_TYPE_FENCE:
					return "D3D12_DRED_ALLOCATION_TYPE_FENCE";
				case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP";
				case D3D12_DRED_ALLOCATION_TYPE_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_HEAP";
				case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP";
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE";
				case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY:
					return "D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR";
				case D3D12_DRED_ALLOCATION_TYPE_RESOURCE:
					return "D3D12_DRED_ALLOCATION_TYPE_RESOURCE";
				case D3D12_DRED_ALLOCATION_TYPE_PASS:
					return "D3D12_DRED_ALLOCATION_TYPE_PASS";
				case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION:
					return "D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION";
				case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY:
					return "D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY";
				case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION:
					return "D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP";
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL";
				case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER:
					return "D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER";
				case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT:
					return "D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT";
				case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND:
					return "D3D12_DRED_ALLOCATION_TYPE_METACOMMAND";
				case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP:
					return "D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND";
			#if (WDK_NTDDI_VERSION > NTDDI_WIN10_VB)
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER";
				case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP:
					return "D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP";
			#endif
				case D3D12_DRED_ALLOCATION_TYPE_INVALID:
					return "D3D12_DRED_ALLOCATION_TYPE_INVALID";
				default:
					return "";
				}
			};
			std::cerr << "DRED - Existing Allocations:" << std::endl;
			const D3D12_DRED_ALLOCATION_NODE* existingNode = DredPageFaultOutput.pHeadExistingAllocationNode;
			while (existingNode)
			{
				if (existingNode->ObjectNameA)
					std::cerr << existingNode->ObjectNameA;
				else if (existingNode->ObjectNameW)
					std::wcerr << existingNode->ObjectNameW;
				else
					std::cerr << "Unknown Object";
				std::cerr << " : " << D3D12_DRED_ALLOCATION_TYPE_ToString(existingNode->AllocationType) << std::endl;
				existingNode = existingNode->pNext;
				std::cerr << std::endl;
			}
			std::cerr << "DRED - Freed Allocations:" << std::endl;
			const D3D12_DRED_ALLOCATION_NODE* freedNode = DredPageFaultOutput.pHeadRecentFreedAllocationNode;
			while (freedNode)
			{
				if (freedNode->ObjectNameA)
					std::cerr << freedNode->ObjectNameA;
				else if (freedNode->ObjectNameW)
					std::wcerr << freedNode->ObjectNameW;
				else
					std::cerr << "Unknown Object";
				std::cerr << " : " << D3D12_DRED_ALLOCATION_TYPE_ToString(freedNode->AllocationType) << std::endl;
				freedNode = freedNode->pNext;
				std::cerr << std::endl;
			}
		}
		else
		{
			std::cerr << "ID3D12DeviceRemovedExtendedDataSettings::SetPageFaultEnablement() is not set correctly." << std::endl;
		}
	}
}
#else
inline void ID3D12DeviceRemovedExtendedDataParser(ID3D12Device* device)
{
}
#endif

inline bool ID3D12DeviceCriticalError(HRESULT res)
{
	if (res == DXGI_ERROR_DEVICE_HUNG
		|| res == DXGI_ERROR_DEVICE_REMOVED
		|| res == DXGI_ERROR_DEVICE_RESET
		|| res == DXGI_ERROR_DRIVER_INTERNAL_ERROR
		|| res == DXGI_ERROR_INVALID_CALL)
		return true;
	else
		return false;
}

#define SIMUL_D3D12_DRED_CHECK(res, device) { if (ID3D12DeviceCriticalError(res)) { ID3D12DeviceRemovedExtendedDataParser(device); } }
