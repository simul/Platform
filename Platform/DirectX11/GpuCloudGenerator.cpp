#define NOMINMAX
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "GpuCloudGenerator.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#include "CreateEffectDX1x.h"
#include "D3dx11effect.h"
#include <math.h>

using namespace simul;
using namespace dx11;

GpuCloudGenerator::GpuCloudGenerator()
{
	for(int i=0;i<3;i++)
		finalTexture[i]=NULL;
	for(int i=0;i<2;i++)
	{
		directLightTextures[i]=NULL;
		indirectLightTextures[i]=NULL;
	}
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	//if(iformat==DXGI_FORMAT_R32G32B32A32_FLOAT)
	//	size=4;
	return grid[0]*grid[1]*grid[2];
}


void GpuCloudGenerator::PerformGPURelight	(int light_index
											,const clouds::GpuCloudsParameters &params
											,float *target
											,int start_texel
											,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext=renderPlatform->GetImmediateContext();
	start_texel*=2;
	texels*=2;
	const int *light_grid=NULL;
	if(light_index==0)
		light_grid=params.density_grid;
	else
		light_grid=params.light_grid;
	int gridsize=light_grid[0]*light_grid[1]*light_grid[2];
	if(start_texel<0)
		start_texel=0;
	if(start_texel>gridsize)
		start_texel=gridsize;	
	if(start_texel+texels>gridsize)
		texels=gridsize-start_texel; 
	directLightTextures[light_index]->ensureTexture3DSizeAndFormat(renderPlatform
				,light_grid[0],light_grid[1],light_grid[2]
				,crossplatform::R_32_FLOAT,true);
	indirectLightTextures[light_index]->ensureTexture3DSizeAndFormat(renderPlatform
				,light_grid[0],light_grid[1],light_grid[2]
				,crossplatform::R_32_FLOAT,true);

	//SetGpuCloudConstants(gpuCloudConstants);
	gpuCloudConstants.yRange			=vec4(0.0,1.0,0,0);
	if(light_index==0)
	{
		gpuCloudConstants.extinctions		=vec2(params.lightspace_extinctions[2],params.lightspace_extinctions[3]);
		gpuCloudConstants.transformMatrix	=params.Matrix4x4AmbientToDensityTexcoords;
	}
	else
	{
		gpuCloudConstants.extinctions		=vec2(params.lightspace_extinctions[0],params.lightspace_extinctions[1]);
		gpuCloudConstants.transformMatrix	=params.Matrix4x4LightToDensityTexcoords;
	}
	gpuCloudConstants.transformMatrix.transpose();
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)light_grid[2]);

	//transformMatrix * (0,0,1)
	simul::sky::float4 step(gpuCloudConstants.transformMatrix._31,gpuCloudConstants.transformMatrix._32,gpuCloudConstants.transformMatrix._33,0);
	// We require stepLength to be the distance in km of each step along the light path.
	// So we divide by the light texel count in the light direction, then multiply by the three density axis scales.
	step*=gpuCloudConstants.zPixelLightspace;
	step*=simul::sky::float4(params.DensityGridScalesM[0],params.DensityGridScalesM[1],params.DensityGridScalesM[2],1.0);
	gpuCloudConstants.stepLength		=simul::sky::length(step); 
	if(params.wrap_light_tex[light_index])
		effect->SetSamplerState(deviceContext,"lightSamplerState",m_pWwcSamplerState);
	else if(light_grid[0]>light_grid[1])
		effect->SetSamplerState(deviceContext,"lightSamplerState",m_pWccSamplerState);
	else
		effect->SetSamplerState(deviceContext,"lightSamplerState",m_pCwcSamplerState);
	effect->SetTexture(deviceContext,"densityTexture",density_texture);

	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(light_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(light_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(light_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;

	// discard z dimension:
	int t0	=(start_texel)/light_grid[2];
	int t1	=(start_texel+texels+light_grid[2]-1)/light_grid[2];
	int t	=t1-t0;
	// which blocks to execute?
	int x0	=t0/BLOCKSIZE/subgrid.y;
	int x1	=((t0+t+BLOCKSIZE-1)/BLOCKSIZE+subgrid.y-1)/subgrid.y;
	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(deviceContext);
	if(x1>x0)
	{
		effect->SetUnorderedAccessView(deviceContext,"targetTexture1",directLightTextures[light_index]);
	
	effect->SetTexture(deviceContext,"densityTexture",density_texture);
		effect->Apply(deviceContext,lightingComputeTechnique,0);
		deviceContext.asD3D11DeviceContext()->Dispatch(x1-x0,subgrid.y,1);
	effect->SetTexture(deviceContext,"densityTexture",NULL);
		effect->Unapply(deviceContext);
	}
	int z0	=start_texel/light_grid[1]/light_grid[0];
	int z1	=(start_texel+texels+light_grid[1]*light_grid[0]-1)/light_grid[1]/light_grid[0];
	if(z1>z0)
	{
		if(harmonic_secondary)
		{
			gpuCloudConstants.threadOffset=uint3(0,0,0);
			gpuCloudConstants.Apply(deviceContext);
			effect->SetTexture(deviceContext,"lightTexture1"				,directLightTextures[light_index]);
			effect->SetUnorderedAccessView(deviceContext,"targetTexture1",indirectLightTextures[light_index]);
	
	effect->SetTexture(deviceContext,"densityTexture",density_texture);
			effect->Apply(deviceContext,secondaryHarmonicTechnique,0);
			deviceContext.asD3D11DeviceContext()->Dispatch(subgrid.x,subgrid.y,subgrid.z);
			effect->SetUnorderedAccessView(deviceContext,"targetTexture1",(ID3D11UnorderedAccessView*)NULL);
	
	effect->SetTexture(deviceContext,"densityTexture",NULL);
		effect->Unapply(deviceContext);
		}
		else
		for(int z=z0;z<z1;z++)
		{
			gpuCloudConstants.threadOffset=uint3(0,0,z);
			gpuCloudConstants.Apply(deviceContext);
			effect->SetUnorderedAccessView(deviceContext,"targetTexture1",indirectLightTextures[light_index]);
			effect->SetTexture(deviceContext,"lightTexture1"				,directLightTextures[light_index]);
	
	effect->SetTexture(deviceContext,"densityTexture",density_texture);
			effect->Apply(deviceContext,secondaryLightingComputeTechnique,0);
			deviceContext.asD3D11DeviceContext()->Dispatch(subgrid.x,subgrid.y,1);
			effect->SetUnorderedAccessView(deviceContext,"targetTexture1",NULL);
	
	effect->SetTexture(deviceContext,"densityTexture",NULL);
			effect->Unapply(deviceContext);
		}
	}

	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		directLightTextures[light_index]->copyToMemory(deviceContext,target,start_texel,texels);
		target+=gridsize;
		indirectLightTextures[light_index]->copyToMemory(deviceContext,target,start_texel,texels);
	}
}

void GpuCloudGenerator::GPUTransferDataToTexture(int cycled_index
												,const clouds::GpuCloudsParameters &params
												,unsigned char *target
												,int start_texel
												,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext=renderPlatform->GetImmediateContext();
	int density_gridsize				=params.density_grid[0]*params.density_grid[1]*params.density_grid[2];

	int z0								=start_texel/(params.density_grid[0]*params.density_grid[1]);
	int z1								=(start_texel+texels)/(params.density_grid[0]*params.density_grid[1]);
	int zmax							=params.density_grid[2];

	float y_start						=(float)z0/(float)zmax;
	float y_range						=(float)z1/(float)zmax-y_start;
	gpuCloudConstants.yRange			=vec4(y_start,y_range,0,0);
	gpuCloudConstants.transformMatrix	=params.Matrix4x4DensityToLightTransform;
	gpuCloudConstants.transformMatrix.transpose();

	gpuCloudConstants.zSize				=((float)params.density_grid[2]);
	gpuCloudConstants.zPixel			=(1.f/(float)params.density_grid[2]);
	gpuCloudConstants.zPixelLightspace	=(1.f/(float)params.light_grid[2]);

	setTexture(effect->asD3DX11Effect(),"densityTexture"	,density_texture->AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"ambientTexture1"	,directLightTextures[0]->AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"ambientTexture2"	,indirectLightTextures[0]->AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"lightTexture1"		,directLightTextures[1]->AsD3D11ShaderResourceView());
	setTexture(effect->asD3DX11Effect(),"lightTexture2"		,indirectLightTextures[1]->AsD3D11ShaderResourceView());
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	gpuCloudConstants.Apply(deviceContext);
	for(int i=0;i<3;i++)
	{
		finalTexture[i]->ensureTexture3DSizeAndFormat(renderPlatform,params.density_grid[0],params.density_grid[1],params.density_grid[2],crossplatform::RGBA_8_UNORM,true);
	}
	// divide the grid into 8x8x8 blocks:
	static const int BLOCKWIDTH=8;
	static const int BLOCKSIZE=BLOCKWIDTH*BLOCKWIDTH*BLOCKWIDTH;
	uint3 subgrid;
	subgrid.x=(params.density_grid[0]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.y=(params.density_grid[1]+BLOCKWIDTH-1)/BLOCKWIDTH;
	subgrid.z=(params.density_grid[2]+BLOCKWIDTH-1)/BLOCKWIDTH;
	int subgridsize=subgrid.x*subgrid.y*subgrid.z;
	// which blocks to execute?
	int x0	=start_texel/BLOCKSIZE/subgrid.y/subgrid.z;
	int x1	=(((start_texel+texels+BLOCKSIZE-1)/(BLOCKSIZE)+subgrid.y-1)/subgrid.y+subgrid.z-1)/subgrid.z;

	gpuCloudConstants.threadOffset=uint3(x0*BLOCKWIDTH,0,0);
	gpuCloudConstants.Apply(deviceContext);
	//simul::dx11::setParameter(effect->asD3DX11Effect(),"targetTexture",density_texture.AsD3D11UnorderedAccessView());
	effect->SetUnorderedAccessView(deviceContext,"targetTexture",finalTexture[cycled_index]);
	effect->Apply(deviceContext,transformComputeTechnique,0);
	if(x1>x0)
		deviceContext.asD3D11DeviceContext()->Dispatch(x1-x0,subgrid.y,subgrid.z);
	effect->SetUnorderedAccessView(deviceContext,"targetTexture",(ID3D11UnorderedAccessView*)NULL);
	setTexture(effect->asD3DX11Effect(),"densityTexture"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"ambientTexture1"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"ambientTexture2"	,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"lightTexture1"		,(ID3D11ShaderResourceView*)NULL);
	setTexture(effect->asD3DX11Effect(),"lightTexture2"		,(ID3D11ShaderResourceView*)NULL);
		effect->Unapply(deviceContext);
	// copy to CPU memory if required by CloudKeyframer.
	if(target)
	{
		finalTexture[cycled_index]->copyToMemory(deviceContext,target,start_texel,texels);
	}
}