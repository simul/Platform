// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with that agreement.

#include "SimulTerrainRenderer.h"
#include "Simul/Terrain/Cutout.h"
#include "Simul/Terrain/HeightMapNode.h"
#include "Simul/Clouds/BaseCloudRenderer.h"

#ifdef XBOX
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("game:\\");
	static D3DPOOL d3d_memory_pool=D3DUSAGE_CPU_CACHED_MEMORY;
#else
	#include <tchar.h>
	#include <d3d9.h>
	#include <d3dx9.h>
	#include <dxerr.h>
	#include <string>
	typedef std::basic_string<TCHAR> tstring;
	static tstring filepath=TEXT("");
	static D3DPOOL d3d_memory_pool=D3DPOOL_MANAGED;
#endif

#include <stack>
#include "Simul/Base/SmartPtr.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Terrain/Road.h"
#include "CreateDX9Effect.h"
#include "Macros.h"
#include "Resources.h"
#undef min
#undef max
typedef std::basic_string<TCHAR> tstring;
simul::terrain::HeightMapInterface *heightMapInterface;

SimulTerrainRenderer::SimulTerrainRenderer() :
	m_pVtxDecl(NULL)
	,m_pd3dDevice(NULL)
	,m_pTerrainEffect(NULL)
	,terrain_texture(NULL)
	,detail_texture(NULL)
	,road_texture(NULL)
	,vertexBuffer(NULL)
	,cloud_textures(NULL)
	,skyInterface(NULL)
	,elevation_map_texture(NULL)
	,show_wireframe(false)
	,rebuild_effect(true)
	,y_vertical(true)
	,max_fade_distance_metres(300000.f)
{
	heightmap=new simul::terrain::HeightMapNode();
	heightMapInterface=heightmap.get();
	heightmap->SetFractalFrequency(8);
	heightmap->SetPersistence(0.85f);
	heightmap->SetNumMipMapLevels(3);
	heightmap->SetPageSize(257);
	heightmap->SetTileSize(17);
	heightmap->SetMaxHeight(3000.f);
	heightmap->SetFractalOctaves(6);
	heightmap->SetFractalScale(160000.f);
	heightmap->SetPageWorldX(120000.f);
	heightmap->SetPageWorldZ(120000.f);
	heightmap->SetBaseAltitude(-2000.f);
	heightmap->SetFlattenBelow(0.f);
	heightmap->Rebuild();
}

struct TerrainVertex_t
{
	float x,y,z;
	float normal_x,normal_y,normal_z,ca;
	float tex_x,tex_y;
	float offset;
};

bool SimulTerrainRenderer::Create( LPDIRECT3DDEVICE9 dev)
{
	m_pd3dDevice=dev;
	HRESULT hr=S_OK;
	cam_pos.x=cam_pos.y=cam_pos.z=0;
	D3DXMatrixIdentity(&view);
	D3DXMatrixIdentity(&proj);
	return (hr==S_OK);
}

bool SimulTerrainRenderer::MakeMapTexture()
{
	HRESULT hr=S_OK;
	int size=heightmap->GetPageSize();
	SAFE_RELEASE(elevation_map_texture);
	if(FAILED(hr=D3DXCreateTexture(m_pd3dDevice,size,size,1,D3DUSAGE_AUTOGENMIPMAP,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&elevation_map_texture)))
		return (hr==S_OK);
	D3DLOCKED_RECT lockedRect={0};
	if(FAILED(hr=elevation_map_texture->LockRect(0,&lockedRect,NULL,NULL)))
		return (hr==S_OK);
	unsigned char *ptr=(unsigned char *)(lockedRect.pBits);
	float base=std::max(heightmap->GetBaseAltitude(),heightmap->GetFlattenBelow());
	float max_height=heightmap->GetMaxHeight();
	for(int i=0;i<size;i++)
	{
		for(int j=0;j<size;j++)
		{
			float h=1.f*(0.f+(heightmap->GetHeightAt(i,j)-base)/max_height);
			float c=h*255.f;
			*(ptr++)=(unsigned char)(c);
			*(ptr++)=(unsigned char)(c);
			*(ptr++)=(unsigned char)(c);
			*(ptr++)=(unsigned char)(c);
		}
	}
	hr=elevation_map_texture->UnlockRect(0);
	return (hr==S_OK);
}

static float saturate(float f)
{
	if(f<0.f)
		return 0.f;
	if(f>1.f)
		return 1.f;
	return f;
}
/*static float SnowFunction(float ,simul::math::Vector3 &normal)
{
	float high_enought=1.f;
	float angle_ok=saturate((normal.z-0.5f)/0.1f);
	return high_enought*angle_ok;
}*/
static float GrassFunction(float altitude_metres,simul::math::Vector3 &normal)
{
	float high_enough=saturate((2000.f-altitude_metres)/200.f);
	//(1.f-saturate((altitude_metres-4400.f)/200.f))*
	float angle_ok=saturate((normal.z-0.9f)/0.1f);
	return high_enough*angle_ok;
}


void SimulTerrainRenderer::GetVertex(int i,int j,TerrainVertex_t *V)
{
	int grid=heightMapInterface->GetPageSize();
	float X=(float)i/((float)grid-1.f);
	float Y=(float)j/((float)grid-1.f);
	
	V->x=(X-0.5f)*heightMapInterface->GetPageWorldX();
	V->y=(Y-0.5f)*heightMapInterface->GetPageWorldZ();
	V->z=heightMapInterface->GetHeightAt(i,j);
	if(y_vertical)
		std::swap(V->y,V->z);
	simul::math::Vector3 n=heightMapInterface->GetNormalAt(i,j);
	if(y_vertical)
		std::swap(n.y,n.z);
	V->normal_x=n.x;
	V->normal_y=n.y;
	V->normal_z=n.z;
	//V->ca=1.f-(1.f-saturate((V->y-4400.f)/200.f))*saturate((n.z-0.8f)/0.1f)*saturate((800.f-V->y)/200.f);
	V->ca=GrassFunction(y_vertical?V->y:V->z,n);
	static float tex_scale=20.f;
	V->tex_x=tex_scale*X;
	V->tex_y=tex_scale*Y;
	V->offset=0.f;
}

void SimulTerrainRenderer::GetVertex(float x,float y,TerrainVertex_t *V)
{
	float X=x/heightMapInterface->GetPageWorldX()+0.5f;
	float Y=y/heightMapInterface->GetPageWorldZ()+0.5f;
	
	V->x=x;
	V->y=y;
	V->z=heightMapInterface->GetHeightAt(x,y);
	if(y_vertical)
		std::swap(V->y,V->z);
	simul::math::Vector3 n=heightMapInterface->GetNormalAt(x,y);
	V->normal_x=n.x;
	V->normal_y=n.y;
	V->normal_z=n.z;
	if(y_vertical)
		std::swap(n.y,n.z);
	//V->ca=(saturate((V->y-4400.f)/200.f))*saturate((n.z-0.8f)/0.1f)*saturate((800.f-V->y)/200.f);
	V->ca=GrassFunction(V->y,n);
	V->tex_x=130.f*X;
	V->tex_y=130.f*Y;
	V->offset=0.f;
}

void SimulTerrainRenderer::TerrainModified()
{
	heightmap->Rebuild();
}
bool SimulTerrainRenderer::RestoreDeviceObjects(void *dev)
{
	m_pd3dDevice=(LPDIRECT3DDEVICE9)dev;
	enabled=false;
	HRESULT hr;
	D3DVERTEXELEMENT9 decl[]=
	{
		{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
		{0,12,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
		{0,28,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,1},
		{0,36,D3DDECLTYPE_FLOAT1,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},
		D3DDECL_END()
	};
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateVertexDeclaration(decl,&m_pVtxDecl);

	SAFE_RELEASE(terrain_texture);
	B_RETURN(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/MudGrass01.dds"),&terrain_texture));

	SAFE_RELEASE(detail_texture);
	B_RETURN(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/grass01.dds"),&detail_texture));

	SAFE_RELEASE(road_texture);
	V_CHECK(hr=D3DXCreateTextureFromFile(m_pd3dDevice,TEXT("Media/Textures/road.dds"),&road_texture));

	CreateEffect();

	int num_vertices=heightMapInterface->GetPageSize()*heightMapInterface->GetPageSize();
	if(skyInterface)
		dir_to_sun=(const float*)skyInterface->GetDirectionToLight();
	int grid=heightMapInterface->GetPageSize();

	SAFE_RELEASE(vertexBuffer);
	B_RETURN(m_pd3dDevice->CreateVertexBuffer(num_vertices*sizeof(TerrainVertex_t),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &vertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	B_RETURN(vertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;
	for(int i=0;i<grid;i++)
	{
		for(int j=0;j<grid;j++)
		{
			GetVertex(i,j,V);
			V++;
		}
	}
    B_RETURN(vertexBuffer->Unlock());

	ReleaseIndexBuffers();

//	BuildRoad();

	int squares=heightMapInterface->GetTileSize()-1;
	size_t tile_grid=(heightMapInterface->GetPageSize()-1)/squares;
	tiles.resize(tile_grid);
	for(size_t i=0;i<tile_grid;i++)
	{
		for(size_t j=0;j<tile_grid;j++)
		{
			tiles[i].push_back(TerrainTile(squares,heightMapInterface->GetNumMipMapLevels()));
			for(int m=0;m<heightMapInterface->GetNumMipMapLevels();m++)
				B_RETURN(BuildTile(&(tiles[i][j]),(int)i,(int)j,m));
		}
	}
	enabled=true;
	// Useful:
	MakeMapTexture();
	return (hr==S_OK);
}

bool SimulTerrainRenderer::CreateEffect()
{
	HRESULT hr=S_OK;
	std::map<std::string,std::string> defines;
	if(wrap_clouds)
		defines["WRAP_CLOUDS"]="1";
	char max_fade_distance_str[25];
	sprintf_s(max_fade_distance_str,25,"%g",max_fade_distance_metres);
	defines["MAX_FADE_DISTANCE_METRES"]=max_fade_distance_str;
	if(!y_vertical)
		defines["Z_VERTICAL"]='1';
	else
		defines["Y_VERTICAL"]='1';
	hr=CreateDX9Effect(m_pd3dDevice,m_pTerrainEffect,"simul_terrain.fx",defines);

	m_hTechniqueTerrain	=m_pTerrainEffect->GetTechniqueByName("simul_terrain");
	m_hTechniqueDepthOnly=m_pTerrainEffect->GetTechniqueByName("simul_depth_only");
	techniqueRoad		=m_pTerrainEffect->GetTechniqueByName("simul_road");
	worldViewProj		=m_pTerrainEffect->GetParameterByName(NULL,"worldViewProj");
	eyePosition			=m_pTerrainEffect->GetParameterByName(NULL,"eyePosition");
	lightDirection		=m_pTerrainEffect->GetParameterByName(NULL,"lightDir");

	cloudScales			=m_pTerrainEffect->GetParameterByName(NULL,"cloudScales");
	cloudOffset			=m_pTerrainEffect->GetParameterByName(NULL,"cloudOffset");
	cloudInterp			=m_pTerrainEffect->GetParameterByName(NULL,"cloudInterp");


	lightColour			=m_pTerrainEffect->GetParameterByName(NULL,"lightColour");
	ambientColour		=m_pTerrainEffect->GetParameterByName(NULL,"ambientColour");

	g_mainTexture		=m_pTerrainEffect->GetParameterByName(NULL,"g_mainTexture");
	detailTexture		=m_pTerrainEffect->GetParameterByName(NULL,"detailTexture");
	roadTexture			=m_pTerrainEffect->GetParameterByName(NULL,"roadTexture");

	cloudTexture1		=m_pTerrainEffect->GetParameterByName(NULL,"cloudTexture1");
	cloudTexture2		=m_pTerrainEffect->GetParameterByName(NULL,"cloudTexture2");

	rebuild_effect=false;
	return (hr==S_OK);
}


bool SimulTerrainRenderer::InvalidateDeviceObjects()
{
	HRESULT hr=S_OK;
	if(m_pTerrainEffect)
        hr=m_pTerrainEffect->OnLostDevice();
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(vertexBuffer);
	ReleaseIndexBuffers();
	SAFE_RELEASE(terrain_texture);
	SAFE_RELEASE(detail_texture);
	SAFE_RELEASE(road_texture);
	cloud_textures=NULL;
	SAFE_RELEASE(elevation_map_texture);
	return (hr==S_OK);
}
/*
bool SimulTerrainRenderer::Destroy()
{
	HRESULT hr=InvalidateDeviceObjects();
	SAFE_RELEASE(m_pVtxDecl);
	SAFE_RELEASE(vertexBuffer);
	ReleaseIndexBuffers();
	SAFE_RELEASE(m_pTerrainEffect);
	SAFE_RELEASE(terrain_texture);
	SAFE_RELEASE(detail_texture);
	SAFE_RELEASE(road_texture);
	cloud_textures=NULL;
	heightmap=NULL;
	return (hr==S_OK);
}*/

SimulTerrainRenderer::~SimulTerrainRenderer()
{
	InvalidateDeviceObjects();
}
	static const float radius=50.f;
	static const float height=150.f;

bool SimulTerrainRenderer::RenderOnlyDepth()
{
	PIXBeginNamedEvent(0xFF006600,"SimulTerrainRenderer::RenderOnlyDepth");
	HRESULT hr=InternalRender(true);
	PIXEndNamedEvent();
	return (hr==S_OK);
}

bool SimulTerrainRenderer::Render()
{
	PIXBeginNamedEvent(0xFF00FF00,"SimulTerrainRenderer::Render");
	HRESULT hr=InternalRender(false);
	PIXEndNamedEvent();

	if(highlight_pos.x+highlight_pos.y+highlight_pos.z!=0)
	{
		static float cc=100.f;
		float pos[13*3];
		for(int i=0;i<13;i++)
		{
			float angle=2.f*3.14159f*(float)i/12.f;
			float dx=cc*cos(angle);
			float dy=cc*sin(angle);
			pos[i*3+0]=highlight_pos.x+dx;
			pos[i*3+1]=highlight_pos.z+dy;
			pos[i*3+2]=highlight_pos.y;//z;
			if(y_vertical)
			{
				std::swap(pos[i*3+1],pos[i*3+2]);
			}
		}
		//RenderLines(m_pd3dDevice,12,pos);
	}
	return (hr==S_OK);
}

int SimulTerrainRenderer::GetMip(int i,int j) const
{
	if(i<0||j<0||i>=(int)tiles.size()||j>=(int)tiles.size())
		return -1;
	simul::math::Vector3 pos=tiles[i][j].pos;
	pos-=simul::math::Vector3((const float*)(&cam_pos));
	float dist=pos.Magnitude();
	int mip_level=(int)(dist/12000.f);
	if(mip_level>heightMapInterface->GetNumMipMapLevels()-1)
		mip_level=heightMapInterface->GetNumMipMapLevels()-1;
	return mip_level;
}

bool SimulTerrainRenderer::InternalRender(bool depth_only)
{
	HRESULT hr=S_OK;
	if(!enabled)
		return (hr==S_OK);
	if(rebuild_effect)
		CreateEffect();
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
    m_pd3dDevice->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);

	if(depth_only)
		m_pTerrainEffect->SetTechnique( m_hTechniqueDepthOnly );
	else
	{
		m_pTerrainEffect->SetTechnique( m_hTechniqueTerrain );
	}

	D3DXMATRIX tmp1,tmp2,wvp;
	D3DXMatrixInverse(&tmp1,NULL,&view);
	cam_pos.x=tmp1._41;
	cam_pos.y=tmp1._42;
	cam_pos.z=tmp1._43;
	//D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&view,&proj);
	D3DXMatrixTranspose(&wvp,&tmp2);
	m_pTerrainEffect->SetMatrix(worldViewProj,(const D3DXMATRIX *)(&wvp));
	m_pTerrainEffect->SetVector(eyePosition,(const D3DXVECTOR4 *)(&cam_pos));

	if(skyInterface)
	{
		D3DXVECTOR4 sun_dir(skyInterface->GetDirectionToLight());
		if(y_vertical)
			std::swap(sun_dir.y,sun_dir.z);

		m_pTerrainEffect->SetVector	(lightDirection		,&sun_dir);
		float alt_km=cam_pos.z*0.001f;
		if(y_vertical)
			alt_km=cam_pos.y*0.001f;
		static float light_mult=0.08f;
		simul::sky::float4 light_colour=light_mult*skyInterface->GetLocalIrradiance(alt_km);
		simul::sky::float4 ambient_colour=skyInterface->GetAmbientLight(alt_km);
		m_pTerrainEffect->SetVector	(lightColour		,(const D3DXVECTOR4 *)(&light_colour));
		m_pTerrainEffect->SetVector	(ambientColour		,(const D3DXVECTOR4 *)(&ambient_colour));
	}
	m_pTerrainEffect->SetVector	(cloudScales		,(const D3DXVECTOR4 *)(cloud_scales));
	m_pTerrainEffect->SetVector	(cloudOffset		,(const D3DXVECTOR4 *)(cloud_offset));
	m_pTerrainEffect->SetFloat	(cloudInterp		,cloud_interp);

	m_pTerrainEffect->SetTexture(g_mainTexture		,terrain_texture);
	m_pTerrainEffect->SetTexture(detailTexture		,detail_texture);
	if(cloud_textures)
	{
		m_pTerrainEffect->SetTexture(cloudTexture1		,cloud_textures[0]);
		m_pTerrainEffect->SetTexture(cloudTexture2		,cloud_textures[1]);
	}
	else
	{
		m_pTerrainEffect->SetTexture(cloudTexture1		,NULL);
		m_pTerrainEffect->SetTexture(cloudTexture2		,NULL);
	}

	UINT passes=1;
	hr=m_pTerrainEffect->Begin( &passes, 0 );

	B_RETURN(hr=m_pd3dDevice->SetStreamSource(0,
			vertexBuffer,
			0,
			sizeof(TerrainVertex_t)
			));

	B_RETURN(hr=m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl ));
	for(unsigned p = 0 ; p < passes ; ++p )
	{
		hr=m_pTerrainEffect->BeginPass(p);

		if(p<2||(cloud_textures&&p==3)||(!cloud_textures&&p==2)||(show_wireframe&&p==4))
		for(size_t i=0;i<tiles.size();i++)
		{
			for(size_t j=0;j<tiles.size();j++)
			{
				int mip_level=GetMip(i,j);
				TerrainTileMIP *mip=&tiles[i][j].mips[mip_level];
				B_RETURN(hr=m_pd3dDevice->SetIndices(mip->indexBuffer));

				if(mip->tri_strip)
					B_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,mip->num_verts,0,mip->num_prims-2))
				else if(mip->num_squares)
					B_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,mip->num_verts,0,mip->num_prims/3))
				
				if(mip->edges.size())
				for(size_t k=0;k<4;k++)
				{
					int other_mip_level=GetMip(i+(k==0)-(k==1),j+(k==2)-(k==3));
					if(other_mip_level<0)
						continue;
					int idx=other_mip_level-mip_level;
					if(idx<0)
						idx=0;
					B_RETURN(hr=m_pd3dDevice->SetIndices(mip->edges[idx].edge[k].indexBuffer));
					int num_tris=mip->edges[idx].edge[k].num_tris;
					int num_verts=mip->edges[idx].edge[k].num_verts;
					B_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,num_verts,0,num_tris))
					
				}
				if(mip->extraIndexBuffer)
				{
					B_RETURN(hr=m_pd3dDevice->SetStreamSource(0,mip->extraVertexBuffer,0,sizeof(TerrainVertex_t)));
					B_RETURN(hr=m_pd3dDevice->SetIndices(mip->extraIndexBuffer));
					B_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,(unsigned)mip->extra_vertices.size(),0,(unsigned)mip->extra_triangles.size()/3))
					B_RETURN(hr=m_pd3dDevice->SetStreamSource(0,vertexBuffer,0,sizeof(TerrainVertex_t)));
				}
			}
		}

		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();
	m_pTerrainEffect->SetTechnique( techniqueRoad );
	m_pTerrainEffect->SetTexture(g_mainTexture,road_texture);
	hr=m_pTerrainEffect->Begin( &passes, 0 );
	for(unsigned p = 0 ; p < passes ; ++p )
	{
		hr=m_pTerrainEffect->BeginPass(p);
		for(size_t i=0;i<roads.size();i++)
		{
			if(roads[i].roadVertexBuffer)
			{
				B_RETURN(hr=m_pd3dDevice->SetStreamSource(0,roads[i].roadVertexBuffer,0,sizeof(TerrainVertex_t)));
				B_RETURN(hr=m_pd3dDevice->SetIndices(roads[i].roadIndexBuffer));
				B_RETURN(hr=m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,0,0,roads[i].num_verts,0,roads[i].num_verts-2))
				B_RETURN(hr=m_pd3dDevice->SetStreamSource(0,vertexBuffer,0,sizeof(TerrainVertex_t)));
			}
		}
		hr=m_pTerrainEffect->EndPass();
	}
	hr=m_pTerrainEffect->End();

	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    hr=m_pd3dDevice->SetRenderState(D3DRS_ZENABLE,TRUE);
	return (hr==S_OK);
}

