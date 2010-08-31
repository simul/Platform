#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#ifndef _MSC_VER
#include <cstdio>
#include <cstring>
#endif
#include "LoadGLProgram.h"
static std::string shaderPath;
static std::string last_filename;
void printShaderInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
    char *infoLog;

	glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        infoLog = (char *)malloc(infologLength);
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		std::string info_log=infoLog;

		size_t pos=info_log.find_first_of('(');
		info_log=info_log.substr(1,info_log.length()-1);
		
		std::cout<<std::endl<<last_filename.c_str()<<info_log.c_str()<<std::endl;
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
		std::cout<<std::endl<<infoLog<<std::endl;
        free(infoLog);
    }
}

void SetShaderPath(const char *path)
{
	shaderPath=path;
}

GLuint LoadProgram(GLuint prog,const char *filename,const char *defines)
{
    std::string filePath=shaderPath;
	char last=0;
	if(filePath.length())
		filePath[filePath.length()-1];
	if(last!='/'&&last!='\\')
		filePath+="/";
	filePath+=filename;
	last_filename=filePath;
	std::ifstream ifs(filePath.c_str());
	if(!ifs.good())
	{
		std::cerr<<"\nERROR:\tShader file "<<filename<<" not found, exiting.\n";
		std::cerr<<"\n\t\tShader path is "<<shaderPath.c_str()<<", is this correct?\n";
		exit(1);
	}
	std::string shader_source;

/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;   // 255 + NULL terminator
	static char program[MAX_LINES*MAX_LINE_LENGTH];
	char *ptr=program;

	if(defines)
	{
		int len=strlen(defines);
		strcpy(ptr,defines);
		ptr[len]='\n';
		ptr+=len+1;
	}

	while(!ifs.eof())
	{
		ifs.getline(ptr,MAX_LINE_LENGTH);
		int len=strlen(ptr);
		ptr[len]='\n';
		ptr+=len+1;
	}
	ptr[0]=0;

	const char *strings[1];
	strings[0]=program;
	int lenOfStrings[1];
	lenOfStrings[0]=strlen(strings[0]);
	glShaderSource(prog,1,strings,NULL);
    if(!prog)
		std::cerr<<std::endl<<"Error creating program "<<filePath.c_str()<<std::endl;
	else
	{
		glCompileShader(prog);
	}
	std::cout<<std::endl<<filePath.c_str()<<": ";
	printShaderInfoLog(prog);

	int result=1;
	glGetShaderiv(prog,GL_COMPILE_STATUS,&result);
	if(!result)
	{
		std::cerr<<"\nERROR:\tShader failed to compile, exiting\n";
		exit(1);
	}
    return prog;
}
