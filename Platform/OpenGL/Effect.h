#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
typedef unsigned GLuint;
namespace simul
{
	namespace opengl
	{
		// Platform-specific data for constant buffer, managed by RenderPlatform.
		class SIMUL_OPENGL_EXPORT PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
			GLuint	ubo;
			size_t size;
			int bindingIndex;
		public:
			PlatformConstantBuffer():ubo(0)
			{
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *dev,size_t sz,void *addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex);
			void Apply(simul::crossplatform::DeviceContext &deviceContext,size_t size,void *addr);
			void Unbind(simul::crossplatform::DeviceContext &deviceContext);
		};
		class SIMUL_OPENGL_EXPORT PassState
		{
		public:
			virtual void Apply()=0;
		};
		class SIMUL_OPENGL_EXPORT EffectTechnique:public crossplatform::EffectTechnique
		{
		public:
			typedef std::map<int,PassState *> PassStateMap;
			PassStateMap passStates;
			int NumPasses() const;
		};
		class SIMUL_OPENGL_EXPORT Effect:public crossplatform::Effect
		{
			int current_texture_number;
			void FillInTechniques();
		public:
			Effect(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			~Effect();
			inline GLuint asGLint() const
			{
				return (GLint)(uintptr_t)platform_effect;
			}
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
			void SetUnorderedAccessView(crossplatform::DeviceContext&,const char *name,crossplatform::Texture *tex);
			void SetTexture		(crossplatform::DeviceContext&,const char *name	,crossplatform::Texture *tex);
			void SetTexture		(crossplatform::DeviceContext&,const char *name	,crossplatform::Texture &t);
			void SetParameter	(const char *name	,float value)		;
			void SetParameter	(const char *name	,vec2)				;
			void SetParameter	(const char *name	,vec3)				;
			void SetParameter	(const char *name	,vec4)				;
			void SetParameter	(const char *name	,int value)			;
			void SetVector		(const char *name	,const float *vec)	;
			void SetMatrix		(const char *name	,const float *m)	;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass);
			void Unapply(crossplatform::DeviceContext &deviceContext);
		};
	}
}
