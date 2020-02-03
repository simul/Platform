
/* Parts of this program are Copyright (c) 2011, Max Aizenshtein <max.sniffer@gmail.com>
*/

#include <stdio.h>
#include <vector>
#include <iomanip> // for setw
#include "Sfx.h"
#include "SfxClasses.h"
#include "VisualStudioDebugOutput.h"
#include "json.hpp"
#include "Environ.h"
#include <cstdio>

using json = nlohmann::json;

VisualStudioDebugOutput d(true);

std::string outputfile;
int main(int argc, char** argv) 
{
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

	SfxOptions sfxOptions;
	std::string SIMUL=GetEnv("SIMUL");
	std::string SIMUL_BUILD= GetEnv("SIMUL_BUILD");
	std::map<std::string, std::string> environment;
	if (SIMUL_BUILD == "" || SIMUL_BUILD == "1")
		SIMUL_BUILD = SIMUL;
	effect = sfxGenEffect();
	
	char log[50000];
	std::vector<std::string> pathStrings;
	const char **paths=NULL;
	const char *sourcefile="";
	const char **args=NULL;
	bool force=false;
	std::string platformFilename="";
	std::string optimization;
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
					paths[n++]=arg;
				else if (argtype == 'm' || argtype == 'M')
					sfxOptions.intermediateDirectory = arg;
				else if (argtype == 's' || argtype == 'S')
				{
					SIMUL=arg;
				}
				else if (argtype == 'b' || argtype == 'B')
				{
					SIMUL_BUILD=arg;
				}
				else if (argtype == 't' || argtype == 'T')
				{
					if (strcasecmp(arg, "dx12") == 0||strcasecmp(arg, "d3d12") == 0)
					{
						pathStrings.push_back(SIMUL + "/Platform/DirectX12/HLSL");
						paths[n++] = (pathStrings.back()).c_str();
						pathStrings.push_back(SIMUL + "/Platform/CrossPlatform/SL");
						paths[n++] = (pathStrings.back()).c_str();
						sfxOptions.intermediateDirectory = SIMUL_BUILD + "/Platform/DirectX12/shaderbin";
						outputfile = SIMUL_BUILD + "/Platform/DirectX12/shaderbin";
						platformFilename = SIMUL + "/Platform/DirectX12/HLSL/HLSL12.json";
					}
					if (strcasecmp(arg, "vulkan") == 0)
					{
						pathStrings.push_back(SIMUL + "/Platform/Vulkan/GLSL");
						paths[n++] = (pathStrings.back()).c_str();
						pathStrings.push_back(SIMUL + "/Platform/CrossPlatform/SL");
						paths[n++] = (pathStrings.back()).c_str();
						sfxOptions.intermediateDirectory = SIMUL_BUILD + "/Platform/Vulkan/sfx_intermediate";
						outputfile = SIMUL_BUILD + "/Platform/Vulkan/shaderbin";
						platformFilename = SIMUL + "/Platform/Vulkan/GLSL/GLSL.json";
					}
					if (strcasecmp(arg, "opengl") == 0)
					{
						pathStrings.push_back(SIMUL + "/Platform/OpenGL/GLSL");
						paths[n++] = (pathStrings.back()).c_str();
						pathStrings.push_back(SIMUL + "/Platform/CrossPlatform/SL");
						paths[n++] = (pathStrings.back()).c_str();
						sfxOptions.intermediateDirectory = SIMUL_BUILD + "/Platform/OpenGL/shaderbin";
						outputfile = SIMUL_BUILD + "/Platform/OpenGL/shaderbin";
						platformFilename = SIMUL + "/Platform/OpenGL/GLSL/GLSL.json";
					}
				}
				else if (argtype == 'f' || argtype == 'F')
					sfxOptions.force = true;
				else if (argtype == 'v' || argtype == 'V')
					sfxOptions.verbose = true;
				else if (argtype == 'l' || argtype == 'L')
				{
					sfxOptions.disableLineWrites = true;
					std::cout << "Disabling #line directives" << std::endl;
					//printf("Disabling #line directives");
				}
				else if (argtype == 'p' || argtype == 'P')
					platformFilename = arg;
				else if (argtype == 'o' || argtype == 'O')
					outputfile = arg;
				else if (argtype == 'd' || argtype == 'D')
					sfxOptions.debugInfo = true;
				else if (argtype == 'e' || argtype == 'E')
				{
					std::string c( arg);
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
				sourcefile=argv[i];
		}
		paths[n]=0;
		args[a]=0;
	}
	for (auto e : environment)
	{
		SetEnv(e.first, e.second);
	}
	if(sfxOptions.intermediateDirectory.length()==0)
	{
		sfxOptions.intermediateDirectory="sfx_intermediate";
	}
	if(strlen(sourcefile)==0)
	{
		std::cerr<<("No source file from args :\n");
		sfxGetEffectLog(effect, log, sizeof(log));
		std::cerr<<log<<std::endl;
		return 2;
	}
	std::ifstream i(platformFilename);
	if(!i.good())
	{
		//printf(platformFilename.c_str());
		std::cerr<<"Error: Can't find config file "<<platformFilename.c_str()<<std::endl;
		return 18;
	}
	sfxOptions.optimizationLevel=-1;
	if(optimization.length())
	{
		sfxOptions.optimizationLevel=atoi(optimization.c_str());
	}
	SfxConfig sfxConfig;
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
		if (j.count("failOnCerr") > 0)
			sfxConfig.failOnCerr							=j["failOnCerr"];
		if(j.count("includeOption")>0)
			sfxConfig.includeOption							=j["includeOption"];
		sfxConfig.entryPointOption							=j["entryPointOption"];
		if(j.count("debugOption")>0)
			sfxConfig.debugOption							=j["debugOption"]; 
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
		if (j.count("inputDeclaration")>0)
			sfxConfig.inputDeclaration						=j["inputDeclaration"];
		if (j.count("vertexInputDeclaration")>0)
			sfxConfig.vertexInputDeclaration				=j["vertexInputDeclaration"];
		if (j.count("outputDeclaration")>0)
			sfxConfig.outputDeclaration						=j["outputDeclaration"];
		if (j.count("pixelOutputDeclaration")>0)
			sfxConfig.pixelOutputDeclaration				=j["pixelOutputDeclaration"];
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
			sfxConfig.computePreamble						= j["computePreamble"];
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
	}
	catch(std::exception &e)
	{
		std::cerr<<e.what()<<std::endl;
		return 3;
	}
	
	//std::cout<<"Sfx compiling"<<sourcefile<<std::endl;
	if (!sfxParseEffectFromFile(effect,sourcefile,paths,outputfile.c_str(),&sfxConfig,&sfxOptions,args))
	{
		std::cerr<<("Error creating effect:\n");
		sfxGetEffectLog(effect, log, sizeof(log));
		std::cerr<<log<<std::endl;
	}
	else
	{
		//std::cout<<("Compiled successfully\n");
		if (sfxOptions.verbose)
		{
			std::cout<< outputfile.c_str()<<"(0): info: output file."<< std::endl;
			printf("%s",outputfile.c_str());
		}
		ret = 0;
	}
	sfxDeleteEffect(effect);
	return ret;
 }
