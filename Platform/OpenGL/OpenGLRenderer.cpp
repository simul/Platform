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
#include "Simul/Sky/Float4.h"
#include "Simul/Base/Timer.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#define GLUT_BITMAP_HELVETICA_12	((void*)7)

OpenGLRenderer::OpenGLRenderer(simul::clouds::Environment *env)
	:width(0)
	,height(0)
	,cam(NULL)
	,ShowFlares(true)
	,ShowFades(false)
	,ShowTerrain(true)
	,ShowCloudCrossSections(false)
	,celestial_display(false)
	,y_vertical(false)
	,UseHdrPostprocessor(true)
	,ShowOSD(false)
	,ShowWater(true)
	,ReverseDepth(false)
	,MixCloudsAndTerrain(false)
{
	simulHDRRenderer=new SimulGLHDRRenderer(width,height);
	simulWeatherRenderer=new SimulGLWeatherRenderer(env,true,false,width,height);
	simulOpticsRenderer=new SimulOpticsRendererGL();
	simulTerrainRenderer=new SimulGLTerrainRenderer();
	simulTerrainRenderer->SetBaseSkyInterface(simulWeatherRenderer->GetSkyKeyframer());
	SetYVertical(y_vertical);
}

OpenGLRenderer::~OpenGLRenderer()
{
	if(simulTerrainRenderer)
		simulTerrainRenderer->InvalidateDeviceObjects();
	if(simulWeatherRenderer)
		simulWeatherRenderer->InvalidateDeviceObjects();
	gpuCloudGenerator.InvalidateDeviceObjects();
	gpuSkyGenerator.InvalidateDeviceObjects();
}

void OpenGLRenderer::paintGL()
{
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
	static float max_dist=250000.f;
	glLoadMatrixf(cam->MakeProjectionMatrix(1.f,max_dist,(float)width/(float)height,y_vertical));
	glViewport(0,0,width,height);
	if(simulWeatherRenderer.get())
	{
		simulWeatherRenderer->Update(0.0);
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
			simulHDRRenderer->StartRender();
			simulWeatherRenderer->SetExposureHint(simulHDRRenderer->GetExposure());
		}
		else
			simulWeatherRenderer->SetExposureHint(1.0f);
ERROR_CHECK
		if(MixCloudsAndTerrain)
			simulWeatherRenderer->SetAlwaysRenderCloudsLate(MixCloudsAndTerrain);
		simulWeatherRenderer->RenderSky(UseSkyBuffer,false);

		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->StartRender();
		if(simulTerrainRenderer&&ShowTerrain)
			simulTerrainRenderer->Render();
		if(simulWeatherRenderer->GetBaseAtmosphericsRenderer()&&simulWeatherRenderer->GetShowAtmospherics())
			simulWeatherRenderer->GetBaseAtmosphericsRenderer()->FinishRender();
		simulWeatherRenderer->RenderLightning();
			
		simulWeatherRenderer->SetDepthTexture(simulWeatherRenderer->GetBaseAtmosphericsRenderer()->GetDepthAlphaTexture());
		simulWeatherRenderer->RenderLateCloudLayer(true);

		simulWeatherRenderer->DoOcclusionTests();
		simulWeatherRenderer->RenderPrecipitation();
		if(simulOpticsRenderer&&ShowFlares)
		{
			simul::sky::float4 dir,light;
			dir=simulWeatherRenderer->GetSkyRenderer()->GetDirectionToSun();
			light=simulWeatherRenderer->GetSkyRenderer()->GetLightColour();
			float occ=simulWeatherRenderer->GetSkyRenderer()->GetSunOcclusion();
			float exp=(simulHDRRenderer?simulHDRRenderer->GetExposure():1.f)*(1.f-occ);
			simulOpticsRenderer->RenderFlare(exp,dir,light);
		}
		if(simulHDRRenderer&&UseHdrPostprocessor)
			simulHDRRenderer->FinishRender();
ERROR_CHECK
		if(simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer()&&celestial_display)
			simulWeatherRenderer->GetSkyRenderer()->RenderCelestialDisplay(width,height);
		
		SetTopDownOrthoProjection(width,height);
		if(ShowFades&&simulWeatherRenderer&&simulWeatherRenderer->GetSkyRenderer())
			simulWeatherRenderer->GetSkyRenderer()->RenderFades(width,height);
		if(ShowCloudCrossSections)
		{
			if(simulWeatherRenderer->GetCloudRenderer()&&simulWeatherRenderer->GetCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->GetCloudRenderer()->RenderCrossSections(width,height);
			}
			if(simulWeatherRenderer->Get2DCloudRenderer()&&simulWeatherRenderer->Get2DCloudRenderer()->GetCloudKeyframer()->GetVisible())
			{
				simulWeatherRenderer->Get2DCloudRenderer()->RenderCrossSections(width,height);
			}
		}
		if(ShowOSD&&simulWeatherRenderer->GetCloudRenderer())
			simulWeatherRenderer->GetCloudRenderer()->RenderDebugInfo(width,height);
	}
	renderUI();
	glPopAttrib();
}

void OpenGLRenderer::renderUI()
{
	glUseProgram(NULL);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_1D);
	SetOrthoProjection(width,height);
	static char text[500];
	float y=12.f;
	static int line_height=16;
	RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,"OpenGL");
	if(ShowOSD)
	{
	static simul::base::Timer timer;
		timer.TimeSum=0;
		float t=timer.FinishTime();
		static float framerate=1.f;
		framerate*=0.99f;
		framerate+=0.01f*(1000.f/t);
		static char osd_text[256];
		sprintf_s(osd_text,256,"%3.3f fps",framerate);
		RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,osd_text);

		if(simulWeatherRenderer)
			RenderString(12.f,y+=line_height,GLUT_BITMAP_HELVETICA_12,simulWeatherRenderer->GetDebugText());
		
		
		timer.StartTime();
		
		
	}
}
	
void OpenGLRenderer::SetCelestialDisplay(bool val)
{
	celestial_display=val;
}

void OpenGLRenderer::resizeGL(int w,int h)
{
	width=w;
	height=h;
	if(simulWeatherRenderer)
		simulWeatherRenderer->SetScreenSize(width,height);
	if(simulHDRRenderer)
		simulHDRRenderer->SetBufferSize(width,height);
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

void OpenGLRenderer::SetYVertical(bool y)
{
	y_vertical=y;
	if(simulWeatherRenderer.get())
		simulWeatherRenderer->SetYVertical(y);
	//if(simulTerrainRenderer.get())
	//	simulTerrainRenderer->SetYVertical(y_vertical);
	if(simulOpticsRenderer)
		simulOpticsRenderer->SetYVertical(y_vertical);
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
