
#include "RenderTextureFBO.h"
#include "LoadGLProgram.h"
#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "SimulGLLightningRenderer.h"
#include "SimulGLUtilities.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/SkyKeyframer.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Clouds/LightningRenderInterface.h"
#include "Simul/Clouds/Cloud2DGeometryHelper.h"

#if 0//def _MSC_VER
static GLuint buffer_format=GL_RGBA16F_ARB;
#else
static GLuint buffer_format=GL_RGBA32F_ARB;
#endif

SimulGLWeatherRenderer::SimulGLWeatherRenderer(bool usebuffer,bool tonemap,int width,
		int height,bool sky,bool clouds3d,bool clouds2d,
		bool rain,
		bool colour_sky):
		BufferWidth(width)
		,BufferHeight(height)
{
	glewInit();
	CheckExtension("GL_VERSION_2_0");
	CheckExtension("GL_ARB_fragment_program");
	CheckExtension("GL_ARB_vertex_program");
	CheckExtension("GL_ARB_texture_float");
	CheckExtension("GL_ARB_color_buffer_float");
	CheckExtension("GL_EXT_framebuffer_object");

	scene_buffer=NULL;
    if(scene_buffer)
        delete scene_buffer;
   scene_buffer=new RenderTexture(BufferWidth,BufferHeight,GL_TEXTURE_2D);

    scene_buffer->InitColor_Tex(0,buffer_format);
  // scene_buffer->InitDepth_RB();

	// Now we know what time of day it is, initialize the sky texture:
	if(sky)
	{
		simulSkyRenderer=new SimulGLSkyRenderer();
		baseSkyRenderer=simulSkyRenderer.get();
	}
	if(simulSkyRenderer)
	{
		simulSkyRenderer->Create(0.5f);
		simulSkyRenderer->RestoreDeviceObjects();
	}
	EnableLayers(clouds3d,clouds2d);
	ConnectInterfaces();
}


void SimulGLWeatherRenderer::EnableLayers(bool clouds3d,bool clouds2d)
{
	simul::clouds::BaseWeatherRenderer::EnableLayers(clouds3d,clouds2d);
	//if(clouds2d)
	//	base2DCloudRenderer=simul2DCloudRenderer=new SimulGL2DCloudRenderer();
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simul2DCloudRenderer->SetFadeTable(simulSkyRenderer->GetFadeTableInterface());
		simul2DCloudRenderer->Create();
		simul2DCloudRenderer->RestoreDeviceObjects();
	}
	if(clouds3d)
	{	
		simulCloudRenderer=new SimulGLCloudRenderer();
		baseCloudRenderer=simulCloudRenderer.get();
		simulLightningRenderer=new SimulGLLightningRenderer(simulCloudRenderer->GetLightningRenderInterface());
		baseLightningRenderer=simulLightningRenderer.get();
	}
	if(simulCloudRenderer)
	{
		simulCloudRenderer->Create();
		simulCloudRenderer->RestoreDeviceObjects();
	}
	if(simulLightningRenderer)
	{
		simulLightningRenderer->RestoreDeviceObjects();
	}
}

void SimulGLWeatherRenderer::ConnectInterfaces()
{
	if(simulSkyRenderer)
	{
		if(simul2DCloudRenderer)
		{
			simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
			simul2DCloudRenderer->SetFadeTableInterface(simulSkyRenderer->GetFadeTableInterface());
		}
		if(simulCloudRenderer)
		{
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
		}
		//if(simulAtmosphericsRenderer)
		//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetSkyKeyframer());
	}
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
}

void SimulGLWeatherRenderer::RenderSky(bool buffered)
{

	ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(simulSkyRenderer)
		simulSkyRenderer->RenderPlanets();
	// Everything between Activate() and DeactivateAndRender() is drawn to the buffer object.
	if(buffered)
		scene_buffer->Activate();

    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	ERROR_CHECK
	if(simulSkyRenderer)
	{
	ERROR_CHECK
		simulSkyRenderer->Render();
	}
	ERROR_CHECK
    if(simul2DCloudRenderer)
		simul2DCloudRenderer->Render();
	ERROR_CHECK

	if(buffered)
		scene_buffer->DeactivateAndRender(true);
	ERROR_CHECK

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
	ERROR_CHECK
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	ERROR_CHECK
		int d=0;
	glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&d);
	glPopAttrib();
	ERROR_CHECK
}

void SimulGLWeatherRenderer::RenderLightning()
{
	if(simulCloudRenderer&&simulLightningRenderer&&layer1)
		simulLightningRenderer->Render();
}


void SimulGLWeatherRenderer::RenderClouds(bool buffered,bool depth_testing,bool default_fog)
{
	ERROR_CHECK
	glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	if(buffered)
		scene_buffer->Activate();

    if(simulCloudRenderer)
		simulCloudRenderer->Render(depth_testing,default_fog);

	if(buffered)
		scene_buffer->DeactivateAndRender(true);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
	glPopAttrib();
	ERROR_CHECK
}

// Render the clouds to the cloud buffer:
void SimulGLWeatherRenderer::SetPrecalculatedGamma(float g)
{
    if(simulCloudRenderer)
		simulCloudRenderer->GetCloudKeyframer()->SetPrecalculatedGamma(g);
}

void SimulGLWeatherRenderer::Update(float dt)
{
	if(simulCloudRenderer.get())
		simulCloudRenderer->Update(dt);
	if(simul2DCloudRenderer.get())
		simul2DCloudRenderer->Update(dt);
	if(GetSkyRenderer())
		GetSkyRenderer()->Update(dt);
}

class SimulGLSkyRenderer *SimulGLWeatherRenderer::GetSkyRenderer()
{
	return simulSkyRenderer.get();
}

class SimulGLCloudRenderer *SimulGLWeatherRenderer::GetCloudRenderer()
{
	return simulCloudRenderer.get();
}

class SimulGL2DCloudRenderer *SimulGLWeatherRenderer::Get2DCloudRenderer()
{
	return simul2DCloudRenderer.get();
}

static void writechar(std::ostream &os,char c)
{
	os.write((const char*)&c,sizeof(c));
}
static char readchar(std::istream &is)
{
	char c;
	is.read((char*)&c,sizeof(c));
	return c;
}

std::ostream &SimulGLWeatherRenderer::Save(std::ostream &os) const
{
	//\211   P   N   G  \r  \n \032 \n
	writechar(os,-45);
	writechar(os,'S');
	writechar(os,'E');
	writechar(os,'Q');
	writechar(os,'\r');
	writechar(os,'\n');
	writechar(os,32);
	int num_streams=2;
	os.write((const char*)&num_streams,sizeof(num_streams));
	int stream_type=simulSkyRenderer->IsColourSkyEnabled()?2:1;
	os.write((const char*)&stream_type,sizeof(stream_type));
	simulSkyRenderer->Save(os);
	stream_type=0;
	os.write((const char*)&stream_type,sizeof(stream_type));
	simulCloudRenderer->Save(os);
	return os;
}

std::istream &SimulGLWeatherRenderer::Load(std::istream &is)
{
	if(readchar(is)!=-45) return is;
	if(readchar(is)!='S') return is;
	if(readchar(is)!='E') return is;
	if(readchar(is)!='Q') return is;
	if(readchar(is)!='\r') return is;
	if(readchar(is)!='\n') return is;
	if(readchar(is)!=32) return is;
	if(readchar(is)!='\n') return is;
	int num_streams=0;
	is.read((char*)&num_streams,sizeof(num_streams));
	for(int i=0;i<num_streams;i++)
	{
		int stream_type=-1;
		is.read((char*)&stream_type,sizeof(stream_type));
		if(stream_type==1||stream_type==2)
		{
			simulSkyRenderer->EnableColourSky(stream_type==2);
			simulSkyRenderer->Load(is);
		}
		if(stream_type==0)
		{
			simulCloudRenderer->Load(is);
		}
	}
	ConnectInterfaces();
	return is;
}
