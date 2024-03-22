#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
#include "Texture.h"
#include "Export.h"
#include "Topology.h"
#include <set>
#include <stack>
#include <parallel_hashmap/phmap.h>
#include <limits.h> 
#include <string.h>	// for memset

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

#ifndef INT_MAX
#define INT_MAX 2147483647
#endif

struct ID3D11DeviceContext;
struct IDirect3DDevice9;  
struct ID3D12GraphicsCommandList;
namespace sce
{
	namespace Gnmx
	{
		class LightweightGfxContext;
	}
}
namespace nvn
{
	class CommandBuffer;
}
namespace platform
{
	namespace crossplatform
	{
		class Buffer;
		class Effect;
		class EffectPass;
		class EffectTechnique;
		class Texture;
		class SamplerState;
		class Layout;
		class TopLevelAccelerationStructure;
		
		struct TextureFence
		{
			crossplatform::Texture *texture;
			unsigned long long label;
		};
		struct TextureAssignment
		{
			crossplatform::Texture *texture;
			crossplatform::TopLevelAccelerationStructure *accelerationStructure;
			int dimensions;
			bool uav;
			SubresourceRange subresource;
			crossplatform::ShaderResourceType resourceType;
		};
		//! A container class intended to reproduce some of the behaviour of std::map with ints for indices, but to be much much faster.
		template<typename T,int count> class FastMap
		{
			int index_limit;
			int index_start;
			T values[count+1];
			unsigned has_value;
			int sz;
		public:
			FastMap():index_limit(0),index_start(count),has_value(0),sz(0)
			{
				memset (values,0,(count+1)*sizeof(T));
			}
			~FastMap()
			{
			}
			const T& operator[](size_t i) const
			{
			#ifdef _DEBUG
				if(i>=index_limit||!HasValue(i))
				{
					return nullptr;
					//throw std::runtime_error("");
				}
				#endif
				return values[i];
			}
			bool HasValue(int index)
			{
				return ( (((unsigned)1<<(unsigned)index)&has_value) !=0);
			}
			T& operator[](size_t i)
			{
				unsigned value=(1<<i);
				if(!(has_value&value))
				{
					sz++;
					index_limit=std::max(index_limit,(int)(i+1));
					index_start=std::min(index_start,(int)i);
				}
				has_value|=value;
				return values[i];
			}
			void clear()
			{
				index_start=count;
				index_limit=0;
				has_value=0;
				sz=0;
			}
			int size() const
			{
				return sz;
			}
			struct iterator
			{
				int position;
				int first;
				unsigned has_value;
				bool operator==(const iterator &i)
				{
					return (position==i.position);
				}
				bool operator!=(const iterator &i)
				{
					return (position!=i.position);
				}
				iterator& operator++ ()     // prefix ++
				{
					do
					{
						first++;
						has_value>>=(unsigned)1;
					}
					while((1&has_value)==0&&has_value!=0);
					position++;
					return *this;
				}
			};
			iterator begin()
			{
				iterator i;
				i.position=0;
				i.first=index_start;
				i.has_value=(has_value>>(unsigned)index_start);
				return i;
			}
			iterator end()
			{
				iterator i;
				i.position=sz;
				i.first=index_limit;
				i.has_value=0;
				return i;
			}
		};
		class SIMUL_CROSSPLATFORM_EXPORT ConstantBufferBase;
		class SIMUL_CROSSPLATFORM_EXPORT PlatformStructuredBuffer;
		typedef FastMap<ConstantBufferBase*,32> ConstantBufferAssignmentMap;
		typedef FastMap<PlatformStructuredBuffer*,32> StructuredBufferAssignmentMap;
		typedef FastMap<TextureAssignment,32> TextureAssignmentMap;
		typedef FastMap<Buffer*,4> VertexBufferAssignmentMap;
		typedef FastMap<SamplerState*,32> SamplerStateAssignmentMap;
		typedef phmap::flat_hash_map<const Texture*,TextureFence> FenceMap;
		//! A structure to describe the state that is associated with a given deviceContext.
		//! When rendering is to be performed, we can ensure that the state is applied.
		struct SIMUL_CROSSPLATFORM_EXPORT ContextState
		{
			ContextState();
			~ContextState();
			ContextState(const ContextState& cs);
			ContextState& operator=(const ContextState& cs);
			bool IsDepthActive() const;
			bool last_action_was_compute = false;
			Viewport viewports[8];
			bool viewportsChanged = true;
			bool scissorChanged=true;
			const Buffer *indexBuffer=nullptr;
			phmap::flat_hash_map<int,const Buffer*> applyVertexBuffers;
			phmap::flat_hash_map<int,Buffer*> streamoutTargets;
			ConstantBufferAssignmentMap applyBuffers;
			StructuredBufferAssignmentMap applyStructuredBuffers;
			StructuredBufferAssignmentMap applyRwStructuredBuffers;
			SamplerStateAssignmentMap samplerStateOverrides;
			TextureAssignmentMap textureAssignmentMap;
			TextureAssignmentMap rwTextureAssignmentMap;
			FenceMap fenceMap;
			EffectPass *currentEffectPass=nullptr;
			Effect *currentEffect=nullptr;
			Layout *currentLayout=nullptr;
			Topology topology = Topology::UNDEFINED;
			tvector4<int> scissor={0,0,INT_MAX,INT_MAX};
			uint32_t viewMask = 0; //For ViewInstancing/Multiview
			int apply_count = 0;
			bool contextActive=false;
			bool externalContext=false;
			bool vulkanInsideRenderPass=false;

