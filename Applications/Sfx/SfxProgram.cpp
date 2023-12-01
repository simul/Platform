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

Technique::Technique(const vector<Pass>& passes)
	:passes(passes)
{
	for(auto p:passes)
	{
		m_passes[p.name]=&p;
	}
}

Pass::Pass(const char *n)
{
	name=n;
}

Pass::Pass(const char *n,const PassState &passState)
{
	name=n;
	this->passState=passState;
	map<ShaderType,string>::const_iterator it;
	numVariants=1;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		variantCount[i]=0;
		auto t=(sfx::ShaderType)i;
		it=passState.shaders.find(t);
		if(it==passState.shaders.end())
			continue;
		if(it->second==string("gsConstructed"))
			continue;
		m_shaderInstanceNames[i]=it->second;
		shaderVariantNames[i]=GetShaderVariants(t);
		variantCount[i]=(int)shaderVariantNames[i].size();
		numVariants*=variantCount[i];
	}
}

std::vector<std::string> Pass::GetShaderVariants(ShaderType t) const
{
	std::string variant_list=m_shaderInstanceNames[(int)t];
	std::vector<std::string> v=split(variant_list,',');
	return v;
}

int Pass::GetNumVariants() const
{
	return numVariants;
}

int Pass::getIndex(ShaderType t,int v)const
{
	int variant=v;
	int index[NUM_OF_SHADER_TYPES]={0};
	int mul[NUM_OF_SHADER_TYPES]={0};
	int last_mul=1;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		if(variantCount[i])
		{
			mul[i]=last_mul;
			last_mul*=variantCount[i];
		}
	}
	int n=v;
	for(int i=NUM_OF_SHADER_TYPES-1;i>=0;i--)
	{
		if(variantCount[i])
		{
			index[i]=n/mul[i];
			n-=index[i]*mul[i];
		}
		if((int)t==i)
			return index[i];
	}
	return 0;
}

std::string Pass::GetVariantName(int v) const
{
	if(numVariants==1)
		return name;
	int variant=v;
	int index[NUM_OF_SHADER_TYPES]={0};
	int mul[NUM_OF_SHADER_TYPES]={0};
	int last_mul=1;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		if(variantCount[i])
		{
			mul[i]=last_mul;
			last_mul*=variantCount[i];
		}
	}
	int n=v;
	for(int i=NUM_OF_SHADER_TYPES-1;i>=0;i--)
	{
		if(variantCount[i])
		{
			index[i]=n/mul[i];
			n-=index[i]*mul[i];
		}
	}
	std::string res=name;
	for(int i=0;i<NUM_OF_SHADER_TYPES;i++)
	{
		if(variantCount[i])
		{
			res+="."+shaderVariantNames[i][index[i]];
		}
	}
	return res;
}

std::string Pass::GetShader(ShaderType t,int i) const
{
	return shaderVariantNames[(int)t][getIndex(t,i)];
}