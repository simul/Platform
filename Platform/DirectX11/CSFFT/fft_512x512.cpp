#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "fft_512x512.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/DirectX11/Utilities.h"
using namespace simul;

#define TWO_PI 6.283185307179586476925286766559

#define FFT_DIMENSIONS 3U
#define FFT_PLAN_SIZE_LIMIT (1U << 27)

#define FFT_FORWARD -1
#define FFT_INVERSE 1

Fft::Fft()
	:renderPlatform(NULL)
	,effect(NULL)
	,numBuffers(0)
{
}
Fft::~Fft()
{
	InvalidateDeviceObjects();
}

void Fft::RestoreDeviceObjects(crossplatform::RenderPlatform* r, unsigned s,int size)
{
	InvalidateDeviceObjects();
	renderPlatform=r;
	fftConstants.RestoreDeviceObjects(renderPlatform);
	// Context
	RecompileShaders();
	
	if(s&&size)
	{
		slices = s;
		// Constants
		// Create 6 cbuffers for 512x512 transform
		CreateCBuffers(slices,size);
		tempBuffer.RestoreDeviceObjects(renderPlatform,(size* slices) * size,true);
	}
}

void Fft::RecompileShaders()
{
}

void Fft::SetShader(simul::crossplatform::Effect *e)
{
	if(effect!=e)
	{
		effect=e;
		fftConstants.LinkToEffect(effect,"FftConstants");
	}
}

void Fft::InvalidateDeviceObjects()
{
	fftConstants.InvalidateDeviceObjects();
	tempBuffer.InvalidateDeviceObjects();
	numBuffers=0;
	renderPlatform=NULL;
	effect=NULL;
}

void Fft::radix008A(	crossplatform::DeviceContext &deviceContext
						,simul::crossplatform::StructuredBuffer<vec2> &dst
						,simul::crossplatform::StructuredBuffer<vec2> &src
						,unsigned thread_count
						,unsigned istride)
{
    // Setup execution configuration
	unsigned grid = thread_count / COHERENCY_GRANULARITY;
	src.Apply(deviceContext,effect,"g_SrcData");
	dst.ApplyAsUnorderedAccessView(deviceContext,effect,"g_DstData");
	if(istride>1)
		effect->Apply(deviceContext,effect->GetTechniqueByName("radix_008a"),0);
	else
		effect->Apply(deviceContext,effect->GetTechniqueByName("radix_008a2"),0);
	deviceContext.asD3D11DeviceContext()->Dispatch(grid, 1, 1);
	effect->SetTexture(deviceContext,"g_SrcData",NULL);
	effect->SetUnorderedAccessView(deviceContext,"g_DstData",NULL);
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
}
					 
void Fft::fft_512x512_c2c(crossplatform::DeviceContext &deviceContext,simul::crossplatform::StructuredBuffer<vec2> &dst,
							simul::crossplatform::StructuredBuffer<vec2> &src,int grid_size)
{
	crossplatform::DeviceContext &immediateContext=renderPlatform->GetImmediateContext();
	const unsigned thread_count = slices*(grid_size*grid_size)/8;
	ID3D11Buffer* cs_cbs[1];
	unsigned istride = grid_size * grid_size / 8;
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
		FftConstants &f=fftConstants;
		f=fftc[i];
		fftConstants.Apply(deviceContext);
		// current source for the operation is either pSRV_Src (the first time), or swaps between pSRV_Tmp and pSRV_Dst.
		simul::crossplatform::StructuredBuffer<vec2> *srv=(i%2==0?(i==0?&src:&dst):&tempBuffer);
		// destination for the operation alternates between pUAV_Tmp and pUAV_Dst. The final one should always be pUAV_Dst,
		// so we must do an even number of operations.
		simul::crossplatform::StructuredBuffer<vec2> *uav=(i%2==0?&tempBuffer:&dst);
		radix008A(deviceContext,*uav,*srv,thread_count,istride);
		istride/=8;
		i++;
	}
}

void Fft::CreateCBuffers( unsigned slices,int size)
{
	// Create 6 cbuffers for 512x512 transform.

	// Buffer 0
	const unsigned thread_count = slices * (size * size) / 8;
	unsigned ostride = size * size / 8;
	unsigned istride = ostride;
	double phase_base = -TWO_PI / ((double)size * (double)size);
	
	FftConstants cb_data_buf0 =
	{
		thread_count,
		ostride,
		istride,
		size,
		(float)phase_base
	};
/*	if(ppRadix008A_CB)
	{
		for(int i=0;i<numBuffers;i++)
			SAFE_RELEASE(ppRadix008A_CB[i]);
	}*/
	int X=(int)(log((double)size)/log(2.0));
	numBuffers=2*X/3;
	//ppRadix008A_CB=new ID3D11Buffer*[numBuffers];

	int sz=size;
	for(int i=0;i<numBuffers;i++)
	{
		FftConstants cb_data_buf0 =
		{
			thread_count,
			ostride,
			istride,
			sz,
			(float)phase_base
			,0.f,0.f,0.f
		};
		fftc[i]=cb_data_buf0;

		istride /= 8;
		phase_base *= 8.0;
		if(i==2)
		{
			ostride /= size;
			sz=1;
		}
	}
}
