#include <GL/glew.h>
#include <stdint.h> // for uintptr_t
#include <stdint.h> // for uintptr_t
#include "Simul/Platform/OpenGL/GpuSkyGenerator.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimulGLUtilities.h"
#include "Simul/Sky/SkyInterface.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Platform/OpenGL/GLSL/CppGlsl.hs"
#include "Simul/Platform/CrossPlatform/SL/simul_gpu_sky.sl"
#include "Simul/Base/Timer.h"
#include <math.h>
using namespace simul;
using namespace opengl;

GpuSkyGenerator::GpuSkyGenerator()
	:loss_program(0)
	,insc_program(0)
	,skyl_program(0)
	,copy_program(0)
	,loss_cache(NULL)
	,insc_cache(NULL)
	,skyl_cache(NULL)
{
	SetDirectTargets(NULL,NULL,NULL,NULL);
}

GpuSkyGenerator::~GpuSkyGenerator()
{
	InvalidateDeviceObjects();
	delete [] loss_cache;
	delete [] insc_cache;
	delete [] skyl_cache;
}

void GpuSkyGenerator::RestoreDeviceObjects(void *)
{
	gpuSkyConstants.RestoreDeviceObjects();
	RecompileShaders();
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
GL_ERROR_CHECK
	SAFE_DELETE_PROGRAM(copy_program);
	SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(skyl_program);
GL_ERROR_CHECK
	gpuSkyConstants.Release();
GL_ERROR_CHECK
}

void GpuSkyGenerator::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(copy_program);
	SAFE_DELETE_PROGRAM(loss_program);
	SAFE_DELETE_PROGRAM(insc_program);
	SAFE_DELETE_PROGRAM(skyl_program);
	loss_program=MakeProgram("simple.vert",NULL,"simul_gpu_loss.frag");
GL_ERROR_CHECK
	std::map<std::string,std::string> defines;

	insc_program=MakeProgram("simple.vert",NULL,"simul_gpu_insc.frag",defines);
	skyl_program=MakeProgram("simple.vert",NULL,"simul_gpu_skyl.frag");
	copy_program=MakeProgram("simple.vert",NULL,"simul_gpu_sky_copy.frag");
	gpuSkyConstants.LinkToProgram(loss_program,"GpuSkyConstants",1);
	gpuSkyConstants.LinkToProgram(insc_program,"GpuSkyConstants",1);
	gpuSkyConstants.LinkToProgram(skyl_program,"GpuSkyConstants",1);
	gpuSkyConstants.LinkToProgram(copy_program,"GpuSkyConstants",1);
GL_ERROR_CHECK
}

//! Return true if the derived class can make sky tables using the GPU.
bool GpuSkyGenerator::CanPerformGPUGeneration() const
{
	return Enabled;
}

static int range(int x,int start,int end)
{
	if(x<start)
		x=start;
	if(x>end)
		x=end;
	return x;
}

