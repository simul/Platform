#pragma once
#include <d3d11.h>
#include <utility>
#include <vector>
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/CppSl.hs"
#include "Simul/Base/FileLoader.h"

namespace simul
{
	namespace dx11
	{
		extern void SetFileLoader(simul::base::FileLoader *l);
		struct TextureStruct
		{
			TextureStruct();
			~TextureStruct();
			void release();
			ID3D11Resource*			texture;
			ID3D11ShaderResourceView*   shaderResourceView;
			D3D11_MAPPED_SUBRESOURCE	mapped;
			int width,length;
			void setTexels(ID3D11DeviceContext *context,const float *float4_array,int texel_index,int num_texels);
			void setTexels(ID3D11DeviceContext *context,const unsigned *uint_array,int texel_index,int num_texels);
			void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			void ensureTexture3DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,int d,DXGI_FORMAT f);
			void map(ID3D11DeviceContext *context);
			void unmap();
		private:
			ID3D11DeviceContext *last_context;
		};
		struct ComputableTexture
		{
			ComputableTexture();
			~ComputableTexture();

			ID3D11Texture2D*            g_pTex_Output;
			ID3D11UnorderedAccessView*  g_pUAV_Output;
			ID3D11ShaderResourceView*   g_pSRV_Output;

			void release();
			void init(ID3D11Device *pd3dDevice,int w,int h);
		};
		struct ComputableTexture3D
		{
			ComputableTexture3D();
			~ComputableTexture3D();

			ID3D11Texture3D*            texture;
			ID3D11UnorderedAccessView*  uav;
			ID3D11ShaderResourceView*   srv;

