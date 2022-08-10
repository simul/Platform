#include "Platform/Core/CommandLineParams.h"
using namespace platform;
using namespace core;

bool CommandLineParams::operator()(const char *s)
{
	for(int i=0;i<strings.size();i++)
	{
		if(strings[i]==s)
			return true;
	}
	return false;
}