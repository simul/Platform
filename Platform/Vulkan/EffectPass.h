#pragma once
#include "Simul/Platform/Vulkan/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/Vulkan/Shader.h"
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
        //! A Vulkan program object (combination of shaders)
		class SIMUL_VULKAN_EXPORT EffectPass :public simul::crossplatform::EffectPass
		{
		public:
			            EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *);
                        ~EffectPass();
			void        InvalidateDeviceObjects();

			void        Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) override;
            void        SetTextureHandles(crossplatform::DeviceContext& deviceContext);

            std::string PassName;

			vk::Pipeline &GetVulkanPipeline()
			{
				return mPipeline;
			}
        private:
			void ApplyContextState(crossplatform::DeviceContext& deviceContext,vk::DescriptorSet &descriptorSet);
			void Initialize(vk::DescriptorSet &descriptorSet);
            void MapTexturesToUBO(crossplatform::Effect* curEffect);
			
			vk::DescriptorSetLayout		mDescLayout;
			vk::PipelineLayout			mPipelineLayout;
			vk::Pipeline				mPipeline;
			vk::PipelineCache			mPipelineCache;
			vk::RenderPass				mRenderPass;
			vk::DescriptorPool			mDescriptorPool;
			
			UINT						mLastFrameIndex;
			UINT						mCurApplyCount;
			//! Number of ring buffers
			static const UINT			kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			// each frame in the 3-frame loop carries a variable-size list of descriptors: one for each submit.
			std::list<vk::DescriptorSet> mDescriptorSets[kNumBuffers];
			std::list<vk::DescriptorSet>::iterator i_desc;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif