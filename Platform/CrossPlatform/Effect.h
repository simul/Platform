#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
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
		class PlatformConstantBuffer
		{
		public:
			virtual ~PlatformConstantBuffer(){}
			virtual void RestoreDeviceObjects(void *dev,size_t sz,void *addr)=0;
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
				platformConstantBuffer->RestoreDeviceObjects(p->GetDevice(),sizeof(T),(T*)this);
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
			void *platform_technique;
			inline ID3DX11EffectTechnique *asD3DX11EffectTechnique()
			{
				return (ID3DX11EffectTechnique*)platform_technique;
			}
			inline GLuint asGLuint() const
			{
				return (GLuint)((uintptr_t)platform_technique);
			}
		};
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
			typedef std::map<std::string,EffectTechnique *> TechniqueMap;
			typedef std::map<int,EffectTechnique *> IndexMap;
			TechniqueMap techniques;
			IndexMap techniques_by_index;
		public:
			void *platform_effect;
			Effect();
			virtual ~Effect();
			inline ID3DX11Effect *asD3DX11Effect()
			{
				return (ID3DX11Effect*)platform_effect;
			}
			virtual EffectTechnique *GetTechniqueByName(const char *name)=0;
			virtual EffectTechnique *GetTechniqueByIndex(int index)=0;
			virtual void SetTexture(const char *name,Texture *tex)=0;
			virtual void SetTexture(const char *name,Texture &t)=0;
			virtual void Apply(simul::crossplatform::DeviceContext &deviceContext,EffectTechnique *effectTechnique,int pass)=0;
		};
	}
}
