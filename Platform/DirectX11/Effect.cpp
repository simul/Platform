#define NOMINMAX
#include "Effect.h"
#include "CreateEffectDX1x.h"
#include "Utilities.h"
#include "Texture.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "D3dx11effect.h"

#include <string>

using namespace simul;
using namespace dx11;
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
	for(int i=0;i<QueryLatency;i++)
		SetDebugObjectName( d3d11Query[i], name );
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
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
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->Begin(d3d11Query[currFrame]);
}

void Query::End(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	pContext->End(d3d11Query[currFrame]);
	gotResults[currFrame]=false;
	doneQuery[currFrame]=true;
}

bool Query::GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz)
{
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
}

RenderState::RenderState()
	:m_depthStencilState(NULL)
	,m_blendState(NULL)
{
}
RenderState::~RenderState()
{
	SAFE_RELEASE(m_depthStencilState)
	SAFE_RELEASE(m_blendState)
}
static const int NUM_STAGING_BUFFERS=4;
PlatformStructuredBuffer::PlatformStructuredBuffer()
				:num_elements(0)
				,element_bytesize(0)
				,buffer(0)
				,read_data(0)
				,shaderResourceView(0)
				,unorderedAccessView(0)
				,lastContext(NULL)
#if _XBOX_ONE
	,m_pPlacementBuffer( nullptr )
	,byteWidth( 0 )
#endif
	,m_nContexts( 0 )
	,m_nObjects( 0 )
	,m_nBuffering( 0 )
	,iObject(0)
			{
				stagingBuffers=new ID3D11Buffer*[NUM_STAGING_BUFFERS];
				for(int i=0;i<NUM_STAGING_BUFFERS;i++)
					stagingBuffers[i]=NULL;
				memset(&mapped,0,sizeof(mapped));
			}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
	delete[] stagingBuffers;
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform *r,int ct,int unit_size,bool computable,bool cpu_r,void *init_data)
{
	InvalidateDeviceObjects();
	renderPlatform=r;
	cpu_read=cpu_r;
	num_elements=ct;
	element_bytesize=unit_size;
	D3D11_BUFFER_DESC sbDesc;
	memset(&sbDesc,0,sizeof(sbDesc));
	if(computable)
	{
		sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.Usage				=D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags		=0;
	}
	else
	{
		sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE ;
		D3D11_USAGE usage			=D3D11_USAGE_DYNAMIC;
		if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
			usage=D3D11_USAGE_DEFAULT;
		sbDesc.Usage				=usage;
		sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
	}
	sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
	sbDesc.StructureByteStride	=element_bytesize;
	sbDesc.ByteWidth			=element_bytesize*num_elements;
	
	D3D11_SUBRESOURCE_DATA sbInit = {init_data, 0, 0};

	m_nContexts = 1;
	m_nObjects = 1;
	m_nBuffering = 12;
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
	{
		m_index.resize( m_nContexts * m_nObjects, 0 );
		UINT numBuffers = m_nContexts * m_nObjects * m_nBuffering;
		SIMUL_ASSERT( sbDesc.Usage == D3D11_USAGE_DEFAULT );
		byteWidth = sbDesc.ByteWidth;

		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pPlacementBuffer);
		#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		// Force cache line alignment and packing to avoid cache flushing on Unmap in RoundRobinBuffers
		byteWidth  = static_cast<UINT>( NextMultiple( byteWidth, SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE ) );
		#endif
	
		m_pPlacementBuffer = static_cast<BYTE*>( VirtualAlloc( nullptr, numBuffers * byteWidth, 
			MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, 
			PAGE_WRITECOMBINE | PAGE_READWRITE ));

		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(m_pPlacementBuffer,numBuffers * byteWidth,"dx11::PlatformStructuredBuffer");

		#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		SIMUL_ASSERT( ( BytePtrToUint64( m_pPlacementBuffer ) & ( SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE - 1 ) ) == 0 );
		#endif
		if (init_data)
		{
			for (uint i = 0; i < numBuffers; i++)
			{
				memcpy(m_pPlacementBuffer+(i*byteWidth), init_data, byteWidth);
			}
		}
		V_CHECK( ((ID3D11DeviceX*)renderPlatform->AsD3D11Device())->CreatePlacementBuffer( &sbDesc, m_pPlacementBuffer, &buffer ) );
	}
	else
#endif
	{
		V_CHECK(renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &buffer));
		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(buffer,sbDesc.ByteWidth,"dx11::PlatformStructuredBuffer");
	}
	if(cpu_read)
	for(int i=0;i<NUM_STAGING_BUFFERS;i++)
	{
		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(stagingBuffers[i]);
		SAFE_RELEASE(stagingBuffers[i]);
	}
	// May not be needed, make sure not to set cpu_read if not:
	if(computable&&cpu_read)
	{
		sbDesc.BindFlags=0;
		sbDesc.Usage				=D3D11_USAGE_STAGING;
		sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_READ;
		sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
		for(int i=0;i<NUM_STAGING_BUFFERS;i++)
		{
			renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &stagingBuffers[i]);
			if(renderPlatform&&renderPlatform->GetMemoryInterface())
				renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffers[i],sbDesc.ByteWidth,"dx11::PlatformStructuredBuffer staging");
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	memset(&srv_desc,0,sizeof(srv_desc));
	srv_desc.Format						=DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension				=D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.ElementOffset		=0;
	srv_desc.Buffer.ElementWidth		=0;
	srv_desc.Buffer.FirstElement		=0;
	srv_desc.Buffer.NumElements			=num_elements;
	V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(buffer, &srv_desc,&shaderResourceView));

	if(computable)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		memset(&uav_desc,0,sizeof(uav_desc));
		uav_desc.Format						=DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement		=0;
		uav_desc.Buffer.Flags				=0;
		uav_desc.Buffer.NumElements			=num_elements;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(buffer, &uav_desc,&unorderedAccessView));
	}
	numCopies=0;
}

void *PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext &deviceContext)
{
#if  SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
	{
		iObject++;
		iObject=iObject%m_nObjects;
		UINT iObjectIdx = iObject;
		UINT &iBufIdx = m_index[ iObjectIdx ];
		iBufIdx++;
		if ( iBufIdx >= m_nBuffering )
		{
			iBufIdx = 0;
		}
		void *ppMappedData =( &m_pPlacementBuffer[ ( iObjectIdx * m_nBuffering + iBufIdx ) * byteWidth ] );
		return ppMappedData;
	}
	else
#endif
	{
	lastContext=deviceContext.asD3D11DeviceContext();
		D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
		if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			map_type=D3D11_MAP_WRITE;
	if(!mapped.pData)
			lastContext->Map(buffer,0,map_type,SIMUL_D3D11_MAP_FLAGS,&mapped);
	void *ptr=(void *)mapped.pData;
	return ptr;
}
}

const void *PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	mapped.pData=NULL;
	if(numCopies>=NUM_STAGING_BUFFERS)
	{
		HRESULT hr=DXGI_ERROR_WAS_STILL_DRAWING;
		int wait=0;
		while(hr==DXGI_ERROR_WAS_STILL_DRAWING)
		{
			hr=lastContext->Map(stagingBuffers[NUM_STAGING_BUFFERS-1],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT|SIMUL_D3D11_MAP_FLAGS,&mapped);
			wait++;
		}
		if(wait>1)
			SIMUL_CERR<<"PlatformStructuredBuffer::OpenReadBuffer waited "<<wait<<" times."<<std::endl;
		if(hr!=S_OK)
			mapped.pData=NULL;
		if(wait>1)
		{
			SIMUL_CERR_ONCE << "PlatformStructuredBuffer::OpenReadBuffer waited " << wait << " times." << std::endl;
	}
	}
	return mapped.pData;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	if(mapped.pData)
		lastContext->Unmap(stagingBuffers[NUM_STAGING_BUFFERS-1], 0 );
	mapped.pData=NULL;
	lastContext=NULL;
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext &deviceContext)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	for(int i=0;i<NUM_STAGING_BUFFERS-1;i++)
		std::swap(stagingBuffers[(NUM_STAGING_BUFFERS-1-i)],stagingBuffers[(NUM_STAGING_BUFFERS-2-i)]);
	if(numCopies<NUM_STAGING_BUFFERS)
	{
		numCopies++;
	}
	else
	{
		lastContext->CopyResource(stagingBuffers[0],buffer);
	}
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext &deviceContext,void *data)
{
	lastContext=deviceContext.asD3D11DeviceContext();
	if(lastContext)
	{
		lastContext=deviceContext.asD3D11DeviceContext();
		D3D11_MAP map_type=D3D11_MAP_WRITE_DISCARD;
		if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			map_type=D3D11_MAP_WRITE;
		HRESULT hr=lastContext->Map(buffer,0,map_type,SIMUL_D3D11_MAP_FLAGS,&mapped);
		if(hr==S_OK)
		{
			memcpy(mapped.pData,data,num_elements*element_bytesize);
			mapped.RowPitch=0;
			mapped.DepthPitch=0;
			lastContext->Unmap(buffer,0);
		}
		else
			SIMUL_BREAK_ONCE("Map failed");
	}
	else
		SIMUL_BREAK_ONCE("Uninitialized device context");
	mapped.pData=NULL;
}

void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	if(!effect)
		return;
	if(!effect->asD3DX11Effect())
		return;
	if(!effect->asD3DX11Effect()->GetVariableByName(name))
		return;
	ID3DX11EffectShaderResourceVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsShaderResource();
		
	if(!var->IsValid())
	{
		SIMUL_CERR<<"Constant Buffer not found: "<<name<<", in effect "<<effect->filename.c_str()<<std::endl;
		if(effect->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(effect->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	var->SetResource(shaderResourceView);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	if (!effect->asD3DX11Effect())
		return;
	if (!effect->asD3DX11Effect()->GetVariableByName(name))
		return;
	ID3DX11EffectUnorderedAccessViewVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsUnorderedAccessView();
	if(!var->IsValid())
	{
		SIMUL_CERR<<"Constant Buffer not found: "<<name<<", in effect "<<effect->filename.c_str()<<std::endl;
		if(effect->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(effect->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
	}
	var->SetUnorderedAccessView(unorderedAccessView);
}

void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext &deviceContext)
{
}
void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if(lastContext&&mapped.pData)
		lastContext->Unmap(buffer,0);
	mapped.pData=NULL;
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(shaderResourceView);
	SAFE_RELEASE(buffer);
	for(int i=0;i<NUM_STAGING_BUFFERS;i++)
	{
		if(renderPlatform&&renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(stagingBuffers[i]);
		SAFE_RELEASE(stagingBuffers[i]);
	}
	num_elements=0;
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if(renderPlatform&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pPlacementBuffer);
	VirtualFree( m_pPlacementBuffer, 0, MEM_RELEASE );
	m_pPlacementBuffer=nullptr;
	buffer=nullptr;
#else
	SAFE_RELEASE(buffer);
	if(renderPlatform&&renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(buffer);
#endif
}

int EffectTechnique::NumPasses() const
{
	D3DX11_TECHNIQUE_DESC desc;
	ID3DX11EffectTechnique *tech=const_cast<EffectTechnique*>(this)->asD3DX11EffectTechnique();
	tech->GetDesc(&desc);
	return (int)desc.Passes;
}

dx11::Effect::Effect() :currentPass(NULL)
{
}

EffectTechnique *Effect::CreateTechnique()
{
	return new dx11::EffectTechnique;
}
#define D3DCOMPILE_DEBUG 1
void Effect::Load(crossplatform::RenderPlatform *r,const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	renderPlatform=r;
	if(!renderPlatform)
		return;
	// PREFER to use the platform shader:
	std::string filename_fx(filename_utf8);
	if(filename_fx.find(".")>=filename_fx.length())
		filename_fx+=".fx";
	int index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),renderPlatform->GetShaderPathsUtf8());

	if(index<-1||index>=renderPlatform->GetShaderPathsUtf8().size())
	{
		filename_fx=(filename_utf8);
		if(filename_fx.find(".")>=filename_fx.length())
			filename_fx+=".sfx";
		index=simul::base::FileLoader::GetFileLoader()->FindIndexInPathStack(filename_fx.c_str(),renderPlatform->GetShaderPathsUtf8());
	}
	if(index<0||index>=renderPlatform->GetShaderPathsUtf8().size())
		filenameInUseUtf8=filename_fx;
	else if(index<renderPlatform->GetShaderPathsUtf8().size())
		filenameInUseUtf8=(renderPlatform->GetShaderPathsUtf8()[index]+"/")+filename_fx;
	unsigned flags=D3DCOMPILE_OPTIMIZATION_LEVEL3;
	// D3D shader compiler is broken for some shaders...
	if(filename_fx.find("atmospherics")==0||filename_fx.find("spherical_harmonics")==0)
		flags=D3DCOMPILE_SKIP_OPTIMIZATION;

	HRESULT hr = CreateEffect(renderPlatform->AsD3D11Device(), &e, filename_fx.c_str(), defines,flags, renderPlatform->GetShaderBuildMode()
		,renderPlatform->GetShaderPathsUtf8(),renderPlatform->GetShaderBinaryPath());//);D3DCOMPILE_DEBUG
	platform_effect	=e;
	groups.clear();
	if(e)
	{
		D3DX11_EFFECT_DESC desc;
		e->GetDesc(&desc);
		for(int i=0;i<(int)desc.Groups;i++)
		{
			ID3DX11EffectGroup *g=e->GetGroupByIndex(i);
			D3DX11_GROUP_DESC gdesc;
			g->GetDesc(&gdesc);
			crossplatform::EffectTechniqueGroup *G=new crossplatform::EffectTechniqueGroup;
			if(gdesc.Name)
				groups[gdesc.Name]=G;
			else
				groups[""]=G;// The ungrouped techniques!
			for(int j=0;j<(int)gdesc.Techniques;j++)
			{
				ID3DX11EffectTechnique *t	=g->GetTechniqueByIndex(j);
				D3DX11_TECHNIQUE_DESC tdesc;
				t->GetDesc(&tdesc);
				dx11::EffectTechnique *T	=new dx11::EffectTechnique;
				T->name						=tdesc.Name;
				G->techniques[tdesc.Name]	=T;
				T->platform_technique		=t;
				G->techniques_by_index[j]	=T;
				for(int k=0;k<(int)tdesc.Passes;k++)
				{
					ID3DX11EffectPass *p=t->GetPassByIndex(k);
					D3DX11_PASS_DESC passDesc;
					p->GetDesc(&passDesc);
					D3DX11_PASS_SHADER_DESC shaderDesc;
					p->GetComputeShaderDesc(&shaderDesc);
					T->AddPass(passDesc.Name,k);
				}
			}
		}
	}
}

dx11::Effect::~Effect()
{
	InvalidateDeviceObjects();
}

void Effect::InvalidateDeviceObjects()
{
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	SAFE_RELEASE(e);
	platform_effect=e;
}

crossplatform::EffectTechnique *dx11::Effect::GetTechniqueByName(const char *name)
{
	auto c=techniqueCharMap.find(name);
	if(c!=techniqueCharMap.end())
	{
		return c->second;
	}
	auto g=groups[""];
	if (!g)
		return nullptr;
	auto v=g->techniques.find(name);
	if(v!=g->techniques.end())
	{
		crossplatform::EffectTechnique *tech=v->second;
		techniqueCharMap[name]=tech;
		return tech;
	}
	if(!platform_effect)
		return NULL;
	ID3DX11Effect *e=(ID3DX11Effect *)platform_effect;
	ID3DX11EffectTechnique *t=e->GetTechniqueByName(name);
	if(!t->IsValid())
	{
		SIMUL_CERR<<"Invalid Effect technique "<<name<<" in effect "<<this->filename.c_str()<<std::endl;
		if(this->filenameInUseUtf8.length())
			SIMUL_FILE_LINE_CERR(this->filenameInUseUtf8.c_str(),0)<<"See effect file."<<std::endl;
		return nullptr;
	}
	crossplatform::EffectTechnique *tech=new dx11::EffectTechnique;
	tech->platform_technique=t;
	techniques[name]=tech;
	techniqueCharMap[name]=tech;
	groups[""]->techniques[name]=tech;
	if(!tech->platform_technique)
	{
		SIMUL_BREAK_ONCE("NULL technique");
	}
	return tech;
}

crossplatform::EffectTechnique *dx11::Effect::GetTechniqueByIndex(int index)
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
	tech=new dx11::EffectTechnique;
	tech->platform_technique=t;
	techniques[desc.Name]=tech;
	techniques_by_index[index]=tech;
	groups[""]->techniques[desc.Name]=tech;
	groups[""]->techniques_by_index[index]=tech;
	return tech;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *t,int index,int mip)
{
	crossplatform::ShaderResource shaderResource			=GetShaderResource(name);
	SetUnorderedAccessView(deviceContext,shaderResource,t,index,mip);
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *t,int index,int mip)
{
	ID3DX11EffectUnorderedAccessViewVariable *var=(ID3DX11EffectUnorderedAccessViewVariable*)(shaderResource.platform_shader_resource);
	if(!asD3DX11Effect())
	{
		SIMUL_CERR_ONCE<<"Invalid effect "<<std::endl;
		return;
	}
	if(!var)
	{
		SIMUL_CERR_ONCE<<"Invalid Resource "<<std::endl;
		return;
	}
	if(t)
	{
		ID3D11UnorderedAccessView *uav=t->AsD3D11UnorderedAccessView(index,mip);
		if(!uav)
		{
			SIMUL_CERR_ONCE<<"Unordered access view not found."<<std::endl;
		}
		var->SetUnorderedAccessView(uav);
	}
	else
		var->SetUnorderedAccessView(NULL);
	crossplatform::Effect::SetUnorderedAccessView(deviceContext,shaderResource,t,index,mip);
}


void dx11::Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *t,int index,int mip)
{
	if(!asD3DX11Effect())
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return;
	}
	ID3DX11Effect *e=asD3DX11Effect();
	if(!e)
		return;
	crossplatform::ShaderResource res			=GetShaderResource(name);
	SetTexture(deviceContext, res, t, index,mip);
}


