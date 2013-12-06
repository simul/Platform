#pragma once
#ifndef SIMUL_PLATFORM_DIRECTX11_UTILITIES_H
#define SIMUL_PLATFORM_DIRECTX11_UTILITIES_H

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
		struct SIMUL_DIRECTX11_EXPORT TextureStruct
		{
			TextureStruct();
			~TextureStruct();
			void release();
			ID3D11Resource*				texture;
			ID3D11ShaderResourceView*   shaderResourceView;
			ID3D11UnorderedAccessView*  unorderedAccessView;
			ID3D11RenderTargetView*		renderTargetView;
			ID3D11Resource*				stagingBuffer;

			D3D11_MAPPED_SUBRESOURCE	mapped;
			int width,length,depth;
			DXGI_FORMAT format;
			void copyToMemory(ID3D11Device *pd3dDevice,ID3D11DeviceContext *context,void *target,int start_texel=0,int texels=0);
			void setTexels(ID3D11DeviceContext *context,const void *src,int texel_index,int num_texels);
			void init(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f);
			void ensureTexture3DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,int d,DXGI_FORMAT f,bool computable=false);
			void ensureTexture2DSizeAndFormat(ID3D11Device *pd3dDevice,int w,int l,DXGI_FORMAT f,bool computable=false,bool rendertarget=false);
			void ensureTexture1DSizeAndFormat(ID3D11Device *pd3dDevice,int w,DXGI_FORMAT f,bool computable=false);
			void map(ID3D11DeviceContext *context);
			bool isMapped() const;
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
		struct ArrayTexture
		{
			ArrayTexture()
				:m_pArrayTexture(NULL)
				,m_pArrayTexture_SRV(NULL)
				,unorderedAccessView(NULL)
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
				SAFE_RELEASE(unorderedAccessView);
			}
			void create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files);
			void create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable);
			ID3D11Texture2D*					m_pArrayTexture;
			ID3D11ShaderResourceView*			m_pArrayTexture_SRV;
			ID3D11UnorderedAccessView*			unorderedAccessView;
		};
		//! A vertex buffer wrapper class for arbitrary vertex types.
		template<class T> struct VertexBuffer
		{
			ID3D11Buffer				*vertexBuffer;
			//ID3D11UnorderedAccessView	*unorderedAccessView;
			VertexBuffer()
				:vertexBuffer(NULL)
				//,unorderedAccessView(NULL)
			{
			}
			~VertexBuffer()
			{
				release();
			}
			//! Make sure the buffer has the number of vertices specified.
			void ensureBufferSize(ID3D11Device *pd3dDevice,int numVertices,void *data=NULL)
			{
				release();
				D3D11_BUFFER_DESC desc	=
				{
					numVertices*sizeof(T),
					D3D11_USAGE_DEFAULT,
					D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_STREAM_OUTPUT	//D3D11_BIND_UNORDERED_ACCESS is useless for VB's in DX11
					,0// CPU
					,0//D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
					,sizeof(T)			//StructureByteStride
				};
				SAFE_RELEASE(vertexBuffer);
				D3D11_SUBRESOURCE_DATA init;
				init.pSysMem			=data;
				init.SysMemPitch		=sizeof(T);
				init.SysMemSlicePitch	=0;
				V_CHECK(pd3dDevice->CreateBuffer(&desc,data?(&init):NULL,&vertexBuffer));
			/*	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
				ZeroMemory(&uav_desc,sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
				uav_desc.Format					=DXGI_FORMAT_R32_FLOAT;
				uav_desc.ViewDimension			=D3D11_UAV_DIMENSION_BUFFER;
				uav_desc.Buffer.FirstElement	=0;
				uav_desc.Buffer.NumElements		=numVertices;
				uav_desc.Buffer.Flags			=0;
				V_CHECK(pd3dDevice->CreateUnorderedAccessView(vertexBuffer, &uav_desc, &unorderedAccessView));*/
			}
			//! Use this vertex buffer in the next draw call - wraps IASetVertexBuffers.
			void apply(ID3D11DeviceContext *pContext,int slot)
			{
				UINT stride = sizeof(T);
				UINT offset = 0;
				pContext->IASetVertexBuffers(	slot,				// the first input slot for binding
												1,					// the number of buffers in the array
												&vertexBuffer,		// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset );			// array of offset values, one for each buffer
			}
			//! Write to this vertex buffer from the Geometry shader in the next draw call - wraps SOSetTargets.
			void setAsStreamOutTarget(ID3D11DeviceContext *pContext)
			{
				UINT offset = 0;
				pContext->SOSetTargets(1,&vertexBuffer,&offset );
			}
			void release()
			{
				SAFE_RELEASE(vertexBuffer)
				//SAFE_RELEASE(unorderedAccessView)
			}
		};
		struct Mesh
		{
			Mesh();
			~Mesh();
			void release();
			template<class T> void init(ID3D11Device *pd3dDevice,const std::vector<T> &vertices,std::vector<unsigned short> indices)
			{
				int num_vertices	=(int)vertices.size();
				int num_indices		=(int)indices.size();
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
		public:
			static ID3D1xEffect		*m_pDebugEffect;
			static ID3D11InputLayout*m_pCubemapVtxDecl;
			static ID3D1xBuffer		* m_pVertexBuffer;
			static ID3D1xDevice		*m_pd3dDevice;
			UtilityRenderer();
			~UtilityRenderer();
			static void SetMatrices(D3DXMATRIX v,D3DXMATRIX p);
			static void RestoreDeviceObjects(void *m_pd3dDevice);
			static void InvalidateDeviceObjects();
			static void RecompileShaders();
			static void SetScreenSize(int w,int h);
			static void PrintAt3dPos(		ID3D11DeviceContext* pContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			static void DrawLines(			ID3D11DeviceContext* pContext,VertexXyzRgba *lines,int vertex_count,bool strip);
			static void RenderAngledQuad(	ID3D11DeviceContext *pContext,const float *dir,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj,D3DXVECTOR3 sun_dir);
			static void DrawTexture(		ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *t);
			static void DrawTextureMS(		ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *t);
			static void DrawQuad(			ID3D11DeviceContext *pContext,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech);	
			static void DrawQuad2(			ID3D11DeviceContext *pContext,int x1,int y1,int dx,int dy,ID3D1xEffect *eff,ID3D1xEffectTechnique* tech);
			static void DrawQuad2(			ID3D11DeviceContext *pContext,float x1,float y1,float dx,float dy,ID3D1xEffect *eff,ID3D1xEffectTechnique* tech);
			static void DrawQuad(			ID3D11DeviceContext *pContext);
			static void DrawCube(void *context);
			static void DrawSphere(void *context,int latitudes,int longitudes);
			static void DrawCubemap(void *context,ID3D1xShaderResourceView *m_pCubeEnvMapSRV,D3DXMATRIX view,D3DXMATRIX proj,float offsetx,float offsety);
		};
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
				InvalidateDeviceObjects();
				SAFE_RELEASE(m_pD3D11Buffer);	
				D3D11_SUBRESOURCE_DATA			cb_init_data;
				cb_init_data.pSysMem			= this;
				cb_init_data.SysMemPitch		= 0;
				cb_init_data.SysMemSlicePitch	= 0;
				D3D11_BUFFER_DESC				cb_desc;
				cb_desc.Usage					= D3D11_USAGE_DYNAMIC;
				cb_desc.BindFlags				= D3D11_BIND_CONSTANT_BUFFER;
				cb_desc.CPUAccessFlags			= D3D11_CPU_ACCESS_WRITE;
				cb_desc.MiscFlags				= 0;
				cb_desc.ByteWidth				= PAD16(sizeof(T));
				cb_desc.StructureByteStride		= 0;
				pd3dDevice->CreateBuffer(&cb_desc,&cb_init_data, &m_pD3D11Buffer);
				if(m_pD3DX11EffectConstantBuffer)
					m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
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
			//! Apply the stored data using the given context, in preparation for rendering.
			void Apply(ID3D11DeviceContext *pContext)
			{
				D3D11_MAPPED_SUBRESOURCE mapped_res;
				pContext->Map(m_pD3D11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
				*(T*)mapped_res.pData = *this;
				pContext->Unmap(m_pD3D11Buffer, 0);
				if(m_pD3DX11EffectConstantBuffer)
					m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
			}
			//! Unbind from the effect.
			void Unbind(ID3D11DeviceContext *pContext)
			{
				if(m_pD3DX11EffectConstantBuffer)
					m_pD3DX11EffectConstantBuffer->SetConstantBuffer(NULL);
			}
		};

		template<class T> class StructuredBuffer 
		{
		public:
			StructuredBuffer()
				:size(0)
				,buffer(0)
				,shaderResourceView(0)
				,unorderedAccessView(0)
			{
				memset(&mapped,0,sizeof(mapped));
			}
			~StructuredBuffer()
			{
				release();
			}
			void RestoreDeviceObjects(ID3D11Device *pd3dDevice,int s,bool computable=false)
			{
				release();
				size=s;
				D3D11_BUFFER_DESC sbDesc;
				memset(&sbDesc,0,sizeof(sbDesc));
				if(computable)
				{
					sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
					sbDesc.Usage				=D3D11_USAGE_DEFAULT;
					sbDesc.CPUAccessFlags		=0;
				}
				else
				{
					sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE;
					sbDesc.Usage				=D3D11_USAGE_DYNAMIC;
					sbDesc.CPUAccessFlags		=D3D11_CPU_ACCESS_WRITE;
				}
				sbDesc.MiscFlags			=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED ;
				sbDesc.StructureByteStride	=sizeof(T);
				sbDesc.ByteWidth			=sizeof(T) *size;
				V_CHECK(pd3dDevice->CreateBuffer( &sbDesc, NULL, &buffer ));

				D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
				memset(&srv_desc,0,sizeof(srv_desc));
				srv_desc.Format						=DXGI_FORMAT_UNKNOWN;
				srv_desc.ViewDimension				=D3D11_SRV_DIMENSION_BUFFER;
				srv_desc.Buffer.ElementOffset		=0;
				srv_desc.Buffer.ElementWidth		=0;
				srv_desc.Buffer.FirstElement		=0;
				srv_desc.Buffer.NumElements			=size;
				V_CHECK(pd3dDevice->CreateShaderResourceView(buffer, &srv_desc,&shaderResourceView));
				
				if(computable)
				{
					D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
					memset(&uav_desc,0,sizeof(uav_desc));
					uav_desc.Format						=DXGI_FORMAT_UNKNOWN;
					uav_desc.ViewDimension				=D3D11_UAV_DIMENSION_BUFFER;
					uav_desc.Buffer.FirstElement		=0;
					uav_desc.Buffer.Flags				=0;
					uav_desc.Buffer.NumElements			=size;
					V_CHECK(pd3dDevice->CreateUnorderedAccessView(buffer, &uav_desc,&unorderedAccessView));
				}
			}
			T *GetBuffer(ID3D11DeviceContext *pContext)
			{
				lastContext=pContext;
				if(!mapped.pData)
					pContext->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
				T *ptr=(T *)mapped.pData;
				return ptr;
			}
			void apply(ID3D11DeviceContext *pContext,ID3DX11Effect *effect,const char *name)
			{
				if(lastContext&&mapped.pData)
					lastContext->Unmap(buffer,0);
				memset(&mapped,0,sizeof(mapped));
				ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
				var->SetResource(shaderResourceView);
			}
			void release()
			{
				if(lastContext&&mapped.pData)
					lastContext->Unmap(buffer,0);
				SAFE_RELEASE(unorderedAccessView);
				SAFE_RELEASE(shaderResourceView);
				SAFE_RELEASE(buffer);
				size=0;
			}
			ID3D11Buffer						*buffer;
			ID3D11ShaderResourceView			*shaderResourceView;
			ID3D11UnorderedAccessView			*unorderedAccessView;
			D3D11_MAPPED_SUBRESOURCE			mapped;
			int size;
			ID3D11DeviceContext					*lastContext;
		};
	}
}

namespace std
{
	template<> inline void swap(simul::dx11::TextureStruct& _Left, simul::dx11::TextureStruct& _Right)
	{
		std::swap(_Left.shaderResourceView	,_Right.shaderResourceView);
		std::swap(_Left.unorderedAccessView	,_Right.unorderedAccessView);
		std::swap(_Left.renderTargetView	,_Right.renderTargetView);
		std::swap(_Left.stagingBuffer		,_Right.stagingBuffer);
		std::swap(_Left.texture				,_Right.texture);
		std::swap(_Left.width				,_Right.width);
		std::swap(_Left.length				,_Right.length);
		std::swap(_Left.depth				,_Right.depth);
		std::swap(_Left.mapped				,_Right.mapped);
		std::swap(_Left.format				,_Right.format);
	}
	template<class T> inline void swap(simul::dx11::VertexBuffer<T>& _Left, simul::dx11::VertexBuffer<T>& _Right)
	{
		std::swap(_Left.vertexBuffer		,_Right.vertexBuffer);
//		std::swap(_Left.unorderedAccessView	,_Right.unorderedAccessView);
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

#endif //SIMUL_PLATFORM_DIRECTX11_UTILITIES_H
