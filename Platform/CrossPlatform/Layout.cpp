#include "Layout.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"

using namespace simul;
using namespace crossplatform;

/*
static int sizeOf(const LayoutDesc *d)
{
	return d->format;
}
*/
Layout::Layout()
	:apply_count(0)
	,struct_size(0)
{
}


Layout::~Layout()
{
	SIMUL_ASSERT(apply_count==0);
}

void Layout::SetDesc(const LayoutDesc *d,int num)
{
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

int Layout::GetStructSize() const
{
	return struct_size;
}