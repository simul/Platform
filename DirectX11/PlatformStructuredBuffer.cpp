#include "PlatformStructuredBuffer.h"

#if PLATFORM_D3D11_SFX
#include "Platform/DirectX11/RenderPlatform.h"
#include "Platform/Core/StringToWString.h"
#include "Utilities.h"
#if !PLATFORM_D3D11_SFX
#include "D3dx11effect.h"
#endif

using namespace simul;
using namespace dx11;


static const int NUM_STAGING_BUFFERS = 4;
PlatformStructuredBuffer::PlatformStructuredBuffer()
	:num_elements(0)
	, element_bytesize(0)
	, buffer(0)
	, read_data(0)
	, shaderResourceView(0)
	, unorderedAccessView(0)
	, lastContext(NULL)
#if _XBOX_ONE
	, m_pPlacementBuffer(nullptr)
	, byteWidth(0)
#endif
	, m_nContexts(0)
	, m_nObjects(0)
	, m_nBuffering(0)
	, iObject(0)
{
	stagingBuffers = new ID3D11Buffer * [NUM_STAGING_BUFFERS];
	for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
		stagingBuffers[i] = NULL;
	memset(&mapped, 0, sizeof(mapped));
}

PlatformStructuredBuffer::~PlatformStructuredBuffer()
{
	InvalidateDeviceObjects();
	delete[] stagingBuffers;
}

