/* Copyright (c) 2011, Max Aizenshtein <max.sniffer@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the <organization> nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#pragma once
#include <string>
#include <map>
#include <vector>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif 

struct SfxConfig
{
	SfxConfig():
		multiplePixelOutputFormats(false)
		,identicalIOBlocks(false)
		,supportInlineSamplerStates(true)
		,lineDirectiveFilenames(true)
		,passThroughSamplers(true)
		,maintainSamplerDeclaration(true)
		,failOnCerr(false)
		,generateSlots(false)
		,numTextureSlots(32)
		,sharedSlots(true)
		,combineTexturesSamplers(false)
		,combineInShader(false)
		,forceSM51(false)
		,supportRaytracing(false)
	{
		samplerDeclaration= "SamplerState {name}: register(s{slot});";
	}
	std::string platformFilename;
	std::string api;
	std::string compiler;
	std::map<int, std::string> stages;
	std::string defaultOptions;
	std::string sourceExtension;
	std::string outputExtension;
	std::string outputOption;
	std::string includeOption;
	std::string entryPointOption;
	std::string debugOption;
	std::string releaseOptions;
	std::string debugOutputFileOption;
	std::string compilerMessageRegex;
	std::string compilerMessageReplace;
	bool multiplePixelOutputFormats;
	bool identicalIOBlocks;
	bool supportInlineSamplerStates;
	bool lineDirectiveFilenames;
	bool passThroughSamplers;
	bool maintainSamplerDeclaration;
	//! GLslang for example, doesn't set ERRORLEVEL, but outputs to cerr if there are errors.
	bool failOnCerr;
	//! Should SFX override the provided slots and generate unique ones?
	bool generateSlots;
	//! Maximum number of slots - if we try to generate more we have a problem!
	int numTextureSlots;
	//! Can we use the same slots for multiple objects. If not, we must generate unique slots.
	bool sharedSlots;
	//! Because GLSL and HLSL use opposite interpretations of the y coordinate in texture lookups...
	bool reverseTexCoordY = false;
	bool reverseVertexOutputY = false;
	//! If set to true, textures will be combined with the sampler used
	//! on them
	bool combineTexturesSamplers;
	//! This interacts with 'combineTexturesSamplers' if true, we can perform sampler2D(texHandle | samplerHandle)
	//! otherwise we will need to send the handle combined API side
	bool combineInShader;
	bool supportsPercentOperator=true;
	//! Custom get size expression
	std::string getSizeExpression;
	std::string getRWSizeExpression;
	std::string textureDeclaration;
	std::string imageDeclaration;
	std::string imageDeclarationWriteOnly;
	std::string samplerDeclaration;
	std::string structDeclaration;
	std::string constantBufferDeclaration;
	std::string pixelInputDeclaration;
	std::string vertexInputDeclaration;
	std::string vertexOutputDeclaration;
	std::string pixelOutputDeclaration;
	std::string pixelOutputDeclarationDSB;
	std::string samplingSyntax;
	std::string loadSyntax;
	std::string storeSyntax;
	//! Generic preamble
	std::string preamble;
	std::string optimizationLevelOption;
	//! Preamble only for compute shaders
	std::string computePreamble;
	std::map<std::string,std::string> vertexSemantics;
	std::map<std::string,std::string> pixelSemantics;
	std::map<std::string, std::string> vertexOutputAssignment;
	std::map<std::string, std::string> fragmentOutputAssignment;
	//! Mapping of semantics used in compute shaders
	std::map<std::string, std::string> computeSemantics;
	//! How the compute thread layout is represented
	std::string computeLayout;
	std::map<std::string,std::string> replace;
	//! Macros to be defined
	std::map<std::string, std::string> define;
	//! SFX format to language format
	std::map<std::string, std::string> toImageFormat;
	//! SFX format to object type
	std::map<std::string, std::string> toImageType;
	//! SFX format to texture object type
	std::map<std::string, std::string> toTextureType;
	//! From templates to type names, e.g. Texture2D<uint> -> uTexture2D
	std::map<std::string, std::string> templateTypes;
	//! Declaration for a structured buffer
	std::string structuredBufferDeclaration;
	//! GFX Root signature source
	std::string graphicsRootSignatureSource;
	//! TEMP: Forces all shaders to be 5_1
	bool forceSM51;
	//! Does this API support Raytracing
	bool supportRaytracing;
	//! Holds list of parsed paths passed from the command line
	std::vector<std::string> shaderPaths;
};

struct SfxOptions
{
	SfxOptions():force(false)
		,verbose(false)
		,debugInfo(false)
		,disableLineWrites(false)
		,optimizationLevel(-1)
	{
	}
	bool force;
	bool verbose;
	bool debugInfo;
	//! If true, the output file will contain all the compiled binaries, with a table to point to their offsets.
	bool wrapOutput;
	//! If true, #line directives will not be put in, so that compile output will show the line number from the generated file.
	bool disableLineWrites;
	std::string intermediateDirectory;
	std::string outputFile;
	int optimizationLevel;
};

/**************************************************
* sfxGenEffect
* Return value: Effect id
**************************************************/
int sfxGenEffect();

/**************************************************
* sfxParseEffectFromFileSIMUL
* Input:
*	effect  -- sfx effect id
*	src	-- File contents with includes inlined
*	filenamesUtf8	-- File name list
* Return value: Status
**************************************************/
bool sfxParseEffectFromTextSIMUL(int effect, const char* src,const char **filenamesUtf8);
/**************************************************
* sfxCreateEffectFromFile
* Input:
*	effect  -- sfx effect id
*	file	-- File name
* Return value: Status
**************************************************/
bool sfxParseEffectFromFile( int effect, const char* file,const char **paths,const char *outputfile,SfxConfig *config,const SfxOptions *sfxOptions,const char **args);

/**************************************************
* sfxCreateEffectFromMemory
* Input:
*	effect  -- sfx effect id
*	src	-- Source
* Return value: Status
**************************************************/
bool sfxParseEffectFromMemory( int effect, const char* src ,const char *filename,const char *output_filename,const SfxConfig *config,const SfxOptions *sfxOptions);

/**************************************************
* sfxCompileProgram
* Input:
*	effect  -- sfx effect id
*	program -- Program name
* Return value: GL program id if success, 0 otherwise
**************************************************/
bool sfxCompileProgram(int effect, const char* program);

/**************************************************
* sfxGetProgramCount
* Return value: Number of programs
**************************************************/
int sfxGetProgramCount(int effect);

/**************************************************
* sfxGetProgramName
* Input:
*	effect  -- sfx effect id
*	program -- Index of program
*	name	-- Destination address
*	bufSize -- Size of the buffer
**************************************************/
void sfxGetProgramName(int effect, int program, char* name, int bufSize);

/**************************************************
* sfxGetProgramIndex
* Input:
*	effect  -- sfx effect id
*	name -- name of program
**************************************************/
size_t sfxGetProgramIndex(int effect, const char* name);

/**************************************************
* sfxGenerateSampler
* Input:
*	effect  -- sfx effect id
*	sampler -- Sampler name
* Return value: GL sampler id if success, -1 otherwise
**************************************************/
int sfxGenerateSampler(int effect, const char* sampler);

/**************************************************
* sfxGetEffectLog
* Input:
*	effect  -- sfx effect id
*	log	 -- Destination address
*	bufSize -- Size of the buffer
**************************************************/
void sfxGetEffectLog(int effect, char* log, int bufSize);


/**************************************************
* sfxDeleteEffect
* Input:
*	effect  -- sfx effect id
**************************************************/
void sfxDeleteEffect(int effect);


/**************************************************
* sfxGetProgramName
* Input:
*	effect  -- sfx effect id
*	program -- Index of program
**************************************************/
const char* sfxGetProgramName(int effect, int program);

/**************************************************
* sfxGetProgramIndex
* Input:
*	effect  -- sfx effect id
*	name -- name of program
**************************************************/
size_t sfxGetProgramIndex(int effect, const char* name);

/**************************************************
* sfxGetEffectLog
* Input:
*	effect  -- sfx effect id
* Return value: Log string
**************************************************/
const char* sfxGetEffectLog(int effect);

