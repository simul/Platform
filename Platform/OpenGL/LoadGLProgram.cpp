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
#include "LoadGLProgram.h"
#include "SimulGLUtilities.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/DefaultFileLoader.h"

#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#define DebugBreak()
#endif

using namespace simul;
using namespace base;
using namespace opengl;
using namespace std;

namespace simul
{
	namespace opengl
	{
		std::vector<std::string> texturePathsUtf8;
		std::vector<std::string> shaderPathsUtf8;
		void PushShaderPath(const char *path_utf8)
		{
			shaderPathsUtf8.push_back(std::string(path_utf8)+"/");
		}
		static int LineCount(const std::string &str)
		{
			int pos=(int)str.find('\n');
			int count=1;
			while(pos>=0)
			{
				pos=(int)str.find('\n',pos+1);
				count++;
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
			//! Add a tie point for the start of this file, plus one to mark where its parent restarts.
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
					int next_line_in_parent	=p.local+global_line-p.global;
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

		std::string loadShaderSource(const char *filename_utf8)
		{
			void *shader_source=NULL;
			unsigned fileSize=0;
		ERRNO_CHECK
			simul::base::FileLoader::GetFileLoader()->AcquireFileContents(shader_source,fileSize,filename_utf8,true);
		ERRNO_CHECK
			if(!shader_source)
			{
				std::cerr<<"\nERROR:\tShader file "<<filename_utf8<<" not found, exiting.\n";
				std::cerr<<"\n\t\tShader paths are:"<<std::endl;
				for(int i=0;i<(int)shaderPathsUtf8.size();i++)
					std::cerr<<" "<<shaderPathsUtf8[i].c_str()<<std::endl;
				DebugBreak();
				std::cerr<<"exit(1)"<<std::endl;
				exit(1);
			}
			ERRNO_CHECK
			std::string str((const char*)shader_source);
			simul::base::FileLoader::GetFileLoader()->ReleaseFileContents(shader_source);
			ERRNO_CHECK
			// Change Windows-style CR-LF's to simple Unix-style LF's
			base::find_and_replace( str,"\r\n","\n");
			// Convert any left-over CR's into LF's
			base::find_and_replace( str,"\r","\n");
			ERRNO_CHECK
			return str;
		}
		void ProcessIncludes(string &src,string &filenameUtf8,bool line_source_filenames,vector<string> &sourceFilesUtf8)
		{
			size_t pos			=0;
			// problem: if we insert this at line 0, SOME Glsl compilers will moan about #version not being the first line.
			//src					=src.insert(0,base::stringFormat("#line 1 \"%s\"\n",filenameUtf8.c_str()));

			// instead we find '#version' and insert after that.
			int first			=(int)src.find("#version");
			if(first>=0)
				pos				=src.find('\n',first)+1;
			// Is this file in the source list?
			int index			=(int)(find(sourceFilesUtf8.begin(), sourceFilesUtf8.end(), filenameUtf8)-sourceFilesUtf8.begin());
			// And SOME Glsl compilers will give a syntax error
			// when you provide a source filename with the #line directive, as it's not in the GLSL spec:
			if(line_source_filenames)
				src				=src.insert(pos,base::stringFormat("#line 1 \"%s\"\n",filenameUtf8.c_str()));
			else
			{
				if(index==(int)sourceFilesUtf8.size())
					sourceFilesUtf8.push_back(filenameUtf8);
				src				=src.insert(pos,base::stringFormat("#line 1 %d\n",index));
			}
			pos					=src.find('\n',pos+1);
			int next			=(int)src.find('\n',pos+1);
			int line_number		=0;
			while(next>=0)
			{
				string line						=src.substr(pos+1,next-pos);
				int inc							=(int)line.find("#include");
				if(inc==0)
				{
					int start_of_line			=(int)pos+1;
					pos+=9;
					int eol						=(int)src.find("\n",pos+1);
					string include_file			=line.substr(10,line.length()-12);
					src							=src.insert(start_of_line,"//");
					// Go to after the newline at the end of the #include statement. Two for "//" and one for "\n"
					eol							+=3;
					string includeFilenameUtf8	=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(include_file.c_str(),shaderPathsUtf8);
					string newsrc				=loadShaderSource(includeFilenameUtf8.c_str());
					ProcessIncludes(newsrc,includeFilenameUtf8,line_source_filenames,sourceFilesUtf8);
					
					//First put the "restore" #line directive after the commented-out #include.
					if(line_source_filenames)
						src=src.insert(eol,base::stringFormat("\n#line %d \"%s\"\n",line_number,filenameUtf8.c_str()));
					else
						src=src.insert(eol,base::stringFormat("\n#line %d %d\n",line_number,index));
					// Now insert the contents of the #include file before the closing #line directive.
					src								=src.insert(eol,newsrc);
					next							+=(int)newsrc.length();
					line_number--;
				}
				else
					line_number++;
				pos=next;
				next=(int)src.find('\n',pos+1);
			}
		}
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
			if(errpos>=0)
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
			return "";
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
			if(info_log.find("No errors")>=info_log.length())
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
		GLuint CompileShaderFromSource(GLuint sh,const std::string &source,const map<string,string> &defines,const vector<string> &sourceFilesUtf8)
		{
			std::string src=source;
		/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
			const int MAX_STRINGS=12;
			const char *strings[MAX_STRINGS];
			int start_of_line=0;
			// We can insert #defines, but only AFTER the #version string.
			int pos=(int)src.find("#version");
			if(pos>=0)
			{
				start_of_line=(int)src.find("\n",pos)+1;
				if(start_of_line<0)
					start_of_line=(int)src.length();
			}
			for(map<string,string>::const_iterator i=defines.begin();i!=defines.end();i++)
			{
				std::string def;
				def+="#define ";
				def+=i->first;
				def+=" ";
				def+=i->second;
				def+="\r\n";
				src=src.insert(start_of_line,def);
//				int start_line=(int)GetLineNumber(src,start_of_line);
				//filenameChart.add("defines",start_line,def);
			}
			int lenOfStrings[MAX_STRINGS];
			strings[0]		=src.c_str();
			lenOfStrings[0]	=(int)strlen(strings[0]);
			glShaderSource(sh,1,strings,NULL);
		GL_ERROR_CHECK
			if(!sh)
				return 0;
			glCompileShader(sh);
		GL_ERROR_CHECK
			printShaderInfoLog(sh,sourceFilesUtf8);
			int result=1;
			glGetShaderiv(sh,GL_COMPILE_STATUS,&result);
		GL_ERROR_CHECK
			if(!result)
			{
				return 0;
			}
			return sh;
		}
		vector<string> effectSourceFilesUtf8;
		void printEffectLog(GLint effect)
		{
   			std::string log		=glfxGetEffectLog(effect);
			std::cerr<<log.c_str();
//			printShaderInfoLog(log,effectSourceFilesUtf8);
		}
		
		GLuint CompileShaderFromSource(GLuint sh,const std::string &source,const map<string,string> &defines)
		{
			vector<string> sourceFilesUtf8;
			return CompileShaderFromSource(sh,source,defines,sourceFilesUtf8);
		}
		
		GLint CreateEffect(const char *filename_utf8,const std::map<std::string,std::string>&defines)
		{
			std::string filenameUtf8	=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,shaderPathsUtf8);
			if(!filenameUtf8.length())
				return 0;
			GLint effect=glfxGenEffect();
#if 1
			std::string src				=loadShaderSource(filenameUtf8.c_str());
			ProcessIncludes(src,filenameUtf8,false,effectSourceFilesUtf8);
			
			int pos=(int)src.find("\r\n");
			while(pos>=0)
			{
				src.replace(pos,2,"\n");
				pos=(int)src.find("\r\n",pos+1);
			}
			const char **filenames=new const char*[effectSourceFilesUtf8.size()+1];
			for(int i=0;i<effectSourceFilesUtf8.size();i++)
			{
				filenames[i]=effectSourceFilesUtf8[i].c_str();
			}
			filenames[effectSourceFilesUtf8.size()]=0;
			if(!glfxParseEffectFromTextSIMUL( effect, src.c_str(),filenames))
			{
   				std::string log = glfxGetEffectLog(effect);
   				std::cout << "Error parsing effect: " << log << std::endl;
				effect=-1;
			}
			delete filenames;
#else
			if (!glfxParseEffectFromFile(effect,filenameUtf8.c_str()))
			{
   				std::string log = glfxGetEffectLog(effect);
   				std::cout << "Error parsing effect: " << log << std::endl;
			}
#endif
			return effect;
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
			vertex_shader			=CompileShaderFromSource(vertex_shader	,vert_src,defines);
			fragment_shader			=CompileShaderFromSource(fragment_shader	,frag_src,defines);
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
			int result						=IDRETRY;
			GLuint vertex_shader			=0;
			GL_ERROR_CHECK
			while(result==IDRETRY)
			{
				vertex_shader				=LoadShader(vert_filename,defines);
 				if(!vertex_shader)
				{
					std::cerr<<vert_filename<<"(0): ERROR C1000: Shader failed to compile\n";
		#ifdef _MSC_VER
					std::string msg_text	=vert_filename;
					msg_text				+=" failed to compile. Edit shader and try again?";
					result					=MessageBoxA(NULL,msg_text.c_str(),"Simul",MB_RETRYCANCEL|MB_SETFOREGROUND|MB_TOPMOST);
					DebugBreak();
		#else
					break;
		#endif
				}
				else break;
			}
			GL_ERROR_CHECK
			glAttachShader(prog,vertex_shader);
			GL_ERROR_CHECK
			if(geom_filename)
			{
				GLuint geometry_shader	=LoadShader(geom_filename,defines);
				if(!geometry_shader)
				{
					std::cerr<<"ERROR:\tShader failed to compile\n";
					DebugBreak();
				}
				glAttachShader(prog,geometry_shader);
				GL_ERROR_CHECK
			}
			GL_ERROR_CHECK
			GLuint fragment_shader=0;
			result=IDRETRY;
			while(result==IDRETRY)
			{
				fragment_shader		=LoadShader(frag_filename,defines);
				if(!fragment_shader)
				{
					std::cerr<<frag_filename<<"(0): ERROR C1000: Shader failed to compile\n";
		#ifdef _MSC_VER
					std::string msg_text=frag_filename;
					msg_text+=" failed to compile. Edit shader and try again?";
					result=MessageBoxA(NULL,msg_text.c_str(),"Simul",MB_RETRYCANCEL|MB_SETFOREGROUND|MB_TOPMOST);
					DebugBreak();
		#else
					break;
		#endif
				}
				else break;
			}
			GL_ERROR_CHECK
			glAttachShader(prog,fragment_shader);
			GL_ERROR_CHECK
			glLinkProgram(prog);
			GL_ERROR_CHECK
			glUseProgram(prog);
			printProgramInfoLog(prog);
			return prog;
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
		GL_ERROR_CHECK
			std::string filenameUtf8	=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename,shaderPathsUtf8);
			std::string src				=loadShaderSource(filenameUtf8.c_str());
		GL_ERROR_CHECK
			vector<string> sourceFilesUtf8;
			ProcessIncludes(src,filenameUtf8,false,sourceFilesUtf8);
		GL_ERROR_CHECK
			GLuint sh=glCreateShader(shader_type);
		GL_ERROR_CHECK
			sh=CompileShaderFromSource(sh,src,defines,sourceFilesUtf8);
			GL_ERROR_CHECK
			return sh;
		}
	}
}
