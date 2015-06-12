#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include <fstream>
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Platform/CrossPlatform/SL/text_constants.sl"
namespace simul
{
	namespace crossplatform
	{
		struct DeviceContext;
		class TextRenderer
		{
		private:

		public:
			TextRenderer();
			~TextRenderer();

			void RestoreDeviceObjects(crossplatform::RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			void Render(crossplatform::DeviceContext &deviceContext, float x,float y,float screen_width,float screen_height,const char *txt,const float *clr=NULL,const float *bck=NULL,bool mirrorY=false);

		private:

			void Recompile();
			crossplatform::Effect						*effect;
	
			crossplatform::ConstantBuffer<TextConstants>	constantBuffer;
			crossplatform::Texture*			font_texture;
			crossplatform::RenderPlatform *renderPlatform;
			bool recompile;
		};
	}
}
#endif