#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include <vector>
#include "Export.h"
#include "Platform/Core/MemoryInterface.h"
#include "Platform/CrossPlatform/BaseRenderer.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Topology.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/debug_constants.sl"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Allocator.h"

#define SIMUL_GPU_TRACK_MEMORY(mem,size) \
	if (renderPlatform && renderPlatform->GetMemoryInterface()) \
		renderPlatform->GetMemoryInterface()->TrackVideoMemory(mem,size, __FILE__);
#define SIMUL_GPU_UNTRACK_MEMORY(mem) \
	if (renderPlatform && renderPlatform->GetMemoryInterface()) \
		renderPlatform->GetMemoryInterface()->UntrackVideoMemory(mem);

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
struct ID3D11Device;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct VertexXyzRgba
{
	float x,y,z;
	float r,g,b,a;
};
namespace vk
{
	class Device;
	class Instance;
}
namespace platform
{
	/// The namespace and library for cross-platform base classes, which abstract rendering functionality.
	namespace crossplatform
	{
		class GpuProfiler;
		class Material;
		class Effect;
		class EffectTechnique;
		class TextRenderer;
		struct Viewport;
		class Light;
		class Texture;
		class BaseFramebuffer;
		class SamplerState;
		class Mesh;
		class PlatformConstantBuffer;
		class PlatformStructuredBuffer;
		class Buffer;
		class Layout;
		struct RenderState;
		struct RenderStateDesc;
		struct DeviceContext;
		struct LayoutDesc;
		struct SamplerStateDesc;
		struct PhysicalLightRenderData;
		struct Query;
		struct TargetsAndViewport;
		class SwapChain;
		struct Window;
		class DisplaySurface;
		class BottomLevelAccelerationStructure;
		class TopLevelAccelerationStructure;
		class AccelerationStructureManager;
		class ShaderBindingTable;

		//! Type of resource transition, some platforms used this (dx12)
		enum ResourceTransition
		{
			Readable			= 0,
			ReadableGraphics	= Readable,
			ReadableCompute		= 1,
			Writeable			= 2,
			UnorderedAccess		= 3
		};
		/// Should correspond to UnityGfxRenderer
		enum class RenderPlatformType
		{
			Unknown					= -1,
			OpenGL					= 0,	// Desktop OpenGL
			D3D11					= 2,	// Direct3D 11
			Null					= 4,	// null means don't render, as opposed to Unknown which means uninitialized.
			PS4						= 13,	// PlayStation 4
			XboxOne					= 14,	// Xbox One        
			Metal					= 16,	// iOS Metal
			D3D12					= 18,	// Direct3D 12
			Vulkan					= 21,	// Vulkan
			Switch					= 22,	// Nintendo Switch NVN API
			XboxOneD3D12			= 23,	// XboxOne Direct3D 12
			GameCoreXboxOne			= 24,	// Game Core Xbox One graphics API using Direct3D 12.
			GameCoreXboxSeries		= 25,	// Game Core Xbox Series graphics API using Direct3D 12.
			GameCoreScarlett [[deprecated("Use GameCoreXboxSeries instead.")]] = 25, // Game Core Scarlett graphics API using Direct3D 12. 
			Commodore				= 45,	// Commodore 64
			Spectrum				= 25,	// ZX Spectrum
			PS5						= 26,	// PlayStation 5
			PS5NGGC					= 27,	// PlayStation 5 NGGC
			GLES					= 28,
			D3D11_FastSemantics	    = 1002, // Direct3D 11
		};
		enum RenderingFeatures:uint32_t
		{
			None=0,
			Raytracing=1
		};
		/// A vertex format for debugging.
		struct PosColourVertex
		{
			vec3 pos;
			vec4 colour;
		};
		struct SIMUL_CROSSPLATFORM_EXPORT Fence
		{
			virtual ~Fence() = default;
			enum class Signaller : uint32_t { CPU, GPU };
			typedef Signaller Waiter;
			uint64_t value;
			virtual void RestoreDeviceObjects(RenderPlatform *r)
			{
			}
			virtual void InvalidateDeviceObjects()
			{
			}
		};
		/// Given a viewport struct and a texture, get the texture coordinates that viewport represents within the texture.
		vec4 SIMUL_CROSSPLATFORM_EXPORT ViewportToTexCoordsXYWH(const Viewport *vi,const Texture *t);
		/// Given a viewport struct and a texture, get the texture coordinates that viewport represents within the texture.
		vec4 SIMUL_CROSSPLATFORM_EXPORT ViewportToTexCoordsXYWH(const int4 *vi,const Texture *t);

		/*! RenderPlatform is an interface that allows Platform's rendering functions to be developed
			in a cross-platform manner. By abstracting the common functionality of the different graphics API's
			into an interface, we can write render code that need not know which API is being used. It is possible
			to create platform-specific objects like /link CreateTexture textures/endlink, /link CreateEffect effects/endlink
			and /link CreateBuffer buffers/endlink

			Be sure to make the following calls at the appropriate places:
			RestoreDeviceObjects(), InvalidateDeviceObjects(), RecompileShaders()
			*/
		class SIMUL_CROSSPLATFORM_EXPORT RenderPlatform
		{
		protected:
			RenderingFeatures renderingFeatures=RenderingFeatures::None;
			int ApiCallLimit=0;
			long long last_begin_frame_number=0;
			//! This is called by draw functions to do any lazy updating prior to the actual API draw/dispatch call.
			virtual bool ApplyContextState(crossplatform::DeviceContext & /*deviceContext*/,bool /*error_checking*/ =true);
			virtual Viewport PlatformGetViewport(crossplatform::DeviceContext &deviceContext,int index);
		public:
			/// Get the current state to be applied to the given context at the next draw or dispatch.
			crossplatform::ContextState *GetContextState(crossplatform::DeviceContext &deviceContext);
			virtual void T1(){}
			RenderPlatform(platform::core::MemoryInterface*m=NULL);
			virtual ~RenderPlatform();
			virtual float GetDefaultOutputGamma() const
			{
				return 1.0f;
			}
			void SetCanSaveAndRestore(bool b)
			{
				can_save_and_restore=b;
			}
			bool GetCanSaveAndRestore() const
			{
				return can_save_and_restore;
			}
			//! Get the mask of bits representing supported features.
			const RenderingFeatures &GetRenderingFeatures() const
			{
				return renderingFeatures;
			}
			//! Returns true if all the specified feature bits are supported.
			bool HasRenderingFeatures(RenderingFeatures r) const;
			//! Returns the name of the render platform - DirectX 11, OpenGL, etc.
			virtual const char *GetName() const = 0;
			virtual std::string GetPathName() const;
			virtual RenderPlatformType GetType() const = 0;
			virtual const char *GetSfxConfigFilename() const 
			{
				return "";
			}
			//virtual void *GetNativeDevicePointer()=0
			//! Returns the DX12 graphics command list
			virtual ID3D12GraphicsCommandList* AsD3D12CommandList();
			virtual ID3D12Device* AsD3D12Device();
			virtual ID3D11Device *AsD3D11Device();
			virtual vk::Device *AsVulkanDevice();
			virtual vk::Instance* AsVulkanInstance();
			//! Platform-dependent function called when initializing the Render Platform.
			virtual void RestoreDeviceObjects(void*);
			//! Platform-dependent function called when uninitializing the Render Platform.
			virtual void InvalidateDeviceObjects();
			//! Platform-dependent function to reload the shaders - only use this for debug purposes.
			virtual void RecompileShaders	();
			//! Implementations of RenderPlatform will cache the API state in order to reduce driver overhead.
			//! But we can't always be sure that external render code hasn't modified the API state. So by calling SynchronizeCacheAndState()
			//! the API state is forced to the cached state. This can be called at the start of Renderplatform's rendering per-frame.
			virtual void SynchronizeCacheAndState(crossplatform::DeviceContext &) {}
			//! Gets an object containing immediate-context API-specific values.
			virtual GraphicsDeviceContext &GetImmediateContext();
			//! Gets an object containing the current global compute context.
			ComputeDeviceContext &GetComputeDeviceContext() { return computeContext; }
			//! Push the given file path onto the texture path stack.
			virtual void PushTexturePath	(const char *pathUtf8);
			//! Remove a path from the top of the texture path stack.
			virtual void PopTexturePath		();
			//! Returns the stack of shader source paths.
			std::vector<std::string> GetShaderPathsUtf8();
			//! Replace the entire stack of shader source paths.
			void SetShaderPathsUtf8(const std::vector<std::string> &pathsUtf8);
			//! Push the given file path onto the shader path stack.
			void PushShaderPath(const char *path_utf8);
			//! Remove a path from the top of the shader source path stack.
			void PopShaderPath();
			//! Set the path where generated shader binaries should be saved, and where stored shader binaries should be loaded from.
			//! Push the given file path onto the shader path stack.
			void PushShaderBinaryPath(const char* path_utf8);
			//! Remove a path from the top of the shader source path stack.
			void PopShaderBinaryPath();
			//! Returns the path where generated shader binaries should be saved, and where stored shader binaries should be loaded from.
			std::vector<std::string> GetShaderBinaryPathsUtf8();
			//! When shaders should be built, or loaded if available.
			void SetShaderBuildMode			(ShaderBuildMode s);
			//! When shaders should be built, or loaded if available.
			ShaderBuildMode GetShaderBuildMode() const;
			//! Clear/reload all textures stored in the main list.
			void ClearTextures();
			//! For platforms that support named events, e.g. PIX in DirectX. Use BeginEvent(), EndEvent() as pairs.
			virtual void BeginEvent			(DeviceContext &deviceContext,const char *name);
			//! For platforms that support named events, e.g. PIX in DirectX. Use BeginEvent(), EndEvent() as pairs.
			virtual void EndEvent			(DeviceContext &);
			//! Mark the beginning of a frame, global for all DeviceContexts that will use this RenderPlatform instance.
			//! The frame number will be incremented.
			virtual void BeginFrame();
			//! Mark the beginning of a frame, global for all DeviceContexts that will use this RenderPlatform instance.
			//! The frame number will be the value specified in f.
			void BeginFrame					(long long f);
			//! Mark the end of a frame, global for all DeviceContexts that use this RenderPlatform instance.
			//! There should be no further rendering or compute until BeginFrame() has been called again.
			virtual void EndFrame			();
			bool						FrameStarted() const;
			long long					GetFrameNumber() const;
			//! Makes sure the resource is in the required state specified by transition. 
			virtual void ResourceTransition (DeviceContext &, crossplatform::Texture *, ResourceTransition ) {};
			//! Ensures that all UAV read and write operation to the textures are completed.
			virtual void ResourceBarrierUAV (DeviceContext& deviceContext, crossplatform::Texture* texture) {};
			//! Ensures that all UAV read and write operation to the PlatformStructuredBuffer are completed.
			virtual void ResourceBarrierUAV (DeviceContext& deviceContext, PlatformStructuredBuffer* sb) {};
			//! Copy a given texture to another.
			virtual void CopyTexture		(DeviceContext &,crossplatform::Texture *dst,crossplatform::Texture *src){};
			//! Execute the currently applied compute shader.
			virtual void DispatchCompute	(DeviceContext &deviceContext,int w,int l,int d)=0;
			//! Execute the currently applied raytracing shaders.
			virtual void DispatchRays		(DeviceContext &deviceContext, const uint3 &dispatch, const crossplatform::ShaderBindingTable* sbt = nullptr){}
			//! Add a signal command to the CPU thread or GPU queue.
			virtual void Signal				(DeviceContextType &type, Fence::Signaller signaller, Fence *fence){}
			//! Add a wait command to the CPU thread or GPU queue. 
			virtual void Wait				(DeviceContextType &type, Fence::Waiter waiter, Fence *fence, uint64_t timeout_nanoseconds = UINT64_MAX){}
			//! Check the status of the fence. Returns true is fence is completed.
			virtual bool GetFenceStatus		(crossplatform::Fence* fence) { return false; }
			//! Execute all previous commands. You must call RestartCommands() to continue rendering after adding in synchronisation.
			virtual void ExecuteCommands	(DeviceContext &deviceContext){};
			//! Restart the commands for rendering after calling ExcuteCommands(). 
			virtual void RestartCommands	(DeviceContext &deviceContext){};
			//! Clear the current render target (i.e. the screen). In most API's this is simply a case of drawing a full-screen quad in the specified rgba colour.
			virtual void Clear				(GraphicsDeviceContext &deviceContext,vec4 colour_rgba);
			//! Draw the specified number of vertices.
			virtual void Draw				(GraphicsDeviceContext &deviceContext,int num_verts,int start_vert)=0;
			//! Draw the specified number of vertices using the bound index arrays.
			virtual void DrawIndexed		(GraphicsDeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0)=0;
			virtual void DrawLine			(GraphicsDeviceContext &deviceContext,const float *pGlobalBasePosition, const float *pGlobalEndPosition,const float *colour,float width);
		
