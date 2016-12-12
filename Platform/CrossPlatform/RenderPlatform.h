#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include <vector>
#include "Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Base/MemoryInterface.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Topology.h"
#include "Simul/Platform/CrossPlatform/SL/Cppsl.hs"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/SL/debug_constants.sl"
#include "Simul/Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif
struct ID3D11Device;
struct VertexXyzRgba
{
	float x,y,z;
	float r,g,b,a;
};
namespace simul
{
	/// The namespace and library for cross-platform base classes, which abstract rendering functionality.
	namespace crossplatform
	{
		class GpuProfiler;
		class Material;
		class Effect;
		class EffectTechnique;
		class TextRenderer;
		struct EffectDefineOptions;
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
		/// A vertex format for debugging.
		struct PosColourVertex
		{
			vec3 pos;
			vec4 colour;
		};
		/// Given a viewport struct and a texture, get the texture coordinates that viewport represents within the texture.
		vec4 SIMUL_CROSSPLATFORM_EXPORT ViewportToTexCoordsXYWH(const Viewport *vi,const Texture *t);
		/// A base class for API-specific rendering.

		/*! RenderPlatform is an interface that allows Simul's rendering functions to be developed
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
			//! This is called by draw functions to do any lazy updating prior to the actual API draw/dispatch call.
			virtual void ApplyContextState(crossplatform::DeviceContext & /*deviceContext*/,bool /*error_checking*/ =true){}
			virtual Viewport PlatformGetViewport(crossplatform::DeviceContext &deviceContext,int index);
		public:
			virtual void T1(){}
			RenderPlatform(simul::base::MemoryInterface*m=NULL);
			virtual ~RenderPlatform();
			//! Returns the name of the render platform - DirectX 11, OpenGL, etc.
			virtual const char *GetName() const = 0;
			virtual ID3D11Device *AsD3D11Device();
			//! Call this once, when the 3D graphics device has been initialized, and pass the API-specific device pointer/identifier.
			virtual void RestoreDeviceObjects(void*);
			//! Call this once, when the 3d graphics device object is being shut down.
			virtual void InvalidateDeviceObjects();
			//! Optional - call this to recompile the standard shaders.
			virtual void RecompileShaders	();
			//! Implementations of RenderPlatform will cache the API state in order to reduce driver overhead.
			//! But we can't always be sure that external render code hasn't modified the API state. So by calling SynchronizeCacheAndState()
			//! the API state is forced to the cached state. This can be called at the start of Renderplatform's rendering per-frame.
			virtual void SynchronizeCacheAndState(crossplatform::DeviceContext &) {}
			//! Gets an object containing immediate-context API-specific values.
			DeviceContext &GetImmediateContext();
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
			void SetShaderBinaryPath(const char *path_utf8);
			//! Returns the path where generated shader binaries should be saved, and where stored shader binaries should be loaded from.
			const char *GetShaderBinaryPath();
			//! When shaders should be built, or loaded if available.
			void SetShaderBuildMode			(ShaderBuildMode s);
			//! When shaders should be built, or loaded if available.
			ShaderBuildMode GetShaderBuildMode() const;
			//! For platforms that support named events, e.g. PIX in DirectX. Use BeginEvent(), EndEvent() as pairs.
			virtual void BeginEvent			(DeviceContext &deviceContext,const char *name);
			//! For platforms that support named events, e.g. PIX in DirectX. Use BeginEvent(), EndEvent() as pairs.
			virtual void EndEvent			(DeviceContext &deviceContext);
			virtual void StartRender		(DeviceContext &deviceContext)=0;
			virtual void EndRender			(DeviceContext &deviceContext)=0;
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3])		=0;

			virtual void CopyTexture		(DeviceContext &,crossplatform::Texture *,crossplatform::Texture *){};
			//! Execute the currently applied compute shader.
			virtual void DispatchCompute	(DeviceContext &deviceContext,int w,int l,int d)=0;
			//! Clear the current render target (i.e. the screen). In most API's this is simply a case of drawing a full-screen quad in the specified rgba colour.
			virtual void Clear				(DeviceContext &deviceContext,vec4 colour_rgba);
			//! Draw the specified number of vertices.
			virtual void Draw				(DeviceContext &deviceContext,int num_verts,int start_vert)=0;
			//! Draw the specified number of vertices using the bound index arrays.
			virtual void DrawIndexed		(DeviceContext &deviceContext,int num_indices,int start_index=0,int base_vertex=0)=0;
			virtual void DrawMarker			(DeviceContext &deviceContext,const double *matrix)			=0;
			virtual void DrawLine			(crossplatform::DeviceContext &deviceContext,const float *pGlobalBasePosition, const float *pGlobalEndPosition,const float *colour,float width);
			virtual void DrawCrossHair		(DeviceContext &deviceContext,const double *pGlobalPosition)	=0;
			virtual void DrawCamera			(DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)=0;
			virtual void DrawLineLoop		(DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4])=0;

			virtual void DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,vec4 mult,bool blend=false,float gamma=1.0f);
			void DrawTexture				(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f,bool blend=false,float gamma=1.0f);
			void DrawDepth					(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v=NULL,const float *proj=NULL);
			// Draw an onscreen quad without passing vertex positions, but using the "rect" constant from the shader to pass the position and extent of the quad.
			virtual void DrawQuad			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique,const char *pass=NULL);
			virtual void DrawQuad			(DeviceContext &deviceContext)=0;

			virtual void Print				(DeviceContext &deviceContext,int x,int y,const char *text,const float* colr=NULL,const float* bkg=NULL);
			virtual void DrawLines			(DeviceContext &deviceContext,PosColourVertex *lines,int count,bool strip=false,bool test_depth=false,bool view_centred=false)		=0;
			void Draw2dLine					(DeviceContext &deviceContext,vec2 pos1,vec2 pos2,vec4 colour);
			virtual void Draw2dLines		(DeviceContext &deviceContext,PosColourVertex *lines,int vertex_count,bool strip)		=0;
			/// Draw a circle facing the viewer at the specified direction and angular size.
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false);
			/// Draw a circle in 3D space at pos
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *pos,const float *dir,float radius,const float *colr,bool fill=false);
			/// Draw a cubemap as a sphere at the specified screen position and size.
			virtual void DrawCubemap		(DeviceContext &deviceContext,Texture *cubemap,float offsetx,float offsety,float size,float exposure,float gamma,float displayLod=0.0f);
			virtual void PrintAt3dPos		(DeviceContext &deviceContext,const float *p,const char *text,const float* colr,const float* bkg=nullptr,int offsetx=0,int offsety=0,bool centred=false);
			virtual void SetModelMatrix		(crossplatform::DeviceContext &deviceContext,const double *mat,const crossplatform::PhysicalLightRenderData &physicalLightRenderData);
			virtual void					ApplyDefaultMaterial			()	=0;
			/// Create a platform-specific material instance.
			virtual Material				*CreateMaterial					()	=0;
			/// Create a platform-specific mesh instance.
			virtual Mesh					*CreateMesh						()	=0;
			/// Create a platform-specific light instance.
			virtual Light					*CreateLight					()	=0;
			/// Create a platform-specific texture instance.
			virtual Texture					*CreateTexture					(const char *lFileNameUtf8 =nullptr)	=0;
			/// Create a platform-specific framebuffer instance - i.e. an optional colour and an optional depth rendertarget. Optionally takes a name string.
			virtual BaseFramebuffer			*CreateFramebuffer				(const char * =nullptr)	=0;
			/// Create a platform-specific sampler state instance.
			virtual SamplerState			*CreateSamplerState				(SamplerStateDesc *)	=0;
			/// Look for a sampler state of the stated name, and create one if it does not exist. The resulting state will be owned by the RenderPlatform, so do not destroy it.
			/// This is for states that will be shared by multiple shaders. There will be a warning if a description is passed that conflicts with the current definition,
			/// as the Effects system assumes that SamplerState names are unique.
			SamplerState					*GetOrCreateSamplerStateByName	(const char *name_utf8,simul::crossplatform::SamplerStateDesc *desc=0);
			/// Create a platform-specific effect instance.
			Effect							*CreateEffect					(const char *filename_utf8);
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					()=0;
			/// Create a platform-specific effect instance.
			virtual Effect					*CreateEffect					(const char *filename_utf8,const std::map<std::string,std::string> &defines);
			/// Create a platform-specific constant buffer instance. This is not usually used directly, instead, create a
			/// simul::crossplatform::ConstantBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;
			/// Create a platform-specific structured buffer instance. This is not usually used directly, instead, create a
			/// simul::crossplatform::StructuredBuffer, and pass this RenderPlatform's pointer to it in RestoreDeviceObjects().
			virtual PlatformStructuredBuffer	*CreatePlatformStructuredBuffer	()	=0;
			/// Create a platform-specific buffer instance - e.g. vertex buffers, index buffers etc.
			virtual Buffer					*CreateBuffer					()	=0;
			/// Create a platform-specific layout instance based on the given layout description \em layoutDesc and buffer \em buffer.
			virtual Layout					*CreateLayout					(int num_elements,const LayoutDesc *layoutDesc)	=0;
			/// Create a platform-specific RenderState object - e.g. a Blend state, Depth state, etc.
			virtual RenderState				*CreateRenderState				(const RenderStateDesc &desc)=0;
			/// Create an API-specific query object, e.g. for occlusion or timing tests.
			virtual Query					*CreateQuery					(QueryType q)=0;
			// API stuff: these are the main API-call replacements, corresponding to devicecontext calls in DX11:
			/// Activate the specifided vertex buffers in preparation for rendering.
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,Buffer **buffers,const crossplatform::Layout *layout,const int *vertexSteps=NULL)=0;
			/// Graphics hardware can write to vertex buffers using vertex and geometry shaders; use this function to set the target buffer.
			virtual void					SetStreamOutTarget				(DeviceContext &deviceContext,Buffer *buffer,int start_index=0)=0;
			/// Make the specified rendertargets and optional depth target active.
			virtual void					ActivateRenderTargets			(DeviceContext &deviceContext,int num,Texture **targs,Texture *depth)=0;
			virtual void					DeactivateRenderTargets			(DeviceContext &deviceContext) =0;
			virtual void					SetViewports(DeviceContext &deviceContext,int num,const Viewport *vps)=0;
			/// Get the viewport at the given index.
			virtual Viewport				GetViewport(DeviceContext &deviceContext,int index);
			/// Activate the specified index buffer in preparation for rendering.
			virtual void					SetIndexBuffer					(DeviceContext &deviceContext,Buffer *buffer)=0;
			//! Set the topology for following draw calls, e.g. TRIANGLELIST etc.
			virtual void					SetTopology						(DeviceContext &deviceContext,Topology t)=0;
			//! Set the layout for following draw calls - format of the vertex buffer.
			virtual void					SetLayout						(DeviceContext &deviceContext,Layout *l);
			/// This function is called to ensure that the named shader is compiled with all the possible combinations of \#define's given in \em options.
			virtual void					EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<EffectDefineOptions> &options);
			/// Called to store the render state - blending, depth check, etc. - for later retrieval with RestoreRenderState.
			/// Some platforms may not support this.
			virtual void					StoreRenderState				(DeviceContext &deviceContext)=0;
			/// Called to restore the render state previously stored with StoreRenderState. There must be exactly one call of RestoreRenderState
			/// for each StoreRenderState call, and they are not expected to be nested.
			virtual void					RestoreRenderState				(DeviceContext &deviceContext)=0;
			/// Apply the RenderState to the device context - e.g. blend state, depth masking etc.
			virtual void					SetRenderState					(DeviceContext &deviceContext,const RenderState *s)=0;
			/// Apply a standard renderstate - e.g. opaque blending
			virtual void					SetStandardRenderState			(DeviceContext &deviceContext,StandardRenderState s);
			//! Store the current rendertargets and viewports at the top of the stack
			virtual void					PushRenderTargets(DeviceContext &deviceContext)=0;
			//! Restore rendertargets and viewports from the top of the stack.
			virtual void					PopRenderTargets(DeviceContext &deviceContext)=0;
			//! Resolve a MSAA texture to a normal texture.
			virtual void					Resolve(DeviceContext &deviceContext,Texture *destination,Texture *source)=0;
			//! Save a texture to disk.
			virtual void					SaveTexture(Texture *texture,const char *lFileNameUtf8)=0;
			/// Clear the contents of the given texture to the specified colour
			virtual void					ClearTexture(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *texture,const vec4& colour);
			//! Query for the texture value at the specified position in the texture. On most API's, the query will have a few frames' latency.
			vec4							TexelQuery(DeviceContext &deviceContext,int query_id,uint2 pos,Texture *texture);
			//! This was introduced because Unity's deferred renderer flips the image vertically sometime after we render.
			bool mirrorY, mirrorY2, mirrorYText;
			crossplatform::Effect *solidEffect;
			crossplatform::Effect *copyEffect;
			std::set<crossplatform::Material*> materials;
			std::vector<std::string> GetTexturePathsUtf8();
			simul::base::MemoryInterface *GetMemoryInterface();
			void SetMemoryInterface(simul::base::MemoryInterface *m);
			crossplatform::Effect *GetDebugEffect();
			ConstantBuffer<DebugConstants> &GetDebugConstantBuffer();
			ConstantBuffer<SolidConstants> &GetSolidConstantBuffer();
		protected:
			simul::base::MemoryInterface *memoryInterface;
			std::vector<std::string> shaderPathsUtf8;
			std::vector<std::string> texturePathsUtf8;
			std::string shaderBinaryPathUtf8;
			std::map<std::string,SamplerState*> sharedSamplerStates;

			ShaderBuildMode					shaderBuildMode;
			DeviceContext					immediateContext;

			// All for debug Effect
			crossplatform::Effect			*debugEffect;
			crossplatform::EffectTechnique	*textured;
			crossplatform::EffectTechnique	*showVolume;
			crossplatform::ShaderResource	volumeTexture;
			crossplatform::ShaderResource	imageTexture;
			//

			crossplatform::ConstantBuffer<DebugConstants> debugConstants;
			crossplatform::ConstantBuffer<SolidConstants> solidConstants;
			
			crossplatform::StructuredBuffer<vec4> textureQueryResult;
			crossplatform::GpuProfiler		*gpuProfiler;
		public:		
			crossplatform::GpuProfiler		*GetGpuProfiler();
			TextRenderer					*textRenderer;
			std::map<StandardRenderState,RenderState*> standardRenderStates;
			void							EnsureEffectIsBuiltPartialSpec	(const char *filename_utf8,const std::vector<EffectDefineOptions> &options,const std::map<std::string,std::string> &defines);
		};

		/// Draw a horizontal grid in 3D.
		///
		/// \param [in,out]	deviceContext	Context for the device.
		/// \param [in]	centrePos	Origin of the grid in 3D.
		/// \param [in]	square_size	Spacing between lines - in whatever units the renderer is working in.
		/// \param [in]	brightness 	Brightness of the lines.
		/// \param [in]	numLines	Number of gridlines to draw.
		extern SIMUL_CROSSPLATFORM_EXPORT void DrawGrid(crossplatform::DeviceContext &deviceContext, vec3 centrePos, float square_size, float brightness, int numLines);
		// Clang works differently to VC++:
#ifndef _MSC_VER
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
		template<class T> void StructuredBuffer<T>::RestoreDeviceObjects(RenderPlatform *p, int ct, bool computable, T *data)
		{
			count = ct;
			delete platformStructuredBuffer;
			platformStructuredBuffer = NULL;
			platformStructuredBuffer = p->CreatePlatformStructuredBuffer();
			platformStructuredBuffer->RestoreDeviceObjects(p, count, sizeof(T), computable, data);
		}
#endif
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif
