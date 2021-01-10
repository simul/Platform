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

Pass::Pass(const PassState &passState)
{
	this->passState=passState;
	map<ShaderType,string>::const_iterator it;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		it=passState.shaders.find((sfx::ShaderType)i);
		if(it==passState.shaders.end())
			continue;
		if(it->second==string("gsConstructed"))
			continue;
		m_shaderInstanceNames[i]=it->second;
	}
}
