#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/Material.h"
#include "Simul/Platform/DirectX11/Mesh.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/Light.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/TextRenderer.h"
#include "Simul/Platform/DirectX11/Buffer.h"
#include "Simul/Platform/DirectX11/Layout.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/CompileShaderDX1x.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Camera/Camera.h"
#include "D3dx11effect.h"

using namespace simul;
using namespace dx11;
namespace simul
{
	namespace dx11
	{
		TextRenderer textRenderer;
	}
}


struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t box_vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};


RenderPlatform::RenderPlatform()
	:device(NULL)
	,m_pDebugEffect(NULL)
	,m_pCubemapVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,solidEffect(NULL)
	,reverseDepth(false)
	,mirrorY(false)
	,m_pDepthStencilStateStored11(NULL)
	,m_pRasterizerStateStored11(NULL)
	,m_pBlendStateStored11(NULL)
	,m_pSamplerStateStored11(NULL)
	,m_StencilRefStored11(0)
	,m_SampleMaskStored11(0)
{
	for(int i=0;i<4;i++)
		m_BlendFactorStored11[i];
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

void RenderPlatform::RestoreDeviceObjects(void *d)
{
	device=(ID3D11Device*)d;
	solidConstants.RestoreDeviceObjects(this);
	textRenderer.RestoreDeviceObjects(device);

	RecompileShaders();
	SAFE_RELEASE(m_pVertexBuffer);
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		crossplatform::EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("vec3_input_signature");
		tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		SAFE_RELEASE(m_pCubemapVtxDecl);
		V_CHECK(AsD3D11Device()->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pCubemapVtxDecl));
	}
	D3D11_BUFFER_DESC desc=
	{
        36*sizeof(vec3),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem		=box_vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(AsD3D11Device()->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));

	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
	solidConstants.InvalidateDeviceObjects();
	textRenderer.InvalidateDeviceObjects();
	SAFE_DELETE(solidEffect);
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		delete mat;
	}
	materials.clear();
	SAFE_RELEASE(m_pCubemapVtxDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_DELETE(m_pDebugEffect);
	
    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
	m_StencilRefStored11=0;
	m_SampleMaskStored11=0;
	for(int i=0;i<4;i++)
		m_BlendFactorStored11[i];
}

void RenderPlatform::RecompileShaders()
{
	SAFE_DELETE(m_pDebugEffect);
	SAFE_DELETE(solidEffect);
	if(!device)
		return;
	std::map<std::string,std::string> defines;
	m_pDebugEffect=CreateEffect("simul_debug",defines);
	if(reverseDepth)
		defines["REVERSE_DEPTH"]="1";
	solidEffect=CreateEffect("solid",defines);
	solidConstants.LinkToEffect(solidEffect,"SolidConstants");
	//solidConstants.LinkToProgram(solid_program,"SolidConstants",1);
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		mat->SetEffect(solidEffect);
	}
	textRenderer.RecompileShaders();
}

void RenderPlatform::PushTexturePath(const char *pathUtf8)
{
	simul::dx11::PushTexturePath(pathUtf8);
}

void RenderPlatform::PopTexturePath()
{
	simul::dx11::PopTexturePath();
}

void RenderPlatform::StartRender()
{
	/*glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_LIGHTING_BIT);
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glUseProgram(solid_program);*/
}

void RenderPlatform::EndRender()
{
	/*glUseProgram(0);
	glPopAttrib();
	glPopAttrib();*/
}
void RenderPlatform::SetReverseDepth(bool r)
{
	reverseDepth=r;
}

