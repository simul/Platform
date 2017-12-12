#define NOMINMAX

#include "Effect.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/FileLoader.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "SimulDirectXHeader.h"
#include "MacrosDx1x.h"

#include <algorithm>
#include <string>
#include <array>

using namespace simul;
using namespace dx12;

inline bool IsPowerOfTwo( UINT64 n )
{
    return ( ( n & (n-1) ) == 0 && (n) != 0 );
}

inline UINT64 NextMultiple( UINT64 value, UINT64 multiple )
{
    SIMUL_ASSERT( IsPowerOfTwo(multiple) );

    return (value + multiple - 1) & ~(multiple - 1);
}

template< class T >
UINT64 BytePtrToUint64( _In_ T* ptr )
{
    return static_cast< UINT64 >( reinterpret_cast< BYTE* >( ptr ) - static_cast< BYTE* >( nullptr ) );
}

Query::Query(crossplatform::QueryType t):
	crossplatform::Query(t),
	mQueryHeap(nullptr),
	mReadBuffer(nullptr),
	mQueryData(nullptr),
	mTime(0),
	mIsDisjoint(false)
{
	mD3DType = dx12::RenderPlatform::ToD3dQueryType(t);
	if (t == crossplatform::QueryType::QUERY_TIMESTAMP_DISJOINT)
	{
		mIsDisjoint = true;
	}
}

Query::~Query()
{
	InvalidateDeviceObjects();
}

void Query::SetName(const char *name)
{
	std::string n (name);
	mQueryHeap->SetName(std::wstring(n.begin(),n.end()).c_str());
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	InvalidateDeviceObjects();
	auto rPlat = (dx12::RenderPlatform*)r;

	// Create a query heap
	HRESULT res					= S_FALSE;
	mD3DType					= dx12::RenderPlatform::ToD3dQueryType(type);
	D3D12_QUERY_HEAP_DESC hDesc = {};
	hDesc.Count					= QueryLatency;
	hDesc.NodeMask				= 0;
	hDesc.Type					= dx12::RenderPlatform::ToD3D12QueryHeapType(type);
#ifdef _XBOX_ONE
	res							= r->AsD3D12Device()->CreateQueryHeap(&hDesc,IID_GRAPHICS_PPV_ARGS(&mQueryHeap));
#else
	res							= r->AsD3D12Device()->CreateQueryHeap(&hDesc,IID_PPV_ARGS(&mQueryHeap));
#endif
	SIMUL_ASSERT(res == S_OK);

	// Create a redback buffer to get data
	res = rPlat->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(QueryLatency * sizeof(UINT64)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
#ifdef _XBOX_ONE
		IID_GRAPHICS_PPV_ARGS(&mReadBuffer)
#else
		IID_PPV_ARGS(&mReadBuffer)
#endif
	);
	SIMUL_ASSERT(res == S_OK);
	mReadBuffer->SetName(L"QueryReadBuffer");
}

void Query::InvalidateDeviceObjects() 
{
	SAFE_RELEASE(mQueryHeap);
	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i]= true;	// ?????????
		doneQuery[i] = false;
	}
	SAFE_RELEASE(mReadBuffer);
	mQueryData = nullptr;
}

void Query::Begin(crossplatform::DeviceContext &deviceContext)
{

}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
	gotResults[currFrame] = false;
	doneQuery[currFrame]  = true;

	// For now, only take into account time stamp queries
	if (mD3DType != D3D12_QUERY_TYPE_TIMESTAMP)
	{
		return;
	}

	deviceContext.asD3D12Context()->EndQuery
	(
		mQueryHeap,
		mD3DType,
		currFrame
	);

	deviceContext.asD3D12Context()->ResolveQueryData
	(
		mQueryHeap, mD3DType,
		currFrame, 1,
		mReadBuffer, currFrame * sizeof(UINT64)
	);
}

bool Query::GetData(crossplatform::DeviceContext& deviceContext,void *data,size_t sz)
{
	gotResults[currFrame]	= true;
	currFrame				= (currFrame + 1) % QueryLatency;

	// We provide frequency information
	if (mIsDisjoint)
	{
		auto rPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
		crossplatform::DisjointQueryStruct disjointData;
		disjointData.Disjoint	= false;
		disjointData.Frequency	= rPlat->GetTimeStampFreq();
		memcpy(data, &disjointData, sz);
		return true;
	}

	// For now, only take into account time stamp queries
	if (mD3DType != D3D12_QUERY_TYPE_TIMESTAMP)
	{
		const UINT64 invalid = 0;
		data = (void*)invalid;
		return true;
	}

	if (!doneQuery[currFrame])
	{
		return false;
	}
	
	// Get the values from the buffer
	HRESULT res				= S_FALSE;
	res						= mReadBuffer->Map(0, &CD3DX12_RANGE(0, 1), reinterpret_cast<void**>(&mQueryData));
	SIMUL_ASSERT(res == S_OK);
	{
		UINT64* d			= (UINT64*)mQueryData;
		UINT64 time			= d[currFrame];
		mTime				= time;
		memcpy(data, &mTime, sz);
	}
	mReadBuffer->Unmap(0, &CD3DX12_RANGE(0, 0));

	return true;
}

RenderState::RenderState()
{
	BlendDesc			= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RasterDesc			= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	DepthStencilDesc	= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RasterDesc.FrontCounterClockwise = true;	
}

RenderState::~RenderState()
{
}

PlatformStructuredBuffer::PlatformStructuredBuffer():
	mNumElements(0),
	mElementByteSize(0),
	mBufferDefault(nullptr),
	mBufferUpload(nullptr),
	mChanged(false),
	mReadSrc(nullptr),
	mTempBuffer(nullptr),
	mRenderPlatform(nullptr),
	mBufferRead(nullptr)
{
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r, int ct, int unit_size, bool computable,bool cpu_read, void *init_data)
{
	HRESULT res			= S_FALSE;
	mNumElements		= ct;
	mElementByteSize	= unit_size;
	mTotalSize			= mNumElements * mElementByteSize;
	renderPlatform=r;
	mRenderPlatform		= (dx12::RenderPlatform*)renderPlatform;
	
	// TO-DO: do we need this? Is it different for non UAV?
	const int rawAlign	= D3D12_CS_4_X_RAW_UAV_BYTE_ALIGNMENT;
	int s				= mTotalSize % rawAlign;
	mTotalSize			= s ? mTotalSize + (rawAlign - s) : mTotalSize;

	if (mTotalSize == 0)
	{
		SIMUL_BREAK("Invalid arguments for the SB creation");
	}

	mCurrentState		= computable ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ;

	// Default heap
	D3D12_RESOURCE_FLAGS bufferFlags = computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize,bufferFlags),
		mCurrentState,
		nullptr,
#ifdef _XBOX_ONE
		IID_GRAPHICS_PPV_ARGS(&mBufferDefault)
#else
		IID_PPV_ARGS(&mBufferDefault)
#endif
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferDefault->SetName(L"SBDefaultBuffer");

	// Upload heap
	res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
#ifdef _XBOX_ONE
		IID_GRAPHICS_PPV_ARGS(&mBufferUpload)
#else
		IID_PPV_ARGS(&mBufferUpload)
#endif
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferUpload->SetName(L"SBUploadBuffer");

	// Initialize it if we have data
	if (init_data)
	{
		D3D12_SUBRESOURCE_DATA dataToCopy	= {};
		dataToCopy.pData					= init_data;
		dataToCopy.RowPitch					= dataToCopy.SlicePitch = mTotalSize;

		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST,true);

		UpdateSubresources(mRenderPlatform->AsD3D12CommandList(), mBufferDefault, mBufferUpload, 0, 0, 1, &dataToCopy);

		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState,true);
	}

	// Create the views
	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement				= 0;
	srvDesc.Buffer.NumElements				= mNumElements;
	srvDesc.Buffer.StructureByteStride		= mElementByteSize;
	srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;

	mBufferSrvHeap.Restore(mRenderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBSrvHeap", false) ;
	mRenderPlatform->AsD3D12Device()->CreateShaderResourceView(mBufferDefault, &srvDesc, mBufferSrvHeap.CpuHandle());
	mSrvHandle = mBufferSrvHeap.CpuHandle();
	mBufferSrvHeap.Offset();

	// UAV
	if (computable)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc	= {};
		uavDesc.ViewDimension						= D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Format								= DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.CounterOffsetInBytes			= 0;
		uavDesc.Buffer.FirstElement					= 0;
		uavDesc.Buffer.NumElements					= mNumElements;
		uavDesc.Buffer.StructureByteStride			= mElementByteSize;
		uavDesc.Buffer.Flags						= D3D12_BUFFER_UAV_FLAG_NONE;

		mBufferUavHeap.Restore(mRenderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBUavHeap", false);
		mRenderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mBufferDefault, nullptr, &uavDesc, mBufferUavHeap.CpuHandle());
		mUavHandle = mBufferUavHeap.CpuHandle();
		mBufferUavHeap.Offset();
	}

	// Create the READ_BACK buffers
	mBufferRead = new ID3D12Resource*[mReadTotalCnt];
	for (unsigned int i = 0; i < mReadTotalCnt; i++)
	{
		res = mRenderPlatform->AsD3D12Device()->CreateCommittedResource
		(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
#ifdef _XBOX_ONE
			IID_GRAPHICS_PPV_ARGS(&mBufferRead[i])
#else
			IID_PPV_ARGS(&mBufferRead[i])
#endif
		);
		SIMUL_ASSERT(res == S_OK);
		mBufferRead[i]->SetName(L"SBReadbackBuffer");
	}

	// Create a temporal buffer that we will use to upload data to the GPU
	mTempBuffer = malloc(mTotalSize);
	memset(mTempBuffer, 0, mTotalSize);

	// Some cool memory log
	//SIMUL_COUT << "[RAM] Allocating:" << std::to_string((float)mTotalSize / 1048576.0f) << ".MB" << " , " << mTotalSize << ".bytes" << std::endl;
}


void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext& deviceContext, crossplatform::Effect *effect, const char *name)
{
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext, effect, name);

	// If it changed (GetBuffer() was called) we will upload the data in the temp buffer to the GPU
	if (mChanged)
	{
		D3D12_SUBRESOURCE_DATA dataToCopy = {};
		dataToCopy.pData = mTempBuffer;
		dataToCopy.RowPitch = dataToCopy.SlicePitch = mTotalSize;

		auto r = (dx12::RenderPlatform*)deviceContext.renderPlatform;
		r->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST,true);

		UpdateSubresources(r->AsD3D12CommandList(), mBufferDefault, mBufferUpload, 0, 0, 1, &dataToCopy);

		r->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState,true);

		mChanged = false;
	}
}

void *PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &deviceContext)
{
	if (mChanged)
	{
		SIMUL_CERR_ONCE << " This SB hasn't been ActuallyAplied(), maybe is not in use by the shader." << std::endl;
		return nullptr;
	}
	mChanged = true;
	return mTempBuffer;
}

const void *PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map
	// Map from the oldest frame that should be ready at this point
	// We intend to read from CPU so we pass a valid range!
	const CD3DX12_RANGE readRange(0, 1);
	HRESULT res			= S_FALSE;
	unsigned int curIdx = (deviceContext.frame_number + 1) % mReadTotalCnt;

	res					= mBufferRead[curIdx]->Map(0, &readRange, reinterpret_cast<void**>(&mReadSrc));

	SIMUL_ASSERT(res == S_OK);
	return mReadSrc;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	// We are mapping from a readback buffer so it doesnt matter what we modified,
	// the GPU won't have acces to it. We pass a 0,0 range here.
	const CD3DX12_RANGE readRange(0, 0);
	unsigned int curIdx = (deviceContext.frame_number + 1) % mReadTotalCnt;
	mBufferRead[curIdx]->Unmap(0, &readRange);
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	// Check state
	bool changed = false;
	if ((mCurrentState & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changed = true;
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE,true);
	}

	// Schedule a copy
	unsigned int curIdx = (deviceContext.frame_number) % mReadTotalCnt;
	deviceContext.renderPlatform->AsD3D12CommandList()->CopyResource(mBufferRead[curIdx], mBufferDefault);

	// Restore state
	if (changed)
	{
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_SOURCE, mCurrentState,true);
	}
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &deviceContext,void *data)
{
	SIMUL_BREAK_ONCE("This is not working in dx12\n");
	if (data)
	{
		D3D12_SUBRESOURCE_DATA dataToCopy	= {};
		dataToCopy.pData					= data;
		dataToCopy.RowPitch					= dataToCopy.SlicePitch = mTotalSize;
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST,true);
		UpdateSubresources(mRenderPlatform->AsD3D12CommandList(), mBufferDefault, mBufferUpload, 0, 0, 1, &dataToCopy);
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState,true);
	}
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if (mTempBuffer)
	{
		delete mTempBuffer;
		mTempBuffer = nullptr;
	}
	mBufferSrvHeap.Release(mRenderPlatform);
	mBufferUavHeap.Release(mRenderPlatform);
	mRenderPlatform->PushToReleaseManager(mBufferDefault, "SBDefault");
	mRenderPlatform->PushToReleaseManager(mBufferUpload, "SBUpload");
	for (unsigned int i = 0; i < mReadTotalCnt; i++)
	{
		mRenderPlatform->PushToReleaseManager(mBufferRead[i], "SBRead");
	}
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{

}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12ShaderResourceView(crossplatform::DeviceContext &deviceContext)
{
	// Check the resource state
	if (mCurrentState != D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		SIMUL_ASSERT(mRenderPlatform != nullptr);
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_GENERIC_READ);
		mCurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}
	return &mSrvHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12UnorderedAccessView(crossplatform::DeviceContext &deviceContext,int mip /*= 0*/)
{
	// Check the resource state
	if (mCurrentState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		SIMUL_ASSERT(mRenderPlatform != nullptr);
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		mCurrentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	return &mUavHandle;
}

void PlatformStructuredBuffer::ActualApply(simul::crossplatform::DeviceContext& deviceContext, EffectPass* currentEffectPass, int slot)
{

}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_index.size();
}

dx12::Effect::Effect() 
{
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx12::EffectTechnique;
}


void Shader::load(crossplatform::RenderPlatform *renderPlatform, const char *filename_utf8, crossplatform::ShaderType t)
{
	simul::base::MemoryInterface* allocator = renderPlatform->GetMemoryInterface();
	simul::base::FileLoader* fileLoader		= simul::base::FileLoader::GetFileLoader();

	struct FileBlob
	{
		void*		pData;
		uint32_t	DataSize;
	};

	// Load the shader binary
	FileBlob shaderBlob;
	std::string filenameUtf8	= renderPlatform->GetShaderBinaryPath();
	filenameUtf8				+= "/";
	filenameUtf8				+= filename_utf8;
	fileLoader->AcquireFileContents(shaderBlob.pData, shaderBlob.DataSize, filenameUtf8.c_str(), false);
	if (!shaderBlob.pData)
	{
		// Some engines force filenames to lower case because reasons:
		std::transform(filenameUtf8.begin(), filenameUtf8.end(), filenameUtf8.begin(), ::tolower);
		fileLoader->AcquireFileContents(shaderBlob.pData, shaderBlob.DataSize, filenameUtf8.c_str(), false);
		if (!shaderBlob.pData)
		{
			return;
		}
	}

	// Copy shader
	type = t;
	if (t == crossplatform::SHADERTYPE_PIXEL)
	{
		D3DCreateBlob(shaderBlob.DataSize, &pixelShader12);
		memcpy(pixelShader12->GetBufferPointer(), shaderBlob.pData, pixelShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_VERTEX)
	{
		D3DCreateBlob(shaderBlob.DataSize, &vertexShader12);
		memcpy(vertexShader12->GetBufferPointer(), shaderBlob.pData, vertexShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_COMPUTE)
	{
		D3DCreateBlob(shaderBlob.DataSize, &computeShader12);
		memcpy(computeShader12->GetBufferPointer(), shaderBlob.pData, computeShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_GEOMETRY)
	{
		SIMUL_CERR << "Geometry shaders are not implemented in Dx12" << std::endl;
	}

	// Free the loaded memory
	fileLoader->ReleaseFileContents(shaderBlob.pData);
}

void Effect::EnsureEffect(crossplatform::RenderPlatform *r, const char *filename_utf8)
{
#ifndef _XBOX_ONE
	// We will only recompile if we are in windows
	// SFX will handle the "if changed"
	auto buildMode = r->GetShaderBuildMode();
	if ((buildMode & crossplatform::BUILD_IF_CHANGED) != 0)
	{
		std::string simulPath = std::getenv("SIMUL");

		if (simulPath != "")
		{
			// Sfx path
			std::string exe = simulPath;
			exe += "\\Tools\\bin\\Sfx.exe";
			std::wstring sfxPath(exe.begin(), exe.end());

			/*
			Command line:
				argv[0] we need to pass the module name
				<input.sfx>
				-I <include path; include path>
				-O <output>
				-P <config.json>
			*/
			std::string cmdLine = exe;
			{
				// File to compile
				cmdLine += " " + simulPath + "\\Platform\\CrossPlatform\\SFX\\" + filename_utf8 + ".sfx";

				// Includes
				cmdLine += " -I\"" + simulPath + "\\Platform\\DirectX12\\HLSL;";
				cmdLine += simulPath + "\\Platform\\CrossPlatform\\SL\"";

				// Platform file
				cmdLine += " -P\"" + simulPath + "\\Platform\\DirectX12\\HLSL\\HLSL12.json\"";
				
				// Ouput file
				std::string outDir = r->GetShaderBinaryPath();
				for (unsigned int i = 0; i < outDir.size(); i++)
				{
					if (outDir[i] == '/')
					{
						outDir[i] = '\\';
					}
				}
				if (outDir[outDir.size() - 1] == '\\')
				{
					outDir.erase(outDir.size() - 1);
				}

				cmdLine += " -O\"" + outDir + "\"";
				// Intermediate folder
				cmdLine += " -M\"";				
				cmdLine +=simulPath + "\\Platform\\DirectX12\\sfx_intermediate\"";
				// Force
				if ((buildMode & crossplatform::ShaderBuildMode::ALWAYS_BUILD) != 0)
				{
					cmdLine += " -F";
				}
			}

			// Convert the command line to a wide string
			size_t newsize = cmdLine.size() + 1;
			wchar_t* wcstring = new wchar_t[newsize];
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wcstring, newsize, cmdLine.c_str(), _TRUNCATE);

			// Setup pipes to get cout/cerr
			SECURITY_ATTRIBUTES secAttrib	= {};
			secAttrib.nLength				= sizeof(SECURITY_ATTRIBUTES);
			secAttrib.bInheritHandle		= TRUE;
			secAttrib.lpSecurityDescriptor	= NULL;

			HANDLE coutWrite				= 0;
			HANDLE coutRead					= 0;
			HANDLE cerrWrite				= 0;
			HANDLE cerrRead					= 0;

			CreatePipe(&coutRead, &coutWrite, &secAttrib, 100);
			CreatePipe(&cerrRead, &cerrWrite, &secAttrib, 100);

			// Create the process
			STARTUPINFO startInfo			= {};
			startInfo.hStdOutput			= coutWrite;
			startInfo.hStdError				= cerrWrite;
			PROCESS_INFORMATION processInfo = {};
			bool success = CreateProcess
			(
				sfxPath.c_str(), wcstring,
				NULL, NULL, false, CREATE_NO_WINDOW, NULL, NULL,
				&startInfo, &processInfo
			);

			// Wait until if finishes
			if (success)
			{
				// Wait for the main handle and the output pipes
				HANDLE hWaitHandles[]	= {processInfo.hProcess, coutRead, cerrRead };
				DWORD ret				= WaitForMultipleObjects(3, hWaitHandles, FALSE, INFINITE);

				// Print the pipes
				const DWORD BUFSIZE = 4096;
				BYTE buff[BUFSIZE];
				DWORD dwBytesRead;
				DWORD dwBytesAvailable;
				while (PeekNamedPipe(coutRead, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
				{
					ReadFile(coutRead, buff, BUFSIZE - 1, &dwBytesRead, 0);
					SIMUL_COUT << std::string((char*)buff, (size_t)dwBytesRead).c_str();
				}
				while (PeekNamedPipe(cerrRead, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
				{
					ReadFile(cerrRead, buff, BUFSIZE - 1, &dwBytesRead, 0);
					SIMUL_COUT << std::string((char*)buff, (size_t)dwBytesRead).c_str();
				}
			}
			else
			{
				DWORD error = GetLastError();
				SIMUL_COUT << "Could not create the sfx process. Error:" << error << std::endl;
				return;
			}
		}
		else
		{
			SIMUL_COUT << "The env var SIMUL is not defined, skipping rebuild. \n";
			return;
		}
	}
#endif
}

void Effect::Load(crossplatform::RenderPlatform *r,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{	
	EnsureEffect(r, filename_utf8);
	crossplatform::Effect::Load(r, filename_utf8, defines);
}

void Effect::PostLoad()
{
	// Ensure slots are correct
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			dx12::Shader* v = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_VERTEX];
			dx12::Shader* p = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_PIXEL];
			dx12::Shader* c = (dx12::Shader*)pass.second->shaders[crossplatform::SHADERTYPE_COMPUTE];
			// Graphics pipeline
			if (v && p)
			{
				CheckShaderSlots(v, v->vertexShader12);
				CheckShaderSlots(p, p->pixelShader12);
				pass.second->SetConstantBufferSlots	(p->constantBufferSlots | v->constantBufferSlots);
				pass.second->SetTextureSlots		(p->textureSlots		| v->textureSlots);
				pass.second->SetTextureSlotsForSB	(p->textureSlotsForSB	| v->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(p->rwTextureSlots		| v->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(p->rwTextureSlotsForSB | v->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(p->samplerSlots		| v->samplerSlots);
			}
			// Compute pipeline
			if (c)
			{
				CheckShaderSlots(c, c->computeShader12);
				pass.second->SetConstantBufferSlots	(c->constantBufferSlots);
				pass.second->SetTextureSlots		(c->textureSlots);
				pass.second->SetTextureSlotsForSB	(c->textureSlotsForSB);
				pass.second->SetRwTextureSlots		(c->rwTextureSlots);
				pass.second->SetRwTextureSlotsForSB	(c->rwTextureSlotsForSB);
				pass.second->SetSamplerSlots		(c->samplerSlots);
			}
			dx12::EffectPass *ep=(EffectPass *)pass.second;
			ep->MakeResourceSlotMap();
		}
	}
}

dx12::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	for (auto& tech : techniques)
	{
		for (auto& pass : tech.second->passes_by_name)
		{
			auto pass12 = (dx12::EffectPass*)pass.second;
			pass12->InvalidateDeviceObjects();
		}
	}
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByName(const char *name)
{
	if(techniques.find(name)!=techniques.end())
	{
		return techniques[name];
	}
	auto g=groups[""];
	if(g->techniques.find(name)!=g->techniques.end())
	{
		return g->techniques[name];
	}
	return NULL;
}

crossplatform::EffectTechnique *dx12::Effect::GetTechniqueByIndex(int index)
{
	if(techniques_by_index.find(index)!=techniques_by_index.end())
	{
		return techniques_by_index[index];
	}
	auto g=groups[""];
	if(g->techniques_by_index.find(index)!=g->techniques_by_index.end())
	{
		return g->techniques_by_index[index];
	}
	return NULL;
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	crossplatform::Effect::Apply(deviceContext,effectTechnique,pass_num);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	crossplatform::Effect::Apply(deviceContext,effectTechnique,passname);
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n",this->filename.c_str()));
	apply_count--;
	crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
	cs->textureAssignmentMapValid = false;
	cs->rwTextureAssignmentMapValid = false;
	crossplatform::Effect::Apply(deviceContext, currentTechnique, currentPass);
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::Unapply(deviceContext);
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext,  crossplatform::ConstantBufferBase *s)
{
	RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
	s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());
	crossplatform::Effect::SetConstantBuffer(deviceContext, s);
}

void Effect::CheckShaderSlots(dx12::Shader * shader, ID3DBlob * shaderBlob)
{
	HRESULT res = S_FALSE;
	// Load the shader reflection code
	if (!shader->mShaderReflection)
	{
		res = D3DReflect(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), IID_PPV_ARGS(&shader->mShaderReflection));
		SIMUL_ASSERT(res == S_OK);
	}

	// Get shader description
	D3D12_SHADER_DESC shaderDesc = {};
	res = shader->mShaderReflection->GetDesc(&shaderDesc);
	SIMUL_ASSERT(res == S_OK);

	// Reset the slot counter
	shader->textureSlots		= 0;
	shader->textureSlotsForSB	= 0;
	shader->rwTextureSlots		= 0;
	shader->rwTextureSlotsForSB = 0;
	shader->constantBufferSlots = 0;
	shader->samplerSlots		= 0;

	// Check the shader bound resources
	for (unsigned int i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC slotDesc = {};
		res = shader->mShaderReflection->GetResourceBindingDesc(i, &slotDesc);
		SIMUL_ASSERT(res == S_OK);

		D3D_SHADER_INPUT_TYPE type	= slotDesc.Type;
		UINT slot					= slotDesc.BindPoint;
		if (type == D3D_SIT_CBUFFER)
		{
			// NOTE: Local resources declared in the shader (like a matrix or a float4) will be added
			// to a global constant buffer but we shouldn't add it to our list
			// It looks like the only way to know if it is the globla is by the name and because it 
			// does have the flag '0'
			if (slotDesc.uFlags)
			{
			shader->setUsesConstantBufferSlot(slot);
		}
		}
		else if (type == D3D_SIT_TBUFFER)
		{
			SIMUL_BREAK("Check this");
		}
		else if (type == D3D_SIT_TEXTURE)
		{
			shader->setUsesTextureSlot(slot);
		}
		else if (type == D3D_SIT_SAMPLER)
		{
			shader->setUsesSamplerSlot(slot);
		}
		else if (type == D3D_SIT_UAV_RWTYPED)
		{
			shader->setUsesRwTextureSlot(slot);
		}
		else if (type == D3D_SIT_STRUCTURED)
		{
			shader->setUsesTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED)
		{
			shader->setUsesRwTextureSlotForSB(slot);
		}
		else if (type == D3D_SIT_BYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWBYTEADDRESS)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_APPEND_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_CONSUME_STRUCTURED)
		{
			SIMUL_BREAK("");
		}
		else if (type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER)
		{
			SIMUL_BREAK("");
		}
		else
		{
			SIMUL_BREAK("Unknown slot type.");
		}
	}
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx12::EffectPass;
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}

void EffectPass::InvalidateDeviceObjects()
{
	// TO-DO: nice memory leaks here
	mComputePso = nullptr;
	mGraphicsPsoMap.clear();
}

void EffectPass::Apply(crossplatform::DeviceContext &deviceContext,bool asCompute)
{		
	// DX12: For each pass we'll have a PSO. PSOs hold: shader bytecode, input vertex format,
	// primitive topology type, blend state, rasterizer state, depth stencil state, msaa parameters, 
	// output buffer and root signature
	auto curRenderPlat = (dx12::RenderPlatform*)deviceContext.renderPlatform;
	if (	(mGraphicsPsoMap.empty() && !mComputePso) ||
			(((mGraphicsPsoMap.find(curRenderPlat->GetCurrentPixelFormat()) == mGraphicsPsoMap.end()) && !mComputePso)))
	{
		// Shader bytecode
		dx12::Shader *v = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
		dx12::Shader *p = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
		dx12::Shader *c = (dx12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

		if(v && p && c)
			SIMUL_BREAK("A pass can't have pixel,vertex and compute shaders.")
		// Init as graphics
		if (v && p)
		{
			// Build a new pso pair <pixel format, PSO>
			std::pair<DXGI_FORMAT, ID3D12PipelineState*> psoPair;
			psoPair.first	= curRenderPlat->GetCurrentPixelFormat();
			mIsCompute		= false;
			
			// Try to get the input layout (if none, we dont need to set it to the pso)
			D3D12_INPUT_LAYOUT_DESC* pCurInputLayout		= curRenderPlat->GetCurrentInputLayout();

			// Load the rendering states (blending, rasterizer and depth&stencil)
			dx12::RenderState *effectBlendState			= (dx12::RenderState*)(blendState);
			dx12::RenderState *effectRasterizerState	= (dx12::RenderState*)(rasterizerState);
			dx12::RenderState *effectDepthStencilState	= (dx12::RenderState*)(depthStencilState);

			// Find out the states this effect will use
			CD3DX12_BLEND_DESC defaultBlend	= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			if (effectBlendState)
			{
				defaultBlend = CD3DX12_BLEND_DESC(effectBlendState->BlendDesc);
			}

			CD3DX12_RASTERIZER_DESC defaultRaster = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			if (effectRasterizerState)
			{
				defaultRaster = CD3DX12_RASTERIZER_DESC(effectRasterizerState->RasterDesc);
			}

			CD3DX12_DEPTH_STENCIL_DESC defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			if (deviceContext.viewStruct.frustum.reverseDepth)
			{
				defaultDepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
			}
			if (effectDepthStencilState)
			{
				defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(effectDepthStencilState->DepthStencilDesc);
				if (deviceContext.viewStruct.frustum.reverseDepth)
				{
					defaultDepthStencil.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
				}
			}

			// Find the current primitive topology type
			D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = {};
			D3D12_PRIMITIVE_TOPOLOGY curTopology		= curRenderPlat->GetCurrentPrimitiveTopology();

			// Invalid
			if (curTopology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
			{
				primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			// Point
			else if (curTopology == D3D_PRIMITIVE_TOPOLOGY_POINTLIST)
			{
				primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			}
			// Lines
			else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_LINELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_LINESTRIP)
			{
				primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			}
			// Triangles
			else if (curTopology >= D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST && curTopology <= D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ)
			{
				primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			// Patches
			else
			{
				primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
			}

			// Build the PSO description 
			D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc = {};
			gpsoDesc.pRootSignature					= curRenderPlat->GetGraphicsRootSignature();
			gpsoDesc.VS								= CD3DX12_SHADER_BYTECODE(v->vertexShader12);
			gpsoDesc.PS								= CD3DX12_SHADER_BYTECODE(p->pixelShader12);
			gpsoDesc.InputLayout.NumElements		= pCurInputLayout ? pCurInputLayout->NumElements : 0;
			gpsoDesc.InputLayout.pInputElementDescs	= pCurInputLayout ? pCurInputLayout->pInputElementDescs : nullptr;
			gpsoDesc.RasterizerState				= defaultRaster;
			gpsoDesc.BlendState						= defaultBlend;
			gpsoDesc.DepthStencilState				= defaultDepthStencil;
			gpsoDesc.SampleMask						= UINT_MAX;
			gpsoDesc.PrimitiveTopologyType			= primitiveType;
			gpsoDesc.NumRenderTargets				= 1;
			gpsoDesc.RTVFormats[0]					= psoPair.first;
			gpsoDesc.DSVFormat						= DXGI_FORMAT_D32_FLOAT;
			gpsoDesc.SampleDesc.Count				= 1;

			HRESULT res = curRenderPlat->AsD3D12Device()->CreateGraphicsPipelineState
			(
				&gpsoDesc,
#ifdef _XBOX_ONE
				IID_GRAPHICS_PPV_ARGS(&psoPair.second)
#else
				IID_PPV_ARGS(&psoPair.second)
#endif
			);

#ifndef _XBOX_ONE
			// On XBOXONE we must provide the a root signature with the compiled shader,
			// if not, it will complain about it and will actually go ahead and do it at runtime.
			// It is causing this assert to be triggered...
			SIMUL_ASSERT(res == S_OK);
#endif 

			std::wstring name = L"GraphicsPSO_";
			std::string techName = deviceContext.renderPlatform->GetContextState(deviceContext)->currentTechnique->name;
			name += std::wstring(techName.begin(), techName.end());
			if(psoPair.second)
				psoPair.second->SetName(name.c_str());
			else
			{
				// Having failed to set posPair.second, we either get a null pointer reference here, or later, when we try to set the pso.
				SIMUL_ASSERT(res == S_OK);
			}
			
			mGraphicsPsoMap.insert(psoPair);
		}
		// Init as compute
		else if (c)
		{
			mIsCompute = true;

			D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc	= {};
			cpsoDesc.CS									= CD3DX12_SHADER_BYTECODE(c->computeShader12);
#ifndef _XBOX_ONE
			cpsoDesc.Flags								= D3D12_PIPELINE_STATE_FLAG_NONE;
#endif
			cpsoDesc.pRootSignature						= curRenderPlat->GetGraphicsRootSignature();
			cpsoDesc.NodeMask							= 0;
			HRESULT res = curRenderPlat->AsD3D12Device()->CreateComputePipelineState
			(
				&cpsoDesc,
#ifdef _XBOX_ONE
				IID_GRAPHICS_PPV_ARGS(&mComputePso)
#else
				IID_PPV_ARGS(&mComputePso)
#endif
			);

#ifndef _XBOX_ONE
			// On XBOXONE we must provide the a root signature with the compiled shader,
			// if not, it will complain about it and will actually go ahead and do it at runtime.
			// It is causing this assert to be triggered...
			SIMUL_ASSERT(res == S_OK);
#endif 
			if(res == S_OK)
			{
			std::wstring name = L"ComputePSO_";
			std::string techName = deviceContext.renderPlatform->GetContextState(deviceContext)->currentTechnique->name;
			name += std::wstring(techName.begin(), techName.end());
			mComputePso->SetName(name.c_str());
		}
		else
		{
#ifndef _XBOX_ONE
				SIMUL_BREAK("Failed to create PSO")
				return;
#endif 
			}
		}
		else
		{
			SIMUL_BREAK("Could't find any shaders")
		}
	}

	// Set current RootSignature and PSO
	// We will set the DescriptorHeaps and RootDescriptorTablesArguments from ApplyContextState (at RenderPlatform)
	auto cmdList	= curRenderPlat->AsD3D12CommandList();
	if (mIsCompute)
	{
		cmdList->SetPipelineState(mComputePso);
	}
	else
	{
		auto pFormat = curRenderPlat->GetCurrentPixelFormat();
		cmdList->SetPipelineState(mGraphicsPsoMap[pFormat]);
	}
}

void EffectPass::SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers, dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	int usedSlots			= 0;
	auto nullSampler		= rPlat->GetNullSampler();

	// The handles for the required constant buffers:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSamplers> srcHandles;
	for(int i=0;i<numSamplerResourcerSlots;i++)
	{
		int slot=samplerResourceSlots[i];
		auto samp	= samplers[slot];
#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
		if (!samp)
		{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". Sampler slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
		}
#endif
		srcHandles[slot] = *samp->AsD3D12SamplerState();
		usedSlots		|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSamplers; s++)
	{
		if (!usesSamplerSlot(s))
		{
			// Hardware tier 1 requires all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), nullSampler, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, samplerHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
			samplerHeap->Offset();
	}

#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
	// Check slots
	unsigned int requiredSlots = GetSamplerSlots();
	if (requiredSlots != usedSlots)
	{
		unsigned int missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSamplers; i++)
		{
			unsigned int testSlot = 1 << i;
			if (testSlot & missingSlots)
			{
				SIMUL_CERR << "Resource binding error at: " << mPassName << ". Sampler slot " << i << " was not set!" << std::endl;
		}
		}
	}
#endif 
}

void EffectPass::SetConstantBuffers(crossplatform::ConstantBufferAssignmentMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	int usedSlots			= 0;
	auto nullCbv			= rPlat->GetNullCBV();

	// The handles for the required constant buffers:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumCBV> srcHandles;
	for (int i = 0; i < numConstantBufferResourceSlots; i++)
{
		int slot	= constantBufferResourceSlots[i];
		auto cb		= cBuffers[slot];
#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
		if (!cb || !usesConstantBufferSlot(slot) || slot != cb->GetIndex())
	{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". Constant buffer slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
	}
#endif
		auto d12cb			= (dx12::PlatformConstantBuffer*)cb->GetPlatformConstantBuffer();
		srcHandles[slot]	= d12cb->AsD3D12ConstantBuffer();
		usedSlots			|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumCBV; s++)
	{
		if (!usesConstantBufferSlot(s))
	{
			// Hardware tiers 1 & 2 require all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
		{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullCbv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}

#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
	// Check slots
	unsigned int requiredSlots = GetConstantBufferSlots();
	if (requiredSlots != usedSlots)
	{
		unsigned int missingSlots = requiredSlots & (~usedSlots);
		for (int i = 0; i < 32; i++)
		{
			unsigned int testSlot = 1 << i;
			if (testSlot & missingSlots)
			{
				SIMUL_CERR << "Resource binding error at: " << mPassName << ". Constant buffer slot " << i << " was not set!" << std::endl;
			}
		}
	}
#endif 
}

void EffectPass::SetSRVs(crossplatform::TextureAssignmentMap& textures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context)
{
	auto rPlat				= (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullSrv			= rPlat->GetNullSRV();
	int usedSBSlots			= 0;
	int usedTextureSlots	= 0;

	// The handles for the required SRVs:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSRV>	srcHandles;
	memset(srcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumSRV);
	std::array<bool, ResourceBindingLimits::NumSRV>							usedSlotsArray;
	memset(usedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumSRV);

	// Iterate over the textures:
	for (int i = 0; i < numResourceSlots; i++)
{	
		int slot	= resourceSlots[i];
		auto ta		= textures[slot];

		if (!usesTextureSlot(slot))
	{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". Texture slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
		}

		// If the texture is null or invalid, set a dummy:
		if (!ta.texture || !ta.texture->IsValid())
		{
			if (ta.dimensions == 2)
			{
				ta.texture = rPlat->GetDummy2D();
			}
			else if (ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
			else
			{
				SIMUL_CERR << "We only have dummy for 2D and 3D textures. \n";
			}
		}
		srcHandles[slot]	= *ta.texture->AsD3D12ShaderResourceView(true, ta.resourceType, ta.index, ta.mip);
		usedSlotsArray[slot]= true;
		usedTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numSbResourceSlots; i++)
	{
		int slot = sbResourceSlots[i];
		if (usedSlotsArray[slot])
		{
			SIMUL_CERR << "The slot: " << slot << " at pass: " << mPassName << ", has already being used by a texture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb || !usesTextureSlotForSB(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". Structured buffer slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
		}
		srcHandles[slot]		= *sb->AsD3D12ShaderResourceView(context);
		usedSlotsArray[slot]	= true;
		usedSBSlots				|= (1 << slot);
	}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		if (!usesTextureSlot(s) && !usesTextureSlotForSB(s))
		{
			// Hardware tier 1 requires all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullSrv, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
			frameHeap->Offset();
	}


#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
	// Check texture slots
	unsigned int requiredSlotsTexture = this->GetTextureSlots();
	if (requiredSlotsTexture != usedTextureSlots)
	{
		unsigned int missingSlots = requiredSlotsTexture & (~usedTextureSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSRV; i++)
		{
			if (this->usesTextureSlot(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mPassName << ". Texture slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}

	// Check structured buffer slots
	unsigned int requiredSlotsSB = GetStructuredBufferSlots();
	if (requiredSlotsSB != usedSBSlots)
	{
		unsigned int missingSlots = requiredSlotsSB & (~usedSBSlots);
		for (int i = 0; i < ResourceBindingLimits::NumSRV; i++)
		{
			if (usesTextureSlotForSB(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mPassName << ". Structured buffer slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}
#endif
}

void EffectPass::SetUAVs(crossplatform::TextureAssignmentMap & rwTextures, crossplatform::StructuredBufferAssignmentMap & sBuffers, dx12::Heap * frameHeap, ID3D12Device * device, crossplatform::DeviceContext & context)
{	
	auto rPlat = (dx12::RenderPlatform*)context.renderPlatform;
	const auto resLimits	= rPlat->GetResourceBindingLimits();
	auto nullUav			= rPlat->GetNullUAV();
	int usedRwSBSlots		= 0;
	int usedRwTextureSlots	= 0;

	// The the handles for the required UAVs:
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumUAV>	srcHandles;
	memset(srcHandles.data(), 0, sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * ResourceBindingLimits::NumUAV);
	std::array<bool, ResourceBindingLimits::NumUAV>							usedSlotsArray;
	memset(usedSlotsArray.data(), 0, sizeof(bool) * ResourceBindingLimits::NumUAV);

	// Iterate over the textures:
	for (int i = 0; i < numRwResourceSlots; i++)
	{
		int slot	= rwResourceSlots[i];
		auto ta		= rwTextures[slot];

		if (!usesRwTextureSlot(slot))
		{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". RWTexture slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
		}

		// If the texture is null or invalid, set a dummy:
		if (!ta.texture || !ta.texture->IsValid())
		{
			if (ta.dimensions == 2)
			{
				ta.texture = rPlat->GetDummy2D();
			}
			else if (ta.dimensions == 3)
			{
				ta.texture = rPlat->GetDummy3D();
			}
			else
			{
				SIMUL_CERR << "We only have dummy for 2D and 3D textures. \n";
		}
		}
		srcHandles[slot]	= *ta.texture->AsD3D12UnorderedAccessView(ta.index, ta.mip);
		usedSlotsArray[slot]= true;
		usedRwTextureSlots |= (1 << slot);
	}
	// Iterate over the structured buffers:
	for (int i = 0; i < numRwSbResourceSlots; i++)
	{
		int slot = rwSbResourceSlots[i];
		if (usedSlotsArray[slot])
		{
			SIMUL_CERR << "The slot: " << slot << " at pass: " << mPassName << ", has already being used by a RWTexture. \n";
		}
		auto sb = (dx12::PlatformStructuredBuffer*)sBuffers[slot];
		if (!sb || !usesRwTextureSlotForSB(slot))
			{
			SIMUL_CERR << "Resource binding error at: " << mPassName << ". RWStructured buffer slot " << slot << " is invalid." << std::endl;
			SIMUL_BREAK("");
				}
		srcHandles[slot]		= *sb->AsD3D12UnorderedAccessView(context);
		usedSlotsArray[slot]	= true;
		usedRwSBSlots			|= (1 << slot);
			}
	// Iterate over all the slots and fill them:
	for (int s = 0; s < ResourceBindingLimits::NumSRV; s++)
	{
		if (!usesRwTextureSlot(s) && !usesRwTextureSlotForSB(s))
		{
			// Hardware tiers 1 & 2 require all slots to be filed:
			if (resLimits.BindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), nullUav, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
		else
		{
			device->CopyDescriptorsSimple(1, frameHeap->CpuHandle(), srcHandles[s], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		frameHeap->Offset();
	}


#ifdef SIMUL_DX12_SLOTS_CHECK || _DEBUG
	// Check RwTexture slots
	unsigned int requiredSlotsTexture = GetRwTextureSlots();
	if (requiredSlotsTexture != usedRwTextureSlots)
	{
		unsigned int missingSlots = requiredSlotsTexture & (~usedRwTextureSlots);
		for (int i = 0; i < ResourceBindingLimits::NumUAV; i++)
		{
			if (this->usesRwTextureSlot(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mPassName << ". RwTexture slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}

	// Check RwStructured buffer slots
	unsigned int requiredSlotsSB = GetRwStructuredBufferSlots();
	if (requiredSlotsSB != usedRwSBSlots)
	{
		unsigned int missingSlots = requiredSlotsSB & (~usedRwSBSlots);
		for (int i = 0; i < ResourceBindingLimits::NumUAV; i++)
		{
			if (usesRwTextureSlotForSB(i))
			{
				unsigned int testSlot = 1 << i;
				if (testSlot & missingSlots)
				{
					SIMUL_CERR << "Resource binding error at: " << mPassName << ". RwStructured buffer slot " << i << " was not set!" << std::endl;
				}
			}
		}
	}
#endif
}

EffectPass::EffectPass()
{
}

EffectPass::~EffectPass()
{
	for (auto& ele : mGraphicsPsoMap)
	{
		SAFE_RELEASE(ele.second);
	}
	mGraphicsPsoMap.clear();
}
