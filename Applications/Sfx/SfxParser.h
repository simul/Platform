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

#include "SfxClasses.h"

using namespace sfx;
/// Values that represent SamplerParam.
enum SamplerParam
{
	SAMPLER_PARAM_STRING,
	SAMPLER_PARAM_INT,
	SAMPLER_PARAM_FLOAT
};
/// Values that represent RegisterParamType.
enum RegisterParamType
{
	REGISTER_NONE,
	REGISTER_INT,
	REGISTER_NAME
};

struct ShaderParameterType
{
	union
	{
		int num;
		float fnum;
		bool boolean;
	};
	std::string str;
};

struct sfxstype
{
	sfxstype() {}
	std::vector<sfxstype> children;
	struct variable
	{
		union
		{
			int num;
			unsigned int unum;
			float fnum;
			bool boolean;
			float vec4[4];
			int type_enum;		// corresponds to yytokentype enum
		};
		bool operator<(const variable &v)
		{
			return (identifier<v.identifier);
		}
		std::string storage;
		std::string type;
		std::string identifier;
		std::string templ;
		std::string semantic;
		std::string default_;
		bool has_default;
		ShaderResourceType shaderResourceType;
	};

	struct samplerVar
	{
		std::string  binding;
		std::string  name;
	};

	int	lineno;
	int token;
	union
	{
		int			num;
		unsigned	unum;
		float		fnum;
		bool		boolean;
		Technique *	tech;
		Pass*		prog;

		std::map<std::string, Pass>*			passes;
		std::vector<variable>*			vars;
		std::vector<samplerVar>*			texNames;
	};
	
	union
	{
		SamplerParam		samplerParamType;
		ShaderType			sType;
		ShaderCommand		sCommand;
		RenderStateType		sRenderStateType;
		RegisterParamType	rType;
		Topology			topology;
	};

	// Carrying these around is bad luck, or more like bad performance. But whatever...
	std::string strs[6];
	ShaderResourceType shaderResourceType;
};

namespace sfx
{
	extern RenderStateType renderStateType;
	extern bool gLexPassthrough;
	extern bool read_shader;
	extern bool read_function;

#ifdef LINUX
int fopen_s(FILE** pFile, const char *filename, const char *mode);
#endif

	extern BlendOption toBlend(const std::string &str);
	extern BlendOperation toBlendOp(const std::string &str);
	extern DepthComparison toDepthFunc(const std::string &str);
	extern FillMode toFillMode(const std::string &str);
	extern CullMode toCullMode(const std::string &str);
	extern PixelOutputFormat toPixelOutputFmt(const std::string &str);

}
#define SFXSTYPE sfxstype
// prevent echoing of unprocessed chars:
#define ECHO

std::string sfxreadblock(unsigned char openChar, unsigned char closeChar);
void sfxerror(const char*);
int sfxparse();
void sfxReset();
bool getSfxError();
extern std::string current_filename;
extern void SetLexStartState(int);
extern const char *GetStartModeText();
extern bool is_equal(const std::string& a, const char * b);
extern bool IsArrayTexture(ShaderResourceType t);
extern bool IsRW(ShaderResourceType t);
extern bool IsTexture(ShaderResourceType t);
extern bool IsStructuredBuffer(ShaderResourceType t);
extern bool IsRWStructuredBuffer(ShaderResourceType t);
extern bool IsCubemap(ShaderResourceType t);
extern bool IsMSAATexture(ShaderResourceType t);
extern int GetTextureDimension(ShaderResourceType t,bool array_as_2d=false);