			virtual void DrawLineLoop		(GraphicsDeviceContext &,const double *,int ,const double *,const float [4]){}

			virtual void DrawTexture		(GraphicsDeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend=false,float gamma=1.0f,bool debug=false);
			void DrawTexture				(GraphicsDeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f,bool blend=false,float gamma=1.0f,bool debug=false);
			void DrawDepth					(GraphicsDeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v=NULL,const float *proj=NULL);
			// Draw an onscreen quad without passing vertex positions, but using the "rect" constant from the shader to pass the position and extent of the quad.
			virtual void DrawQuad			(GraphicsDeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique,const char *pass=NULL);
			virtual void DrawQuad			(GraphicsDeviceContext &){}

			//! Print at the specified position, returns the number of lines.
			virtual int Print				(GraphicsDeviceContext &deviceContext,int x,int y,const char *text,const float* colr=NULL,const float* bkg=NULL);
			//! Print diagnostics, starting from the top, and going down the screen one line each time as the frame progresses, then restarting next frame.
			void LinePrint					(GraphicsDeviceContext &deviceContext,const char *text,const float* colr=NULL,const float* bkg=NULL);
			virtual void DrawLines			(GraphicsDeviceContext &,PosColourVertex * /*lines*/,int /*count*/,bool /*strip*/=false,bool /*test_depth*/=false,bool /*view_centred*/=false){}
			void Draw2dLine					(GraphicsDeviceContext &deviceContext,vec2 pos1,vec2 pos2,vec4 colour);
			virtual void Draw2dLines		(GraphicsDeviceContext &/*deviceContext*/,PosColourVertex * /*lines*/,int /*vertex_count*/,bool /*strip*/){}
			/// Draw a circle facing the viewer at the specified direction and angular size.
			virtual void DrawCircle			(GraphicsDeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false);
			/// Draw a circle in 3D space at pos
			virtual void DrawCircle			(GraphicsDeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill=false);
			/// Draw a cubemap as a sphere at the specified screen position and size.
			virtual void DrawCubemap		(GraphicsDeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float size,float exposure,float gamma,float displayLod=0.0f);
			void							DrawAxes(GraphicsDeviceContext &deviceContext,mat4 &m,float size);
			virtual void PrintAt3dPos		(GraphicsDeviceContext &deviceContext,const float *p,const char *text,const float* colr,const float* bkg=nullptr,int offsetx=0,int offsety=0,bool centred=false);
			virtual void SetModelMatrix		(GraphicsDeviceContext &deviceContext,const double *mat,const crossplatform::PhysicalLightRenderData &physicalLightRenderData);
			virtual void					ApplyDefaultMaterial			(){}
			/// Create a platform-specific material instance.
			Material						*GetOrCreateMaterial(const char *name);
			/// Create a platform specific raytracing acceleration structure.
			virtual BottomLevelAccelerationStructure* CreateBottomLevelAccelerationStructure();
			/// Create a platform specific raytracing acceleration structure.
			virtual TopLevelAccelerationStructure* CreateTopLevelAccelerationStructure();
			/// Create a platform agnostic raytracing acceleration structure maanger.
			AccelerationStructureManager*	CreateAccelerationStructureManager();
			/// Create a platform agnostic raytracing shader binding table.
			virtual ShaderBindingTable*		CreateShaderBindingTable();
			/// Create a platform-specific mesh instance.
			virtual Mesh					*CreateMesh						();
			/// Create a texture of the given file or name. If filename exists, it will be loaded.
			/// Otherwise just created. Textures created with this function are owned by the RenderPlatform.
			Texture							*GetOrCreateTexture(const char* filename, bool gen_mips=false);
			/// Create a platform-specific texture instance. Textures created with this function are owned by the caller.
			Texture							*CreateTexture					(const char *lFileNameUtf8=nullptr ,bool gen_mips=false);
			/// called automatically from invalidate to clear the texture from e.g. the unfinishedTextures list. Do not call this directly.
			void							InvalidatingTexture(Texture *t);
			/// Create a platform-specific framebuffer instance - i.e. an optional colour and an optional depth rendertarget. Optionally takes a name string.
			virtual BaseFramebuffer			*CreateFramebuffer				(const char * =nullptr)	=0;
			/// Create a platform-specific sampler state instance.
			virtual SamplerState			*CreateSamplerState				(SamplerStateDesc *)	=0;
			/// Look for a sampler state of the stated name, and create one if it does not exist. The resulting state will be owned by the RenderPlatform, so do not destroy it.
			/// This is for states that will be shared by multiple shaders. There will be a warning if a description is passed that conflicts with the current definition,
			/// as the Effects system assumes that SamplerState names are unique.
			SamplerState					*GetOrCreateSamplerStateByName	(const char *name_utf8,platform::crossplatform::SamplerStateDesc *desc=0);
			/// Destroy the effect when it is safe to do so. The pointer can now be reassigned or nulled.
			void							Destroy(Effect *&e);
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					()=0;
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					(const char *filename_utf8);
			/// Get the effect named, or return null if it's not been created.
			Effect							*GetEffect						(const char *name_utf8);
			/// Create a platform-specific constant buffer instance. This is not usually used directly, instead, create a
			/// platform::crossplatform::ConstantBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;
			/// Create a platform-specific structured buffer instance. This is not usually used directly, instead, create a
			/// platform::crossplatform::StructuredBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformStructuredBuffer	*CreatePlatformStructuredBuffer	()	=0;
			/// Create a platform-specific buffer instance - e.g. vertex buffers, index buffers etc.
			virtual Buffer					*CreateBuffer					()	=0;
			/// Create a platform-specific layout instance based on the given layout description \em layoutDesc and buffer \em buffer.
			virtual Layout					*CreateLayout					(int num_elements,const LayoutDesc *layoutDesc,bool interleaved);
			/// Create a platform-specific RenderState object - e.g. a Blend state, Depth state, etc.
			virtual RenderState				*CreateRenderState				(const RenderStateDesc &desc);
			/// Create an API-specific query object, e.g. for occlusion or timing tests.
			virtual Query					*CreateQuery					(QueryType q)=0;
			virtual Fence					*CreateFence(const char* name){return nullptr;}
			/// Get or create an API-specific shader object.
			virtual Shader					*EnsureShader(const char *filenameUtf8, ShaderType t);
			virtual Shader					*EnsureShader(const char *filenameUtf8, const void *sfxb_ptr, size_t inline_offset, size_t inline_length, ShaderType t);
			/// Create a shader.
			virtual Shader					*CreateShader()=0;
			virtual DisplaySurface*         CreateDisplaySurface();

			virtual GpuProfiler*			CreateGpuProfiler();
			// API stuff: these are the main API-call replacements, corresponding to devicecontext calls in DX11:
			/// Activate the specifided vertex buffers in preparation for rendering.
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,const Buffer *const*buffers,const crossplatform::Layout *layout,const int *vertexSteps=NULL);
			virtual void					ClearVertexBuffers				(DeviceContext& deviceContext);
			/// Graphics hardware can write to vertex buffers using vertex and geometry shaders; use this function to set the target buffer.
			virtual void					SetStreamOutTarget				(GraphicsDeviceContext &,Buffer *,int =0){}

			virtual void					ApplyDefaultRenderTargets		(crossplatform::GraphicsDeviceContext &){};
			/// Make the specified rendertargets and optional depth target active.
			virtual void					ActivateRenderTargets			(GraphicsDeviceContext &deviceContext,int num,Texture **targs,Texture *depth)=0;
			virtual void                    ActivateRenderTargets			(GraphicsDeviceContext &, TargetsAndViewport* );
			virtual void					DeactivateRenderTargets			(GraphicsDeviceContext &deviceContext) ;
			virtual void					SetViewports					(GraphicsDeviceContext &deviceContext,int num,const Viewport *vps);
			//! Set the scissor rectange: x,y, width and height. NOTE: NOT left,right,top,bottom!
			virtual void					SetScissor						(GraphicsDeviceContext &deviceContext,int4 sc);
			//! Get the scissor rectange: x,y, width and height. NOTE: NOT left,right,top,bottom!
			virtual int4					GetScissor						(GraphicsDeviceContext &deviceContext) const;
			/// Get the viewport at the given index.
			virtual Viewport				GetViewport						(GraphicsDeviceContext &deviceContext,int index);
			/// Activate the specified index buffer in preparation for rendering.
			virtual void					SetIndexBuffer					(GraphicsDeviceContext &deviceContext,const Buffer *buffer);
			//! Set the topology for following draw calls, e.g. TRIANGLELIST etc.
			virtual void					SetTopology						(GraphicsDeviceContext &deviceContext,Topology t);

			virtual void					SetTexture						(DeviceContext& deviceContext, const ShaderResource& res, Texture* tex, int array_idx = -1, int mip = -1);
			virtual void					SetUnorderedAccessView			(DeviceContext& deviceContext, const ShaderResource& res, Texture* tex, int index = -1, int mip = -1);
			//! Set the layout for following draw calls - format of the vertex buffer.
			virtual void					SetLayout						(GraphicsDeviceContext &deviceContext,Layout *l);

			virtual void					SetConstantBuffer				(DeviceContext& deviceContext,ConstantBufferBase *s);

			/// <summary>
			/// Apply a structured buffer for use in a shader.
			/// </summary>
			/// <param name="deviceContext"></param>
			/// <param name="s"></param>
			virtual void					SetStructuredBuffer				(DeviceContext& deviceContext, BaseStructuredBuffer* s,  const ShaderResource& shaderResource);
			///
			virtual void					SetAccelerationStructure		(DeviceContext& deviceContext, const ShaderResource& res, TopLevelAccelerationStructure* a);
			/// This function is called to ensure that the named shader is compiled.
			virtual void					EnsureEffectIsBuilt				(const char *filename_utf8);

			/// <summary>
			/// Apply the specified effect pass for use in a draw or compute call. Must be followed by UnapplyPass() when done.
			/// </summary>
			virtual void					ApplyPass						(DeviceContext& deviceContext, EffectPass* pass);

			/// <summary>
			/// Unapply the previously applied effect pass after use in a draw or compute call.
			/// </summary>
			virtual void					UnapplyPass						(DeviceContext& deviceContext);

			/// Called to store the render state - blending, depth check, etc. - for later retrieval with RestoreRenderState.
			/// Some platforms may not support this.
			virtual void					StoreRenderState				(DeviceContext &){}
			/// Called to restore the render state previously stored with StoreRenderState. There must be exactly one call of RestoreRenderState
			/// for each StoreRenderState call, and they are not expected to be nested.
			virtual void					RestoreRenderState				(DeviceContext &){}
			/// Apply the RenderState to the device context - e.g. blend state, depth masking etc.
			virtual void					SetRenderState					(DeviceContext &deviceContext,const RenderState *s)=0;
			/// Apply a standard renderstate - e.g. opaque blending
			virtual void					SetStandardRenderState			(DeviceContext &deviceContext,StandardRenderState s);
			//! Store the current rendertargets and viewports at the top of the stack
			virtual void					PushRenderTargets				(GraphicsDeviceContext &deviceContext, TargetsAndViewport *tv);
			//! Restore rendertargets and viewports from the top of the stack.
			virtual void					PopRenderTargets				(GraphicsDeviceContext &deviceContext);
			//! Resolve an MSAA texture to a normal texture.
			virtual void					Resolve							(GraphicsDeviceContext &,Texture * /*destination*/,Texture * /*source*/){}

			void							LatLongTextureToCubemap			(DeviceContext &deviceContext,Texture *destination,Texture *source);
			//! Save a texture to disk.
			virtual void					SaveTexture						(Texture *,const char *){}
			/// Clear the contents of the given texture to the specified colour
			virtual void					ClearTexture					(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,const vec4& colour);

			/// Fill in mipmaps from the zero level down.
			virtual void					GenerateMips					(GraphicsDeviceContext &deviceContext,Texture *t,bool wrap,int array_idx=-1);
		
			//! Query for the texture value at the specified position in the texture. On most API's, the query will have a few frames' latency.
			vec4							TexelQuery						(DeviceContext &deviceContext,int query_id,uint2 pos,Texture *texture);
			virtual void					WaitForGpu						(DeviceContext &){}
			virtual void					WaitForFencedResources			(DeviceContext &){}
			virtual void					RestoreColourTextureState		(DeviceContext& deviceContext, crossplatform::Texture* tex) {}
			virtual void					RestoreDepthTextureState		(DeviceContext& deviceContext, crossplatform::Texture* tex) {}
			virtual void					InvalidCachedFramebuffersAndRenderPasses() {};

			//! Get the memory allocator - used in particular where API's allocate memory directly.
			platform::core::MemoryInterface *GetAllocator()
			{
				return &allocator;
			}
			std::map<std::string, crossplatform::Material*> &GetMaterials()
			{
				return materials;
			}
			std::map<std::string, crossplatform::Texture*>& GetTextures()
			{
				return textures;
			}
			//! This was introduced because Unity's deferred renderer flips the image vertically sometime after we render.
			bool mirrorY, mirrorY2, mirrorYText;
			crossplatform::Effect *solidEffect;
			crossplatform::Effect *copyEffect;
			std::map<std::string,crossplatform::Material*> materials;
			std::map<std::string, crossplatform::Texture*> textures;
			std::vector<std::string> GetTexturePathsUtf8(); 
			platform::core::MemoryInterface *GetMemoryInterface();
			void SetMemoryInterface(platform::core::MemoryInterface *m);
			crossplatform::Effect *GetDebugEffect();
			ConstantBuffer<DebugConstants> &GetDebugConstantBuffer();
			// Does the format use stencil?
			static PixelFormat ToColourFormat(PixelFormat f);
			static bool IsDepthFormat(PixelFormat f);
			static bool IsStencilFormat(PixelFormat f);
			// Track resources for debugging:
			static std::map<unsigned long long,std::string> ResourceMap;
			
		protected:
			// to be called as soon as possible in the frame, for the first available GraphicsDeviceContext.
			virtual void ContextFrameBegin(GraphicsDeviceContext&);
			Allocator allocator;
			void FinishLoadingTextures(DeviceContext& deviceContext);
			void FinishGeneratingTextureMips(DeviceContext& deviceContext);
			std::set<Texture*> unfinishedTextures;
			std::set<Texture*> unMippedTextures;
			/// Create a platform-specific texture instance. Textures created with this function are owned by the caller.
			virtual Texture* createTexture() = 0;
			platform::core::MemoryInterface *memoryInterface;
			std::vector<std::string> shaderPathsUtf8;
			std::vector<std::string> texturePathsUtf8;
			std::vector<std::string> shaderBinaryPathsUtf8;
			std::map<std::string,SamplerState*> sharedSamplerStates;

