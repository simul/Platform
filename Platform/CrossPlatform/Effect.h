#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include <string>
#include <map>
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
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
				platformConstantBuffer=p->CreatePlatformConstantBuffer();
				platformConstantBuffer->RestoreDeviceObjects(p,sizeof(T),(T*)this);
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
				platformConstantBuffer->Apply(deviceContext,sizeof(T),(T*)this);
			}
			//! Unbind from the effect.
			void Unbind(DeviceContext &deviceContext)
			{
				platformConstantBuffer->Unbind(deviceContext);
			}
		};
		class Texture;
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
				return pass_indices[std::string(name)];
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
		typedef std::map<std::string,EffectTechniqueGroup *> GroupMap;
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
		public:
			GroupMap groups;
			TechniqueMap techniques;
			IndexMap techniques_by_index;
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
			EffectTechniqueGroup *GetTechniqueGroupByName(const char *name);
			virtual EffectTechnique *GetTechniqueByName(const char *name)		=0;
			virtual EffectTechnique *GetTechniqueByIndex(int index)				=0;
			virtual void SetUnorderedAccessView(const char *name,Texture *tex)	=0;
			virtual void SetTexture		(const char *name	,Texture *tex)		=0;
			virtual void SetTexture		(const char *name	,Texture &t)		=0;
			virtual void SetParameter	(const char *name	,float value)		=0;
			virtual void SetParameter	(const char *name	,vec2)				=0;
			virtual void SetParameter	(const char *name	,vec3)				=0;
			virtual void SetParameter	(const char *name	,vec4)				=0;
			virtual void SetParameter	(const char *name	,int value)			=0;
			virtual void SetVector		(const char *name	,const float *vec)	=0;
			virtual void SetMatrix		(const char *name	,const float *m)	=0;
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,int pass)=0;
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,const char *pass)=0;
			virtual void Unapply(DeviceContext &deviceContext)=0;
		};
	}
}
