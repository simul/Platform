// Copyright (c) 2011-2012 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license or nondisclosure
// agreement with Simul Software Ltd and may not be copied or disclosed except
// in accordance with the terms of that agreement.

#pragma once
#include "Simul/Math/Vector3.h"
#include "Simul/Clouds/BasePrecipitationRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
typedef long HRESULT;
namespace simul
{
	namespace opengl
	{
		class SimulGLPrecipitationRenderer: public simul::clouds::BasePrecipitationRenderer
		{
		public:
			SimulGLPrecipitationRenderer();
			virtual ~SimulGLPrecipitationRenderer();
			//! Call this when the device has been created or reset.
			void RestoreDeviceObjects(crossplatform::RenderPlatform *);
			//! Call this when the D3D device has been shut down.
			void InvalidateDeviceObjects();
			void RecompileShaders();
			//! Call this to draw the clouds, including any illumination by lightning.
			void Render(simul::crossplatform::DeviceContext &deviceContext
						,crossplatform::Texture *depth_tex
						,float max_fade_distance_metres
						,simul::sky::float4 viewportTextureRegionXYWH);
			// Set a texture not created by this class to be used:
			bool SetExternalRainTexture(void* tex);
		protected:
			virtual void TextureRepeatChanged();
			bool		external_rain_texture;
			GLuint		program;
			GLuint		rain_texture;
		};
	}
}