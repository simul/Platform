// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// Simul2DCloudRendererDX11.cpp A renderer for 2D cloud layers.
#define NOMINMAX
#include "Simul2DCloudRendererdx1x.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Clouds/FastCloudNode.h"
#include "Simul/Clouds/TextureGenerator.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Math/Pi.h"
#include "CreateEffectDX1x.h"
#include "Simul/Platform/Crossplatform/Layout.h"
#include "Simul/Platform/Crossplatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/Crossplatform/Buffer.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "D3dx11effect.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx9.h>
#include <d3dx11.h>
#endif

using namespace simul;
using namespace dx11;

Simul2DCloudRendererDX11::Simul2DCloudRendererDX11(simul::clouds::CloudKeyframer *ck,simul::base::MemoryInterface *mem) :
	simul::clouds::Base2DCloudRenderer(ck,mem)
{
}

Simul2DCloudRendererDX11::~Simul2DCloudRendererDX11()
{
	InvalidateDeviceObjects();
}

bool Simul2DCloudRendererDX11::Render(crossplatform::DeviceContext &deviceContext,float exposure,bool cubemap,crossplatform::NearFarPass nearFarPass
									  ,crossplatform::Texture *depthTexture,bool write_alpha
									  ,const simul::sky::float4& viewportTextureRegionXYWH,const simul::sky::float4& )
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"2DCloudRenderer")
	
	crossplatform::EffectTechnique*		tech			=technique;
	if(depthTexture&&depthTexture->GetSampleCount()>1)
	{
			tech=msaaTechnique;
	}
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"Set constants")
	float ir_integration_factors[]={0,0,0,0};
	Set2DCloudConstants(cloud2DConstants,deviceContext.viewStruct.view,deviceContext.viewStruct.proj,exposure,viewportTextureRegionXYWH,ir_integration_factors);
	cloud2DConstants.Apply(deviceContext);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	if(tech)//&&cloud2DConstants.cloudiness>0)
	{

	effect->SetTexture(deviceContext,"imageTexture",detail);
	effect->SetTexture(deviceContext,"noiseTexture",noise);
	effect->SetTexture(deviceContext,"coverageTexture",coverage);
	simul::dx11::setTexture(effect->asD3DX11Effect(),"lossTexture",skyLossTexture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect->asD3DX11Effect(),"inscTexture",overcInscTexture->AsD3D11ShaderResourceView());
	simul::dx11::setTexture(effect->asD3DX11Effect(),"skylTexture",skylightTexture->AsD3D11ShaderResourceView());
	// Set both MS and regular - we'll only use one of them:
		if(depthTexture&&depthTexture->GetSampleCount()>0)
			effect->SetTexture(deviceContext,"depthTextureMS",depthTexture);
	else
			effect->SetTexture(deviceContext,"depthTexture",depthTexture);
	effect->SetTexture(deviceContext,"illuminationTexture",illuminationTexture);
	effect->SetTexture(deviceContext,"lightTableTexture",lightTableTexture);
	
	math::Vector3 cam_pos=simul::dx11::GetCameraPosVector(deviceContext.viewStruct.view,false);


	UINT prevOffset;
	DXGI_FORMAT prevFormat;
	ID3D11Buffer* pPrevBuffer;
	pContext->IAGetIndexBuffer(&pPrevBuffer, &prevFormat, &prevOffset);

	inputLayout->Apply(deviceContext);
	renderPlatform->SetVertexBuffers(deviceContext,0,1,&vertexBuffer);
	renderPlatform->SetIndexBuffer(deviceContext,indexBuffer);		

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	effect->Apply(deviceContext,tech,0);
	SIMUL_COMBINED_PROFILE_START(deviceContext.platform_context,"DrawIndexed")

	pContext->DrawIndexed(num_indices-2,0,0);
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)

	pContext->IASetIndexBuffer(pPrevBuffer, prevFormat, prevOffset);
	inputLayout->Unapply(deviceContext);
	SAFE_RELEASE(pPrevBuffer);
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
	}
	SIMUL_COMBINED_PROFILE_END(deviceContext.platform_context)
	return true;
}
