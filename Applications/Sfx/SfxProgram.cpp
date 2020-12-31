#include <map>
#include <string>
#include <cstring>
#include <sstream>	// for ostringstream
#include <cstdio>
#include <cassert>
#include <fstream>
#include <iostream>

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
#endif
#include "GeneratedFiles/SfxScanner.h"
#include "SfxProgram.h"
#include "StringFunctions.h"
#include "StringToWString.h"
#include "SfxEffect.h"
#include "SfxErrorCheck.h"
using namespace std;

Technique::Technique(const map<std::string, Pass>& passes)
	:m_passes(passes)
{
}

Pass::Pass()
{
}

Pass::Pass(const map<ShaderType,string>& shaders,const PassState &passState)
{
	this->passState=passState;
	map<ShaderType,string>::const_iterator it;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		it=shaders.find((sfx::ShaderType)i);
		if(it==shaders.end())
			continue;
		if(it->second==string("gsConstructed"))
			continue;
		m_compiledShaderNames[i]=it->second;
	}
}

CompiledShader::CompiledShader(const std::set<Declaration*> &d)
	:	global_line_number(0)
{
	declarations=d;
}

CompiledShader::CompiledShader(const CompiledShader &cs)
{
	shaderType			=cs.shaderType;
	m_profile			=cs.m_profile;
	m_functionName		=cs.m_functionName;
	m_preamble			=cs.m_preamble;
	m_augmentedSource	=cs.m_augmentedSource;
	sbFilenames			=cs.sbFilenames;
	entryPoint			=cs.entryPoint;
	declarations		=cs.declarations;
	global_line_number	=cs.global_line_number;
	constantBuffers		=cs.constantBuffers;
}