bool SimulTerrainRenderer::RenderMap(int width)
{
	int w=(width-16)/3;

	int h=w;
	HRESULT hr=RenderTexture(m_pd3dDevice,8,8,w,h,elevation_map_texture);

	return(hr==S_OK);
	//int size=heightmap->GetPageSize();
	//return RenderTexture(m_pd3dDevice,16,16,size,size,elevation_map_texture);
}

void SimulTerrainRenderer::SetMatrices(const D3DXMATRIX &v,const D3DXMATRIX &p)
{
	view=v;
	proj=p;
}

void SimulTerrainRenderer::SetCloudTextures(void **t,bool wrap)
{
	cloud_textures=(LPDIRECT3DVOLUMETEXTURE9*)t;
	if(wrap_clouds!=wrap)
	{
		wrap_clouds=wrap;
		rebuild_effect=true;
	}
}

simul::terrain::HeightMapInterface *SimulTerrainRenderer::GetHeightMapInterface()
{
	return heightMapInterface;
}

void SimulTerrainRenderer::Highlight(const float *x,const float *d)
{
	simul::math::Vector3 X(x);
	simul::math::Vector3 D(d);
	D.Normalize();
	simul::math::Vector3 I;
	if(y_vertical)
	{
		std::swap(X.y,X.z);
		std::swap(D.y,D.z);
	}
	highlight_pos.x=highlight_pos.y=highlight_pos.z=0;
	if(heightMapInterface->Collide(X,D,I))
	{
		if((I-X).Magnitude()<100000.f)
		{
			highlight_pos.x=I.x;
			highlight_pos.y=I.y;
			highlight_pos.z=I.z;
			if(y_vertical)
				std::swap(highlight_pos.y,highlight_pos.z);
		}
	}
}

