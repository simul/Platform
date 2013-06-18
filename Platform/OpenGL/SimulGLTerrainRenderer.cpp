#include <GL/glew.h>
#include "Simul/Platform/OpenGL/SimulGLTerrainRenderer.h"
#include "Simul/Platform/OpenGL/LoadGLProgram.h"
#include "Simul/Platform/OpenGL/SimuLGLUtilities.h"
#include "Simul/Platform/OpenGL/LoadGLImage.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Sky/SkyInterface.h"

SimulGLTerrainRenderer::SimulGLTerrainRenderer()
	:program(0)
{
}

SimulGLTerrainRenderer::~SimulGLTerrainRenderer()
{
}

void SimulGLTerrainRenderer::RecompileShaders()
{
	SAFE_DELETE_PROGRAM(program);
	program							=MakeProgram("simul_terrain");//WithGS
	ERROR_CHECK
	glUseProgram(program);
	ERROR_CHECK
	printProgramInfoLog(program);
	ERROR_CHECK
	eyePosition_param				= glGetUniformLocation(program,"eyePosition");
	textures_param					= glGetUniformLocation(program,"textures");
	worldViewProj_param				= glGetUniformLocation(program,"worldViewProj");
	lightDir_param					= glGetUniformLocation(program,"lightDir");
	sunlight_param					= glGetUniformLocation(program,"sunlight");
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
	{
		simul::base::RuntimeError("SimulGLTerrainRenderer needs the GL_EXT_texture_array extension.");
		return;
	}
	//if(!IsExtensionSupported("GL_EXT_gpu_shader4"))
	//	return;
	//GL_TEXTURE_2D_ARRAY_EXT
    glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, texArray);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //glTexParameterfv(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_BORDER_COLOR, borderColor);
	unsigned bpp,width,height;
	ERROR_CHECK
	unsigned char *data=LoadGLBitmap("terrain.png",bpp,width,height);
	unsigned char *moss=LoadGLBitmap("moss.png",bpp,width,height);
	ERROR_CHECK
	int num_layers=2;
	int num_mips=8;
	int m=1;
	for(int i=0;i<num_mips;i++)
	{
		glTexImage3D(GL_TEXTURE_2D_ARRAY_EXT, i,GL_RGBA	,width/m,height/m,num_layers,0,(bpp==24)?GL_RGB:GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if(i==0)
		{
			glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,i,0,0,0,width/m,height/m,1,(bpp==24)?GL_RGB:GL_RGBA,GL_UNSIGNED_BYTE,data);
			glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,i,0,0,1,width/m,height/m,1,(bpp==24)?GL_RGB:GL_RGBA,GL_UNSIGNED_BYTE,moss);
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

void SimulGLTerrainRenderer::Render(void *context)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	ERROR_CHECK
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(ReverseDepth?GL_GEQUAL:GL_LEQUAL);
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT,GL_FILL);
	glPolygonMode(GL_BACK,GL_LINE);
	glUseProgram(program);
	ERROR_CHECK
	simul::math::Vector3 cam_pos;
	CalcCameraPosition(cam_pos);

	simul::math::Matrix4x4 view,proj,viewproj;
	glGetFloatv(GL_PROJECTION_MATRIX,proj.RowPointer(0));
	glGetFloatv(GL_MODELVIEW_MATRIX,view.RowPointer(0));
	simul::math::Multiply4x4(viewproj,view,proj);

	glUniformMatrix4fv(worldViewProj_param,1,false,viewproj.RowPointer(0));

	glUniform3f(eyePosition_param,cam_pos.x,cam_pos.y,cam_pos.z);
	if(baseSkyInterface)
	{
		simul::math::Vector3 irr=baseSkyInterface->GetLocalIrradiance(0.f);
	irr*=0.05f;
	glUniform3f(sunlight_param,irr.x,irr.y,irr.z);
		simul::math::Vector3 sun_dir=baseSkyInterface->GetDirectionToLight(0.f);
	glUniform3f(lightDir_param,sun_dir.x,sun_dir.y,sun_dir.z);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, texArray);
	glUniform1i(textures_param,0); 

	ERROR_CHECK
	int h=heightMapInterface->GetPageSize();
	simul::math::Vector3 origin=heightMapInterface->GetOrigin();
	float PageWorldX=heightMapInterface->GetPageWorldX();
	float PageWorldY=heightMapInterface->GetPageWorldY();
	float PageSize=(float)heightMapInterface->GetPageSize();
	glBegin(GL_TRIANGLE_STRIP);
	for(int i=0;i<h-1;i++)
	{
		float x1=(i  )*PageWorldX/(float)PageSize+origin.x;
		float x2=(i+1)*PageWorldX/(float)PageSize+origin.x;
		for(int j=0;j<h-1;j++)
		{
			int J=j;
			if(i%2)
				J=(h-2-j);
			float y=(J)*PageWorldX/(float)PageSize+origin.y;
			float z1=heightMapInterface->GetHeightAt(i,J);
			float z2=heightMapInterface->GetHeightAt(i+1,J);
			simul::math::Vector3 X1(x1,y,z1);
			simul::math::Vector3 X2(x2,y,z2);
			if(i%2==1)
				std::swap(X1,X2);
			glTexCoord3f(0,0,1.f);
			glVertex3f(X1.x,X1.y,X1.z);
			glTexCoord3f(0,0,1.f);
			glVertex3f(X2.x,X2.y,X2.z);
		}
	}
	glEnd();
	glUseProgram(0);
ERROR_CHECK
	glPopAttrib();
ERROR_CHECK
}