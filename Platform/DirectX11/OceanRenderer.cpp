#define NOMINMAX
#include "OceanRenderer.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include <D3DX11tex.h>
#include "CompileShaderDX1x.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#pragma warning(disable:4995)
#include <vector>
#include <string>
#include <map>
#include <assert.h>
using namespace std;
using namespace simul;
using namespace dx11;

#define FRESNEL_TEX_SIZE			256
#define PERLIN_TEX_SIZE				64
#define SAFE_DELETE_ARRAY(c) delete [] c;c=NULL;

struct ocean_vertex
{
	float index_x;
	float index_y;
};

// Mesh properties:

// Shading properties:
// Two colors for waterbody and sky color
D3DXVECTOR3 g_SkyColor= D3DXVECTOR3(0.38f, 0.45f, 0.56f);
D3DXVECTOR3 g_WaterbodyColor = D3DXVECTOR3(0.12f, 0.14f, 0.17f);
// Blending term for sky cubemap
float g_SkyBlending = 16.0f;

// Perlin wave parameters
float g_PerlinSize = 1.0f;
float g_PerlinSpeed = 0.06f;
D3DXVECTOR3 g_PerlinAmplitude = D3DXVECTOR3(35, 42, 57);
D3DXVECTOR3 g_PerlinGradient = D3DXVECTOR3(1.4f, 1.6f, 2.2f);
D3DXVECTOR3 g_PerlinOctave = D3DXVECTOR3(1.12f, 0.59f, 0.23f);

D3DXVECTOR3 g_BendParam = D3DXVECTOR3(0.1f, -0.4f, 0.2f);

// Sunspot parameters
D3DXVECTOR3 g_SunDir = D3DXVECTOR3(0.936016f, -0.343206f, 0.0780013f);
D3DXVECTOR3 g_SunColor = D3DXVECTOR3(1.0f, 1.0f, 0.6f);
float g_Shineness = 400.0f;


// Constant buffer
struct Const_Per_Call
{
	D3DXMATRIX	g_matLocal;
	D3DXMATRIX	g_matWorldViewProj;
	D3DXMATRIX	g_matWorld;
	D3DXVECTOR2 g_UVBase;
	D3DXVECTOR2 g_PerlinMovement;
	D3DXVECTOR3	g_LocalEye;
	// Atmospherics
	float		hazeEccentricity;
	D3DXVECTOR3	lightDir;
	D3DXVECTOR4 mieRayleighRatio;
};

struct Const_Shading
{
	// The color of bottomless water body
	D3DXVECTOR3		g_WaterbodyColor;
	// The strength, direction and color of sun streak
	float			g_Shineness;
	D3DXVECTOR3		g_SunDir;
	float			unused1;
	D3DXVECTOR3		g_SunColor;
	float			unused2;
	// The parameter is used for fixing an artifact
	D3DXVECTOR3		g_BendParam;
	// Perlin noise for distant wave crest
	float			g_PerlinSize;
	D3DXVECTOR3		g_PerlinAmplitude;
	float			unused3;
	D3DXVECTOR3		g_PerlinOctave;
	float			unused4;
	D3DXVECTOR3		g_PerlinGradient;
	// Constants for calculating texcoord from position
	float			g_TexelLength_x2;
	float			g_UVScale;
	float			g_UVOffset;
};


OceanRenderer::OceanRenderer(simul::terrain::SeaKeyframer *s)
	:simul::terrain::BaseSeaRenderer(s)
	,oceanSimulator(NULL)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,g_pPerCallCB(NULL)
	,g_pPerFrameCB(NULL)
	,g_pShadingCB(NULL)
	// D3D11 buffers and layout
	,g_pMeshVB(NULL)
	,g_pMeshIB(NULL)
	,g_pMeshLayout(NULL)
	// Color look up 1D texture
	,g_pFresnelMap(NULL)
	,g_pSRV_Fresnel(NULL)
	// Distant perlin wave
	,g_pSRV_Perlin(NULL)
	// Environment maps
	,g_pSRV_ReflectCube(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
{
}

OceanRenderer::~OceanRenderer()
{
	InvalidateDeviceObjects();
}


void OceanRenderer::Update(float dt)
{
	// Update simulation
	app_time += (double)dt;
}

void OceanRenderer::RecompileShaders()
{
	SAFE_RELEASE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	if(ReverseDepth)
		defines["REVERSE_DEPTH"]="1";
	defines["FX"]="1";
	effect=LoadEffect(m_pd3dDevice,"ocean.fx",defines);

	// Input layout
	D3D11_INPUT_ELEMENT_DESC mesh_layout_desc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	
	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("ocean");
	if(tech)
	{
		D3DX11_PASS_DESC PassDesc;
		tech->GetPassByIndex(0)->GetDesc(&PassDesc);
		m_pd3dDevice->CreateInputLayout(mesh_layout_desc,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pMeshLayout);
	}
}

void OceanRenderer::RestoreDeviceObjects(ID3D11Device* dev)
{
	InvalidateDeviceObjects();
	m_pd3dDevice=dev;
	oceanSimulator = new OceanSimulator(ocean_parameters);
	oceanSimulator->RestoreDeviceObjects(m_pd3dDevice);
	// Update the simulation for the first time.
	oceanSimulator->updateDisplacementMap(0);

	// D3D buffers
	createSurfaceMesh();
	createFresnelMap();
	loadTextures();

	RecompileShaders();

	// Constants
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = PAD16(sizeof(Const_Per_Call));
	cb_desc.StructureByteStride = 0;
	m_pd3dDevice->CreateBuffer(&cb_desc, NULL, &g_pPerCallCB);
	assert(g_pPerCallCB);

	Const_Shading shading_data;
	// Grid side length * 2
	shading_data.g_TexelLength_x2 = ocean_parameters->patch_length / ocean_parameters->dmap_dim * 2;;
	// Color
	shading_data.g_WaterbodyColor = g_WaterbodyColor;
	// Texcoord
	shading_data.g_UVScale = 1.0f / ocean_parameters->patch_length;
	shading_data.g_UVOffset = 0.5f / ocean_parameters->dmap_dim;
	// Perlin
	shading_data.g_PerlinSize = g_PerlinSize;
	shading_data.g_PerlinAmplitude = g_PerlinAmplitude;
	shading_data.g_PerlinGradient = g_PerlinGradient;
	shading_data.g_PerlinOctave = g_PerlinOctave;
	// Multiple reflection workaround
	shading_data.g_BendParam = g_BendParam;
	// Sun streaks
	shading_data.g_SunColor = g_SunColor;
	shading_data.g_SunDir = g_SunDir;
	shading_data.g_Shineness = g_Shineness;

	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem = &shading_data;
	cb_init_data.SysMemPitch = 0;
	cb_init_data.SysMemSlicePitch = 0;

	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(Const_Shading));
	cb_desc.StructureByteStride = 0;
	m_pd3dDevice->CreateBuffer(&cb_desc, &cb_init_data, &g_pShadingCB);
	assert(g_pShadingCB);

	D3D11_BLEND_DESC blend_desc;
	memset(&blend_desc, 0, sizeof(D3D11_BLEND_DESC));
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	
	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
}

void OceanRenderer::SetCubemapTexture(void *c)
{
	g_pSRV_ReflectCube=(ID3D1xShaderResourceView*)c;
}

void OceanRenderer::SetLossTexture(void *t)
{
	skyLossTexture_SRV=((ID3D1xShaderResourceView*)t);
}

void OceanRenderer::SetInscatterTextures(void *t,void *s)
{
	skyInscatterTexture_SRV=((ID3D1xShaderResourceView*)t);
	skylightTexture_SRV=((ID3D1xShaderResourceView*)s);
}

void OceanRenderer::InvalidateDeviceObjects()
{
	if(oceanSimulator)
	{
		oceanSimulator->InvalidateDeviceObjects();
    // Ocean object
		delete (oceanSimulator);
		oceanSimulator=NULL;
	}
	SAFE_RELEASE(g_pMeshIB);
	SAFE_RELEASE(g_pMeshVB);
	SAFE_RELEASE(g_pMeshLayout);
	
	SAFE_RELEASE(effect);

	SAFE_RELEASE(g_pFresnelMap);
	SAFE_RELEASE(g_pSRV_Fresnel);
	SAFE_RELEASE(g_pSRV_Perlin);

	SAFE_RELEASE(g_pPerCallCB);
	SAFE_RELEASE(g_pPerFrameCB);
	SAFE_RELEASE(g_pShadingCB);

	g_render_list.clear();
}

#define MESH_INDEX_2D(x, y)	(((y) + vert_rect.bottom) * (g_MeshDim + 1) + (x) + vert_rect.left)


void OceanRenderer::createSurfaceMesh()
{
	// --------------------------------- Vertex Buffer -------------------------------
	int num_verts = (g_MeshDim + 1) * (g_MeshDim + 1);
	ocean_vertex* pV = new ocean_vertex[num_verts];
	assert(pV);

	int i, j;
	for (i = 0; i <= g_MeshDim; i++)
	{
		for (j = 0; j <= g_MeshDim; j++)
		{
			pV[i * (g_MeshDim + 1) + j].index_x = (float)j;
			pV[i * (g_MeshDim + 1) + j].index_y = (float)i;
		}
	}

	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = num_verts * sizeof(ocean_vertex);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;
	vb_desc.StructureByteStride = sizeof(ocean_vertex);

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = pV;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;
	
	SAFE_RELEASE(g_pMeshVB);
	m_pd3dDevice->CreateBuffer(&vb_desc, &init_data, &g_pMeshVB);

	SAFE_DELETE_ARRAY(pV);

	// --------------------------------- Index Buffer -------------------------------
	// The index numbers for all mesh LODs (up to 256x256)
	const int index_size_lookup[] = {0, 0, 4284, 18828, 69444, 254412, 956916, 3689820, 14464836};

	memset(&g_mesh_patterns[0][0][0][0][0], 0, sizeof(g_mesh_patterns));

	g_Lods = 0;
	for (i = g_MeshDim; i > 1; i >>= 1)
		g_Lods ++;

	// Generate patch meshes. Each patch contains two parts: the inner mesh which is a regular
	// grids in a triangle strip. The boundary mesh is constructed w.r.t. the edge degrees to
	// meet water-tight requirement.
	DWORD* index_array = new DWORD[index_size_lookup[g_Lods]];
	assert(index_array);
	EnumeratePatterns(index_array);
	D3D11_BUFFER_DESC ib_desc;
	ib_desc.ByteWidth = index_size_lookup[g_Lods] * sizeof(DWORD);
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags = 0;
	ib_desc.MiscFlags = 0;
	ib_desc.StructureByteStride = sizeof(DWORD);

	init_data.pSysMem = index_array;

	SAFE_RELEASE(g_pMeshIB);
	m_pd3dDevice->CreateBuffer(&ib_desc, &init_data, &g_pMeshIB);

	SAFE_DELETE_ARRAY(index_array);
}

void OceanRenderer::EnumeratePatterns(unsigned long* index_array)
{
	int offset = 0;
	int level_size = g_MeshDim;
	// Enumerate patterns
	for (int level = 0; level <= g_Lods - 2; level ++)
	{
		int left_degree = level_size;
		for (int left_type = 0; left_type < 3; left_type ++)
		{
			int right_degree = level_size;
			for (int right_type = 0; right_type < 3; right_type ++)
			{
				int bottom_degree = level_size;
				for (int bottom_type = 0; bottom_type < 3; bottom_type ++)
				{
					int top_degree = level_size;
					for (int top_type = 0; top_type < 3; top_type ++)
					{
						QuadRenderParam* pattern = &g_mesh_patterns[level][left_type][right_type][bottom_type][top_type];
						// Inner mesh (triangle strip)
						Rect inner_rect;
						inner_rect.left   = (left_degree   == level_size) ? 0 : 1;
						inner_rect.right  = (right_degree  == level_size) ? level_size : level_size - 1;
						inner_rect.bottom = (bottom_degree == level_size) ? 0 : 1;
						inner_rect.top    = (top_degree    == level_size) ? level_size : level_size - 1;
						int num_new_indices = generateInnerMesh(inner_rect, index_array + offset);
						pattern->inner_start_index = offset;
						pattern->num_inner_verts = (level_size + 1) * (level_size + 1);
						pattern->num_inner_faces = num_new_indices - 2;
						offset += num_new_indices;
						// Boundary mesh (triangle list)
						int l_degree = (left_degree   == level_size) ? 0 : left_degree;
						int r_degree = (right_degree  == level_size) ? 0 : right_degree;
						int b_degree = (bottom_degree == level_size) ? 0 : bottom_degree;
						int t_degree = (top_degree    == level_size) ? 0 : top_degree;
						Rect outer_rect = {0, level_size, level_size, 0};
						num_new_indices = generateBoundaryMesh(l_degree, r_degree, b_degree, t_degree, outer_rect, index_array + offset);
						pattern->boundary_start_index = offset;
						pattern->num_boundary_verts = (level_size + 1) * (level_size + 1);
						pattern->num_boundary_faces = num_new_indices / 3;
						offset += num_new_indices;

						top_degree /= 2;
					}
					bottom_degree /= 2;
				}
				right_degree /= 2;
			}
			left_degree /= 2;
		}
		level_size /= 2;
	}
//	assert(offset == index_size_lookup[g_Lods]);
}

