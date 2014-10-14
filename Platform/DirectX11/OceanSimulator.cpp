
#define NOMINMAX
#include "CreateEffectDX1x.h"

#include <assert.h>
#include "OceanSimulator.h"
#include "Utilities.h"
#include "CompileShaderDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "D3dx11effect.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Math/Pi.h"
using namespace simul;
using namespace dx11;

#define SAFE_DELETE_ARRAY(c) delete [] c;c=NULL;

// Disable warning "conditional expression is constant"
#pragma warning(disable:4127)


#define GRAV_ACCEL	9.810f	// The acceleration of gravity, m/s^2

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
float Phillips(vec2 K, vec2 W, float v, float a, float dir_depend)
{
	// largest possible wave from constant wind of velocity v
	// m^2/s^2 / (m/s^2)  = m
	float l = v * v / GRAV_ACCEL;
	// damp out waves with very small length w << l
	// w in m
	float w = l / 1000;

	float Ksqr = K.x * K.x + K.y * K.y;
	float Kcos = K.x * W.x + K.y * W.y;
	// The exponent must be unitless, so the units of l*l *(K.K) must be unity.
	// Therefore, K has units of 1/cm.
	float phillips = a * expf(-1.0f / (l * l * Ksqr)) / (Ksqr * Ksqr * Ksqr) * (Kcos * Kcos);

	// filter out waves moving opposite to wind
	if (Kcos < 0)
		phillips *= dir_depend;

	// damp out waves with very small length w << l
	return phillips * expf(-Ksqr * w * w);
}

OceanSimulator::OceanSimulator(simul::terrain::SeaKeyframer *s)
	:m_param(s)
	,renderPlatform(NULL)
	,effect(NULL)
	,start_time(0.f)
	,displacement(NULL)
	,gradient(NULL)
	,gridSize(0)
	,propertiesChecksum(0)
{
}

void OceanSimulator::InvalidateDeviceObjects()
{
	immutableConstants		.InvalidateDeviceObjects();
	changePerFrameConstants	.InvalidateDeviceObjects();

	Choppy	.InvalidateDeviceObjects();
	Omega	.InvalidateDeviceObjects();
	if(displacement)
		displacement->InvalidateDeviceObjects();
	if(gradient)
		gradient	->InvalidateDeviceObjects();
	dxyz	.InvalidateDeviceObjects();
	H0		.InvalidateDeviceObjects();
	m_fft	.InvalidateDeviceObjects();
	effect=NULL;
	SAFE_DELETE(displacement);
	SAFE_DELETE(gradient);
}

void OceanSimulator::RestoreDeviceObjects(simul::crossplatform::RenderPlatform *r)
{
	renderPlatform=r;
	if(!renderPlatform)
		return;
	SAFE_DELETE(displacement);
	SAFE_DELETE(gradient);
	displacement	=renderPlatform->CreateTexture();
	gradient		=renderPlatform->CreateTexture();
	
	immutableConstants		.RestoreDeviceObjects(renderPlatform);
	changePerFrameConstants	.RestoreDeviceObjects(renderPlatform);

	m_fft.RestoreDeviceObjects(renderPlatform);
	gridSize=0;
	// FFT
	//fft512x512_create_plan(&m_fft, pd3dDevice, 3);
}

void OceanSimulator::EnsureTablesInitialized(simul::crossplatform::DeviceContext &deviceContext)
{
	if(m_param->dmap_dim!=gridSize||propertiesChecksum!=m_param->CalcChecksum())
	{
		gridSize = m_param->dmap_dim;
		propertiesChecksum=m_param->CalcChecksum();
		m_fft.RestoreDeviceObjects(renderPlatform, 3,gridSize);
		int input_full_size = (gridSize + 4) * (gridSize + 1);
		// This value should be (hmap_dim / 2 + 1) * hmap_dim, but we use full sized buffer here for simplicity.
		int input_half_size = gridSize * gridSize;
		int output_size = gridSize * gridSize;
	
		// RW buffer allocations
		// H0
		H0.RestoreDeviceObjects(renderPlatform,input_full_size,false);
		// Notice: The following 3 buffers should be half sized buffer because of conjugate symmetric input. But
		// we use full sized buffers due to the CS4.0 restriction.
		// Put H(t), Dx(t) and Dy(t) into one buffer because CS4.0 allows only 1 UAV at a time
		Choppy.RestoreDeviceObjects(renderPlatform,3 * input_half_size,true);
		Omega.RestoreDeviceObjects(renderPlatform,input_full_size,false);

		// Notice: The following 3 should be real number data. But here we use the complex numbers and C2C FFT
		// due to the CS4.0 restriction.
		// Put Dz, Dx and Dy into one buffer because CS4.0 allows only 1 UAV at a time
		dxyz.RestoreDeviceObjects(renderPlatform, 3 * output_size,true);

		// Height map H(0)
		int height_map_size = (gridSize + 4) * (gridSize + 1);
		vec2* h0_data = new vec2[height_map_size * sizeof(vec2)];
		float* omega_data = new float[height_map_size * sizeof(float)];
		initHeightMap(h0_data, omega_data);
		H0.SetData(deviceContext,h0_data);
		Omega.SetData(deviceContext,omega_data);
		SAFE_DELETE_ARRAY(h0_data);
		SAFE_DELETE_ARRAY(omega_data);
		
		// Constant buffers
		// We use full sized data here. The value "output_width" should be actual_dim/2+1 though.
		immutableConstants.g_ActualDim			=gridSize;
		immutableConstants.g_InWidth			=gridSize + 4;
		immutableConstants.g_OutWidth			=gridSize;
		immutableConstants.g_OutHeight			=gridSize;
		immutableConstants.g_DxAddressOffset	=gridSize*gridSize;
		immutableConstants.g_DyAddressOffset	=gridSize*gridSize*2;

	}
	// Have now created: H0, Ht, Omega, Dxyz - as both UAV's and SRV's.

	// D3D11 Textures - these ar the outputs of the ocean simulator.
	displacement->ensureTexture2DSizeAndFormat(renderPlatform,gridSize,gridSize,crossplatform::RGBA_32_FLOAT,false,true);
	gradient->ensureTexture2DSizeAndFormat(renderPlatform,gridSize,gridSize,crossplatform::RGBA_16_FLOAT,false,true);
}

void OceanSimulator::SetShader(crossplatform::Effect		*e)
{
	if(!renderPlatform)
		return;
	effect=e;
	m_fft					.SetShader(e);
	m_fft					.RecompileShaders();
	immutableConstants		.LinkToEffect(effect,"cbImmutable");
	changePerFrameConstants	.LinkToEffect(effect,"cbChangePerFrame");
}

OceanSimulator::~OceanSimulator()
{
	InvalidateDeviceObjects();
}
static vec2 vec2Normalize(vec2 v)
{
	float sz=sqrt(v.x*v.x+v.y*v.y);
	v.x/=sz;
	v.y/=sz;
	return v;
}
// Initialize the vector field.
// wlen_x: width of wave tile, in meters
// wlen_y: length of wave tile, in meters
void OceanSimulator::initHeightMap(vec2* out_h0, float* out_omega)
{
	int i, j;
	vec2 K, Kn;

	vec2 wind_dir;
	vec2 pwind_dir(m_param->wind_dir[0],m_param->wind_dir[1]);
	if(fabs(pwind_dir.x)+fabs(pwind_dir.y)>0)
		wind_dir=vec2Normalize(pwind_dir);
	else
		wind_dir=vec2(1.0f,0);
	float a = m_param->wave_amplitude * 1e-4f;	// It is too small. We must scale it for editing.
	float v = m_param->wind_speed;
	float dir_depend = m_param->wind_dependency;

	int height_map_dim = gridSize;
	float patch_length = m_param->patch_length;

	// initialize random generator.
	srand(0);
	static float scale=0.5f;
	const float HALF_SQRT_2=0.7071068f;
	// scale_ratio=scale/patch_length;

	// Here we create a table, where the x and y axes represent the x and y components of
	// the wave vector, K.
	// This means that the larger waves are towards the centre of the table, and smaller
	// waves are towards the outside.
	// Also this means that the ratio of the largest to the smallest wavelength is given
	// roughly by half the resolution of the table.
	for (i = 0; i <= height_map_dim; i++)
	{
		// K is wave-vector, range [-|DX/W, |DX/W], [-|DY/H, |DY/H]
		// According to Tessendorf, k points in the wave's direction of travel,
		// and has magnitude k related to the wavelength l by k=2pi/l.
		K.y = (-1.0f+2.0f*(float)i/(float)height_map_dim)*2.0f*pi/scale;
		// So K.y goes from -2 pi/scale to +2 pi/scale
		// And so the wavelength range goes from scale to scale*gridsize/2
		for (j = 0; j <= height_map_dim; j++)
		{
			K.x = (-1.0f+2.0f*(float)j/(float)height_map_dim)*2.0f*pi/scale;
			// i.e. K.x goes from 
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

void OceanSimulator::updateDisplacementMap(simul::crossplatform::DeviceContext &deviceContext,float time)
{
	EnsureTablesInitialized(deviceContext);
	// ---------------------------- H(0) -> H(t), D(x, t), D(y, t) --------------------------------
	// Compute shader
	crossplatform::EffectTechnique *tech	=effect->GetTechniqueByName("update_spectrum");
	// Buffers
	simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputH0"	,H0.AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputOmega"	,Omega.AsD3D11ShaderResourceView());

	simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"g_OutputHt",Choppy.AsD3D11UnorderedAccessView());
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
	effect->Apply(deviceContext,tech,0);
	renderPlatform->DispatchCompute(deviceContext,group_count_x,group_count_y,1);

	simul::dx11::unbindTextures(effect->asD3DX11Effect());
	simul::dx11::setUnorderedAccessView(effect->asD3DX11Effect(),"g_OutputHt",NULL);
	effect->Unapply(deviceContext);
	// Perform Fast (inverse) Fourier Transform from the source Ht to the destination Dxyz.
	// NOTE: we also provide the SRV of Dxyz so that FFT can use it as a temporary buffer and save space.
	m_fft.fft_512x512_c2c(deviceContext,dxyz.AsD3D11UnorderedAccessView(),dxyz.AsD3D11ShaderResourceView(),Choppy.AsD3D11ShaderResourceView(),gridSize);
	// Now we will use the transformed Dxyz to create our displacement map
	// --------------------------------- Wrap Dx, Dy and Dz ---------------------------------------
	// Save the current RenderTarget and viewport:
	ID3D11RenderTargetView* old_target;
	ID3D11DepthStencilView* old_depth;
	deviceContext.asD3D11DeviceContext()->OMGetRenderTargets(1, &old_target, &old_depth); 
	D3D11_VIEWPORT old_viewport;
	UINT num_viewport = 1;
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num_viewport, &old_viewport);
	// Set the new viewport as equal to the size of the displacement texture we're writing to:
	D3D11_VIEWPORT new_vp={0,0,(float)m_param->dmap_dim,(float)m_param->dmap_dim,0.0f,1.0f};
	deviceContext.asD3D11DeviceContext()->RSSetViewports(1, &new_vp);
	// Set the RenderTarget as the displacement map:
	{
		displacement->activateRenderTarget(deviceContext);
	//	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1, &displacement.renderTargetView, NULL);
		// Assign the constant-buffers to the pixel shader:
		immutableConstants		.Apply(deviceContext);
		changePerFrameConstants	.Apply(deviceContext);
		// Assign the Dxyz source as a resource for the pixel shader:
		simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz"				,dxyz.AsD3D11ShaderResourceView());
		// Assign the shaders:
		effect->Apply(deviceContext,effect->GetTechniqueByName("update_displacement"),0);
		UtilityRenderer::DrawQuad(deviceContext);
		// Unbind the shader resource (i.e. the input texture map). Must do this or we get a DX warning when we
		// try to write to the texture again:
		simul::dx11::unbindTextures(effect->asD3DX11Effect());
		simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz"				,NULL);
		effect->Unapply(deviceContext);
		unbindTextures(effect->asD3DX11Effect());
		// Now generate the gradient map.
		// Set the gradient texture as the RenderTarget:
		displacement->deactivateRenderTarget();
	}
	{
		gradient->activateRenderTarget(deviceContext);
		//deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1, &gradient.renderTargetView, NULL);
		// VS & PS
		// Use the Displacement map as the texture input:
		effect->SetTexture(deviceContext,"g_samplerDisplacementMap"	,displacement);
		// Perform draw call
		effect->Apply(deviceContext,effect->GetTechniqueByName("gradient_folding"),0);
		UtilityRenderer::DrawQuad(deviceContext);
		// Unbind the shader resource (the texture):
		simul::dx11::unbindTextures(effect->asD3DX11Effect());
		simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz"				,(ID3D11ShaderResourceView*)NULL);
		effect->GetTechniqueByName("gradient_folding")->asD3DX11EffectTechnique()->GetPassByIndex(0)->Apply(0,deviceContext.asD3D11DeviceContext());
		// Reset the renderTarget to what it was before:
		gradient->deactivateRenderTarget();
		effect->UnbindTextures(deviceContext);
		effect->Unapply(deviceContext);
	}
	deviceContext.asD3D11DeviceContext()->RSSetViewports(1, &old_viewport);
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(1, &old_target, old_depth);
	SAFE_RELEASE(old_target);
	SAFE_RELEASE(old_depth);
	// Make mipmaps for the gradient texture, apparently this is a quick operation:
	deviceContext.asD3D11DeviceContext()->GenerateMips(gradient->AsD3D11ShaderResourceView());
}

ID3D11ShaderResourceView* OceanSimulator::GetFftOutput()
{
	return dxyz.AsD3D11ShaderResourceView();//m_pSRV_Dxyz;
}

ID3D11ShaderResourceView* OceanSimulator::GetFftInput()
{
	return H0.AsD3D11ShaderResourceView();//m_pSRV_Dxyz;
}

crossplatform::Texture* OceanSimulator::getDisplacementMap()
{
	return displacement;
}

ID3D11ShaderResourceView* OceanSimulator::GetSpectrum()
{
	return Choppy.AsD3D11ShaderResourceView();
}

crossplatform::Texture* OceanSimulator::getGradientMap()
{
	return gradient;
}

const simul::terrain::SeaKeyframer *OceanSimulator::GetSeaKeyframer()
{
	return m_param;
}