			ShaderBuildMode					shaderBuildMode;
			GraphicsDeviceContext			immediateContext;
			ComputeDeviceContext			computeContext;
			// All for debug Effect
			crossplatform::Effect			*debugEffect=nullptr;
			crossplatform::Effect			*mipEffect=nullptr;
			crossplatform::EffectTechnique	*textured;
			crossplatform::EffectTechnique	*untextured;
			crossplatform::EffectTechnique	*showVolume;
			crossplatform::ShaderResource	volumeTexture;
			crossplatform::ShaderResource	imageTexture;
			//

			crossplatform::ConstantBuffer<DebugConstants> debugConstants;
		
			crossplatform::StructuredBuffer<vec4> textureQueryResult;
			crossplatform::GpuProfiler		*gpuProfiler=nullptr;
			bool							gpuProfileFrameStarted = false;
			bool can_save_and_restore;
			//! Last frame number
			long long					mLastFrame=-1;
			bool						frame_started=false;
			long long					frameNumber = 0;
			std::set<crossplatform::Texture*> fencedTextures;
			virtual void ResetImmediateCommandList() {}
		public:
			std::set< Effect*> destroyEffects;
			std::map<std::string, Effect*> effects;
			// all shaders are stored here and referenced by techniques.
			std::map<std::string, Shader*> shaders;
			std::unordered_map<const void *,ContextState *> contextState;
			crossplatform::GpuProfiler		*GetGpuProfiler();
			TextRenderer					*textRenderer;
			std::map<StandardRenderState,RenderState*> standardRenderStates;
		};

		/// Draw a horizontal grid in 3D.
		///
		/// \param [in,out]	deviceContext	Context for the device.
		/// \param [in]	centrePos	Origin of the grid in 3D.
		/// \param [in]	square_size	Spacing between lines - in whatever units the renderer is working in.
		/// \param [in]	brightness 	Brightness of the lines.
		/// \param [in]	numLines	Number of gridlines to draw.
		extern SIMUL_CROSSPLATFORM_EXPORT void DrawGrid(crossplatform::GraphicsDeviceContext &deviceContext, vec3 centrePos, float square_size, float brightness, int numLines);
		// Clang works differently to VC++:
#if !defined( _MSC_VER ) || defined(_GAMING_XBOX)
		template<class T> void ConstantBuffer<T>::RestoreDeviceObjects(RenderPlatform *p)
		{
			InvalidateDeviceObjects();
			if (p)
			{
				platformConstantBuffer = p->CreatePlatformConstantBuffer();
				platformConstantBuffer->RestoreDeviceObjects(p, sizeof(T), (T*)this);
			}
		}

		template<class T> void ConstantBuffer<T>::LinkToEffect(Effect *effect, const char *name)
		{
			if (IsLinkedToEffect(effect))
				return;
			if (effect&&platformConstantBuffer)
			{
				defaultName=name;
				platformConstantBuffer->LinkToEffect(effect, name, T::bindingIndex);
				linkedEffects.insert(effect);
				effect->StoreConstantBufferLink(this);
			}
		}

		template<class T> bool ConstantBuffer<T>::IsLinkedToEffect(crossplatform::Effect *effect)
		{
			if(!effect)
				return false;
			if (linkedEffects.find(effect) != linkedEffects.end())
			{
				if (effect->IsLinkedToConstantBuffer(this))
					return true;
			}
			return false;
		}
		template<class T, BufferUsageHint bufferUsageHint> void StructuredBuffer<T, bufferUsageHint>::RestoreDeviceObjects(RenderPlatform* p, int ct, bool computable, bool cpu_read, T* data, const char* n)
		{
			count = ct;
			if (!platformStructuredBuffer)
				platformStructuredBuffer = p->CreatePlatformStructuredBuffer();
			else
				platformStructuredBuffer->InvalidateDeviceObjects();
			platformStructuredBuffer->RestoreDeviceObjects(p, count, sizeof(T), computable, cpu_read, data, n, bufferUsageHint);
		}
#endif
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#endif
