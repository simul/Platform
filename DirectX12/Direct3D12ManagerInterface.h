#ifndef Direct3D12ManagerINTERFACE
#define Direct3D12ManagerINTERFACE
#include <string>
struct Output
{
	std::string monitorName;
	int desktopX;
	int desktopY;
	int width;
	int height;
	int numerator,denominator;
};

#endif