#ifdef _MSC_VER
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glut.h>
#endif
#include "OpenGLRenderer.h"
// For font definition define:
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Platform/OpenGL/SimulGLSkyRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGL2DCloudRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLAtmosphericsRenderer.h"
#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/Profiler.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#define GLUT_BITMAP_HELVETICA_12	((void*)7)

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env)
	:ScreenWidth(0)
	,ScreenHeight(0)
	,cam(NULL)
	,ShowFlares(true)
	,ShowFades(false)
	,ShowTerrain(true)
	,ShowCloudCrossSections(false)
	,CelestialDisplay(false)
	,UseHdrPostprocessor(true)
	,ShowOSD(false)
	,ShowWater(true)
	,ReverseDepth(false)
	,MixCloudsAndTerrain(false)
{
	simulHDRRenderer=new SimulGLHDRRenderer(ScreenWidth,ScreenHeight);
	simulWeatherRenderer=new SimulGLWeatherRenderer(env,true,false,ScreenWidth,ScreenHeight);
	simulOpticsRenderer=new SimulOpticsRendererGL();
	simulTerrainRenderer=new SimulGLTerrainRenderer();
	simulTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	simul::opengl::Profiler::GetGlobalProfiler().Initialize(NULL);
}

OpenGLRenderer::~OpenGLRenderer()
{
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
	simul::opengl::Profiler::GetGlobalProfiler().Uninitialize();
}

void OpenGLRenderer::paintGL()
{
	void *context=NULL;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetReverseDepth(ReverseDepth);
	if(simulTerrainRenderer)
		simulTerrainRenderer->SetReverseDepth(ReverseDepth);
	glClearColor(0,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glPushAttrib(GL_ENABLE_BIT);
	// If called from some other OpenGL program, we should already have a modelview and projection matrix.
	// Here we will generate the modelview matrix from the camera class:
    glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(cam->MakeViewMatrix(false));
	glMatrixMode(GL_PROJECTION);
	static float nearPlane=0.01f;
	static float farPlane=250000.f;
	glLoadMatrixf(cam->MakeProjectionMatrix(nearPlane,farPlane,(float)ScreenWidth/(float)ScreenHeight,false));
	glViewport(0,0,ScreenWidth,ScreenHeight);
	if(simulWeatherRenderer.get())
	{
		simulWeatherRenderer->Update();
		GLuint fogMode[]={GL_EXP,GL_EXP2,GL_LINEAR};	// Storage For Three Types Of Fog
		GLuint fogfilter=0;								// Which Fog To Use
		simul::sky::float4 fogColor=simulWeatherRenderer->GetHorizonColour(0.001f*cam->GetPosition()[2]);
		glFogi(GL_FOG_MODE,fogMode[fogfilter]);			// Fog Mode
		glFogfv(GL_FOG_COLOR,fogColor);					// Set Fog Color
		glFogf(GL_FOG_DENSITY,0.35f);					// How Dense Will The Fog Be
		glFogf(GL_FOG_START,1.0f);						// Fog Start Depth
		glFogf(GL_FOG_END,5.0f);						// Fog End Depth
	/*	glEnable(GL_FOG);*/
ERROR_CHECK
		if(simulHDRRenderer&&UseHdrPostprocessor)
		{
			simulHDRRenderer->StartRender(context);
			simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		}
		else
			simulWeatherRenderer->SetExposureHint(1.0f);
ERROR_CHECK
		if(MixCloudsAndTerrain)
			simulWeatherRenderer->SetAlwaysRenderCloudsLate(MixCloudsAndTerrain);
		simulWeatherRenderer->RenderSky(context,UseSkyBuffer,false);

		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->StartRender(context);
		if(simulTerrainRenderer&&ShowTerrain)
			simulTerrainRenderer->Render(context);
		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->FinishRender(context);
		simulWeatherRenderer->RenderLightning(context);
			
		simulWeatherRenderer->SetDepthTexture(simulWeatherRenderer->GetBaseAtmosphericsRenderer()->GetDepthAlphaTexture());
		simulWeatherRenderer->RenderLateCloudLayer(context,true);

		simulWeatherRenderer->DoOcclusionTests();
		simulWeatherRenderer->RenderPrecipitation(context);
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			dir=simulWeatherRenderer->GetSkyRenderer()->GetDirectionToSun();
			light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
			float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			simulOpticsRenderer->RenderFlare(context,exp,dir,light);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulHDRRenderer->FinishRender(context);
ERROR_CHECK
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&CelestialDisplay)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(context,ScreenWidth,ScreenHeight);
		
		SetTopDownOrthoProjection(ScreenWidth,ScreenHeight);
		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(context,ScreenWidth,ScreenHeight);
		if(ShowCloudCrossSections)
		{
			if(simulWeatherRenderer->GetCloudRenderer()&&simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(context,ScreenWidth,ScreenHeight);
			}
			if(simulWeatherRenderer->Get2DCloudRenderer()&&simulWeatherRenderer->Get2DCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(context,ScreenWidth,ScreenHeight);
			}
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(NULL,ScreenWidth,ScreenHeight);
	}
	renderUI();
	glPopAttrib();
	simul::opengl::Profiler::GetGlobalProfiler().EndFrame();
}

void OpenGLRenderer::renderUI()
{
	glUseProgram(0);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D,0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	SetOrthoProjection(ScreenWidth,ScreenHeight);
	static char text[500];
	float y=12.f;
	static int line_height=16;
	RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,"OpenGL");
	if(ShowOSD)
	{
	static simul::base::Timer timer;
		timer.TimeSum=0;
		float t=timer.FinishTime();
		if(t<1.f)
			t=1.f;
		if(t>1000.f)
			t=1000.f;
		static float framerate=1.f;
		framerate*=0.95f;
		framerate+=0.05f*(1000.f/t);
		static char osd_text[256];
		sprintf_s(osd_text,256,"%3.3f fps",framerate);
		RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,osd_text);
		if(simulWeatherRenderer)
			RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,simulWeatherRenderer->GetDebugText());
		timer.StartTime();
	}
}

void OpenGLRenderer::resizeGL(int w,int h)
{
	ScreenWidth=w;
	ScreenHeight=h;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetScreenSize(ScreenWidth,ScreenHeight);
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(ScreenWidth,ScreenHeight);
}

void OpenGLRenderer::initializeGL()
{
    GLenum glewError = glewInit();
    if( glewError != GLEW_OK )
    {
        std::cerr<<"Error initializing GLEW! "<<glewGetErrorString( glewError )<<"\n";
        return;
    }
    //Make sure OpenGL 2.1 is supported
    if( !GLEW_VERSION_2_1 )
    {
        std::cerr<<"OpenGL 2.1 not supported!\n" ;
        return;
    }
//	const char* extensionsString = (const char*)glGetString(GL_EXTENSIONS);
// If the GL_GREMEDY_string_marker extension is supported:
	if(glewIsSupported("GL_GREMEDY_string_marker"))
	{
		// Get a pointer to the glStringMarkerGREMEDY function:
		glStringMarkerGREMEDY = (PFNGLSTRINGMARKERGREMEDYPROC)wglGetProcAddress("glStringMarkerGREMEDY");
	}
//CheckGLError(__FILE__,__LINE__,res);
	if(!GLEW_VERSION_2_0)
	{
		std::cerr<<"GL ERROR: No OpenGL 2.0 support on this hardware!\n";
	}
	CheckExtension("GL_VERSION_2_0");
	const GLubyte* pVersion = glGetString(GL_VERSION); 
	std::cout<<"GL_VERSION: "<<pVersion<<std::endl;
	if(cam)
		cam->LookInDirection(simul::math::Vector3(1.f,0,0),simul::math::Vector3(0,0,1.f));
	gpuCloudGenerator.RestoreDeviceObjects(NULL);
	gpuSkyGenerator.RestoreDeviceObjects(NULL);
	if(simulWeatherRenderer)
		simulWeatherRenderer->RestoreDeviceObjects(NULL);
	if(simulHDRRenderer)
		simulHDRRenderer->RestoreDeviceObjects();
	if(simulOpticsRenderer)
		simulOpticsRenderer->RestoreDeviceObjects(NULL);
	if(simulTerrainRenderer)
		simulTerrainRenderer->RestoreDeviceObjects(NULL);
}

void OpenGLRenderer::SetCamera(simul::camera::Camera *c)
{
	cam=c;
}

void OpenGLRenderer::ReloadTextures()
{
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->ReloadTextures();
}

void OpenGLRenderer::RecompileShaders()
{
	if(simulHDRRenderer.get())
		simulHDRRenderer->RecompileShaders();
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->RecompileShaders();
	if(simulTerrainRenderer.get())
		simulTerrainRenderer->RecompileShaders();
	gpuCloudGenerator.RecompileShaders();
	gpuSkyGenerator.RecompileShaders();
}
