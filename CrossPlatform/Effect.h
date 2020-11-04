#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/CrossPlatform/Topology.h"
#include "Platform/CrossPlatform/Layout.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/Query.h"
#include "Platform/CrossPlatform/PlatformStructuredBuffer.h"
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <set>
#include <stdint.h>
struct ID3DX11Effect;
struct ID3DX11EffectTechnique;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
typedef unsigned int GLuint;
struct D3D12_CPU_DESCRIPTOR_HANDLE;
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		/*!
				OpenGL					|	Direct3D
				-------------------------------------------
				Vertex Shader			|	Vertex Shader
				Tessellation Control	|	Hull Shader
				Tessellation Evaluation	|	Domain Shader
				Geometry Shader			|	Geometry Shader
				Fragment Shader			|	Pixel Shader
				Compute Shader			|	Compute Shader
		*/
		enum ShaderType
		{
			SHADERTYPE_VERTEX,
			SHADERTYPE_HULL,		// tesselation control.
			SHADERTYPE_DOMAIN,		// tesselation evaluation.
			SHADERTYPE_GEOMETRY,
			SHADERTYPE_PIXEL,
			SHADERTYPE_COMPUTE,
			SHADERTYPE_COUNT
		};
		/// Tells the renderer what to do with shader source to get binaries. values can be combined, e.g. ALWAYS_BUILD|TRY_AGAIN_ON_FAIL
		enum ShaderBuildMode
		{
			NEVER_BUILD=0
			,ALWAYS_BUILD=1
			,BUILD_IF_CHANGED=2
			, BREAK_ON_FAIL = 8			// 0x1000
			, TRY_AGAIN_ON_FAIL = 12	// 0x11000 - includes break.
			, DEBUG_SHADERS=16
		};
		inline ShaderBuildMode operator|(ShaderBuildMode a, ShaderBuildMode b)
		{
			return static_cast<ShaderBuildMode>(static_cast<int>(a) | static_cast<int>(b));
		}
		inline ShaderBuildMode operator&(ShaderBuildMode a, ShaderBuildMode b)
		{
			return static_cast<ShaderBuildMode>(static_cast<int>(a) & static_cast<int>(b));
		}
		inline ShaderBuildMode operator~(ShaderBuildMode a)
		{
			return static_cast<ShaderBuildMode>(~static_cast<int>(a));
		}
		struct DeviceContext;
		class RenderPlatform;
		class Effect;
		
		enum BlendOption
		{
			BLEND_ZERO
			,BLEND_ONE
			,BLEND_SRC_COLOR
			,BLEND_INV_SRC_COLOR
			,BLEND_SRC_ALPHA
			,BLEND_INV_SRC_ALPHA
			,BLEND_DEST_ALPHA
			,BLEND_INV_DEST_ALPHA
			,BLEND_DEST_COLOR
			,BLEND_INV_DEST_COLOR
			,BLEND_SRC_ALPHA_SAT
			,BLEND_BLEND_FACTOR
			,BLEND_INV_BLEND_FACTOR
			,BLEND_SRC1_COLOR
			,BLEND_INV_SRC1_COLOR
			,BLEND_SRC1_ALPHA
			,BLEND_INV_SRC1_ALPHA
		};
		enum BlendOperation
		{
			BLEND_OP_NONE
			,BLEND_OP_ADD
			,BLEND_OP_SUBTRACT
			,BLEND_OP_MAX
			,BLEND_OP_MIN
		};
		struct RTBlendDesc
		{
			BlendOperation blendOperation;
			BlendOperation blendOperationAlpha;
			BlendOption SrcBlend;
			BlendOption DestBlend;
			BlendOption SrcBlendAlpha;
			BlendOption DestBlendAlpha;
			unsigned char RenderTargetWriteMask;
		};
		struct BlendDesc
		{
			bool AlphaToCoverageEnable;
			bool IndependentBlendEnable;
			int numRTs;
			RTBlendDesc RenderTarget[8];
		};
		struct DepthStencilDesc
		{
			bool test;
			bool write;
			DepthComparison comparison;
		};
		enum ViewportScissor
		{
			VIEWPORT_SCISSOR_DISABLE      = 0, ///< Disable the scissor rectangle for a viewport.
			VIEWPORT_SCISSOR_ENABLE       = 1, ///< Enable the scissor rectangle for a viewport.
		};
		enum CullFaceMode
		{
			CULL_FACE_NONE              = 0, ///< Disable face culling.
			CULL_FACE_FRONT             = 1, ///< Cull front-facing primitives only.
			CULL_FACE_BACK              = 2, ///< Cull back-facing primitives only.
			CULL_FACE_FRONTANDBACK      = 3, ///< Cull front and back faces.
		};
		enum FrontFace
		{
			FRONTFACE_CLOCKWISE                     = 1, ///< Clockwise is front-facing.
			FRONTFACE_COUNTERCLOCKWISE              = 0, ///< Counter-clockwise is front-facing.
		};
		enum PolygonMode
		{
			POLYGON_MODE_POINT              = 0, ///< Render polygons as points.
			POLYGON_MODE_LINE               = 1, ///< Render polygons in wireframe.
			POLYGON_MODE_FILL               = 2, ///< Render polygons as solid/filled.
		};
		enum PolygonOffsetMode
		{
			POLYGON_OFFSET_ENABLE           = 1, ///< Enable polygon offset.
			POLYGON_OFFSET_DISABLE          = 0, ///< Disable polygon offset.
		};
		struct RasterizerDesc
		{
			ViewportScissor		viewportScissor;
			CullFaceMode		cullFaceMode;
			FrontFace			frontFace;
			PolygonMode			polygonMode;
			PolygonOffsetMode	polygonOffsetMode;
		};
        //! Specifies the bound render target pixel format
        struct RenderTargetFormatDesc
        {
            PixelOutputFormat   formats[8];
        };
		enum RenderStateType
		{
			NONE
			,BLEND
			,DEPTH
			,RASTERIZER
            ,RTFORMAT
			,NUM_RENDERSTATE_TYPES
		};
		//! An initialization structure for a RenderState. Create a RenderStateDesc and pass it to RenderPlatform::CreateRenderState,
		//! then store the returned pointer. Delete the pointer when done.
		struct RenderStateDesc
		{
			RenderStateType type;
			union
			{
				DepthStencilDesc        depth;
				BlendDesc               blend;
				RasterizerDesc          rasterizer;
                RenderTargetFormatDesc  rtFormat;
			};
			std::string name;
		};
		struct SIMUL_CROSSPLATFORM_EXPORT RenderState
		{
			RenderStateDesc desc;
			RenderStateType type;
			std::string name;
			RenderState():type(NONE){}
			virtual ~RenderState(){}
			virtual void InvalidateDeviceObjects() {}
		};
		class SIMUL_CROSSPLATFORM_EXPORT Shader
		{
		public:
			Shader():
				textureSlots(0)
				,textureSlotsForSB(0)
				,rwTextureSlots(0)
				,rwTextureSlotsForSB(0)
				,constantBufferSlots(0)
				,samplerSlots(0)
				,renderPlatform(nullptr)
				{
				}
			virtual ~Shader(){}
			virtual void Release(){}
			virtual void load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void* data, size_t len, crossplatform::ShaderType t) = 0;
			void setUsesTextureSlot(int s);
			void setUsesTextureSlotForSB(int s);
			void setUsesConstantBufferSlot(int s);
			void setUsesSamplerSlot(int s);
			void setUsesRwTextureSlot(int s);
			void setUsesRwTextureSlotForSB(int s);
			bool usesTextureSlot(int s) const;
			bool usesTextureSlotForSB(int s) const;
			bool usesRwTextureSlotForSB(int s) const;
			bool usesConstantBufferSlot(int s) const;
			bool usesSamplerSlot(int s) const;
			bool usesRwTextureSlot(int s) const;
			std::string pixel_str;
			std::string name;
			std::string entryPoint;
			crossplatform::ShaderType type;
			// if it's a vertex shader...
			crossplatform::Layout layout;
			std::unordered_map<int,crossplatform::SamplerState *>	samplerStates;
			unsigned textureSlots;			//t
			unsigned textureSlotsForSB;		//t
			unsigned rwTextureSlots;		//u
			unsigned rwTextureSlotsForSB;	//u
			unsigned constantBufferSlots;	//b
			unsigned samplerSlots;			//s
			crossplatform::RenderPlatform *renderPlatform;
		};
		/// A class representing a shader resource.
		struct SIMUL_CROSSPLATFORM_EXPORT ShaderResource
		{
			ShaderResource():   shaderResourceType(crossplatform::ShaderResourceType::UNKNOWN), 
                                platform_shader_resource(nullptr),
                                slot(-1),
                                dimensions(-1),
                                valid(false)
            {}
			ShaderResourceType  shaderResourceType;
			void*               platform_shader_resource;
			int                 slot;
			int                 dimensions;
			bool                valid;
		};
		class SIMUL_CROSSPLATFORM_EXPORT EffectPass
		{
		public:
			crossplatform::RenderState *blendState;
			crossplatform::RenderState *depthStencilState;
			crossplatform::RenderState *rasterizerState;
            crossplatform::RenderState *renderTargetFormatState;
			Shader* shaders[crossplatform::SHADERTYPE_COUNT];
			Shader* pixelShaders[OUTPUT_FORMAT_COUNT];
            std::string rtFormatState;
			std::string name;
			EffectPass(RenderPlatform *r,Effect *parent);
			virtual ~EffectPass();

			void SetName(const char *n)
			{
				if(n)
					name=n;
			}

			crossplatform::Effect* GetEffect()
			{
				return effect;
			}
			//! This updates the mapping from the pass's list of resources to the effect slot numbers (0-31)
			void MakeResourceSlotMap();

			// These are efficient maps from the shader pass's resources to the effect's resource slots.
			int numResourceSlots;
			unsigned char resourceSlots[32];
			int numRwResourceSlots;
			unsigned char rwResourceSlots[32];
			int numSbResourceSlots;
			unsigned char sbResourceSlots[32];
			int numRwSbResourceSlots;
			unsigned char rwSbResourceSlots[32];
			int numSamplerResourceSlots;
			unsigned char samplerResourceSlots[32];
			int numConstantBufferResourceSlots;
			unsigned char constantBufferResourceSlots[32];

			bool usesTextureSlot(int s) const;
			bool usesTextureSlotForSB(int s) const;
			bool usesRwTextureSlotForSB(int s) const;
			bool usesConstantBufferSlot(int s) const;
			bool usesSamplerSlot(int s) const;
			bool usesRwTextureSlot(int s) const;

			bool usesTextures() const;
			bool usesSBs() const;
			bool usesRwSBs() const;
			bool usesConstantBuffers() const;
			bool usesSamplers() const;
			bool usesRwTextures() const;
			bool shouldFenceOutputs() const
			{
				return should_fence_outputs;
			}
			void setShouldFenceOutputs(bool f) 
			{
				should_fence_outputs=f;
			}
			void SetUsesConstantBufferSlots(unsigned);
			void SetUsesTextureSlots(unsigned);
			void SetUsesTextureSlotsForSB(unsigned);
			void SetUsesRwTextureSlots(unsigned);
			void SetUsesRwTextureSlotsForSB(unsigned);
			void SetUsesSamplerSlots(unsigned);

			void SetConstantBufferSlots(unsigned s)	{ constantBufferSlots = s; }
			void SetTextureSlots(unsigned s)		{ textureSlots = s; }
			void SetTextureSlotsForSB(unsigned s)	{ textureSlotsForSB = s; }
			void SetRwTextureSlots(unsigned s)		{ rwTextureSlots = s; }
			void SetRwTextureSlotsForSB(unsigned s) { rwTextureSlotsForSB = s; }
			void SetSamplerSlots(unsigned s)		{ samplerSlots = s; }

			unsigned GetSamplerSlots()const
			{
				return samplerSlots;
			}
			unsigned GetRwTextureSlots() const
			{
				return rwTextureSlots;
			}
			unsigned GetTextureSlots() const
			{
				return textureSlots;
			}
			unsigned GetRwStructuredBufferSlots() const
			{
				return rwTextureSlotsForSB;
			}
			unsigned GetStructuredBufferSlots() const
			{
				return textureSlotsForSB;
			}
			unsigned GetConstantBufferSlots() const
			{
				return constantBufferSlots;
			}
			void *GetPlatformPass()
			{
				return platform_pass;
			}
			void SetPlatformPass(void *p)
			{
				platform_pass=p;
			}
			void SetTopology(Topology t=Topology::UNDEFINED)
			{
				topology=t;
			}
			Topology GetTopology()
			{
				return topology;
			}
			virtual void Apply(crossplatform::DeviceContext &deviceContext,bool test=false)=0;
		protected:
			unsigned samplerSlots;
			unsigned constantBufferSlots;
			unsigned textureSlots;
			unsigned textureSlotsForSB;
			unsigned rwTextureSlots;
			unsigned rwTextureSlotsForSB;
			bool should_fence_outputs;
			void *platform_pass;
			crossplatform::RenderPlatform *renderPlatform;
			crossplatform::Effect* effect;
			crossplatform::Topology topology=Topology::UNDEFINED;
		};
		class SIMUL_CROSSPLATFORM_EXPORT PlatformConstantBuffer
		{
		protected:
			crossplatform::RenderPlatform *renderPlatform;
			long long validFrame;
			int internalFrameNumber;
			std::string name;
		public:
			PlatformConstantBuffer():renderPlatform(nullptr), validFrame(-1), internalFrameNumber(0){}
			virtual ~PlatformConstantBuffer(){}
			virtual void RestoreDeviceObjects(RenderPlatform *dev,size_t sz,void *addr)=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void LinkToEffect(crossplatform::Effect *effect,const char *name,int bindingIndex)=0;
			virtual void Apply(DeviceContext &deviceContext,size_t size,void *addr)=0;
			virtual void Reapply(DeviceContext &deviceContext,size_t size,void *addr)
			{
				Apply(deviceContext,size,addr);
			}
			virtual void Unbind(DeviceContext &deviceContext)=0;
			void SetChanged()
			{
				validFrame =0;
			}
			void SetName(const char *n)
			{
				name=n;
			}
			/// For RenderPlatform's use only: do not call.
			virtual void ActualApply(simul::crossplatform::DeviceContext &,EffectPass *,int){}
		};
		
		class Texture;
		class SamplerState;
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechnique
		{
		public:
			typedef std::map<std::string,EffectPass *> PassMap;
			typedef std::map<int,EffectPass *> PassIndexMap;
			typedef std::map<std::string,int> IndexMap;
			std::string name;
			PassMap passes_by_name;
			PassIndexMap passes_by_index;
			IndexMap pass_indices;
			EffectTechnique(RenderPlatform *r,Effect *e);
			virtual ~EffectTechnique();
			void *platform_technique;
			inline ID3DX11EffectTechnique *asD3DX11EffectTechnique()
			{
				return (ID3DX11EffectTechnique*)platform_technique;
			}
			virtual GLuint passAsGLuint(int )
			{
				return (GLuint)0;
			}
			virtual GLuint passAsGLuint(const char *)
			{
				return (GLuint)0;
			}
			inline int GetPassIndex(const char *n)
			{
				std::string str(n);
				if(pass_indices.find(str)==pass_indices.end())
					return -1;
				return pass_indices[str];
			}
			bool shouldFenceOutputs() const
			{
				return should_fence_outputs;
			}
			
			void setShouldFenceOutputs(bool f) 
			{
				should_fence_outputs=f;
			}
			bool should_fence_outputs;
			int NumPasses() const;
			virtual EffectPass *AddPass(const char *name,int i)=0;
			EffectPass *GetPass(int i);
			EffectPass *GetPass(const char *name);
		protected:
			RenderPlatform *renderPlatform;
			crossplatform::Effect *effect;
		};
		typedef std::map<std::string,EffectTechnique *> TechniqueMap;
		typedef std::unordered_map<const char *,EffectTechnique *> TechniqueCharMap;
		typedef std::map<int,EffectTechnique *> IndexMap;
		/// Crossplatform equivalent of D3DXEffectGroup - a named group of techniques.
		class SIMUL_CROSSPLATFORM_EXPORT EffectTechniqueGroup
		{
			TechniqueCharMap charMap;
		public:
			~EffectTechniqueGroup();
			TechniqueMap techniques;
			IndexMap techniques_by_index;
			EffectTechnique *GetTechniqueByName(const char *name);
			EffectTechnique *GetTechniqueByIndex(int index);
		};
		/// A crossplatform structure for a \#define and its possible values.
		/// This allows all of the macro combinations to be built to binary.
		struct SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions
		{
			std::string name;
			std::vector<std::string> options;
		};
		class Buffer;

		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1);
		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1,const char *option2);
		extern SIMUL_CROSSPLATFORM_EXPORT EffectDefineOptions CreateDefineOptions(const char *name,const char *option1,const char *option2,const char *option3);
		typedef std::map<std::string,EffectTechniqueGroup *> GroupMap;
		typedef std::unordered_map<const char *,EffectTechniqueGroup *> GroupCharMap;
		/// The cross-platform base class for shader effects.
		class SIMUL_CROSSPLATFORM_EXPORT Effect
		{
		protected:
			crossplatform::RenderPlatform *renderPlatform;
			virtual EffectTechnique *CreateTechnique()=0;
			EffectTechnique *EnsureTechniqueExists(const std::string &groupname,const std::string &techname,const std::string &passname);
			const char *GetTechniqueName(const EffectTechnique *t) const;
			std::set<ConstantBufferBase*> linkedConstantBuffers;
			std::map<std::string,crossplatform::ShaderResource> shaderResources;
			GroupCharMap groupCharMap;
			typedef std::unordered_map<std::string,ShaderResource*> TextureDetailsMap;
			typedef std::unordered_map<const char *,ShaderResource*> TextureCharMap;
			TextureDetailsMap textureDetailsMap;
			ShaderResource *textureResources[32];
			mutable TextureCharMap textureCharMap;
			std::unordered_map<std::string,crossplatform::SamplerState *> samplerStates;
			std::unordered_map<std::string,crossplatform::RenderState *> depthStencilStates;
			std::unordered_map<std::string,crossplatform::RenderState *> blendStates;
			std::unordered_map<std::string,crossplatform::RenderState *> rasterizerStates;
            std::unordered_map<std::string, crossplatform::RenderState *> rtFormatStates;
			SamplerStateAssignmentMap samplerSlots;	// The slots for THIS effect - may not be the sampler's defaults.
			const ShaderResource *GetTextureDetails(const char *name);
			virtual void PostLoad(){}
		public:
			GroupMap groups;
			TechniqueMap techniques;
			TechniqueCharMap techniqueCharMap;
			IndexMap techniques_by_index;
			std::string filename;
			std::string filenameInUseUtf8;
			void *platform_effect;
			Effect();
			virtual ~Effect();
			inline ID3DX11Effect *asD3DX11Effect()
			{
				return (ID3DX11Effect*)platform_effect;
			}
			void SetName(const char *n)
			{
				filename=n;
			}
			const char *GetName()const
			{
				return filename.c_str();
			}
			void InvalidateDeviceObjects();
			virtual void Load(RenderPlatform *renderPlatform,const char *filename_utf8,const std::map<std::string,std::string> &defines);
			void Load(RenderPlatform *r,const char *filename_utf8)
			{
				std::map<std::string,std::string> defines;
				Load(r,filename_utf8,defines);
			}
			virtual void Compile(const char *);
			// Which texture is at this slot. Warning: slow.
			std::string GetTextureForSlot(int s) const
			{
				for(auto i:textureDetailsMap)
				{
					if(i.second->slot==s&&(i.second->shaderResourceType&ShaderResourceType::RW)!=ShaderResourceType::RW)
						return i.first;
					if(i.second->slot+1000==s&&(i.second->shaderResourceType&ShaderResourceType::RW)==ShaderResourceType::RW)
						return i.first;
				}
				return std::string("Unknown");

			}
			const crossplatform::ShaderResource *GetShaderResourceAtSlot(int s) ;
			EffectTechniqueGroup *GetTechniqueGroupByName(const char *name);
			virtual EffectTechnique *GetTechniqueByName(const char *name);
			virtual EffectTechnique *GetTechniqueByIndex(int index)				=0;
			//! Set the texture for read-write access by compute shaders in this effect.
			virtual void SetUnorderedAccessView(DeviceContext &deviceContext,const char *name,Texture *tex,int index=-1,int mip=-1)	;
			virtual ShaderResource GetShaderResource(const char *name);
			//! Set the texture for read-write access by compute shaders in this effect.
			virtual void SetUnorderedAccessView(DeviceContext &deviceContext,const ShaderResource &name,Texture *tex,int index=-1,int mip=-1)	;
			//! Set the texture for this effect. If mip is specified, the specific mipmap will be used, otherwise it's the full texture with all its mipmaps.
			virtual void SetTexture		(DeviceContext &deviceContext,const ShaderResource &name	,Texture *tex,int array_idx=-1,int mip=-1);
			//! Set the texture for this effect. If mip is specified, the specific mipmap will be used, otherwise it's the full texture with all its mipmaps.
			virtual void SetTexture		(DeviceContext &deviceContext,const char *name	,Texture *tex,int array_idx=-1,int mip=-1);
			//! Set the texture for this effect.
			virtual void SetSamplerState(DeviceContext &deviceContext,const ShaderResource &name	,SamplerState *s);
			//! Set a constant buffer for this effect.
			virtual void SetConstantBuffer(DeviceContext &deviceContext,ConstantBufferBase *s);
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,const char *tech_name,const char *pass);
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,const char *tech_name,int pass=0);
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,int pass=0);
			/// Activate the shader. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext &deviceContext,EffectTechnique *effectTechnique,const char *pass);
			/// Apply the specified shader effect pass. Unapply must be called after rendering is done.
			virtual void Apply(DeviceContext& deviceContext, EffectPass* p);
			/// Call Reapply between Apply and Unapply to apply the effect of modified constant buffers etc.
			virtual void Reapply(DeviceContext &deviceContext);
			/// Deactivate the shader.
			virtual void Unapply(DeviceContext &deviceContext);
			/// Zero-out the textures that are set for this shader. Call before apply.
			virtual void UnbindTextures(crossplatform::DeviceContext &deviceContext);

			void StoreConstantBufferLink(crossplatform::ConstantBufferBase *);
			bool IsLinkedToConstantBuffer(crossplatform::ConstantBufferBase*) const;
			
			int GetSlot(const char *name);
			int GetSamplerStateSlot(const char *name);
			int GetDimensions(const char *name);
			crossplatform::ShaderResourceType GetResourceType(const char *name);

			//! Map of sampler states used by this effect
			crossplatform::SamplerStateAssignmentMap& GetSamplers() { return samplerSlots; }
			/// Ensure it's built and up-to-date.
			void EnsureEffect(crossplatform::RenderPlatform *r, const char *filename_utf8);
		};
		class SIMUL_CROSSPLATFORM_EXPORT ConstantBufferBase
		{
		protected:
			PlatformConstantBuffer* platformConstantBuffer;
			std::string defaultName;
		public:
			ConstantBufferBase(const char* name);
			~ConstantBufferBase()
			{
				delete platformConstantBuffer;
			}
			const char* GetDefaultName() const
			{
				return defaultName.c_str();
			}

			PlatformConstantBuffer* GetPlatformConstantBuffer()
			{
				return platformConstantBuffer;
			}
			virtual int GetIndex() const = 0;
			virtual size_t GetSize() const = 0;
			virtual void* GetAddr() const = 0;
		};
		template<class T> class ConstantBuffer :public ConstantBufferBase, public T
		{
			std::set<Effect*> linkedEffects;
		public:
			ConstantBuffer() :ConstantBufferBase(nullptr)
			{
				// Clear out the part of memory that corresponds to the base class.
				// We should ONLY inherit from simple structs.
				memset(((T*)this), 0, sizeof(T));
			}
			~ConstantBuffer()
			{
				InvalidateDeviceObjects();
			}
			const ConstantBuffer<T>& operator=(const T& t)
			{
				T::operator=(t);
				return *this;
			}
			void copyTo(void* pData)
			{
				*(T*)pData = *this;
			}
			/// For Effect's use only, do not call.
			size_t GetSize() const override
			{
				return sizeof(T);
			}
			/// For Effect's use only, do not call.
			void* GetAddr() const override
			{
				return (void*)((T*)this);
			}
			/// Get the binding index in shaders.
			int GetIndex() const override
			{
				return T::bindingIndex;
			}
			//! Create the buffer object.
#if defined( _MSC_VER) && !defined( _GAMING_XBOX )
			void RestoreDeviceObjects(RenderPlatform* p)
			{
				InvalidateDeviceObjects();
				if (p)
				{
					SIMUL_ASSERT(platformConstantBuffer == nullptr);
					if (platformConstantBuffer)
						delete platformConstantBuffer;
					platformConstantBuffer = p->CreatePlatformConstantBuffer();
					platformConstantBuffer->SetName(typeid(T).name());
					platformConstantBuffer->RestoreDeviceObjects(p, sizeof(T), (T*)this);
				}
			}
			void LinkToEffect(Effect* effect, const char* name)
			{
				if (!effect)
					return;
				if (IsLinkedToEffect(effect))
					return;
				defaultName = name;
				platformConstantBuffer->SetName(defaultName.c_str());
				SIMUL_ASSERT(platformConstantBuffer != nullptr);
				SIMUL_ASSERT(effect != nullptr);
				if (effect && platformConstantBuffer)
				{
					platformConstantBuffer->LinkToEffect(effect, name, T::bindingIndex);
					linkedEffects.insert(effect);
					effect->StoreConstantBufferLink(this);
				}
			}
			bool IsLinkedToEffect(crossplatform::Effect* effect)
			{
				if (!effect)
					return false;
				if (linkedEffects.find(effect) != linkedEffects.end())
				{
					if (effect->IsLinkedToConstantBuffer(this))
						return true;
				}
				return false;
			}
#else
			void RestoreDeviceObjects(RenderPlatform* p);
			//! Find the constant buffer in the given effect, and link to it.
			void LinkToEffect(Effect* effect, const char* name);
			bool IsLinkedToEffect(crossplatform::Effect* effect);
#endif
			//! Free the allocated buffer.
			void InvalidateDeviceObjects()
			{
				linkedEffects.clear();
				if (platformConstantBuffer)
					platformConstantBuffer->InvalidateDeviceObjects();
				delete platformConstantBuffer;
				platformConstantBuffer = NULL;
			}
			//! Unbind from the effect.
			void Unbind(DeviceContext& deviceContext)
			{
				if (platformConstantBuffer)
					platformConstantBuffer->Unbind(deviceContext);
			}
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
