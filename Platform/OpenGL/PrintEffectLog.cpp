#include <GL/glew.h>
#include <GL/glfx.h>
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
#include "PrintEffectLog.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/DefaultFileLoader.h"

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#define BREAK_IF_DEBUGGING
#endif

using namespace simul;
using namespace base;
using namespace opengl;
using namespace std;

namespace simul
{
	namespace opengl
	{
		std::string RewriteErrorLine(std::string line,const vector<string> &sourceFilesUtf8)
		{
			bool is_error=true;
			int errpos=(int)line.find("ERROR");
			if(errpos<0)
				errpos=(int)line.find("error");
			if(errpos<0)
			{
				errpos=(int)line.find("WARNING");
				is_error=false;
			}
			if(errpos<0)
			{
				errpos=(int)line.find("warning");
				is_error=false;
			}
			//if(errpos>=0)
			{
				int first_colon		=(int)line.find(":");
				int second_colon	=(int)line.find(":",first_colon+1);
				int third_colon		=(int)line.find(":",second_colon+1);
				int first_bracket	=(int)line.find("(");
				int second_bracket	=(int)line.find(")",first_bracket+1);
				int numberstart,numberlen=0;
			//somefile.glsl(263): error C2065: 'space_' : undeclared identifier
				if(third_colon>=0&&second_colon>=0)
				{
					numberstart	=first_colon+1;
					numberlen	=second_colon-first_colon-1;
				}
			//	ERROR: 0:11: 'assign' :  cannot convert from '2-component vector of float' to 'float'
				else if((third_colon<0||numberlen>6)&&second_bracket>=0)
				{
					if(first_colon<first_bracket)
					{
						numberstart	=first_colon+1;
						numberlen	=first_bracket-first_colon-1;
					}
					else
					{
						numberstart=0;
						numberlen=first_bracket;
					}
				}
				else
					return "";
				std::string filenumber_str=line.substr(numberstart,numberlen);
				std::string err_msg=line.substr(numberstart+numberlen,line.length()-numberstart-numberlen);
				if(third_colon>=0)
				{
					third_colon-=numberstart+numberlen;
					err_msg.replace(0,1,"(");
				}
				const char *err_warn	=is_error?"error":"warning";
				if(third_colon>=0)
					err_msg.replace(third_colon,1,base::stringFormat("): %s C7555: ",err_warn).c_str());
#if 0
				int at_pos=(int)err_msg.find("0(");
				while(at_pos>=0)
				{
					int end_brk=(int)err_msg.find(")",at_pos);
					if(end_brk<0)
						break;
					std::string l=err_msg.substr(at_pos+2,end_brk-at_pos-2);
					int num=atoi(l.c_str());
					string filename=sourceFilesUtf8[num];
					err_msg.replace(at_pos,end_brk-at_pos,filename+"("+base::stringFormat("%d",n.line)+") ");
					at_pos=(int)err_msg.find("0(");
				}
#endif
				int filenumber=atoi(filenumber_str.c_str());
				string filename=sourceFilesUtf8[filenumber];
				std::string err_line	=filename+err_msg;
				//base::stringFormat("%s(%d): %s G1000: %s",filename.c_str(),line,err_warn,err_msg.c_str());
					//(n.filename+"(")+n.line+"): "+err_warn+" G1000: "+err_msg;
				return err_line;
			}
		}
		void printShaderInfoLog(GLuint sh,const vector<string> &sourceFilesUtf8)
		{
			int infologLength = 0;
			int charsWritten  = 0;
			char *infoLog=NULL;

			glGetShaderiv(sh, GL_INFO_LOG_LENGTH,&infologLength);

			if (infologLength > 1)
			{
				infoLog = (char *)malloc(infologLength);
				glGetShaderInfoLog(sh, infologLength, &charsWritten, infoLog);
				std::string info_log=infoLog;
				printShaderInfoLog(info_log,sourceFilesUtf8);
				free(infoLog);
			}
		}
		void printShaderInfoLog(const std::string &info_log,const vector<string> &sourceFilesUtf8)
		{
			//if(info_log.find("No errors")>=info_log.length())
			{
				int pos=0;
				int next=(int)info_log.find('\n',pos+1);
				while(next>=0)
				{
					std::string line		=info_log.substr(pos,next-pos);
					std::string error_line	=RewriteErrorLine(line,sourceFilesUtf8);
					if(error_line.length())
						std::cerr<<error_line.c_str()<<std::endl;
					pos=next;
					next=(int)info_log.find('\n',pos+1);
				}
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
					std::cerr<<info_log.c_str()<<std::endl;
				}
				else if(info_log.find("WARNING")<info_log.length())
					std::cout<<infoLog<<std::endl;
				free(infoLog);
			}
			GL_ERROR_CHECK
		}
		vector<string> effectSourceFilesUtf8;
		void printEffectLog(GLint effect)
		{
   			std::string log		=glfxGetEffectLog(effect);
			std::cerr<<log.c_str()<<std::endl;
		}
		
	}
}