namespace
{
    const float DEFAULT_LIGHT_POSITION[]				={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]					={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF				=180.0f;
}

void RenderPlatform::IntializeLightingEnvironment(const float pAmbientLight[3])
{
 /*   glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_DIRECTION_LIGHT_POSITION);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, DEFAULT_LIGHT_COLOR);
    glLightfv(GL_LIGHT0, GL_SPECULAR, DEFAULT_LIGHT_COLOR);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, DEFAULT_LIGHT_SPOT_CUTOFF);
    glEnable(GL_LIGHT0);
    // Set ambient light.
    GLfloat lAmbientLight[] = {static_cast<GLfloat>(pAmbientLight[0]), static_cast<GLfloat>(pAmbientLight[1]),static_cast<GLfloat>(pAmbientLight[2]), 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lAmbientLight);*/
}

void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	deviceContext.asD3D11DeviceContext()->Dispatch(w,l,d);
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	tech->asD3DX11EffectTechnique()->GetPassByIndex(index)->Apply(0,deviceContext.asD3D11DeviceContext());
}

void RenderPlatform::Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->Draw(num_verts,start_vert);
}

void RenderPlatform::DrawMarker(crossplatform::DeviceContext &deviceContext,const double *matrix)
{
 /*   glColor3f(0.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) matrix);

    glBegin(GL_LINE_LOOP);
        glVertex3f(+1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
    glEnd();
    glPopMatrix();*/
}

void RenderPlatform::DrawLine(crossplatform::DeviceContext &deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
/*    glColor3f(colour[0],colour[1],colour[2]);
    glLineWidth(width);

    glBegin(GL_LINES);

    glVertex3dv((const GLdouble *)pGlobalBasePosition);
    glVertex3dv((const GLdouble *)pGlobalEndPosition);

    glEnd();*/
}

void RenderPlatform::DrawCrossHair(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition)
{
/*    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((double*) pGlobalPosition);

    double lCrossHair[6][3] = { { -3, 0, 0 }, { 3, 0, 0 },
    { 0, -3, 0 }, { 0, 3, 0 },
    { 0, 0, -3 }, { 0, 0, 3 } };

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[0]);
    glVertex3dv(lCrossHair[1]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[2]);
    glVertex3dv(lCrossHair[3]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[4]);
    glVertex3dv(lCrossHair[5]);

    glEnd();
	
	glMatrixMode(GL_MODELVIEW);
    glPopMatrix();*/
}

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)
{
 /*   glColor3d(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) pGlobalPosition);
    glRotated(pRoll, 1.0, 0.0, 0.0);

    int i;
    float lCamera[10][2] = {{ 0, 5.5 }, { -3, 4.5 },
    { -3, 7.5 }, { -6, 10.5 }, { -23, 10.5 },
    { -23, -4.5 }, { -20, -7.5 }, { -3, -7.5 },
    { -3, -4.5 }, { 0, -5.5 }   };

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
    }
    glEnd();

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
        }
    }
    glEnd();

    for (i = 0; i < 10; i++)
    {
        glBegin( GL_LINES );
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
        glEnd();
    }
    glPopMatrix();*/
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &deviceContext,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
/*    glPushMatrix();
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    glPopMatrix();*/
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const float BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
/*    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);*/
}

void MakeWorldViewProjMatrix(float *wvp,const double *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,view(v),proj(p),model(w);
	simul::math::Multiply4x4(tmp1,model,view);
	simul::math::Multiply4x4(*(simul::math::Matrix4x4*)wvp,tmp1,proj);
}

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *m,const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	//math::Vector3 dirToLight;
	//simul::math::Multiply3(dirToLight,(const float*)&physicalLightRenderData.dirToLight,model);
	solidConstants.worldViewProj=modelviewproj;
	solidConstants.world=model;
	
	solidConstants.lightIrradiance	=physicalLightRenderData.lightColour;
	solidConstants.lightDir			=physicalLightRenderData.dirToLight;
	solidConstants.Apply(deviceContext);
	solidConstants.Apply(deviceContext);
	
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	solidEffect->asD3DX11Effect()->GetTechniqueByName("solid")->GetPassByIndex(0)->Apply(0,pContext);
}

