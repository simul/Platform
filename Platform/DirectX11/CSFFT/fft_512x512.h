
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/ocean_constants.sl"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

//Memory access coherency (in threads)
#define COHERENCY_GRANULARITY 128

struct Fft
{
	Fft();
	~Fft();
	void RestoreDeviceObjects(simul::crossplatform::RenderPlatform* r, unsigned slices=0,int size=0);
	void InvalidateDeviceObjects();
	void RecompileShaders();
	void SetShader(simul::crossplatform::Effect *e);
	void CreateCBuffers( unsigned slices,int size);
	void fft_512x512_c2c(simul::crossplatform::DeviceContext &deviceContext
							,simul::crossplatform::StructuredBuffer<vec2> &dst,
							simul::crossplatform::StructuredBuffer<vec2> &src,int size);
	void radix008A(simul::crossplatform::DeviceContext &deviceContext
					,simul::crossplatform::StructuredBuffer<vec2> &dst,
				   simul::crossplatform::StructuredBuffer<vec2> &src,
				   unsigned thread_count,
				   unsigned istride);
protected:
	simul::crossplatform::RenderPlatform* renderPlatform;
	simul::crossplatform::Effect *effect;
	simul::crossplatform::ConstantBuffer<FftConstants> fftConstants;
	FftConstants fftc[10];
	// More than one array can be transformed at same time
	unsigned						slices;
	// For 512x512 config, we need 6 constant buffers
	int							numBuffers;
	// Temporary buffers
	simul::crossplatform::StructuredBuffer<vec2> tempBuffer;
};

