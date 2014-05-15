#ifndef SIMUL_PLATFORM_COMMANDLINEPARAMS_H
#define SIMUL_PLATFORM_COMMANDLINEPARAMS_H
#include <string>
#include "Simul/Base/StringToWString.h"
struct ID3D11DeviceContext;
struct IDirect3DDevice9;
namespace simul
{
	namespace crossplatform
	{
		struct CommandLineParams
		{
			int quitafterframe;
			int win_w,win_h;
			int pos_x,pos_y;
			std::string seq_filename_utf8,sq_filename_utf8;
			std::string logfile_utf8;
			bool screenshot;
			std::string screenshotFilenameUtf8;
		};
		inline void GetCommandLineParams(crossplatform::CommandLineParams &commandLineParams,int argCount,const char **szArgList)
		{
			commandLineParams.pos_x=16;
			commandLineParams.pos_y=16;
			commandLineParams.quitafterframe=0;
			commandLineParams.win_h=480;
			commandLineParams.win_w=640;
	
			if (szArgList)
			{
				bool sc=false;
				for (int i = 0; i < argCount; i++)
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
					if(arg.find("SCREENSHOT")<arg.length())
					{
						commandLineParams.screenshot=true;
						sc=true;
					}
					unsigned pos=(unsigned)arg.find(":");
					if(pos<arg.length())
					{
						std::string left=arg.substr(0,pos);
						std::string right=arg.substr(pos+1,arg.length()-pos-1);
						int val=atoi(right.c_str());
						if(left.compare("-log")==0)
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
					}
					//-startx:960, -starty:20 -width:640 -height:360
				}
			}
		}
		
		inline void GetCommandLineParams(crossplatform::CommandLineParams &commandLineParams,int argCount,const wchar_t **szArgList)
		{
			char **args=new char *[argCount];
			for(int i=0;i<argCount;i++)
			{
				std::string str=base::WStringToString(szArgList[i]);
				args[i]=new char[str.length()+1];
				strcpy_s(args[i],str.length(),str.c_str());
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