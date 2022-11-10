#ifndef Text3DRenderer_H
#define Text3DRenderer_H

#include <fstream>
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/Shaders/SL/CppSl.sl"
#include "Platform/Shaders/SL/text_constants.sl"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace platform
{
	namespace crossplatform
	{
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Text3DRenderer
		{
		private:

		public:
			Text3DRenderer();
			~Text3DRenderer();
			

			void PushFontPath(const char *p);
			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			int Render(GraphicsDeviceContext &deviceContext,float x,float y,float screen_width,float screen_height,const char *txt,const float *clr=NULL,const float *bck=NULL,bool mirrorY=false);
			int Render(MultiviewGraphicsDeviceContext &deviceContext,float* xs,float* ys,float screen_width,float screen_height,const char *txt,const float *clr=NULL,const float *bck=NULL,bool mirrorY=false);
			int GetDefaultTextHeight() const;
			struct FontIndex
			{
				float x;
				float w;
				int pixel_width;
			};
		private:
			std::vector<std::string> fontPaths;
			void Recompile();
			crossplatform::Effect						*effect=nullptr;
			crossplatform::EffectTechnique				*backgTech=nullptr;
			crossplatform::EffectTechnique				*textTech=nullptr;
			crossplatform::ShaderResource				textureResource;
			crossplatform::ShaderResource				_fontChars;
			crossplatform::ConstantBuffer<TextConstants>	constantBuffer;
			// We k
			std::map<const void*,crossplatform::StructuredBuffer<FontChar>> fontChars;
			crossplatform::Texture*			font_texture=nullptr;
			crossplatform::RenderPlatform *renderPlatform=nullptr;
			bool recompile=true;
			int fontWidth = 0;
			FontIndex * fontIndices = nullptr;
			int defaultTextHeight=20;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif

#endif