			bool effectPassValid=false;
			bool vertexBuffersValid=false;
			bool constantBuffersValid=false;
			bool structuredBuffersValid=false;
			bool rwStructuredBuffersValid=false;
			bool samplerStateOverridesValid=false;
			bool textureAssignmentMapValid=false;
			bool rwTextureAssignmentMapValid=false;
			bool streamoutTargetsValid=false;
			uint32_t textureSlots=0;
			uint32_t rwTextureSlots=0;
			uint32_t rwTextureSlotsForSB=0;
			uint32_t textureSlotsForSB=0;
			uint32_t bufferSlots=0;

			uint32_t resourceGroupApplyCounter[4]={0,0,0,0};
			uint32_t resourceGroupUploadedCounter[4] = {0, 0, 0, 0};
			uint32_t resourceGroupAppliedCounter[4] = {0, 0, 0, 0};
			uint32_t resourceGroupAppliedCounterCompute[4] = {0, 0, 0, 0};
			/// Reset the temporary properties, retain persistent properties.
			void invalidate()
			{
				effectPassValid=false;
				vertexBuffersValid=false;
				constantBuffersValid=false;
				structuredBuffersValid=false;
				rwStructuredBuffersValid=false;
				samplerStateOverridesValid=true;
				textureAssignmentMapValid=false;
				rwTextureAssignmentMapValid=false;
				streamoutTargetsValid=false;
				textureSlots=0;
				rwTextureSlots=0;
				rwTextureSlotsForSB=0;
				textureSlotsForSB=0;
				bufferSlots=0;
				viewMask=0;
				contextActive = true;
				externalContext = false;
			}
			
			/// Reset the temporary and persistent properties.
			void Reset()
			{
				invalidate();
				
				currentLayout = nullptr;
				topology = Topology::UNDEFINED;
				last_action_was_compute = false;
				memset(viewports, 0, 8 * sizeof(Viewport));
				indexBuffer = nullptr;
				applyVertexBuffers.clear();
				streamoutTargets.clear();
				applyBuffers.clear();
				applyStructuredBuffers.clear();
				applyRwStructuredBuffers.clear();
				samplerStateOverrides.clear();
				textureAssignmentMap.clear();
				rwTextureAssignmentMap.clear();
				fenceMap.clear();

				currentEffectPass = nullptr;
				currentEffect = nullptr;
				apply_count = 0;
				resourceGroupAppliedCounter[0] = resourceGroupAppliedCounter[1] = resourceGroupAppliedCounter[2] = 0;
				resourceGroupApplyCounter[0] = resourceGroupApplyCounter[1] = resourceGroupApplyCounter[2]=0;
				resourceGroupUploadedCounter[0] = resourceGroupUploadedCounter[1] = resourceGroupUploadedCounter[2] = 0;
				resourceGroupAppliedCounterCompute[0] = resourceGroupAppliedCounterCompute[1] = resourceGroupAppliedCounterCompute[2] = 0;
			}
		};
		class EffectTechnique;
		class RenderPlatform;
		struct GraphicsDeviceContext;
		struct MultiviewGraphicsDeviceContext;
		struct ComputeDeviceContext;
		enum class DeviceContextType:uint8_t
		{
			GRAPHICS, 
			MULTIVIEW_GRAPHICS, 
			COMPUTE, 
			COPY
		};
		struct SIMUL_CROSSPLATFORM_EXPORT DeviceContext
		{
		protected:
			long long frame_number = 0;
			//! Only RenderPlatform should call this.
			long long GetFrameNumber() const;
			//! Only RenderPlatform should call this.
			void SetFrameNumber(long long n);
			friend class RenderPlatform;
		public:
			DeviceContextType deviceContextType;
			