void OceanRenderer::createFresnelMap()
{
	DWORD* buffer = new DWORD[FRESNEL_TEX_SIZE];
	for (int i = 0; i < FRESNEL_TEX_SIZE; i++)
	{
		float cos_a = i / (FLOAT)FRESNEL_TEX_SIZE;
		// Using water's refraction index 1.33
		DWORD fresnel = (DWORD)(D3DXFresnelTerm(cos_a, 1.33f) * 255);
		DWORD sky_blend = (DWORD)(powf(1 / (1 + cos_a), g_SkyBlending) * 255);
		buffer[i] = (sky_blend << 8) | fresnel;
	}
	D3D11_TEXTURE1D_DESC tex_desc;
	tex_desc.Width = FRESNEL_TEX_SIZE;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = buffer;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	m_pd3dDevice->CreateTexture1D(&tex_desc, &init_data, &g_pFresnelMap);
	assert(g_pFresnelMap);

	SAFE_DELETE_ARRAY(buffer);

	// Create shader resource
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	srv_desc.Texture1D.MipLevels = 1;
	srv_desc.Texture1D.MostDetailedMip = 0;

	m_pd3dDevice->CreateShaderResourceView(g_pFresnelMap, &srv_desc, &g_pSRV_Fresnel);
	assert(g_pSRV_Fresnel);
}

void OceanRenderer::loadTextures()
{
    WCHAR strPath[MAX_PATH];
    swprintf_s(strPath, MAX_PATH, L"../../Platform/DirectX11/Textures/perlin_noise.dds");
	D3DX11CreateShaderResourceViewFromFile(m_pd3dDevice, strPath, NULL, NULL, &g_pSRV_Perlin, NULL);
	assert(g_pSRV_Perlin);
}


static D3DXVECTOR3 GetCameraPosVector(D3DXMATRIX &view)
{
	D3DXMATRIX tmp1;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	D3DXVECTOR3 cam_pos;
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	return cam_pos;
}

void OceanRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
//	if(IsYVertical())
//		view=D3DXMATRIX(1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1) * v;
	proj=p;
}

void OceanRenderer::Render(void *context,float exposure)
{
	if(skyInterface)
		app_time=skyInterface->GetTime();
	oceanSimulator->updateDisplacementMap((float)app_time);
	ID3D11DeviceContext*	pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* displacement_map = oceanSimulator->getDisplacementMap();
	ID3D11ShaderResourceView* gradient_map = oceanSimulator->getGradientMap();

	// Build rendering list
	g_render_list.clear();
	float ocean_extent = ocean_parameters->patch_length * (1 << g_FurthestCover);
	QuadNode root_node = {D3DXVECTOR2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node,ocean_parameters->patch_length,view,proj);

	// Matrices
	D3DXMATRIX matView = view;
	D3DXMATRIX matProj = proj;

	// VS & PS
	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("ocean");
	tech->GetPassByIndex(0)->Apply(0,pContext);

	// Textures
	setParameter(effect,"g_texDisplacement"		,displacement_map);
	setParameter(effect,"g_texPerlin"			,g_pSRV_Perlin);
	setParameter(effect,"g_texGradient"			,gradient_map);
	setParameter(effect,"g_texFresnel"			,g_pSRV_Fresnel);
	setParameter(effect,"g_texReflectCube"		,g_pSRV_ReflectCube);
	setParameter(effect,"g_skyLossTexture"		,skyLossTexture_SRV);
	setParameter(effect,"g_skyInscatterTexture"	,skyInscatterTexture_SRV);

	// IA setup
	pContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1] = {g_pMeshVB};
	UINT strides[1] = {sizeof(ocean_vertex)};
	UINT offsets[1] = {0};
	pContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);
	pContext->IASetInputLayout(g_pMeshLayout);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	setConstantBuffer(effect,"cbShading"	,g_pShadingCB);

	// We assume the center of the ocean surface at (0, 0, 0).
	for (int i = 0; i < (int)g_render_list.size(); i++)
	{
		QuadNode& node = g_render_list[i];
		if (!node.isLeaf())
			continue;
		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node,ocean_parameters->patch_length);
		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;
		// Matrices and constants
		Const_Per_Call call_consts;
		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);
		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMatrixTranspose(&call_consts.g_matWorld, &matWorld);
		D3DXMATRIX matWVP = matWorld * matView * matProj;
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);
		// Texcoord for perlin noise
		D3DXVECTOR2 uv_base = node.bottom_left / ocean_parameters->patch_length * g_PerlinSize;
		call_consts.g_UVBase = uv_base;
		// Constant g_PerlinSpeed need to be adjusted mannually
		D3DXVECTOR2 perlin_move =D3DXVECTOR2(ocean_parameters->wind_dir)*(-(float)app_time)* g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;
		// Eye point
		D3DXMATRIX matInvWV = matWorld * matView;
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye = vLocalEye;

		// Atmospherics
		if(skyInterface)
		{
			call_consts.hazeEccentricity=skyInterface->GetMieEccentricity();
			call_consts.lightDir=D3DXVECTOR3((const float *)(skyInterface->GetDirectionToLight(0.f)));
			call_consts.mieRayleighRatio=D3DXVECTOR4((const float *)(skyInterface->GetMieRayleighRatio()));
		}
		else
		{
			call_consts.hazeEccentricity=0.85f;
			call_consts.lightDir=D3DXVECTOR3(0,0,1.f);
			call_consts.mieRayleighRatio=D3DXVECTOR4(0,0,0,0);
		}

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pContext->Unmap(g_pPerCallCB, 0);

		cbs[0] = g_pPerCallCB;
		//pContext->VSSetConstantBuffers(4, 1, cbs);
		//pContext->PSSetConstantBuffers(4, 1, cbs);
		setConstantBuffer(effect,"cbChangePerCall"	,g_pPerCallCB);
		tech->GetPassByIndex(0)->Apply(0,pContext);
		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}

	// Unbind
	unbindTextures(effect);

	tech->GetPassByIndex(0)->Apply(0,pContext);
}

