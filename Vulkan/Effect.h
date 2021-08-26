#pragma once
#include <vulkan/vulkan.hpp>

#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Vulkan/PlatformConstantBuffer.h"
#include "Platform/Vulkan/PlatformStructuredBuffer.h"
#include "Platform/Vulkan/Shader.h"
#include "Platform/Vulkan/EffectPass.h"

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

			//int	 mSlot;

			//int size;
		};

		// Vulkan Query implementation
		struct SIMUL_VULKAN_EXPORT Query:public crossplatform::Query
		{
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				queryPoolCI.pNext = nullptr;
				queryPoolCI.flags = (vk::QueryPoolCreateFlags)0;
				queryPoolCI.queryType = toVkQueryType(t);
				queryPoolCI.queryCount = crossplatform::Query::QueryLatency;
				queryPoolCI.pipelineStatistics = (vk::QueryPipelineStatisticFlags)0;
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
			void SetName(const char*) override;

		private:
			vk::Device* mDevice = nullptr;
			vk::QueryPool mQueryPool = nullptr;
			vk::QueryPoolCreateInfo queryPoolCI;

			vk::QueryType toVkQueryType(crossplatform::QueryType t);
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
			void							Load(crossplatform::RenderPlatform* r, const char* filename_utf8) override;
			void							Compile(const char *filename_utf8) override;
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index) override;
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