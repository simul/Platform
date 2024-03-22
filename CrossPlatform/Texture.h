#pragma once
#include "Platform/Core/MemoryInterface.h"
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/PixelFormat.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include <vector>
#include <string>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif
#ifdef __GNUC__
#define PLATFORM_PACK(__Declaration__) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PLATFORM_PACK(__Declaration__) __pragma(pack(push, 1)) __Declaration__ __pragma(pack(pop))
#endif

namespace sce
{
	namespace Gnm
	{
		class Texture;
		class Sampler;
	}
}
namespace vk
{
	class Image;
	class ImageView;
	class Sampler;
}
namespace nvn
{
	class Texture;
}

struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11Resource;
struct ID3D11SamplerState;
typedef unsigned GLuint;
extern "C"
{
    struct ID3D12Resource;
}

struct D3D12_CPU_DESCRIPTOR_HANDLE;

namespace platform
{
	namespace crossplatform
	{
		class RenderPlatform;
		class Texture;
		struct DeviceContext;
		struct GraphicsDeviceContext;
		enum DepthComparison
		{
			DEPTH_NEVER=0,
			DEPTH_ALWAYS,
			DEPTH_LESS,
			DEPTH_EQUAL,
			DEPTH_LESS_EQUAL,
			DEPTH_GREATER,
			DEPTH_NOT_EQUAL,
			DEPTH_GREATER_EQUAL
		};
		enum class CompressionFormat : uint32_t
		{
			UNCOMPRESSED,
			BC1,
			BC3,
			BC4,
			BC5,
			ETC1,
			ETC2,
			PVRTC1_4_OPAQUE_ONLY,
			BC7_M6_OPAQUE_ONLY,
			BC6H
		};
		struct SamplerStateDesc
		{
			enum Wrapping
			{
				WRAP,CLAMP,MIRROR
			};
			//TODO: Add Mipmap mode for min filtering - AJR
			enum Filtering
			{
				POINT,LINEAR,ANISOTROPIC
			};
			Wrapping x,y,z;
			Filtering filtering;
			DepthComparison depthComparison;
			int slot;			// register slot
		};

		enum class VideoTextureType
		{
			NONE = 0,
			ENCODE,
			DECODE,
			PROCESS
		};

		/// A structure for creating or initializing textures.
		struct TextureCreate
		{
			void *external_texture=nullptr;
			int w=0;
			int l=0;
			int d=1;
			int arraysize=1;
			int mips = 1;
			bool cubemap=false;
			PixelFormat f=PixelFormat::UNKNOWN;
			bool make_rt=false;
			bool setDepthStencil=false;
			VideoTextureType vidTexType = VideoTextureType::NONE;
			bool computable = true;
			int numOfSamples=1;
			int aa_quality = 0;
			vec4 clear;
			float clearDepth=0.0f;
			uint32_t clearStencil = 0;
			bool shared = false;
			CompressionFormat compressionFormat= CompressionFormat::UNCOMPRESSED;
			// N arrays of bytes, where N=arraysize*mips and ordered by mips then layers.
			std::shared_ptr<std::vector<std::vector<uint8_t>>> initialData;
			bool cpuWritable = false;
			const char* name = nullptr;
			bool forceInit=false;			//!< Initialize fully, even if it looks the same.
		};
		//! A crossplatform viewport structure.
		struct Viewport
		{
			int x=0,y=0;
			int w=0,h=0;
			inline const Viewport &operator=(const tvector4<int> &i)
			{
				x=i.x;
				y=i.y;
				w=i.z;
				h=i.w;
				return *this;
			}
		};
		
		class SIMUL_CROSSPLATFORM_EXPORT SamplerState
		{
		public:
			SamplerState();
			virtual ~SamplerState();
			virtual void InvalidateDeviceObjects()=0;
			virtual ID3D11SamplerState *asD3D11SamplerState()
			{
				return NULL;
			}
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12SamplerState()
			{
				return NULL;
			}
			virtual const sce::Gnm::Sampler *AsGnmSampler() const {return 0;}
			virtual GLuint asGLuint()
			{
				return 0;
			}
			virtual vk::Sampler *AsVulkanSampler()
			{
				return nullptr;
			}
			int default_slot;
			crossplatform::RenderPlatform *renderPlatform;
			SamplerStateDesc samplerStateDesc;
		};
		// TODO: is this just a repro of Vulkan's Aspect flags?
		enum class TextureAspectFlags : uint8_t
		{
			NONE		= 0x00,
			COLOUR		= 0x01,
			DEPTH		= 0x02,
			STENCIL		= 0x04,
			METADATA	= 0x08,
			PLANE_0		= 0x10,
			PLANE_1		= 0x20,
			PLANE_2		= 0x40
		};
		inline TextureAspectFlags operator|(TextureAspectFlags a, TextureAspectFlags b)
		{
			return static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
		}
		inline TextureAspectFlags operator&(TextureAspectFlags a, TextureAspectFlags b)
		{
			return static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
		}
		inline TextureAspectFlags operator^(TextureAspectFlags a, TextureAspectFlags b)
		{
			return static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) ^ static_cast<uint8_t>(b));
		}
		inline TextureAspectFlags operator~(TextureAspectFlags a)
		{
			return static_cast<TextureAspectFlags>(~static_cast<uint8_t>(a));
		}
		inline TextureAspectFlags& operator|=(TextureAspectFlags& a, TextureAspectFlags b)
		{
			a = static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
			return a;
		}
		inline TextureAspectFlags& operator&=(TextureAspectFlags& a, TextureAspectFlags b)
		{
			a = static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
			return a;
		}
		inline TextureAspectFlags& operator^=(TextureAspectFlags& a, TextureAspectFlags b)
		{
			a = static_cast<TextureAspectFlags>(static_cast<uint8_t>(a) ^ static_cast<uint8_t>(b));
			return a;
		}
		
		//! Structure to describe range of subresources in a Texture/Images for mip levels and array layers.
		//! We do NOT set defaults for these structs, as this causes frequent unwanted execution of constructors, and we want these structs to be POD.
		PLATFORM_PACK(struct SIMUL_CROSSPLATFORM_EXPORT SubresourceRange
		{
			TextureAspectFlags aspectMask;		//! How pixel data should be used: RGBA, Depth, Stencil, YCbCr layers, etc.
			uint8_t baseMipLevel;				//! The first mip level in the view.
			uint8_t mipLevelCount;				//! The number of mip levels, starting from the baseMipLevel, in the view.
			uint8_t baseArrayLayer;				//! The first array layer in the view.
			uint8_t arrayLayerCount;			//! The number of array layers, starting from the baseArrayLayer, in the view.
		});
		static_assert(sizeof(crossplatform::SubresourceRange) == 5, "Size of SubresourceRange should be 5 bytes.");
		static constexpr SubresourceRange DefaultSubresourceRange = {TextureAspectFlags::COLOUR, 0, 0xFF, 0, 0xFF};

		//! Structure to describe range of subresources in a Texture/Images for a single mip level and array layers.
		PLATFORM_PACK(struct SIMUL_CROSSPLATFORM_EXPORT SubresourceLayers
		{
			TextureAspectFlags aspectMask ;		//! How pixel data should be used: RGBA, Depth, Stencil, YCbCr layers, etc.
			uint8_t mipLevel;					//! The single mip level in the view.
			uint8_t baseArrayLayer;				//! The first array layer in the view.
			uint8_t arrayLayerCount;			//! The number of array layers, starting from the baseArrayLayer, in the view.
		});
		static_assert(sizeof(crossplatform::SubresourceLayers) == 4, "Size of SubresourceLayers should be 4 bytes.");
		static constexpr SubresourceLayers DefaultSubresourceLayers = {TextureAspectFlags::COLOUR, 0, 0, 0xFF};

		PLATFORM_PACK(struct SIMUL_CROSSPLATFORM_EXPORT TextureViewElements
		{
			ShaderResourceType type;
			SubresourceRange subresourceRange;
			uint8_t pad;
		});
		static_assert(sizeof(crossplatform::TextureViewElements) == 8, "Size of TextureViewElements should be 8 bytes.");

		/// Base class for a view of a texture (i.e. for shaders to use). TextureView instances should not be created, except inside derived classes of crossplatform::Texture.
		struct SIMUL_CROSSPLATFORM_EXPORT TextureView
		{
			union
			{
				TextureViewElements elements;
				uint64_t hash;
			};
		};
		static_assert(sizeof(crossplatform::TextureView) <= sizeof(uint64_t), "Size of TextureView should be 8 bytes.");
		static constexpr TextureView DefaultTextureView = {ShaderResourceType::UNKNOWN, DefaultSubresourceRange};

		inline TextureView MakeTextureView(ShaderResourceType type, TextureAspectFlags aspect, uint8_t baseMip, uint8_t mipCount, uint8_t baseLayer, uint8_t layerCount)
		{
			TextureView view;
			view.elements.type = type;
			view.elements.subresourceRange = {aspect, baseMip, mipCount, baseLayer, layerCount};
			view.elements.pad = 0;
			return view;
		}
		inline TextureView MakeTextureView(ShaderResourceType type, SubresourceRange subresourceRange)
		{
			TextureView view;
			view.elements.type = type;
			view.elements.subresourceRange = subresourceRange;
			view.elements.pad = 0;
			return view;
		}
		//#define MAKE_TEXTURE_VIEW(type, aspect, baseMip, mipCount, baseLayer, layerCount) MakeTextureView(type, aspect, baseMip, mipCount, baseLayer, layerCount)
		//#define MAKE_TEXTURE_VIEW_2(type, subresourceRange) MakeTextureView(type, subresourceRange)

		typedef void ApiRenderTarget;
		typedef void ApiDepthRenderTarget;
		//! Stores information about the current render targets
		struct SIMUL_CROSSPLATFORM_EXPORT TargetsAndViewport
		{
			struct TextureTarget
			{
				Texture* texture = nullptr;
				SubresourceLayers subresource = DefaultSubresourceLayers;
			};
			TargetsAndViewport()
				:temp(false), num(0), m_dt(nullptr), depthFormat(UNKNOWN)
			{
				for (int i = 0; i < 8; i++)
				{
					m_rt[i] = nullptr;
					rtFormats[i] = UNKNOWN;
				}
			}
			bool                        temp;
			int                         num;
			//! API pointer to the target
			const ApiRenderTarget* m_rt[8];
			//! If using Simul textures, the targets, including layer and mip specification.
			TextureTarget				textureTargets[8];
			TextureTarget				depthTarget;
			//! The pixel format of the target
			PixelFormat                 rtFormats[8];
			//! If any, the API pointer to the depth surface
			const ApiDepthRenderTarget* m_dt;
			//! Depth format
			PixelFormat                 depthFormat;
			crossplatform::Viewport     viewport;
		};

		/// A Texture base class.
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			Texture(const char *name=NULL);
			virtual ~Texture();
			virtual void SetName(const char *n);
			const char* GetName() const;
			/// Set the fence on this texture: it cannot be used until the fence has been triggered by the rendering API.
			void SetFence(DeviceContext &,unsigned long long);
			/// Get the current fence on this texture; it should not be used until the API has passed this fence.
			unsigned long long GetFence(DeviceContext &) const;
			/// Clear the fence: this texture is ok to use now.
			void ClearFence(DeviceContext &deviceContext);
			/// For API's that care about Resource State, aka Layout, tell the Simul API what state it was in to begin with.
			virtual void StoreExternalState(crossplatform::ResourceState){}
			/// For API's that care about Resource State, aka Layout, restore the state internally.
			virtual void RestoreExternalTextureState(DeviceContext &) {}
			virtual void SwitchToContext(DeviceContext &) {}
			/// Is the texture "unfenceable": if so, it need never be checked for fences, either because it is constant,
			/// or because we don't care if it's not been updated.
			bool IsUnfenceable() const
			{
				return unfenceable;
			}
			/// Set whether to never check for fences on this texture.
			void SetUnfenceable(bool v)
			{
				unfenceable=v;
			}
			/// Get whether texture is a depth stencil
			bool IsDepthStencil() const
			{
				return depthStencil;
			}
			bool ShouldGenerateMips() const
			{
				return shouldGenerateMips;
			}
			virtual bool LoadFromFile(RenderPlatform *r,const char *pFilePathUtf8,bool gen_mips=false)=0;
			virtual bool LoadTextureArray(RenderPlatform *r,const std::vector<std::string> &texture_files,bool gen_mips=false)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects();
            virtual nvn::Texture* AsNXTexture() { return 0; };
			//! Returns the GnmTexture specified by layer,mip. Default values of -1 mean "all".
			virtual sce::Gnm::Texture *AsGnmTexture(const crossplatform::TextureView& textureView){return 0;}
			virtual ID3D11Texture2D *AsD3D11Texture2D(){return 0;}
			virtual ID3D11Resource *AsD3D11Resource(){return 0;}
			virtual ID3D12Resource *AsD3D12Resource() { return 0; }
			//! Returns the SRV specified by layer,mip. The type t ensures that the assigned resource is compatible (UNKNWON matches anything).
			//! Layer -1 means all layers at the given mip, while mip -1 defaults to the whole texture/layer.
			virtual ID3D11ShaderResourceView* AsD3D11ShaderResourceView(const crossplatform::TextureView& textureView) { return 0; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12ShaderResourceView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView, bool setState = true, bool pixelShader = true) { return 0; }
			//! Returns the UAV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11UnorderedAccessView* AsD3D11UnorderedAccessView(const crossplatform::TextureView& textureView) { return 0; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12UnorderedAccessView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView) { return 0; }

			virtual ID3D11DepthStencilView* AsD3D11DepthStencilView(const crossplatform::TextureView& textureView) { return 0; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12DepthStencilView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView) { return 0; }
			//! Returns the RTV specified by layer,mip. Layer -1 means all layers at the given mip, while mip -1 defaults to mip zero.
			virtual ID3D11RenderTargetView* AsD3D11RenderTargetView(const crossplatform::TextureView& textureView) { return 0; }
			virtual D3D12_CPU_DESCRIPTOR_HANDLE* AsD3D12RenderTargetView(crossplatform::DeviceContext& deviceContext, const crossplatform::TextureView& textureView) { return 0; }
			virtual bool HasRenderTargets() const { return renderTarget; }
			virtual bool IsComputable() const { return computable; }
			/// Asynchronously move this texture to fast RAM.
			/// Some hardware has "fast RAM" that we can prefetch textures into.
			virtual void MoveToFastRAM() {}
			/// Asynchronously move this texture to slow RAM.
			virtual void MoveToSlowRAM() {}
			virtual void DiscardFromFastRAM() {}
			virtual GLuint AsGLuint(){return 0;}
			virtual vk::Image* AsVulkanImage() { return nullptr; }
			virtual vk::ImageView* AsVulkanImageView(crossplatform::TextureView textureView) { return nullptr; }
			//! Get the crossplatform pixel format.
			PixelFormat GetFormat() const
			{
				return pixelFormat;
			}
			//! Initialize this object as a wrapper around a native, platform-specific texture. The interpretations of t and srv are platform-dependent. Returns true if successful.
			virtual bool InitFromExternalTexture2D(crossplatform::RenderPlatform *renderPlatform,void *t,int w=0,int l=0,PixelFormat f=PixelFormat::UNKNOWN,bool make_rt=false, bool setDepthStencil=false, int numOfSamples = 1)=0;
			virtual bool InitFromExternalTexture(crossplatform::RenderPlatform *renderPlatform, const TextureCreate *textureCreate);
			virtual bool InitFromExternalTexture3D(crossplatform::RenderPlatform*, void*, bool = false) { return false; }
			virtual bool EnsureTexture(RenderPlatform *, TextureCreate *);
			//! \deprecated
			bool ensureTexture2DSizeAndFormat(RenderPlatform* renderPlatform, int w, int l, int m
				, PixelFormat f, bool computable = false, bool rendertarget = false, bool depthstencil = false, int num_samples = 1, int aa_quality = 0, bool wrap = false,
				vec4 clear = vec4(0.0f, 0.0f, 0.0f, 0.0f), float clearDepth = 0.0f, uint32_t clearStencil = 0, bool shared = false
				, crossplatform::CompressionFormat compressionFormat = crossplatform::CompressionFormat::UNCOMPRESSED
				, const uint8_t** initData = nullptr);
			//! Initialize as a standard 2D texture. Not all platforms need \a wrap to be specified. Returns true if modified, false otherwise.
			virtual bool ensureTexture2DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int m
				,PixelFormat f, std::shared_ptr<std::vector<std::vector<uint8_t>>> data ,bool computable=false,bool rendertarget=false,bool depthstencil=false,int num_samples=1,int aa_quality=0,bool wrap=false, vec4 clear = vec4(0.0f, 0.0f, 0.0f, 0.0f), float clearDepth = 0.0f, uint32_t clearStencil = 0, bool shared = false
				, crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED)=0;
			// Create texture for use as a reference frame in video encoding or decoding.
			virtual bool ensureVideoTexture(RenderPlatform* renderPlatform, int w, int l, PixelFormat f, VideoTextureType texType) { return false; };
			//! Initialize as an array texture if necessary. Returns true if the texture was initialized, or false if it was already in the required format.
			virtual bool ensureTextureArraySizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int num,int mips,PixelFormat f
				, std::shared_ptr<std::vector<std::vector<uint8_t>>> initialData
				,bool computable=false,bool rendertarget=false,bool depthstencil=false,bool cubemap=false
				, crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED)=0;
			//! Initialize as a volume texture.
			virtual bool ensureTexture3DSizeAndFormat(RenderPlatform *renderPlatform,int w,int l,int d,PixelFormat frmt,bool computable=false,int mips=1,bool rendertargets=false)=0;
			//! Clear the colour aspect
			virtual void ClearColour(GraphicsDeviceContext &deviceContext, vec4 colourClear = vec4(0.0f, 0.0f, 0.0f, 0.0f)) = 0;
			//! Clear the depth stencil aspect
			virtual void ClearDepthStencil(GraphicsDeviceContext &deviceContext, float depthClear = 0, int stencilClear = 0) = 0;
			//! Generate the mipmaps automatically.
			virtual void GenerateMips(GraphicsDeviceContext &deviceContext)=0;
			//! Set the texture data from CPU memory.
			virtual void setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)=0;
			//! Set the texture data from CPU memory, but defer the transfer until we next have a valid DeviceContext.
			virtual void setTexels(const void *src,int texel_index,int num_texels){}
			//! Activate as a rendertarget - must call deactivateRenderTarget afterwards.
			virtual void activateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext, crossplatform::TextureView textureView = DefaultTextureView);
			//! Deactivate as a rendertarget.
			virtual void deactivateRenderTarget(crossplatform::GraphicsDeviceContext &deviceContext);
			virtual int GetLength() const
			{
				return length;
			}
			virtual int GetWidth() const
			{
				return width;
			}
			virtual int GetDepth() const
			{
				return depth;
			}
			virtual int GetDimension() const
			{
				return dim;
			}
			int GetMipCount() const
			{
				return mips;
			}
			int GetArraySize() const
			{
				return arraySize;
			}
			const std::string GetName()
			{
				return name;
			}
			virtual void FinishLoading(DeviceContext & ) {}
			//! If the texture is multisampled, this returns the samples per texel. Zero means it is not an MS texture,
			//! while 1 means it is MS, even though the sample count is unity.
			virtual int GetSampleCount() const=0;
			virtual bool IsCubemap() const;
			//! The number of "faces": either equal to the array size, or in the case of a cubemap, six times that number.
			int NumFaces() const
			{
				return cubemap?arraySize*6:arraySize;
			}
			virtual bool IsYUV() const
			{
				return false;
			}
			virtual void copyToMemory(DeviceContext &deviceContext,void *target,int start_texel,int num_texels)=0;
			virtual ShaderResourceType GetShaderResourceTypeForRTVAndDSV();
			int width,length,depth,arraySize,dim,mips;
			PixelFormat pixelFormat;
			crossplatform::CompressionFormat compressionFormat=crossplatform::CompressionFormat::UNCOMPRESSED;
			RenderPlatform *renderPlatform;
		protected:
			bool cubemap;
			bool computable;
			bool renderTarget;
			bool external_texture;
			bool depthStencil;
			bool shouldGenerateMips = false;
			bool textureUploadComplete=true;
			std::shared_ptr<std::vector<std::vector<uint8_t>>> upload_data;
			std::string name;
			platform::crossplatform::TargetsAndViewport targetsAndViewport;
			// For API's that don't track resources:
			bool unfenceable;
			// a wrapper around stbi_load_from_memory.
			bool TranslateLoadedTextureData(void *&target,const void *src,size_t size,int &x,int &y,int &num_channels,int req_num_channels);
			void FreeTranslatedTextureData(void *data);
			uint32_t CalculateSubresourceIndex(uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize);
			tvector3<uint32_t> CalculateSubresourceSlices(uint32_t Index, uint32_t MipSlice, uint32_t ArraySlice); // Returned as { MipSlice, ArraySlice, PlaneSlice }
			bool ValidateTextureView(const TextureView &textureView);
			crossplatform::ShaderResourceType GetDefaultShaderResourceType() const;
			crossplatform::TextureView defaultTextureView;
			void SetDefaultTextureView();
		private:
			bool stbi_loaded = false;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
