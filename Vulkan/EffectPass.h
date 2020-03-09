#pragma once
#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/Vulkan/Shader.h"
#include <vulkan/vulkan.hpp>
#include <list>

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

			vk::RenderPass &GetVulkanRenderPass(crossplatform::DeviceContext & deviceContext,crossplatform::PixelFormat pixf,crossplatform::Topology topology);
        private:
			void ApplyContextState(crossplatform::DeviceContext& deviceContext,vk::DescriptorSet &descriptorSet);
			void Initialize();
			void Initialize(vk::DescriptorSet &descriptorSet);
            void MapTexturesToUBO(crossplatform::Effect* curEffect);
			
			vk::DescriptorSetLayout		mDescLayout;
			vk::PipelineLayout			mPipelineLayout;

			struct RenderPassPipeline
			{
				vk::Pipeline				mPipeline;
				vk::PipelineCache			mPipelineCache;
				vk::RenderPass				mRenderPass;
			};
			typedef unsigned long long RenderPassHash;
			static inline RenderPassHash MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology)
			{
				unsigned long long hashval = (unsigned long long)pixelFormat * 1000 + (unsigned long long)topology;
				return hashval;
			}
			std::map<RenderPassHash,RenderPassPipeline> mRenderPasses;
			vk::DescriptorPool			mDescriptorPool;
			
			long long					mLastFrameIndex;
			int							mInternalFrameIndex;	// incremented internally.
			unsigned					mCurApplyCount;
			//! Number of ring buffers
			static const unsigned		kNumBuffers = (SIMUL_VULKAN_FRAME_LAG+1);
			// each frame in the 3-frame loop carries a variable-size list of descriptors: one for each submit.
			std::list<vk::DescriptorSet> mDescriptorSets[kNumBuffers];
			std::list<vk::DescriptorSet>::iterator i_desc[kNumBuffers];
			static int GenerateSamplerSlot(int s,bool offset=true);
			static int GenerateTextureSlot(int s,bool offset=true);
			static int GenerateTextureWriteSlot(int s,bool offset=true);
			static int GenerateConstantBufferSlot(int s,bool offset=true);
			void InitializePipeline(crossplatform::DeviceContext &deviceContext,RenderPassPipeline *renderPassPipeline,crossplatform::PixelFormat pixelFormat, crossplatform::Topology topology);
			bool initialized=false;
			vk::DescriptorSetLayoutBinding *layout_bindings=nullptr;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif