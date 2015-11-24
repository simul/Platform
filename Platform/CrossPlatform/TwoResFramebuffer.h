#ifndef TWORESFRAMEBUFFER_H
#define TWORESFRAMEBUFFER_H
#include "Simul/Platform/CrossPlatform/BaseFramebuffer.h"
#include "Simul/Geometry/Orientation.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		/// A small structure for per-frame amortization of buffers.
		struct AmortizationStruct
		{
		private:
			int amortization;
		public:
			int framenumber;
			int4 validRegion;
			//int2 pixelOffset;
			int2 lowResOffset;
			int2 *pattern;
			AmortizationStruct()
				:framenumber(0)
				,amortization(1)
				,validRegion(0,0,0,0)
				//,pixelOffset(0,0)
				,lowResOffset(0,0)
				,pattern(NULL)
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
			int2 offset() const
			{
				if(!pattern||amortization<=1)
					return int2(0,0);
				int a			=amortization;
				int sub_frame	=framenumber%(a*a);
				int offsx		=sub_frame/a;
				int offsy		=sub_frame-offsx*a;
				return pattern[sub_frame];//(int2(offsx,offsy);//
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
			void updateRegion(int2 deltaOffset,vec2 newPixelOffset)
			{
				int2 new_pos=int2((int)newPixelOffset.x,(int)newPixelOffset.y);
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
		class Texture;
		struct DeviceContext;
		class RenderPlatform;
		struct Viewport;
		struct ViewStruct;
		//! A framebuffer class for mixed-resolution rendering.
		class SIMUL_CROSSPLATFORM_EXPORT TwoResFramebuffer
		{
		public:
			TwoResFramebuffer();
			crossplatform::Texture *GetLowResFramebuffer(int index)
			{
				return lowResFramebuffers[index];
			}
			crossplatform::Texture *GetLossTexture();
			crossplatform::Texture *GetVolumeTexture(int num);
			virtual void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			virtual void InvalidateDeviceObjects();
			virtual void SetDimensions(int w,int h);
			virtual void GetDimensions(int &w,int &h);
			// Assign the current frame's projection matrix for this buffer. Just for debugging.
			void SetProjection(const float *p);
			int GetDownscale() const
			{
				return Downscale;
			}
			void SetDownscale(int d);
			/// Activate BOTH low-resolution framebuffers - far in target 0, near in target 1. Must be followed by DeactivatelLowRes after rendering.
			virtual void ActivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate both low-res framebuffers.
			virtual void DeactivateLowRes(crossplatform::DeviceContext &);
			/// Deactivate the depth buffer
			virtual void DeactivateDepth(crossplatform::DeviceContext &);
			virtual void ActivateVolume(crossplatform::DeviceContext &,int num);
			virtual void DeactivateVolume(crossplatform::DeviceContext &);
			/// This must be called to ensure that the amortization struct is up to date.
			void CompleteFrame();
			/// Debugging onscreen info:
			///
			/// \param [in,out]	deviceContext	Context for the device.
			/// \param	depthTexture			The main depth texture.
			/// \param	viewport			 	The viewport in use for the depth texture.
			/// \param	x0					 	The left edge of area to use for the debug display.
			/// \param	y0					 	The top of this debug display.
			/// \param	dx					 	The width of the display.
			/// \param	dy					 	The height of the display.
			void RenderDepthBuffers(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *depthTexture,const crossplatform::Viewport *viewport,int x0,int y0,int dx,int dy);

			/// Update the pixel offset for the specified view.
			void UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct);
			/// Offset in pixels from top-left of the low-res view to top-left of the full-res.
			vec2								pixelOffset;
			/// Swap stochastic texture buffers.
			void Swap();
			/// Returns	the low-res depth texture.
			crossplatform::Texture				*GetLowResDepthTexture(int idx=-1);
			crossplatform::PixelFormat GetDepthFormat() const;
			void SetDepthFormat(crossplatform::PixelFormat p);
			int									final_octave;
			vec2								GetPixelOffset() const
			{
				return pixelOffset;
			}
			void								SetAmortization(int a)
			{
				amortizationStruct.setAmortization(a);
 			}
			int									GetAmortization() const
			{
				return amortizationStruct.getAmortization();
			}
			const AmortizationStruct			&GetAmortizationStruct() const
			{
				return amortizationStruct;
			}
		protected:
			mat4								proj;
			int									Width,Height,Downscale;
			AmortizationStruct					amortizationStruct;
			int									volume_num;
			crossplatform::PixelFormat			depthFormat;
			simul::geometry::SimulOrientation	view_o;
			crossplatform::RenderPlatform		*renderPlatform;
			crossplatform::Texture				*nearFarTextures[5];
			crossplatform::Texture				*lossTexture;
			crossplatform::Texture				*volumeTextures[2];
			crossplatform::Texture				*lowResFramebuffers[4];
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif