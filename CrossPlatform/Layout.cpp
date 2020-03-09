#include "Layout.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/DeviceContext.h"

using namespace simul;
using namespace crossplatform;

Layout::Layout()
	:apply_count(0)
	,struct_size(0)
{
}


Layout::~Layout()
{
	SIMUL_ASSERT(apply_count==0);
}

void Layout::SetDesc(const LayoutDesc *d,int num,bool i)
{
	interleaved = i;
	parts.clear();
	struct_size=0;
	for(int i=0;i<num;i++)
	{
		parts.push_back(*d);
		SIMUL_ASSERT(d->alignedByteOffset==struct_size);
		struct_size+=GetByteSize(d->format);
		d++;
	}
}

int Layout::GetPitch() const
{
	return interleaved?struct_size:0;
}


int Layout::GetStructSize() const
{
	return struct_size;
}

void Layout::Apply(DeviceContext &deviceContext)
{
	deviceContext.contextState.currentLayout=this;
}

void Layout::Unapply(DeviceContext &deviceContext)
{
	deviceContext.contextState.currentLayout=nullptr;
}