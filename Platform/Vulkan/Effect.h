#pragma once

#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/Vulkan/PlatformConstantBuffer.h"
#include "Simul/Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Simul/Platform/Vulkan/Shader.h"
#include "Simul/Platform/Vulkan/EffectPass.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
		//! Used internally to bind texture handles:
		class TexHandlesUBO
		{
		public:
						TexHandlesUBO();
						~TexHandlesUBO();
		  

		private:
			void	Release();

			int	 mSlot;

			int size;
		};

		// Vulkan Query implementation
		struct SIMUL_VULKAN_EXPORT Query:public crossplatform::Query
		{
			//GLuint glQuery[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
		//		for(int i=0;i<QueryLatency;i++)
		//			glQuery[i]		=0;
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
			void SetName(const char*) override{}
		};

		//! Vulkan render state holder
		struct SIMUL_VULKAN_EXPORT RenderState:public crossplatform::RenderState
		{
			RenderState(){}
			virtual ~RenderState(){}
		};

		//! Holds Passes
		class SIMUL_VULKAN_EXPORT EffectTechnique:public crossplatform::EffectTechnique
		{
		public:
			EffectTechnique(crossplatform::RenderPlatform *r,crossplatform::Effect *e):crossplatform::EffectTechnique(r,e)
			{
			}
			crossplatform::EffectPass* AddPass(const char *name, int i) override;
		};
		//! The Vulkan implementation of Effect
		class SIMUL_VULKAN_EXPORT Effect:public crossplatform::Effect
		{
		public:
											Effect();
			virtual						 ~Effect();
			void							Load(crossplatform::RenderPlatform* r, const char* filename_utf8, const std::map<std::string, std::string>& defines) override;
			void							Compile(const char *filename_utf8) override;
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index) override;
		
			void							SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const char* name, crossplatform::Texture* tex, int index = -1, int mip = -1)override;
			void							SetUnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::ShaderResource& name, crossplatform::Texture* tex, int index = -1, int mip = -1)override;		
			void							SetConstantBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::ConstantBufferBase* s) override;
			
			void							Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,int pass) override;
			void							Apply(crossplatform::DeviceContext& deviceContext,crossplatform::EffectTechnique* effectTechnique,const char* pass) override;
			void							Reapply(crossplatform::DeviceContext& deviceContext) override;
			void							Unapply(crossplatform::DeviceContext& deviceContext) override;
			void							UnbindTextures(crossplatform::DeviceContext& deviceContext) override;
		
		private:
			EffectTechnique*				CreateTechnique() override;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif