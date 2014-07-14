
#include "Simul/Platform/DirectX11/SimulDirectXHeader.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/ocean_constants.sl"


//Memory access coherency (in threads)
#define COHERENCY_GRANULARITY 128


///////////////////////////////////////////////////////////////////////////////
// Common types
///////////////////////////////////////////////////////////////////////////////

struct Fft
{
	Fft();
	~Fft();
	void RestoreDeviceObjects(ID3D11Device* pd3dDevice, UINT slices,int size);
	void InvalidateDeviceObjects();
	void RecompileShaders();
	void CreateCBuffers(ID3D11Device* pd3dDevice, UINT slices,int size);
	void fft_512x512_c2c(	ID3D11UnorderedAccessView* pUAV_Dst,
							ID3D11ShaderResourceView* pSRV_Dst,
							ID3D11ShaderResourceView* pSRV_Src,int size);
	void radix008A(ID3D11UnorderedAccessView* pUAV_Dst,
				   ID3D11ShaderResourceView* pSRV_Src,
				   UINT thread_count,
				   UINT istride);
protected:
	ID3D11Device				*m_pd3dDevice;
	// D3D11 objects
	ID3D11DeviceContext			*pd3dImmediateContext;
	ID3D11ComputeShader			*pRadix008A_CS;
	ID3D11ComputeShader			*pRadix008A_CS2;

	// More than one array can be transformed at same time
	UINT						slices;

	// For 512x512 config, we need 6 constant buffers
	ID3D11Buffer				**ppRadix008A_CB;
	int							numBuffers;
	// Temporary buffers
	ID3D11Buffer				*pBuffer_Tmp;
	ID3D11UnorderedAccessView	*pUAV_Tmp;
	ID3D11ShaderResourceView	*pSRV_Tmp;
};