void SimulTerrainRenderer::Update(float )
{
}

void SimulTerrainRenderer::SetCloudShadowCallback(simul::clouds::CloudShadowCallback *cb)
{
	if(cb)
	{
		SetCloudTextures	(cb->GetCloudTextures(),cb->GetWrap());
		SetCloudScales		(cb->GetCloudScales());
		SetCloudOffset		(cb->GetCloudOffset());
		setCloudInterpolation(cb->GetInterpolation());
	}
}

void SimulTerrainRenderer::ReleaseIndexBuffers()
{
	for(size_t i=0;i<tiles.size();i++)
	{
		for(size_t j=0;j<tiles.size();j++)
		{
			tiles[i][j].Reset();
		}
	}
	for(size_t i=0;i<roads.size();i++)
	{
		roads[i].Reset();
	}
}

unsigned SimulTerrainRenderer::TilePosToIndex(int i,int j,int x,int y) const
{
	int grid=heightMapInterface->GetPageSize();
	int tile_size=heightMapInterface->GetTileSize();
	int I=i*(tile_size-1)+x;
	int J=j*(tile_size-1)+y;
	return I*(grid)+J;
}
simul::base::SmartPtr<simul::terrain::Road> road;
	
bool SimulTerrainRenderer::BuildRoad()
{
	HRESULT hr=S_OK;
	road=new simul::terrain::Road(heightMapInterface);

	roads.push_back(RoadRenderable());
	RoadRenderable *rr=&(roads.back());
	rr->num_verts=2*road->GetNumSegments()*12;

	
	SAFE_RELEASE(rr->roadVertexBuffer);
	B_RETURN(m_pd3dDevice->CreateVertexBuffer( rr->num_verts*sizeof(TerrainVertex_t),D3DUSAGE_WRITEONLY,0,
									  D3DPOOL_DEFAULT, &rr->roadVertexBuffer,
									  NULL ));
	TerrainVertex_t *vertices;
	B_RETURN(rr->roadVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
	TerrainVertex_t *V=vertices;

	//RHS of road
	float along=0.f;
	float tex_repeat_length=50.f;
	for(int a=0;a<rr->num_verts/2;a++)
	{
		simul::math::float3 x=road->GetCentreLine ((float)a/(float)(rr->num_verts/2-1));
		simul::math::float3 nextx
							=road->GetCentreLine ((float)(a+1)/(float)(rr->num_verts/2-1));
		simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(rr->num_verts/2-1));
		simul::math::float2 left ((const float*)(x+dx));
		simul::math::float2 right((const float*)(x-dx));
		GetVertex(right.x,right.y,V);
		V->z=x.z;
		V->tex_x=0.f;
		V->tex_y=along/tex_repeat_length;
		V++;
		GetVertex( left.x, left.y,V);
		V->z=x.z;
		if(y_vertical)
			std::swap(V->y,V->z);
		V->tex_x=1.f;
		V->tex_y=along/tex_repeat_length;
		V++;
		along+=simul::math::float3::length(nextx-x);
	}
    B_RETURN(rr->roadVertexBuffer->Unlock());
	
	B_RETURN(m_pd3dDevice->CreateIndexBuffer(rr->num_verts*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
		D3DPOOL_DEFAULT, &rr->roadIndexBuffer, NULL ));
	unsigned *indexData;
	B_RETURN(rr->roadIndexBuffer->Lock(0, rr->num_verts, (void**)&indexData, 0 ));

	unsigned *index=indexData;
	for(int x=0;x<rr->num_verts/2;x++)
	{
		*(index++)	=2*x;
		*(index++)	=2*x+1;
	}
	B_RETURN(rr->roadIndexBuffer->Unlock());
	return (hr==S_OK);
}

bool SimulTerrainRenderer::BuildMIPEdge(TerrainTile *tile,int i,int j,int mip_level,int lower_level,int nsew)
{
	HRESULT hr=S_OK;
	TerrainTileMIP *mip=&(tile->mips[mip_level]);
	MIPEdges *edges=NULL;
	int idx=lower_level-mip_level;
	if(idx>=(int)mip->edges.size())
	{
		mip->edges.push_back(MIPEdges());
	}
	edges=&(mip->edges[idx]);
	MIPEdge *edge=&edges->edge[nsew];

	int grid=heightMapInterface->GetTileSize();

	int tile_size=heightMapInterface->GetTileSize();
	tile_size-=1;
	tile_size>>=mip_level;
	int skip1=1<<mip_level;

	int lower_tile_size=heightMapInterface->GetTileSize();
	lower_tile_size-=1;
	lower_tile_size>>=lower_level;
	int skip2=1<<lower_level;

	// now have two tile sizes, e.g. 9 and 3.
	
	// for each of the lower sizes, e.g. 3, we have one triangle, plus a fan of size 2^(diff)
	int diff=lower_level-mip_level;
	int U=1<<diff;
	edge->num_verts=lower_tile_size*(1+U);
	edge->num_tris=lower_tile_size*(1+U)-2;

	if(edge->num_tris>0)
	{
		B_RETURN(m_pd3dDevice->CreateIndexBuffer(edge->num_tris*3*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
												  D3DPOOL_DEFAULT, &edge->indexBuffer,NULL));
		unsigned *indexData;
		B_RETURN(edge->indexBuffer->Lock(0,edge->num_tris,(void**)&indexData,0));

		unsigned *index=indexData;
		for(int x=0;x<lower_tile_size;x++)
		{
			// The big central triangles:
			int offs=((skip1==skip2)?(x==0?skip2:0):skip2/2);
			int a1;
			int a2;
			int a3;
			int b1;
			int b2;
			int b3;
			for(int u=0;u<1+U;u++)
			{
				if(x==0&&u==1)
					continue;
				if(x==lower_tile_size-1&&u==U)
					continue;
				if(!u)
				{
					a1=0;
					a2=0;
					a3=skip1;
					b1=(x+1)*skip2;
					b2=(x  )*skip2;
					b3=(x  )*skip2+offs;
				}
				else
				{
					int v=(u-1);
					int w=U==1?1:(u-1)>=(U/2)?1:0;
					a1=0;
					a2=skip1;
					a3=skip1;
					b1=(x+w)*skip2;
					b2=x*skip2+v*skip1;
					b3=b2+skip1;
				}
				if(nsew==0)
				{
					*(index++)	=TilePosToIndex(i,j,(grid-1-a1)		,b1		);
					*(index++)	=TilePosToIndex(i,j,(grid-1-a2)		,b2		);
					*(index++)	=TilePosToIndex(i,j,(grid-1-a3)		,b3		);
				}													
				if(nsew==1)											
				{													
					*(index++)	=TilePosToIndex(i,j,(a2)			,b2		);
					*(index++)	=TilePosToIndex(i,j,(a1)			,b1		);
					*(index++)	=TilePosToIndex(i,j,(a3)			,b3		);
				}
				if(nsew==2)
				{
					*(index++)	=TilePosToIndex(i,j,b2			,(grid-1-a2)	);
					*(index++)	=TilePosToIndex(i,j,b1			,(grid-1-a1)	);
					*(index++)	=TilePosToIndex(i,j,b3			,(grid-1-a3)	);
				}
				if(nsew==3)
				{
					*(index++)	=TilePosToIndex(i,j,b1			,(a1)			);
					*(index++)	=TilePosToIndex(i,j,b2			,(a2)			);
					*(index++)	=TilePosToIndex(i,j,b3			,(a3)			);
				}
			}
		}
		B_RETURN(edge->indexBuffer->Unlock());
	}
	else
	{
		edge->indexBuffer=0;
	}

	return (hr==S_OK);
}

