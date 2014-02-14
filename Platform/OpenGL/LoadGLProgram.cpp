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

static DefaultFileLoader fl;
static FileLoader *fileLoader=&fl;
namespace simul
{
	namespace opengl
	{
		std::string *shaderPathUtf8=NULL;
		std::string *texture_path=NULL;
		void SetFileLoader(simul::base::FileLoader *l)
		{
			fileLoader=l;
		}
		void SetShaderPath(const char *path_utf8)
		{
			if(path_utf8&&!shaderPathUtf8)
			{
				shaderPathUtf8=new std::string;
			}
			if(path_utf8)
				*shaderPathUtf8=path_utf8;
			else
				delete shaderPathUtf8;
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

		static int GetLineNumber(const std::string &str,int line_pos)
		{
			int pos=0;
			int count=0;
			while(pos>=0&&pos<=line_pos)
			{
				pos=(int)str.find('\n',pos+1);
				if(pos>=0&&pos<=line_pos)
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
					int pos=0;
					int next=(int)info_log.find('\n',pos+1);
					while(next>=0)
					{
						std::string line=info_log.substr(pos,next-pos);
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
							if(third_colon>=0)
							{
								numberstart	=second_colon+1;
								numberlen	=third_colon-second_colon-1;
							}
							else if((third_colon<0||numberlen>6)&&second_bracket>=0)
							{
								numberstart	=first_bracket+1;
								numberlen	=second_bracket-first_bracket-1;
							}
							else
							{
								pos=next;
								next=(int)info_log.find('\n',pos+1);
								continue;
							}
							std::string linestr=line.substr(numberstart,numberlen);
							std::string err_msg=line.substr(numberstart+numberlen+1,line.length()-numberstart-numberlen-1);
							int at_pos=(int)err_msg.find("0(");
							while(at_pos>=0)
							{
								int end_brk=(int)err_msg.find(")",at_pos);
								if(end_brk<0)
									break;
								std::string l=err_msg.substr(at_pos+2,end_brk-at_pos-2);
								int num=atoi(l.c_str());
								NameLine n=filenameChart.find(num);
								err_msg.replace(at_pos,end_brk-at_pos,n.filename+"("+base::stringFormat("%d",n.line)+") ");
								at_pos=(int)err_msg.find("0(");
							}
							int number=atoi(linestr.c_str());
							NameLine n=filenameChart.find(number);
							const char *err_warn=is_error?"error":"warning";
							std::cerr<<(*shaderPathUtf8).c_str()<<"/"<<n.filename.c_str()<<"("<<n.line<<"): "<<err_warn<<" G1000: "<<err_msg.c_str()<<std::endl;
						}
						pos=next;
						next=(int)info_log.find('\n',pos+1);
					}
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
					std::cerr<<info_log.c_str()<<std::endl;
				}
				else if(info_log.find("WARNING")<info_log.length())
					std::cout<<infoLog<<std::endl;
				free(infoLog);
			}
			GL_ERROR_CHECK
				}


		GLuint CompileShaderFromSource(GLuint sh,const std::string &source,const map<string,string> &defines,FilenameChart filenameChart)
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
				start_of_line=src.find("\n",pos)+1;
				if(start_of_line<0)
					start_of_line=src.length();
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
				int start_line=GetLineNumber(src,start_of_line);
				filenameChart.add("defines",start_line,def);
			}
			int lenOfStrings[MAX_STRINGS];
			strings[0]		=src.c_str();
			lenOfStrings[0]	=strlen(strings[0]);
			glShaderSource(sh,1,strings,NULL);
		GL_ERROR_CHECK
			if(!sh)
				return 0;
			glCompileShader(sh);
		GL_ERROR_CHECK
			printShaderInfoLog(sh,filenameChart);
			int result=1;
			glGetShaderiv(sh,GL_COMPILE_STATUS,&result);
		GL_ERROR_CHECK
			if(!result)
			{
				return 0;
			}
			return sh;
		}

		GLuint CompileShaderFromSource(GLuint sh,const std::string &source,const map<string,string> &defines)
		{
			FilenameChart filenameChart;
			return CompileShaderFromSource(sh,source,defines,filenameChart);
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
			GL_ERROR_CHECK
			GLuint prog						=glCreateProgram();
			int result=IDRETRY;
			GLuint vertex_shader=0;
			GL_ERROR_CHECK
			while(result==IDRETRY)
			{
				vertex_shader			=LoadShader(vert_filename,defines);
				if(!vertex_shader)
				{
					std::cerr<<vert_filename<<"(0): ERROR C1000: Shader failed to compile\n";
		#ifdef _MSC_VER
					std::string msg_text=vert_filename;
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

		std::string loadShaderSource(const char *filename_utf8)
		{
		/*  No vertex or fragment program should be longer than 512 lines by 255 characters. */
		//	const int MAX_LINES=512;
		//	const int MAX_LINE_LENGTH=256;   // 255 + NULL terminator
			//char shader_source[MAX_LINES*MAX_LINE_LENGTH];
			std::string filenameUtf8=*shaderPathUtf8;
			char last=0;
			if(shaderPathUtf8->length())
				last=filenameUtf8[shaderPathUtf8->length()-1];
			if(last!='/'&&last!='\\')
				filenameUtf8+="/";
			filenameUtf8+=filename_utf8;
	
			void *shader_source=NULL;
			unsigned fileSize=0;
			fileLoader->AcquireFileContents(shader_source,fileSize,filenameUtf8.c_str(),true);
			if(!shader_source)
			{
				std::cerr<<"\nERROR:\tShader file "<<filename_utf8<<" not found, exiting.\n";
				std::cerr<<"\n\t\tShader path is "<<shaderPathUtf8->c_str()<<", is this correct?\n";
				DebugBreak();
				exit(1);
			}
		/*	char *ptr=shader_source;
			while(!ifs.eof())
			{
				ifs.getline(ptr,MAX_LINE_LENGTH);
				int len=strlen(ptr);
				ptr+=len;
				*ptr++='\r';
				*ptr++='\n';
			}
			ifs.close();*/
			//if(ptr!=shader_source)
			//	ptr[-2]=0;
			std::string str((const char*)shader_source);
			fileLoader->ReleaseFileContents(shader_source);
			int pos=str.find("\n");
			while(pos>=0)
			{
				str.replace(pos,1,"\r\n");
				pos=str.find("\n",pos+2);
			}
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
		GL_ERROR_CHECK
			std::string src=loadShaderSource(filename);
		GL_ERROR_CHECK
			FilenameChart filenameChart;
			filenameChart.add(filename,0,src);
			// process #includes.
			std::vector<std::string> include_files;
			size_t pos=0;
			while((pos=src.find("#include",pos))<src.length())
			{
				if(pos>0&&src[pos-1]!='\n')
				{
					pos++;
					continue;
				}
				int start_of_line=pos;
				int start_line=GetLineNumber(src,pos);
				pos+=9;
				int n=src.find("\n",pos+1);
				int r=src.find("\r",pos+1);
				int eol=n;
				if(r>=0&&r<n)
					eol=r;
				std::string include_file=src.substr(pos,eol-pos);
				include_file=include_file.substr(1,include_file.length()-2);
				include_files.push_back(include_file);
				src=src.insert(start_of_line,"//");
				eol+=2;
				src=src.insert(eol,"\r\n");
				std::string newsrc=loadShaderSource(include_file.c_str());
				src=src.insert(eol+2,newsrc);
				filenameChart.add(include_file.c_str(),start_line+1,newsrc);
			}
		GL_ERROR_CHECK
			GLuint sh=glCreateShader(shader_type);
		GL_ERROR_CHECK
			sh=CompileShaderFromSource(sh,src,defines,filenameChart);
			GL_ERROR_CHECK
			return sh;
		}
	}
}
