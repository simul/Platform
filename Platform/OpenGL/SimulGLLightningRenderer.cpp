// Copyright (c) 2007-2014 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

#include <GL/glew.h>
#include "SimulGLLightningRenderer.h"
#include <math.h>

#include "Simul/Base/SmartPtr.h"
#include "Simul/Math/Pi.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#include "LoadGLImage.h"
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

using namespace simul;
using namespace clouds;
using namespace math;
using namespace sky;
using namespace opengl;

SimulGLLightningRenderer::SimulGLLightningRenderer(simul::clouds::CloudKeyframer *ck,simul::sky::BaseSkyInterface *sk) :
	simul::clouds::BaseLightningRenderer(ck,sk)
	,lightning_texture(0)
	,enable_geometry_shaders(false)
{
}

SimulGLLightningRenderer::~SimulGLLightningRenderer()
{
}

void SimulGLLightningRenderer::RestoreDeviceObjects()
{
	RecompileShaders();
	CreateLightningTexture();
}

void SimulGLLightningRenderer::RecompileShaders()
{
	const char* version = (const char*)glGetString(GL_VERSION); 
	double vers=atof(version);
	enable_geometry_shaders=(vers>=3.2);
	enable_geometry_shaders=glewIsSupported("GL_EXT_geometry_shader4")!=0;
enable_geometry_shaders=false;
	if(enable_geometry_shaders)
	{
		std::cout<<"Enabled geometry shaders for lightning."<<std::endl;
		lightning_program		=MakeProgramWithGS("simul_lightning");
		glow_program				=MakeProgram("simul_lightning.vert","simul_lightning.geom","simul_lightning_glow.frag");
	}
	else
		lightning_program		=MakeProgram("simul_simple_lightning");
	lightningTexture_param		=glGetUniformLocation(lightning_program,"lightningTexture");
	
	glUseProgram(NULL);
}

void SimulGLLightningRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(lightning_program);
}

static void glGetMatrix(GLfloat *m,GLenum src=GL_PROJECTION_MATRIX)
{
	glGetFloatv(src,m);
}

void GetCameraPosVector(simul::math::Vector3 &cam_pos,simul::math::Vector3 &cam_dir)
{
	simul::math::Matrix4x4 modelview;
	glGetMatrix(modelview.RowPointer(0),GL_MODELVIEW_MATRIX);
	simul::math::Matrix4x4 inv;
	modelview.Inverse(inv);
	cam_pos.x=inv(3,0);
	cam_pos.y=inv(3,1);
	cam_pos.z=inv(3,2);

	
	cam_dir.x=inv(2,0);
	cam_dir.y=inv(2,1);
	cam_dir.z=inv(2,2);
}
					static float ww=50.f;
					static float uu=9.f;

void SimulGLLightningRenderer::Render(crossplatform::DeviceContext &,crossplatform::Texture *,simul::sky::float4 ,crossplatform::Texture *)
{
}

bool SimulGLLightningRenderer::CreateLightningTexture()
{
	unsigned size=64;
	unsigned char *lightning_tex_data=new unsigned char[size*4];
	glGenTextures(1,&lightning_texture);
	
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_1D,lightning_texture);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_T,GL_REPEAT);

	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float s=linear*2.f;
		if(s>1.f)
			s=2.f-s;
		float u=s/0.2f;
		if(u>1.f)
			u=1.f;
		float level=u;
		float r=level;
		float g=level;
		float b=level;
		float a=level;
		if(r>1.f)
			r=1.f;
		if(g>1.f)
			g=1.f;
		if(b>1.f)
			b=1.f;
		if(a>1.f)
			a=1.f;
		lightning_tex_data[4*i+0]=(unsigned char)(255.f*r);
		lightning_tex_data[4*i+1]=(unsigned char)(255.f*g);
		lightning_tex_data[4*i+2]=(unsigned char)(255.f*b);
		lightning_tex_data[4*i+3]=(unsigned char)(255.f*a);
	}
	glTexImage1D(GL_TEXTURE_1D,0,GL_RGBA8,size,0,GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,0);
	glTexSubImage1D(
		GL_TEXTURE_1D,
		0,0,
		size,
		GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
		lightning_tex_data);
	GL_ERROR_CHECK
	delete [] lightning_tex_data;
	
	return true;
}
