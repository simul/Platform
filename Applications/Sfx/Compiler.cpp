#undef _HAS_STD_BYTE
#include "Compiler.h"
#include "SfxEffect.h"
#include "StringFunctions.h"
#include "StringToWString.h"
#include "FileLoader.h"
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <sstream>	// for ostringstream
#include <cstdio>
#include <cassert>
#include <fstream>
#include <iostream>
#include <regex>
#include <functional>


#ifndef _MSC_VER
typedef int errno_t;
#include <errno.h>
#endif
#ifdef _MSC_VER
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Sfx.h"
#include "SfxClasses.h"

#ifdef _MSC_VER
#define YY_NO_UNISTD_H
#include <windows.h>
#include <direct.h>
#else
typedef struct stat Stat;
#endif
#include "GeneratedFiles/SfxScanner.h"
#include "SfxProgram.h"
#include "StringFunctions.h"
#include "StringToWString.h"
#include "SfxEffect.h"
#include "SfxErrorCheck.h"
#include "Preprocessor.h"

using namespace std;
typedef std::function<void(const std::string &)> OutputDelegate;

#if 0
static std::string WStringToUtf8(const wchar_t *src_w)
{
	int src_length=(int)wcslen(src_w);
#ifdef _MSC_VER
	int size_needed = WideCharToMultiByte(CP_UTF8, 0,src_w, (int)src_length, NULL, 0, NULL, NULL);
#else
	int size_needed=2*src_length;
#endif
	char *output_buffer = new char [size_needed+1];
#ifdef _MSC_VER
	WideCharToMultiByte (CP_UTF8,0,src_w,(int)src_length,output_buffer, size_needed, NULL, NULL);
#else
	wcstombs(output_buffer, src_w, (size_t)size_needed );
#endif
	output_buffer[size_needed]=0;
	std::string str_utf8=std::string(output_buffer);
	delete [] output_buffer;
	return str_utf8;
}
#endif
static void FixRelativePaths(std::string &str,const std::string &sourcePathUtf8)
{
	int pos=0;
	int eol=(int)str.find("\n");
	if(eol<0)
		eol=(int)str.length();
	while(eol>=0)
	{
		string line=str.substr(pos,eol-pos);
		if(line[0]=='.')
		{
			line=sourcePathUtf8+line;
			str.replace(pos,eol-pos,line);
		}
		pos=(int)str.find("\n",pos+1);
		if(pos<0)
			pos=(int)str.length();
		else
			pos++;
		eol=(int)str.find("\n",pos);
	}
}

