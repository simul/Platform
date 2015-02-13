#define NOMINMAX
#include <gl/glew.h>
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Platform/OpenGL/GpuCloudGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Base/Timer.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/gpu_cloud_constants.sl"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"

using namespace simul;
using namespace opengl;

GpuCloudGenerator::GpuCloudGenerator():BaseGpuCloudGenerator()
	,density(NULL)
	,density_gridsize(0)
	,readback_to_cpu(true)
	,last_generation_number(-1)
{
	for(int i=0;i<3;i++)
		finalTexture[i]=NULL;
}

GpuCloudGenerator::~GpuCloudGenerator()
{
	InvalidateDeviceObjects();
	delete [] density;
}

void GpuCloudGenerator::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseGpuCloudGenerator::RestoreDeviceObjects(r);
	fb[0].RestoreDeviceObjects(r);
	fb[1].RestoreDeviceObjects(r);
	world_fb.RestoreDeviceObjects(r);
	dens_fb.RestoreDeviceObjects(r);
	iformat=crossplatform::INT_32_FLOAT;
	itype=GL_LUMINANCE;
	RecompileShaders();
	maskTexture.ensureTexture2DSizeAndFormat(renderPlatform,16,16,crossplatform::RGBA_16_FLOAT,false,true);
	gpuCloudConstants.RestoreDeviceObjects(renderPlatform);
}

void GpuCloudGenerator::InvalidateDeviceObjects()
{
	BaseGpuCloudGenerator::InvalidateDeviceObjects();
	for(int i=0;i<2;i++)
	{
		fb[i].InvalidateDeviceObjects();
	}
	gpuCloudConstants.InvalidateDeviceObjects();
	maskTexture.InvalidateDeviceObjects();
	SAFE_DELETE_TEXTURE(volume_noise_tex);
	renderPlatform = NULL;
}

void GpuCloudGenerator::RecompileShaders()
{
	BaseGpuCloudGenerator::RecompileShaders();
	if (!renderPlatform)
		return;
	gpuCloudConstants	.LinkToEffect(effect,"GpuCloudConstants");
	gpuCloudConstants	.LinkToEffect(effect,"GpuCloudConstants");
	gpuCloudConstants	.LinkToEffect(effect,"GpuCloudConstants");
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
	if(iformat==crossplatform::LUM_32_FLOAT)
	{
		dens_fb.SetWidthAndHeight(grid[0],grid[1]*grid[2]);
		if(!dens_fb.InitColor_Tex(0,iformat))
		{
			itype=GL_INTENSITY;
			iformat=crossplatform::INT_32_FLOAT;
			if(!dens_fb.InitColor_Tex(0,iformat))
			{
				itype=GL_RGBA;
				iformat=crossplatform::RGBA_32_FLOAT;
				dens_fb.InitColor_Tex(0,iformat);
			}
		}
	}
	return grid[0]*grid[1]*grid[2];
}

crossplatform::Texture* GpuCloudGenerator::Make3DNoiseTexture(int noise_size,const float *noise_src_ptr,int generation_number)
{
	if(generation_number==last_generation_number&&volume_noise_tex)
		return NULL;
	last_generation_number	=generation_number;
	noiseSize				=noise_size;
	SAFE_DELETE_TEXTURE(volume_noise_tex);
	volume_noise_tex		=(GLuint)make3DTexture(noise_size,noise_size,noise_size,1,true,noise_src_ptr);
	return NULL;
}

void GpuCloudGenerator::CycleTexturesForward()
{
}

void CopyTo3DTexture(int start_texel,int texels,const int grid[3])
{
	
	// Now instead of reading the pixels back to memory, we will copy them layer-by-layer into the volume texture.
	int Y=start_texel/grid[0];
	int H=texels/grid[0];
	int z0=Y/grid[1];
	int z1=(start_texel+texels)/grid[0]/grid[1];
	int y0=Y-z0*grid[1];
	int y1=Y+H-(z1-1)*grid[1];
	if(y1>grid[1])
	{
		y1-=grid[1];
		z1++;
	}
	if(z1>grid[2])
		z1=grid[2];
	for(int z=z0;z<z1;z++)
	{
		int y=0,dy=grid[1];
		if(z==z0)
		{
			y=y0;
			dy=grid[1]-y0;
		}
		if(z==z1-1)
		{
			dy=y1-y;
		}
GL_ERROR_CHECK
		glCopyTexSubImage3D(	GL_TEXTURE_3D,
								0,							//	level
								0,							//	x offset in 3D texture
								y,							//	y offset in 3D texture
								z,							//	z offset in 3D texture
								0,							//	x=0 in source 2D texture
								z*grid[1]+y,				//	y offset in source 2D texture
								grid[0],					//	width to copy
								dy);						// length to copy.
GL_ERROR_CHECK
GL_ERROR_CHECK
	}
}
static void CopyTo3DTextureLayer(int z,const int grid[3])
{
	
	glCopyTexSubImage3D(	GL_TEXTURE_3D,
							0,							//	level
							0,							//	x offset in 3D texture
							0,							//	y offset in 3D texture
							z,							//	z offset in 3D texture
							0,							//	x=0 in source 2D texture
							0,							//	y offset in source 2D texture
							grid[0],					//	width to copy
							grid[1]);					// length to copy.
}
// Fill the stated number of texels of the density texture
void GpuCloudGenerator::FillDensityGrid(int /*index*/,const clouds::GpuCloudsParameters &params
											,int start_texel
											,int texels)
{
	int total_texels=GetDensityGridsize(params.density_grid);
	if(!effect)
		RecompileShaders();
	crossplatform::DeviceContext deviceContext;
	// We render out a 2D texture with each XY layer laid end-to-end, and copy it to the target.
	dens_fb.SetWidthAndHeight(params.density_grid[0],params.density_grid[1]*params.density_grid[2]);
	if(!dens_fb.InitColor_Tex(0,iformat))
	{
		itype=GL_INTENSITY;
		iformat=crossplatform::INT_32_FLOAT;
		if(!dens_fb.InitColor_Tex(0,iformat))
		{
			itype=GL_RGBA;
			iformat=crossplatform::RGBA_32_FLOAT;
			dens_fb.InitColor_Tex(0,iformat);
		}
	}
	int stride=(iformat==GL_RGBA32F_ARB)?4:1;
	simul::math::Vector3 noise_scale(1.f,1.f,(float)params.density_grid[2]/(float)params.density_grid[0]);
	//using noise_size and noise_src_ptr, make a 3d texture:
	//glUseProgram(density_program);
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("gpu_density");
	if(!tech)
		return;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	effect->Apply(deviceContext,tech,0);
	setParameter(tech->passAsGLuint(0),"volumeNoiseTexture"	,0);
//	setParameter(density_program,"maskTexture"			,1);
	//MakeVertexMatrix(params.density_grid,start_texel,texels);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,1.0,0.0,1.0,-1.0,1.0);
	float y_start=(float)start_texel/(float)total_texels;
	float y_end=(float)(start_texel+texels)/(float)total_texels;
	gpuCloudConstants.transformMatrix	=params.Matrix4x4DensityToLightTransform;
	SetGpuCloudConstants(gpuCloudConstants,params,y_start,y_end-y_start);
	gpuCloudConstants.zPixel			=1.f/(float)params.density_grid[2];
	gpuCloudConstants.zSize				=(float)params.density_grid[2];
	gpuCloudConstants.Apply(deviceContext);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,volume_noise_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,maskTexture.AsGLuint());
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	{
GL_ERROR_CHECK
		dens_fb.Activate(deviceContext);

GL_ERROR_CHECK
		DrawQuad(0.f,y_start,1.f,y_end-y_start);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D,density_texture->AsGLuint());

		int w,l,d;
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH	,&w);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_HEIGHT	,&l);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_DEPTH	,&d);
		
		if(!density_texture||density_gridsize!=total_texels*stride||w!=params.density_grid[0]||l!=params.density_grid[1]||d!=params.density_grid[2])
		{
			density_texture->ensureTexture3DSizeAndFormat(renderPlatform
				,params.density_grid[0],params.density_grid[1],params.density_grid[2]
				,crossplatform::R_32_FLOAT,true);
			if(readback_to_cpu&&total_texels*stride>density_gridsize)
			{
				density_gridsize=total_texels*stride;
			}
		}
	GL_ERROR_CHECK
			glBindTexture(GL_TEXTURE_3D,density_texture->AsGLuint());
		CopyTo3DTexture(start_texel,texels,params.density_grid);
		dens_fb.Deactivate(deviceContext);
	}
	effect->Unapply(deviceContext);
