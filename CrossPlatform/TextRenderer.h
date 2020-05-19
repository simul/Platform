#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

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

namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT TextRenderer
		{
		private:

		public:
			TextRenderer();
			~TextRenderer();

			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			void Render(crossplatform::DeviceContext &deviceContext,float x,float y,float screen_width,float screen_height,const char *txt,const float *clr=NULL,const float *bck=NULL,bool mirrorY=false);
			int GetDefaultTextHeight() const;
			struct FontIndex
			{
				float x;
				float w;
				int pixel_width;
			};
		private:
			void Recompile();
			crossplatform::Effect						*effect;
			crossplatform::EffectTechnique				*backgTech;
			crossplatform::EffectTechnique				*textTech;
			crossplatform::ShaderResource				textureResource;
			crossplatform::ShaderResource				_fontChars;
			crossplatform::ConstantBuffer<TextConstants>	constantBuffer;
			// We k
			std::map<const void*,crossplatform::StructuredBuffer<FontChar>> fontChars;
			crossplatform::Texture*			font_texture;
			crossplatform::RenderPlatform *renderPlatform;
			bool recompile;
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