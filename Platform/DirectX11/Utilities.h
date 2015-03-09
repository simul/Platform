#pragma once
#ifndef SIMUL_PLATFORM_DIRECTX11_UTILITIES_H
#define SIMUL_PLATFORM_DIRECTX11_UTILITIES_H

#ifdef _XBOX_ONE
#include <d3d11_x.h>
#else
#include "SimulDirectXHeader.h"
#endif
#include <utility>
#include <vector>
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Base/FileLoader.h"
#include "Simul/Math/Matrix4x4.h"
struct ID3DX11EffectConstantBuffer;
#pragma warning(disable:4251)
namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class RenderPlatform;
	}
	namespace dx11
	{
		extern bool IsTypeless(DXGI_FORMAT fmt, bool partialTypeless);
		extern DXGI_FORMAT TypelessToSrvFormat(DXGI_FORMAT fmt);
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
			void create(ID3D11Device *pd3dDevice,const std::vector<std::string> &texture_files,const std::vector<std::string> &pathsUtf8);
			void create(ID3D11Device *pd3dDevice,int w,int l,int num,DXGI_FORMAT f,bool computable);
			ID3D11Texture2D*					m_pArrayTexture;
			ID3D11ShaderResourceView*			m_pArrayTexture_SRV;
			ID3D11UnorderedAccessView*			unorderedAccessView;
		};
		//! A vertex buffer wrapper class for arbitrary vertex types.
		template<class T> struct VertexBuffer
		{
			ID3D11Buffer				*vertexBuffer;
			
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
			void ensureBufferSize(ID3D11Device *pd3dDevice,int numVertices,void *data=NULL
				,bool stream_output=true,bool cpu_write=false)
			{
				release();
				D3D11_BUFFER_DESC desc	=
				{
					numVertices*sizeof(T),
					cpu_write?D3D11_USAGE_DYNAMIC:D3D11_USAGE_DEFAULT,
					D3D11_BIND_VERTEX_BUFFER|(stream_output?D3D11_BIND_STREAM_OUTPUT:0)//D3D11_BIND_UNORDERED_ACCESS is useless for VB's in DX11
					,(cpu_write?D3D11_CPU_ACCESS_WRITE:0)// CPU
					,0//D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
					,sizeof(T)			//StructureByteStride
				};
				SAFE_RELEASE(vertexBuffer);
				D3D11_SUBRESOURCE_DATA init;
				init.pSysMem			=data;
				init.SysMemPitch		=sizeof(T);
				init.SysMemSlicePitch	=0;
				V_CHECK(pd3dDevice->CreateBuffer(&desc,data?(&init):NULL,&vertexBuffer));
			}
			D3D11_MAPPED_SUBRESOURCE mapped;
			T *Map(ID3D11DeviceContext *pContext)
			{
				HRESULT hr=pContext->Map(vertexBuffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
				return (T*)mapped.pData;
			}
			void Unmap(ID3D11DeviceContext *pContext)
			{
				pContext->Unmap(vertexBuffer,0);
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
		inline void cancelStreamOutTarget(ID3D11DeviceContext *pContext)
		{
			ID3D11Buffer *pBuffer =NULL;
			UINT offset=0;
			pContext->SOSetTargets(1,&pBuffer,&offset);
		}
		class SIMUL_DIRECTX11_EXPORT UtilityRenderer
		{
			static int instance_count;
		public:
			UtilityRenderer();
			~UtilityRenderer();
			static void RenderAngledQuad(	crossplatform::DeviceContext &deviceContext,const float *dir,float half_angle_radians,ID3DX11Effect* effect,ID3DX11EffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj,D3DXVECTOR3 sun_dir);
			static void DrawQuad(			crossplatform::DeviceContext &deviceContext,float x1,float y1,float dx,float dy,ID3DX11EffectTechnique* tech);	
			static void DrawQuad2(			crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3DX11Effect *eff,ID3DX11EffectTechnique* tech,int pass=0);
			static void DrawQuad2(			crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3DX11Effect *eff,ID3DX11EffectTechnique* tech,const char *pass=NULL);
			static void DrawQuad2(			crossplatform::DeviceContext &deviceContext, float x1, float y1, float dx, float dy, ID3DX11Effect *eff, ID3DX11EffectTechnique* tech, int pass );
			static void DrawQuad2(			crossplatform::DeviceContext &deviceContext, float x1, float y1, float dx, float dy, ID3DX11Effect *eff, ID3DX11EffectTechnique* tech, const char *pass );
			static void DrawQuad(			crossplatform::DeviceContext &deviceContext);
			static void DrawSphere(			crossplatform::DeviceContext &deviceContext,int latitudes,int longitudes);
		};
		//! Useful Wrapper class to encapsulate constant buffer behaviour
		
		//! Usage: You \em must use the following member functions to use this buffer.
		//! On device restore, call RestoreDeviceObjects(m_pd3dDevice).
		//! After creating any effect that uses the buffer, call LinkToEffect(m_pSkyEffect,"BufferNameInFxFile").
		//! On device shutdown, call InvalidateDeviceObjects().
		//! Before applying the effect pass, call Apply(pContext)
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
			void Apply(simul::crossplatform::DeviceContext &deviceContext)
			{
				ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.platform_context;
				Apply(pContext);
			}
			//! Unbind from the effect.
			void Unbind(ID3D11DeviceContext *)
			{
				if(m_pD3DX11EffectConstantBuffer)
					m_pD3DX11EffectConstantBuffer->SetConstantBuffer(NULL);
			}
		protected:
			void Apply(ID3D11DeviceContext *pContext)
			{
				D3D11_MAPPED_SUBRESOURCE mapped_res;
				if(pContext->Map(m_pD3D11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res)!=S_OK)
					return;
				*(T*)mapped_res.pData = *this;
				pContext->Unmap(m_pD3D11Buffer, 0);
				if(m_pD3DX11EffectConstantBuffer)
					m_pD3DX11EffectConstantBuffer->SetConstantBuffer(m_pD3D11Buffer);
			}
		};
		inline void SetDebugObjectName( ID3D11DeviceChild* resource,const char *name)
		{
		  #if (defined(_DEBUG) || defined(PROFILE)) && !defined(_XBOX_ONE)
			if(resource)
				resource->SetPrivateData(WKPDID_D3DDebugObjectName,(UINT)(name?strlen(name):0),name?name:"un-named resource");
			#else
			name;resource;
		  #endif
		}
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
				sbDesc.BindFlags			=D3D11_BIND_SHADER_RESOURCE ;
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
