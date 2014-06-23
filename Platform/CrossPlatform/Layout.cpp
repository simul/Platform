#include "Layout.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"

using namespace simul;
using namespace crossplatform;

Layout::Layout()
	:apply_count(0)
{
}


Layout::~Layout()
{
	SIMUL_ASSERT(apply_count==0);
}
