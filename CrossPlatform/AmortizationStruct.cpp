#include "AmortizationStruct.h"
#include "Simul/Math/RandomNumberGenerator.h"
#include <vector>
using namespace simul;
using namespace crossplatform;
// Amortization: 0 = 1x1, 1=2x1, 2=1x2, 3=2x2, 4=3x2, 5=2x3, 6=3x3, etc.
void AmortizationStruct::setAmortization(int a)
{
	if(amortization==a)
		return;
	delete[] pattern;
	pattern=NULL;
	simul::math::RandomNumberGenerator rand;
	amortization=a;
	if(a<=1)
		return;
	uint3 xyz=scale();

	int sz=xyz.x*xyz.y;
	std::vector<uint3> src;
	src.reserve(sz);
	int n=0;
	for(unsigned i=0;i<xyz.x;i++)
	{
		for(unsigned j=0;j<xyz.y;j++)
		{
			uint3 v(i,j,0);
			src.push_back(v);
			n++;
		}
	}
	numOffsets=n;
	pattern=new uint3[numOffsets];
	for(int i=0;i<n;i++)
	{
		int idx=(int)rand.IRand((int)src.size());
		auto u=src.begin()+idx;
		uint3 v=*u;
		pattern[i]=v;
		src.erase(u);
	}
}