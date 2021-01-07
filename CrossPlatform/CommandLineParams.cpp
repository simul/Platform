#include "Platform/CrossPlatform/CommandLineParams.h"
using namespace simul;

using namespace crossplatform;
bool CommandLineParams::operator()(const char *s)
{
	for(int i=0;i<strings.size();i++)
	{
		if(strings[i]==s)
			return true;
	}
	return false;
}