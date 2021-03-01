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
			int mip;// if -1, it's the whole texture.
			int index;	// if -1 it's the whole texture
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
			bool last_action_was_compute;
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
			//EffectTechnique *currentTechnique=nullptr;
			Effect *currentEffect=nullptr;
			Layout *currentLayout=nullptr;
			Topology topology;
			int apply_count = 0;
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
			}
			bool effectPassValid;
			bool vertexBuffersValid;
			bool constantBuffersValid;
			bool structuredBuffersValid;
			bool rwStructuredBuffersValid;
			bool samplerStateOverridesValid;
			bool textureAssignmentMapValid;
			bool rwTextureAssignmentMapValid;
			bool streamoutTargetsValid;
			unsigned textureSlots;
			unsigned rwTextureSlots;
			unsigned rwTextureSlotsForSB;
			unsigned textureSlotsForSB;
			unsigned bufferSlots;
		};
		class EffectTechnique;
		class RenderPlatform;
		struct GraphicsDeviceContext;
		enum class DeviceContextType
		{
			COMPUTE,COPY,GRAPHICS
		};
		struct SIMUL_CROSSPLATFORM_EXPORT DeviceContext
		{
			DeviceContextType deviceContextType;
			long long completed_frame=0;
			long long frame_number=0;
			void *platform_context=nullptr;
			RenderPlatform *renderPlatform=nullptr;
			crossplatform::ContextState contextState;
#ifdef _DEBUG
			int ApiCallCounter=0;
#endif
			virtual GraphicsDeviceContext *AsGraphicsDeviceContext()
			{
				return nullptr;
			}
			inline ID3D11DeviceContext *asD3D11DeviceContext()
			{
				return (ID3D11DeviceContext*)platform_context;
			}
			inline IDirect3DDevice9 *asD3D9Device()
			{
				return (IDirect3DDevice9*)platform_context;
			}
			inline sce::Gnmx::LightweightGfxContext *asGfxContext()
			{
				return (sce::Gnmx::LightweightGfxContext*)platform_context;
			}
			inline nvn::CommandBuffer* asNVNContext()
			{
				return (nvn::CommandBuffer*)platform_context;
			}
			inline ID3D12GraphicsCommandList* asD3D12Context()
			{
				return (ID3D12GraphicsCommandList*)platform_context;
			}
		};
		struct SIMUL_CROSSPLATFORM_EXPORT ComputeDeviceContext : public DeviceContext
		{
			ComputeDeviceContext()
			{
				deviceContextType=DeviceContextType::COMPUTE;
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
			//! Set the RT's to restore to, once all Simul Framebuffers are deactivated. This must be called at least once,
			//! as 
			void setDefaultRenderTargets(const ApiRenderTarget*
				,const ApiDepthRenderTarget*
				,uint32_t viewportLeft
				,uint32_t viewportTop
				,uint32_t viewportRight
				,uint32_t viewportBottom
				,Texture **texture_targets=nullptr
				,int num_targets=1
				,Texture *depth_target=nullptr
			);
			std::stack<crossplatform::TargetsAndViewport*> targetStack;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif

#endif