void OceanRenderer::RenderWireframe(void *context)
{
	ID3D11DeviceContext*	pContext=(ID3D11DeviceContext*)context;
	ID3D11ShaderResourceView* displacement_map = oceanSimulator->getDisplacementMap();
	// Build rendering list
	g_render_list.clear();
	float ocean_extent = ocean_parameters->patch_length * (1 << g_FurthestCover);
	QuadNode root_node = {D3DXVECTOR2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node,ocean_parameters->patch_length,view,proj);

	// Matrices
	D3DXMATRIX matView = view;
	D3DXMATRIX matProj = proj;

	// VS & PS
	ID3DX11EffectTechnique *tech=effect->GetTechniqueByName("wireframe");
	
	setParameter(effect,"g_texDisplacement",displacement_map);
	setParameter(effect,"g_texPerlin",g_pSRV_Perlin);

	// IA setup
	pContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1] = {g_pMeshVB};
	UINT strides[1] = {sizeof(ocean_vertex)};
	UINT offsets[1] = {0};
	pContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	pContext->IASetInputLayout(g_pMeshLayout);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	setConstantBuffer(effect,"cbShading",g_pShadingCB);

	// We assume the center of the ocean surface is at (0, 0, 0).
	for (int i = 0; i < (int)g_render_list.size(); i++)
	{
		QuadNode& node = g_render_list[i];
		
		if (!node.isLeaf())
			continue;

		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node,ocean_parameters->patch_length);

		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;

		// Matrices and constants
		Const_Per_Call call_consts;

		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);

		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMatrixTranspose(&call_consts.g_matWorld, &matWorld);
		D3DXMATRIX matWVP = matWorld * matView * matProj;
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);

		// Texcoord for perlin noise
		D3DXVECTOR2 uv_base = node.bottom_left / ocean_parameters->patch_length * g_PerlinSize;
		call_consts.g_UVBase = uv_base;

		// Constant g_PerlinSpeed need to be adjusted mannually
		D3DXVECTOR2 perlin_move =D3DXVECTOR2(ocean_parameters->wind_dir) *(-(float)app_time)* g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;

		// Eye point
		D3DXMATRIX matInvWV = matWorld * matView;
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye = vLocalEye;

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pContext->Unmap(g_pPerCallCB, 0);

		cbs[0] = g_pPerCallCB;
		setConstantBuffer(effect,"cbChangePerCall",g_pPerCallCB);
		tech->GetPassByIndex(0)->Apply(0,pContext);
		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}
	unbindTextures(effect);
	tech->GetPassByIndex(0)->Apply(0,pContext);
}
