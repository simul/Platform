#ifndef SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#define SIMUL_PLATFORM_CROSSPLATFORM_DEVICECONTEXT_H
#include "BaseRenderer.h"
#include "Texture.h"
#include "Export.h"
#include <functional>
#include <set>
#include <stack>
#include <unordered_map>

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
		enum class ShaderResourceType;
		struct TextureAssignment
		{
			crossplatform::Texture *texture;
			int dimensions;
			bool uav;
			int mip;// if -1, it's the whole texture.
			int index;	// if -1 it's the whole texture
			crossplatform::ShaderResourceType resourceType;
            //! Used on gl platforms
            std::string name;
		};
		//! A container class intended to reproduce some of the behaviour of std::map with ints for indices, but to be much much faster.
		template<typename T,int count> class FastMap
		{
			std::set<Effect*> linkedEffects;
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
		//! A structure to describe the state that is associated with a given deviceContext.
		//! When rendering is to be performed, we can ensure that the state is applied.
		struct SIMUL_CROSSPLATFORM_EXPORT ContextState
		{
			ContextState()
				:last_action_was_compute(false)
				,currentEffectPass(NULL)
				,currentTechnique(NULL)
				,currentEffect(NULL)
				,effectPassValid(false)
				,vertexBuffersValid(false)
				,constantBuffersValid(false)
				,structuredBuffersValid(false)
				,rwStructuredBuffersValid(false)
				,samplerStateOverridesValid(true)
				,textureAssignmentMapValid(false)
				,rwTextureAssignmentMapValid(false)
				,streamoutTargetsValid(false)
				,textureSlots(0)
				,rwTextureSlots(0)
				,rwTextureSlotsForSB(0)
				,textureSlotsForSB(0)
				,bufferSlots(0)
			{

			}

			~ContextState()
			{
			}
			ContextState& operator=(const ContextState& cs);
			bool last_action_was_compute;

			std::unordered_map<int,Buffer*> applyVertexBuffers;
			std::unordered_map<int,Buffer*> streamoutTargets;
			ConstantBufferAssignmentMap applyBuffers;
			StructuredBufferAssignmentMap applyStructuredBuffers;
			StructuredBufferAssignmentMap applyRwStructuredBuffers;
			SamplerStateAssignmentMap samplerStateOverrides;
			TextureAssignmentMap textureAssignmentMap;
			TextureAssignmentMap rwTextureAssignmentMap;
			EffectPass *currentEffectPass;
			EffectTechnique *currentTechnique;
			Effect *currentEffect;
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
		//! The base class for Device contexts. The actual context pointer is only applicable in DirectX - in OpenGL, it will be null.
		//! The DeviceContext also carries a pointer to the platform-specific RenderPlatform.
		//! DeviceContext is context in the DirectX11 sense, encompassing a platform-specific deviceContext pointer
		struct SIMUL_CROSSPLATFORM_EXPORT DeviceContext
		{
			void *platform_context;
			RenderPlatform *renderPlatform;
			EffectTechnique *activeTechnique;
			crossplatform::ContextState contextState;
			long long frame_number;
			bool initialized;
			DeviceContext();
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
			ViewStruct viewStruct;
			uint cur_backbuffer;
		public:
			std::stack<crossplatform::TargetsAndViewport*>& GetFrameBufferStack();
			crossplatform::TargetsAndViewport defaultTargetsAndViewport;
			//! Set the RT's to restore to, once all Simul Framebuffers are deactivated. This must be called at least once,
			//! as 
			void setDefaultRenderTargets(const ApiRenderTarget*,
				const ApiDepthRenderTarget*,
				uint32_t viewportLeft,
				uint32_t viewportTop,
				uint32_t viewportRight,
				uint32_t viewportBottom
			);
			std::stack<crossplatform::TargetsAndViewport*> targetStack;
		};

		struct DeviceContext;
	
		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void(crossplatform::DeviceContext&)> RenderDelegate;
		typedef std::function<void(void*)> StartupDeviceDelegate;
		typedef std::function<void()> ShutdownDeviceDelegate;
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif

#endif