#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX11on12/RenderPlatform.h"
#include "D3dx11effect.h"
#include "SimulDirectXHeader.h"

#include "d3dcompiler.h"	// D3DCreateBlob ()
#include "d3dx12.h"

#include <algorithm>
#include <string>

using namespace simul;
using namespace dx11on12;
#pragma optimize("",off)

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

D3D11_QUERY toD3dQueryType(crossplatform::QueryType t)
{
	switch(t)
	{
		case crossplatform::QUERY_OCCLUSION:
			return D3D11_QUERY_OCCLUSION;
		case crossplatform::QUERY_TIMESTAMP:
			return D3D11_QUERY_TIMESTAMP;
		case crossplatform::QUERY_TIMESTAMP_DISJOINT:
			return D3D11_QUERY_TIMESTAMP_DISJOINT;
		default:
			return D3D11_QUERY_EVENT;
	};
}

void Query::SetName(const char *name)
{
/*
	for(int i=0;i<QueryLatency;i++)
		SetDebugObjectName( d3d11Query[i], name );
*/
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
/*
	InvalidateDeviceObjects();
	ID3D11Device *m_pd3dDevice=r->AsD3D11Device();
	D3D11_QUERY_DESC qdesc=
	{
		toD3dQueryType(type),0
	};
	for(int i=0;i<QueryLatency;i++)
	{
		gotResults[i]=true;
		doneQuery[i]=false;
		m_pd3dDevice->CreateQuery(&qdesc,&d3d11Query[i]);
	}
*/
}
void Query::InvalidateDeviceObjects() 
{
	for(int i=0;i<QueryLatency;i++)
		SAFE_RELEASE(d3d11Query[i]);
	for(int i=0;i<QueryLatency;i++)
	{
		gotResults[i]=true;
		doneQuery[i]=false;
	}
}

void Query::Begin(crossplatform::DeviceContext &deviceContext)
{
/*
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->Begin(d3d11Query[currFrame]);
*/
}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
/*
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->End(d3d11Query[currFrame]);
	gotResults[currFrame]=false;
	doneQuery[currFrame]=true;
*/
}

bool Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
{
/*
	gotResults[currFrame]=true;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	currFrame = (currFrame + 1) % QueryLatency;
	if(!doneQuery[currFrame])
		return false;
	// Get the data from the "next" query - which is the oldest!
	HRESULT hr=pContext->GetData(d3d11Query[currFrame],data,(UINT)sz,0);
	if(hr== S_OK)
	{
		gotResults[currFrame]=true;
	}
	return hr== S_OK;
*/
	return true;
}

RenderState::RenderState()
	:m_depthStencilState(NULL)
	,m_blendState(NULL)
	,m_rasterizerState(NULL)
{
	BlendDesc			= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	RasterDesc			= CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	DepthStencilDesc	= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	RasterDesc.FrontCounterClockwise = true;	
}

RenderState::~RenderState()
{
	SAFE_RELEASE(m_depthStencilState)
	SAFE_RELEASE(m_blendState)
	SAFE_RELEASE(m_rasterizerState)
}

PlatformStructuredBuffer::PlatformStructuredBuffer():
	num_elements(0),
	element_bytesize(0),
	mBufferDefault(nullptr),
	mBufferUpload(nullptr),
	mBufferReadFront(nullptr),
	mBufferReadBack(nullptr),
	mChanged(false),
	mReadSrc(nullptr),
	mTempBuffer(nullptr),
	mRenderPlatform(nullptr)
{

}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data)
{
	HRESULT res			= S_FALSE;
	num_elements		= ct;
	element_bytesize	= unit_size;
	mTotalSize			= num_elements * element_bytesize;
	mRenderPlatform = (dx11on12::RenderPlatform*)renderPlatform;
	
	if (mTotalSize == 0)
	{
		SIMUL_BREAK("Invalid arguments for the SB creation");
	}

	mCurrentState		= computable ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ;

	// Default heap
	D3D12_RESOURCE_FLAGS bufferFlags = computable ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize,bufferFlags),
		mCurrentState,
		nullptr,
		IID_PPV_ARGS(&mBufferDefault)
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferDefault->SetName(L"SBDefaultBuffer");

	// Upload heap
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mBufferUpload)
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferUpload->SetName(L"SBUploadBuffer");

	// Initialize it if we have data
	if (init_data)
	{
		//SIMUL_BREAK_ONCE("Yep");
		D3D12_SUBRESOURCE_DATA dataToCopy	= {};
		dataToCopy.pData					= init_data;
		dataToCopy.RowPitch					= dataToCopy.SlicePitch = mTotalSize;

		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST);
		UpdateSubresources(mRenderPlatform->AsD3D12CommandList(), mBufferDefault, mBufferUpload, 0, 0, 1, &dataToCopy);
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState);
	}

	// Create the views
	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement				= 0;
	srvDesc.Buffer.NumElements				= num_elements;
	srvDesc.Buffer.StructureByteStride		= element_bytesize;
	srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;

	mBufferSrvHeap.Restore((dx11on12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBSrvHeap", false) ;
	renderPlatform->AsD3D12Device()->CreateShaderResourceView(mBufferDefault, &srvDesc, mBufferSrvHeap.CpuHandle());
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
		uavDesc.Buffer.NumElements					= num_elements;
		uavDesc.Buffer.StructureByteStride			= element_bytesize;
		uavDesc.Buffer.Flags						= D3D12_BUFFER_UAV_FLAG_NONE;

		mBufferUavHeap.Restore((dx11on12::RenderPlatform*)renderPlatform, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "SBUavHeap", false);
		renderPlatform->AsD3D12Device()->CreateUnorderedAccessView(mBufferDefault, nullptr, &uavDesc, mBufferUavHeap.CpuHandle());
		mUavHandle = mBufferUavHeap.CpuHandle();
		mBufferUavHeap.Offset();
	}

	// Create the READ_BACK buffers
	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mBufferReadBack)
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferReadBack->SetName(L"SBReadbackBuffer");

	res = renderPlatform->AsD3D12Device()->CreateCommittedResource
	(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mTotalSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&mBufferReadFront)
	);
	SIMUL_ASSERT(res == S_OK);
	mBufferReadFront->SetName(L"SBReadbackBuffer");

	// Create a temporal buffer that we will use to upload data to the GPU
	delete mTempBuffer;
	mTempBuffer = malloc(mTotalSize);
	memset(mTempBuffer, 0, mTotalSize);

	// Some cool memory log
	SIMUL_COUT << "[RAM] Allocating:" << std::to_string((float)mTotalSize / 1048576.0f) << ".MB" << " , " << mTotalSize << ".bytes" << std::endl;

	// Create the fence
	mCbFence.Restore(mRenderPlatform);
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &deviceContext, crossplatform::Effect *effect, const char *name)
{
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext, effect, name);

	// Is this what we want to do??
	if (mChanged)
	{
		D3D12_SUBRESOURCE_DATA dataToCopy = {};
		dataToCopy.pData = mTempBuffer;
		dataToCopy.RowPitch = dataToCopy.SlicePitch = mTotalSize;

		auto r = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;
		r->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_DEST);
		UpdateSubresources(r->AsD3D12CommandList(), mBufferDefault, mBufferUpload, 0, 0, 1, &dataToCopy);
		r->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_DEST, mCurrentState);

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
	HRESULT res = S_FALSE;

	// Debuggggg
	if (mLastFrame != deviceContext.frame_number)
	{
		mLastFrame = deviceContext.frame_number;
		mUseCount = 0;
	}
	mUseCount++;

	if (mFirstUse)
		mFirstUse = false;
	else
		mCbFence.WaitUntilCompleted((dx11on12::RenderPlatform*)deviceContext.renderPlatform);

	// Map from the last frame
	const CD3DX12_RANGE readRange(0, 0);
	res = mBufferReadFront->Map(0, &readRange, reinterpret_cast<void**>(&mReadSrc));
	SIMUL_ASSERT(res == S_OK);

	return mReadSrc;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	const CD3DX12_RANGE readRange(0, 0);
	mBufferReadFront->Unmap(0, &readRange);
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	// Check state
	bool changed = false;
	if ((mCurrentState & D3D12_RESOURCE_STATE_COPY_SOURCE) != D3D12_RESOURCE_STATE_COPY_SOURCE)
	{
		changed = true;
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, mCurrentState, D3D12_RESOURCE_STATE_COPY_SOURCE);
	}
	// Schedule a copy for the next frame
	deviceContext.renderPlatform->AsD3D12CommandList()->CopyResource(mBufferReadBack, mBufferDefault);
	// Restore state
	if (changed)
	{
		mRenderPlatform->ResourceTransitionSimple(mBufferDefault, D3D12_RESOURCE_STATE_COPY_SOURCE, mCurrentState);
	}

	mCbFence.Signal((dx11on12::RenderPlatform*)deviceContext.renderPlatform);

	// Swap it!
	std::swap(mBufferReadBack, mBufferReadFront);
}

void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &deviceContext,void *data)
{
	SIMUL_BREAK_ONCE("Nacho has to implement this");
}

void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	delete mTempBuffer;
	mTempBuffer = nullptr;

	SAFE_RELEASE(mBufferDefault);
	SAFE_RELEASE(mBufferUpload);
	SAFE_RELEASE(mBufferReadFront);
	SAFE_RELEASE(mBufferReadBack);
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{

}

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12ShaderResourceView()
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

D3D12_CPU_DESCRIPTOR_HANDLE* PlatformStructuredBuffer::AsD3D12UnorderedAccessView(int mip /*= 0*/)
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
	D3DX11_TECHNIQUE_DESC desc;
	ID3DX11EffectTechnique *tech=const_cast<EffectTechnique*>(this)->asD3DX11EffectTechnique();
	tech->GetDesc(&desc);
	return (int)desc.Passes;
}

dx11on12::Effect::Effect() 
{
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx11on12::EffectTechnique;
}


void Shader::load(crossplatform::RenderPlatform *renderPlatform, const char *filename_utf8, crossplatform::ShaderType t)
{
	simul::base::MemoryInterface *allocator = renderPlatform->GetMemoryInterface();
	void* pShaderBytecode;
	uint32_t BytecodeLength;
	simul::base::FileLoader *fileLoader = simul::base::FileLoader::GetFileLoader();
	std::string filenameUtf8 = renderPlatform->GetShaderBinaryPath();
	filenameUtf8 += "/";
	filenameUtf8 += filename_utf8;
	fileLoader->AcquireFileContents(pShaderBytecode, BytecodeLength, filenameUtf8.c_str(), false);
	if (!pShaderBytecode)
	{
		// Some engines force filenames to lower case because reasons:
		std::transform(filenameUtf8.begin(), filenameUtf8.end(), filenameUtf8.begin(), ::tolower);
		fileLoader->AcquireFileContents(pShaderBytecode, BytecodeLength, filenameUtf8.c_str(), false);
		if (!pShaderBytecode)
			return;
	}

	type = t;
	if (t == crossplatform::SHADERTYPE_PIXEL)
	{
		D3DCreateBlob(BytecodeLength, &pixelShader12);
		memcpy(pixelShader12->GetBufferPointer(), pShaderBytecode, pixelShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_VERTEX)
	{
		D3DCreateBlob(BytecodeLength, &vertexShader12);
		memcpy(vertexShader12->GetBufferPointer(), pShaderBytecode, vertexShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_COMPUTE)
	{
		D3DCreateBlob(BytecodeLength, &computeShader12);
		memcpy(computeShader12->GetBufferPointer(), pShaderBytecode, computeShader12->GetBufferSize());
	}
	else if (t == crossplatform::SHADERTYPE_GEOMETRY)
	{
		SIMUL_CERR << "Not implemented in DirectX 12" << std::endl;
		//SIMUL_BREAK_ONCE("Not implemented in DirectX 12");
	}
}

#define D3DCOMPILE_DEBUG 1
void Effect::Load(crossplatform::RenderPlatform *r,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{	
	crossplatform::Effect::Load(r, filename_utf8, defines);
}

dx11on12::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	platform_effect=e;
}

crossplatform::EffectTechnique *dx11on12::Effect::GetTechniqueByName(const char *name)
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
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	crossplatform::EffectTechnique *tech=new dx11on12::EffectTechnique;
	ID3DX11EffectTechnique *t=e->GetTechniqueByName(name);
	if(!t->IsValid())
	{
		SIMUL_CERR<<"Invalid Effect technique "<<name<<" in effect "<<this->filename.c_str()<<std::endl;
		if(this->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(this->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	tech->platform_technique=t;
	techniques[name]=tech;
	groups[""]->techniques[name]=tech;
	if(!tech->platform_technique)
	{
		SIMUL_BREAK_ONCE("NULL technique");
	}
	return tech;
}

crossplatform::EffectTechnique *dx11on12::Effect::GetTechniqueByIndex(int index)
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
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	ID3DX11EffectTechnique *t=e->GetTechniqueByIndex(index);
	if(!t)
		return NULL;
	D3DX11_TECHNIQUE_DESC desc;
	t->GetDesc(&desc);
	crossplatform::EffectTechnique *tech=NULL;
	if(techniques.find(desc.Name)!=techniques.end())
	{
		tech=techniques[desc.Name];
		techniques_by_index[index]=tech;
		return tech;;
	}
	tech=new dx11on12::EffectTechnique;
	tech->platform_technique=t;
	techniques[desc.Name]=tech;
	techniques_by_index[index]=tech;
	groups[""]->techniques[desc.Name]=tech;
	groups[""]->techniques_by_index[index]=tech;
	return tech;
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
	crossplatform::Effect::Apply(deviceContext, currentTechnique, currentPass);
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::Unapply(deviceContext);
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext, const char *name, crossplatform::ConstantBufferBase *s)
{
	RenderPlatform *r = (RenderPlatform *)deviceContext.renderPlatform;
	s->GetPlatformConstantBuffer()->Apply(deviceContext, s->GetSize(), s->GetAddr());
	crossplatform::Effect::SetConstantBuffer(deviceContext, name, s);
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx11on12::EffectPass;
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}

void EffectPass::Apply(crossplatform::DeviceContext &deviceContext,bool test)
{
	// Note: If we change the buffers or maybe the textures....
	// we will need another set of PSO and RootSignatures!!!
	// It would be interesting that each effectpass had a list
	// off pso and roots combination so we don't have to toss out anything 
	// I think that in the Dx12MiniEngine they have something like this, they 
	// also use a Hash to indentify each set of PSO and rootS combination


	// DX12:The root signature (on creation) should describe all the resources the shader will be using 
	// textures, constant buffers etc. During command list recording, we will first set the active
	// root signature and then , we will be setting 
	// those resources using calls to cmdList->Set*Root* (SetGraphicsRoot32Constant for example)

	// RootConstants and RootDescriptors can hold CBV,SRV/UAV
	// RootTables	Textures need to be in here

	// Root Signatures have a limit of 64 DWORDS. On old hardware, only 16 DWORDS are in dedicated video
	// the rest will be addressed from slower memory. That means that we could have the resources that change
	// most on top (the first elements)
	// https://en.wikipedia.org/wiki/Feature_levels_in_Direct3D

	// Changing from one RootSignature to another will unbind all the resources, so we will have to
	// rebind them. Ideally, we will only have one RootSignature (HeheHe). Setting the same RootSignature again
	// will not cause this

	// RootConstants and RootDescriptors are automagically versioned by the driver, this means, that we could have
	// a resource that will change each draw call (ex: constant buffer for MVP matrix) and change it after each call
	// and the driver will save the contents of each state.
	// For RootTables, its task of the developer to version it >(
	
	if (!mRootS)
	{
		// The root parameters list
		std::vector<CD3DX12_ROOT_PARAMETER> rootParams;

		// Fill SRV_CBV_UAV ranges
		std::vector<CD3DX12_DESCRIPTOR_RANGE> srvCbvUavRanges;

		// Constant Buffers
		if (usesBuffers())
		{
			for (unsigned slot = 0; slot < 32; slot++)
			{
				if (usesBufferSlot(slot))
				{
					srvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, slot));
				}
			}
		}
		// Textures & RWTextures
		if (usesTextures())
		{
			for (unsigned slot = 0; slot < 32; slot++)
			{
				if (usesTextureSlot(slot))
				{
					srvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, slot));
				}
			}
			for (unsigned slot = 0; slot < 32; slot++)
			{
				if (usesTextureSlot(slot + 1000))
				{
					srvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, slot));
				}
			}
		}
		// Structured Buffers & RWStructured Buffers
		if (usesSBs())
		{
			for (unsigned int slot = 0; slot < 32; slot++)
			{
				if (usesTextureSlotForSB(slot))
				{
					srvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, slot));
				}
			}
			for (unsigned int slot = 0; slot < 32; slot++)
			{
				if (usesTextureSlotForSB(slot + 1000))
				{
					srvCbvUavRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, slot));
				}
			}
		}

		// Samplers
		std::vector<CD3DX12_DESCRIPTOR_RANGE> samplerRanges;
		if (usesSamplers())
		{
			for (unsigned slot = 0; slot < 32; slot++)
			{
				if (usesSamplerSlot(slot))
				{
					samplerRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, slot));
				}
			}
		}

		// Add a descriptor table holding all the CBV SRV and UAV
		// usesTextures() holds both Textures and UAV
		if (usesBuffers() || usesTextures() || usesSBs())
		{
			mSrvCbvUavTableIndex = rootParams.size();
			CD3DX12_ROOT_PARAMETER srvCbvUavParam = {};
			srvCbvUavParam.InitAsDescriptorTable(srvCbvUavRanges.size(), &srvCbvUavRanges[0]);
			rootParams.emplace_back(srvCbvUavParam);
		}

		// Add a descriptor table holding all the samplers
		if(usesSamplers())
		{
			mSamplerTableIndex = rootParams.size();
			CD3DX12_ROOT_PARAMETER samplerParam = {};
			samplerParam.InitAsDescriptorTable(samplerRanges.size(), &samplerRanges[0]);
			rootParams.emplace_back(samplerParam);
		}

		// Used to know if we are doing computing
		dx11on12::Shader *c = (dx11on12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

		// Build the root signature
		HRESULT res = S_FALSE;
		CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.Init
		(
			rootParams.size(),
			&rootParams[0],
			0,
			nullptr,
			c ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		);
		ID3DBlob* error;
		ID3DBlob* rootSerialized;
		res = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSerialized, &error);
		if (res != S_OK)
		{
			SIMUL_CERR << "Root signature serialization failed:" << std::endl;
			OutputDebugStringA((char*)error->GetBufferPointer());
		}
		SIMUL_ASSERT(res == S_OK);
		res = deviceContext.renderPlatform->AsD3D12Device()->CreateRootSignature(0, rootSerialized->GetBufferPointer(), rootSerialized->GetBufferSize(), IID_PPV_ARGS(&mRootS));
	}

	// DX12: For each pass we'll have a PSO. PSOs hold: shader bytecode, input vertex format,
	// primitive topology type, blend state, rasterizer state, depth stencil state, msaa parameters, 
	// output buffer and root signature
	auto curRenderPlat = (dx11on12::RenderPlatform*)deviceContext.renderPlatform;
	if (	(mGraphicsPsoMap.empty() && !mComputePso) ||
			(((mGraphicsPsoMap.find(curRenderPlat->GetCurrentPixelFormat()) == mGraphicsPsoMap.end()) && !mComputePso)))
	{
		// Shader bytecode
		dx11on12::Shader *v = (dx11on12::Shader*)shaders[crossplatform::SHADERTYPE_VERTEX];
		dx11on12::Shader *p = (dx11on12::Shader*)shaders[crossplatform::SHADERTYPE_PIXEL];
		dx11on12::Shader *c = (dx11on12::Shader*)shaders[crossplatform::SHADERTYPE_COMPUTE];

		if(v && p && c)SIMUL_BREAK("A pass can't have pixel,vertex and compute shaders.")
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
			dx11on12::RenderState *effectBlendState			= (dx11on12::RenderState *)(blendState);
			dx11on12::RenderState *effectRasterizerState	= (dx11on12::RenderState *)(rasterizerState);
			dx11on12::RenderState *effectDepthStencilState	= (dx11on12::RenderState *)(depthStencilState);

			// Find out the states this effect will use
			CD3DX12_BLEND_DESC defaultBlend	= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			if (effectBlendState)
				defaultBlend = CD3DX12_BLEND_DESC(effectBlendState->BlendDesc);

			CD3DX12_RASTERIZER_DESC defaultRaster = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			if (effectRasterizerState)
				defaultRaster = CD3DX12_RASTERIZER_DESC(effectRasterizerState->RasterDesc);

			CD3DX12_DEPTH_STENCIL_DESC defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			if (effectDepthStencilState)
				defaultDepthStencil = CD3DX12_DEPTH_STENCIL_DESC(effectDepthStencilState->DepthStencilDesc);

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
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.pRootSignature					= mRootS; 
			psoDesc.VS								= CD3DX12_SHADER_BYTECODE(v->vertexShader12);
			psoDesc.PS								= CD3DX12_SHADER_BYTECODE(p->pixelShader12);
			psoDesc.InputLayout.NumElements			= pCurInputLayout ? pCurInputLayout->NumElements : 0;
			psoDesc.InputLayout.pInputElementDescs	= pCurInputLayout ? pCurInputLayout->pInputElementDescs : nullptr;
			psoDesc.RasterizerState					= defaultRaster;
			psoDesc.BlendState						= defaultBlend;
			psoDesc.DepthStencilState				= defaultDepthStencil;
			psoDesc.SampleMask						= UINT_MAX;
			psoDesc.PrimitiveTopologyType			= primitiveType;
			psoDesc.NumRenderTargets				= 1;
			psoDesc.RTVFormats[0]					= psoPair.first;
			psoDesc.DSVFormat						= DXGI_FORMAT_D32_FLOAT;
			psoDesc.SampleDesc.Count				= 1;

			HRESULT res = curRenderPlat->AsD3D12Device()->CreateGraphicsPipelineState
			(
				&psoDesc,
				IID_PPV_ARGS(&psoPair.second)
			);
			SIMUL_ASSERT(res == S_OK);
			psoPair.second->SetName(L"GraphicsPSO");
			
			mGraphicsPsoMap.insert(psoPair);
		}
		// Init as compute
		else if (c)
		{
			mIsCompute = true;

			D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc	= {};
			psoDesc.CS									= CD3DX12_SHADER_BYTECODE(c->computeShader12);
			psoDesc.Flags								= D3D12_PIPELINE_STATE_FLAG_NONE;
			psoDesc.pRootSignature						= mRootS;
			psoDesc.NodeMask							= 0;
			HRESULT res = curRenderPlat->AsD3D12Device()->CreateComputePipelineState
			(
				&psoDesc,
				IID_PPV_ARGS(&mComputePso)
			);
			SIMUL_ASSERT(res == S_OK);
			mComputePso->SetName(L"ComputePSO");
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
		cmdList->SetComputeRootSignature(mRootS);
		cmdList->SetPipelineState(mComputePso);
	}
	else
	{
		cmdList->SetGraphicsRootSignature(mRootS);
		auto pFormat = curRenderPlat->GetCurrentPixelFormat();
		cmdList->SetPipelineState(mGraphicsPsoMap[pFormat]);
	}
	return;
}

EffectPass::~EffectPass()
{
	SAFE_RELEASE(mRootS);
	for (auto& ele : mGraphicsPsoMap)
	{
		SAFE_RELEASE(ele.second);
	}
	mGraphicsPsoMap.clear();
}
