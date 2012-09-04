// Copyright (c) 2007-2011 Simul Software Ltd
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
#include "LoadGLImage.h"
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"

using namespace simul::clouds;
extern void printProgramInfoLog(GLuint obj);

SimulGLLightningRenderer::SimulGLLightningRenderer(simul::clouds::LightningRenderInterface *lightningRenderInterface) :
	simul::clouds::BaseLightningRenderer(lightningRenderInterface)
	,lightning_vertices(NULL)
	,lightning_texture(0)
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
	lightning_program			=glCreateProgram();
	lightning_vertex_shader		=glCreateShader(GL_VERTEX_SHADER);
	lightning_fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
    lightning_vertex_shader		=LoadProgram(lightning_vertex_shader,"simul_lightning.vert");
    lightning_fragment_shader	=LoadProgram(lightning_fragment_shader,"simul_lightning.frag");
	glAttachShader(lightning_program,lightning_vertex_shader);
	glAttachShader(lightning_program,lightning_fragment_shader);
	glLinkProgram(lightning_program);
	glUseProgram(lightning_program);
	printProgramInfoLog(lightning_program);
	lightningTexture_param		=glGetUniformLocation(lightning_program,"lightningTexture");
	
	printProgramInfoLog(lightning_program);
	glUseProgram(NULL);
}

void SimulGLLightningRenderer::InvalidateDeviceObjects()
{
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

bool SimulGLLightningRenderer::Render()
{
	if(glStringMarkerGREMEDY)
		glStringMarkerGREMEDY(0,"SimulGLLightningRenderer::Render");
	if(!lightning_vertices)
		lightning_vertices=new PosTexVert_t[4500];
ERROR_CHECK
/*	m_pd3dDevice->SetTexture(0,lightning_texture);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 1);
		*/
	//set up matrices
/*	D3DXMATRIX wvp;
	D3DXMatrixIdentity(&world);
	m_pd3dDevice->GetTransform(D3DTS_VIEW,&view);
	m_pd3dDevice->GetTransform(D3DTS_WORLD,&world);
	m_pd3dDevice->GetTransform(D3DTS_PROJECTION,&proj);

	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	D3DXVECTOR4 cam_pos=GetCameraPosVector(view);
*/
ERROR_CHECK
	simul::math::Vector3 view_dir,cam_pos;
	GetCameraPosVector(cam_pos,view_dir);
ERROR_CHECK
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_3D);
	glEnable(GL_TEXTURE_1D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D,lightning_texture);
	glUseProgram(lightning_program);
	glUniform1i(lightningTexture_param,0);

ERROR_CHECK
	simul::math::Vector3 pos;

	static float lm=10.f;
	static float main_bright=1.f;
	int vert_start=0;
	int vert_num=0;

	for(unsigned i=0;i<lightningRenderInterface->GetNumLightSources();i++)
	{
		if(!lightningRenderInterface->IsSourceStarted(i))
			continue;
		bool quads=true;
		simul::math::Vector3 x1,x2;
		float bright1=0.f;
		lightningRenderInterface->GetSegmentVertex(i,0,0,bright1,x1.FloatPointer(0));
		float vertical_shift=0;//helper->GetVerticalShiftDueToCurvature(dist,x1.z);
		for(unsigned jj=0;jj<lightningRenderInterface->GetNumBranches(i);jj++)
		{
			if(jj==1)
			{
				/*hr=m_pLightningEffect->EndPass();
				hr=m_pLightningEffect->End();
				
				m_pLightningEffect->SetTechnique(m_hTechniqueLightningLines);*/
				quads=false;

				/*hr=m_pLightningEffect->Begin(&passes,0);
				hr=m_pLightningEffect->BeginPass(0);*/
			}
			simul::math::Vector3 last_transverse;
			vert_start=vert_num;
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE,GL_ONE);
			if(quads)
			{
				glBegin(GL_QUAD_STRIP);
			}
			else
				glBegin(GL_LINE_STRIP);
			for(unsigned k=0;k<lightningRenderInterface->GetNumSegments(i,jj)&&vert_num<4500;k++)
			{
				lightningRenderInterface->GetSegmentVertex(i,jj,k,bright1,x1.FloatPointer(0));
				simul::math::Vector3 dir;
				lightningRenderInterface->GetSegmentVertexDirection(i,jj,k,dir.FloatPointer(0));

				static float ww=100.f;
				float width=lightningRenderInterface->GetBranchWidth(i,jj);
				float width1=bright1*width;
				if(quads)
					width1*=ww;
				simul::math::Vector3 transverse;
				view_dir=x1-cam_pos;
				CrossProduct(transverse,view_dir,dir);
				transverse.Unit();
				transverse*=width1;
				simul::math::Vector3 t=transverse;
				if(k)
					t=0.5f*(last_transverse+transverse);
				simul::math::Vector3 x1a=x1;//-t;
				if(quads)
					x1a=x1-t;
				simul::math::Vector3 x1b=x1+t;
				//if(!k)
				//	bright1=0;
				//if(k==lightningRenderInterface->GetNumSegments(i,jj)-1)
				//	bright1=0;
				PosTexVert_t &v1=lightning_vertices[vert_num++];

				v1.position.x=x1a.x;
				v1.position.y=x1a.y+vertical_shift;
				v1.position.z=x1a.z;
				v1.texCoords.x=0;
				v1.texCoords.y=bright1;
				if(!quads)
					v1.texCoords.x=0.5f;
					
				glMultiTexCoord2f(GL_TEXTURE0,v1.texCoords.x,v1.texCoords.y);
				glVertex3f(v1.position.x,v1.position.y,v1.position.z);
					

				if(quads)
				{
					PosTexVert_t &v2=lightning_vertices[vert_num++];
					v2.position.x=x1b.x;
					v2.position.y=x1b.y+vertical_shift;
					v2.position.z=x1b.z;
					v2.texCoords.x=1.f;
					v2.texCoords.y=bright1;
					glMultiTexCoord2f(GL_TEXTURE0,v2.texCoords.x,v2.texCoords.y);
					glVertex3f(v2.position.x,v2.position.y,v2.position.z);
				}
				last_transverse=transverse;
			}
			glEnd();
			ERROR_CHECK
		}
	}
	glDisable(GL_TEXTURE_1D);
	ERROR_CHECK
	return true;
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


	const float *lightning_colour=lightningRenderInterface->GetLightningColour();
	for(unsigned i=0;i<size;i++)
	{
		float linear=1.f-fabs((float)(i+.5f)*2.f/(float)size-1.f);
		float level=.5f*linear*linear+5.f*(linear>.97f);
		float r=lightning_colour[0]*level;
		float g=lightning_colour[1]*level;
		float b=lightning_colour[2]*level;
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
	ERROR_CHECK
	delete [] lightning_tex_data;
	glDisable(GL_TEXTURE_1D);
	return true;
}