bool SimulTerrainRenderer::BuildTile(TerrainTile *tile,int i,int j,int mip_level)
{
	int tile_size=heightMapInterface->GetTileSize();
	int skip=1<<mip_level;

	float X=((float)i+0.5f)/((float)tiles.size());
	float Y=((float)j+0.5f)/((float)tiles.size());
	simul::math::Vector3 pos;
	tile->pos[0]=(X-0.5f)*heightMapInterface->GetPageWorldX();
	tile->pos[1]=heightMapInterface->GetHeightAt(i*(tile_size-1)+tile_size/2,j*(tile_size-1)+tile_size/2);
	tile->pos[2]=(Y-0.5f)*heightMapInterface->GetPageWorldZ();

	TerrainTileMIP *mip=&(tile->mips[mip_level]);

	mip->num_verts=heightMapInterface->GetPageSize()*heightMapInterface->GetPageSize();
	simul::terrain::Cutout cutout;
#if 1
	//const float *r=road->GetSegmentData();
	if(road)
	{
		int s=12*road->GetNumSegments();
		simul::math::float2 dx;

		//RHS of road
		for(int a=0;a<s;a++)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x+dx),x.z);
		}
		//LHS of road:
		for(int a=s-1;a>=0;a--)
		{
			simul::math::float3 x=road->GetCentreLine((float)a/(float)(s-1));
			simul::math::float3 dx=road->GetEdgeOffset((float)a/(float)(s-1));
			cutout.addvertex((const float*)(x-dx),x.z);
		}
	}
#else
	float rad=4500.f;
	for(int a=0;a<16;a++)
	{
		float angle=2.f*3.14159f*(float)a/(float)16;
		cutout.addvertex(simul::math::float2(rad*cos(angle),-rad*sin(angle)),0);
	}
#endif

	mip->ApplyCutouts(heightMapInterface,i,j,skip,&cutout);

	HRESULT hr=S_OK;

	tile_size-=1;
	tile_size>>=mip_level;
	tile_size+=1;
	if(mip->tri_strip)
	{
		if(tile_size>=2)
		{
			mip->num_prims=(tile_size-1)*2*(tile_size-3);
			B_RETURN(m_pd3dDevice->CreateIndexBuffer(mip->num_prims*sizeof(unsigned),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->indexBuffer,NULL));
			unsigned *indexData;
			B_RETURN(mip->indexBuffer->Lock(0,mip->num_prims,(void**)&indexData,0));

			unsigned *index=indexData;
			int num=0;
			for(int x=1;x<tile_size-2;x+=2)
			{
				for(int y=1;y<tile_size-1;y++)
				{
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,y*skip);
					num++;
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
					num++;
				}
				*(index++)		=TilePosToIndex(i,j,(x  )*skip,(tile_size-2)*skip);
					num++;
				*(index++)		=TilePosToIndex(i,j,(x  )*skip,(tile_size-2)*skip);
					num++;
				for(int y=1;y<tile_size-1;y++)
				{
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(tile_size-1-y)*skip);
					num++;
					*(index++)	=TilePosToIndex(i,j,(x+2)*skip,(tile_size-1-y)*skip);
					num++;
				}
				*(index++)		=TilePosToIndex(i,j,(x+1)*skip,skip);
					num++;
				*(index++)		=TilePosToIndex(i,j,(x+1)*skip,skip);
					num++;
			}
			B_RETURN(mip->indexBuffer->Unlock());
			for(int lower_level=mip_level;lower_level<heightMapInterface->GetNumMipMapLevels();lower_level++)
			{
				for(int nsew=0;nsew<4;nsew++)
				{
					B_RETURN(BuildMIPEdge(tile,i,j,mip_level,lower_level,nsew));
				}
			}
		}
		else
		{
			mip->num_prims=0;
		}
	}
	else
	{
		if(mip->num_squares)
		{
			// Must create 
			mip->num_prims=6*mip->num_squares;
			B_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->num_prims*sizeof(unsigned)),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->indexBuffer,
											  NULL ));
			unsigned *indexData;
			B_RETURN(mip->indexBuffer->Lock(0, mip->num_prims, (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(int x=0;x<tile_size-1;x++)
			{
				for(int y=0;y<tile_size-1;y++)
				{
					if(!mip->unimpeded_squares[x][y])
						continue;
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(y+1)*skip);
					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,y*skip);

					*(index++)	=TilePosToIndex(i,j,(x+1)*skip,(y+1)*skip);
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,y*skip);
					*(index++)	=TilePosToIndex(i,j,(x  )*skip,(y+1)*skip);
				}
			}
			B_RETURN(mip->indexBuffer->Unlock());
		}

		if(mip->extra_vertices.size())
		{
			// Must create own vertex buffer:
			SAFE_RELEASE(mip->extraVertexBuffer);
			B_RETURN(m_pd3dDevice->CreateVertexBuffer((unsigned)(mip->extra_vertices.size()*sizeof(TerrainVertex_t)),
											D3DUSAGE_WRITEONLY,0,
										  D3DPOOL_DEFAULT, &mip->extraVertexBuffer,
										  NULL ));
			TerrainVertex_t *vertices;
			B_RETURN(mip->extraVertexBuffer->Lock( 0, sizeof(TerrainVertex_t), (void**)&vertices,0 ));
			TerrainVertex_t *V=vertices;
			for(size_t i=0;i<mip->extra_vertices.size();i++)
			{
				GetVertex(mip->extra_vertices[i].x,mip->extra_vertices[i].y,V);
				V->y=mip->extra_vertices[i].z;
				V++;
			}
			B_RETURN(mip->extraVertexBuffer->Unlock());
		}
		if(mip->extra_triangles.size())
		{
			// and the index buffer for these extra vertices:
			B_RETURN(m_pd3dDevice->CreateIndexBuffer((unsigned)(mip->extra_triangles.size()*sizeof(unsigned)),D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,
											  D3DPOOL_DEFAULT, &mip->extraIndexBuffer,
											  NULL ));
			unsigned *indexData;
			B_RETURN(mip->extraIndexBuffer->Lock(0,(unsigned)mip->extra_triangles.size(), (void**)&indexData, 0 ));
			unsigned *index=indexData;
			for(size_t i=0;i<mip->extra_triangles.size();i++)
			{
				*(index++)	=mip->extra_triangles[i];
			}
			B_RETURN(mip->extraIndexBuffer->Unlock());
		}
	}
	return (hr==S_OK);
}

