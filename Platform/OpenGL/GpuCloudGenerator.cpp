#include "Simul/Platform/OpenGL/GpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
using namespace simul::opengl;

GpuCloudGenerator::GpuCloudGenerator():BaseGpuCloudGenerator()
			,density_program(0)
			,clouds_program(0)
			,transform_program(0)
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
	RecompileShaders();
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	for(int i=0;i<2;i++)
	{
		fb[i].InvalidateDeviceObjects();
	}
	SAFE_DELETE_PROGRAM(density_program);
	SAFE_DELETE_PROGRAM(transform_program);
	SAFE_DELETE_PROGRAM(clouds_program);
}

void GpuCloudGenerator::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(density_program);
	SAFE_DELETE_PROGRAM(transform_program);
	SAFE_DELETE_PROGRAM(clouds_program);
	density_program=MakeProgram("simul_gpu_cloud_density");
	clouds_program=MakeProgram("simul_gpu_clouds");
	transform_program=MakeProgram("simul_gpu_cloud_transform");
}

static GLuint make3DTexture(int w,int l,int d,int stride,bool wrap_z,const float *src)
{
	GLuint tex=0;
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_3D,tex);
	glTexImage3D(GL_TEXTURE_3D,0,stride==1?GL_LUMINANCE32F_ARB:GL_RGBA32F_ARB,w,l,d,0,stride==1?GL_LUMINANCE:GL_RGBA,GL_FLOAT,src);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//GL_LINEAR_MIPMAP_LINEAR
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,wrap_z?GL_REPEAT:GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_3D,0);
	return tex;
}

int GpuCloudGenerator::GetDensityGridsize(const int *grid)
{
	dens_fb.SetWidthAndHeight(grid[0],grid[1]);
	GLenum iformat=GL_LUMINANCE32F_ARB;
	if(!dens_fb.InitColor_Tex(0,iformat,GL_FLOAT))
	{
		if(!dens_fb.InitColor_Tex(0,iformat=GL_INTENSITY32F_ARB,GL_FLOAT))
			dens_fb.InitColor_Tex(0,iformat=GL_RGBA32F_ARB,GL_FLOAT);
	}
	int size=1;
	if(iformat==GL_RGBA32F_ARB)
		size=4;
	return grid[0]*grid[1]*grid[2]*size;
}

void GpuCloudGenerator::FillDensityGrid(float *target,const int *grid
											,float humidity
											,float time
											,int noise_size,int octaves,float persistence
											,const float *noise_src_ptr)
{
	if(!density_program)
		RecompileShaders();
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	dens_fb.SetWidthAndHeight(grid[0],grid[1]);
	GLenum iformat=GL_LUMINANCE32F_ARB;
	if(!dens_fb.InitColor_Tex(0,iformat,GL_FLOAT))
	{
		if(!dens_fb.InitColor_Tex(0,iformat=GL_INTENSITY32F_ARB,GL_FLOAT))
			dens_fb.InitColor_Tex(0,iformat=GL_RGBA32F_ARB,GL_FLOAT);
	}
	
	simul::math::Vector3 noiseScale(1.f,1.f,(float)grid[2]/(float)grid[0]);
	
	//using noise_size and noise_src_ptr, make a 3d texture:
	GLuint volume_noise_tex=make3DTexture(noise_size,noise_size,noise_size,1,true,noise_src_ptr);
	
	glEnable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,volume_noise_tex);
	
	glUseProgram(density_program);
	
	glUniform1i(glGetUniformLocation(density_program,"volumeNoiseTexture"	),0);
	glUniform1i(glGetUniformLocation(density_program,"octaves"				),octaves);
	glUniform1f(glGetUniformLocation(density_program,"persistence"			),persistence);
	glUniform1f(glGetUniformLocation(density_program,"humidity"				),humidity);
	glUniform1f(glGetUniformLocation(density_program,"time"					),time);
	glUniform1f(glGetUniformLocation(density_program,"zPixel"				),1.f/(float)grid[2]);
	glUniform3f(glGetUniformLocation(density_program,"noiseScale"			),noiseScale.x,noiseScale.y,noiseScale.z);
	
	// going vertically UP in the volume:
	for(int i=0;i<grid[2];i++)
	{
		float zPosition=((float)i+0.5f)/(float)grid[2];
		setParameter(density_program,"zPosition",zPosition);
			ERROR_CHECK
		dens_fb.Activate();
	ERROR_CHECK
		dens_fb.Clear(0.f,0.f,0.f,0.f,GL_COLOR_BUFFER_BIT);
	ERROR_CHECK
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
	ERROR_CHECK
			DrawQuad(0,0,1,1);
	ERROR_CHECK
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	ERROR_CHECK
		glReadPixels(0,0,grid[0],grid[1],GL_LUMINANCE,GL_FLOAT,(GLvoid*)target);
			ERROR_CHECK
		dens_fb.Deactivate();
		//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			ERROR_CHECK
		target+=grid[0]*grid[1];
	}
	glDisable(GL_TEXTURE_3D);
	glUseProgram(0);
	glDeleteTextures(1,&volume_noise_tex);
}

// The target is a grid of size given by light_gridsizes, arranged as d w-by-l textures.
void GpuCloudGenerator::PerformFullGPURelight(float *target,const int *light_grid
								,const float *density,const int *density_grid
								,const float *Matrix4x4LightToDensityTexcoords,const float *lightspace_extinctions_float3)
{
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(light_grid[0],light_grid[1]);
		fb[i].InitColor_Tex(0,GL_RGBA32F_ARB,GL_FLOAT);
	}
	GLuint density_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2]	,1,false,density);
	glUseProgram(clouds_program);
	setParameter(clouds_program,"input_light_texture",0);
	setParameter(clouds_program,"density_texture",1);
	setMatrix(clouds_program,"lightToDensityMatrix",Matrix4x4LightToDensityTexcoords);
	setParameter(clouds_program,"extinctions",lightspace_extinctions_float3[0],lightspace_extinctions_float3[1]);
	// initialize the first input texture.
	FramebufferGL *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,density_texture);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,F[0]->GetColorTex());
			ERROR_CHECK
			glBindTexture(GL_TEXTURE_2D,0);
			ERROR_CHECK
	F[0]->Activate();
			ERROR_CHECK
	F[0]->Clear(1.f,1.f,1.f,0.f);
			ERROR_CHECK
	F[0]->Deactivate();
			ERROR_CHECK
	for(int i=0;i<light_grid[2];i++)
	{
		float zPosition=((float)i+0.5f)/(float)light_grid[2];
		setParameter(clouds_program,"zPosition",zPosition);
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
			DrawQuad(0,0,1,1);
			ERROR_CHECK
		// Copy F[1] contents to the target
		//glBindFramebuffer(GL_FRAMEBUFFER, F[1]->GetFramebuffer());
		//glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, F[1]->GetFramebuffer());
			ERROR_CHECK
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			ERROR_CHECK
		glReadPixels(0,0,light_grid[0],light_grid[1],GL_RGBA,GL_FLOAT,(GLvoid*)target);
			ERROR_CHECK
		F[1]->Deactivate();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			ERROR_CHECK
		std::swap(F[0],F[1]);
		target+=light_grid[0]*light_grid[1]*4;
	}
	glUseProgram(0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	SAFE_DELETE_TEXTURE(density_texture);
}

// Transform light data into a world-oriented cloud texture.
// The inputs are in RGBA float32 format, with the light values in the RG slots.
void GpuCloudGenerator::GPUTransferDataToTexture(unsigned char *target
											,const float *DensityToLightTransform
											,const float *light,const int *light_grid
											,const float *ambient
											,const float *density,const int *density_grid)
{
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(density_grid[0],density_grid[1]*density_grid[2]);
	world_fb.InitColor_Tex(0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8);
	GLuint density_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2]	,1,false,density);
	GLuint light_texture	=make3DTexture(light_grid[0]	,light_grid[1],light_grid[2]	,4,false,light);
	GLuint ambient_texture	=make3DTexture(density_grid[0],density_grid[1],density_grid[2]	,4,false,ambient);
			glUseProgram(transform_program);
	setParameter(transform_program,"density_texture",0);
	setParameter(transform_program,"light_texture",1);
	setParameter(transform_program,"ambient_texture",2);
	setMatrix(transform_program,"transformMatrix",DensityToLightTransform);
	setParameter(transform_program,"zSize",(float)density_grid[2]);
	setParameter(transform_program,"zPixel",1.f/(float)density_grid[2]);
	glEnable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,density_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,light_texture);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D,ambient_texture);
	//for(int i=0;i<density_grid[2];i++)
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	{
		world_fb.Activate();
		world_fb.Clear(0.f,0.f,0.f,0.f);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			DrawQuad(0,0,1,1);
			ERROR_CHECK
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			ERROR_CHECK
		glReadPixels(0,0,density_grid[0],density_grid[1]*density_grid[2],GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,(GLvoid*)target);
			ERROR_CHECK
		world_fb.Deactivate();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			ERROR_CHECK
		//target+=density_grid[0]*density_grid[1]*4;
	}
	glUseProgram(0);
	glDisable(GL_TEXTURE_3D);
	glDisable(GL_TEXTURE_2D);
	glDeleteTextures(1,&ambient_texture);
	glDeleteTextures(1,&density_texture);
	glDeleteTextures(1,&light_texture);
}