
#define NOMINMAX
#include "CreateEffectDX1x.h"

#include <assert.h>
#include "OceanSimulator.h"
#include "Utilities.h"
#include "CompileShaderDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;

#define SAFE_DELETE_ARRAY(c) delete [] c;c=NULL;

// Disable warning "conditional expression is constant"
#pragma warning(disable:4127)


#define HALF_SQRT_2	0.7071068f
#define GRAV_ACCEL	981.0f	// The acceleration of gravity, cm/s^2

#define BLOCK_SIZE_X 16
#define BLOCK_SIZE_Y 16

// Generating gaussian random number with mean 0 and standard deviation 1.
float Gauss()
{
	float u1 = rand() / (float)RAND_MAX;
	float u2 = rand() / (float)RAND_MAX;
	if (u1 < 1e-6f)
		u1 = 1e-6f;
	return sqrtf(-2.f* logf(u1)) * cosf(2.f*3.1415926536f * u2);
}

// Phillips Spectrum
// K: normalized wave vector, W: wind direction, v: wind velocity, a: amplitude constant
float Phillips(D3DXVECTOR2 K, D3DXVECTOR2 W, float v, float a, float dir_depend)
{
	// largest possible wave from constant wind of velocity v
	float l = v * v / GRAV_ACCEL;
	// damp out waves with very small length w << l
	float w = l / 1000;

	float Ksqr = K.x * K.x + K.y * K.y;
	float Kcos = K.x * W.x + K.y * W.y;
	float phillips = a * expf(-1 / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips * expf(-Ksqr * w * w);
}

void createBufferAndUAV(ID3D11Device* pd3dDevice, void* data, UINT byte_width, UINT byte_stride,
						ID3D11Buffer** ppBuffer, ID3D11UnorderedAccessView** ppUAV, ID3D11ShaderResourceView** ppSRV)
{
	// Create buffer
	D3D11_BUFFER_DESC buf_desc;
	buf_desc.ByteWidth = byte_width;
    buf_desc.Usage = D3D11_USAGE_DEFAULT;
    buf_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    buf_desc.CPUAccessFlags = 0;
    buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    buf_desc.StructureByteStride = byte_stride;

	D3D11_SUBRESOURCE_DATA init_data = {data, 0, 0};

	pd3dDevice->CreateBuffer(&buf_desc, data != NULL ? &init_data : NULL, ppBuffer);
	assert(*ppBuffer);

	// Create undordered access view
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
	uav_desc.Format = DXGI_FORMAT_UNKNOWN;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uav_desc.Buffer.FirstElement = 0;
	uav_desc.Buffer.NumElements = byte_width / byte_stride;
	uav_desc.Buffer.Flags = 0;

	pd3dDevice->CreateUnorderedAccessView(*ppBuffer, &uav_desc, ppUAV);
	assert(*ppUAV);

	// Create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srv_desc.Buffer.FirstElement = 0;
	srv_desc.Buffer.NumElements = byte_width / byte_stride;

	pd3dDevice->CreateShaderResourceView(*ppBuffer, &srv_desc, ppSRV);
	assert(*ppSRV);
}

void createTextureAndViews(ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format,
						   ID3D11Texture2D** ppTex, ID3D11ShaderResourceView** ppSRV, ID3D11RenderTargetView** ppRTV)
{
	// Create 2D texture
	D3D11_TEXTURE2D_DESC tex_desc;
	tex_desc.Width = width;
	tex_desc.Height = height;
	tex_desc.MipLevels = 0;
	tex_desc.ArraySize = 1;
	tex_desc.Format = format;
	tex_desc.SampleDesc.Count = 1;
	tex_desc.SampleDesc.Quality = 0;
	tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

	pd3dDevice->CreateTexture2D(&tex_desc, NULL, ppTex);
	assert(*ppTex);

	// Create shader resource view
	(*ppTex)->GetDesc(&tex_desc);
	if (ppSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
		srv_desc.Format = format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
		srv_desc.Texture2D.MostDetailedMip = 0;

		pd3dDevice->CreateShaderResourceView(*ppTex, &srv_desc, ppSRV);
		assert(*ppSRV);
	}

	// Create render target view
	if (ppRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
		rtv_desc.Format = format;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtv_desc.Texture2D.MipSlice = 0;

		pd3dDevice->CreateRenderTargetView(*ppTex, &rtv_desc, ppRTV);
		assert(*ppRTV);
	}
}

OceanSimulator::OceanSimulator(simul::terrain::SeaKeyframer *s)
	:m_param(s)
	,m_pd3dDevice(NULL)
	,m_pd3dImmediateContext(NULL)
	,effect(NULL)
	,start_time(0.f)
{
}

void OceanSimulator::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	immutableConstants		.InvalidateDeviceObjects();
	changePerFrameConstants	.InvalidateDeviceObjects();

	choppy		.release();
	omega		.release();
	displacement.InvalidateDeviceObjects();
	gradient	.InvalidateDeviceObjects();
	dxyz		.release();
	h0			.release();
	m_fft.InvalidateDeviceObjects();

	SAFE_RELEASE(m_pd3dImmediateContext);
}

void OceanSimulator::RestoreDeviceObjects(ID3D11Device* pd3dDevice)
{
	m_pd3dDevice=pd3dDevice;
	// If the device becomes invalid at some point, delete current instance and generate a new one.
	assert(pd3dDevice);
	SAFE_RELEASE(m_pd3dImmediateContext);
	pd3dDevice->GetImmediateContext(&m_pd3dImmediateContext);
	assert(m_pd3dImmediateContext);
	// Height map H(0)
	int height_map_size = (m_param->dmap_dim + 4) * (m_param->dmap_dim + 1);
	D3DXVECTOR2* h0_data = new D3DXVECTOR2[height_map_size * sizeof(D3DXVECTOR2)];
	float* omega_data = new float[height_map_size * sizeof(float)];
	initHeightMap(h0_data, omega_data);

	int hmap_dim = m_param->dmap_dim;
	int input_full_size = (hmap_dim + 4) * (hmap_dim + 1);
	// This value should be (hmap_dim / 2 + 1) * hmap_dim, but we use full sized buffer here for simplicity.
	int input_half_size = hmap_dim * hmap_dim;
	int output_size = hmap_dim * hmap_dim;

	// For filling the buffer with zeroes.
	float* zero_data = new float[3 * output_size * 2];
	for(int i=0;i<3 * output_size * 2;i++)
		zero_data[i]=0.f;
	//memset(zero_data, 0, 3 * output_size * sizeof(float) * 2);
	
	// RW buffer allocations
	// H0
	UINT float2_stride = 2 * sizeof(float);
	createBufferAndUAV(pd3dDevice, h0_data, input_full_size * float2_stride, float2_stride
		, &h0.buffer, &h0.unorderedAccessView, &h0.shaderResourceView);

	// Notice: The following 3 buffers should be half sized buffer because of conjugate symmetric input. But
	// we use full sized buffers due to the CS4.0 restriction.

	// Put H(t), Dx(t) and Dy(t) into one buffer because CS4.0 allows only 1 UAV at a time
	createBufferAndUAV(pd3dDevice, zero_data, 3 * input_half_size * float2_stride, float2_stride,
		//&m_pBuffer_Float2_Ht, &m_pUAV_Ht, &m_pSRV_Ht);
		&choppy.buffer,&choppy.unorderedAccessView,&choppy.shaderResourceView);
	
	//choppy.RestoreDeviceObjects(pd3dDevice,3 * input_half_size);
	// omega
	createBufferAndUAV(pd3dDevice, omega_data, input_full_size * sizeof(float), sizeof(float)
		, &omega.buffer, &omega.unorderedAccessView, &omega.shaderResourceView);

	// Notice: The following 3 should be real number data. But here we use the complex numbers and C2C FFT
	// due to the CS4.0 restriction.
	// Put Dz, Dx and Dy into one buffer because CS4.0 allows only 1 UAV at a time
	createBufferAndUAV(pd3dDevice, zero_data, 3 * output_size * float2_stride, float2_stride
		, &dxyz.buffer, &dxyz.unorderedAccessView, &dxyz.shaderResourceView);

	SAFE_DELETE_ARRAY(zero_data);
	SAFE_DELETE_ARRAY(h0_data);
	SAFE_DELETE_ARRAY(omega_data);

	// Have now created: H0, Ht, Omega, Dxyz - as both UAV's and SRV's.

	// D3D11 Textures - these ar the outputs of the ocean simulator.
	displacement.ensureTexture2DSizeAndFormat(pd3dDevice,hmap_dim,hmap_dim,DXGI_FORMAT_R32G32B32A32_FLOAT,false,true);
	gradient.ensureTexture2DSizeAndFormat(pd3dDevice,hmap_dim,hmap_dim,DXGI_FORMAT_R16G16B16A16_FLOAT,false,true);

	immutableConstants		.RestoreDeviceObjects(pd3dDevice);
	changePerFrameConstants	.RestoreDeviceObjects(pd3dDevice);
	
	RecompileShaders();

	// Constant buffers
	UINT actual_dim = m_param->dmap_dim;

	// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
	immutableConstants.g_ActualDim			=actual_dim;
	immutableConstants.g_InWidth			=actual_dim + 4;
	immutableConstants.g_OutWidth			=actual_dim;
	immutableConstants.g_OutHeight			=actual_dim;
	immutableConstants.g_DxAddressOffset	=actual_dim*actual_dim;
	immutableConstants.g_DyAddressOffset	=actual_dim*actual_dim*2;

	// FFT
	//fft512x512_create_plan(&m_fft, pd3dDevice, 3);
	m_fft.RestoreDeviceObjects(pd3dDevice, 3);
}

void OceanSimulator::RecompileShaders()
{
	if(!m_pd3dDevice)
		return;
	SAFE_RELEASE(effect);
	effect					=LoadEffect(m_pd3dDevice,"ocean.fx");
	m_fft					.RecompileShaders();
	immutableConstants		.LinkToEffect(effect,"cbImmutable");
	changePerFrameConstants	.LinkToEffect(effect,"cbChangePerFrame");
}

OceanSimulator::~OceanSimulator()
{
	InvalidateDeviceObjects();
}

// Initialize the vector field.
// wlen_x: width of wave tile, in meters
// wlen_y: length of wave tile, in meters
void OceanSimulator::initHeightMap(D3DXVECTOR2* out_h0, float* out_omega)
{
	int i, j;
	D3DXVECTOR2 K, Kn;

	D3DXVECTOR2 wind_dir;
	D3DXVECTOR2 pwind_dir(m_param->wind_dir[0],m_param->wind_dir[1]);
	D3DXVec2Normalize(&wind_dir, &pwind_dir);
	float a = m_param->wave_amplitude * 1e-7f;	// It is too small. We must scale it for editing.
	float v = m_param->wind_speed;
	float dir_depend = m_param->wind_dependency;

	int height_map_dim = m_param->dmap_dim;
	float patch_length = m_param->patch_length;

	// initialize random generator.
	srand(0);

	for (i = 0; i <= height_map_dim; i++)
	{
		// K is wave-vector, range [-|DX/W, |DX/W], [-|DY/H, |DY/H]
		K.y = (-height_map_dim / 2.0f + i) * (2 * D3DX_PI / patch_length);

		for (j = 0; j <= height_map_dim; j++)
		{
			K.x = (-height_map_dim / 2.0f + j) * (2 * D3DX_PI / patch_length);

			float phil = (K.x == 0 && K.y == 0) ? 0 : sqrtf(Phillips(K, wind_dir, v, a, dir_depend));

			out_h0[i * (height_map_dim + 4) + j].x = phil*Gauss()*HALF_SQRT_2;
			out_h0[i * (height_map_dim + 4) + j].y = phil*Gauss()*HALF_SQRT_2;

			// The angular frequency is following the dispersion relation:
			//            out_omega^2 = g |k|
			// The equation of Gerstner wave:
			//            x = x0 - K/k * A * sin(dot(K, x0) - sqrt(g * k) * t), x is a 2D vector.
			//            z = A * cos(dot(K, x0) - sqrt(g * k) * t)
			// Gerstner wave shows that a point on a simple sinusoid wave is doing a uniform circular
			// motion with the center (x0, y0, z0), radius A, and the circular plane is parallel to
			// vector K.
			out_omega[i * (height_map_dim + 4) + j] = sqrtf(GRAV_ACCEL * sqrtf(K.x * K.x + K.y * K.y));
		}
	}
}

