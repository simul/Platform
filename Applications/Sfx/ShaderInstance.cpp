#include "ShaderInstance.h"
using namespace sfx;

ShaderInstance::ShaderInstance(const std::set<Declaration*> &d)
	:	global_line_number(0)
{
	declarations=d;
}

ShaderInstance::ShaderInstance(const ShaderInstance &cs)
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
