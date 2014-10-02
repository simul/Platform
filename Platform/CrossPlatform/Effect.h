#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include <string>
#include <map>
#include <vector>
#ifndef _MSC_VER
#include <stdint.h>
#endif
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
typedef unsigned int GLuint;
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class RenderPlatform;
		class Effect;
		struct SIMUL_CROSSPLATFORM_EXPORT Query
		{
			static const int QueryLatency = 5;
			bool QueryStarted;
			bool QueryFinished;
			int currFrame;
			QueryType type;
			Query(QueryType t)
				:QueryStarted(false)
				,QueryFinished(false)
				,currFrame(0)
				,type(t)
			{
			}
			virtual ~Query()
			{
			}
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void Begin(DeviceContext &deviceContext) =0;
			virtual void End(DeviceContext &deviceContext) =0;
			virtual void GetData(DeviceContext &deviceContext,void *data,size_t sz) =0;
		};
		
		enum BlendOption
		{
			BLEND_ZERO
			,BLEND_ONE
			,BLEND_SRC_COLOR
			,BLEND_INV_SRC_COLOR
			,BLEND_SRC_ALPHA
			,BLEND_INV_SRC_ALPHA
			,BLEND_DEST_ALPHA
			,BLEND_INV_DEST_ALPHA
			,BLEND_DEST_COLOR
			,BLEND_INV_DEST_COLOR
			,BLEND_SRC_ALPHA_SAT
			,BLEND_BLEND_FACTOR
			,BLEND_INV_BLEND_FACTOR
			,BLEND_SRC1_COLOR
			,BLEND_INV_SRC1_COLOR
			,BLEND_SRC1_ALPHA
			,BLEND_INV_SRC1_ALPHA
		};
		struct RTBlendDesc
		{
			bool BlendEnable;
			BlendOption SrcBlend;
			BlendOption DestBlend;
			BlendOption SrcBlendAlpha;
			BlendOption DestBlendAlpha;
			unsigned char RenderTargetWriteMask;
		};
		struct BlendDesc
		{
			bool AlphaToCoverageEnable;
			bool IndependentBlendEnable;
			int numRTs;
			RTBlendDesc RenderTarget[8];
		};
		enum DepthComparison
		{
			DEPTH_LESS,
			DEPTH_EQUAL,
			DEPTH_LESS_EQUAL,
			DEPTH_GREATER,
			DEPTH_NOT_EQUAL,
			DEPTH_GREATER_EQUAL
		} ;
		struct DepthStencilDesc
		{
			bool test;
			bool write;
			DepthComparison comparison;
		};
		enum RenderStateType
		{
			NONE,BLEND,DEPTH
		};
		/// An initialization structure for a RenderState. Create a RenderStateDesc and pass it to RenderPlatform::CreateRenderState,
		/// then store the returned pointer. Delete the pointer when done.
		struct RenderStateDesc
		{
			RenderStateType type;
			union
			{
				DepthStencilDesc depth;
				BlendDesc blend;
			};
		};
		struct SIMUL_CROSSPLATFORM_EXPORT RenderState
		{
			RenderStateType type;
			RenderState():type(NONE){}
			virtual ~RenderState(){}
		};
		class ConstantBufferBase
		{
		public:
		};
		class SIMUL_CROSSPLATFORM_EXPORT PlatformConstantBuffer
		{
		public:
			virtual ~PlatformConstantBuffer(){}
			virtual void RestoreDeviceObjects(RenderPlatform *dev,size_t sz,void *addr)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)=0;
			virtual void Apply(DeviceContext &deviceContext,size_t size,void *addr)=0;
			virtual void Unbind(DeviceContext &deviceContext)=0;
		};
		template<class T> class ConstantBuffer:public ConstantBufferBase,public T
		{
			PlatformConstantBuffer *platformConstantBuffer;
		public:
			ConstantBuffer():platformConstantBuffer(NULL)
			{
				// Clear out the part of memory that corresponds to the base class.
				// We should ONLY inherit from simple structs.
				memset(((T*)this),0,sizeof(T));
			}
			~ConstantBuffer()
			{
				InvalidateDeviceObjects();
				delete platformConstantBuffer;
			}
			void copyTo(void *pData)
			{
				*(T*)pData = *this;
			}
			//! Create the buffer object.
			void RestoreDeviceObjects(RenderPlatform *p)
			{
				InvalidateDeviceObjects();
				if(p)
				{
					platformConstantBuffer=p->CreatePlatformConstantBuffer();
					platformConstantBuffer->RestoreDeviceObjects(p,sizeof(T),(T*)this);
				}
			}
			//! Find the constant buffer in the given effect, and link to it.
			void LinkToEffect(Effect *effect,const char *name)
			{
				platformConstantBuffer->LinkToEffect(effect,name,T::bindingIndex);
			}
			//! Free the allocated buffer.
			void InvalidateDeviceObjects()
			{
				if(platformConstantBuffer)
					platformConstantBuffer->InvalidateDeviceObjects();
				delete platformConstantBuffer;
				platformConstantBuffer=NULL;
			}
			//! Apply the stored data using the given context, in preparation for rendering.
			void Apply(DeviceContext &deviceContext)
			{
				if(platformConstantBuffer)
					platformConstantBuffer->Apply(deviceContext,sizeof(T),(T*)this);
			}
			//! Unbind from the effect.
			void Unbind(DeviceContext &deviceContext)
			{
				if(platformConstantBuffer)
					platformConstantBuffer->Unbind(deviceContext);
			}
		};
		/// A base class for structured buffers, used by StructuredBuffer internally.
		class SIMUL_CROSSPLATFORM_EXPORT PlatformStructuredBuffer
		{
		public:
			virtual ~PlatformStructuredBuffer(){}
			virtual void RestoreDeviceObjects(RenderPlatform *r,int count,int unit_size,bool computable,void *init_data)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)=0;
			virtual void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name)=0;
			virtual void Unbind(DeviceContext &deviceContext)=0;
			virtual void *GetBuffer(crossplatform::DeviceContext &deviceContext)=0;
			virtual void SetData(crossplatform::DeviceContext &deviceContext,void *data)=0;
			virtual ID3D11ShaderResourceView *AsD3D11ShaderResourceView()=0;
			virtual ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()=0;
		};

		/// Templated structured buffer, which uses platform-specific implementations of PlatformStructuredBuffer.
		///
		/// Declare like so:
		/// \code
		/// 	StructuredBuffer<Example> example;
		/// \endcode
		template<class T> class StructuredBuffer 
		{
			PlatformStructuredBuffer *platformStructuredBuffer;
		public:
			StructuredBuffer()
				:platformStructuredBuffer(NULL)
				,count(0)
			{
			}
			~StructuredBuffer()
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(RenderPlatform *p,int ct,bool computable=false,T *data=NULL)
			{
				count=ct;
//				unit_size=sizeof(T);
				SAFE_DELETE(platformStructuredBuffer);
				platformStructuredBuffer=p->CreatePlatformStructuredBuffer();
				platformStructuredBuffer->RestoreDeviceObjects(p,count,sizeof(T),computable,data);
			}
			T *GetBuffer(crossplatform::DeviceContext &deviceContext)
			{
				return (T*)platformStructuredBuffer->GetBuffer(deviceContext);
			}
			void SetData(crossplatform::DeviceContext &deviceContext,T *data)
			{
				platformStructuredBuffer->SetData(deviceContext,(void*)data);
			}
			ID3D11ShaderResourceView *AsD3D11ShaderResourceView()
			{
				return platformStructuredBuffer->AsD3D11ShaderResourceView();
			}
			ID3D11UnorderedAccessView *AsD3D11UnorderedAccessView()
			{
				return platformStructuredBuffer->AsD3D11UnorderedAccessView();
			}
			void Apply(crossplatform::DeviceContext &pContext,crossplatform::Effect *effect,const char *name)
			{
				platformStructuredBuffer->Apply(pContext,effect,name);
			}
			void InvalidateDeviceObjects()
			{
				if(platformStructuredBuffer)
					platformStructuredBuffer->InvalidateDeviceObjects();
				SAFE_DELETE(platformStructuredBuffer);
				count=0;
			}
			int count;
		};
		class Texture;
		class SamplerState;
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechnique
		{
		public:
			typedef std::map<std::string,void *> PassMap;
			typedef std::map<int,void *> PassIndexMap;
			typedef std::map<std::string,int> IndexMap;
			PassMap passes_by_name;
			PassIndexMap passes_by_index;
			IndexMap pass_indices;
			EffectTechnique()
				:platform_technique(NULL)
			{
			}
			virtual int NumPasses() const=0;
			void *platform_technique;
			inline ID3DX11EffectTechnique *asD3DX11EffectTechnique()
			{
				return (ID3DX11EffectTechnique*)platform_technique;
			}
			inline GLuint passAsGLuint(int p)
			{
				return (GLuint)((uintptr_t)passes_by_index[p]);
			}
			inline GLuint passAsGLuint(const char *name)
			{
				return (GLuint)((uintptr_t)(passes_by_name[std::string(name)]));
			}
			inline int GetPassIndex(const char *name)
			{
				std::string str(name);
				if(pass_indices.find(str)==pass_indices.end())
					return -1;
				return pass_indices[str];
			}
		};
		typedef std::map<std::string,EffectTechnique *> TechniqueMap;
		typedef std::map<int,EffectTechnique *> IndexMap;
		/// Crossplatform equivalent of D3DXEffectGroup - a named group of techniques.
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechniqueGroup
		{
		public:
			TechniqueMap techniques;
			IndexMap techniques_by_index;
			EffectTechnique *GetTechniqueByName(const char *name);
			EffectTechnique *GetTechniqueByIndex(int index);
		};
		/// A crossplatform structure for a \#define and its possible values.
		/// This allows all of the macro combinations to be built to binary.
		struct SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions
		{
			std::string name;
			std::vector<std::string> options;
		};
		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1);
		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1,const char *option2);
		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1,const char *option2,const char *option3);
		typedef std::map<std::string,EffectTechniqueGroup *> GroupMap;
		/// The cross-platform base class for shader effects.
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
		public:
			GroupMap groups;
			TechniqueMap techniques;
			IndexMap techniques_by_index;
			std::string filename;
			std::string filenameInUseUtf8;
			int apply_count;
			int currentPass;
			crossplatform::EffectTechnique *currentTechnique;
			void *platform_effect;
			Effect();
			virtual ~Effect();
			inline ID3DX11Effect *asD3DX11Effect()
			{
				return (ID3DX11Effect*)platform_effect;
			}
			void SetName(const char *n)
			{
				filename=n;
			}
			const char *GetName()const
			{
				return filename.c_str();
			}
			virtual void Load(RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines)=0;
			EffectTechniqueGroup *GetTechniqueGroupByName(const char *name);
			virtual EffectTechnique *GetTechniqueByName(const char *name)		=0;
			virtual EffectTechnique *GetTechniqueByIndex(int index)				=0;
			virtual void SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext,const char *name,Texture *tex)	=0;
			virtual void SetTexture		(crossplatform::DeviceContext &deviceContext,const char *name	,Texture *tex)		=0;
			virtual void SetTexture		(crossplatform::DeviceContext &deviceContext,const char *name	,Texture &t)		=0;
			virtual void SetSamplerState(crossplatform::DeviceContext &deviceContext,const char *name	,SamplerState *s)		=0;
			virtual void SetParameter	(const char *name	,float value)		=0;
			virtual void SetParameter	(const char *name	,vec2)				=0;
			virtual void SetParameter	(const char *name	,vec3)				=0;
			virtual void SetParameter	(const char *name	,vec4)				=0;
			virtual void SetParameter	(const char *name	,int value)			=0;
			virtual void SetParameter	(const char *name	,int2 value)		=0;
			virtual void SetVector		(const char *name	,const float *vec)	=0;
			virtual void SetMatrix		(const char *name	,const float *m)	=0;
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,int pass)=0;
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,const char *pass)=0;
			/// Call Reapply between Apply and Unapply to apply the effect of modified constant buffers etc.
			virtual void Reapply(DeviceContext &deviceContext)=0;
			/// Deactivate the shader.
			virtual void Unapply(DeviceContext &deviceContext)=0;
			/// Zero-out the textures that are set for this shader. Call before apply.
			virtual void UnbindTextures(crossplatform::DeviceContext &deviceContext)=0;
		};
	}
}