void OceanSimulator::updateDisplacementMap(float time)
{
	simul::crossplatform::DeviceContext deviceContext;
	deviceContext.platform_context=m_pd3dImmediateContext;
	// ---------------------------- H(0) -> H(t), D(x, t), D(y, t) --------------------------------
	// Compute shader
	ID3DX11EffectTechnique *tech	=effect->GetTechniqueByName("update_spectrum");
	// Buffers
	simul::dx11::setTexture(effect,"g_InputH0"		,h0.shaderResourceView);
	simul::dx11::setTexture(effect,"g_InputOmega"	,omega.shaderResourceView);

	simul::dx11::setUnorderedAccessView(effect,"g_OutputHt",choppy.unorderedAccessView);
	if(start_time==0)
		start_time=time;
	float time_seconds						=(time-start_time)*3600.f*24.f;
	changePerFrameConstants.g_Time			=time_seconds*m_param->time_scale;
	changePerFrameConstants.g_ChoppyScale	=m_param->choppy_scale;
	changePerFrameConstants.g_GridLen		=m_param->dmap_dim / m_param->patch_length;
	
	immutableConstants		.Apply(deviceContext);
	changePerFrameConstants	.Apply(deviceContext);

	// Run the CS
	UINT group_count_x = (m_param->dmap_dim + BLOCK_SIZE_X - 1) / BLOCK_SIZE_X;
	UINT group_count_y = (m_param->dmap_dim + BLOCK_SIZE_Y - 1) / BLOCK_SIZE_Y;
	tech->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	m_pd3dImmediateContext->Dispatch(group_count_x,group_count_y,1);

	simul::dx11::unbindTextures(effect);
	simul::dx11::setUnorderedAccessView(effect,"g_OutputHt",NULL);

	tech->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	// Perform Fast (inverse) Fourier Transform from the source Ht to the destination Dxyz.
	// NOTE: we also provide the SRV of Dxyz so that FFT can use it as a temporary buffer and save space.
	m_fft.fft_512x512_c2c(dxyz.unorderedAccessView,dxyz.shaderResourceView,choppy.shaderResourceView);
	// Now we will use the transformed Dxyz to create our displacement map
	// --------------------------------- Wrap Dx, Dy and Dz ---------------------------------------
	// Save the current RenderTarget and viewport:
	ID3D11RenderTargetView* old_target;
	ID3D11DepthStencilView* old_depth;
	m_pd3dImmediateContext->OMGetRenderTargets(1, &old_target, &old_depth); 
	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	m_pd3dImmediateContext->RSGetViewports(&num_viewport, &old_viewport);
	// Set the new viewport as equal to the size of the displacement texture we're writing to:
	D3D11_VIEWPORT new_vp={0,0,(float)m_param->dmap_dim,(float)m_param->dmap_dim,0.0f,1.0f};
	m_pd3dImmediateContext->RSSetViewports(1, &new_vp);
	// Set the RenderTarget as the displacement map:
	m_pd3dImmediateContext->OMSetRenderTargets(1, &displacement.renderTargetView, NULL);
	// Assign the constant-buffers to the pixel shader:
	immutableConstants		.Apply(deviceContext);
	changePerFrameConstants	.Apply(deviceContext);
	// Assign the Dxyz source as a resource for the pixel shader:
	simul::dx11::setTexture(effect,"g_InputDxyz"				,dxyz.shaderResourceView);
	// Assign the shaders:
	effect->GetTechniqueByName("update_displacement")->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	UtilityRenderer::DrawQuad(m_pd3dImmediateContext);
	// Unbind the shader resource (i.e. the input texture map). Must do this or we get a DX warning when we
	// try to write to the texture again:
	simul::dx11::unbindTextures(effect);
	simul::dx11::setTexture(effect,"g_InputDxyz"				,NULL);
	effect->GetTechniqueByName("update_displacement")->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	// Now generate the gradient map.
	// Set the gradient texture as the RenderTarget:
	m_pd3dImmediateContext->OMSetRenderTargets(1, &gradient.renderTargetView, NULL);
	// VS & PS
	// Use the Displacement map as the texture input:
	simul::dx11::setTexture(effect,"g_samplerDisplacementMap"	,displacement.shaderResourceView);
	effect->GetTechniqueByName("gradient_folding")->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	// Perform draw call
	UtilityRenderer::DrawQuad(m_pd3dImmediateContext);
	// Unbind the shader resource (the texture):
	simul::dx11::unbindTextures(effect);
	simul::dx11::setTexture(effect,"g_InputDxyz"				,(ID3D11ShaderResourceView*)NULL);
	effect->GetTechniqueByName("gradient_folding")->GetPassByIndex(0)->Apply(0,m_pd3dImmediateContext);
	// Reset the renderTarget to what it was before:
	m_pd3dImmediateContext->RSSetViewports(1, &old_viewport);
	m_pd3dImmediateContext->OMSetRenderTargets(1, &old_target, old_depth);
	SAFE_RELEASE(old_target);
	SAFE_RELEASE(old_depth);
	// Make mipmaps for the gradient texture, apparently this is a quick operation:
	m_pd3dImmediateContext->GenerateMips(gradient.shaderResourceView);
}

ID3D11ShaderResourceView* OceanSimulator::GetFftOutput()
{
	return dxyz.shaderResourceView;//m_pSRV_Dxyz;
}

ID3D11ShaderResourceView* OceanSimulator::GetFftInput()
{
	return h0.shaderResourceView;//m_pSRV_Dxyz;
}

ID3D11ShaderResourceView* OceanSimulator::getDisplacementMap()
{
	return displacement.shaderResourceView;
}

ID3D11ShaderResourceView* OceanSimulator::GetSpectrum()
{
	return choppy.shaderResourceView;
}

ID3D11ShaderResourceView* OceanSimulator::getGradientMap()
{
	return gradient.shaderResourceView;
}

const simul::terrain::SeaKeyframer *OceanSimulator::GetSeaKeyframer()
{
	return m_param;
}