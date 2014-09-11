#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/solid_constants.sl"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
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
		/// A RenderPlatform for OpenGL.
		class SIMUL_OPENGL_EXPORT RenderPlatform:public crossplatform::RenderPlatform
		{
		public:
			RenderPlatform();
			virtual ~RenderPlatform();
			void RestoreDeviceObjects(void*);
			void InvalidateDeviceObjects();
			void RecompileShaders();
			
			virtual ID3D11Device *AsD3D11Device()
			{
				return NULL;
			}
			void PushTexturePath(const char *pathUtf8);
			void PopTexturePath();
			void StartRender();
			void EndRender();
			void SetReverseDepth(bool);
			void IntializeLightingEnvironment(const float pAmbientLight[3]);
			void DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d);
			void ApplyShaderPass	(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *,crossplatform::EffectTechnique *,int);
			void Draw				(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert);
			void DrawMarker			(crossplatform::DeviceContext &deviceContext,const double *matrix);
			void DrawLine			(crossplatform::DeviceContext &deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width);
			void DrawCrossHair		(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition);
			void DrawCamera			(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll);
			void DrawLineLoop		(crossplatform::DeviceContext &deviceContext,const double *mat,int num,const double *vertexArray,const float colr[4]);
			void DrawTexture		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f,bool blend=false);
			void DrawDepth			(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex);
			void DrawQuad			(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique);
			void DrawQuad			(crossplatform::DeviceContext &deviceContext);
			void Print				(crossplatform::DeviceContext &deviceContext,int x	,int y	,const char *text,const float* colr=NULL,const float* bkg=NULL);
			void DrawLines			(crossplatform::DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false,bool test_depth=false);
			void Draw2dLines		(crossplatform::DeviceContext &deviceContext,Vertext *lines,int count,bool strip);
			void PrintAt3dPos		(crossplatform::DeviceContext &deviceContext,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0);
			void DrawCircle			(crossplatform::DeviceContext &context,const float *dir,float rads,const float *colr,bool fill=false);
			void ApplyDefaultMaterial();
			void SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *mat,const crossplatform::PhysicalLightRenderData &physicalLightRenderData);
			crossplatform::Material					*CreateMaterial();
			crossplatform::Mesh						*CreateMesh();
			crossplatform::Light					*CreateLight();
			crossplatform::Texture					*CreateTexture(const char *lFileNameUtf8);
			crossplatform::Effect					*CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines);
			crossplatform::PlatformConstantBuffer	*CreatePlatformConstantBuffer();
			crossplatform::PlatformStructuredBuffer	*CreatePlatformStructuredBuffer();
			crossplatform::Buffer					*CreateBuffer();
			crossplatform::Layout					*CreateLayout(int num_elements,crossplatform::LayoutDesc *,crossplatform::Buffer *);
			void									*GetDevice();
			void									SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer **buffers);
			void									ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth);
	
			crossplatform::Viewport					GetViewport(crossplatform::DeviceContext &deviceContext,int index);
			void									SetViewports(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Viewport *vps);

			void									SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer);
			void									EnsureEffectIsBuilt				(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options);

			void StoreRenderState(crossplatform::DeviceContext &deviceContext);
			void RestoreRenderState(crossplatform::DeviceContext &deviceContext);

			GLuint solid_program;
			simul::opengl::ConstantBuffer<SolidConstants> solidConstants;
			std::set<opengl::Material*> materials;
			bool reverseDepth;
			// OpenGL-specific stuff:
			static GLuint ToGLFormat(crossplatform::PixelFormat p);
			static int FormatCount(crossplatform::PixelFormat p);
		protected:
			void DrawTexture		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,GLuint tex,float mult=1.f,bool blend=false);
			crossplatform::Effect *effect;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