//	glUseProgram(0);
GL_ERROR_CHECK
GL_ERROR_CHECK
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
GL_ERROR_CHECK
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

// The target is a grid of size given by light_gridsizes, arranged as d w-by-l textures.
void GpuCloudGenerator::PerformGPURelight(int light_index
											,const clouds::GpuCloudsParameters &params
											,float *target
											,int start_texel
											,int texels)
{
	crossplatform::DeviceContext deviceContext;
GL_ERROR_CHECK
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight(params.light_grid[0],params.light_grid[1]);
		fb[i].InitColor_Tex(0,crossplatform::RGBA_32_FLOAT);
	}
	directLightTextures[light_index].ensureTexture3DSizeAndFormat(NULL
				,params.light_grid[0],params.light_grid[1],params.light_grid[2]
		,crossplatform::RGBA_32_FLOAT/*GL_LUMINANCE32F_ARB*/,true);
	indirectLightTextures[light_index].ensureTexture3DSizeAndFormat(NULL
				,params.light_grid[0],params.light_grid[1],params.light_grid[2]
				,crossplatform::LUM_32_FLOAT,true);
	// We use the density framebuffer texture as our density texture. This works well
	// because we don't need to do any filtering.
	//GLuint density_texture	=dens_fb.GetColorTex();
	// blit from dens_fb...
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("gpu_lighting");
	if(!tech)
		return;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	effect->Apply(deviceContext,tech,0);
	setParameter(tech->passAsGLuint(0),"input_light_texture",0);
	setParameter(tech->passAsGLuint(0),"density_texture",1);
	int total_texels=params.light_grid[0]*params.light_grid[1]*params.light_grid[2];
	float y_start=(float)start_texel/(float)total_texels;
	float y_end=(float)(start_texel+texels)/(float)total_texels;
	gpuCloudConstants.transformMatrix	=params.Matrix4x4LightToDensityTexcoords;
	SetGpuCloudConstants(gpuCloudConstants,params,y_start,y_end-y_start);
	gpuCloudConstants.extinctions		=params.lightspace_extinctions;
	gpuCloudConstants.Apply(deviceContext);
	// initialize the first input texture.
	FramebufferGL *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,density_texture->AsGLuint());
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetTexture()->AsGLuint());
	GL_ERROR_CHECK
	glBindTexture(GL_TEXTURE_2D,0);
	GL_ERROR_CHECK
	int light_gridsize=params.light_grid[0]*params.light_grid[1]*params.light_grid[2];
	int z0=(start_texel*params.light_grid[2])/light_gridsize;
	int z1=((start_texel+texels)*params.light_grid[2])/light_gridsize;
	if(z0%2)
	{
		std::swap(F[0],F[1]);
	}
	if(z0==0)
	{
		F[0]->Activate(deviceContext);
		F[0]->Clear(deviceContext, 1.f, 1.f, 1.f, 1.f, 1.f);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			if(target)
				glReadPixels(0,0,params.light_grid[0],params.light_grid[1],GL_RGBA,GL_FLOAT,(GLvoid*)target);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D,directLightTextures[light_index].AsGLuint());
			CopyTo3DTextureLayer(0,params.light_grid);
		F[0]->Deactivate(deviceContext);
		z0++;
	}
	if(target)
		target+=z0*params.light_grid[0]*params.light_grid[1]*4;
GL_ERROR_CHECK
//	float draw_time=0.f,read_time=0.f;
	for(int i=z0;i<z1;i++)
	{
		float zPosition=((float)i-0.5f)/(float)params.light_grid[2];

		gpuCloudConstants.zPosition=zPosition;
		gpuCloudConstants.Apply(deviceContext);
		F[1]->Activate(deviceContext);
float u=(float)i/(float)z1;
F[1]->Clear(deviceContext, u, u, u, u, 1.f);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetTexture()->AsGLuint());
			effect->Reapply(deviceContext);
			DrawQuad(0,0,1,1);
			GL_ERROR_CHECK
		// Copy F[1] contents to the target
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			if(target)
				glReadPixels(0,0,params.light_grid[0],params.light_grid[1],GL_RGBA,GL_FLOAT,(GLvoid*)target);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D,directLightTextures[light_index].AsGLuint());
			CopyTo3DTextureLayer(i,params.light_grid);
			GL_ERROR_CHECK
		F[1]->Deactivate(deviceContext);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		std::swap(F[0],F[1]);
		if(target)
			target+=params.light_grid[0]*params.light_grid[1]*4;
	}
	effect->Unapply(deviceContext);
	
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

// Transform light data into a world-oriented cloud texture.
// The inputs are in RGBA float32 format, with the light values in the RG slots.
void GpuCloudGenerator::GPUTransferDataToTexture(int cycled_index
												,const clouds::GpuCloudsParameters &params
												,unsigned char *target
												,int start_texel
												,int texels)
{
	if(texels<=0)
		return;
	crossplatform::DeviceContext deviceContext;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	int total_texels=params.density_grid[0]*params.density_grid[1]*params.density_grid[2];
	// For each level in the z direction, we render out a 2D texture and copy it to the target.
	world_fb.SetWidthAndHeight(params.density_grid[0],params.density_grid[1]*params.density_grid[2]);
	world_fb.InitColor_Tex(0,crossplatform::RGBA_8_UNORM);
	effect->Apply(deviceContext,effect->GetTechniqueByName("gpu_transform"),0);
	setParameter(effect->GetTechniqueByName("gpu_transform")->passAsGLuint(0),"density_texture",0);
	setParameter(effect->GetTechniqueByName("gpu_transform")->passAsGLuint(0),"light_texture",1);
	setParameter(effect->GetTechniqueByName("gpu_transform")->passAsGLuint(0),"ambient_texture",2);

	float y_start=(float)start_texel/(float)total_texels;
	float y_end=(float)(start_texel+texels)/(float)total_texels;
	gpuCloudConstants.transformMatrix	=params.Matrix4x4DensityToLightTransform;
	SetGpuCloudConstants(gpuCloudConstants,params,y_start,y_end-y_start);

	gpuCloudConstants.zPixel			=1.f/(float)params.density_grid[2];
	gpuCloudConstants.zSize				=(float)params.density_grid[2];

	gpuCloudConstants.Apply(deviceContext);
	
	//effect->SetTexture(deviceContext,"density_texture",density_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D,density_texture->AsGLuint());
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D,directLightTextures[1].AsGLuint());
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D,directLightTextures[0].AsGLuint());
	// Instead of a loop, we do a single big render, by tiling the z layers in the y direction.
	{
		world_fb.Activate(deviceContext);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0,1.0,0,1.0,-1.0,1.0);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			// input light values:
			DrawQuad(0.f,y_start,1.f,y_end-y_start);
			GL_ERROR_CHECK
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			GL_ERROR_CHECK
			int YMAX=params.density_grid[1]*params.density_grid[2];
			int Y0=(int)(y_start*(float)YMAX);
			int Y1=(int)(y_end*(float)YMAX);
			if(target)
			{
				target+=Y0*params.density_grid[0]*4;
				glReadPixels(0,Y0,params.density_grid[0],Y1-Y0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,(GLvoid*)target);
			}
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D,finalTexture[cycled_index]->AsGLuint());
			CopyTo3DTexture(start_texel,texels,params.density_grid);
			GL_ERROR_CHECK
		world_fb.Deactivate(deviceContext);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			GL_ERROR_CHECK
		//target+=density_grid[0]*density_grid[1]*4;
	}
	effect->Unapply(deviceContext);
	
	
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}