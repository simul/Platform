#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Math/Vector3.h"

SimulGLTerrainRenderer::SimulGLTerrainRenderer()
	:program(0)
	,max_fade_distance_metres(200000.f)
{
}

SimulGLTerrainRenderer::~SimulGLTerrainRenderer()
{
}

void SimulGLTerrainRenderer::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(program);
	program							=MakeProgram("simul_terrain");
	ERROR_CHECK
	glUseProgram(program);
	ERROR_CHECK
	printProgramInfoLog(program);
	ERROR_CHECK
	eyePosition_param				= glGetUniformLocation(program,"eyePosition");
	maxFadeDistanceMetres_param		= glGetUniformLocation(program,"maxFadeDistanceMetres");
	textures_param					= glGetUniformLocation(program,"textures");
	printProgramInfoLog(program);
	ERROR_CHECK
}

void SimulGLTerrainRenderer::RestoreDeviceObjects(void*)
{
	RecompileShaders();
	MakeTextures();
}

void SimulGLTerrainRenderer::MakeTextures()
{
    glGenTextures(1, &texArray);
	if(!IsExtensionSupported("GL_EXT_texture_array"))
		return;
    glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, texArray);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //glTexParameterfv(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_BORDER_COLOR, borderColor);
	unsigned bpp,width,height;
	unsigned char *data=LoadGLBitmap("terrain.png",bpp,width,height);
	unsigned char *moss=LoadGLBitmap("moss.png",bpp,width,height);
	int num_layers=2;
	int num_mips=8;
	int m=1;
	for(int i=0;i<num_mips;i++)
	{
		glTexImage3D	(GL_TEXTURE_2D_ARRAY_EXT, i,GL_RGBA	,width/m,height/m,num_layers,0	,(bpp==24)?GL_RGB:GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if(!i)
		{
			glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,0,0,0,0,width,height,1,(bpp==24)?GL_RGB:GL_RGBA,GL_UNSIGNED_BYTE,data);
			glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,0,0,0,1,width,height,1,(bpp==24)?GL_RGB:GL_RGBA,GL_UNSIGNED_BYTE,moss);
		}
		m*=2;
	}
	//glTexImage3D	(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA, width/2, height/2, num_layers, ...);
	//glTexImage3D	(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA, width/4, height/4, num_layers, ...);

	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	ERROR_CHECK
}

void SimulGLTerrainRenderer::InvalidateDeviceObjects()
{
	SAFE_DELETE_PROGRAM(program);
	SAFE_DELETE_TEXTURE(texArray);
}

void SimulGLTerrainRenderer::SetMaxFadeDistanceKm(float dist_km)
{
	max_fade_distance_metres=dist_km*1000.f;
}

void SimulGLTerrainRenderer::Render()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	ERROR_CHECK
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT,GL_FILL);
	glPolygonMode(GL_BACK,GL_LINE);
/*	glUseProgram(0);
	ERROR_CHECK
	glDepthFunc(GL_LESS);
	ERROR_CHECK
	glFrontFace(GL_CCW);
	ERROR_CHECK
	ERROR_CHECK
	ERROR_CHECK
	ERROR_CHECK
	glCullFace(GL_BACK);
	ERROR_CHECK*/
	glUseProgram(program);
	ERROR_CHECK
	simul::math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);
	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
	glUniform1f(maxFadeDistanceMetres_param,max_fade_distance_metres);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, texArray);
	glUniform1i(textures_param,0); 

	ERROR_CHECK
	int h=heightMapInterface->GetPageSize();
	glBegin(GL_TRIANGLE_STRIP);
	for(int i=0;i<h-1;i++)
	{
		float x1=(i-h/2)*1000.f;
		float x2=(i-h/2+1)*1000.f;
		for(int j=0;j<h-1;j++)
		{
			int J=j;
			if(i%2)
				J=(h-2-j);
			float y=(J-h/2)*1000.f;
			float z1=heightMapInterface->GetHeightAt(i,J);
			float z2=heightMapInterface->GetHeightAt(i+1,J);
			simul::math::Vector3 X1(x1,y,z1);
			simul::math::Vector3 X2(x2,y,z2);
			if(i%2)
			{
				glVertex3f(X2.x,X2.y,X2.z);
				glVertex3f(X1.x,X1.y,X1.z);
			}
			else
			{
				glVertex3f(X1.x,X1.y,X1.z);
				glVertex3f(X2.x,X2.y,X2.z);
			}
		}
	}
	glEnd();
	glUseProgram(0);
ERROR_CHECK
	glPopAttrib();
ERROR_CHECK
}