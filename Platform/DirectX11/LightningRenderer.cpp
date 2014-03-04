#define NOMINMAX
#include "LightningRenderer.h"
#include "Simul/Sky/SkyInterface.h"

using namespace simul;
using namespace dx11;

LightningRenderer::LightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk)
	:BaseLightningRenderer(ck,sk)
	,effect(NULL)
	,inputLayout(NULL)
{
}

LightningRenderer::~LightningRenderer()
{
}

void LightningRenderer::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	vertexBuffer.ensureBufferSize(m_pd3dDevice,1000,NULL,false,true);
	lightningConstants.RestoreDeviceObjects(m_pd3dDevice);
	lightningPerViewConstants.RestoreDeviceObjects(m_pd3dDevice);
	RecompileShaders();
}

void LightningRenderer::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	CreateEffect(m_pd3dDevice,&effect,"lightning.fx",defines);
	D3DX11_PASS_DESC PassDesc;
	effect->GetTechniqueByIndex(0)->GetPassByIndex(0)->GetDesc(&PassDesc);
	SAFE_RELEASE(inputLayout);
	const D3D11_INPUT_ELEMENT_DESC mesh_layout_desc[] =
    {
        {"POSITION",0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,	D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,	D3D11_INPUT_PER_VERTEX_DATA,0},
    };
	V_CHECK(m_pd3dDevice->CreateInputLayout(mesh_layout_desc,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&inputLayout));

	lightningConstants.LinkToEffect(effect,"LightningConstants");
	lightningPerViewConstants.LinkToEffect(effect,"LightningPerViewConstants");
}

void LightningRenderer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	SAFE_RELEASE(inputLayout);
	lightningConstants.InvalidateDeviceObjects();
	lightningPerViewConstants.InvalidateDeviceObjects();
	vertexBuffer.release();
}

void LightningRenderer::Render(void *context,const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	LightningVertex *vertices=vertexBuffer.Map(pContext);
	if(!vertices)
		return;

	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout(&previousInputLayout);
	D3D10_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	D3D11_VIEWPORT viewport;
	UINT num_v		=1;
	pContext->RSGetViewports(&num_v,&viewport);

	D3DXMATRIX wvp;
	simul::dx11::MakeViewProjMatrix(wvp,(const float*)&view,(const float*)&proj);
	lightningPerViewConstants.worldViewProj=wvp;
	lightningPerViewConstants.worldViewProj.transpose();
	
	lightningPerViewConstants.viewportPixels=vec2(viewport.Width,viewport.Height);
	lightningPerViewConstants._line_width=4;
	lightningPerViewConstants.Apply(pContext);
	std::vector<int> start;
	std::vector<int> count;
	int v=0;
	float time=baseSkyInterface->GetTime();
	for(int i=0;i<cloudKeyframer->GetNumLightningBolts(time);i++)
	{
		const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,i);
		if(!lightningRenderInterface)
			continue;
		if(!lightningRenderInterface->IsSourceStarted(time))
			continue;
		sky::float4 colour=lightningRenderInterface->GetLightningColour();
		lightningConstants.lightningColour	=colour;
		lightningConstants.Apply(pContext);

		dx11::setParameter(effect,"lightningColour",colour);
		simul::sky::float4 x1,x2;
		static float maxwidth=8.f;
	static float ww=5.f;
		simul::math::Vector3 view_dir,cam_pos;
		GetCameraPosVector((const float*)&view);
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(int j=0;j<lightningRenderInterface->GetNumLevels();j++)
		{
			for(int jj=0;jj<lightningRenderInterface->GetNumBranches(j);jj++)
			{
				const simul::clouds::LightningRenderInterface::Branch &branch=lightningRenderInterface->GetBranch(time,j,jj);
				float dist=0.001f*(cam_pos-simul::math::Vector3(branch.vertices[0])).Magnitude();

				int v_start=v;
				start.push_back(v);

				for(int k=-1;k<branch.numVertices;k++)
				{
					if(v>=1000)
						break;
					bool start=(k<0);
					if(start)
						x1=(const float *)branch.vertices[k+2];
					else
						x1=(const float *)branch.vertices[k];
					if(k+1<branch.numVertices)
						x2=(const float *)branch.vertices[k+1];
					else
						x2=2.f*x1-simul::sky::float4((const float *)branch.vertices[k-1]);
					bool end=(k==branch.numVertices-1);
					if(k<0)
					{
						simul::sky::float4 dir=x2-x1;
						x1=x2+dir;
					}
					float width=branch.brightness*branch.width;
					width*=ww;

					float brightness=branch.brightness;
					float dh=x1.z/1000.f-K.cloud_base_km;
					if(dh>0)
					{
						static float cc=0.1f;
						float d=exp(-dh/cc);
						brightness*=d;
					}
					if(end)
						brightness=0.f;
					vertices[v].texCoords=vec4(width,width,x1.w,brightness);
					vertices[v].position=vec4(x1.x,x1.y,x1.z+vertical_shift,x1.w);
					v++;
				}
				count.push_back(v-v_start);
			}
		}
	}
	vertexBuffer.Unmap(pContext);
	vertexBuffer.apply(pContext,0);
	pContext->IASetInputLayout(inputLayout);

	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("lightning_thick");
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);

	ApplyPass(pContext,tech->GetPassByIndex(0));
	for(int i=0;i<start.size();i++)
	{
		if(i)
		{
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
			tech=effect->GetTechniqueByName("lightning_thin");
			ApplyPass(pContext,tech->GetPassByIndex(0));
		}
		if(count[i]>0)
			pContext->Draw(count[i],start[i]);
	}
	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout(previousInputLayout);
}