#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace opengl
	{
		class Material;
		class SIMUL_OPENGL_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			
			void PushTexturePath(const char *pathUtf8);
			void PopTexturePath();
			void StartRender();
			void EndRender();
			void SetReverseDepth(bool);
			void IntializeLightingEnvironment(const float pAmbientLight[3]);
			void DrawMarker		(void *context,const double *matrix);
			void DrawLine		(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			void DrawCrossHair	(void *context,const double *pGlobalPosition);
			void DrawCamera		(void *context,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop	(void *context,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void DrawTexture	(void *context,int x1,int y1,int dx,int dy,void *tex,float mult=1.f);
			void DrawQuad		(void *context,int x1,int y1,int dx,int dy,void *effect,void *technique);
			void Print			(void *context,int x,int y	,const char *text);
			void DrawLines		(crossplatform::DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false);
			void Draw2dLines	(crossplatform::DeviceContext &deviceContext,Vertext *lines,int count,bool strip);
			void PrintAt3dPos	(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			void DrawCircle		(crossplatform::DeviceContext &context,const float *dir,float rads,const float *colr,bool fill=false);
			void ApplyDefaultMaterial();
			void SetModelMatrix(void *,const crossplatform::ViewStruct &,const double *mat);
			scene::Material *CreateMaterial();
			crossplatform::Mesh		*CreateMesh();
			scene::Light	*CreateLight();
			crossplatform::Texture	*CreateTexture(const char *lFileNameUtf8);
			void			*GetDevice();
			
			GLuint solid_program;
			simul::opengl::ConstantBuffer<SolidConstants> solidConstants;
			std::set<opengl::Material*> materials;
			bool reverseDepth;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