void GpuSkyGenerator::MakeLossAndInscatterTextures(int cycled_index,
				simul::sky::AtmosphericScatteringInterface *
				,const simul::sky::GpuSkyParameters &p
				,const simul::sky::GpuSkyAtmosphereParameters &a
				,const simul::sky::GpuSkyInfraredParameters &ir)
{
	if(p.fill_up_to_texels<0)
		return;
	if(keyframe_checksums[cycled_index]!=p.keyframe_checksum)
	{
		keyframe_checksums[cycled_index]	=p.keyframe_checksum;
		fadeTexIndex[cycled_index]=0;
	}
	if(fadeTexIndex[cycled_index]>=p.fill_up_to_texels)
		return;
	if(loss_program<=0)
		RecompileShaders();
//	float maxOutputAltKm=p.altitudes_km[p.altitudes_km.size()-1];
// we will render to these three textures, one distance at a time.
// The rendertextures are altitudes x elevations
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight((int)p.altitudes_km.size(),p.numElevations);
		fb[i].InitColor_Tex(0,GL_RGBA32F_ARB);
	}
	int new_cache_size=(int)p.altitudes_km.size()*p.numElevations*p.numDistances;
	if(new_cache_size>cache_size)
	{
		delete [] loss_cache;
		delete [] insc_cache;
		delete [] skyl_cache;
		cache_size=new_cache_size;
		loss_cache=new simul::sky::float4[cache_size];
		insc_cache=new simul::sky::float4[cache_size];
		skyl_cache=new simul::sky::float4[cache_size];
	}
	crossplatform::BaseFramebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	//glEnable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	GLuint dens_tex=make2DTexture(a.table_size,1,(const float *)a.density_table);
	this->SetGpuSkyConstants(gpuSkyConstants,p,a,ir);
	for(int i=0;i<3;i++)
	{
		if(finalLoss[i])
			finalLoss[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,GL_RGBA32F,true);
		if(finalInsc[i])
			finalInsc[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,GL_RGBA32F,true);
		if(finalSkyl[i])
			finalSkyl[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,GL_RGBA32F,true);
	}
	if(light_table)
		light_table->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size()*32,3,4,GL_RGBA32F,true);
	
	int xy_size		=(int)p.altitudes_km.size()*p.numElevations;
	
	int start_step	=(fadeTexIndex[cycled_index]*3)/xy_size;
	int end_step	=((p.fill_up_to_texels)*3+p.numDistances-1)/xy_size;

	int start_loss	=range(start_step			,0,p.numDistances);
	int end_loss	=range(end_step				,0,p.numDistances);
	int num_loss	=range(end_loss-start_loss	,0,p.numDistances);
	static float uu=0.5f;
	gpuSkyConstants.texCoordZ		=((float)start_loss-uu)/(float)p.numDistances;
	
	gpuSkyConstants.Apply();
	glDisable(GL_BLEND);
	if(num_loss>0)
	{
		simul::sky::float4 *target=loss_cache;

		GL_ERROR_CHECK
		float prevDistKm=pow((float)(std::max(start_loss-1,0))/((float)p.numDistances-1.f),2.f)*p.max_distance_km;
		// Copy layer to initial texture
		if(start_loss>0)
		{
			glUseProgram(copy_program);
			setParameter(copy_program,"source_texture",0);
			setParameter(copy_program,"tz",gpuSkyConstants.texCoordZ);
			F[0]->Activate(NULL);
				// input light values:
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalLoss[cycled_index]->tex);
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				DrawQuad(0,0,1,1);
			F[0]->Deactivate(NULL);
		}
		glUseProgram(loss_program);
		setParameter(loss_program,"input_loss_texture",0);
		setParameter(loss_program,"density_texture",1);
		for(int i=start_loss;i<end_loss;i++)
		{
		// The midpoint of the step represented by this layer
			float zPosition=pow((float)(i)/((float)p.numDistances-1.f),2.f);
			float distKm=zPosition*p.max_distance_km;
			if(i==p.numDistances-1)
				distKm=1000.f;
			gpuSkyConstants.distanceKm		=distKm;
			gpuSkyConstants.prevDistanceKm	=prevDistKm;
	gpuSkyConstants.texCoordZ		=((float)i)/(float)p.numDistances;
			gpuSkyConstants.Apply();
		glUseProgram(loss_program);
		GL_ERROR_CHECK
			F[1]->Activate(NULL);
		GL_ERROR_CHECK
				if(i==0)
				{
					F[1]->Clear(NULL,1.f,1.f,1.f,1.f,1.f);
				}
				else
				{
					F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
					OrthoMatrices();
					// input light values:
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D,dens_tex);
					DrawQuad(0,0,1,1);
				}
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	//std::cout<<"\tGpu sky: render loss"<<i<<" "<<timer.UpdateTime()<<std::endl;
		GL_ERROR_CHECK
				if(target)
					glReadPixels(0,0,(GLsizei)p.altitudes_km.size(),p.numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
				if(finalLoss[cycled_index])
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_3D,finalLoss[cycled_index]->tex);
					glCopyTexSubImage3D(GL_TEXTURE_3D
 									,0
 									,0
 									,0
 									,i
 									,0
 									,0
 									,(GLsizei)p.altitudes_km.size()
									,p.numElevations);
				}
	//std::cout<<"\tGpu sky: loss read"<<i<<" "<<timer.UpdateTime()<<std::endl;
			F[1]->Deactivate(NULL);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			if(target)
				target+=p.altitudes_km.size()*p.numElevations;
			prevDistKm=distKm;
		}
	}
	glUseProgram(0);
	// Now we will generate the inscatter texture.
	GLuint optd_tex=make2DTexture(a.table_size,a.table_size,(const float *)a.optical_table);
	
	int start_insc	=range(start_step-p.numDistances	,0,p.numDistances);
	int end_insc	=range(end_step-p.numDistances		,0,p.numDistances);
	int num_insc	=range(end_insc-start_insc			,0,p.numDistances);
	
	// First we make the loss into a 3D texture.
	GLuint loss_tex=finalLoss[cycled_index]->tex;
	if(num_insc>0)
	{		// Copy layer to initial texture
		if(start_insc>0)
		{
			glUseProgram(copy_program);
			setParameter(copy_program,"source_texture",0);
			gpuSkyConstants.texCoordZ		=((float)start_insc-uu)/(float)p.numDistances;
			gpuSkyConstants.Apply();
			setParameter(copy_program,"tz",gpuSkyConstants.texCoordZ);
			F[0]->Activate(NULL);
				// input light values:
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalInsc[cycled_index]->tex);
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				DrawQuad(0,0,1,1);
			F[0]->Deactivate(NULL);
		}
		// Now render out the inscatter.
		glUseProgram(insc_program);
		gpuSkyConstants.Apply();
		setParameter(insc_program,"input_insc_texture",0);
		setParameter(insc_program,"density_texture",1);
		setParameter(insc_program,"loss_texture",2);
		setParameter(insc_program,"optical_depth_texture",3);
	GL_ERROR_CHECK
		simul::sky::float4 *target=insc_cache;
		if(target)
			target+=start_insc*p.altitudes_km.size()*p.numElevations;
		float prevDistKm=pow((float)(std::max(start_insc-1,0))/((float)p.numDistances-1.f),2.f)*p.max_distance_km;
		for(int i=start_insc;i<end_insc;i++)
		{
		// The midpoint of the step represented by this layer
			float zPosition=pow((float)(i)/((float)p.numDistances-1.f),2.f);
			float distKm=zPosition*p.max_distance_km;
			if(i==p.numDistances-1)
				distKm=1000.f;
			gpuSkyConstants.distanceKm		=distKm;
			gpuSkyConstants.prevDistanceKm	=prevDistKm;
			gpuSkyConstants.Apply();
			F[1]->Activate(NULL);
				F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
				if(i>0)
				{
					OrthoMatrices();
					// input inscatter values:
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D,dens_tex);
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_3D,loss_tex);
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D,optd_tex);
					DrawQuad(0,0,1,1);
				}
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
				if(target)
					glReadPixels(0,0,(GLsizei)p.altitudes_km.size(),p.numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
				if(finalInsc[cycled_index])
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_3D,finalInsc[cycled_index]->tex);
					glCopyTexSubImage3D(GL_TEXTURE_3D
 									,0
 									,0
 									,0
 									,i
 									,0
 									,0
 									,(GLsizei)p.altitudes_km.size()
									,p.numElevations);
		GL_ERROR_CHECK
				}
			F[1]->Deactivate(NULL);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			if(target)
				target+=p.altitudes_km.size()*p.numElevations;
			prevDistKm=distKm;
		}
	}
	glUseProgram(0);
	
	int start_skyl	=range(start_step	-2*p.numDistances	,0	,p.numDistances);
	int end_skyl	=range(end_step		-2*p.numDistances	,0	,p.numDistances);
	int num_skyl	=range(end_skyl-start_skyl				,0	,p.numDistances);
	
	GLuint insc_tex=finalInsc[cycled_index]->tex;
	if(num_skyl>0)
	{
		// Copy layer to initial texture
		if(start_skyl>0)
		{
			glUseProgram(copy_program);
			setParameter(copy_program,"source_texture",0);
			gpuSkyConstants.texCoordZ		=((float)start_skyl-uu)/(float)p.numDistances;
			gpuSkyConstants.Apply();
			setParameter(copy_program,"tz",gpuSkyConstants.texCoordZ);
			F[0]->Activate(NULL);
				// input light values:
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalSkyl[cycled_index]->tex);
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				DrawQuad(0,0,1,1);
			F[0]->Deactivate(NULL);
		}
		// Finally we will generate the skylight texture.
		// First we make the inscatter into a 3D texture.
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_3D,insc_tex);
		// Now render out the skylight.
		glUseProgram(skyl_program);
		gpuSkyConstants.Apply();
		setParameter(skyl_program,"input_skyl_texture",0);
		setParameter(skyl_program,"density_texture",1);
		setParameter(skyl_program,"loss_texture",2);
	//	setParameter(skyl_program,"optical_depth_texture",3);
		setParameter(skyl_program,"insc_texture",4);
	GL_ERROR_CHECK
		simul::sky::float4 *target=skyl_cache;
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		if(target)
			target+=p.altitudes_km.size()*p.numElevations*start_skyl;
		float prevDistKm=pow((float)(std::max(start_skyl-1,0))/((float)p.numDistances-1.f),2.f)*p.max_distance_km;
		for(int i=start_skyl;i<end_skyl;i++)
		{
		// The midpoint of the step represented by this layer
			float zPosition=pow((float)(i)/((float)p.numDistances-1.f),2.f);
			float distKm=zPosition*p.max_distance_km;
			if(i==p.numDistances-1)
				distKm=1000.f;
			gpuSkyConstants.distanceKm		=distKm;
			gpuSkyConstants.prevDistanceKm	=prevDistKm;
			gpuSkyConstants.Apply();
			F[1]->Activate(NULL);
				F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
				if(i>0)
				{
					OrthoMatrices();
					// input inscatter values:
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D,(GLuint)F[0]->GetColorTex());
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D,dens_tex);
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_3D,loss_tex);
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D,optd_tex);
					DrawQuad(0,0,1,1);
				}
				glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		GL_ERROR_CHECK
				if(target)
					glReadPixels(0,0,(GLsizei)p.altitudes_km.size(),p.numElevations,GL_RGBA,GL_FLOAT,(GLvoid*)target);
				if(finalSkyl[cycled_index])
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_3D,finalSkyl[cycled_index]->tex);
					glCopyTexSubImage3D(GL_TEXTURE_3D
 									,0
 									,0
 									,0
 									,i
 									,0
 									,0
 									,(GLsizei)p.altitudes_km.size()
									,p.numElevations);
		GL_ERROR_CHECK
				}
			F[1]->Deactivate(NULL);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			if(target)
				target+=p.altitudes_km.size()*p.numElevations;
			prevDistKm=distKm;
		}
	}
	glUseProgram(0);
	SAFE_DELETE_TEXTURE(dens_tex);
	SAFE_DELETE_TEXTURE(optd_tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D,0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_3D,0);
//	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	fadeTexIndex[cycled_index]=p.fill_up_to_texels;
}
void GpuSkyGenerator::CopyToMemory(int /*cycled_index*/,simul::sky::float4 *loss,simul::sky::float4 *insc,simul::sky::float4 *skyl)
{
	memcpy(loss,loss_cache,cache_size*sizeof(simul::sky::float4));
	memcpy(insc,insc_cache,cache_size*sizeof(simul::sky::float4));
	memcpy(skyl,skyl_cache,cache_size*sizeof(simul::sky::float4));
}