SimulTerrainRenderer::TerrainTile::TerrainTile(int tilesize,int num_mips)
{
	for(int i=0;i<num_mips;i++)
	{
		mips.push_back(TerrainTileMIP(tilesize));
		tilesize>>=1;
	}
}

SimulTerrainRenderer::TerrainTileMIP::TerrainTileMIP(int tilesize)
	:indexBuffer(NULL)
	,extraVertexBuffer(NULL)
	,extraIndexBuffer(NULL)
	,num_prims(0)
{
	tile_size=tilesize;
	unimpeded_squares.resize(tilesize);
	num_squares=0;
	tri_strip=false;
	for(int i=0;i<tilesize;i++)
	{
		unimpeded_squares[i].resize(tilesize);
	}
}

void SimulTerrainRenderer::TerrainTileMIP::Reset()
{
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(extraVertexBuffer);
	SAFE_RELEASE(extraIndexBuffer);
	indexBuffer=NULL;
	num_prims=0;
	num_squares=0;
	num_verts=0;
	tri_strip=false;
	for(size_t i=0;i<edges.size();i++)
		edges[i].Reset();
}

void SimulTerrainRenderer::MIPEdge::Reset()
{
	SAFE_RELEASE(indexBuffer);
	num_tris=0;
}
void SimulTerrainRenderer::MIPEdges::Reset()
{
	for(int i=0;i<4;i++)
		edge[i].Reset();
}

void SimulTerrainRenderer::RoadRenderable::Reset()
{
	SAFE_RELEASE(roadVertexBuffer);
	SAFE_RELEASE(roadIndexBuffer);
	roadVertexBuffer=NULL;
	roadIndexBuffer=NULL;
	num_verts=0;
}

