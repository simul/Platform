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

#include <d3d11.h>
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/CppSl.hs"
#include "Simul/Platform/CrossPlatform/ocean_constants.sl"


//Memory access coherency (in threads)
#define COHERENCY_GRANULARITY 128


///////////////////////////////////////////////////////////////////////////////
// Common types
///////////////////////////////////////////////////////////////////////////////

struct FFT_512x512
{
	FFT_512x512();
	~FFT_512x512();
	void RestoreDeviceObjects(ID3D11Device* pd3dDevice, UINT slices);
	void InvalidateDeviceObjects();
	void RecompileShaders();
	void CreateCBuffers(ID3D11Device* pd3dDevice, UINT slices);
	void fft_512x512_c2c(	ID3D11UnorderedAccessView* pUAV_Dst,
							ID3D11ShaderResourceView* pSRV_Dst,
							ID3D11ShaderResourceView* pSRV_Src);
	void radix008A(ID3D11UnorderedAccessView* pUAV_Dst,
				   ID3D11ShaderResourceView* pSRV_Src,
				   UINT thread_count,
				   UINT istride);
protected:
	ID3D11Device* m_pd3dDevice;
	// D3D11 objects
	ID3D11DeviceContext* pd3dImmediateContext;
	ID3D11ComputeShader* pRadix008A_CS;
	ID3D11ComputeShader* pRadix008A_CS2;

	// More than one array can be transformed at same time
	UINT slices;

	// For 512x512 config, we need 6 constant buffers
	ID3D11Buffer* pRadix008A_CB[6];
	// Temporary buffers
	ID3D11Buffer* pBuffer_Tmp;
	ID3D11UnorderedAccessView* pUAV_Tmp;
	ID3D11ShaderResourceView* pSRV_Tmp;
};

