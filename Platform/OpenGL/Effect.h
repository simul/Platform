#pragma once

#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"

#include "GL/glew.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace opengl
	{
        /*
        struct TextureComb
        {
            std::string Sampler;
            GLint       Offset; //! In bytes
        };
        */

        //! Used internally to bind texture handles:
        class TexHandlesUBO
        {
        public:
                        TexHandlesUBO();
                        ~TexHandlesUBO();
            void        Init(size_t count, GLuint program, int slot);
            void        Bind(GLuint program);
            void        Update(GLuint64 value,size_t offset);

            const char* Name = "_TextureHandles_";

        private:
            void    Release();

            GLuint  mId;
            int     mSlot;
        };

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
			bool GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void SetName(const char*){}
		};

        //! OpenGL render state holder
		struct SIMUL_OPENGL_EXPORT RenderState:public crossplatform::RenderState
		{
			crossplatform::RenderStateDesc desc;
			RenderState(){}
			virtual ~RenderState(){}
		};

		//! OpenGL Constant Buffer implementation (UBO)
		class SIMUL_OPENGL_EXPORT PlatformConstantBuffer : public crossplatform::PlatformConstantBuffer
		{
		public:
                PlatformConstantBuffer();
                ~PlatformConstantBuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void* addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex);
			void Apply(simul::crossplatform::DeviceContext& deviceContext,size_t size,void* addr);
			void Unbind(simul::crossplatform::DeviceContext& deviceContext);

        private:
            GLuint      mUBOId;
            int         mBindingSlot;
		};

        //! OpenGL Structured Buffer implementation (SSBO)
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
			GLuint								ssbo;
			int num_elements;
			int element_bytesize;
			unsigned char *read_data;
			unsigned char *write_data;
		public:
			PlatformStructuredBuffer();
			virtual ~PlatformStructuredBuffer()
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r, int count, int unit_size, bool computable, bool cpu_read, void *init_data);
			void *GetBuffer(crossplatform::DeviceContext &deviceContext);
			const void *OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			void CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			void CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void SetData(crossplatform::DeviceContext &deviceContext,void *data);
			GLuint AsGLuint() const
			{
				return ssbo;
			}
			void InvalidateDeviceObjects();
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name);
			void ApplyAsUnorderedAccessView(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,const char *name);
			void Unbind(crossplatform::DeviceContext &deviceContext);
		};

        //! An OpenGl program object (combination of shaders)
		class SIMUL_OPENGL_EXPORT EffectPass :public simul::crossplatform::EffectPass
		{
		public:
			        EffectPass();
                    ~EffectPass();
			void    InvalidateDeviceObjects();
			void    Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) override;
            void    SetTextureHandles(crossplatform::DeviceContext& deviceContext);
            GLuint  GetGLId();

            std::string PassName;
        private:
            GLuint                                          mProgramId;
            // std::map<std::string, std::vector<TextureComb>> mTextureMap;
            std::map<std::string,int>                       mTextureSet;
            TexHandlesUBO*                                  mHandlesUBO;
		};

        //! Holds Passes
		class SIMUL_OPENGL_EXPORT EffectTechnique:public crossplatform::EffectTechnique
		{
		public:
			crossplatform::EffectPass* AddPass(const char *name, int i) override;
		};

        //! An OpenGl shader
		class SIMUL_OPENGL_EXPORT Shader:public simul::crossplatform::Shader
		{
		public:
            Shader();
            ~Shader();
            void load(crossplatform::RenderPlatform *renderPlatform, const char *filename, crossplatform::ShaderType t) override;
        
            GLuint ShaderId;
		};

		//! The OpenGL implementation of Effect
		class SIMUL_OPENGL_EXPORT Effect:public crossplatform::Effect
		{
			EffectTechnique *CreateTechnique();
		public:
			Effect();
			~Effect();
            virtual crossplatform::ShaderResource GetShaderResource(const char *name)override;
			void Load(crossplatform::RenderPlatform* renderPlatform,const char* filename_utf8,const std::map<std::string,std::string>& defines);
			crossplatform::EffectTechnique* GetTechniqueByName(const char* name);
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index);
		
            void SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index = -1, int mip = -1)override;
            void SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index = -1, int mip = -1)override;

            void SetTexture(crossplatform::DeviceContext& deviceContext,crossplatform::ShaderResource& shaderResource,crossplatform::Texture* t,int index=-1,int mip=-1) override;
			void SetTexture(crossplatform::DeviceContext& deviceContext,const char* name,crossplatform::Texture* tex,int index=-1,int mip=-1) override;
		
            void SetConstantBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::ConstantBufferBase* s)override;
			
            void Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,int pass);
			void Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,const char* pass);
			void Reapply(crossplatform::DeviceContext& deviceContext);
			void Unapply(crossplatform::DeviceContext& deviceContext);
			void UnbindTextures(crossplatform::DeviceContext& deviceContext);
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif