#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#ifndef _MSC_VER
#include <cstdio>
#include <cstring>
#else
#include <windows.h>
#endif
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
static std::string shaderPath;
static std::string last_filename;

using namespace simul;
using namespace opengl;

void printShaderInfoLog(GLuint sh)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

	glGetShaderiv(sh, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(sh, infologLength, &charsWritten, infoLog);
		std::string info_log=infoLog;
		if(info_log.find("No errors")>=info_log.length())
		{
			std::cerr<<std::endl<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		}
		else if(info_log.find("WARNING")<info_log.length())
			std::cout<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		free(infoLog);
    }
}

void printProgramInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        infoLog = (char *)malloc(infologLength);
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::string info_log=infoLog;
		if(info_log.find("No errors")>=info_log.length())
		{
			std::cerr<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		}
		else if(info_log.find("WARNING")<info_log.length())
			std::cout<<last_filename.c_str()<<":\n"<<infoLog<<std::endl;
        free(infoLog);
    }
}

namespace simul
{
	namespace opengl
	{
		void SetShaderPath(const char *path)
		{
			shaderPath=path;
		}
	}
}

GLuint MakeProgram(const char *filename,const char *defines)
{
	char v[100];
	char f[100];
	sprintf_s(v,98,"%s.vert",filename);
	sprintf_s(f,98,"%s.frag",filename);
	return MakeProgram(v,NULL,f,defines);
}

GLuint MakeProgramWithGS(const char *filename,const char *defines)
{
	char v[100];
	char f[100];
	char g[100];
	sprintf_s(v,98,"%s.vert",filename);
	sprintf_s(f,98,"%s.frag",filename);
	sprintf_s(g,98,"%s.geom",filename);
	return MakeProgram(v,g,f,defines);
}

GLuint SetShaders(const char *vert_src,const char *frag_src)
{
	GLuint prog				=glCreateProgram();
	GLuint vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
	std::vector<std::string> v;
	v.push_back(vert_src);
	std::vector<std::string> f;
	f.push_back(frag_src);
    vertex_shader			=SetShader(vertex_shader	,v);
    fragment_shader			=SetShader(fragment_shader	,f);
	glAttachShader(prog,vertex_shader);
	glAttachShader(prog,fragment_shader);
	glLinkProgram(prog);
	glUseProgram(prog);
	printProgramInfoLog(prog);
	return prog;
}

GLuint MakeProgram(const char *vert_filename,const char *geom_filename,const char *frag_filename,const char *defines)
{
	GLuint prog						=glCreateProgram();
	GLuint vertex_shader			=LoadShader(vert_filename,defines);
    if(!vertex_shader)
    {
		std::cerr<<"ERROR:\tShader failed to compile\n";
		DebugBreak();
	}
	glAttachShader(prog,vertex_shader);
	if(geom_filename)
	{
		GLuint geometry_shader	=LoadShader(geom_filename,defines);
		if(!geometry_shader)
		{
			std::cerr<<"ERROR:\tShader failed to compile\n";
			DebugBreak();
		}
		glAttachShader(prog,geometry_shader);
		ERROR_CHECK
	}
	ERROR_CHECK
    GLuint fragment_shader		=LoadShader(frag_filename,defines);
    if(!fragment_shader)
    {
		std::cerr<<"ERROR:\tShader failed to compile\n";
		DebugBreak();
	}
	ERROR_CHECK
	glAttachShader(prog,fragment_shader);
	ERROR_CHECK
	glLinkProgram(prog);
	glUseProgram(prog);
	printProgramInfoLog(prog);
	ERROR_CHECK
	return prog;
}

GLuint SetShader(GLuint sh,const std::vector<std::string> &sources,const char *defines)
{
/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_STRINGS=12;
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;					// 255 + NULL terminator
	const char *strings[MAX_STRINGS];
	int s=0;
	if(defines&&strlen(defines))
	{
		static char program[MAX_LINES*MAX_LINE_LENGTH];
		char *ptr=program;
		sprintf_s(ptr,MAX_LINES*MAX_LINE_LENGTH,"%s\n",defines?defines:"");
		strings[s++]=program;
	}
	int lenOfStrings[MAX_STRINGS];
	for(int i=0;i<(int)sources.size()&&i<MAX_STRINGS-1;i++,s++)
	{
		strings[s]		=sources[i].c_str();
		lenOfStrings[s]	=strlen(strings[s]);
	}
	glShaderSource(sh,s,strings,NULL);
    if(!sh)
		return 0;
	else
	{
		glCompileShader(sh);
	}
	printShaderInfoLog(sh);

	int result=1;
	glGetShaderiv(sh,GL_COMPILE_STATUS,&result);
	if(!result)
	{
		return 0;
	}
    return sh;
}

std::string loadShaderSource(const char *filename)
{
/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;   // 255 + NULL terminator
	char shader_source[MAX_LINES*MAX_LINE_LENGTH];
    std::string filePath=shaderPath;
	char last=0;
	if(filePath.length())
		filePath[filePath.length()-1];
	if(last!='/'&&last!='\\')
		filePath+="/";
	filePath+=filename;
	if(filePath.find(".glsl")>=filePath.length())
		last_filename=filePath;
	std::ifstream ifs(filePath.c_str());
	if(!ifs.good())
	{
		std::cerr<<"\nERROR:\tShader file "<<filename<<" not found, exiting.\n";
		std::cerr<<"\n\t\tShader path is "<<shaderPath.c_str()<<", is this correct?\n";
		exit(1);
	}
	char *ptr=shader_source;
	while(!ifs.eof())
	{
		ifs.getline(ptr,MAX_LINE_LENGTH);
		int len=strlen(ptr);
		ptr[len]='\n';
		ptr+=len+1;
	}
	ifs.close();
	ptr[0]=0;
	std::string str(shader_source);
	return str;
}

GLuint LoadShader(const char *filename,const char *defines)
{
	GLenum shader_type=0;
	std::string filename_str=filename;
	if(filename_str.find(".vert")<filename_str.length())
		shader_type=GL_VERTEX_SHADER;
	else if(filename_str.find(".frag")<filename_str.length())
		shader_type=GL_FRAGMENT_SHADER;
	else if(filename_str.find(".geom")<filename_str.length())
		shader_type=GL_GEOMETRY_SHADER;
	else throw simul::base::RuntimeError((std::string("Shader type not known for file ")+filename_str).c_str());
	
	std::string src=loadShaderSource(filename);
	// process #includes.
	std::vector<std::string> include_files;
	size_t pos=0;
	while((pos=src.find("#include",pos))<src.length())
	{
		int start_of_line=pos;
		pos+=9;
		int eol=src.find("\n",pos+1);
		std::string include_file=src.substr(pos,eol-pos);
		include_file=include_file.substr(1,include_file.length()-2);
		include_files.push_back(include_file);
		src=src.insert(start_of_line,"//");
		eol+=2;
		src=src.insert(eol,"\n");
		src=src.insert(eol+1,loadShaderSource(include_file.c_str()));
	}
	std::vector<std::string> srcs;
	for(int i=0;i<(int)include_files.size();i++)
	{
		//srcs.push_back();
	}
	srcs.push_back(src);
	GLuint sh=glCreateShader(shader_type);
	sh=SetShader(sh,srcs,defines);
    return sh;
}
