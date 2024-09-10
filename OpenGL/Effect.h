#pragma once

#include "Platform/OpenGL/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "glad/glad.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace platform
{
	namespace opengl
	{
		//! Used internally to bind texture handles:
		class TexHandlesUBO
		{
		public:
						TexHandlesUBO();
						~TexHandlesUBO();
			void		Init(size_t count, GLuint program, int index, int slot);
			void		Bind(GLuint program);
			void		Update(GLuint64 value,size_t offset);

			

		private:
			void	Release();

			GLuint	mId;
			int	 mSlot;

			int size;
		};

		// Opengl Query implementation
		struct SIMUL_OPENGL_EXPORT Query:public crossplatform::Query
		{
			GLuint glQuery[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				for (int i = 0; i < QueryLatency; i++)
				{
					glQuery[i] = 0; 
				}

				glGenQueries(crossplatform::Query::QueryLatency, glQuery);
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
				PlatformConstantBuffer(crossplatform::ResourceUsageFrequency F);
				~PlatformConstantBuffer();
			void RestoreDeviceObjects(crossplatform::RenderPlatform* r,size_t sz,void* addr);
			void InvalidateDeviceObjects();
			void LinkToEffect(crossplatform::Effect* effect,const char* name,int bindingIndex);
			void Apply(platform::crossplatform::DeviceContext& deviceContext,size_t size,void* addr);
			void Unbind(platform::crossplatform::DeviceContext& deviceContext);

		private:
			GLuint		mUBOId;
			int		 mBindingSlot;
		};

		//! OpenGL Structured Buffer implementation (SSBO)
		class PlatformStructuredBuffer :public crossplatform::PlatformStructuredBuffer
		{
		public:
			PlatformStructuredBuffer();
			virtual	 ~PlatformStructuredBuffer();

			void		RestoreDeviceObjects(crossplatform::RenderPlatform* r, int count, int unit_size, bool computable, bool cpu_read, void* init_data, const char* name, crossplatform::ResourceUsageFrequency b);
			void*		GetBuffer(crossplatform::DeviceContext& deviceContext);
			const void* OpenReadBuffer(crossplatform::DeviceContext& deviceContext);
			void		CloseReadBuffer(crossplatform::DeviceContext& deviceContext);
			void		CopyToReadBuffer(crossplatform::DeviceContext& deviceContext);
			void		SetData(crossplatform::DeviceContext& deviceContext, void* data);
			void		InvalidateDeviceObjects();

			void		Apply(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& shaderResource);
			void		ApplyAsUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& shaderResource);

			void		AddFence(crossplatform::DeviceContext& deviceContext);

			void		Unbind(crossplatform::DeviceContext& deviceContext);

		private:
			static const int	mNumBuffers = 3;
			int					mLastIdx = 0;
			GLuint				mGPUBuffer[mNumBuffers];
			size_t				mTotalSize;
			int					mBinding;
			void*				mCurReadMap = nullptr;
			void*				mMappedWritePtr = nullptr;
			GLsync				mFences[mNumBuffers];

			inline bool IsBufferMapped(size_t idx)
			{
				GLint mapped;
				glGetNamedBufferParameteriv(mGPUBuffer[idx], GL_BUFFER_MAPPED, &mapped);
				return mapped == GL_TRUE;
			}

			int GetIndex(crossplatform::DeviceContext& deviceContext, int idxOffset = 0);
		};

		//! An OpenGl program object (combination of shaders)
		class SIMUL_OPENGL_EXPORT EffectPass :public platform::crossplatform::EffectPass
		{
		public:
						EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e);
						~EffectPass();
			void		InvalidateDeviceObjects();

			void		Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) override;
			void		SetTextureHandles(crossplatform::DeviceContext& deviceContext);
			GLuint		GetGLId();

		private:
			//! SFX will generate an UBO with arrays of handles, the idea is to map the slot
			//! provided by .sfxo and map it to the actual location within the UBO so we 
			//! avoid doing string checks
			//! test.sfxo
			//!	 pass CS_Godrays.glsl, t:(3), c(12) etc
			//!								^ This maps to pseudo slot number 3
			//!	 uniform _TextureHandles_
			//!	 {
			//!		 uint64_t godraysTexture[16];	... but the texture is in the UBO member = 0!
			//!	 };
			//!	 int slotsmTexturesUBOMappingMap[32] { -1,-1,-1,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
			//!													^ maps to UBO slot 0! so slotsMap[slot] IS the UBO offset!
			//! NOTE: we use int as we store byte offsets (we could calc a "unit" off as it should 
			//!		only store uint64t)
			void MapTexturesToUBO(crossplatform::Effect* curEffect);

			GLuint											mProgramId;
			int											 mTexturesUBOMapping[crossplatform::ShaderType::SHADERTYPE_COUNT][32];
			TexHandlesUBO*									mHandlesUBO[crossplatform::ShaderType::SHADERTYPE_COUNT];
			std::vector<std::string> mUsedTextures;
		};

		//! Holds Passes
		class SIMUL_OPENGL_EXPORT EffectTechnique:public crossplatform::EffectTechnique
		{
		public:
			EffectTechnique(crossplatform::RenderPlatform *r,crossplatform::Effect *e):crossplatform::EffectTechnique(r,e)
			{
			}
			crossplatform::EffectPass* AddPass(const char *name, int i) override;
		};

		//! An OpenGl shader
		class SIMUL_OPENGL_EXPORT Shader:public platform::crossplatform::Shader
		{
		public:
					Shader();
					~Shader();
					bool load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *data, size_t len, crossplatform::ShaderType t) override;
			
			GLuint	ShaderId;
		private: 
			void	Release();
			//temp:
			std::string src;
		};

		//! The OpenGL implementation of Effect
		class SIMUL_OPENGL_EXPORT Effect:public crossplatform::Effect
		{
		public:
											Effect();
											~Effect();
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index);
		
			void							Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,int pass);
			void							Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,const char* pass);
			void							Reapply(crossplatform::DeviceContext& deviceContext);
			void							Unapply(crossplatform::DeviceContext& deviceContext);
			void							UnbindTextures(crossplatform::DeviceContext& deviceContext);
		
		private:
			EffectTechnique*				CreateTechnique();
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif