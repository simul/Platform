#pragma once
#include "Export.h"
#include "Simul/Scene/RenderPlatform.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/solid_constants.sl"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
namespace simul
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT RenderPlatform:public scene::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			virtual void RestoreDeviceObjects(void*);
			virtual void InvalidateDeviceObjects();
			virtual void RecompileShaders();

			virtual void StartRender();
			virtual void EndRender();
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3]);
			virtual void DrawMarker(const double *matrix);
			virtual void DrawLine(const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			virtual void DrawCrossHair(const double *pGlobalPosition);
			virtual void DrawCamera(const double *pGlobalPosition, double pRoll);
			virtual void DrawLineLoop(const double *mat,int num,const double *vertexArray,const float colr[4]);
			virtual void ApplyDefaultMaterial();
			virtual void SetModelMatrix(const double *mat);
			virtual scene::MaterialCache *CreateMaterial();
			virtual scene::Mesh *CreateMesh();
			virtual scene::LightCache *CreateLight();
			virtual scene::Texture *CreateTexture(const char *lFileNameUtf8);
			
			GLuint solid_program;
			simul::opengl::ConstantBuffer<SolidConstants> solidConstants;
		};
	}
}