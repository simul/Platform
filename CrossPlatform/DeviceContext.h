#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
#include "Texture.h"
#include "Export.h"
#include "Topology.h"
#include <set>
#include <stack>
#include <unordered_map>
#include <string.h>	// for memset

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
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
namespace simul
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
		enum class ShaderResourceType;
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
				if(i>=index_limit||!HasValue(i))
				{
					return nullptr;
					//throw std::runtime_error("");
				}
				return values[i];
			}
			bool HasValue(int index)
			{
				return ( (((unsigned)1<<(unsigned)index)&has_value) !=0);
			}
			T& operator[](size_t i)
			{
				if(i>=count)
				{
					//throw std::runtime_error("");
				}
				unsigned value=(1<<i);
				if(!(has_value&value))
				{
					sz++;
					if(i>=index_limit)
						index_limit=(int)(i+1);
					if(i<index_start)
						index_start=(int)i;
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
		typedef std::unordered_map<const Texture*,TextureFence> FenceMap;
		//! A structure to describe the state that is associated with a given deviceContext.
		//! When rendering is to be performed, we can ensure that the state is applied.
		struct SIMUL_CROSSPLATFORM_EXPORT ContextState
		{
			ContextState();
			~ContextState()
			{
			}
			ContextState& operator=(const ContextState& cs);
			bool last_action_was_compute = false;
			Viewport viewports[8];
			const Buffer *indexBuffer=nullptr;
			std::unordered_map<int,const Buffer*> applyVertexBuffers;
			std::unordered_map<int,Buffer*> streamoutTargets;
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
			int4 scissor = { 0,0,INT_MAX,INT_MAX };
			int apply_count = 0;
			int test=0;
			bool contextActive=true;
			bool externalContext=false;

			bool effectPassValid=false;
			bool vertexBuffersValid=false;
			bool constantBuffersValid=false;
			bool structuredBuffersValid=false;
			bool rwStructuredBuffersValid=false;
			bool samplerStateOverridesValid=false;
			bool textureAssignmentMapValid=false;
			bool rwTextureAssignmentMapValid=false;
			bool streamoutTargetsValid=false;
			unsigned int textureSlots=0;
			unsigned int rwTextureSlots=0;
			unsigned int rwTextureSlotsForSB=0;
			unsigned int textureSlotsForSB=0;
			unsigned int bufferSlots=0;

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
				currentLayout = nullptr;
				textureSlots=0;
				rwTextureSlots=0;
				rwTextureSlotsForSB=0;
				textureSlotsForSB=0;
				bufferSlots=0;
				topology = Topology::UNDEFINED;
				contextActive = true;
				externalContext = false;
			}

			void Reset()
			{
				invalidate();
				test=0;
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
			}
		
		};
		class EffectTechnique;
		class RenderPlatform;
		struct GraphicsDeviceContext;
		struct ComputeDeviceContext;
		enum class DeviceContextType
		{
			GRAPHICS, 
			COMPUTE, 
			COPY
		};
		struct SIMUL_CROSSPLATFORM_EXPORT DeviceContext
		{
			DeviceContextType deviceContextType;
			long long completed_frame=0;
			long long frame_number=0;
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
			GraphicsDeviceContext();
			ViewStruct viewStruct;
			uint cur_backbuffer;
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

		struct SIMUL_CROSSPLATFORM_EXPORT ComputeDeviceContext : public DeviceContext
		{
			ComputeDeviceContext()
			{
				deviceContextType = DeviceContextType::COMPUTE;
			}
			ComputeDeviceContext* AsComputeDeviceContext() override
			{
				return this;
			}
			//Some function expect completed_frame and frame_number to increment.
			//This copies the frame numbers from the normal DeviceContext to simulate this.
			void UpdateFrameNumbers(DeviceContext& deviceContext)
			{
				this->completed_frame = deviceContext.completed_frame;
				this->frame_number = deviceContext.frame_number;
			}

		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif

#endif
