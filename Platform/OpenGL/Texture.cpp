#include "GL/glew.h"
#include "Texture.h"
#include "LoadGLImage.h"
#include "SimulGLUtilities.h"
#include "FramebufferGL.h"
#include "RenderPlatform.h"
#include <string>

using namespace simul;
using namespace opengl;
#if defined(_MSC_VER) && (defined(WIN32) || defined(WIN64))
#include <windows.h>
#else
#include <stdio.h>
#include <strings.h>
#define _stricmp strcasecmp
#endif

SamplerState::SamplerState()
	:sampler_state(0)
{
}

SamplerState::~SamplerState()
{
	void InvalidateDeviceObjects();
}

void SamplerState::InvalidateDeviceObjects()
{
	if(sampler_state)
		glDeleteSamplers(1,&sampler_state);
	sampler_state=0;
}


opengl::Texture::Texture()
	:pTextureObject(0)
	,m_fb(0)
{
}

opengl::Texture::~Texture()
{
	InvalidateDeviceObjects();
}

void Texture::InvalidateDeviceObjects()
{
	GL_ERROR_CHECK
	SAFE_DELETE_TEXTURE(pTextureObject);
	SAFE_DELETE_FRAMEBUFFER(m_fb);
	GL_ERROR_CHECK
}
// Load a texture file
void opengl::Texture::LoadFromFile(crossplatform::RenderPlatform *renderPlatform,const char *pFilePathUtf8)
{
	const std::vector<std::string> &pathsUtf8=renderPlatform->GetTexturePathsUtf8();
	dim=2;
	std::string filename(pFilePathUtf8);
	int dot_pos			=(int)filename.find_last_of(".");
	std::string extension;
	pTextureObject		=0;
	if(dot_pos>=0&&dot_pos<(int)filename.length())
		extension		=filename.substr(dot_pos+1,filename.length()-dot_pos-1);
	GL_ERROR_CHECK
		GLint internal_format=GL_RGBA;
	pTextureObject		=LoadGLImage(pFilePathUtf8,pathsUtf8,GL_REPEAT,&width,&length,&internal_format);
	pixelFormat			=opengl::RenderPlatform::FromGLFormat(internal_format);
	GL_ERROR_CHECK

}

void Texture::LoadTextureArray(crossplatform::RenderPlatform *renderPlatform,const std::vector<std::string> &texture_files)
{
ERRNO_CHECK
	const std::vector<std::string> &texturePathsUtf8=renderPlatform->GetTexturePathsUtf8();
	glGenTextures(1, &pTextureObject);
	if(!IsExtensionSupported("GL_EXT_texture_array"))
	{
		simul::base::RuntimeError("LoadTextureArray needs the GL_EXT_texture_array extension.");
		return;
	}
ERRNO_CHECK
	//GL_TEXTURE_2D_ARRAY_EXT
	glBindTexture(GL_TEXTURE_2D_ARRAY, pTextureObject);
	//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	//glTexParameterfv(GL_TEXTURE_2D_ARRAY_EXT, GL_TEXTURE_BORDER_COLOR, borderColor);
	unsigned bpp;
	GL_ERROR_CHECK
	unsigned char *data=LoadGLBitmap(texture_files[0].c_str(),texturePathsUtf8,bpp,width,length);
	GL_ERROR_CHECK
ERRNO_CHECK
	if(!data)
		return;
	GL_ERROR_CHECK
	delete [] data;
	GL_ERROR_CHECK
	dim=2;
	depth=texture_files.size();
	int num_mips=8;
	int m=1;
	glTexStorage3D(	GL_TEXTURE_2D_ARRAY
 					,num_mips
 					,GL_RGBA8
 					,width
 					,length
					,depth);
	GL_ERROR_CHECK
	//for(int i=0;i<num_mips;i++)
	{
		//glTexSubImage3D(GL_TEXTURE_2D_ARRAY, i,0,0,0,width/m,length/m,depth,GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	GL_ERROR_CHECK
	//	if(i==0)
		{
			for(int j=0;j<depth;j++)
			{
				unsigned char *data=LoadGLBitmap(texture_files[j].c_str(),texturePathsUtf8,bpp,width,length);
				glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,0,0,0,j,width/m,length/m,1,(bpp==24)?GL_BGR:GL_BGRA,GL_UNSIGNED_BYTE,data);
			}
		}
	GL_ERROR_CHECK
		m*=2;
	}
	GL_ERROR_CHECK
	//glTexImage3D	(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA, width/2, height/2, num_layers, ...);
	//glTexImage3D	(GL_TEXTURE_2D_ARRAY, 2, GL_RGBA, width/4, height/4, num_layers, ...);

	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	GL_ERROR_CHECK