static size_t loopindex(int idx,size_t length)
{
	while(idx<0)
		idx+=(int)length;
	return (size_t)(idx%(length));
}
// which squares are affected?
void SimulTerrainRenderer::TerrainTileMIP::ApplyCutouts(
	simul::terrain::HeightMapInterface *hmi,int i,int j,int step,const simul::terrain::Cutout *cutout)
{
	simul::math::float2 min_corner,max_corner;
	cutout->GetCorners(min_corner,max_corner);
	
	simul::math::float2 block_min,block_max;

	hmi->GetBlockCorners(i,j,block_min,block_max);

	if(!(block_min<max_corner&&block_max>min_corner)
		)
	{
		num_squares=0;
		tri_strip=true;
		for(int x=0;x<tile_size;x++)
		{
			for(int y=0;y<tile_size;y++)
			{
				this->unimpeded_squares[x][y]=true;
			}
		}
		return;
	}
	tri_strip=false;
	num_squares=0;
	//simul::math::float2 block_size=block_max-block_min;
	// test each square.
	//simul::math::float2 square_size=block_size/(float)(hmi->GetTileSize()-1);
	for(int x=0;x<tile_size;x++)
	{
		for(int y=0;y<tile_size;y++)
		{
			this->unimpeded_squares[x][y]=true;
			simul::math::float2 square_min,square_max;
			hmi->GetVertexPos((i*tile_size+x)*step,(j*tile_size+y)*step,square_min);
			hmi->GetVertexPos((i*tile_size+x+1)*step,(j*tile_size+y+1)*step,square_max);
			if(square_min.x>max_corner.x||square_min.y>max_corner.y
				||square_max.x<min_corner.x||square_max.y<min_corner.y
				)
			{
				num_squares++;
				continue;
			}
			this->unimpeded_squares[x][y]=false;
			struct State
			{
				simul::math::float2 this_point,next_point;
				int Case;
				size_t corner;
				size_t index;
				int counter;
				simul::terrain::Cutout::Corner start_corner;
			};
			std::stack<State> remaining;
			std::stack<simul::terrain::Cutout> polygons;
			State state;
			simul::terrain::Cutout square;
			square.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
			square.addvertex(simul::math::float2(square_min.x,square_max.y),hmi->GetHeightAt(square_min.x,square_max.y));
			square.addvertex(square_max,hmi->GetHeightAt(square_max.x,square_max.y));
			square.addvertex(simul::math::float2(square_max.x,square_min.y),hmi->GetHeightAt(square_max.x,square_min.y));
			simul::terrain::Cutout building;
			state.Case=0;		// going along the edge of the square outside the cutout
			// firstly is the bottom-left corner inside or outside of the cutout?
			state.counter=0;
			state.start_corner=NULL;
			if(cutout->IsInside(square_min))
			{	
				this->unimpeded_squares[x][y]=false;
				state.counter=1;
				state.Case=1;		// going along the edge of the square inside the cutout
			}
			else	// add a vertex:
			{
				building.addvertex(square_min,hmi->GetHeightAt(square_min.x,square_min.y));
				state.start_corner=simul::terrain::Cutout::Corner(&square.vertex(0),&square.vertex(-1));
			}
			state.corner=0;
			state.index=0;
			state.this_point=square.vertex((int)state.corner).pos;
			state.next_point=square.vertex((int)state.corner+1).pos;
			int direction=1;// anticlockwise.
			cutout->unuse();
			while(state.corner<4)
			{
				simul::math::float2 intersect;
				// go round the square clockwise, looking for intersections.
				// crossover=1 means entering - use the anti-clockwise direction, 1 means clockwise.
				int edge=-1;
				int crossover=0;
				bool finished=false;
				if(state.Case==0)
				{
					square.unuse();
					square.vertex((int)state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						if(state.corner<4)
							building.addvertex(state.next_point,hmi->GetHeightAt(state.next_point.x,state.next_point.y));
						state.this_point=state.next_point;
						state.next_point=square.vertex((int)state.corner+1).pos;
					}
					// we've entered the polygon. 
					else if(!state.counter&&crossover==1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex((int)state.corner));
						// was this where we started?
						if(state.start_corner==this_corner)
						{
							finished=true;
						}
						else
						{
							// The state branches here. The first branch moves along the outside of the cutout.
							// The second branch moves on the square in State 1.
							{
								remaining.push(State());
								State &branch=remaining.top();
								branch.this_point=intersect;
								branch.next_point=state.next_point;
								branch.Case=1;
								branch.counter=1;
								branch.corner=state.corner;
								branch.index=branch.corner;
								branch.start_corner=this_corner;

							}
							{
								building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
								state.index=edge;
								if(crossover>0)
									state.index++;
								direction=-crossover;
								cutout->unuse();
								cutout->vertex((int)state.index-((direction<0)?1:0)).used=true;
								state.this_point=intersect;
								if(!state.counter)
								{
									state.Case=2;		// going along the edge of the cutout
									// next point along cutout.
									state.next_point=cutout->vertex((int)state.index+direction).pos;
								}
							}
						}
						unimpeded_squares[x][y]=false;
						state.counter++;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				else if(state.Case==2)
				{
					// do we hit an edge of the square?
					crossover=square.GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						building.addvertex(state.next_point,cutout->GetHeightAt((int)state.index-(direction<0),state.next_point));
						state.index+=direction;
						cutout->unuse();
						cutout->vertex((int)state.index-((direction<0)?1:0)).used=true;
						state.this_point=state.next_point;
						state.next_point=cutout->vertex((int)state.index+direction).pos;
					}
					else if(state.counter&&crossover==-1)
					{
						if(edge<(int)state.corner)
							edge+=4;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							simul::terrain::Cutout::Corner this_corner(&cutout->vertex((int)state.index-(direction<0)),&square.vertex(edge));
							// was this where we started?
							if(state.start_corner==this_corner)
							{
								finished=true;
							}
							state.corner=edge;
							if(state.corner<4)
								building.addvertex(intersect,cutout->GetHeightAt((int)state.index-(direction<0),intersect));
							state.Case=0;		// going along the edge of the square
							state.next_point=square.vertex(edge-crossover).pos;	// next point along cutout.
						}
						else
							building.addvertex(intersect,cutout->GetHeightAt((int)state.index,intersect));
						state.index=edge;
						direction=1;
					}
					else
						DebugBreak();
				}
				else //Case=1
				{
					square.vertex((int)state.index).used=true;
					crossover=cutout->GetCrossover(state.this_point,state.next_point,intersect,edge);
					square.unuse();
					if(crossover==0)
					{
						cutout->unuse();
						state.corner++;
						state.index++;
						state.this_point=state.next_point;
						state.next_point=square.vertex((int)state.corner+1).pos;
					}
					// we've exited the polygon. 
					else if(state.counter&&crossover==-1)
					{
						simul::terrain::Cutout::Corner this_corner(&cutout->vertex(edge),&square.vertex((int)state.index));
						building.addvertex(intersect,cutout->GetHeightAt(edge,intersect));
						cutout->unuse();
						cutout->vertex(edge).used=true;
						direction=1;
						state.this_point=intersect;
						state.counter--;
						if(!state.counter)
						{
							state.Case=0;		// going along the edge of the square
							state.start_corner=this_corner;
							
						}
						this->unimpeded_squares[x][y]=false;
					}
					else
					{
						//ignore this edge:
						cutout->vertex(edge).used=true;
					}
				}
				if(state.corner>=4||finished)
				{
					polygons.push(building);
					building.Clear();
					if(remaining.size())
					{
						state=remaining.top();
						remaining.pop();
					}
				}
			}
			if(unimpeded_squares[x][y])
				num_squares++;
			else
			{
				while(polygons.size())
				{
					simul::terrain::Cutout& building=polygons.top();
					// eliminate close duplicates
					for(size_t i=0;i<building.NumVertices();i++)
					{
						simul::math::float2 diff=building.vertex((int)i).pos-building.vertex((int)i+1).pos;
						// merge values that are less than 1mm apart:
						if(simul::math::float2::length(diff)<0.1f)
						{
						//	building.vertices.erase(building.vertices.begin()+i);
						//	i--;
						}
					}
					std::vector<int> indices;
					for(size_t i=0;i<building.NumVertices();i++)
					{
						indices.push_back((int)extra_vertices.size());
						extra_vertices.push_back(simul::math::float3(
							building.vertex((int)i).pos.x,building.vertex((int)i).pos.y,
							building.vertex((int)i).height));
					}
					//triangulate the "building" polygon:
					while(building.NumVertices()>=3)
					{
						int vert_ear=(int)building.FindEar();
						if(vert_ear<0)
						{
							building.FindEar();
							break;
						}
						// the new triangle is between vert_ear and the two adjacent vertices.
						extra_triangles.push_back(indices[vert_ear]);
						extra_triangles.push_back(indices[loopindex(vert_ear+1,indices.size())]);
						extra_triangles.push_back(indices[loopindex(vert_ear-1,indices.size())]);
						building.RemoveVertex(vert_ear);
						indices.erase(indices.begin()+vert_ear);
						
					}
					if(building.NumVertices()==3)
					{
						for(int i=0;i<(int)building.NumVertices();i++)
						{
							extra_triangles.push_back(indices[i]);
						}
					}
					else
					{
					}
					polygons.pop();
				}
			}
		}
	}
}

void SimulTerrainRenderer::TerrainTile::Reset()
{
	for(size_t i=0;i<mips.size();i++)
	{
		mips[i].Reset();
	}
}