#pragma once
#include <vulkan/vulkan.hpp>
#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/Vulkan/Shader.h"
#include <list>

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
		typedef unsigned long long RenderPassHash;
        //! A Vulkan program object (combination of shaders)
		class SIMUL_VULKAN_EXPORT EffectPass :public simul::crossplatform::EffectPass
		{
		public:
			struct RenderPassPipeline
			{
				vk::Pipeline				mPipeline;
				vk::PipelineCache			mPipelineCache;
				vk::RenderPass				mRenderPass;
			};
			            EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *);
                        ~EffectPass();
			void        InvalidateDeviceObjects();

			void        Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) override;
            void        SetTextureHandles(crossplatform::DeviceContext& deviceContext);

			RenderPassPipeline& GetRenderPassPipeline(crossplatform::GraphicsDeviceContext & deviceContext);

			static RenderPassHash MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology
				, const crossplatform::Layout *layout=nullptr
				, const crossplatform::RenderState *blendState=nullptr
				, const crossplatform::RenderState *depthStencilState=nullptr
				, const crossplatform::RenderState *rasterizerState=nullptr);
			RenderPassHash GetHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology, const crossplatform::Layout* layout);

        private:
			void ApplyContextState(crossplatform::DeviceContext& deviceContext,vk::DescriptorSet &descriptorSet);
			void Initialize();
			void Initialize(vk::DescriptorSet &descriptorSet);
			
			vk::DescriptorSetLayout		mDescLayout;
			vk::PipelineLayout			mPipelineLayout;
	
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
			void InitializePipeline(crossplatform::DeviceContext &deviceContext,RenderPassPipeline *renderPassPipeline,crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology
				, const crossplatform::RenderState *blendState=nullptr
				, const crossplatform::RenderState *depthStencilState=nullptr
				, const crossplatform::RenderState *rasterizerState=nullptr);
			bool initialized=false;
			vk::DescriptorSetLayoutBinding *layout_bindings=nullptr;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif