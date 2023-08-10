#include "PixelFormat.h"
#include "Platform/Core/RuntimeError.h"
#include <iostream>
#ifndef _MSC_VER
#include <stdio.h>
#include <strings.h>
#define _stricmp strcasecmp
#endif
using namespace platform;
using namespace crossplatform;

static bool is_equal(const char * str,const char *tst)
{
return (_stricmp(str,tst) == 0);
}


PixelFormat platform::crossplatform::TypeToFormat(const char *txt)
{
		//return R_16_FLOAT;
	if(is_equal(txt,"vec4"))
		return RGBA_32_FLOAT;
	if(is_equal(txt,"vec3"))
		return RGB_32_FLOAT;
	if(is_equal(txt,"vec2"))
		return RG_32_FLOAT ;
		//return RG_16_FLOAT ;
	if(is_equal(txt,"float"))
		return R_32_FLOAT ;
		//return LUM_32_FLOAT ;
		//return INT_32_FLOAT ;
		//return RGBA_8_UNORM ;
		//return RGBA_8_UNORM_COMPRESSED ;
		//return RGBA_8_UNORM_SRGB ;
		//return RGBA_8_SNORM ;
		//return RGB_8_UNORM ;
		//return RGB_8_SNORM ;
		//return R_8_UNORM ;
		//return R_8_SNORM ;
	if(is_equal(txt,"uint"))
		return R_32_UINT ;
	if(is_equal(txt,"uint2"))
		return RG_32_UINT ;
	if(is_equal(txt,"uint3"))
		return RGB_32_UINT ;
	if(is_equal(txt,"uint4"))
		return RGBA_32_UINT ;
		// depth formats: ;
		//return D_24_UNORM_S_8_UINT ;
		//return D_16_UNORM ;
		//return BGRA_8_UNORM ;
	SIMUL_INTERNAL_CERR<<"Unknown or unsupported type string "<<txt<<"\n";
	return UNKNOWN;
};