#define NOMINMAX
#include "LightningRenderer.h"
#include "Simul/Sky/SkyInterface.h"

using namespace simul;
using namespace dx11;

LightningRenderer::LightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk)
	:BaseLightningRenderer(ck,sk)
	,effect(NULL)
{
}

LightningRenderer::~LightningRenderer()
{
}

void LightningRenderer::RestoreDeviceObjects(void* dev)
{
	m_pd3dDevice=(ID3D11Device*)dev;
	vertexBuffer.ensureBufferSize(m_pd3dDevice,1000,NULL);
	RecompileShaders();
}

void LightningRenderer::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	CreateEffect(m_pd3dDevice,&effect,"lightning.fx",defines);
}

void LightningRenderer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(effect);
	vertexBuffer.release();
}

void LightningRenderer::Render(void *context,const simul::math::Matrix4x4 &view,const simul::math::Matrix4x4 &proj)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)context;
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	LightningVertex *vertices=vertexBuffer.Map(pContext);
	float time=baseSkyInterface->GetTime();
	for(int i=0;i<cloudKeyframer->GetNumLightningBolts(time);i++)
	{
		const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,i);
		if(!lightningRenderInterface)
			continue;
		if(!lightningRenderInterface->IsSourceStarted(time))
			continue;
		sky::float4 colour=lightningRenderInterface->GetLightningColour();
		dx11::setParameter(effect,"lightningColour",colour);
		simul::sky::float4 x1,x2;
		static float maxwidth=8.f;
					static float ww=50.f;
		simul::math::Vector3 view_dir,cam_pos;
		GetCameraPosVector((const float*)&view);
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		int v=0;
		for(int j=0;j<lightningRenderInterface->GetNumLevels();j++)
		{
			for(int jj=0;jj<lightningRenderInterface->GetNumBranches(j);jj++)
			{
				const simul::clouds::LightningRenderInterface::Branch &branch=lightningRenderInterface->GetBranch(time,j,jj);
				float dist=0.001f*(cam_pos-simul::math::Vector3(branch.vertices[0])).Magnitude();
				/*glLineWidth(maxwidth*branch.width/dist);
				if(enable_geometry_shaders)
					glBegin(GL_LINE_STRIP_ADJACENCY);
				else
					glBegin(GL_LINE_STRIP);*/
				for(int k=-1;k<branch.numVertices;k++)
				{
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
					vertices[v].texCoords=vec4(0,width,x1.w,brightness);
					vertices[v].position=vec4(x1.x,x1.y,x1.z+vertical_shift,x1.w);
					v++;
				}
			}
		}
	}
	vertexBuffer.apply(pContext,0);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
}