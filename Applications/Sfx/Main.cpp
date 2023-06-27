
/* Parts of this program are Copyright (c) 2011, Max Aizenshtein <max.sniffer@gmail.com>
*/

#include <stdio.h>
#include <vector>
#include <iomanip> // for setw
#include "Sfx.h"
#include "SfxClasses.h"
#include <filesystem>
#ifdef _MSC_VER
#include "VisualStudioDebugOutput.h"
VisualStudioDebugOutput d(true);
#else
#include <fstream>
#include <iostream>
#endif
#include "json.hpp"
#include "Environ.h"
#include <cstdio>
extern std::string GetExecutableDirectory();
// For operator ""s
using namespace std::literals;
using namespace std::string_literals;
using namespace std::literals::string_literals;

using json = nlohmann::json;
std::string json_path;
std::string StripQuotes(const std::string &s)
{
	std::string a=s;
	while(a[0]==' ')
		a=a.substr(1,a.length()-1);
	if(a[0]=='\"')
		a=a.substr(1,a.length()-1);
	while(a[a.length()-1]==' ')
		a=a.substr(0,a.length()-1);
	if(a[a.length()-1]=='\"')
		a=a.substr(0,a.length()-1);
	return a;
}
std::string ProcessPath(const std::string &s)
{
	std::string p = std::filesystem::weakly_canonical((json_path+"/"s+s).c_str()).generic_string();
	return p;
}
SfxOptions sfxOptions;
const SfxOptions &GetSfxOptions()
{
    return sfxOptions;
}
extern bool terminate_command;
// A ctrl message handler to detect "close" events.
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
        printf("Ctrl-C event\n\n");
        return TRUE;

        // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
		std::cerr<<("Received Ctrl-Close event.\n");
		terminate_command=true;
        return TRUE;

        // Pass other signals to the next handler.
    case CTRL_BREAK_EVENT:
		std::cerr<<("Received Ctrl-Break event.\n");
		terminate_command=true;
        return FALSE;

    case CTRL_LOGOFF_EVENT:
        printf("Ctrl-Logoff event\n\n");
        return FALSE;

    case CTRL_SHUTDOWN_EVENT:
		std::cerr<<("Received Ctrl-Shutdown event.\n");
		terminate_command=true;
        return FALSE;

    default:
        return FALSE;
    }
}
std::string outputfile;
int main(int argc, char** argv) 
{
    if (SetConsoleCtrlHandler(CtrlHandler, TRUE))
	{
	}
	#ifdef _MSC_VER
	SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
	_set_abort_behavior(0, _WRITE_ABORT_MSG);
	#endif
	int ret = 9;
	int effect;
	if (argc<2)
	{
		std::cout<<"Usage: Sfx.exe <effect file> <program name>\n"<<argv[0]<<std::endl;
		return 5;
	}

	std::string SIMUL=GetEnv("SIMUL");
	std::string SIMUL_BUILD= "";
	std::map<std::string, std::string> environment;
	if (SIMUL_BUILD == "" || SIMUL_BUILD == "1")
		SIMUL_BUILD = SIMUL;
	effect = sfxGenEffect();
	
	char log[50000];
	const char **paths=NULL;
	std::string sourcefile;
	const char **args=NULL;
	bool force=false;
	std::string platformFilename="";
	std::vector<std::string> platformFilenames;
	std::string optimization;
	std::vector<std::string> genericPathStrings;
	if(argc>1) 
	{
		paths=new const char *[argc];
		args=new const char *[argc];
		int n=0;
		int a=0;
		for(int i=1;i<argc;i++)
		{
			if(strlen(argv[i])>=2&&(argv[i][0]=='-'))
			{
				const char *arg=argv[i]+2;
				for(int j=0;j<100;j++)
				{
					if(*arg==0||(*arg!='='&&*arg!=' '))
						break;
					arg++;
				}
				char argtype=argv[i][1];
				if(argtype=='i'||argtype=='I')
					genericPathStrings.push_back(StripQuotes(arg));
				else if (argtype == 'm' || argtype == 'M')
					sfxOptions.intermediateDirectory = StripQuotes(arg);
				else if (argtype == 's' || argtype == 'S')
				{
					SIMUL=StripQuotes(arg);
				}
				else if (argtype == 'b' || argtype == 'B')
				{
					SIMUL_BUILD=StripQuotes(arg);
				}
				else if (argtype == 'f' || argtype == 'F')
					sfxOptions.force = true;
				else if (argtype == 'v' || argtype == 'V')
					sfxOptions.verbose = true;
				else if (argtype == 'l' || argtype == 'L')
				{
					sfxOptions.disableLineWrites = true;
					std::cout << "Disabling #line directives" << std::endl;
				}
				else if (argtype == 'p' || argtype == 'P')
					platformFilenames.push_back(StripQuotes(arg));
				else if (argtype == 'o' || argtype == 'O')
					outputfile = StripQuotes(arg);
				else if (argtype == 'd' || argtype == 'D')
				{
					sfxOptions.debugInfo = true;
				}
				else if (argtype == 'e' || argtype == 'E')
				{
					std::string c=StripQuotes(arg);
					size_t eq_pos=c.find_first_of("=");
					environment[c.substr(0, eq_pos)] = c.substr(eq_pos + 1, c.length() - eq_pos - 1);
				}
				else if (argtype == 'z')
					optimization = arg;
				else if (argtype == 'w')
					sfxOptions.wrapOutput = true;
				else
					args[a++]=argv[i];
			}
			else if (strlen(argv[i]) >= 2 && argv[i][0] == '+')
			{
				args[a++] = argv[i]+1;
			}
			else
				sourcefile=StripQuotes(argv[i]);
		}
		paths[n]=0;
		args[a]=0;
	}
	for (auto e : environment)
	{
		SetEnv(e.first, e.second);
	}
	if(sourcefile.length()==0)
	{
		std::cerr<<("No source file from args :\n");
		sfxGetEffectLog(effect, log, sizeof(log));
		std::cerr<<log<<std::endl;
		return 2;
	}
	for(auto platformFilename:platformFilenames)
	{
		std::ifstream i(platformFilename);
		if(!i.good())
		{
			std::cerr<<"Error: Can't find config file "<<platformFilename.c_str()<<std::endl;
			return 18;
		}
		std::filesystem::path p=std::filesystem::weakly_canonical(platformFilename);
		std::string filename_only=p.filename().string();
		std::string platformName=filename_only.replace(filename_only.find(".json"),5,"");
		json_path=std::filesystem::weakly_canonical(p.parent_path()).generic_string();
		std::string exe_dir=GetExecutableDirectory();
		std::string build_dir=std::filesystem::weakly_canonical((exe_dir+"/../..").c_str()).generic_string();
		SetEnv("BUILD_DIR",build_dir.c_str());
		std::string platform_dir=std::filesystem::weakly_canonical((build_dir+"/..").c_str()).generic_string();
		while(p.has_parent_path())
		{
			p=p.parent_path();
			if(p.filename()=="Platform")
			{
				platform_dir=p.generic_string();
				SetEnv("PLATFORM_DIR",platform_dir.c_str());
				break;
			}
		}
		auto pathStrings=genericPathStrings;
		pathStrings.push_back(json_path);
		pathStrings.push_back(platform_dir+"/Shaders/SL"s);
		//std::string platform_build_dir=std::filesystem::weakly_canonical((exe_dir+"/../../Platform").c_str()).generic_string();
		sfxOptions.optimizationLevel=-1;
		if(optimization.length())
		{
			sfxOptions.optimizationLevel=atoi(optimization.c_str());
		}
		SfxConfig sfxConfig;
		if(sfxOptions.debugInfo)
			sfxConfig.define["DEBUG"] = "1";
		sfxConfig.platformFilename=platformFilename;

		json j;
		try
		{
			i >> j;
			//if(sfxOptions.verbose)
			//	std::cout << std::setw(4)<< j << std::endl;
			json compiler = j["compiler"];
			if (compiler.type() == json::value_t::string)
			{
				sfxConfig.compiler = ProcessEnvironmentVariables(compiler);
			}
			else
			{
				sfxConfig.compiler = ProcessEnvironmentVariables(compiler["command"]);
				if(compiler.count("commandPaths")>0)
				{
					for (auto& el : compiler["commandPaths"].items())
					{
						sfxConfig.compilerPaths.push_back(ProcessEnvironmentVariables(el.value()));
					}
				}
				if (compiler.count("stages") > 0)
				{
					json stages = compiler["stages"];
					for (json::iterator it = stages.begin(); it != stages.end(); ++it)
					{
						std::string a = it.key();
						std::string b = it.value();
						int st = -1;
						if (a == "vertex")
							st = (int)sfx::VERTEX_SHADER;
						else if (a == "geometry")
							st = (int)sfx::GEOMETRY_SHADER;
						else if (a == "pixel"||a=="fragment")
							st = (int)sfx::FRAGMENT_SHADER;
						else if (a == "compute")
							st = (int)sfx::COMPUTE_SHADER;
						if(st>=0)
							sfxConfig.stages[st] = b;
					}
				}
			}
			sfxConfig.defaultOptions							=j["defaultOptions"];
			sfxConfig.api										=j["api"];
			sfxConfig.sourceExtension							=j["sourceExtension"];
			sfxConfig.outputExtension							=j["outputExtension"];
			sfxConfig.outputOption								=j["outputOption"];
			if (j.count("maxShaderModel") > 0)
				sfxConfig.maxShaderModel						=j["maxShaderModel"];
			if (j.count("failOnCerr") > 0)
				sfxConfig.failOnCerr							=j["failOnCerr"];
			if(j.count("includeOption")>0)
				sfxConfig.includeOption							=j["includeOption"];
			
			if(j.count("includePaths")>0)
			{
				json includePaths				=j["includePaths"];
				for (json::iterator it = includePaths.begin();it != includePaths.end(); ++it)
				{
					std::string b=it.value();
					pathStrings.push_back(ProcessPath(ProcessEnvironmentVariables(b)));
				}
			}
			if(outputfile.length()==0)
			{
				if(j.count("outputPath")>0)
				{
					outputfile										=ProcessEnvironmentVariables(j["outputPath"]);
				}
				else
				{
					outputfile										=ProcessEnvironmentVariables("$BUILD_DIR/Shaders/"+platformName+"/shaderbin"s);
				}
			}
			if(sfxOptions.intermediateDirectory.length()==0)
			{
				if(j.count("intermediateDirectory")>0)
				{
					sfxOptions.intermediateDirectory				=ProcessEnvironmentVariables(j["intermediateDirectory"]);
				}
				else
				{
					sfxOptions.intermediateDirectory				=ProcessEnvironmentVariables("$BUILD_DIR/Shaders/"+platformName+"/sfx_intermediate"s);
				}
			}
			if(j.count("intermediatePath")>0&&sfxOptions.intermediateDirectory.length()==0)
				sfxOptions.intermediateDirectory				=ProcessEnvironmentVariables(j["intermediatePath"]);
			if(sfxOptions.intermediateDirectory.length()==0)
			{
				sfxOptions.intermediateDirectory="sfx_intermediate";
			}
			sfxConfig.entryPointOption							=j["entryPointOption"];
			if(j.count("debugOption")>0)
				sfxConfig.debugOption							=j["debugOption"]; 
			if(j.count("releaseOptions")>0)
				sfxConfig.releaseOptions						=j["releaseOptions"]; 
			if(j.count("debugOutputFileOption")>0)
				sfxConfig.debugOutputFileOption					=j["debugOutputFileOption"]; 
			if(j.count("multiplePixelOutputFormats")>0)
			sfxConfig.multiplePixelOutputFormats				=j["multiplePixelOutputFormats"];
			if (j.count("identicalIOBlocks") > 0)
				sfxConfig.identicalIOBlocks						=j["identicalIOBlocks"];
			if (j.count("supportInlineSamplerStates")>0)
				sfxConfig.supportInlineSamplerStates			=j["supportInlineSamplerStates"];
			if (j.count("lineDirectiveFilenames")>0)
				sfxConfig.lineDirectiveFilenames				=j["lineDirectiveFilenames"];
			if (j.count("passThroughSamplers")>0)
				sfxConfig.passThroughSamplers					=j["passThroughSamplers"];
			if (j.count("generateSlots")>0)
				sfxConfig.generateSlots							=j["generateSlots"]; 
			if (j.count("numTextureSlots")>0)
				sfxConfig.numTextureSlots						=j["numTextureSlots"];
			if (j.count("sharedSlots")>0)
				sfxConfig.sharedSlots							=j["sharedSlots"];
			if (j.count("reverseTexCoordY")>0)
				sfxConfig.reverseTexCoordY						=j["reverseTexCoordY"];
			if (j.count("reverseTexCoordY")>0)
				sfxConfig.reverseVertexOutputY					=j["reverseVertexOutputY"];
			if (j.count("combineTexturesSamplers")>0)
				sfxConfig.combineTexturesSamplers				=j["combineTexturesSamplers"];
			if (j.count("maintainSamplerDeclaration")>0)
				sfxConfig.maintainSamplerDeclaration			=j["maintainSamplerDeclaration"];
			if (j.count("combineInShader")>0)
				sfxConfig.combineInShader						=j["combineInShader"];
			if (j.count("supportsPercentOperator")>0)
				sfxConfig.supportsPercentOperator				=j["supportsPercentOperator"];
			if (j.count("getSizeExpression")>0)
				sfxConfig.getSizeExpression						=j["getSizeExpression"];
			if (j.count("getRWSizeExpression")>0)
				sfxConfig.getRWSizeExpression					=j["getRWSizeExpression"];
			else
				sfxConfig.getRWSizeExpression					=sfxConfig.getSizeExpression;
			if(j.count("textureDeclaration")>0)
				sfxConfig.textureDeclaration					=j["textureDeclaration"];
			if (j.count("structuredBufferDeclaration")>0)
				sfxConfig.structuredBufferDeclaration			=j["structuredBufferDeclaration"];
			if (j.count("imageDeclaration")>0)
				sfxConfig.imageDeclaration						=j["imageDeclaration"];
			if (j.count("imageDeclarationWriteOnly")>0)
				sfxConfig.imageDeclarationWriteOnly				=j["imageDeclarationWriteOnly"];
			if (j.count("samplerDeclaration")>0)
				sfxConfig.samplerDeclaration					=j["samplerDeclaration"];
			if (j.count("structDeclaration")>0)
				sfxConfig.structDeclaration						=j["structDeclaration"];
			if (j.count("constantBufferDeclaration")>0)
				sfxConfig.constantBufferDeclaration				=j["constantBufferDeclaration"];
			if (j.count("namedConstantBufferDeclaration")>0)
				sfxConfig.namedConstantBufferDeclaration		=j["namedConstantBufferDeclaration"];
			if (j.count("inputDeclaration")>0)
			{
				std::cerr<<platformFilename.c_str()<<": inputDeclaration is deprecated, please use pixelInputDeclaration\n";
				sfxConfig.pixelInputDeclaration					=j["inputDeclaration"];
			}
			if (j.count("pixelInputDeclaration")>0)
				sfxConfig.pixelInputDeclaration					=j["pixelInputDeclaration"];
			if (j.count("vertexInputDeclaration")>0)
				sfxConfig.vertexInputDeclaration				=j["vertexInputDeclaration"];
			if (j.count("outputDeclaration")>0)
			{
				std::cerr<<platformFilename.c_str()<<": outputDeclaration is deprecated, please use vertexOutputDeclaration\n";
				sfxConfig.vertexOutputDeclaration				=j["outputDeclaration"];
			}
			if (j.count("vertexOutputDeclaration")>0)
				sfxConfig.vertexOutputDeclaration				=j["vertexOutputDeclaration"];
			if (j.count("pixelOutputDeclaration")>0)
				sfxConfig.pixelOutputDeclaration				=j["pixelOutputDeclaration"];
			if (j.count("pixelOutputDeclarationDSB") > 0)
				sfxConfig.pixelOutputDeclarationDSB				=j["pixelOutputDeclarationDSB"];
			if (j.count("samplingSyntax")>0)
				sfxConfig.samplingSyntax						=j["samplingSyntax"];
			if (j.count("loadSyntax")>0)
				sfxConfig.loadSyntax							=j["loadSyntax"];
			if (j.count("storeSyntax")>0)
				sfxConfig.storeSyntax							=j["storeSyntax"];
			if (j.count("preamble")>0)
				sfxConfig.preamble								=j["preamble"];
			if (j.count("optimizationLevelOption")>0)
				sfxConfig.optimizationLevelOption				=j["optimizationLevelOption"];
			if (j.count("computePreamble")>0)
				sfxConfig.computePreamble						=j["computePreamble"];
			if(j.count("compilerMessageRegex")>0)
			{
				sfxConfig.compilerMessageRegex					=j["compilerMessageRegex"][0];
				sfxConfig.compilerMessageReplace				=j["compilerMessageRegex"][1];
			}
			if(j.count("vertexSemantics")>0)
			{
				json vertexSemantics				=j["vertexSemantics"];
				for (json::iterator it = vertexSemantics.begin();it != vertexSemantics.end(); ++it)
				{
					std::string a=it.key();
					std::string b=it.value();
					sfxConfig.vertexSemantics[a]			=b;
				}
			}
			if(j.count("pixelSemantics")>0)
			{
				json pixelSemantics				=j["pixelSemantics"];
				for (json::iterator it = pixelSemantics.begin();it != pixelSemantics.end(); ++it)
				{
					std::string a=it.key();
					std::string b=it.value();
					sfxConfig.pixelSemantics[a]			=b;
				}
			}
			if (j.count("vertexOutputAssignment") > 0)
			{
				json vertexOutputAssignment			= j["vertexOutputAssignment"];
				for (json::iterator it = vertexOutputAssignment.begin(); it != vertexOutputAssignment.end(); ++it)
				{
					std::string a = it.key();
					std::string b = it.value();
					sfxConfig.vertexOutputAssignment[a] = b;
				}
			}
			if (j.count("fragmentOutputAssignment") > 0)
			{
				json fragmentOutputAssignment = j["fragmentOutputAssignment"];
				for (json::iterator it = fragmentOutputAssignment.begin(); it != fragmentOutputAssignment.end(); ++it)
				{
					std::string a = it.key();
					std::string b = it.value();
					sfxConfig.fragmentOutputAssignment[a] = b;
				}
			}
			if (j.count("computeSemantics") > 0)
			{
				json computeSemantics = j["computeSemantics"];
				for (json::iterator it = computeSemantics.begin(); it != computeSemantics.end(); ++it)
				{
					std::string a = it.key();
					std::string b = it.value();
					sfxConfig.computeSemantics[a] = b;
				}
			}
			json fmt = j["toImageFormat"];
			for (json::iterator it = fmt.begin(); it != fmt.end(); ++it)
			{
				std::string a = it.key();
				std::string b = it.value();
				sfxConfig.toImageFormat[a] = b;
			}
			json tot = j["toImageType"];
			for (json::iterator it = tot.begin(); it != tot.end(); ++it)
			{
				std::string a = it.key();
				std::string b = it.value();
				sfxConfig.toImageType[a] = b;
			}
			json totex = j["toTextureType"];
			for (json::iterator it = totex.begin(); it != totex.end(); ++it)
			{
				std::string a = it.key();
				std::string b = it.value();
				sfxConfig.toTextureType[a] = b;
			}
			if (j.count("computeLayout")>0)
				sfxConfig.computeLayout = j["computeLayout"];
			if (j.count("templateTypes") > 0)
			{
				json ttypes = j["templateTypes"];
				for (json::iterator it = ttypes.begin(); it != ttypes.end(); ++it)
				{
					std::string a = it.key();
					std::string b = it.value();
					sfxConfig.templateTypes[a] = b;
				}
			}
			json repl=j["replace"];
			for (json::iterator it = repl.begin();it != repl.end(); ++it)
			{
				std::string a=it.key();
				std::string b=it.value();
				sfxConfig.replace[a]			=b;
			}
			json def=j["define"];
			for (json::iterator it = def.begin();it != def.end(); ++it)
			{
				std::string a=it.key();
				std::string b=it.value();
				sfxConfig.define[a]			=b;
			}
			sfxConfig.define["SFX"]="1";
			if (j.count("graphicsRootSignatureSource") > 0)
				sfxConfig.graphicsRootSignatureSource = j["graphicsRootSignatureSource"];
			if (j.count("forceSM51") > 0)
			{
				sfxConfig.forceSM51 = j["forceSM51"];
			}
			if (j.count("supportRaytracing") > 0)
			{
				sfxConfig.supportRaytracing = j["supportRaytracing"];
			}
			if (j.count("supportMultiview") > 0)
			{
				sfxConfig.supportMultiview = j["supportMultiview"];
			}
		}
		catch(std::exception &e)
		{
			std::cerr<<e.what()<<std::endl;
			return 3;
		}
	
		//std::cout<<"Sfx compiling"<<sourcefile<<std::endl;
		if (!sfxParseEffectFromFile(effect,sourcefile.c_str(),pathStrings,outputfile.c_str(),&sfxConfig,&sfxOptions,args))
		{
			std::cerr<<("Error creating effect:\n");
			sfxGetEffectLog(effect, log, sizeof(log));
			std::cerr<<log<<std::endl;
		}
		else
		{
			ret = 0;
		}
		sfxDeleteEffect(effect);
	}
	return ret;
 }
