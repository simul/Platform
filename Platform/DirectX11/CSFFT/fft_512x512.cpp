// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "fft_512x512.h"
using namespace simul;
////////////////////////////////////////////////////////////////////////////////
// Common constants
////////////////////////////////////////////////////////////////////////////////
#define TWO_PI 6.283185307179586476925286766559

#define FFT_DIMENSIONS 3U
#define FFT_PLAN_SIZE_LIMIT (1U << 27)

#define FFT_FORWARD -1
#define FFT_INVERSE 1

HRESULT CompileShaderFromFile( const char* szFileName, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut );

Fft::Fft()
	:renderPlatform(NULL)
	,pRadix008A_CS(NULL)
	,pRadix008A_CS2(NULL)
	,pBuffer_Tmp(NULL)
	,pUAV_Tmp(NULL)
	,pSRV_Tmp(NULL)
	,ppRadix008A_CB(NULL)
	,numBuffers(0)
{
}
Fft::~Fft()
{
	InvalidateDeviceObjects();
}

void Fft::RestoreDeviceObjects(crossplatform::RenderPlatform* r, UINT s,int size)
{
	InvalidateDeviceObjects();
	renderPlatform=r;
	slices = s;

	// Context
	RecompileShaders();

	// Constants
	// Create 6 cbuffers for 512x512 transform
	CreateCBuffers(slices,size);

	// Temp buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = sizeof(float) * 2 * (size* slices) * size;
    buf_desc.Usage = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buf_desc.StructureByteStride = sizeof(float) * 2;

	renderPlatform->AsD3D11Device()->CreateBuffer(&buf_desc, NULL, &pBuffer_Tmp);
	assert(pBuffer_Tmp);

	// Temp undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = (size * slices) * size;
	uav_desc.Buffer.Flags = 0;

	renderPlatform->AsD3D11Device()->CreateUnorderedAccessView(pBuffer_Tmp, &uav_desc, &pUAV_Tmp);

	// Temp shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = (size * slices) * size;

	renderPlatform->AsD3D11Device()->CreateShaderResourceView(pBuffer_Tmp, &srv_desc, &pSRV_Tmp);
}

void Fft::RecompileShaders()
{
	if(!renderPlatform)
		return;
	// Compute shaders
    ID3DBlob* pBlobCS = NULL;
    ID3DBlob* pBlobCS2 = NULL;

    CompileShaderFromFile("fft_512x512_c2c.hlsl","Radix008A_CS", "cs_4_0", &pBlobCS);
    CompileShaderFromFile("fft_512x512_c2c.hlsl","Radix008A_CS2", "cs_4_0", &pBlobCS2);
	
	SAFE_RELEASE(pRadix008A_CS);
	SAFE_RELEASE(pRadix008A_CS2);
    renderPlatform->AsD3D11Device()->CreateComputeShader(pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), NULL, &pRadix008A_CS);
    renderPlatform->AsD3D11Device()->CreateComputeShader(pBlobCS2->GetBufferPointer(), pBlobCS2->GetBufferSize(), NULL, &pRadix008A_CS2);
    
    SAFE_RELEASE(pBlobCS);
    SAFE_RELEASE(pBlobCS2);
}

void Fft::InvalidateDeviceObjects()
{
	SAFE_RELEASE(pSRV_Tmp);
	SAFE_RELEASE(pUAV_Tmp);
	SAFE_RELEASE(pBuffer_Tmp);
	SAFE_RELEASE(pRadix008A_CS);
	SAFE_RELEASE(pRadix008A_CS2);
	
	if(ppRadix008A_CB)
	{
		for(int i=0;i<numBuffers;i++)
			SAFE_RELEASE(ppRadix008A_CB[i]);
	}
	ppRadix008A_CB=NULL;
	numBuffers=0;
	renderPlatform=NULL;
}

void Fft::radix008A(	ID3D11UnorderedAccessView* pUAV_Dst,
						ID3D11ShaderResourceView* pSRV_Src,
						UINT thread_count,
						UINT istride)
{
    // Setup execution configuration
	UINT grid = thread_count / COHERENCY_GRANULARITY;
	// Buffers -
	//		source
	crossplatform::DeviceContext &immediateContext=renderPlatform->GetImmediateContext();
	immediateContext.asD3D11DeviceContext()->CSSetShaderResources(0, 1,&pSRV_Src);
	//		destination
	immediateContext.asD3D11DeviceContext()->CSSetUnorderedAccessViews(0, 1,&pUAV_Dst, 0);
	// Shader
	if (istride > 1)
		immediateContext.asD3D11DeviceContext()->CSSetShader(pRadix008A_CS,NULL,0);
	else
		immediateContext.asD3D11DeviceContext()->CSSetShader(pRadix008A_CS2,NULL,0);
	// Dispatch means run the compute shader.
	immediateContext.asD3D11DeviceContext()->Dispatch(grid, 1, 1);
	// Unbind resource
	ID3D11ShaderResourceView* cs_srvs[1] = {NULL};
	immediateContext.asD3D11DeviceContext()->CSSetShaderResources(0, 1, cs_srvs);
	ID3D11UnorderedAccessView* cs_uavs[1] = {NULL};
	immediateContext.asD3D11DeviceContext()->CSSetUnorderedAccessViews(0, 1, cs_uavs,0);

	// THEN REDO THIS!
	// Shader
	if (istride > 1)
		immediateContext.asD3D11DeviceContext()->CSSetShader(pRadix008A_CS,NULL,0);
	else
		immediateContext.asD3D11DeviceContext()->CSSetShader(pRadix008A_CS2,NULL,0);
}
					 
void Fft::fft_512x512_c2c(	ID3D11UnorderedAccessView* pUAV_Dst,
									ID3D11ShaderResourceView* pSRV_Dst,
									ID3D11ShaderResourceView* pSRV_Src,int grid_size)
{
	crossplatform::DeviceContext &immediateContext=renderPlatform->GetImmediateContext();
	const UINT thread_count = slices * (grid_size * grid_size) / 8;
	ID3D11Buffer* cs_cbs[1];
	UINT istride = grid_size * grid_size / 8;
	// i.e. istride is 32768, 4096, 512, 64, 8, 1
	// So if istride is grid^2/8, and grid=2^X, then
	//			istride is 2^2X/(2^3) = 2^(2X-3)
	//	and how many times can we divide istride by 8? 8=2^3
	//			so after n divisions, we have istride -> 2^(2X-3-3n)
	//			i.e. say grid is 512, so X=9.
	//			then istride -> 2^(15-3n), which is 1 when 15-3n=0, 3n=15, n=5.
	//			so one more division (the 6th) sets istride to zero, being an integer.
	
	// This means that in general, 2X-3-3n=0 at the second-last division,
	//			so 3n=2X-3, n=2X/3-1.
	// But one more gives us the total number, which is N=2X/3.
	int i=0;
	while(istride>0)
	{
		cs_cbs[0] = ppRadix008A_CB[i];
		immediateContext.asD3D11DeviceContext()->CSSetConstantBuffers(0, 1, &cs_cbs[0]);
		// current source for the operation is either pSRV_Src (the first time), or swaps between pSRV_Tmp and pSRV_Dst.
		ID3D11ShaderResourceView *srv=(i%2==0?(i==0?pSRV_Src:pSRV_Dst):pSRV_Tmp);
		// destination for the operation alternates between pUAV_Tmp and pUAV_Dst. The final one should always be pUAV_Dst,
		// so we must do an even number of operations.
		ID3D11UnorderedAccessView *uav=(i%2==0?pUAV_Tmp:pUAV_Dst);
		radix008A(uav, srv, thread_count, istride);
		istride /= 8;
		i++;
	}
}

void Fft::CreateCBuffers( UINT slices,int size)
{
	// Create 6 cbuffers for 512x512 transform.

	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = 32;//sizeof(float) * 5;
	cb_desc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA cb_data;
	cb_data.SysMemPitch = 0;
	cb_data.SysMemSlicePitch = 0;

	struct CB_Structure
	{
		UINT thread_count;
		UINT ostride;
		UINT istride;
		UINT pstride;
		float phase_base;
	};

	// Buffer 0
	const UINT thread_count = slices * (size * size) / 8;
	UINT ostride = size * size / 8;
	UINT istride = ostride;
	double phase_base = -TWO_PI / ((double)size * (double)size);
	
	CB_Structure cb_data_buf0 =
	{
		thread_count,
		ostride,
		istride,
		size,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf0;
	if(ppRadix008A_CB)
	{
		for(int i=0;i<numBuffers;i++)
			SAFE_RELEASE(ppRadix008A_CB[i]);
	}
	int X=(int)(log((double)size)/log(2.0));
	numBuffers=2*X/3;
	ppRadix008A_CB=new ID3D11Buffer*[numBuffers];

#if 1
	int sz=size;
	for(int i=0;i<numBuffers;i++)
	{
		CB_Structure cb_data_buf0 =
		{
			thread_count,
			ostride,
			istride,
			sz,
			(float)phase_base
		};
		cb_data.pSysMem = &cb_data_buf0;

		renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[i]);
		assert(ppRadix008A_CB[i]);
		istride /= 8;
		phase_base *= 8.0;
		if(i==2)
		{
			ostride /= size;
			sz=1;
		}
	}
#else

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[0]);

	// Buffer 1
	istride /= 8;
	phase_base *= 8.0;
	
	CB_Structure cb_data_buf1 =
	{
		thread_count,
		ostride,
		istride,
		size,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf1;

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[1]);

	// Buffer 2
	istride /= 8;
	phase_base *= 8.0;
	
	CB_Structure cb_data_buf2 =
	{
		thread_count,
		ostride,
		istride,
		size,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf2;

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[2]);
	assert(ppRadix008A_CB[2]);

	// Buffer 3
	istride /= 8;
	phase_base *= 8.0;
	ostride /= size;
	
	CB_Structure cb_data_buf3 =
	{
		thread_count,
		ostride,
		istride,
		1,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf3;

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[3]);
	assert(ppRadix008A_CB[3]);

	// Buffer 4
	istride /= 8;
	phase_base *= 8.0;
	
	CB_Structure cb_data_buf4 =
	{
		thread_count,
		ostride,
		istride,
		1,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf4;

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[4]);
	assert(ppRadix008A_CB[4]);

	// Buffer 5
	istride /= 8;
	phase_base *= 8.0;
	
	CB_Structure cb_data_buf5 =
	{
		thread_count,
		ostride,
		istride,
		1,
		(float)phase_base
	};
	cb_data.pSysMem = &cb_data_buf5;

	renderPlatform->AsD3D11Device()->CreateBuffer(&cb_desc, &cb_data, &ppRadix008A_CB[5]);
	assert(ppRadix008A_CB[5]);
#endif
}