bool RunDOSCommand(const wchar_t *wcommand, const string &sourcePathUtf8, ostringstream& log,const SfxConfig &sfxConfig, OutputDelegate outputDelegate)
{
	bool has_errors=false;
#ifndef _MSC_VER
	int res=system(WStringToString(wcommand).c_str());
	has_errors=(res!=0);
#else
	bool pipe_compiler_output=true;
	STARTUPINFOW startInfo;
	PROCESS_INFORMATION processInfo;
	ZeroMemory( &startInfo, sizeof(startInfo) );
	startInfo.cb = sizeof(startInfo);
	ZeroMemory( &processInfo, sizeof(processInfo) );
	wchar_t com[7500];
	wcscpy_s(com,wcommand);
	startInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	startInfo.wShowWindow = SW_HIDE;

	HANDLE hReadOutPipe = NULL;
	HANDLE hWriteOutPipe = NULL;
	HANDLE hReadErrorPipe = NULL;
	HANDLE hWriteErrorPipe = NULL;
	SECURITY_ATTRIBUTES secAttrib; 
// Set the bInheritHandle flag so pipe handles are inherited. 
	
	if(pipe_compiler_output)
	{
		secAttrib.nLength = sizeof(SECURITY_ATTRIBUTES); 
		secAttrib.bInheritHandle = TRUE; 
		secAttrib.lpSecurityDescriptor = NULL; 
		if(!CreatePipe( &hReadOutPipe, &hWriteOutPipe, &secAttrib, 1200 )||!CreatePipe( &hReadErrorPipe, &hWriteErrorPipe, &secAttrib, 1200 ))
		{
			SFX_BREAK("Failed to create pipes.");
		}
	}

	startInfo.hStdOutput	= hWriteOutPipe;
	startInfo.hStdError		= hWriteErrorPipe;
	if(!CreateProcessW( NULL,				// No module name (use command line)
						com,				// Command line
						NULL,				// Process handle not inheritable
						NULL,				// Thread handle not inheritable
						TRUE,				// Set handle inheritance to FALSE
						CREATE_NO_WINDOW,	//CREATE_NO_WINDOW,	// No nasty console windows
						NULL,				// Use parent's environment block
						NULL,				// Use parent's starting directory 
						&startInfo,			// Pointer to STARTUPINFO structure
						&processInfo )		// Pointer to PROCESS_INFORMATION structure
					)
	{
		SFX_CERR << "Failed to create process:" << WStringToUtf8(com) <<  std::endl;
		return false;
	}
	// Wait until child process exits.
	if(processInfo.hProcess==nullptr)
	{
		std::cerr<<"Error: Could not find the executable for "<<WStringToUtf8(com)<<std::endl;
		return false;
	}
	HANDLE WaitHandles[] = {
			processInfo.hProcess, hReadOutPipe, hReadErrorPipe
		};

	const DWORD BUFSIZE = 4096;
	BYTE buff[BUFSIZE];
	while (1)
	{
		DWORD dwBytesRead, dwBytesAvailable=100000;
		DWORD numObjects = pipe_compiler_output ? 3 : 1;
		DWORD dwWaitResult = WaitForMultipleObjects(numObjects, WaitHandles, FALSE, 20000L);

		// Read from the pipes...
		if(pipe_compiler_output)
		{
			while( PeekNamedPipe(hReadOutPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
			  ReadFile(hReadOutPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
			  std::string str((char*)buff, (size_t)dwBytesRead);
			  outputDelegate(str);
			}
			while( PeekNamedPipe(hReadErrorPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
				ReadFile(hReadErrorPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
				std::string str((char*)buff, (size_t)dwBytesRead);
				outputDelegate(str);
				// Force to failed if error...
				if(sfxConfig.failOnCerr)
					has_errors = true;
			}
		}
		// Process is done, or we timed out:
		if (dwWaitResult == WAIT_TIMEOUT)
		{
			SFX_CERR << "Timeout executing " << com << std::endl;
			has_errors = true;
		}
		if (dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_TIMEOUT)
			break;
		//if (dwWaitResult > WAIT_OBJECT_0 && dwWaitResult < WAIT_OBJECT_0 + numObjects)
		//{
			//SFX_CERR << "Pipe closed: " << dwWaitResult << std::endl;
			//pipe_compiler_output = false;
		//}
		if (dwWaitResult == WAIT_FAILED)
		{
			DWORD err = GetLastError();
			char* msg;
			// Ask Windows to prepare a standard message for a GetLastError() code:
			if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL))
				break;

			SFX_CERR<<"Error message: "<<msg<<std::endl;
			{
				CloseHandle( processInfo.hProcess );
				CloseHandle( processInfo.hThread );
				exit(21);
			}
		}
	}
	
	DWORD exitCode=0;
	if(!GetExitCodeProcess(processInfo.hProcess, &exitCode))
		return false;
	if(exitCode!=0&&exitCode!= 0xc0000005)//0xc0000005 is a bullshit error dxc occasionally throws up for no good reason.
	{
		SFX_CERR << "Exit code: 0x" <<std::hex<< (int)exitCode << std::endl;
		has_errors=true;
	}
	//WaitForSingleObject( processInfo.hProcess, INFINITE );
 	CloseHandle( processInfo.hProcess );
	CloseHandle( processInfo.hThread );

	if(has_errors)
		TerminateProcess(processInfo.hProcess,1);
#endif
	if(has_errors)
		return false;
	return true;
}

using namespace sfx;
using std::string;
extern std::vector<std::string> extra_arguments;

string ToString(PixelOutputFormat pixelOutputFormat)
{
	switch(pixelOutputFormat)
	{
	case FMT_UNKNOWN:
		return "";
	case FMT_32_GR:
		return "32gr";
	case FMT_32_AR:
		return "32ar";
	case FMT_FP16_ABGR:
		return "float16abgr";
	case FMT_UNORM16_ABGR:
		return "unorm16abgr";
	case FMT_SNORM16_ABGR:
		return "snorm16abgr";
	case FMT_UINT16_ABGR:
		return "uint16abgr";
	case FMT_SINT16_ABGR:
		return "sint16abgr";
	case FMT_32_ABGR:
		return "float32abgr";
	default:
		return "invalid";
	}
}

string ToPragmaString(PixelOutputFormat pixelOutputFormat)
{
	switch(pixelOutputFormat)
	{
	case FMT_UNKNOWN:
		return "FMT_UNKNOWN";
	case FMT_32_GR:
		return "FMT_32_GR";
	case FMT_32_AR:
		return "FMT_32_AR";
	case FMT_FP16_ABGR:
		return "FMT_FP16_ABGR";
	case FMT_UNORM16_ABGR:
		return "FMT_UNORM16_ABGR";
	case FMT_SNORM16_ABGR:
		return "FMT_SNORM16_ABGR";
	case FMT_UINT16_ABGR:
		return "FMT_UINT16_ABGR";
	case FMT_SINT16_ABGR:
		return "FMT_SINT16_ABGR";
	case FMT_32_ABGR:
		return "FMT_32_ABGR";
	default:
		return "FMT_UNKNOWN";
	}
}


static int do_mkdir(const wchar_t *path_w)
{
	int			 status = 0;
#ifdef _MSC_VER
	struct _stat64i32			st;
	std::wstring wstr=(path_w);
	if (_wstat (wstr.c_str(), &st) != 0)
#else
	std::string path_utf8=WStringToString(path_w);
	Stat			st;
	if (stat(path_utf8.c_str(), &st)!=0)
#endif
	{
		/* Directory does not exist. EEXIST for race condition */
#ifdef _MSC_VER
		if (_wmkdir(wstr.c_str()) != 0 && errno != EEXIST)
#else
		if (mkdir(path_utf8.c_str(),S_IRWXU) != 0 && errno != EEXIST)
#endif
			status = -1;
	}
	else if (!(st.st_mode & S_IFDIR))
	{
		//errno = ENOTDIR;
		status = -1;
	}
	errno=0;
	return(status);
}
static int nextslash(const std::wstring &str,int pos)
{
	int slash=(int)str.find(L'/',pos);
	int back=(int)str.find(L'\\',pos);
	if(slash<0||(back>=0&&back<slash))
		slash=back;
	return slash;
}
static int mkpath(const std::wstring &filename_utf8)
{
	int status = 0;
	int pos=1;// skip first / in Unix pathnames.
	while (status == 0 && (pos = nextslash(filename_utf8,pos))>=0)
	{
		status = do_mkdir(filename_utf8.substr(0,pos).c_str());
		pos++;
	}
	return (status);
}

string FillInVariablesSM51(const string &src,CompiledShader *shader)
{
	std::regex param_re("\\{shader_model\\}");
	string ret=src;
	if (shader)
	{
		std::string sm = "";
		switch (shader->shaderType)
		{
		case ShaderType::VERTEX_SHADER:
			sm = "vs_5_1";
			break;
		case ShaderType::FRAGMENT_SHADER:
			sm = "ps_5_1";
			break;
		case ShaderType::COMPUTE_SHADER:
			sm = "cs_5_1";
			break;
		default:
			sm = "null_null";
			break;
		}
		ret = std::regex_replace(src, param_re, sm);
	}
	return ret;
}


string FillInVariable(const string &src, const std::string &var,const std::string &rep)
{
	std::regex param_re(string("\\{")+var+"\\}");
	string ret = std::regex_replace(src, param_re, rep);
	return ret;
}

string FillInVariables(const string &src, CompiledShader *shader)
{
	std::regex param_re("\\{shader_model\\}");
	string ret = src;
	if (shader)
		ret = std::regex_replace(src, param_re, shader->m_profile);
	return ret;
}

void ReplaceRegexes(string &src, const std::map<string,string> &replace)
{
	for (auto i : replace)
	{
		std::regex param_re(std::string("\\b")+i.first+"\\b");
		src = std::regex_replace(src, param_re, i.second);
	}
}

wstring BuildCompileCommand(CompiledShader *shader,const SfxConfig &sfxConfig,const  SfxOptions &sfxOptions,wstring targetDir,wstring outputFile,
							wstring tempFilename,ShaderType t, PixelOutputFormat pixelOutputFormat)
{
	// Check if we are generating GLSL 
	bool isGlSL = sfxConfig.sourceExtension == "glsl";
	wstring command;
	
	string stageName = "NO_STAGES_IN_JSON";
	// Add the exe path
	auto st=sfxConfig.stages.find((int)t);
	// If there are any stages explicit in the json file, we must find one. Otherwise, assume {stage} is not in the command.
	if(sfxConfig.stages.size())
	{
		if (st != sfxConfig.stages.end())
			stageName=st->second;
		else
			return L"";
	}
	std::string currentCompiler = FillInVariable(sfxConfig.compiler,"stage",stageName);
	command +=  Utf8ToWString(currentCompiler) ;

	// Add additional options (requested from the XX.json)
	string options;
	if (sfxConfig.forceSM51)
	{
		options = FillInVariablesSM51(sfxConfig.defaultOptions, shader);
	}
	else
	{
		options = FillInVariables(sfxConfig.defaultOptions, shader);
	}
	command += wstring(L" ") + Utf8ToWString(options);

	// Add extra arguments  (this have the include dirs)
	for (auto i : extra_arguments)
	{
		command += L" ";
		std::string str=i;
		size_t Ipos = str.find("-I");
		if(sfxConfig.includeOption.length()>0&&Ipos==0)
		{
			string newArgument;
			str.replace(str.begin(), str.begin() + 2,sfxConfig.includeOption);
			size_t pos = str.find(';');
			str.replace(str.begin() + pos, str.begin() + pos + 1, ",");
		}
		command += Utf8ToWString(str);	
	}
	command += L" ";

	// Add entry point option
	if (sfxConfig.entryPointOption.length()&&shader->shaderType!=RAY_GENERATION_SHADER&&shader->shaderType!=CLOSEST_HIT_SHADER&&shader->shaderType!=ANY_HIT_SHADER&&shader->shaderType!=MISS_SHADER)
		command += Utf8ToWString(std::regex_replace(sfxConfig.entryPointOption, std::regex("\\{name\\}"), shader->entryPoint)) + L" ";

	string filename_root=WStringToString(outputFile);
	size_t dot_pos=filename_root.find_last_of(".");
	if(dot_pos<filename_root.size())
		filename_root=filename_root.substr(0,dot_pos);
	if(sfxOptions.debugInfo)
	{
		command+=Utf8ToWString(sfxConfig.debugOption)+L" ";
		if (sfxConfig.debugOutputFileOption.length())
			command += Utf8ToWString(std::regex_replace(sfxConfig.debugOutputFileOption, std::regex("\\{filename_root\\}"), filename_root)) + L" ";
	}
	else
	{
		if(sfxConfig.releaseOptions.length())
		{
			command+=Utf8ToWString(sfxConfig.releaseOptions)+L" ";
		}
	}
	if(sfxConfig.optimizationLevelOption.length()&&sfxOptions.optimizationLevel>=0)
	{
		command += Utf8ToWString(sfxConfig.optimizationLevelOption);
		char ostr[10];
		sprintf(ostr,"%d",sfxOptions.optimizationLevel);
		command += Utf8ToWString(ostr);
		command += L" ";
	}
	if (sfxConfig.outputOption.size())
	{
		command += Utf8ToWString(sfxConfig.outputOption);
		command += L"\"";
		command += outputFile;
		command += L"\" ";
	}
	// Input argument
	// The switch compiler needs an extension
/*	if (isGlSL)
	{
		switch (t)
		{
		case sfx::EXPORT_SHADER:
		case sfx::VERTEX_SHADER:
			command += L"--vertex-shader=";
			break;
		case sfx::FRAGMENT_SHADER:
			command += L"--pixel-shader=";
			break;
		case sfx::COMPUTE_SHADER:
			command += L"--compute-shader=";
			break;
		}
	}*/
	command += L"\"";
	command += tempFilename.c_str();
	command += L"\"";
	
	return command;
}

bool RewriteOutput(const SfxConfig &sfxConfig
	,const SfxOptions &sfxOptions, string sourcePathUtf8,const map<int,string> &fileList,ostringstream *log,string str)
{
	if(sfxConfig.compilerMessageRegex.size())
	{
		try
		{
			std::regex re(sfxConfig.compilerMessageRegex.c_str(), std::regex_constants::icase | std::regex::extended);
			std::smatch base_match;
			while (std::regex_search(str, base_match, re))
			{
				str = std::regex_replace(str, re, sfxConfig.compilerMessageReplace + "\n");
				base_match = std::smatch();
			}
		}
		catch (std::exception &)
		{
		}
		catch (...)
		{
		}
	}
	if(!sfxConfig.lineDirectiveFilenames)
	{
		// If we have a number followed by a bracket at the start
		std::smatch base_match;
		std::regex re("([0-9]+)\\(([0-9?]+)\\):");
		while(std::regex_search(str,base_match,re))
		{
			string filenum=str.substr(base_match[1].first-str.begin(),base_match[1].length());
			int num=atoi(filenum.c_str());
			auto f=fileList.find(num);
			string filename="unknown file";
			if(f!=fileList.end())
			{
				filename=f->second;
			}
			str.replace(base_match[1].first,base_match[1].second,filename);
			base_match=std::smatch();
		}
	}
	bool has_errors=false;
	size_t pos = str.find("Error");
	if(pos>=str.length())
		pos = str.find("error");
	if(pos>=str.length())
		pos=str.find("failed");
	if(pos<str.length())
		has_errors=true;
	FixRelativePaths(str,sourcePathUtf8);
	int bracket_pos=(int)str.find("(");
	if(bracket_pos>0)
	{
		int close_bracket_pos	=(int)str.find(")",bracket_pos);
		int comma_pos			=(int)str.find(",",bracket_pos);
		if(comma_pos>bracket_pos&&comma_pos<close_bracket_pos)
		{
			str.replace(comma_pos,close_bracket_pos-comma_pos,"");
		}
	}
	(*log)<< str.c_str();
	return has_errors;
}

int Compile(CompiledShader *shader,const string &sourceFile,string targetFile,ShaderType t,PixelOutputFormat pixelOutputFormat,const string &sharedSource, ostringstream& sLog
		,const SfxConfig &sfxConfig
		,const SfxOptions &sfxOptions
		,map<int,string> fileList
		,std::ofstream &combinedBinary
		, BinaryMap &binaryMap
		,const Declaration* rtState )
{
	string filenameOnly = GetFilenameOnly( sourceFile);
	wstring targetFilename=StringToWString(filenameOnly);
	int pos=(int)targetFilename.find_last_of(L".");
	if(pos>=0)
		targetFilename=targetFilename.substr(0,pos);
	targetFilename+=L"_"+StringToWString(shader->m_functionName);

	string preamble = "";

	// Add the preamble:
	
	preamble += sfxConfig.preamble + "\n";
	if(t==COMPUTE_SHADER)
		preamble += sfxConfig.computePreamble;
	
	// Pssl recognizes the shader type using a suffix to the filename, before the .pssl extension:
	wstring shaderTypeSuffix;
	switch(t)
	{
	case EXPORT_SHADER:
		shaderTypeSuffix=L"ve";
		break;
	case VERTEX_SHADER:
		shaderTypeSuffix=L"vv";
		break;
	case TESSELATION_CONTROL_SHADER: //SetHullShader:
		break;
	case TESSELATION_EVALUATION_SHADER://SetDomainShader:
		break;
	case GEOMETRY_SHADER:
		shaderTypeSuffix=L"g";
		break;
	case FRAGMENT_SHADER:
		{
			if (rtState && pixelOutputFormat == FMT_UNKNOWN)
			{
				RenderTargetFormatState* rt = (RenderTargetFormatState*)rtState;
				for (int i = 0; i < 8; i++)
				{
					if (rt->formats[i] != FMT_UNKNOWN)
					{
						preamble += "#pragma PSSL_target_output_format(target " + std::to_string(i) + " ";
						preamble += ToPragmaString(rt->formats[i]);
						preamble += ")\n";
					}
				}
				shaderTypeSuffix = StringToWString(rt->name) + L"_p";
			}
			else if(pixelOutputFormat!=FMT_UNKNOWN)
			{
				string frm=ToString(pixelOutputFormat);
				preamble+="#pragma PSSL_target_output_format(default ";
				preamble+=ToPragmaString(pixelOutputFormat);
				preamble+=")\n";
				if(frm=="invalid")
					return 0;
				if(frm.size())
					shaderTypeSuffix=StringToWString(frm)+L"_";
				shaderTypeSuffix+=L"p";
			}
		}
		break;
	case COMPUTE_SHADER:
		shaderTypeSuffix=L"c";
		break;
	default:
		break;
	};
	if(shaderTypeSuffix.length())
		targetFilename+=L"_"+shaderTypeSuffix;

	// Add the root signature (we want to keep it at the top of the file):
	if (!sfxConfig.graphicsRootSignatureSource.empty())
	{
		FileLoader fileLoader;
		const char* rsSourcePath = fileLoader.FindFileInPathStack(sfxConfig.graphicsRootSignatureSource.c_str(), sfxConfig.shaderPaths);
		if (!rsSourcePath)
		{
			std::cerr << "This platform requires a rootsignature file but no file was found in the shader paths" << std::endl;
			return 0;
		}
		std::ifstream rootSrcFile(rsSourcePath);
		std::string rootSrc((std::istreambuf_iterator<char>(rootSrcFile)), (std::istreambuf_iterator<char>()));
		preamble += rootSrc;
	}
	wstring tempFilename ;
	if(sfxOptions.intermediateDirectory.length())
		tempFilename+= StringToWString(sfxOptions.intermediateDirectory+ "/");

	tempFilename+=targetFilename+wstring(L".")+Utf8ToWString(sfxConfig.sourceExtension);
	
	char buffer[_MAX_PATH];
	string wd="";
	#ifdef _MSC_VER
	if(_getcwd(buffer,_MAX_PATH))
	#else
	if(getcwd(buffer,_MAX_PATH))
	#endif
		 wd=string(buffer)+"/";
	string tempf=WStringToUtf8(tempFilename);
	if(tempf.find(":")>=tempf.length())
		tempf=wd+"/"+tempf;
	find_and_replace(tempf,"\\","/");
	find_and_replace(wd,"\\","/");
	
	if (gEffect->GetOptions()->disableLineWrites)
		preamble += "//";

	size_t lineno=count_lines_in_string(preamble)+1;// add 1 because we're inserting a line into the same file.
	preamble+=stringFormat("#line %d \"%s\"\n",lineno,tempf.c_str());

	preamble += shader->m_preamble;

	if(t==COMPUTE_SHADER)
	{
		if (sfxConfig.computePreamble.empty())
		{
			preamble += "#define USE_COMPUTE_SHADER 1\n";
		}
	}
	for(auto i:sfxConfig.define)
	{
		preamble+="#define "+i.first;
		preamble+=" "+i.second;
		preamble+="\n";
	}
	// Shader source!
	string src = preamble;
	src += sharedSource;
	src+=shader->m_augmentedSource;
	ReplaceRegexes(src, sfxConfig.replace);
	const char *strSrc=src.c_str();

	wstring targetDir=StringToWString(GetDirectoryFromFilename(targetFile));
	if(targetDir.size())
		targetDir+=L"/";
	mkpath(targetDir);
	mkpath(StringToWString(sfxOptions.intermediateDirectory)+L"/");
	
	#ifdef _MSC_VER
	ofstream ofs(tempFilename.c_str());
	#else
	ofstream ofs(WStringToUtf8(tempFilename).c_str());
	#endif
	ofs.write(strSrc, strlen(strSrc));
	ofs.close();

#ifdef _MSC_VER
	// Nowe delete the corresponding sdb's
	wstring sdbFile=tempFilename.substr(0,tempFilename.length()-5);
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW((sdbFile+L"*.sdb").c_str(), &fd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			DeleteFileW(((targetDir)+ fd.cFileName).c_str());
		} while (FindNextFileW(hFind, &fd));
		FindClose(hFind);
	}
#endif
	wstring outputFile = ((sfxOptions.wrapOutput ? StringToWString(sfxOptions.intermediateDirectory) : targetDir) + L"/") + (targetFilename + L".") + Utf8ToWString(sfxConfig.outputExtension);
	// Add the output filename
	int slash = (int)outputFile.find_last_of(L"/");
	int backslash = (int)outputFile.find_last_of(L"\\");
	if (backslash > slash)
		slash = backslash;
	string sbf = WStringToUtf8(outputFile.substr(slash + 1, outputFile.length() - slash - 1).c_str());
	if (t == FRAGMENT_SHADER)
	{
		shader->sbFilenames[pixelOutputFormat] = sbf;
	}
	else if (t == EXPORT_SHADER)
	{
		shader->sbFilenames[1] = sbf;
	}
	else
		shader->sbFilenames[0] = sbf;
	// Get the compile command
	wstring psslc = BuildCompileCommand
	(
		shader,
		sfxConfig,
		sfxOptions,
		targetDir,
		outputFile,
		tempFilename,
		t,
		pixelOutputFormat
	);
	ostringstream log;
	// If no compiler provided, we can return now (perhaps we are only interested in
	// the shader source)
	if (psslc.empty()||sfxConfig.compiler.empty())
	{
		if(sfxOptions.verbose)
			std::cout<<WStringToUtf8(tempFilename).c_str()<<"\n";
		// But let's in that case, wrap up the GENERATED SOURCE in sfxb:
		if (sfxOptions.wrapOutput)
		{
			//concatenate
#ifdef _MSC_VER
			std::ifstream if_c(tempFilename.c_str(), std::ios_base::binary);
#else
			std::ifstream if_c(WStringToUtf8(tempFilename).c_str(), std::ios_base::binary);
#endif
			std::streampos startp = combinedBinary.tellp();
			combinedBinary << if_c.rdbuf();
			std::streampos endp = combinedBinary.tellp();
			size_t sz=endp - startp;
			if(!sz)
			{
				SFX_BREAK("Empty output binary");
				exit(1001);
			}
			binaryMap[sbf] = std::make_tuple(startp, sz);
		}
		return true;
	}
	if(sfxOptions.verbose)
		std::cout<<WStringToUtf8(psslc).c_str()<<"\n";

	// Run the provided .exe! 
	OutputDelegate cc=std::bind(&RewriteOutput,sfxConfig,sfxOptions,wd,fileList,&log,std::placeholders::_1);

	bool res=RunDOSCommand(psslc.c_str(),wd,log,sfxConfig,cc);
	if (res)
	{
		if (sfxOptions.verbose)
		{
			std::cout << tempf.c_str() << "(0): info: Temporary shader source file." << std::endl;
		}
		if (log.str().find("warning") < log.str().length())
		{
			std::cerr << (sourceFile).c_str() << "(0): Warning: warnings compiling " << shader->m_functionName.c_str() << std::endl;
		}
		if (log.str().find("error") < log.str().length())
		{
			res = 0;
		}
		std::cerr << log.str() << std::endl;
		// If we provide invalid args to fxc:
		if (log.str().find("Unknown or invalid option") < log.str().length())
		{
			std::cerr << log.str() << std::endl;
			res = 0;
		}
		if (sfxOptions.wrapOutput)
		{
			//concatenate
			#ifdef _MSC_VER
			std::ifstream if_c(outputFile.c_str(), std::ios_base::binary);
			#else
			std::ifstream if_c(WStringToUtf8(outputFile).c_str(), std::ios_base::binary);
			#endif
			std::streampos startp=combinedBinary.tellp();
			combinedBinary << if_c.rdbuf();
			std::streampos endp = combinedBinary.tellp();
			size_t sz=endp - startp;
			if(!sz)
			{
				std::cerr << log.str() << std::endl;
				std::cerr << "Empty output binary" << outputFile.c_str()<< std::endl;
				SFX_BREAK("Empty output binary");
				exit(1);
			}
			binaryMap[sbf] = std::make_tuple(startp, sz);
		}
	}
	else
	{
		std::cerr << sourceFile.c_str() << "(0): error: failed building shader " << shader->m_functionName.c_str()<<std::endl;
		if (sfxOptions.verbose)
			std::cerr << tempf.c_str() << "(0): info: generated temporary shader source file for " << shader->m_functionName.c_str()<<std::endl;
		std::cerr << log.str() << std::endl;
	}

	return res;
}
