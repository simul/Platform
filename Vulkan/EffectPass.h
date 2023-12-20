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

namespace platform
{
	namespace vulkan
	{
		typedef unsigned long long RenderPassHash;

		class SIMUL_VULKAN_EXPORT EffectPass : public platform::crossplatform::EffectPass
		{
		public:
			struct RenderPassPipeline
			{
				vk::Pipeline		pipeline;
				vk::PipelineCache	pipelineCache;
				vk::RenderPass		renderPass;
			};

		public:
			EffectPass(crossplatform::RenderPlatform* r, crossplatform::Effect* e);
			~EffectPass();
			void InvalidateDeviceObjects();

			void Apply(crossplatform::DeviceContext& deviceContext, bool asCompute) override;

			//! For Vulkan alone, this forces the shader to initialize with the source texture as a video decode source.
			//! This MUST be set to true before the first use of the shader, or it won't be applied to the shader's initialization.
			void SetVideoSource(bool s) { m_VideoSource = s; }

			// TODO: this is inappropriate here. Latest created PipelineLayout.
			inline const vk::PipelineLayout &GetLatestPipelineLayout() { return m_PipelineLayout; }
			// TODO: this is inappropriate here. Lastets created DescriptorSet.
			inline const vk::DescriptorSet &GetLatestDescriptorSet() { return m_DescriptorSet; }
		
			//! Does this pass use the specified ResourceGroup octave? All passes use octave 3 at least.
			//! 
			bool UsesResourceLayout(uint8_t l) const
			{
				return(((1<<l)&usingResourceLayouts)!=0);
			}
		private:
			void ApplyContextState(crossplatform::DeviceContext& deviceContext, vk::DescriptorSet& descriptorSet);
			void CreateDescriptorPoolAndSetLayoutAndPipelineLayout();
			void AllocateDescriptorSets(vk::DescriptorSet& descriptorSet);

			vk::ShaderStageFlags GetShaderFlagsForSlot(int slot, bool(platform::crossplatform::Shader::* pfn)(int) const);
			
			void InitializePipeline(crossplatform::GraphicsDeviceContext& deviceContext, RenderPassPipeline* renderPassPipeline
				, crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology
				, const crossplatform::RenderState* blendState = nullptr
				, const crossplatform::RenderState* depthStencilState = nullptr
				, const crossplatform::RenderState* rasterizerState = nullptr
				, bool multiview = false);

		public:
			RenderPassPipeline& GetRenderPassPipeline(crossplatform::GraphicsDeviceContext& deviceContext);
			RenderPassHash GetHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology, const crossplatform::Layout* layout);

		private:
			static RenderPassHash MakeRenderPassHash(crossplatform::PixelFormat pixelFormat, int numOfSamples, crossplatform::Topology topology
				, const crossplatform::Layout* layout = nullptr
				, const crossplatform::RenderState* blendState = nullptr
				, const crossplatform::RenderState* depthStencilState = nullptr
				, const crossplatform::RenderState* rasterizerState = nullptr
				, bool multiview = false);

		private:
			uint8_t usingResourceLayouts=0;
			bool											m_VideoSource = false;
			std::vector<vk::Sampler>						m_ImmutableSamplers;
			// TODO: These should probably be per deviceContext:
			std::vector<vk::WriteDescriptorSet> m_writeDescriptorSets;
			std::vector<vk::DescriptorImageInfo> descriptorImageInfos;
			std::vector<vk::DescriptorBufferInfo> descriptorBufferInfos;

			bool m_Initialized = false;
			vk::DescriptorPool								m_DescriptorPool;
			vk::DescriptorSetLayout							m_DescriptorSetLayout;
			vk::DescriptorSet								m_DescriptorSet;		// The pass's own internal desciptor set.
			vk::PipelineLayout								m_PipelineLayout;
			std::map<RenderPassHash, RenderPassPipeline>	m_RenderPasses;

			int64_t											m_LastFrameIndex;
			size_t											m_InternalFrameIndex;	// incremented internally.
			uint32_t										m_CurrentApplyCount;

			//! Number of ring buffers
			static const uint32_t							s_DescriptorSetCount = (SIMUL_VULKAN_FRAME_LAG + 1);
			// each frame in the 3-frame loop carries a variable-size list of descriptors: one for each submit.
			std::list<vk::DescriptorSet>					m_DescriptorSets[s_DescriptorSetCount];
			std::list<vk::DescriptorSet>::iterator			m_DescriptorSets_It[s_DescriptorSetCount];
			
		};
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif