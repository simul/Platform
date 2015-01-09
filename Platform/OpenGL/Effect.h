#pragma once
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
typedef unsigned GLuint;
namespace simul
{
	namespace opengl
	{
		struct SIMUL_OPENGL_EXPORT Query:public crossplatform::Query
		{
			GLuint glQuery[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				for(int i=0;i<QueryLatency;i++)
					glQuery[i]		=0;
			}
			~Query()
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;
			void Begin(crossplatform::DeviceContext &deviceContext) override;
			void End(crossplatform::DeviceContext &deviceContext) override;
			void GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
		};
		struct SIMUL_OPENGL_EXPORT RenderState:public crossplatform::RenderState
		{
			crossplatform::RenderStateDesc desc;
			RenderState()
			{
			}
			virtual ~RenderState()
			{
			}
		};
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
			GLuint passAsGLuint(int p) override;
			GLuint passAsGLuint(const char *name) override;
		};
		/// The OpenGL implementation of simul::crossplatform::Effect.
		class SIMUL_OPENGL_EXPORT Effect:public crossplatform::Effect
		{
			GLuint current_prog;
			int current_texture_number;
			/// We keep a map of texture names to the arbitrary GL_TEXTUREn indices that
			/// we've assigned them to.
			std::map<std::string,int> textureNumberMap;
			std::map<GLuint,GLuint> prepared_sampler_states;
			bool FillInTechniques();
			void SetTex(const char *name,crossplatform::Texture *tex,bool write,int mip);
			EffectTechnique *CreateTechnique();
			void AddPass(std::string techname, std::string passname, GLuint t);
		public:
			Effect();
			~Effect();
			inline GLuint asGLint() const
			{
				return (GLint)(uintptr_t)platform_effect;
			}
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);
			void SetUnorderedAccessView(crossplatform::DeviceContext&,const char *name,crossplatform::Texture *tex,int mip=0);
			void SetTexture		(crossplatform::DeviceContext&,const char *name	,crossplatform::Texture *tex);
			void SetTexture		(crossplatform::DeviceContext&,const char *name	,crossplatform::Texture &t);
			void SetSamplerState(crossplatform::DeviceContext&,const char *name	,crossplatform::SamplerState *s);
			void SetParameter	(const char *name	,float value)		;
			void SetParameter	(const char *name	,vec2)				;
			void SetParameter	(const char *name	,vec3)				;
			void SetParameter	(const char *name	,vec4)				;
			void SetParameter	(const char *name	,int value)			;
			void SetParameter	(const char *name	,int2)				;
			void SetVector		(const char *name	,const float *vec)	;
			void SetMatrix		(const char *name	,const float *m)	;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass);
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass);
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif