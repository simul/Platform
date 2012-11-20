#include "Simul/Platform/OpenGL/GpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
using namespace simul::opengl;


GpuCloudGenerator::GpuCloudGenerator():BaseGpuCloudGenerator()
{
}

GpuCloudGenerator::~GpuCloudGenerator()
{
}

void GpuCloudGenerator::CreateVolumeNoiseTexture(int size,const float *src_ptr)
{
}

void GpuCloudGenerator::RestoreDeviceObjects(void *dev)
{
	for(int i=0;i<2;i++)
	{
	}
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	for(int i=0;i<2;i++)
	{
		fb[i].InvalidateDeviceObjects();
	}
}

void GpuCloudGenerator::RecompileShaders()
{
}

/*
	float LightToDensityTransform[16];
	float DensityToLightTransform[16];
	int light_gridsizes[3];
	float light_extinctions[4];
	int num_octaves;
	float persistence_val;
	float humidity_val;
	float time_val;
*/

// The target is a grid of size given by light_gridsizes, arranged as d w-by-l textures.
void GpuCloudGenerator::PerformFullGPURelight(float *target_direct_grid)
{
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(this->light_gridsizes[0],this->light_gridsizes[1]);
		fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
	GLuint			program						=MakeProgram("simul_sky");
	GLint			planetTexture_param;
	SAFE_DELETE_PROGRAM(program);
	// initialize the first input texture.
	FramebufferGL *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	
	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,F[0]->GetColorTex());
			ERROR_CHECK
			glBindTexture(GL_TEXTURE_2D,0);;
			ERROR_CHECK
	F[0]->Activate();;
			ERROR_CHECK
	F[0]->Clear(1.f,1.f,1.f,0.f);;
			ERROR_CHECK
	F[0]->Deactivate();;
			ERROR_CHECK
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,F[0]->GetColorTex());
			ERROR_CHECK
	for(int i=0;i<light_gridsizes[2];i++)
	{
		F[1]->Activate();
			F[1]->Clear(0.f,0.f,0.f,0.f);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,F[0]->GetColorTex());
			/*glUseProgram(program);
			DrawQuad(0,0,1,1);
			glUseProgram(0);*/
			ERROR_CHECK
		F[1]->Deactivate();
			ERROR_CHECK
		// Copy F[1] contents to the target
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, F[1]->GetFramebuffer());
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadPixels(0,0,light_gridsizes[0],light_gridsizes[1],GL_RGBA,GL_FLOAT,(GLvoid*)target_direct_grid);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		target_direct_grid+=light_gridsizes[0]*light_gridsizes[1]*4;
	}
}

// Transform light data into a world-oriented cloud texture.
void GpuCloudGenerator::GPUTransferDataToTexture(unsigned char *target_texture
									,const float *light_grid
									,const float *ambient_grid)
{
}