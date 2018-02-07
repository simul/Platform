#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include <math.h>
namespace simul
{
namespace crossplatform
{

/// A small structure for per-frame amortization of buffers.
struct SIMUL_CROSSPLATFORM_EXPORT AmortizationStruct
{
private:
	int amortization;
public:
	int framenumber;
	int framesPerIncrement;
	uint3 *pattern;
	int numOffsets;
	AmortizationStruct()
		:amortization(1)
		,framenumber(0)
		,framesPerIncrement(1)
		,pattern(nullptr)
		,numOffsets(1)
	{
	}
	~AmortizationStruct()
	{
		delete[] pattern;
	}
	void setAmortization(int a);
	int getAmortization() const
	{
		return amortization;
	}
	/// Reset frame data, but not properties.
	void reset()
	{
		framenumber=0;
	}
	uint3 scale() const
	{
		uint3 sc(1,1,1);
		sc.x=1+amortization/2;
		sc.y=sc.x-(amortization+1)%2;
		sc.y=sc.y>0?sc.y:1;
		return sc;
	}
	uint3 offset() const
	{
		if(!pattern||amortization<=1)
			return uint3(0,0,0);
		int sub_frame = fmod((framenumber - fmod(framenumber, numOffsets)) / numOffsets, numOffsets);
		return pattern[sub_frame];
	}
	void validate(int newFramenumber)
	{
		framenumber++;// = newFramenumber;
	}
};
}
}