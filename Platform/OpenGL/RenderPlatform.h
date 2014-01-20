#pragma once
#include "Export.h"
#include "Simul/Scene/RenderPlatform.h"
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT RenderPlatform:public scene::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			virtual void ApplyDefaultMaterial();
			virtual scene::MaterialCache *CreateMaterial();
			virtual scene::Mesh *CreateMesh();
			virtual scene::LightCache *CreateLight();
			virtual scene::Texture *CreateTexture(const char *lFileNameUtf8);
		};
	}
}