void PlatformStructuredBuffer::RestoreDeviceObjects(crossplatform::RenderPlatform* r, int ct, int unit_size, bool computable, bool cpu_r, void* init_data, const char* n)
{
	InvalidateDeviceObjects();
	renderPlatform = r;
	cpu_read = cpu_r;
	num_elements = ct;
	element_bytesize = unit_size;
	D3D11_BUFFER_DESC sbDesc;
	memset(&sbDesc, 0, sizeof(sbDesc));
	if (computable)
	{
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		sbDesc.Usage = D3D11_USAGE_DEFAULT;
		sbDesc.CPUAccessFlags = 0;
	}
	else
	{
		sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_USAGE usage = D3D11_USAGE_DYNAMIC;
		if (((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
			usage = D3D11_USAGE_DEFAULT;
		sbDesc.Usage = usage;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbDesc.StructureByteStride = element_bytesize;
	sbDesc.ByteWidth = element_bytesize * num_elements;

	D3D11_SUBRESOURCE_DATA sbInit = { init_data, 0, 0 };

	m_nContexts = 1;
	m_nObjects = 1;
	m_nBuffering = 12;
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if (((dx11::RenderPlatform*)renderPlatform)->UsesFastSemantics())
	{
		m_index.resize(m_nContexts * m_nObjects, 0);
		UINT numBuffers = m_nContexts * m_nObjects * m_nBuffering;
		SIMUL_ASSERT(sbDesc.Usage == D3D11_USAGE_DEFAULT);
		byteWidth = sbDesc.ByteWidth;

		if (renderPlatform && renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pPlacementBuffer);
#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		// Force cache line alignment and packing to avoid cache flushing on Unmap in RoundRobinBuffers
		byteWidth = static_cast<UINT>(NextMultiple(byteWidth, SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE));
#endif

		m_pPlacementBuffer = static_cast<BYTE*>(VirtualAlloc(nullptr, numBuffers * byteWidth,
			MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT,
			PAGE_WRITECOMBINE | PAGE_READWRITE));

		if (renderPlatform && renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(m_pPlacementBuffer, numBuffers * byteWidth, "dx11::PlatformStructuredBuffer placement");

#ifdef SIMUL_D3D11_MAP_PLACEMENT_BUFFERS_CACHE_LINE_ALIGNMENT_PACK
		SIMUL_ASSERT((BytePtrToUint64(m_pPlacementBuffer) & (SIMUL_D3D11_BUFFER_CACHE_LINE_SIZE - 1)) == 0);
#endif
		if (init_data)
		{
			for (uint i = 0; i < numBuffers; i++)
			{
				memcpy(m_pPlacementBuffer + (i * byteWidth), init_data, byteWidth);
			}
		}
		V_CHECK(((ID3D11DeviceX*)renderPlatform->AsD3D11Device())->CreatePlacementBuffer(&sbDesc, m_pPlacementBuffer, &buffer));
	}
	else
#endif
	{
		if (renderPlatform && renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(buffer);
		V_CHECK(renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &buffer));
		if (renderPlatform && renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->TrackVideoMemory(buffer, sbDesc.ByteWidth, "dx11::PlatformStructuredBuffer main buffer");
	}
	if (cpu_read)
		for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
		{
			if (renderPlatform && renderPlatform->GetMemoryInterface())
				renderPlatform->GetMemoryInterface()->UntrackVideoMemory(stagingBuffers[i]);
			SAFE_RELEASE(stagingBuffers[i]);
		}
	// May not be needed, make sure not to set cpu_read if not:
	if (computable && cpu_read)
	{
		sbDesc.BindFlags = 0;
		sbDesc.Usage = D3D11_USAGE_STAGING;
		sbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		sbDesc.MiscFlags = 0;
		for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
		{
			renderPlatform->AsD3D11Device()->CreateBuffer(&sbDesc, init_data != NULL ? &sbInit : NULL, &stagingBuffers[i]);
			if (renderPlatform && renderPlatform->GetMemoryInterface())
				renderPlatform->GetMemoryInterface()->TrackVideoMemory(stagingBuffers[i], sbDesc.ByteWidth, "dx11::PlatformStructuredBuffer staging");
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	memset(&srv_desc, 0, sizeof(srv_desc));
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.ElementOffset = 0;
	srv_desc.Buffer.ElementWidth = 0;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = num_elements;
	V_CHECK(renderPlatform->AsD3D11Device()->CreateShaderResourceView(buffer, &srv_desc, &shaderResourceView));

	if (computable)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
		memset(&uav_desc, 0, sizeof(uav_desc));
		uav_desc.Format = DXGI_FORMAT_UNKNOWN;
		uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uav_desc.Buffer.FirstElement = 0;
		uav_desc.Buffer.Flags = 0;
		uav_desc.Buffer.NumElements = num_elements;
		V_CHECK(renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(buffer, &uav_desc, &unorderedAccessView));
	}
	numCopies = 0;
}

void* PlatformStructuredBuffer::GetBuffer(crossplatform::DeviceContext& deviceContext)
{
#if  SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if (((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
	{
		iObject++;
		iObject = iObject % m_nObjects;
		UINT iObjectIdx = iObject;
		UINT& iBufIdx = m_index[iObjectIdx];
		iBufIdx++;
		if (iBufIdx >= m_nBuffering)
		{
			iBufIdx = 0;
		}
		void* ppMappedData = (&m_pPlacementBuffer[(iObjectIdx * m_nBuffering + iBufIdx) * byteWidth]);
		return ppMappedData;
	}
	else
#endif
	{
		lastContext = deviceContext.asD3D11DeviceContext();
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
		if (((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			map_type = D3D11_MAP_WRITE;
		if (!mapped.pData)
			lastContext->Map(buffer, 0, map_type, SIMUL_D3D11_MAP_FLAGS, &mapped);
		void* ptr = (void*)mapped.pData;
		return ptr;
	}
}

const void* PlatformStructuredBuffer::OpenReadBuffer(crossplatform::DeviceContext&)
{
	// Only immediate context works for mapping to read!
	crossplatform::DeviceContext& deviceContext = renderPlatform->GetImmediateContext();
	lastContext = deviceContext.asD3D11DeviceContext();
	mapped.pData = NULL;
	if (numCopies >= NUM_STAGING_BUFFERS)
	{
		HRESULT hr = DXGI_ERROR_WAS_STILL_DRAWING;
		int wait = 0;
		while (hr == DXGI_ERROR_WAS_STILL_DRAWING)
		{
			hr = lastContext->Map(stagingBuffers[NUM_STAGING_BUFFERS - 1], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);//|SIMUL_D3D11_MAP_FLAGS
			wait++;
		}
		if (wait > 1)
			SIMUL_CERR << "PlatformStructuredBuffer::OpenReadBuffer waited " << wait << " times." << std::endl;
		if (hr != S_OK)
		{
			D3D11_BUFFER_DESC desc;
			stagingBuffers[NUM_STAGING_BUFFERS - 1]->GetDesc(&desc);
			mapped.pData = NULL;
			if (hr != DXGI_ERROR_WAS_STILL_DRAWING)
			{
				SIMUL_CERR << "Failed to map PlatformStructuredBuffer for reading. " << desc.CPUAccessFlags << std::endl;
				SIMUL_BREAK_ONCE("Failed here");
			}
			return mapped.pData;
		}
		if (wait > 1)
		{
			SIMUL_CERR_ONCE << "PlatformStructuredBuffer::OpenReadBuffer waited " << wait << " times." << std::endl;
		}
	}
	return mapped.pData;
}

void PlatformStructuredBuffer::CloseReadBuffer(crossplatform::DeviceContext&)
{
	if (mapped.pData)
		lastContext->Unmap(stagingBuffers[NUM_STAGING_BUFFERS - 1], 0);
	mapped.pData = NULL;
	lastContext = NULL;
}

void PlatformStructuredBuffer::CopyToReadBuffer(crossplatform::DeviceContext& deviceContext)
{
	lastContext = deviceContext.asD3D11DeviceContext();
	for (int i = 0; i < NUM_STAGING_BUFFERS - 1; i++)
		std::swap(stagingBuffers[(NUM_STAGING_BUFFERS - 1 - i)], stagingBuffers[(NUM_STAGING_BUFFERS - 2 - i)]);
	if (numCopies < NUM_STAGING_BUFFERS)
	{
		numCopies++;
	}
	else
	{
		lastContext->CopyResource(stagingBuffers[0], buffer);
	}
}


void PlatformStructuredBuffer::SetData(crossplatform::DeviceContext& deviceContext, void* data)
{
	lastContext = deviceContext.asD3D11DeviceContext();
	if (lastContext)
	{
		lastContext = deviceContext.asD3D11DeviceContext();
		D3D11_MAP map_type = D3D11_MAP_WRITE_DISCARD;
		if (((dx11::RenderPlatform*)deviceContext.renderPlatform)->UsesFastSemantics())
			map_type = D3D11_MAP_WRITE;
		HRESULT hr = lastContext->Map(buffer, 0, map_type, SIMUL_D3D11_MAP_FLAGS, &mapped);
		if (hr == S_OK)
		{
			memcpy(mapped.pData, data, num_elements * element_bytesize);
			mapped.RowPitch = 0;
			mapped.DepthPitch = 0;
			lastContext->Unmap(buffer, 0);
		}
		else
			SIMUL_BREAK_ONCE("Map failed");
	}
	else
		SIMUL_BREAK_ONCE("Uninitialized device context");
	mapped.pData = NULL;
}
#if PLATFORM_D3D11_SFX
void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext &deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource& shaderResource)
{
	if (lastContext && mapped.pData)
	{
		lastContext->Unmap(buffer, 0);
		mapped.pData=nullptr;
	}
	crossplatform::PlatformStructuredBuffer::Apply(deviceContext, effect, shaderResource);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::Effect* effect, const crossplatform::ShaderResource& shaderResource)
{
	if (lastContext && mapped.pData)
	{
		lastContext->Unmap(buffer, 0);
		mapped.pData = nullptr;
	}
	crossplatform::PlatformStructuredBuffer::ApplyAsUnorderedAccessView(deviceContext, effect, shaderResource);
}
#else
void PlatformStructuredBuffer::Apply(crossplatform::DeviceContext&, crossplatform::Effect* effect, const crossplatform::ShaderResource& shaderResource)
{
	if (lastContext && mapped.pData)
		lastContext->Unmap(buffer, 0);
	mapped.pData = NULL;
	if (!effect)
		return;
	if (!effect->asD3DX11Effect())
		return;
	ID3DX11EffectShaderResourceVariable* var = static_cast<ID3DX11EffectShaderResourceVariable*>(shaderResource.platform_shader_resource);

	if (!var->IsValid())
	{
		return;
	}
	var->SetResource(shaderResourceView);
}

void PlatformStructuredBuffer::ApplyAsUnorderedAccessView(crossplatform::DeviceContext&, crossplatform::Effect* effect, const crossplatform::ShaderResource& shaderResource)
{
	if (lastContext && mapped.pData)
		lastContext->Unmap(buffer, 0);
	mapped.pData = NULL;
	if (!effect->asD3DX11Effect())
		return;
	if(!shaderResource.platform_shader_resource)
		return;
	ID3DX11EffectUnorderedAccessViewVariable* var = static_cast<ID3DX11EffectUnorderedAccessViewVariable*>(shaderResource.platform_shader_resource);
	//ID3DX11EffectUnorderedAccessViewVariable *var	=effect->asD3DX11Effect()->GetVariableByName(name)->AsUnorderedAccessView();
	if (!var->IsValid())
	{
		return;
	}
	var->SetUnorderedAccessView(unorderedAccessView);
}

#endif
void PlatformStructuredBuffer::Unbind(crossplatform::DeviceContext&)
{
}
void PlatformStructuredBuffer::InvalidateDeviceObjects()
{
	if (lastContext && mapped.pData && buffer)
		lastContext->Unmap(buffer, 0);
	mapped.pData = NULL;
	SAFE_RELEASE(unorderedAccessView);
	SAFE_RELEASE(shaderResourceView);
	for (int i = 0; i < NUM_STAGING_BUFFERS; i++)
	{
		if (renderPlatform && renderPlatform->GetMemoryInterface())
			renderPlatform->GetMemoryInterface()->UntrackVideoMemory(stagingBuffers[i]);
		SAFE_RELEASE(stagingBuffers[i]);
	}
	num_elements = 0;
#if SIMUL_D3D11_MAP_USAGE_DEFAULT_PLACEMENT
	if (renderPlatform && renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(m_pPlacementBuffer);
	VirtualFree(m_pPlacementBuffer, 0, MEM_RELEASE);
	m_pPlacementBuffer = nullptr;
	buffer = nullptr;
#else
	if (renderPlatform && renderPlatform->GetMemoryInterface())
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(buffer);
	SAFE_RELEASE(buffer);
#endif
}
#endif