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
#include <algorithm>
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
static std::string shaderPath;
static std::string last_filename;

using namespace simul;
using namespace opengl;
using namespace std;


static int LineCount(const std::string &str)
{
	int pos=str.find('\n');
	int count=1;
	while(pos>=0)
	{
		pos=str.find('\n',pos+1);
		if(pos>=0)
			count++;
	}
	return count;
}

static int GetLineNumber(const std::string &str,int line_pos)
{
	int pos=0;
	int count=0;
	while(pos>=0&&pos<=line_pos)
	{
		count++;
		pos=str.find('\n',pos+1);
	}
	return count;
}
struct TiePoint
{
	TiePoint(const char *f,int loc,int glob)
		:filename(f),local(loc),global(glob)
	{}
	std::string filename;
	int local,global;
};
bool operator<(const TiePoint &t1,const TiePoint &t2)
{
	return (t1.global<t2.global);
}
struct NameLine
{
	NameLine():line(-1){}
	std::string filename;
	int line;
};
struct FilenameChart
{
	std::vector<TiePoint> tiePoints;
	// Add a tie point for the start of this file, plus one to mark where its parent restarts.
	void add(const char *filename,int global_line,const std::string &src)
	{
		int newlen		=LineCount(src);
		int parent_i=-1;
		// Push back the tie points after this one.
		for(int i=0;i<(int)tiePoints.size();i++)
		{
			TiePoint &t=tiePoints[i];
			if(global_line>=t.global)
			{
				parent_i=i;
			}
			if(t.global>=global_line)
			{
				t.global+=newlen;
			}
		}
		tiePoints.push_back(TiePoint(filename,0,global_line));
		if(parent_i>=0)
		{
			const TiePoint &p		=tiePoints[parent_i];
			int next_line_in_parent	=global_line-p.global+1;
			int next_line_global	=global_line+newlen;
			tiePoints.push_back(TiePoint(p.filename.c_str(),next_line_in_parent,next_line_global));
		}
		sort();
	}
	void sort()
	{
		std::sort(tiePoints.begin(),tiePoints.end());
	}
	// From the global line number, find the file.
	NameLine find(int global) const
	{
		int best_i=-1;
		for(int i=0;i<(int)tiePoints.size();i++)
		{
			const TiePoint &t=tiePoints[i];
			if(t.global<=global)
			{
				best_i=i;
			}
		}
		NameLine n;
		if(best_i>=0)
		{
			const TiePoint &t=tiePoints[best_i];
			n.filename=t.filename;
			n.line=global-t.global+t.local;
		}
		return n;
	}
};

void printShaderInfoLog(GLuint sh,const FilenameChart &filenameChart)
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
			//std::cerr<<std::endl<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
			int pos=0;
			int next=info_log.find('\n',pos);
			while(next>=0)
			{
				std::string line=info_log.substr(pos,next);
				if(line.find("ERROR")<line.length())
				{
					int first_colon=line.find(":");
					int second_colon=line.find(":",first_colon+1);
					int third_colon=line.find(":",second_colon+1);
					std::string linestr=line.substr(second_colon+1,third_colon-second_colon-1);
					std::string err_msg=line.substr(third_colon+1,line.length()-third_colon-1);
					int number=atoi(linestr.c_str());
					NameLine n=filenameChart.find(number);
					std::cout<<n.filename.c_str()<<"("<<n.line<<"): Error: "<<err_msg.c_str()<<std::endl;
				}
				pos=next;
				next=info_log.find('\n',pos+1);
			}
			//std::cout<<last_filename.c_str()<<":\n"<<info_log.c_str()<<std::endl;
		}
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


GLuint SetShader(GLuint sh,const std::vector<std::string> &sources,const map<string,string> &defines,const FilenameChart &filenameChart)
{
/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
	const int MAX_STRINGS=12;
	const int MAX_LINES=512;
	const int MAX_LINE_LENGTH=256;					// 255 + NULL terminator
	const char *strings[MAX_STRINGS];
	int s=0;
	if(defines.size())
	{
		static char program[MAX_LINES*MAX_LINE_LENGTH];
		char *ptr=program;
		std::string def;
		for(map<string,string>::const_iterator i=defines.begin();i!=defines.end();i++)
		{
			def+="#define ";
			def+=i->first;
			def+=" ";
			def+=i->second;
			def+="\r\n";
		}
		sprintf_s(ptr,MAX_LINES*MAX_LINE_LENGTH,"%s\n",def.c_str());
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
	printShaderInfoLog(sh,filenameChart);

	int result=1;
	glGetShaderiv(sh,GL_COMPILE_STATUS,&result);
	if(!result)
	{
		return 0;
	}
    return sh;
}

GLuint SetShader(GLuint sh,const std::vector<std::string> &sources,const map<string,string> &defines)
{
	FilenameChart filenameChart;
	return SetShader(sh,sources,defines,filenameChart);
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

GLuint MakeProgram(const char *filename)
{
	 map<string,string> defines;
	 return MakeProgram(filename,defines);
}

GLuint MakeProgram(const char *filename,const map<string,string> &defines)
{
	char v[100];
	char f[100];
	sprintf_s(v,98,"%s.vert",filename);
	sprintf_s(f,98,"%s.frag",filename);
	return MakeProgram(v,NULL,f,defines);
}


GLuint MakeProgramWithGS(const char *filename)
{
	 map<string,string> defines;
	 return MakeProgramWithGS(filename,defines);
}
GLuint MakeProgramWithGS(const char *filename,const map<string,string> &defines)
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
	map<string,string> defines;
	return SetShaders(vert_src,frag_src,defines);
}
GLuint SetShaders(const char *vert_src,const char *frag_src,const map<string,string> &defines)
{
	GLuint prog				=glCreateProgram();
	GLuint vertex_shader	=glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader	=glCreateShader(GL_FRAGMENT_SHADER);
	std::vector<std::string> v;
	v.push_back(vert_src);
	std::vector<std::string> f;
	f.push_back(frag_src);
    vertex_shader			=SetShader(vertex_shader	,v,defines);
    fragment_shader			=SetShader(fragment_shader	,f,defines);
	glAttachShader(prog,vertex_shader);
	glAttachShader(prog,fragment_shader);
	glLinkProgram(prog);
	glUseProgram(prog);
	printProgramInfoLog(prog);
	return prog;
}

GLuint MakeProgram(const char *vert_filename,const char *geom_filename,const char *frag_filename)
{
	map<string,string> defines;
	return MakeProgram(vert_filename,geom_filename,frag_filename,defines);
}

GLuint MakeProgram(const char *vert_filename,const char *geom_filename,const char *frag_filename,const map<string,string> &defines)
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
		ptr+=len;
		*ptr++='\r';
		*ptr++='\n';
	}
	ifs.close();
	if(ptr!=shader_source)
		ptr[-2]=0;
	std::string str(shader_source);
	return str;
}

GLuint LoadShader(const char *filename,const map<string,string> &defines)
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

	FilenameChart filenameChart;
	filenameChart.add(((shaderPath+"/")+filename).c_str(),0,src);
	// process #includes.
	std::vector<std::string> include_files;
	size_t pos=0;
	while((pos=src.find("#include",pos))<src.length())
	{
		int start_of_line=pos;
		int start_line=GetLineNumber(src,pos);
		pos+=9;
		int eol=src.find("\n",pos+1);
		std::string include_file=src.substr(pos,eol-pos);
		include_file=include_file.substr(1,include_file.length()-2);
		include_files.push_back(include_file);
		src=src.insert(start_of_line,"//");
		eol+=2;
		src=src.insert(eol,"\n");
		std::string newsrc=loadShaderSource(include_file.c_str());
		src=src.insert(eol+1,newsrc);
		filenameChart.add(((shaderPath+"/")+include_file).c_str(),start_line,newsrc);
	}
	std::vector<std::string> srcs;
	for(int i=0;i<(int)include_files.size();i++)
	{
		//srcs.push_back();
	}
	srcs.push_back(src);
	GLuint sh=glCreateShader(shader_type);
	sh=SetShader(sh,srcs,defines,filenameChart);
    return sh;
}
