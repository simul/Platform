#pragma once
#include "Simul/Platform/DirectX12/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "SimulDirectXHeader.h"
#include "Simul/Platform/DirectX12/ConstantBuffer.h"

#include "Heap.h"
#include "Fence.h"
#include "RootSignature.h"

#include <string>
#include <map>

#pragma warning(disable:4251)

struct ID3D11Buffer;
struct ID3DX11EffectConstantBuffer;
struct ID3DX11EffectPass;

struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct ID3D12ShaderReflection;
namespace simul
{
	namespace crossplatform
	{
		struct TextureAssignment;
	}
	namespace dx11on12
	{
		struct SIMUL_DIRECTX12_EXPORT Query:public crossplatform::Query
		{
			ID3D11Query *d3d11Query[crossplatform::Query::QueryLatency];
			Query(crossplatform::QueryType t):crossplatform::Query(t)
			{
				for(int i=0;i<QueryLatency;i++)
					d3d11Query[i]		=0;
			}
			virtual ~Query() override
			{
				InvalidateDeviceObjects();
			}
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r) override;
			void InvalidateDeviceObjects() override;
			void Begin(crossplatform::DeviceContext &deviceContext) override;
			void End(crossplatform::DeviceContext &deviceContext) override;
			bool GetData(crossplatform::DeviceContext &deviceContext,void *data,size_t sz) override;
			void SetName(const char *n) override;
		};

		struct SIMUL_DIRECTX12_EXPORT RenderState:public crossplatform::RenderState
		{
			ID3D11DepthStencilState		*m_depthStencilState;
			ID3D11BlendState			*m_blendState;
			ID3D11RasterizerState		*m_rasterizerState;

			D3D12_BLEND_DESC			BlendDesc;
			D3D12_RASTERIZER_DESC		RasterDesc;
			D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;

			RenderState();
			virtual ~RenderState();
		};

		/// DirectX12 structured buffer class
		class EffectPass;
		class Effect;
		class RenderPlatform;
		class PlatformStructuredBuffer:public crossplatform::PlatformStructuredBuffer
		{
		public:
											PlatformStructuredBuffer();
			virtual							~PlatformStructuredBuffer();
			void							RestoreDeviceObjects(crossplatform::RenderPlatform *renderPlatform,int ct,int unit_size,bool computable,void *init_data);
			void							Apply(crossplatform::DeviceContext &deviceContext, crossplatform::Effect *effect, const char *name);
			void*							GetBuffer(crossplatform::DeviceContext &deviceContext);
			const void*						OpenReadBuffer(crossplatform::DeviceContext &deviceContext);
			void							CloseReadBuffer(crossplatform::DeviceContext &deviceContext);
			void							CopyToReadBuffer(crossplatform::DeviceContext &deviceContext);
			void							SetData(crossplatform::DeviceContext &deviceContext,void *data);
			void							InvalidateDeviceObjects();
			void							Unbind(crossplatform::DeviceContext &deviceContext);
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12ShaderResourceView();
			D3D12_CPU_DESCRIPTOR_HANDLE*	AsD3D12UnorderedAccessView(int = 0);
			void							ActualApply(simul::crossplatform::DeviceContext& deviceContext, EffectPass* currentEffectPass, int slot);

		private:
			ID3D12Resource*				mBufferDefault;
			ID3D12Resource*				mBufferUpload;
			ID3D12Resource*				mBufferReadFront;
			ID3D12Resource*				mBufferReadBack;

			/// We hold the currently mapped pointer
			UINT8*						mReadSrc;
			/// Temporal buffer used to upload data to the GPU
			void*						mTempBuffer;
			/// If true, we have to update the GPU data
			bool						mChanged;

			dx12::Heap					mBufferSrvHeap;
			dx12::Heap					mBufferUavHeap;
			dx12::Fence					mCbFence;
			bool						mFirstUse = true;

			// Nacho amazinks debugins
			int							mUseCount = 0;
			int							mLastFrame = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE mSrvHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE mUavHandle;
			D3D12_RESOURCE_STATES		mCurrentState;

			dx11on12::RenderPlatform*	mRenderPlatform;
			int							num_elements;
			int							element_bytesize;
			int							mTotalSize;
		};

		typedef std::unordered_map<crossplatform::SamplerState*, int> SamplerMap;
		typedef std::unordered_map<int, crossplatform::ConstantBufferBase*> ConstantBufferMap;
		typedef std::unordered_map<int, crossplatform::TextureAssignment> TextureMap;
		typedef std::unordered_map<int, crossplatform::PlatformStructuredBuffer*> StructuredBufferMap;
		class SIMUL_DIRECTX12_EXPORT EffectPass:public simul::crossplatform::EffectPass
		{
		public:
			void Apply(crossplatform::DeviceContext &deviceContext,bool asCompute) override;
			bool IsCompute()const { return mIsCompute; }

			void SetSamplers(SamplerMap& samplers,dx12::Heap* samplerHeap, ID3D12Device* device);
			void SetConstantBuffers(ConstantBufferMap& cBuffers, dx12::Heap* frameHeap, ID3D12Device* device,crossplatform::DeviceContext& context);
			void SetTextures(TextureMap& textures, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);
			void SetStructuredBuffers(StructuredBufferMap& sBuffers, dx12::Heap* frameHeap, ID3D12Device* device, crossplatform::DeviceContext& context);

			int ResourceTableIndex()const { return mSrvCbvUavTableIndex; }
			int SamplerTableIndex()const { return mSamplerTableIndex; }

		private:
			virtual ~EffectPass();

			/// This map will hold Pipeline states for the different output formats
			std::map<DXGI_FORMAT, ID3D12PipelineState*> mGraphicsPsoMap;
			/// We only have one compute Pipeline  
			ID3D12PipelineState*						mComputePso = nullptr;
			/// The root signature
			ID3D12RootSignature* mRootS								= nullptr;

			/// Is this a compute pass?
			bool mIsCompute = false;
			/// The index of the SRV_CBV_UAV table in the root parameters
			INT mSrvCbvUavTableIndex	= -1;
			/// The index of the SAMPLER table in the root parameters
			INT mSamplerTableIndex		= -1;
			/// A vector holding the root parameters (in our case it will always be 1 or 2)
			std::vector<CD3DX12_ROOT_PARAMETER> rootParams;

			std::vector<CD3DX12_DESCRIPTOR_RANGE> mSrvCbvUavRanges;
			std::vector<CD3DX12_DESCRIPTOR_RANGE> mSamplerRanges;

			std::unordered_map<int, D3D_SHADER_INPUT_TYPE> mResources;

			dx12::RootSignature mRootSignature;
		};
		class SIMUL_DIRECTX12_EXPORT EffectTechnique:public simul::crossplatform::EffectTechnique
		{
		public:
			int NumPasses() const;
			crossplatform::EffectPass *AddPass(const char *name,int i) override;
		};
		class SIMUL_DIRECTX12_EXPORT Shader :public simul::crossplatform::Shader
		{
		public:
			void load(crossplatform::RenderPlatform *renderPlatform, const char *filename, crossplatform::ShaderType t) override;
			union
			{
				ID3D11GeometryShader		*geometryShader;
				ID3D11VertexShader			*vertexShader;
				ID3D11PixelShader			*pixelShader;
				ID3D11ComputeShader			*computeShader;
			};
			union
			{
				ID3DBlob*					vertexShader12;
				ID3DBlob*					pixelShader12;
				ID3DBlob*					computeShader12;
			};
			ID3D12ShaderReflection*			mShaderReflection = nullptr;
		};
		class SIMUL_DIRECTX12_EXPORT Effect:public simul::crossplatform::Effect
		{
		protected:
			EffectTechnique *CreateTechnique();
		public:
			Effect();
			virtual ~Effect();
			void Load(crossplatform::RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void InvalidateDeviceObjects();
			crossplatform::EffectTechnique *GetTechniqueByName(const char *name);
			crossplatform::EffectTechnique *GetTechniqueByIndex(int index);

			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass) override;
			void Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *pass) override;
			void Reapply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext) override;
			void UnbindTextures(crossplatform::DeviceContext &deviceContext);
			void SetConstantBuffer(crossplatform::DeviceContext &deviceContext, const char *name, crossplatform::ConstantBufferBase *s) override;

			void CheckShaderSlots(dx11on12::Shader* shader, ID3DBlob* shaderBlob);

			// Nacho: I need to retrieve the samplers of this effect so I can access them (to get the CPU descriptor handle)
			std::unordered_map<crossplatform::SamplerState*, int>& GetSamplers() { return samplerSlots; }
		};
	}
}
