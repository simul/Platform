#pragma once
#include <string>
#include <vector>
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/Export.h"

#if defined(UNIX) || defined(__linux__)
	#include <string.h>
	#define _strcpy(d,n,s) (strncpy(d,s,n))
	#define strcpy_s(d, n, s) (strncpy(d,s,n));
#endif

namespace platform
{
	namespace core
	{
		/// A simple structure to store the command-line parameters for an executable.
		struct PLATFORM_CORE_EXPORT CommandLineParams
		{
			CommandLineParams()
				:quitafterframe(0)
				,win_w(1280)
				,win_h(720)
				,pos_x(16)
				,pos_y(16)
				,screenshot(false)
				,dx11(false)
				,dx12(false)
				,opengl(false)
				,vulkan(false)
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
			bool dx11, dx12, opengl, vulkan;
			std::vector<std::string> strings;
		};
		/// Convert the inputs to an executable into a CommandLineParams struct.
		inline void GetCommandLineParams(CommandLineParams &commandLineParams,int argCount,const char **szArgList)
		{
			if (szArgList)
			{
				bool sc=false;
				// Start at 1 because we don't care about the .exe name
				for (int i = 1; i < argCount; i++)
				{
					std::string arg(szArgList[i]);
					auto FoundArg = [&](const std::string& value)->bool { return (arg.find(value) != std::string::npos); };

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
						if(left.compare("-test")==0)
						{
							commandLineParams.screenshot=true;
							commandLineParams.screenshotFilenameUtf8=right;
							commandLineParams.quitafterframe=50;
						}
					}
					else if(arg.find("SCREENSHOT")<arg.length()||arg.find("screenshot")<arg.length())
					{
						commandLineParams.screenshot=true;
						sc=true;
					}
					else if (FoundArg("-d3d11") || FoundArg("-D3D11") || FoundArg("-dx11") || FoundArg("-DX11"))
					{
						commandLineParams.dx11 = true;
					}
					else if (FoundArg("-d3d12") || FoundArg("-D3D12") || FoundArg("-dx12") || FoundArg("-DX12"))
					{
						commandLineParams.dx12 = true;
					}
					else if (FoundArg("-opengl") || FoundArg("-OPENGL") || FoundArg("-gl") || FoundArg("-GL"))
					{
						commandLineParams.opengl = true;
					}
					else if (FoundArg("-vulkan") || FoundArg("-VULKAN") || FoundArg("-vk") || FoundArg("-VK"))
					{
						commandLineParams.vulkan= true;
					}
					else
						commandLineParams.strings.push_back(arg);
				}
			}
		}
		
		/// Convert the inputs to an executable into a CommandLineParams struct.
		inline void GetCommandLineParams(CommandLineParams &commandLineParams,int argCount,const wchar_t **szArgList)
		{
			char **args=new char *[argCount];
			for(int i=0;i<argCount;i++)
			{
				std::string str=core::WStringToString(szArgList[i]);
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