crossplatform::Material *RenderPlatform::CreateMaterial()
{
	dx11::Material *mat=new dx11::Material;
	mat->SetEffect(solidEffect);
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new dx11::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new dx11::Light();
}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	crossplatform::Texture * tex=new dx11::Texture(device);
	if(fileNameUtf8)
		tex->LoadFromFile(this,fileNameUtf8);
	return tex;
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	std::string fn(filename_utf8);
	if(fn.find(".")>=fn.length())
		fn+=".fx";
	crossplatform::Effect *e= new dx11::Effect(this,fn.c_str(),defines);
	e->SetName(filename_utf8);
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx11::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx11::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx11::Buffer();
	return b;
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RG_32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case R_32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RG_32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	};
}

crossplatform::PixelFormat RenderPlatform::FromDxgiFormat(DXGI_FORMAT f)
{
	using namespace crossplatform;
	switch(f)
	{
	case DXGI_FORMAT_R16_FLOAT:
		return R_16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return RGBA_16_FLOAT;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return RGBA_32_FLOAT;
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return RGB_32_FLOAT;
	case DXGI_FORMAT_R32G32_FLOAT:
		return RG_32_FLOAT;
	case DXGI_FORMAT_R32_FLOAT:
		return R_32_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
	case DXGI_FORMAT_R32_UINT:
		return R_32_UINT;
	case DXGI_FORMAT_R32G32_UINT:
		return RG_32_UINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return RGB_32_UINT;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return RGBA_32_UINT;
	default:
		return UNKNOWN;
	};
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,crossplatform::LayoutDesc *desc,crossplatform::Buffer *buffer)
{
	D3D11_INPUT_ELEMENT_DESC *decl=new D3D11_INPUT_ELEMENT_DESC[num_elements];
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
		D3D11_INPUT_ELEMENT_DESC &D=decl[i];
		D.SemanticName=d.semanticName;
		D.SemanticIndex=d.semanticIndex;
		D.Format=ToDxgiFormat(d.format);
		D.AlignedByteOffset=d.alignedByteOffset;
		D.InputSlot=d.inputSlot;
		D.InputSlotClass=d.perInstance?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
		D.InstanceDataStepRate=d.instanceDataStepRate;
	};
	dx11::Layout *l=new dx11::Layout();
	//hr=m_pd3dDevice->CreateInputLayout(decl,2, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &starsLayout);
	
	ID3DBlob *VS;
	ID3DBlob *errorMsgs=NULL;
	std::string dummy_shader;
	dummy_shader="struct vertexInput"
				"{";
	for(int i=0;i<num_elements;i++)
	{
		D3D11_INPUT_ELEMENT_DESC &dec=decl[i];
		std::string format;
		switch(dec.Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			format="float4";
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			format="float3";
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
			format="float2";
			break;
		case DXGI_FORMAT_R32_FLOAT:
			format="float";
			break;
		};
		dummy_shader+="   ";
		dummy_shader+=format+" ";
		std::string name=dec.SemanticName;
		if(strcmp(dec.SemanticName,"POSITION")!=0)
			name+=('0'+dec.SemanticIndex);
		dummy_shader+=name;
		dummy_shader+="_";
		dummy_shader+=" : ";
		dummy_shader+=name;
		dummy_shader+=";";
				//"	float3 position		: POSITION;"
				//"	float texCoords		: TEXCOORD0;";
	}
	dummy_shader+="};"
				"struct vertexOutput"
				"{"
				"	float4 hPosition	: SV_POSITION;"
				"};"
				"vertexOutput VS_Main(vertexInput IN) "
				"{"
				"	vertexOutput OUT;"
				"	OUT.hPosition	=float4(1.0,1.0,1.0,1.0);"
				"	return OUT;"
				"}";
	const char *str=dummy_shader.c_str();
	size_t len=strlen(str);
#if WINVER<0x602
	HRESULT hr=D3DX11CompileFromMemory(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, 0, &VS, &errorMsgs, 0);
#else
	HRESULT hr=D3DCompile(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, &VS, &errorMsgs);
#endif
	if(hr!=S_OK)
	{
		const char *e=(const char*)errorMsgs->GetBufferPointer();
		std::cerr<<e<<std::endl;
	}
	AsD3D11Device()->CreateInputLayout(decl, num_elements, VS->GetBufferPointer(), VS->GetBufferSize(), &l->d3d11InputLayout);
	
	if(VS)
		VS->Release();
	if(errorMsgs)
		errorMsgs->Release();
	delete [] decl;
	return l;
}

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer **buffers)
{
	UINT offset = 0;
	ID3D11Buffer *buf[10];
	UINT strides[10];
	UINT offsets[10];
	for(int i=0;i<num_buffers;i++)
	{
		strides[i]=buffers[i]->stride;
		buf[i]=buffers[i]->AsD3D11Buffer();
		offsets[i]=0;
	}
	deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(	0,				// the first input slot for binding
									num_buffers,	// the number of buffers in the array
									buf,			// the array of vertex buffers
									strides,			// array of stride values, one for each buffer
									offsets );		// array of offset values, one for each buffer

};

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	ID3D11RenderTargetView *rt[4];
	SIMUL_ASSERT(num<=4);
	for(int i=0;i<num;i++)
		rt[i]=targs[i]->AsD3D11RenderTargetView();
	ID3D11DepthStencilView *d=NULL;
	if(depth)
		d=depth->AsD3D11DepthStencilView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(num,rt,d);
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Viewport *vps)
{
	D3D11_VIEWPORT viewports[4];
	SIMUL_ASSERT(num<=4);
	for(int i=0;i<num;i++)
	{
		viewports[i].Width		=(float)vps[i].w;
		viewports[i].Height		=(float)vps[i].h;
		viewports[i].TopLeftX	=(float)vps[i].x;
		viewports[i].TopLeftY	=(float)vps[i].y;
		viewports[i].MinDepth	=vps[i].znear;
		viewports[i].MaxDepth	=vps[i].zfar;
	}
	deviceContext.asD3D11DeviceContext()->RSSetViewports(num,viewports);
}
void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	if(!buffer)
	{
		deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
		return;
	}
	DXGI_FORMAT f;
	int index_byte_size=buffer->stride;
	switch(index_byte_size)
	{
	case 4:
		f=DXGI_FORMAT_R32_UINT;
		break;
	case 2:
		f=DXGI_FORMAT_R16_UINT;
		break;
	default:
		f=DXGI_FORMAT_UNKNOWN;
		SIMUL_BREAK("Can't use DXGI_FORMAT_UNKNOWN for an index buffer.")
		break;
	};
	deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(buffer->AsD3D11Buffer(), f, 0);
}

void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
    pContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
	SetDebugObjectName(m_pDepthStencilStateStored11,"m_pDepthStencilStateStored11");
    pContext->RSGetState(&m_pRasterizerStateStored11 );
	SetDebugObjectName(m_pRasterizerStateStored11,"m_pRasterizerStateStored11");
    pContext->OMGetBlendState(&m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
	SetDebugObjectName(m_pBlendStateStored11,"m_pBlendStateStored11");
    pContext->PSGetSamplers(0, 1, &m_pSamplerStateStored11 );
	SetDebugObjectName(m_pSamplerStateStored11,"m_pSamplerStateStored11");
}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
    pContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pContext->RSSetState(m_pRasterizerStateStored11 );
    pContext->OMSetBlendState(m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pContext->PSSetSamplers(0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *srv,float mult)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTexture",srv);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"multiplier",mult);
	crossplatform::EffectTechnique *tech=m_pDebugEffect->GetTechniqueByName("textured");
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	if(srv)
	{
		srv->GetDesc(&desc);
		bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
		if(msaa)
		{
			tech=m_pDebugEffect->GetTechniqueByName("textured");
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureMS",srv);
		}
		else if(desc.Format==DXGI_FORMAT_R32_UINT||desc.Format==DXGI_FORMAT_R32G32_UINT||desc.Format==DXGI_FORMAT_R32G32B32_UINT||desc.Format==DXGI_FORMAT_R32G32B32A32_UINT)
		{
			tech=m_pDebugEffect->GetTechniqueByName("compacted_texture");
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint2",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint3",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint4",srv);
		}
	}
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
		y1=(int)viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,m_pDebugEffect->asD3DX11Effect(),tech->asD3DX11EffectTechnique());
	}
	simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTexture",NULL);
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult)
{
	DrawTexture(deviceContext,x1,y1,dx,dy,tex->AsD3D11ShaderResourceView(),mult);
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex)
{
	crossplatform::EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("show_depth");
	if(tex->GetSampleCount()>0)
	{
		tech=m_pDebugEffect->GetTechniqueByName("show_depth_ms");
		m_pDebugEffect->SetTexture(deviceContext,"imageTextureMS",tex);
	}
	else
	{
		m_pDebugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	m_pDebugEffect->SetParameter("tanHalfFov",vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov));
	static float cc=300000.f;
	vec4 depthToLinFadeDistParams=camera::GetDepthToDistanceParameters(deviceContext.viewStruct,cc);//(deviceContext.viewStruct.proj[3*4+2],cc,deviceContext.viewStruct.proj[2*4+2]*cc);
	m_pDebugEffect->SetParameter("depthToLinFadeDistParams",depthToLinFadeDistParams);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
	{
		y1=(int)viewport.Height-y1;
		dy*=-1;
	}
	{
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,m_pDebugEffect->asD3DX11Effect(),tech->asD3DX11EffectTechnique());
	}
}

void RenderPlatform::DrawQuad		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	ID3DX11Effect			*eff		=effect->asD3DX11Effect();
	ID3DX11EffectTechnique	*tech		=technique->asD3DX11EffectTechnique();
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
		y1=(int)viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,eff,tech);
	}
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(NULL);
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::Print(crossplatform::DeviceContext &deviceContext,int x,int y	,const char *text)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	float clr[]={1.f,1.f,0.f,1.f};
	float black[]={0.f,0.f,0.f,0.5f};
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	int h=(int)viewport.Height;
	int pos=0;
	
	while(*text!=0)
	{
		textRenderer.Render(deviceContext,(float)x,(float)y,(float)viewport.Width,(float)h,text,clr,black,mirrorY);
		while(*text!='\n'&&*text!=0)
		{
			text++;
			pos++;
		}
		if(!(*text))
			break;
		text++;
		pos++;
		y+=16;
	}
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,Vertext *line_vertices,int vertex_count,bool strip,bool test_depth)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		crossplatform::EffectTechniqueGroup *g=m_pDebugEffect->GetTechniqueGroupByName(test_depth?"lines_3d_depth":"lines_3d");
		camera::Frustum f=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
			crossplatform::EffectTechnique *tech=g->GetTechniqueByIndex(0);
		if(test_depth)
			tech=g->GetTechniqueByName(f.reverseDepth?"depth_reverse":"depth_forward");
		D3DXMATRIX wvp;
		camera::MakeWorldViewProjMatrix((float*)&wvp,(const float*)&world,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		m_pDebugEffect->SetMatrix("worldViewProj",&wvp._11);
	
		ID3D11Buffer *					vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(Vertext),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = line_vertices;
		InitData.SysMemPitch = sizeof(Vertext);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->asD3DX11EffectTechnique()->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		pContext->IASetInputLayout(m_pVtxDecl);
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(Vertext);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,							// the first input slot for binding
													1,				// the number of buffers in the array
													&vertexBuffer,	// the array of vertex buffers
													&stride,		// array of stride values, one for each buffer
													&offset);		// array of 
		hr=ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		ID3DX11EffectTechnique *tech			=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("lines_2d");
		
		unsigned int num_v=1;
		D3D11_VIEWPORT								viewport;
		pContext->RSGetViewports(&num_v,&viewport);
		dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"rect",vec4(-1.0,-1.0,2.0f/viewport.Width,2.0f/viewport.Height));

		ID3D11Buffer *vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(VertexXyzRgba),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = lines;
		InitData.SysMemPitch = sizeof(VertexXyzRgba);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		pContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,				// the first input slot for binding
										1,				// the number of buffers in the array
										&vertexBuffer,	// the array of vertex buffers
										&stride,		// array of stride values, one for each buffer
										&offset);		// array of 
		hr=ApplyPass(pContext,tech->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void RenderPlatform::DrawCircle(crossplatform::DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill)
{
	ID3D11DeviceContext *pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(fill?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	pContext->IASetInputLayout(NULL);
	{
		ID3DX11EffectGroup *g=m_pDebugEffect->asD3DX11Effect()->GetGroupByName("circle");
		ID3DX11EffectTechnique *tech=fill?g->GetTechniqueByName("filled"):g->GetTechniqueByName("outline");
		
		simul::math::Vector3 d(dir);
		d.Normalize();
		float Yaw=atan2(d.x,d.y);
		float Pitch=-asin(d.z);
		simul::math::Matrix4x4 world, tmp1, tmp2;
	
		simul::geometry::SimulOrientation ori;
		ori.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		ori.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world	=ori.T4;
		//set up matrices
		math::Matrix4x4 view=deviceContext.viewStruct.view;
		view._41=0.f;
		view._42=0.f;
		view._43=0.f;
		simul::math::Multiply4x4(tmp1,world,view);
		simul::math::Multiply4x4(tmp2,tmp1,deviceContext.viewStruct.proj);
		camera::MakeWorldViewProjMatrix(tmp2,world,view,deviceContext.viewStruct.proj);
		m_pDebugEffect->SetMatrix("worldViewProj",&tmp2._11);
		m_pDebugEffect->SetParameter("radius",rads);
		m_pDebugEffect->SetVector("colour",colr);
		ApplyPass(pContext,tech->GetPassByIndex(0));
	}
	pContext->Draw(fill?64:32,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::PrintAt3dPos(crossplatform::DeviceContext &deviceContext,const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
	//renderPlatform->PrintAt3dPos((ID3D11DeviceContext *)context,p,text,colr,offsetx,offsety);
}

void RenderPlatform::DrawCube(crossplatform::DeviceContext &deviceContext)
{
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(m_pCubemapVtxDecl);

	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void RenderPlatform::DrawCubemap(crossplatform::DeviceContext &deviceContext,ID3D11ShaderResourceView *m_pCubeEnvMapSRV,float offsetx,float offsety)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	unsigned int num_v=0;
	D3D11_VIEWPORT								m_OldViewports[4];
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width		=m_OldViewports[0].Width*0.3f;
	viewport.Height		=m_OldViewports[0].Height*0.3f;
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	viewport.TopLeftX	=0.5f*(1.f+offsetx)*m_OldViewports[0].Width-viewport.Width/2;
	viewport.TopLeftY	=0.5f*(1.f-offsety)*m_OldViewports[0].Height-viewport.Height/2;
	pContext->RSSetViewports(1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=camera::Camera::MakeProjectionMatrix(1.f,(float)viewport.Height/(float)viewport.Width,1.f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.Identity();
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float size=3.f;
	float d=2.0f*size/size_req;
	simul::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	simul::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._41=offs.x;
	world._42=offs.y;
	world._43=offs.z;
	camera::MakeWorldViewProjMatrix(wvp,world,view,proj);
	simul::dx11::setMatrix(m_pDebugEffect->asD3DX11Effect(),"worldViewProj",&wvp._11);
	//ID3DX11EffectTechnique*			tech		=m_pDebugEffect->GetTechniqueByName("draw_cubemap");
	ID3DX11EffectTechnique*				tech		=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("draw_cubemap_sphere");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pDebugEffect->asD3DX11Effect()->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(m_pCubeEnvMapSRV);
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"latitudes",16);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"longitudes",32);
	static float rr=6.f;
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"radius",rr);
	UtilityRenderer::DrawSphere(deviceContext,16,32);
	pContext->RSSetViewports(num_v,m_OldViewports);
}
