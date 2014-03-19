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
	}
	else
		lightning_program		=MakeProgram("simul_simple_lightning");
	glow_program				=MakeProgram("simul_lightning.vert","simul_lightning.geom","simul_lightning_glow.frag");
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

void SimulGLLightningRenderer::Render(void*,const simul::math::Matrix4x4 &v,const simul::math::Matrix4x4 &p,const void *depth_tex,simul::sky::float4 depthViewportXYWH,const void *cloud_depth_tex)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(0,"SimulGLLightningRenderer::Render");
GL_ERROR_CHECK
	simul::math::Vector3 view_dir,cam_pos;
	GetCameraPosVector(cam_pos,view_dir);
GL_ERROR_CHECK
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_1D);
	glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D,lightning_texture);
	glUseProgram(lightning_program);
	glUniform1i(lightningTexture_param,0);
	simul::math::Matrix4x4 view,proj,viewproj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Multiply4x4(viewproj,view,proj);
	int main_viewport[4];
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	float vpx=(float)main_viewport[2];
	float vpy=(float)main_viewport[3];
	setMatrix(lightning_program		,"worldViewProj"	,viewproj.RowPointer(0));
	if(enable_geometry_shaders)
		setParameter(lightning_program	,"viewportPixels"	,vpx,vpy);
	int texcoord_index=glGetAttribLocation(lightning_program,"in_coord");
GL_ERROR_CHECK
	simul::math::Vector3 pos;
	static float lm=10.f;
	static float main_bright=1.f;
	static int cc=8;
	float time=baseSkyInterface->GetTime();
	glEnable(GL_LINE_SMOOTH);
	const simul::clouds::CloudKeyframer::Keyframe &K=cloudKeyframer->GetInterpolatedKeyframe();
	for(int i=0;i<cloudKeyframer->GetNumLightningBolts(time);i++)
	{
		const simul::clouds::LightningRenderInterface *lightningRenderInterface=cloudKeyframer->GetLightningBolt(time,i);
		if(!lightningRenderInterface)
			continue;
		simul::clouds::LightningProperties props=cloudKeyframer->GetLightningProperties(time,i);
		props.seed=0;
		setParameter(lightning_program,"lightningColour",props.colour);
		simul::sky::float4 x1,x2;
		static float maxwidth	=8.f;
		float vertical_shift	=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(int j=0;j<props.numLevels;j++)
		{
			for(int jj=0;jj<props.branchCount;jj++)
			{
				const simul::clouds::LightningRenderInterface::Branch &branch
					=lightningRenderInterface->GetBranch(props,time,j,jj);
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE,GL_ONE);
				glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ONE);
				float dist=0.001f*(cam_pos-simul::math::Vector3(branch.vertices[0])).Magnitude();
				glLineWidth(maxwidth*branch.width/dist);
				if(enable_geometry_shaders)
					glBegin(GL_LINE_STRIP_ADJACENCY);
				else
					glBegin(GL_LINE_STRIP);
				for(int k=-1;k<branch.numVertices;k++)
				{
					bool start=(k<0);
					if(start)
						x1=(const float *)branch.vertices[k+2];
					else
						x1=(const float *)branch.vertices[k];
					if(k+1<branch.numVertices)
						x2=(const float *)branch.vertices[k+1];
					else
						x2=2.f*x1-simul::sky::float4((const float *)branch.vertices[k-1]);
					bool end=(k==branch.numVertices-1);
					if(k<0)
					{
						simul::sky::float4 dir=x2-x1;
						x1=x2+dir;
					}
					float width=branch.brightness*branch.width;
					width*=ww;

					float brightness=branch.brightness;
					float dh=x1.z/1000.f-K.cloud_base_km;
					if(dh>0)
					{
						static float cc=0.1f;
						float d=exp(-dh/cc);
						brightness*=d;
					}
					if(end)
						brightness=0.f;
					glVertexAttrib3f(texcoord_index,width,x1.w,brightness);
					glVertex4f(x1.x,x1.y,x1.z+vertical_shift,x1.w);
				}
				glEnd();
				int err=glGetError();
				if(err!=0)
				{
					glDisable(GL_TEXTURE_1D);
					glPopAttrib();
					return;
				}
			}
		}
	}
	glDisable(GL_TEXTURE_1D);
	GL_ERROR_CHECK
	glPopAttrib();
}

bool SimulGLLightningRenderer::CreateLightningTexture()
{
	unsigned size=64;
	unsigned char *lightning_tex_data=new unsigned char[size*4];
	glGenTextures(1,&lightning_texture);
	glEnable(GL_TEXTURE_1D);
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
	glDisable(GL_TEXTURE_1D);
	return true;
}