			void release();
			void init(ID3D11Device *pd3dDevice,int w,int l,int d);
		};
		struct ArrayTexture
		{
			ArrayTexture()
				:m_pArrayTexture(NULL)
				,m_pArrayTexture_SRV(NULL)
			{
			}
			~ArrayTexture()
			{
				release();
			}
			void release()
			{
				SAFE_RELEASE(m_pArrayTexture)
				SAFE_RELEASE(m_pArrayTexture_SRV)
			}
			void create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files);
			ID3D11Texture2D*					m_pArrayTexture;
			ID3D11ShaderResourceView*			m_pArrayTexture_SRV;
		};
		struct Mesh
		{
			Mesh();
			~Mesh();
			void release();
			template<class T> void init(ID3D11Device *pd3dDevice,const std::vector<T> &vertices,std::vector<unsigned short> indices)
			{
				int num_vertices	=vertices.size();
				int num_indices		=indices.size();
				T *v				=new T[num_vertices];
				unsigned short *ind	=new unsigned short[num_indices];
				for(int i=0;i<num_vertices;i++)
					v[i]=vertices[i];
				for(int i=0;i<num_indices;i++)
					ind[i]=indices[i];
				init(pd3dDevice,num_vertices,num_indices,v,ind);
				delete [] v;
				delete [] ind;
			}
			template<class T> void init(ID3D11Device *pd3dDevice,int num_vertices,int num_indices,T *vertices,unsigned short *indices)
			{
				release();
				stride=sizeof(T);
				numVertices=num_vertices;
				numIndices=num_indices;
				D3D11_BUFFER_DESC vertexBufferDesc=
				{
					num_vertices*sizeof(T),
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_VERTEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				D3D11_SUBRESOURCE_DATA InitData;
				ZeroMemory(&InitData,sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = vertices;
				InitData.SysMemPitch = sizeof(T);
				HRESULT hr=pd3dDevice->CreateBuffer(&vertexBufferDesc,&InitData,&vertexBuffer);
				
				// index buffer
				D3D11_BUFFER_DESC indexBufferDesc=
				{
					num_indices*sizeof(unsigned short),
					D3D11_USAGE_DYNAMIC,
					D3D11_BIND_INDEX_BUFFER,
					D3D11_CPU_ACCESS_WRITE,
					0
				};
				ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
				InitData.pSysMem = indices;
				InitData.SysMemPitch = sizeof(unsigned short);
				hr=pd3dDevice->CreateBuffer(&indexBufferDesc,&InitData,&indexBuffer);
			}
			void apply(ID3D11DeviceContext *pImmediateContext,unsigned instanceStride,ID3D11Buffer *instanceBuffer);
			ID3D11Buffer *vertexBuffer;
			ID3D11Buffer *indexBuffer;
			unsigned stride;
			unsigned numVertices;
			unsigned numIndices;
		};
		class UtilityRenderer
		{
			static int instance_count;
			static int screen_width;
			static int screen_height;
			static D3DXMATRIX view;
			static D3DXMATRIX proj;
			static ID3D1xEffect *m_pDebugEffect;
			static ID3D11InputLayout *m_pBufferVertexDecl;
			static ID3D1xBuffer* m_pVertexBuffer;
			static ID3D1xDevice		*m_pd3dDevice;
		public:
			UtilityRenderer();
			~UtilityRenderer();
			static void SetMatrices(D3DXMATRIX v,D3DXMATRIX p);
			static void RestoreDeviceObjects(void *m_pd3dDevice);
			static void InvalidateDeviceObjects();
			static void RecompileShaders();
			static void SetScreenSize(int w,int h);
			static void PrintAt3dPos(ID3D11DeviceContext* pd3dImmediateContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			static void DrawLines(ID3D11DeviceContext* pd3dImmediateContext,VertexXyzRgba *lines,int vertex_count,bool strip);
			static void RenderAngledQuad(ID3D11DeviceContext *context,const float *dir,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj,D3DXVECTOR3 sun_dir);
			static void RenderTexture(ID3D11DeviceContext *m_pImmediateContext,int x1,int y1,int dx,int dy,ID3D1xEffectTechnique* tech);
			static void DrawQuad(ID3D11DeviceContext *m_pImmediateContext,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech);	
			static void DrawQuad2(ID3D11DeviceContext *m_pImmediateContext,int x1,int y1,int dx,int dy,ID3D1xEffect *eff,ID3D1xEffectTechnique* tech);
			static void DrawQuad2(ID3D11DeviceContext *m_pImmediateContext,float x1,float y1,float dx,float dy,ID3D1xEffect *eff,ID3D1xEffectTechnique* tech);
			static void DrawQuad(ID3D11DeviceContext *m_pImmediateContext);
		};
	}
}
namespace std
{
	template<> inline void swap(simul::dx11::TextureStruct& _Left, simul::dx11::TextureStruct& _Right)
	{
		std::swap(_Left.shaderResourceView	,_Right.shaderResourceView);
		std::swap(_Left.texture				,_Right.texture);
		std::swap(_Left.width				,_Right.width);
		std::swap(_Left.length				,_Right.length);
		std::swap(_Left.mapped				,_Right.mapped);
	}
}

#define SET_VERTEX_BUFFER(context,vertexBuffer,VertexType)\
	UINT stride = sizeof(VertexType);\
	UINT offset = 0;\
	context->IASetVertexBuffers(	0,				\
									1,				\
									&vertexBuffer,	\
									&stride,		\
									&offset);

//! Useful Wrapper class to encapsulate constant buffer behaviour
template<class T> class ConstantBuffer:public T
{
public:
	ConstantBuffer()
		:m_pD3D11Buffer(NULL)
		,m_pD3DX11EffectConstantBuffer(NULL)
	{
		// Clear out the part of memory that corresponds to the base class.
		// We should ONLY inherit from simple structs.
		memset(((T*)this),0,sizeof(T));
	}
	~ConstantBuffer()
	{
		InvalidateDeviceObjects();
	}
	ID3D11Buffer*					m_pD3D11Buffer;
	ID3DX11EffectConstantBuffer*	m_pD3DX11EffectConstantBuffer;
	//! Create the buffer object.
	void RestoreDeviceObjects(ID3D11Device *pd3dDevice)
	{
		SAFE_RELEASE(m_pD3D11Buffer);	
		D3D11_SUBRESOURCE_DATA cb_init_data;
		cb_init_data.pSysMem = this;
		cb_init_data.SysMemPitch = 0;
		cb_init_data.SysMemSlicePitch = 0;
		D3D11_BUFFER_DESC cb_desc;
		cb_desc.Usage				= D3D11_USAGE_DYNAMIC;
		cb_desc.BindFlags			= D3D11_BIND_CONSTANT_BUFFER;
		cb_desc.CPUAccessFlags		= D3D11_CPU_ACCESS_WRITE;
		cb_desc.MiscFlags			= 0;
		cb_desc.ByteWidth			= PAD16(sizeof(T));
		cb_desc.StructureByteStride = 0;
		pd3dDevice->CreateBuffer(&cb_desc,&cb_init_data, &m_pD3D11Buffer);
	}
	//! Find the constant buffer in the given effect, and link to it.
	void LinkToEffect(ID3DX11Effect *effect,const char *name)
	{
		m_pD3DX11EffectConstantBuffer=effect->GetConstantBufferByName(name);
		if(m_pD3DX11EffectConstantBuffer)
			m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
		else
			std::cerr<<"ConstantBuffer<> LinkToEffect did not find the buffer named "<<name<<" in the effect."<<std::endl;
	}
	//! Free the allocated buffer.
	void InvalidateDeviceObjects()
	{
		SAFE_RELEASE(m_pD3D11Buffer);
		m_pD3DX11EffectConstantBuffer=NULL;
	}
	//! Apply the stored data using the given context, in preparation for renderiing.
	void Apply(ID3D11DeviceContext *pContext)
	{
		D3D11_MAPPED_SUBRESOURCE mapped_res;
		pContext->Map(m_pD3D11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		*(T*)mapped_res.pData = *this;
		pContext->Unmap(m_pD3D11Buffer, 0);
		m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
	}
};