			void *platform_context=nullptr;
			void *platform_context_allocator=nullptr;
			RenderPlatform *renderPlatform=nullptr;
			crossplatform::ContextState contextState;
#ifdef _DEBUG
			int ApiCallCounter=0;
#endif
			virtual GraphicsDeviceContext* AsGraphicsDeviceContext()
			{ 
				return nullptr;
			}
			virtual MultiviewGraphicsDeviceContext* AsMultiviewGraphicsDeviceContext()
			{
				return nullptr;
			}
			virtual ComputeDeviceContext* AsComputeDeviceContext()
			{
				return nullptr;
			}
			ID3D11DeviceContext *asD3D11DeviceContext()
			{
				return (ID3D11DeviceContext*)platform_context;
			}
			IDirect3DDevice9 *asD3D9Device()
			{
				return (IDirect3DDevice9*)platform_context;
			}
			sce::Gnmx::LightweightGfxContext *asGfxContext()
			{
				return (sce::Gnmx::LightweightGfxContext*)platform_context;
			}
			nvn::CommandBuffer* asNVNContext()
			{
				return (nvn::CommandBuffer*)platform_context;
			}
			ID3D12GraphicsCommandList* asD3D12Context()
			{
				return (ID3D12GraphicsCommandList*)platform_context;
			}
		};
		
		//! The base class for Device contexts. The actual context pointer is only applicable in DirectX - in OpenGL, it will be null.
		//! The DeviceContext also carries a pointer to the platform-specific RenderPlatform.
		//! DeviceContext is context in the DirectX11 sense, encompassing a platform-specific deviceContext pointer
		struct SIMUL_CROSSPLATFORM_EXPORT GraphicsDeviceContext : public DeviceContext
		{
			GraphicsDeviceContext *AsGraphicsDeviceContext() override
			{
				return this;
			}
			bool initialized=false;
			int framePrintX=0;
			int framePrintY=0;
			double predictedDisplayTimeS = 0.0;
			GraphicsDeviceContext();
			~GraphicsDeviceContext();
			ViewStruct viewStruct;
			//uint cur_backbuffer;
			std::stack<crossplatform::TargetsAndViewport*>& GetFrameBufferStack();
			crossplatform::TargetsAndViewport defaultTargetsAndViewport;
			
			crossplatform::TargetsAndViewport *GetCurrentTargetsAndViewport();
			//! Set the RT's to restore to, once all Simul Framebuffers are deactivated. This must be called at least once,
			//! as 
			void setDefaultRenderTargets(const ApiRenderTarget* rt
				,const ApiDepthRenderTarget* dt
				,uint32_t viewportLeft
				,uint32_t viewportTop
				,uint32_t viewportRight
				,uint32_t viewportBottom
				,Texture **texture_targets=nullptr
				,int num_targets=0
				,Texture *depth_target=nullptr
			);
			std::stack<crossplatform::TargetsAndViewport*> targetStack;
		};

		//! MultiviewGraphicsDeviceContext extends GraphicsDeviceContext. It contains multiple ViewStruct that can be
		//! associated with a layer in the Texture; therefore GraphicsDeviceContext::viewStruct should be ignored or
		//! used a back up if MultiviewGraphicsDeviceContext::viewStructs is empty.
		struct SIMUL_CROSSPLATFORM_EXPORT MultiviewGraphicsDeviceContext : public GraphicsDeviceContext
		{
			MultiviewGraphicsDeviceContext* AsMultiviewGraphicsDeviceContext() override
			{
				return this;
			}
			MultiviewGraphicsDeviceContext();

			std::vector<ViewStruct> viewStructs;
		};

		struct SIMUL_CROSSPLATFORM_EXPORT ComputeDeviceContext : public DeviceContext
		{
			ComputeDeviceContext* AsComputeDeviceContext() override
			{
				return this;
			}

			ComputeDeviceContext();
			//Some function expect completed_frame and frame_number to increment.
			//This copies the frame numbers from the normal DeviceContext to simulate this.
			void UpdateFrameNumbers(DeviceContext& deviceContext);
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif

#endif
