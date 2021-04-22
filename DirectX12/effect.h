#pragma once
#include "Platform/DirectX12/Export.h"
#include "Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Platform/DirectX12/ConstantBuffer.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/Query.h"

#include "Heap.h"

#include <string>
#include <map>
#include <array>

#pragma warning(disable:4251)

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12ShaderReflection;
namespace simul
{
	namespace crossplatform
	{
		struct TextureAssignment;
	}
	namespace dx12
	{
		// Note: not a real D3D12_... _DESC:
		struct D3D12_RENDER_TARGET_FORMAT_DESC
		{
			//! Returns an unique number that represents this rt desc
			inline size_t GetHash()
			{
				size_t h = 0;
				h |= (size_t)RTFormats[0] << ( 5 * 0);
				h |= (size_t)RTFormats[1] << ( 5 * 1);
				h |= (size_t)RTFormats[2] << ( 5 * 2);
				h |= (size_t)RTFormats[3] << ( 5 * 3);
				h |= (size_t)RTFormats[4] << ( 5 * 4);
				h |= (size_t)RTFormats[5] << ( 5 * 5);
				h |= (size_t)RTFormats[6] << ( 5 * 6);
				h |= (size_t)RTFormats[7] << ( 5 * 7);
				h ^= (size_t)Count<<( 5 * 8);
				return h;
			}
			UINT Count;
			DXGI_FORMAT RTFormats[8];
		};

		//! Holds dx12 rendering state
		struct SIMUL_DIRECTX12_EXPORT RenderState:public crossplatform::RenderState
		{
			RenderState();
			virtual ~RenderState();
			D3D12_BLEND_DESC				BlendDesc;
			D3D12_RASTERIZER_DESC			RasterDesc;
			D3D12_DEPTH_STENCIL_DESC		DepthStencilDesc;
			D3D12_RENDER_TARGET_FORMAT_DESC	RtFormatDesc;
		};
		//! DirectX12 structured buffer class
		class EffectPass;
		class Effect;
		class RenderPlatform;
		class ShaderBindingTable;
		
		//! DirectX12 Effect Pass implementation, this will hold several PSOs, its also in charge of 
		// setting resources.
		class SIMUL_DIRECTX12_EXPORT EffectPass:public simul::crossplatform::EffectPass
		{
		public:
			EffectPass(crossplatform::RenderPlatform *r,crossplatform::Effect *e);
			virtual		~EffectPass();
			void		InvalidateDeviceObjects();
			void		Apply(crossplatform::DeviceContext &deviceContext,bool asCompute) override;
			bool		IsCompute()const { return mIsCompute; }
			bool		IsRaytrace() const {return mIsRaytrace;}

			void		SetSamplers(crossplatform::SamplerStateAssignmentMap& samplers,dx12::Heap* samplerHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void		SetConstantBuffers(crossplatform::ConstantBufferAssignmentMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device,crossplatform::DeviceContext& context);
			void		SetSRVs(crossplatform::TextureAssignmentMap &textures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void		SetUAVs(crossplatform::TextureAssignmentMap &rwTextures, crossplatform::StructuredBufferAssignmentMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void		CheckSlots(int requiredSlots, int usedSlots, int numSlots, const char* type);
			void		CreateComputePso(crossplatform::DeviceContext& deviceContext);
			ID3D12PipelineState* GetGraphicsPso(crossplatform::GraphicsDeviceContext& deviceContext);
			ID3D12PipelineState* GetComputePso() { return mComputePso; }
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
			ID3D12StateObject* GetRayTracingPso() { return mRaytracePso; }
#endif
			void		CreateLocalRootSignature();
			void		CreateRaytracePso();
			const ShaderBindingTable* GetShaderBindingTable() { return shaderBindingTable; }
			void		InitShaderBindingTable();
			
		public:
			const LPCWSTR								hitGroupExportName = L"HitGroup";
		private:
			ShaderBindingTable*							shaderBindingTable = nullptr;
			ID3D12RootSignature*						localRootSignature = nullptr;
			
			struct Pso
			{
				D3D12_GRAPHICS_PIPELINE_STATE_DESC		desc;
				ID3D12PipelineState*					pipelineState = nullptr;
			};
			//! We hold a map with unique PSOs
			std::unordered_map<uint64_t, Pso>			mGraphicsPsoMap;
			std::unordered_map<uint64_t, D3D12_RENDER_TARGET_FORMAT_DESC*> mTargetsMap;
			//! We only have one compute/raytrace Pipeline  
			ID3D12PipelineState*						mComputePso = nullptr;
#if PLATFORM_SUPPORT_D3D12_RAYTRACING
			//! For raytracing, the pipeline state object is NOT a PipelineState, but a ID3D12StateObject...!
			ID3D12StateObject*							mRaytracePso = nullptr;
#endif
			//! Is this a compute pass?
			bool										mIsCompute = false;
			//! Is this a raytrace pass?
			bool										mIsRaytrace = false;
			std::vector<CD3DX12_DESCRIPTOR_RANGE>		mSrvCbvUavRanges;
			std::vector<CD3DX12_DESCRIPTOR_RANGE>		mSamplerRanges;
			std::string									mTechName;
			//! Arrays used by the Set* methods declared here to avoid runtime memory allocation
			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumCBV>	mCbSrcHandles;

			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumSRV>	mSrvSrcHandles;
			std::array<bool, ResourceBindingLimits::NumSRV>							mSrvUsedSlotsArray;

			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, ResourceBindingLimits::NumUAV>	mUavSrcHandles;
			std::array<bool, ResourceBindingLimits::NumUAV>							mUavUsedSlotsArray;

			D3D12_DEPTH_STENCIL_DESC*	mInUseOverrideDepthState;
			D3D12_BLEND_DESC*			mInUseOverrideBlendState;
		};

		class SIMUL_DIRECTX12_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
		public:
			EffectTechnique(crossplatform::RenderPlatform *r,crossplatform::Effect *e);
			int NumPasses() const;
			crossplatform::EffectPass *AddPass(const char *name,int i) override;
		};

		class SIMUL_DIRECTX12_EXPORT Shader :public simul::crossplatform::Shader
		{
		public:
			~Shader();
			void load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *data, size_t len, crossplatform::ShaderType t) override;
			std::vector<uint8_t>		shader12;
			// If raytracing, store in D3D12Resource pointers.
			ID3D12Resource *shaderTableResource=nullptr;
			ID3D12ShaderReflection*		mShaderReflection = nullptr;
#ifdef WIN64
			ID3D12LibraryReflection*	mLibraryReflection = nullptr;
#endif
		};

		//! DirectX12 Effect implementation
		class SIMUL_DIRECTX12_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
		public:
			Effect();
			virtual ~Effect();

			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void PostLoad() override;
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique* GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique* GetTechniqueByIndex(int index);

			//! This method uses the shader reflection code to check the resources slots. This is needed because
			//! the sfx compiler is not as smart as the dx compiler so it will report that some resources are in use
			//! when it is not true. In Dx12 we MUST be explicit about what are we doing so we need to know exactly the
			//! resources that will be in use.
			void CheckShaderSlots(dx12::Shader* shader, const std::vector<uint8_t>& shaderBlob);

			Heap* GetEffectSamplerHeap();

		private:
			//! Heap with the static samplers used by the effect
			Heap* mSamplersHeap;
		};
	}
}