void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,crossplatform::ShaderResource &shaderResource,crossplatform::Texture *t,int index,int mip)
{
	// If invalid, we already had the error when we assigned this ShaderResource. So fail silently to avoid spamming output.
	if(!shaderResource.valid)
		return;
	// TODO: disallow SetTexture when the texture doesn't match the ShaderResource's type.
	ID3DX11EffectShaderResourceVariable *var=(ID3DX11EffectShaderResourceVariable*)(shaderResource.platform_shader_resource);
	if(!var||!var->IsValid())
	{
		SIMUL_BREAK_ONCE("Invalid shader texture ");
		return;
	}
	if(t)
	{
		dx11::Texture *T=(dx11::Texture*)t;
		auto srv=T->AsD3D11ShaderResourceView(shaderResource.shaderResourceType,index,mip);
		var->SetResource(srv);
	}
	else
	{
		var->SetResource(NULL);
	}
	crossplatform::Effect::SetTexture(deviceContext,shaderResource,t,index,mip);
}

void Effect::SetConstantBuffer(crossplatform::DeviceContext &deviceContext,const char *name	,crossplatform::ConstantBufferBase *s)	
{
	if(!asD3DX11Effect())
		return;
	ID3DX11EffectConstantBuffer *pD3DX11EffectConstantBuffer=asD3DX11Effect()->GetConstantBufferByName(name);
	if(pD3DX11EffectConstantBuffer)
	{
		crossplatform::PlatformConstantBuffer *pcb=s->GetPlatformConstantBuffer();
		dx11::PlatformConstantBuffer *pcb11=(dx11::PlatformConstantBuffer *)pcb;
		pcb11->Apply(deviceContext,s->GetSize(),s->GetAddr());
#ifdef D3D11_FAST_SEMANTICS
		if(((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			pD3DX11EffectConstantBuffer->SetConstantBuffer(pcb11->asD3D11Buffer(),pcb11->GetBaseAddr());
		else
#endif
		pD3DX11EffectConstantBuffer->SetConstantBuffer(pcb11->asD3D11Buffer());
		if (currentTechnique)
		{
			ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
			if(currentPass)
				V_CHECK(currentPass->Apply(0, deviceContext.asD3D11DeviceContext()));
		}
	}
}


crossplatform::ShaderResource Effect::GetShaderResource(const char *name)
{
	// First do a simple search by pointer.
	auto i=shaderResources.find(name);
	if(i!=shaderResources.end())
		return i->second;
	crossplatform::ShaderResource &res=shaderResources[name];
	res.platform_shader_resource=0;
	res.valid=false;
	ID3DX11Effect *effect=asD3DX11Effect();
	if(!effect)
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return res;
	}
	ID3DX11EffectVariable *var=effect->GetVariableByName(name);
	if(!var->IsValid())
	{
		SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Invalid shader variable ")+name).c_str());
		return res;
	}
	D3DX11_EFFECT_TYPE_DESC  desc;
	var->GetType()->GetDesc(&desc);
	desc.Class;
	res.shaderResourceType=((simul::dx11::RenderPlatform*)renderPlatform)->FromD3DShaderVariableType(desc.Type);
	res.slot=(int)shaderResources.size();
	ID3DX11EffectShaderResourceVariable*	srv	=var->AsShaderResource();
	if(srv->IsValid())
	{
		res.platform_shader_resource=(void*)srv;
	}
	else
	{
		ID3DX11EffectUnorderedAccessViewVariable *uav=var->AsUnorderedAccessView();
		if(uav->IsValid())
		{
			res.slot+=1000;
			res.platform_shader_resource=(void*)uav;
		}
		else
		{
			SIMUL_ASSERT_WARN(var->IsValid()!=0,(std::string("Unknown resource type ")+name).c_str());
			return res;
		}
	}
	res.valid=true;
	return res;
}

void Effect::SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s)
{
	if(!asD3DX11Effect())
	{
		SIMUL_CERR<<"Invalid effect "<<std::endl;
		return;
	}
	if (!s)
		return;
	ID3DX11EffectSamplerVariable*	var	=asD3DX11Effect()->GetVariableByName(name)->AsSampler();
	var->SetSampler(0,s->asD3D11SamplerState());
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	if(!effectTechnique)
		return;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
	currentPass						=tech->GetPassByIndex(pass_num);
	HRESULT hr						=currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	crossplatform::Effect::Apply(deviceContext,effectTechnique,pass_num);
	V_CHECK_ONCE(hr);
}

void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	cs->invalidate();
	cs->currentTechnique=effectTechnique;
	cs->currentEffect=this;
	ID3DX11Effect *effect			=asD3DX11Effect();
	currentTechnique				=effectTechnique;
	if(effectTechnique)
	{
		ID3DX11EffectTechnique *tech	=effectTechnique->asD3DX11EffectTechnique();
		if (!tech->IsValid())
		{
			const char *techname=effectTechnique->name.c_str();
			SIMUL_BREAK_ONCE(base::QuickFormat("Invalid technique %s of shader %s\n",techname,this->filename.c_str()));
			return;
		}
		if(!passname)
			currentPass = tech->GetPassByIndex(0);
		else
			currentPass = tech->GetPassByName(passname);
		if (!currentPass->IsValid())
		{
			const char *techname=effectTechnique->name.c_str();
			SIMUL_BREAK_ONCE(base::QuickFormat("Invalid pass %s sent to Effect::Apply for technique %s of shader %s\n",passname,techname,this->filename.c_str()));
			D3DX11_TECHNIQUE_DESC desc;
			ID3DX11EffectTechnique *t=const_cast<ID3DX11EffectTechnique*>(tech);
			t->GetDesc(&desc);
			std::cerr<<"Passes are: ";
			for(int i=0;i<(int)desc.Passes;i++)
			{
				ID3DX11EffectPass *p=tech->GetPassByIndex(i);
				D3DX11_PASS_DESC pdesc;
				p->GetDesc(&pdesc);
				std::cerr<<pdesc.Name<<" ";
			}
			std::cerr<<std::endl;
		}
		else
		{
			HRESULT hr = currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
			V_CHECK(hr);
		}
	}
	else
	{
		SIMUL_BREAK_ONCE(base::QuickFormat("NULL technique sent to Effect::Apply for shader %s\n",this->filename.c_str()));
	}
	crossplatform::Effect::Apply(deviceContext,effectTechnique,passname);
}

void Effect::Reapply(crossplatform::DeviceContext &deviceContext)
{
	if(apply_count!=1)
		SIMUL_BREAK_ONCE(base::QuickFormat("Effect::Reapply can only be called after Apply and before Unapply. Effect: %s\n",this->filename.c_str()));
	ID3DX11Effect *effect			=asD3DX11Effect();
	if(!effect)
		return;
	if (!currentTechnique)
		return;
	ID3DX11EffectTechnique *tech	=currentTechnique->asD3DX11EffectTechnique();
	if(currentPass)
		V_CHECK(currentPass->Apply(0, deviceContext.asD3D11DeviceContext()));
}

void Effect::Unapply(crossplatform::DeviceContext &deviceContext)
{
	if(currentPass)
		currentPass->Apply(0, deviceContext.asD3D11DeviceContext());
	deviceContext.asD3D11DeviceContext()->CSSetShader(nullptr,nullptr,0);
	deviceContext.asD3D11DeviceContext()->GSSetShader(nullptr,nullptr,0);
	deviceContext.asD3D11DeviceContext()->PSSetShader(nullptr,nullptr,0);
	deviceContext.asD3D11DeviceContext()->VSSetShader(nullptr,nullptr,0);
	currentTechnique=NULL;
	currentPass = NULL;
	//UnbindTextures(deviceContext);
	crossplatform::Effect::Unapply(deviceContext);
}
void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	auto c=deviceContext.asD3D11DeviceContext();
	static ID3D11ShaderResourceView *src[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	c->VSSetShaderResources(0,32,src);
	c->HSSetShaderResources(0,32,src);
	c->DSSetShaderResources(0,32,src);
	c->GSSetShaderResources(0,32,src);
	c->PSSetShaderResources(0,32,src);
	c->CSSetShaderResources(0,32,src);
	static ID3D11UnorderedAccessView *uav[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	c->CSSetUnorderedAccessViews(0,8,uav,0);
	crossplatform::Effect::UnbindTextures(deviceContext);
}
crossplatform::EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	crossplatform::EffectPass *p=new dx11::EffectPass;
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}
void EffectPass::Apply(crossplatform::DeviceContext &deviceContext,bool test)
{

}