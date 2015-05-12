#ifndef SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H
#define SIMUL_PLATFORM_OPENGL_SIMULGLUTILITIES_H

#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/OpenGL/Export.h"
#include <assert.h>
#include <stdlib.h>	// For definition of NULL
#include <iostream>	// for cerr

namespace simul
{
	namespace base
	{
		class FileLoader;
	}
	namespace sky
	{
		struct float4;
	}
}
struct VertexXyzRgba;
SIMUL_OPENGL_EXPORT_CLASS Utilities
{
	static int instance_count;
	static Utilities *ut;
	static int screen_width;
	static int screen_height;
	Utilities();
	~Utilities();
public:
	static Utilities &GetSingleton();
	void RestoreDeviceObjects(void *);
	void SetScreenSize(int w,int h);
	void InvalidateDeviceObjects();
	static void Kill();
};

namespace simul
{
	namespace opengl
	{
		extern void RenderTexture(int x,int y,int w,int h);
		//! A wrapper around glewIsSupported() that checks for errors.
		extern bool SIMUL_OPENGL_EXPORT CheckExtension(const char *txt);

		extern bool IsExtensionSupported(const char *name);
		extern SIMUL_OPENGL_EXPORT void Ortho();
		//! A wrapper around glOrtho() that also resets the GL matrices, and stores window height for later use.
		extern SIMUL_OPENGL_EXPORT void SetOrthoProjection(int w,int h);
		extern SIMUL_OPENGL_EXPORT void SetTopDownOrthoProjection(int w,int h);
		//! A wrapper around gluPerspective() that also stores window height for later use.
		extern SIMUL_OPENGL_EXPORT void SetPerspectiveProjection(int w,int h,float field_of_view);
		//! Controls vertical syncing - 0 means do not sync, 1 means sync with the monitor, 2 means sync at half speed, etc.
		extern SIMUL_OPENGL_EXPORT void SetVSync(int vsync);
		extern SIMUL_OPENGL_EXPORT void DrawQuad(int x,int y,int w,int h);
		//! Draw a simple onscreen quad from 0,0 to w,h
		extern SIMUL_OPENGL_EXPORT void DrawQuad(float x,float y,float w,float h);
		extern SIMUL_OPENGL_EXPORT void DrawFullScreenQuad();
		//! Get the current framerate - must be called once every frame to work correctly.
		extern SIMUL_OPENGL_EXPORT float GetFramerate();
		//! Reset the GL error - useful if we know there will be one and want to ignore it.
		extern SIMUL_OPENGL_EXPORT void ResetGLError();
		//! Check for a GL error, and halt the program if found.
		extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number);
		//! Check the given error code, and halt the program if it is non-zero.
		extern SIMUL_OPENGL_EXPORT void CheckGLError(const char *filename,int line_number,int err);
		#define GL_ERROR_CHECK simul::opengl::CheckGLError(__FILE__,__LINE__);ERRNO_CHECK
		#define SAFE_DELETE_PROGRAM(prog)		{if(prog){GLuint shaders[2];GLsizei count;glGetAttachedShaders(prog,2,&count,shaders);for(int i=0;i<count;i++) glDeleteShader(shaders[i]); glDeleteProgram(prog);prog=0;}}
		#define SAFE_DELETE_TEXTURE(tex)		{if(tex) glDeleteTextures(1,&tex);tex=0;}
		#define SAFE_DELETE_BUFFER(buff)		{if(buff) glDeleteBuffers(1,&buff);buff=0;}
		#define SAFE_DELETE_VAO(vao)			{if(vao) glDeleteVertexArrays(1,&vao);vao=0;}
		#define SAFE_DELETE_FRAMEBUFFER(fb)		{if(fb) glDeleteFramebuffers(1,&fb);fb=0;}
		#define SAFE_DELETE_RENDERBUFFER(rb)	{if(rb) glDeleteRenderbuffers(1,&rb);rb=0;}
		extern SIMUL_OPENGL_EXPORT bool RenderAngledQuad(const float *dir,float half_angle_radians);
		extern SIMUL_OPENGL_EXPORT void PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0,bool centred=false);

		extern SIMUL_OPENGL_EXPORT void CalcCameraPosition(float *cam_pos,float *cam_dir=0);
		extern SIMUL_OPENGL_EXPORT void FixGlProjectionMatrix(float required_distance);
		extern SIMUL_OPENGL_EXPORT void OrthoMatrices();

		extern void setParameter(GLuint program,const char *name,float value);
		extern void setParameter(GLuint program,const char *name,float value1,float value2);
		extern void setParameter(GLuint program,const char *name,float value1,float value2,float value3);
		extern void setParameter(GLuint program,const char *name,float value1,float value2,float value3,float value4);
		extern void setParameter(GLuint program,const char *name,int value);
		extern void setVector4(GLuint program,const char *name,const float *value);
		extern void setParameter(GLuint program,const char *name,const simul::sky::float4 &value);
		extern void setParameter2(GLuint program,const char *name,const simul::sky::float4 &value);
		extern void setParameter3(GLuint program,const char *name,const simul::sky::float4 &value);
		extern void setMatrix(GLuint program,const char *name,const float *value);
		extern void setMatrixTranspose(GLuint program,const char *name,const float *value);
		extern void setTexture(GLuint program,const char *name,int texture_number,GLuint texture);
		extern void set3DTexture(GLuint program,const char *name,int texture_number,GLuint texture);

		extern void setParameter(GLint,int value);
		extern void setParameter(GLint,float value);
		extern void setParameter2(GLint,const simul::sky::float4 &value);
		extern void setParameter3(GLint,const simul::sky::float4 &value);
		extern void setParameter(GLint loc,const float *value);
		extern void linkToConstantBuffer(GLuint program,const char *name,GLuint bindingIndex);

		// make a 2D texture.
		extern GLuint make2DTexture(int w,int l,const float *src=NULL);

		#define MAKE_GL_CONSTANT_BUFFER(ubo,Struct,bindingIndex)	\
			glGenBuffers(1, &ubo);	\
			glBindBuffer(GL_constant_buffer, ubo);	\
			glBufferData(GL_constant_buffer, sizeof(Struct), NULL, GL_DYNAMIC_DRAW);	\
			glBindBuffer(GL_constant_buffer, 0);		\
			glBindBufferRange(GL_constant_buffer,bindingIndex,ubo,0, sizeof(Struct));


		#define UPDATE_GL_CONSTANT_BUFFER(ubo,constants,bindingIndex)	\
			glBindBuffer(GL_constant_buffer, ubo);	\
			glBufferSubData(GL_constant_buffer,0, sizeof(constants), &constants);	\
			glBindBuffer(GL_constant_buffer, 0);		\
			glBindBufferBase(GL_constant_buffer,bindingIndex,ubo);

		//! Useful Wrapper class to encapsulate constant buffer behaviour
		template<class T> class ConstantBuffer:public T
		{
		public:
			ConstantBuffer()
				:ubo(0)
			{
				// Clear out the part of memory that corresponds to the base class.
				// We should ONLY inherit from simple structs.
				memset(static_cast<T*>(this),0,sizeof(T));
			}
			~ConstantBuffer()
			{
				Release();
			}
			GLuint	ubo;
			//GLint bindingIndex;
			//! Create the buffer object.
			void RestoreDeviceObjects()
			{
				Release();
				glGenBuffers(1, &ubo);
				glBindBuffer(GL_constant_buffer, ubo);
				glBufferData(GL_constant_buffer, sizeof(T), NULL, GL_DYNAMIC_DRAW );
				glBindBuffer(GL_constant_buffer, 0);
			}
			//! Find the constant buffer in the given effect, and link to it.
			void LinkToProgram(GLuint program,const char *name,GLint bindingIndex)
			{
				//if(bindingIndex<=0)
				//	exit(1);
				//this->bindingIndex=bindingIndex;
				GLint indexInShader=glGetUniformBlockIndex(program,name);
				if(indexInShader>=0)
				{
					glUniformBlockBinding(program,indexInShader,bindingIndex);
					GLint blockSize;
					glGetActiveUniformBlockiv(program, indexInShader,
                      GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
					// blocksize can be greater than sizeof(T), because GLSL might expect whole chunks of 16 bytes, while C++ only allocates what it needs. But
					// blocksize can't be less than sizeof(T), because it won't have the space to take the data we're sending it!
					if(blockSize<sizeof(T))
					{
						std::cerr<<"blockSize<sizeof(T) - shader/C++ mismatch in GLSL."<<std::endl;
						return;
					}
					glBindBufferBase(GL_constant_buffer,bindingIndex,ubo);
					glBindBufferRange(GL_constant_buffer,bindingIndex,ubo,0,sizeof(T));	
				}
				else
					std::cerr<<"ConstantBuffer<> LinkToProgram did not find the buffer named "<<name<<" in the program."<<std::endl;
			}
			//! Free the allocated buffer.
			void Release()
			{
				SAFE_DELETE_BUFFER(ubo);
			}
			//! Apply the stored data using the given context, in preparation for rendering.
			void Apply()
			{
				glBindBuffer(GL_constant_buffer,ubo);
				glBufferSubData(GL_constant_buffer,0,sizeof(T),static_cast<T*>(this));
				glBindBuffer(GL_constant_buffer,0);
			}
		};
	}
}
#endif