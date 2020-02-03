#pragma once
#include <string>
#include <map>
#include "SfxClasses.h"
#include "SfxEffect.h"
extern int Compile(sfx::CompiledShader *shader, const std::string &sourceFile, std::string targetFile
					, sfx::ShaderType t
					, sfx::PixelOutputFormat pixelOutputFormat
					, const std::string &sharedSource
					, std::ostringstream& sLog, const SfxConfig &sfxConfig
					, const SfxOptions &sfxOptions
					, std::map<int, std::string> fileList
					, std::ofstream &combinedBinary
					, BinaryMap &binaryMap
					, const Declaration* rtState = nullptr);