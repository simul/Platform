#ifndef BASE_FRAMEBUFFER_H
#define BASE_FRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/spherical_harmonics_constants.sl"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		/// A small structure for per-frame amortization of buffers.
		struct SIMUL_CROSSPLATFORM_EXPORT AmortizationStruct
		{
		private:
			int amortization;
			int4 validRegion;
		public:
			int framenumber;
			int framesPerIncrement;
			uint3 *pattern;
			int numOffsets;
			AmortizationStruct()
				:amortization(1)
				,framenumber(0)
				,validRegion(0,0,0,0)
				,framesPerIncrement(1)
				,pattern(NULL)
				,numOffsets(1)
			{
			}
			~AmortizationStruct()
			{
				delete [] pattern;
			}
			void setAmortization(int a);
			int getAmortization() const
			{
				return amortization;
			}
			/// Reset frame data, but not properties.
			void reset()
			{
				framenumber=0;
			}
			uint3 scale() const
			{
				uint3 sc(1,1,1);
				sc.x=1+amortization/2;
				sc.y=sc.x-(amortization+1)%2;
				return sc;
			}
			uint3 offset() const
			{
				if(!pattern||amortization<=1)
					return uint3(0,0,0);
				int sub_frame	=(framenumber/framesPerIncrement)%(numOffsets);
				return pattern[sub_frame];
			}
			int4 xy(int2 texsize) const
			{
				int4 xy;
				int X=validRegion.x;
				int X0=0;
				if(X<=0)
				{
					X=texsize.x-validRegion.z;
					X0=validRegion.z;
				}
				int Y=validRegion.y;
				int Y0=0;
				if(Y<=0)
				{
					Y=texsize.y-validRegion.w;
					Y0=validRegion.w;
				}
				xy.x=X;
				xy.y=Y;
				xy.z=X0;
				xy.w=Y0;
				return xy;
			}
			int2 sideOffset(int2 texsize) const
			{
				int4 XYX0Y0=xy(texsize);
				int2 offset;//	=(const int*)pixelOffset;
				offset.x	=XYX0Y0.z;//+texsize.x;
				offset.y	=0;
				return offset;
			}
			int2 verticalEdgeOffset(int2 texsize) const
			{
				int4 XYX0Y0=xy(texsize);
				int2 offset(0,XYX0Y0.w);
				return offset;
			}
			void updateRegion(int2 deltaOffset)
			{
				//int2 new_pos=int2((int)newPixelOffset.x,(int)newPixelOffset.y);
				validRegion.x-=deltaOffset.x;
				validRegion.y-=deltaOffset.y;
				validRegion.z-=abs(deltaOffset.x);
				validRegion.w-=abs(deltaOffset.y);
				if(validRegion.x<0)
				{
					//validRegion.z+=validRegion.x;
					validRegion.x=0;
				}
				else if(validRegion.x>0)
				{
					//validRegion.z-=validRegion.x;
				}
				if(validRegion.y<0)
				{
				//	validRegion.w+=validRegion.y;
					validRegion.y=0;
				}
				else if(validRegion.y>0)
				{
				//	validRegion.w-=validRegion.y;
				}
				if(validRegion.z<0)
					validRegion.z=0;
				if(validRegion.w<0)
					validRegion.w=0;
				//pixelOffset=new_pos;
			}
			void validate(int4 region)
			{
				validRegion=region;
			}
		};
		//! A base class for API-dependent framebuffer classes.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS BaseFramebuffer
		{
		public:
			BaseFramebuffer(const char *n);
			virtual ~BaseFramebuffer();
			//! Call this when the API-dependent device has been created.
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			//! Call this when the API-dependent device has been lost or is shutting down.
			virtual void InvalidateDeviceObjects();
			/// Return true if the API-dependent objects have been updated to match the properties.
			virtual bool IsValid() const;
			virtual void SetExternalTextures(crossplatform::Texture *colour,crossplatform::Texture *depth);
			//! Call this if needed (not usually) to ensure that the buffers are created.
			virtual bool CreateBuffers();
			//! Activate the framebuffer and set the viewport- must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateViewport(crossplatform::DeviceContext &,float viewportX, float viewportY, float viewportW, float viewportH)=0;
			//! Activate the framebuffer - must be followed after rendering by a call to \ref Deactivate().
			virtual void Activate(crossplatform::DeviceContext &)=0;
			//! Activate the colour part of this framebuffer, without depth - must be followed after rendering by a call to \ref Deactivate().
			virtual void ActivateColour(crossplatform::DeviceContext &,const float viewportXYWH[4])=0;
			virtual void ActivateDepth(crossplatform::DeviceContext &)=0;
			//! Return true if the framebuffer's depth buffer has been activated and not yet deactivated.
			virtual bool IsDepthActive() const;
			//! Return true if the framebuffer's colour buffer has been activated and not yet deactivated.
			virtual bool IsColourActive() const;
			//! Deactivate the framebuffer - must be preceded a call to \ref Activate().
			virtual void Deactivate(DeviceContext &)=0;
			//! Deactivate only the depth buffer, so it can be used as a texture for rendering to the colour buffer.
			virtual void DeactivateDepth(DeviceContext &){}
			//! Set the API-dependent colour buffer format for this framebuffer. Across all API's, setting 0 means no rendering to colour.
			virtual void SetFormat(PixelFormat);
			//! Set the API-dependent colour depth format for this framebuffer. Across all API's, setting 0 means no rendering to depth.
			virtual void SetDepthFormat(PixelFormat);
			virtual void SetGenerateMips(bool);
			//! Clear the colour and depth buffers if present.
			virtual void Clear(crossplatform::DeviceContext &context,float R,float G,float B,float A,float depth,int mask=0)=0;
			//! Set the size of the framebuffer in pixel height and width.
			virtual void ClearColour(crossplatform::DeviceContext &context,float,float,float,float)=0;
			//! Set the size of the framebuffer.
			virtual void SetWidthAndHeight(int w,int h,int num_mips=1);
			//! Set this to be a cubemap framebuffer, so that its texture object will be a cubemap. Equivalent to SetWidthAndHeight.
			virtual void SetAsCubemap(int face_size,int num_mips=1,crossplatform::PixelFormat f=RGBA_32_FLOAT);
			virtual void SetCubeFace(int f);
			//! Some hardware has fast RAM that's good for framebuffers.
			virtual void SetUseFastRAM(bool /*colour*/,bool /*depth*/){};
			virtual void SetAntialiasing(int s)=0;
			//! Calculate the spherical harmonics of this cubemap and store the result internally.
			//! Changing the number of bands will resize the internal storeage.
			virtual void CalcSphericalHarmonics(crossplatform::DeviceContext &deviceContext);
			virtual void RecompileShaders();
			//! Get the texture for the colour buffer target.
			inline Texture *GetTexture()
			{
				return buffer_texture;
			}
			//! Get the texture for the depth buffer target.
			inline Texture *GetDepthTexture()
			{
				return buffer_depth_texture;
			}
			//! Get the dimension of the surface
			inline int GetWidth()
			{
				return Width;
			}
			//! Get the dimension of the surface
			inline int GetHeight()
			{
				return Height;
			}
		//protected:
			//! The size of the buffer
			int Width,Height;
			int mips;
			int numAntialiasingSamples;
			bool depth_active, colour_active;
			crossplatform::RenderPlatform *renderPlatform;
			crossplatform::StructuredBuffer<vec4> &GetSphericalHarmonics()
			{
				return sphericalHarmonics;
			}
			void SetBands(int b)
			{
				if(b>MAX_SH_BANDS)
					b=MAX_SH_BANDS;
				if(bands!=b)
				{
					bands=b;
					sphericalHarmonics.InvalidateDeviceObjects();
				}
			}
		protected:
			//! The depth buffer.
			crossplatform::Texture				*buffer_texture;
			crossplatform::Texture				*buffer_depth_texture;
			bool GenerateMips;
			bool is_cubemap;
			int	current_face;
			crossplatform::PixelFormat target_format;
			crossplatform::PixelFormat depth_format;
			int bands;
			int activate_count;

			crossplatform::ConstantBuffer<SphericalHarmonicsConstants>	sphericalHarmonicsConstants;
			crossplatform::StructuredBuffer<SphericalHarmonicsSample>	sphericalSamples;
			crossplatform::StructuredBuffer<vec4>						sphericalHarmonics;
			crossplatform::Effect										*sphericalHarmonicsEffect;
			bool external_texture;
			bool external_depth_texture;
			std::string name;
			int shSeed;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif