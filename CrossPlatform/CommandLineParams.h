#ifndef SIMUL_PLATFORM_COMMANDLINEPARAMS_H
#define SIMUL_PLATFORM_COMMANDLINEPARAMS_H
#include <string>
#include <vector>
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/Export.h"

namespace simul
{
	namespace crossplatform
	{
		/// A simple structure to store the command-line parameters for an executable.
		struct SIMUL_CROSSPLATFORM_EXPORT CommandLineParams
		{
			CommandLineParams()
				:quitafterframe(0)
				,win_w(1280)
				,win_h(720)
				,pos_x(16)
				,pos_y(16)
				,screenshot(false)
			{
			}
			bool operator()(const char *);
			int quitafterframe;
			int win_w,win_h;
			int pos_x,pos_y;
			std::string seq_filename_utf8,sq_filename_utf8;
			std::string logfile_utf8;
			bool screenshot;
			std::string screenshotFilenameUtf8;
			std::vector<std::string> strings;
		};
		/// Convert the inputs to an executable into a CommandLineParams struct.
		inline void GetCommandLineParams(crossplatform::CommandLineParams &commandLineParams,int argCount,const char **szArgList)
		{
			if (szArgList)
			{
				bool sc=false;
				// Start at 1 because we don't care about the .exe name
				for (int i = 1; i < argCount; i++)
				{
					std::string arg(szArgList[i]);
					if(arg.find(".seq")==arg.length()-4&&arg.length()>4)
						commandLineParams.seq_filename_utf8=arg;
					if(arg.find(".sq")==arg.length()-3&&arg.length()>3)
						commandLineParams.sq_filename_utf8=arg;
					if(sc)
					{
						commandLineParams.screenshotFilenameUtf8=arg;
						sc=false;
						continue;
					}
					unsigned pos=(unsigned)arg.find(":");
					if(pos>=arg.length())
						pos = (unsigned)arg.find("=");
					if(pos>=2&&pos<arg.length())
					{
						std::string left=arg.substr(0,pos);
						std::string right=arg.substr(pos+1,arg.length()-pos-1);
						int val=atoi(right.c_str());
						if(left.compare("-log")==0||left.compare("-logfile")==0)
							commandLineParams.logfile_utf8=right;
						if(left.compare("-startx")==0)
							commandLineParams.pos_x=val;
						if(left.compare("-starty")==0)
							commandLineParams.pos_y=val;
						if(left.compare("-width")==0)
							commandLineParams.win_w=val;
						if(left.compare("-height")==0)
							commandLineParams.win_h=val;
						if(left.compare("-quitafterframe")==0)
							commandLineParams.quitafterframe=val;
						if(left.compare("-screenshot")==0)
						{
							commandLineParams.screenshot=true;
							commandLineParams.screenshotFilenameUtf8=right;
						}
					}
					else if(arg.find("SCREENSHOT")<arg.length()||arg.find("screenshot")<arg.length())
					{
						commandLineParams.screenshot=true;
						sc=true;
					}
					else
						commandLineParams.strings.push_back(arg);
				}
			}
		}
		
		/// Convert the inputs to an executable into a CommandLineParams struct.
		inline void GetCommandLineParams(crossplatform::CommandLineParams &commandLineParams,int argCount,const wchar_t **szArgList)
		{
			char **args=new char *[argCount];
			for(int i=0;i<argCount;i++)
			{
				std::string str=base::WStringToString(szArgList[i]);
				int len=(int)str.length();
				args[i]=new char[len+1];
				strcpy_s(args[i],len+1,str.c_str());
				args[i][str.length()]=0;
			}
			GetCommandLineParams(commandLineParams,argCount,(const char **)args);
			for(int i=0;i<argCount;i++)
				delete [] args[i];
			delete [] args;
		}
	}
}
#endif