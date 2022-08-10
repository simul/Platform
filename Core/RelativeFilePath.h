#pragma once
#include <string>

namespace platform
{
	namespace core
	{
		extern std::string Abs2Rel(const std::string &pcAbsPath,const std::string &pcCurrDir);
		extern std::string Rel2Abs(const std::string &pcRelPath,const std::string &pcCurrDir);
		extern std::string Rel2Rel(const std::string &pcRelPath,const std::string &pcRelCurrDir);
	}
}
