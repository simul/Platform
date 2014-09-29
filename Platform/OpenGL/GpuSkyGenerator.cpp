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
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include <math.h>
using namespace simul;
using namespace opengl;

GpuSkyGenerator::GpuSkyGenerator()
{
	SetDirectTargets(NULL,NULL,NULL,NULL);
}

GpuSkyGenerator::~GpuSkyGenerator()
{
	InvalidateDeviceObjects();
}

void GpuSkyGenerator::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	BaseGpuSkyGenerator::RestoreDeviceObjects(r);
	RecompileShaders();
}

void GpuSkyGenerator::InvalidateDeviceObjects()
{
	BaseGpuSkyGenerator::InvalidateDeviceObjects();
GL_ERROR_CHECK
}

void GpuSkyGenerator::RecompileShaders()
{
	BaseGpuSkyGenerator::RecompileShaders();
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

void GpuSkyGenerator::MakeLossAndInscatterTextures(sky::float4 wavelengthsNm,int cycled_index,
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
	crossplatform::DeviceContext deviceContext;
	if(!effect)
		RecompileShaders();
//	float maxOutputAltKm=p.altitudes_km[p.altitudes_km.size()-1];
// we will render to these three textures, one distance at a time.
// The rendertextures are altitudes x elevations
	for(int i=0;i<2;i++)
	{
		fb[i].SetWidthAndHeight((int)p.altitudes_km.size(),p.numElevations);
		fb[i].InitColor_Tex(0,crossplatform::RGBA_32_FLOAT);
	}
	crossplatform::BaseFramebuffer *F[2];
	F[0]=&fb[0];
	F[1]=&fb[1];
	//
	
	if(dens_tex->width!=a.table_size||optd_tex->width!=a.table_size)
		tables_checksum=0;
	dens_tex->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,a.table_size,1,crossplatform::RGBA_32_FLOAT,false);
	optd_tex->ensureTexture2DSizeAndFormat(deviceContext.renderPlatform,a.table_size,a.table_size,crossplatform::RGBA_32_FLOAT,false,false);
	if(a.tables_checksum!=tables_checksum)
	{
		dens_tex->setTexels(deviceContext,(unsigned*)a.density_table,0,a.table_size);
		optd_tex->setTexels(deviceContext,(unsigned *)a.optical_table,0,a.table_size*a.table_size);
		tables_checksum=a.tables_checksum;
	}

	this->SetGpuSkyConstants(gpuSkyConstants,p,a,ir);
	for(int i=0;i<3;i++)
	{
		if(finalLoss[i])
			finalLoss[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true);
		if(finalInsc[i])
			finalInsc[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true);
		if(finalSkyl[i])
			finalSkyl[i]->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size(),p.numElevations,p.numDistances,crossplatform::RGBA_32_FLOAT,true);
	}
	if(light_table)
		light_table->ensureTexture3DSizeAndFormat(NULL,(int)p.altitudes_km.size()*32,3,4,crossplatform::RGBA_32_FLOAT,true);
	
	int xy_size		=(int)p.altitudes_km.size()*p.numElevations;
	
	int start_step	=(fadeTexIndex[cycled_index]*3)/xy_size;
	int end_step	=((p.fill_up_to_texels)*3+p.numDistances-1)/xy_size;

	int start_loss	=range(start_step			,0,p.numDistances);
	int end_loss	=range(end_step				,0,p.numDistances);
	int num_loss	=range(end_loss-start_loss	,0,p.numDistances);
	static float uu=0.5f;
	gpuSkyConstants.texCoordZ		=((float)start_loss-uu)/(float)p.numDistances;
	renderPlatform->SetStandardRenderState(deviceContext,crossplatform::STANDARD_OPAQUE_BLENDING);
	gpuSkyConstants.Apply(deviceContext);
	glDisable(GL_BLEND);
	if(num_loss>0)
	{
		GL_ERROR_CHECK
		float prevDistKm=pow((float)(std::max(start_loss-1,0))/((float)p.numDistances-1.f),2.f)*p.max_distance_km;
		// Copy layer to initial texture
		if(start_loss>0)
		{
			effect->Apply(deviceContext,effect->GetTechniqueByName("copy"),0);
			effect->SetTexture(deviceContext,"source_texture",finalLoss[cycled_index]);
			effect->SetParameter("tz",gpuSkyConstants.texCoordZ);
			F[0]->Activate(deviceContext);
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				renderPlatform->DrawQuad(deviceContext);
			F[0]->Deactivate(deviceContext);
			effect->UnbindTextures(deviceContext);
			effect->Unapply(deviceContext);
		}
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
			gpuSkyConstants.Apply(deviceContext);
		GL_ERROR_CHECK
			F[1]->Activate(deviceContext);
		GL_ERROR_CHECK
			if(i==0)
			{
				F[1]->Clear(NULL,1.f,1.f,1.f,1.f,1.f);
			}
			else
			{
				F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
				OrthoMatrices();
				effect->Apply(deviceContext,effect->GetTechniqueByName("simul_gpu_loss"),0);
				effect->SetTexture(deviceContext,"density_texture",dens_tex);
				effect->SetTexture(deviceContext,"input_loss_texture",F[0]->GetTexture()); 
				renderPlatform->DrawQuad(deviceContext);
				effect->UnbindTextures(deviceContext);
				effect->Unapply(deviceContext);
			}
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		GL_ERROR_CHECK
			if(finalLoss[cycled_index])
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalLoss[cycled_index]->AsGLuint());
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
			F[1]->Deactivate(deviceContext);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			prevDistKm=distKm;
		}
	}
	glUseProgram(0);
	// Now we will generate the inscatter texture.
	
	int start_insc	=range(start_step-p.numDistances	,0,p.numDistances);
	int end_insc	=range(end_step-p.numDistances		,0,p.numDistances);
	int num_insc	=range(end_insc-start_insc			,0,p.numDistances);
	
	// First we make the loss into a 3D texture.
	GLuint loss_t=finalLoss[cycled_index]->AsGLuint();
	if(num_insc>0)
	{		// Copy layer to initial texture
		if(start_insc>0)
		{
			effect->Apply(deviceContext,effect->GetTechniqueByName("copy"),0);
			effect->SetTexture(deviceContext,"source_texture",finalInsc[cycled_index]);
			gpuSkyConstants.texCoordZ		=((float)start_insc-uu)/(float)p.numDistances;
			gpuSkyConstants.Apply(deviceContext);
			F[0]->Activate(deviceContext);
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				renderPlatform->DrawQuad(deviceContext);
			F[0]->Deactivate(deviceContext);
			effect->UnbindTextures(deviceContext);
			effect->Unapply(deviceContext);
		}
		// Now render out the inscatter.
		gpuSkyConstants.Apply(deviceContext);
	GL_ERROR_CHECK
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
			gpuSkyConstants.Apply(deviceContext);
			F[1]->Activate(deviceContext);
		
			F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
			if(i>0)
			{
				OrthoMatrices();
				// input inscatter values:
				effect->Apply(deviceContext,effect->GetTechniqueByName("simul_gpu_insc"),0);
				effect->SetTexture(deviceContext,"input_insc_texture",F[0]->GetTexture());
				effect->SetTexture(deviceContext,"dens_texture",dens_tex);
				effect->SetTexture(deviceContext,"loss_texture",finalLoss[cycled_index]);
				effect->SetTexture(deviceContext,"optical_depth_texture",optd_tex); 
				renderPlatform->DrawQuad(deviceContext);
				effect->Unapply(deviceContext);
			}
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			if(finalInsc[cycled_index])
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalInsc[cycled_index]->AsGLuint());
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
			F[1]->Deactivate(deviceContext);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			prevDistKm=distKm;
		}
	}
	
	int start_skyl	=range(start_step	-2*p.numDistances	,0	,p.numDistances);
	int end_skyl	=range(end_step		-2*p.numDistances	,0	,p.numDistances);
	int num_skyl	=range(end_skyl-start_skyl				,0	,p.numDistances);
	
	GLuint insc_tex=finalInsc[cycled_index]->AsGLuint();
	if(num_skyl>0)
	{
		// Copy layer to initial texture
		if(start_skyl>0)
		{
			effect->Apply(deviceContext,effect->GetTechniqueByName("copy"),0);
			effect->SetTexture(deviceContext,"source_texture",finalSkyl[cycled_index]);
			gpuSkyConstants.texCoordZ		=((float)start_skyl-uu)/(float)p.numDistances;
			gpuSkyConstants.Apply(deviceContext);
			effect->SetParameter("tz",gpuSkyConstants.texCoordZ);
			F[0]->Activate(deviceContext);
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
				//glTexParameteri( GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
				renderPlatform->DrawQuad(deviceContext);
			F[0]->Deactivate(deviceContext);
			effect->UnbindTextures(deviceContext);
			effect->Unapply(deviceContext);
		}
		// Finally we will generate the skylight texture.
		// First we make the inscatter into a 3D texture.
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_3D,insc_tex);
		// Now render out the skylight.
		gpuSkyConstants.Apply(deviceContext);
	GL_ERROR_CHECK
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
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
			gpuSkyConstants.Apply(deviceContext);
			F[1]->Activate(deviceContext);
			F[1]->Clear(NULL,0.f,0.f,0.f,0.f,1.f);
			if(i>0)
			{
				effect->Apply(deviceContext,effect->GetTechniqueByName("simul_gpu_skyl"),0);
				effect->SetTexture(deviceContext,"input_skyl_texture",F[0]->GetTexture());
				effect->SetTexture(deviceContext,"density_texture",dens_tex);
				effect->SetTexture(deviceContext,"loss_texture",finalLoss[cycled_index]);
				effect->SetTexture(deviceContext,"insc_texture",finalInsc[cycled_index]);
				OrthoMatrices(); 
				renderPlatform->DrawQuad(deviceContext);
				effect->UnbindTextures(deviceContext);
				effect->Unapply(deviceContext);
			}
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		GL_ERROR_CHECK
			if(finalSkyl[cycled_index])
			{
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_3D,finalSkyl[cycled_index]->AsGLuint());
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
			F[1]->Deactivate(deviceContext);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			std::swap(F[0],F[1]);
			prevDistKm=distKm;
		}
	}
	glUseProgram(0);
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
//	
	fadeTexIndex[cycled_index]=p.fill_up_to_texels;
}