ERRNO_CHECK
}

bool opengl::Texture::IsValid() const
{
	return (pTextureObject>0);
}

void Texture::ensureTexture2DSizeAndFormat(simul::crossplatform::RenderPlatform *,int w,int l
	,crossplatform::PixelFormat p,bool computable,bool rendertarget,bool depthstencil,int num_samples,int aa_quality,bool wrap)
{
	if(w==width&&l==length&&pixelFormat==p)
		return;
GL_ERROR_CHECK
	pixelFormat=p;
	GLuint internal_format=opengl::RenderPlatform::ToGLFormat(pixelFormat);
	width=w;
	length=l;
	dim=2;
	glGenTextures(1,&pTextureObject);
GL_ERROR_CHECK
	glBindTexture(GL_TEXTURE_2D,pTextureObject);
GL_ERROR_CHECK
	glTexStorage2D(	GL_TEXTURE_2D,
 					1,
 					internal_format,
 					w,
 					l);
GL_ERROR_CHECK
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w,l, internal_format, RenderPlatform::ToGLExternalFormat(pixelFormat), NULL);
	//glTexImage2D(GL_TEXTURE_2D,0,internal_format,w,l,0,RenderPlatform::ToGLExternalFormat(pixelFormat),RenderPlatform::DataType(pixelFormat),NULL);
glGenerateMipmap(GL_TEXTURE_2D); 
GL_ERROR_CHECK
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap?GL_REPEAT:GL_CLAMP_TO_EDGE);//
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap?GL_REPEAT:GL_CLAMP_TO_EDGE);//
GL_ERROR_CHECK
	if(rendertarget)
	{
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		glGenFramebuffers(1, &m_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pTextureObject, 0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
		{
			FramebufferGL::CheckFramebufferStatus();
			SIMUL_BREAK("Framebuffer incomplete for rendertarget texture");
		}
	}
	if(depthstencil)
	{
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		glGenFramebuffers(1, &m_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, pTextureObject, 0);
		
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
		{
			FramebufferGL::CheckFramebufferStatus();
			SIMUL_BREAK("Framebuffer incomplete for rendertarget texture");
		}
	}
	glBindTexture(GL_TEXTURE_2D,0);
GL_ERROR_CHECK
}

void Texture::ensureTextureArraySizeAndFormat(crossplatform::RenderPlatform *renderPlatform,int w,int l,int num_layers,crossplatform::PixelFormat f,bool computable,bool rendertarget,bool cubemap)
{
	pixelFormat=f;
	if(w==width&&l==length)
		return;
	pixelFormat=f;
	GLuint internal_format=opengl::RenderPlatform::ToGLFormat(pixelFormat);
	GLuint layout=opengl::RenderPlatform::ToGLExternalFormat(pixelFormat);
	GLenum datatype=opengl::RenderPlatform::DataType(pixelFormat);
	width=w;
	length=l;
	depth=num_layers;
	dim=2;
	glGenTextures(1,&pTextureObject);
	glBindTexture(GL_TEXTURE_2D_ARRAY,pTextureObject);
	int m=1;
	
	glTexStorage3D(	GL_TEXTURE_2D_ARRAY,
 					1,
 					internal_format,
 					w,
 					l
					,num_layers);
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY); 
	glBindTexture(GL_TEXTURE_2D_ARRAY,0);
/*	for(int i=0;i<1;i++)//num_mips
	{
		glTexImage3D(GL_TEXTURE_2D_ARRAY, i,internal_format	,width/m,length/m,num_layers,0,layout, datatype, NULL);
	GL_ERROR_CHECK
		//if(i==0)
		{
			//glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,i,0,0,0,width/m,length/m,1,layout,datatype,NULL);
	GL_ERROR_CHECK
			//glTexSubImage3D	(GL_TEXTURE_2D_ARRAY,i,0,0,1,width/m,length/m,1,layout,datatype,NULL);
		}
	GL_ERROR_CHECK
		m*=2;
	}*/
}

void Texture::setTexels(crossplatform::DeviceContext &deviceContext,const void *src,int texel_index,int num_texels)
{
	GL_ERROR_CHECK
	GLuint frmt		=opengl::RenderPlatform::ToGLFormat(pixelFormat);
	GLenum ext_frmt	=opengl::RenderPlatform::ToGLExternalFormat(pixelFormat);
	GLenum dt		=opengl::RenderPlatform::DataType(pixelFormat);
	if(dim==2)
	{
		glBindTexture(GL_TEXTURE_2D,pTextureObject);
		GL_ERROR_CHECK
		if(texel_index==0&&num_texels==width*length)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width,length, ext_frmt, dt, src);
//	glTexImage2D(GL_TEXTURE_2D,0,frmt,width,length,0,ext_frmt,dt,src);
	glGenerateMipmap(GL_TEXTURE_2D); 
		GL_ERROR_CHECK
	glBindTexture(GL_TEXTURE_2D,0);
	}
	else if(dim==3)
	{
		glBindTexture(GL_TEXTURE_3D,pTextureObject);
		GL_ERROR_CHECK
		if(texel_index==0&&num_texels==width*length*depth)
			glTexSubImage3D(GL_TEXTURE_3D,0,0,0,0,width,length,depth,ext_frmt,dt,src);
	glGenerateMipmap(GL_TEXTURE_3D); 
		GL_ERROR_CHECK
		glBindTexture(GL_TEXTURE_3D,0);
	}
#if 0
	int start_slice				=it.texel_index/sliceStride;
	int start_texel_in_slice	=it.texel_index-start_slice*sliceStride;
	int start_row				=start_texel_in_slice/stride;
	int start_texel_in_row		=start_texel_in_slice-start_row*stride;

	int end_slice				=(it.texel_index+num_texels)/sliceStride;
	int end_texel_in_slice		=(it.texel_index+num_texels)-end_slice*sliceStride;
	int end_row					=end_texel_in_slice/stride;
	int end_texel_in_row		=end_texel_in_slice-end_row*stride;

	int first_row_length		=stride-start_texel_in_row;
	int first_slice_rows		=sent_length-start_row-1;

	if(first_row_length==stride)
	{
		first_row_length=0;
		first_slice_rows++;
		start_row--;
	}
	int num_slices=end_slice-start_slice-1;
	if(first_slice_rows==(int)sent_length)
	{
		first_slice_rows=0;
		num_slices++;
		start_slice--;
	}
	if(end_slice==start_slice)
	{
		if(end_row==start_row)
		{
			first_row_length=end_texel_in_row-start_texel_in_row;
			end_texel_in_row=0;
		}
		first_slice_rows=end_row-start_row-1;
	}
	const unsigned *uptr=&((fillTextures[it.texture_index].texels)[it.texel_index]);
	int num_written=0;
	if(first_row_length>0)
	{
		block_texture_fill t(start_texel_in_row,start_row,start_slice,first_row_length,1,1,uptr);
		num_written+=first_row_length;
		texel_index+=t.w*t.l*t.d;
	glTexImage2D(GL_TEXTURE_2D,0,frmt,w,l,0,GL_RGBA,GL_UNSIGNED_INT,NULL);
	}
	// The rest of the first slice:
	if(first_slice_rows>0)
	{
		block_texture_fill t(0,start_row+1,start_slice,stride,first_slice_rows,1,uptr);
		uptr+=stride*first_slice_rows;
		num_written+=stride*first_slice_rows;
		it.texel_index+=t.w*t.l*t.d;
		return t;
	}
	// The main block:
	if(num_slices>0)
	{
		block_texture_fill t(0,0,start_slice+1,stride,sent_length,num_slices,uptr);
		uptr+=stride*sent_length*num_slices;
		num_written+=stride*sent_length*num_slices;
		it.texel_index+=t.w*t.l*t.d;
		return t;
	}
	// the rows at the end:
	if(end_slice!=start_slice&&end_row>0)
	{
		block_texture_fill t(0,0,end_slice,stride,end_row,1,uptr);
		uptr+=stride*end_row;
		num_written+=stride*end_row;
		it.texel_index+=t.w*t.l*t.d;
		return t;
	}
	// and the final row:
	if(end_texel_in_row>0)
	{
		block_texture_fill t(0,end_row,end_slice,end_texel_in_row,1,1,uptr);
		uptr+=end_texel_in_row;
		num_written+=end_texel_in_row;
		it.texel_index+=t.w*t.l*t.d;
		return t;
	}

#endif
}

void Texture::activateRenderTarget(simul::crossplatform::DeviceContext &)
{
	if(!m_fb)
		return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb); 
	GL_ERROR_CHECK
	FramebufferGL::CheckFramebufferStatus();
	glGetIntegerv(GL_VIEWPORT,main_viewport);
	GL_ERROR_CHECK
	glViewport(0,0,width,length);
	GL_ERROR_CHECK
	FramebufferGL::fb_stack.push(m_fb);
	if(depth>1)
	{
		const GLenum buffers[9] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6, GL_COLOR_ATTACHMENT7 };
		glDrawBuffers(depth, buffers);
	}
	GL_ERROR_CHECK
}

void Texture::deactivateRenderTarget()
{
	//glFlush(); 
	GL_ERROR_CHECK
	FramebufferGL::CheckFramebufferStatus();
	GL_ERROR_CHECK
	// remove m_fb from the stack and...
	FramebufferGL::fb_stack.pop();
	// ..restore the n one down.
	GLuint last_fb=FramebufferGL::fb_stack.top();
	GL_ERROR_CHECK
    glBindFramebuffer(GL_FRAMEBUFFER,last_fb);
	GL_ERROR_CHECK
	glViewport(0,0,main_viewport[2],main_viewport[3]);
	GL_ERROR_CHECK
}

int Texture::GetLength() const
{
	return length;
}

int Texture::GetWidth() const
{
	return width;
}

int Texture::GetSampleCount() const
{
	return 0;
}

int Texture::GetDimension() const
{
	return dim;
}



void simul::opengl::Texture::setTexels(void *,const void *src,int x,int y,int z,int w,int l,int d)
{
	glTexSubImage3D(	GL_TEXTURE_3D,0,
						x,y,z,
						w,l,d,
						GL_RGBA,GL_UNSIGNED_INT_8_8_8_8,
						src);
}

void simul::opengl::Texture::ensureTexture3DSizeAndFormat(crossplatform::RenderPlatform *,int w,int l,int d,crossplatform::PixelFormat pf,bool /*computable*/,int m,bool rendertargets)
{
	GL_ERROR_CHECK
	pixelFormat=pf;
	GLuint frmt=opengl::RenderPlatform::ToGLFormat(pixelFormat);
	dim=3;
	width=w;
	length=l;
	depth=d;
	if(pTextureObject)
	{
		int W,L,D;
		glBindTexture(GL_TEXTURE_3D,pTextureObject);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_WIDTH,&W);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_HEIGHT,&L);
		glGetTexLevelParameteriv(GL_TEXTURE_3D,0,GL_TEXTURE_DEPTH,&D);
		if(w!=W||l!=L||d!=D)
		{
			SAFE_DELETE_TEXTURE(pTextureObject);
		}
	}
	GL_ERROR_CHECK
	if(!pTextureObject)
	{
		glGenTextures(1,&(pTextureObject));
		glBindTexture(GL_TEXTURE_3D,pTextureObject);
		GLenum number_format=GL_RGBA;
		GLenum number_type	=GL_UNSIGNED_INT;
		switch(frmt)
		{
		case GL_RGBA:
			number_format	=GL_RGBA;
			number_type		=GL_UNSIGNED_INT;
			break;
		case GL_RGBA32F:
			number_format	=GL_RGBA;
			number_type		=GL_FLOAT;
			break;
		case GL_LUMINANCE32F_ARB:
			number_format	=GL_LUMINANCE;
			number_type		=GL_FLOAT;
			break;
		//((frmt==GL_RGBA)?GL_UNSIGNED_INT:GL_UNSIGNED_SHORT)
		default:
			break;
		};
	glTexStorage3D(	GL_TEXTURE_3D
 					,m
 					,frmt
 					,w
 					,l
					,d);
		//glTexImage3D(GL_TEXTURE_3D,0,(GLint)frmt,w,l,d,0,number_format,number_type,0);
	GL_ERROR_CHECK
	//	glTexImage3D(GL_TEXTURE_3D,0,GL_LUMINANCE32F_ARB:GL_RGBA32F_ARB,w,l,d,0,GL_LUMINANCE:GL_RGBA,GL_FLOAT,src);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
	if(rendertargets)
	{
		SAFE_DELETE_FRAMEBUFFER(m_fb);
		glGenFramebuffers(1, &m_fb);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
		for (int i=0;i<8&&i<d;i++)
		{
			glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0+i, pTextureObject, 0,i);
		}
		GLenum status= (GLenum) glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if(status!=GL_FRAMEBUFFER_COMPLETE)
		{
			FramebufferGL::CheckFramebufferStatus();
			SIMUL_BREAK("Framebuffer incomplete for rendertarget texture");
		}
	}
	mips=m;
	GL_ERROR_CHECK
}

void Texture::GenerateMips(crossplatform::DeviceContext &deviceContext)
{
}

void Texture::copyToMemory(crossplatform::DeviceContext &,void *,int ,int )
{
}
