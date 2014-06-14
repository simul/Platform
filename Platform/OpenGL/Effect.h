#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
typedef unsigned GLuint;
namespace simul
{
	namespace opengl
	{
		// Platform-specific data for constant buffer, managed by RenderPlatform.
		class PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			GLuint	ubo;
			size_t size;
		public:
			PlatformConstantBuffer():ubo(0)
			{
			}
			void RestoreDeviceObjects(void *dev,size_t sz,void *addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			void Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void Unbind(simul::crossplatform::DeviceContext &deviceContext);
		};
		class Effect:public crossplatform::Effect
		{
			GLint effect;
		public:
			Effect();
			~Effect();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
			void SetTexture(const char *name,crossplatform::Texture *tex);
			void SetTexture(const char *name,crossplatform::Texture &t);
		};
	}
}
