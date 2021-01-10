#pragma once
#include <string>
#include <map>
#include <set>
#include "SfxClasses.h"

namespace sfx
{
	//! A shader to be compiled. 
	struct ShaderInstance
	{
		ShaderInstance(const std::set<Declaration*> &d);
		ShaderInstance(const ShaderInstance &cs);
		ShaderType shaderType;
		std::string m_profile;
		std::string m_functionName;
		std::string m_preamble;
		std::string m_augmentedSource;
		std::string entryPoint;
		std::map<int,std::string> sbFilenames;// maps from PixelOutputFormat for pixel shaders, or int for vertex(0) and export(1) shaders.
		std::set<Declaration*> declarations;
		std::set<ConstantBuffer*> constantBuffers;
		int global_line_number;
		std::string rtFormatStateName;
	};
}