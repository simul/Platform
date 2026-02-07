#pragma once
#include <string>
#include <map>
#include <memory>
#include "SfxClasses.h"
#include "SfxEffect.h"
#include "ShaderInstance.h"

extern void ReplaceRegexes(std::string &src, const std::map<std::string,std::string> &replace);
extern int Compile(std::shared_ptr<sfx::ShaderInstance> shader, const std::string &sourceFile, std::string targetFile
					, sfx::ShaderType t
					, sfx::PixelOutputFormat pixelOutputFormat
					, const std::string &sharedSource
					, std::ostringstream& sLog, const SfxConfig &sfxConfig
					, const SfxOptions &sfxOptions
					, std::map<int, std::string> fileList
					, std::ofstream &combinedBinary
					, BinaryMap &binaryMap
					, const Declaration* rtState = nullptr);