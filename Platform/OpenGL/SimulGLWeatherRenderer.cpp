#include "SimulGLWeatherRenderer.h"
#include "SimulGLSkyRenderer.h"
#include "SimulGLCloudRenderer.h"
#include "SimulGL2DCloudRenderer.h"
#include "Simul/Clouds/CloudInterface.h"
#include "Simul/Sky/FadeTableInterface.h"
#include "Simul/Sky/AltitudeFadeTable.h"
#include "Simul/Sky/SkyInterface.h"
#include "RenderTextureFBO.h"

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
	clouds_buffer=NULL;
    if(clouds_buffer)
        delete clouds_buffer;
    clouds_buffer=new RenderTexture(BufferWidth,BufferHeight,GL_TEXTURE_2D);
    clouds_buffer->InitColor_Tex(0,buffer_format);

	// Now we know what time of day it is, initialize the sky texture:
	simulSkyRenderer=new SimulGLSkyRenderer();
	if(simulSkyRenderer)
	{
		simulSkyRenderer->Create(0.5f);
		simulSkyRenderer->RestoreDeviceObjects();
	}
	//simul2DCloudRenderer=new SimulGL2DCloudRenderer();
	if(simul2DCloudRenderer)
	{
		simul2DCloudRenderer->SetSkyInterface(simulSkyRenderer->GetSkyInterface());
		simul2DCloudRenderer->SetFadeTable(simulSkyRenderer->GetFadeTableInterface());
		simul2DCloudRenderer->Create();
		simul2DCloudRenderer->RestoreDeviceObjects();
	}
	
	simulCloudRenderer=new SimulGLCloudRenderer();
	if(simulCloudRenderer)
	{
		simulCloudRenderer->Create();
		simulCloudRenderer->RestoreDeviceObjects();
	}
	ConnectInterfaces();
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
			simulCloudRenderer->SetSkyInterface(simulSkyRenderer->GetFadeTable());
			simulCloudRenderer->SetFadeTable(simulSkyRenderer->GetFadeTableInterface());
			simulSkyRenderer->SetOvercastCallback(simulCloudRenderer->GetOvercastCallback());
		}
		//if(simulAtmosphericsRenderer)
		//	simulAtmosphericsRenderer->SetSkyInterface(simulSkyRenderer->GetFadeTable());
	}
}

SimulGLWeatherRenderer::~SimulGLWeatherRenderer()
{
}

void SimulGLWeatherRenderer::Render(bool)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	if(simulSkyRenderer)
	{
		simulSkyRenderer->Render();
		simulSkyRenderer->RenderPlanets();
	}
    if(simul2DCloudRenderer)
		simul2DCloudRenderer->Render();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
// Render the clouds to the cloud buffer:

void SimulGLWeatherRenderer::RenderLateCloudLayer(float gamma)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	//clouds_buffer->Activate();

    if(simulCloudRenderer)
		simulCloudRenderer->Render(gamma);

	//clouds_buffer->DeactivateAndRender(BufferWidth,BufferHeight,0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void SimulGLWeatherRenderer::Update(float dt)
{
	if(simul2DCloudRenderer.get())
		simul2DCloudRenderer->Update(dt);
	if(GetSkyRenderer())
	{
		GetSkyRenderer()->Update(dt);
	}
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